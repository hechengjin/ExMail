/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsCOMPtr.h"
#include "mimepbuf.h"
#include "mimemoz2.h"
#include "prmem.h"
#include "prio.h"
#include "plstr.h"
#include "nsMimeStringResources.h"
#include "nsNetUtil.h"
#include "nsMsgUtils.h"
//
// External Defines...
//
extern nsresult
nsMsgCreateTempFile(const char *tFileName, nsIFile **tFile);

/* See mimepbuf.h for a description of the mission of this file.

   Implementation:

     When asked to buffer an object, we first try to malloc() a buffer to
   hold the upcoming part.  First we try to allocate a 50k buffer, and
   then back off by 5k until we are able to complete the allocation,
   or are unable to allocate anything.

   As data is handed to us, we store it in the memory buffer, until the
   size of the memory buffer is exceeded (including the case where no
   memory buffer was able to be allocated at all.)

   Once we've filled the memory buffer, we open a temp file on disk.
   Anything that is currently in the memory buffer is then flushed out
   to the disk file (and the memory buffer is discarded.)  Subsequent
   data that is passed in is appended to the file.

   Thus only one of the memory buffer or the disk buffer ever exist at
   the same time; and small parts tend to live completely in memory
   while large parts tend to live on disk.

   When we are asked to read the data back out of the buffer, we call
   the provided read-function with either: the contents of the memory
   buffer; or blocks read from the disk file.
 */

#define TARGET_MEMORY_BUFFER_SIZE    (1024 * 50)  /* try for 50k mem buffer */
#define TARGET_MEMORY_BUFFER_QUANTUM (1024 * 5)   /* decrease in steps of 5k */
#define DISK_BUFFER_SIZE       (1024 * 10)  /* read disk in 10k chunks */


struct MimePartBufferData
{
  char        *part_buffer;          /* Buffer used for part-lookahead. */
  int32_t     part_buffer_fp;        /* Active length. */
  int32_t     part_buffer_size;      /* How big it is. */

  nsCOMPtr <nsIFile> file_buffer;    /* The nsIFile of a temp file used when we
                                                          run out of room in the head_buffer. */
  nsCOMPtr <nsIInputStream> input_file_stream;    /* A stream to it. */
  nsCOMPtr <nsIOutputStream> output_file_stream;  /* A stream to it. */
};

MimePartBufferData *
MimePartBufferCreate (void)
{
  MimePartBufferData *data = PR_NEW(MimePartBufferData);
  if (!data) return 0;
  memset(data, 0, sizeof(*data));
  return data;
}


void
MimePartBufferClose (MimePartBufferData *data)
{
  NS_ASSERTION(data, "MimePartBufferClose: no data");
  if (!data) return;

  if (data->input_file_stream)
  {
    data->input_file_stream->Close();
    data->input_file_stream = nullptr;
  }

  if (data->output_file_stream)
  {
    data->output_file_stream->Close();
    data->output_file_stream = nullptr;
  }
}


void
MimePartBufferReset (MimePartBufferData *data)
{
  NS_ASSERTION(data, "MimePartBufferReset: no data");
  if (!data) return;

  PR_FREEIF(data->part_buffer);
  data->part_buffer_fp = 0;

  if (data->input_file_stream)
  {
    data->input_file_stream->Close();
    data->input_file_stream = nullptr;
  }

  if (data->output_file_stream)
  {
    data->output_file_stream->Close();
    data->output_file_stream = nullptr;
  }

  if (data->file_buffer)
  {
    data->file_buffer->Remove(false);
    data->file_buffer = nullptr;
  }
}


void
MimePartBufferDestroy (MimePartBufferData *data)
{
  NS_ASSERTION(data, "MimePartBufferDestroy: no data");
  if (!data) return;
  MimePartBufferReset (data);
  PR_Free(data);
}


int
MimePartBufferWrite (MimePartBufferData *data,
           const char *buf, int32_t size)
{
  NS_ASSERTION(data && buf && size > 0, "MimePartBufferWrite: Bad param");
  if (!data || !buf || size <= 0)
    return -1;

  /* If we don't yet have a buffer (either memory or file) try and make a
    memory buffer.
    */
  if (!data->part_buffer &&
      !data->file_buffer)
  {
    int target_size = TARGET_MEMORY_BUFFER_SIZE;
    while (target_size > 0)
    {
      data->part_buffer = (char *) PR_MALLOC(target_size);
      if (data->part_buffer) break;          /* got it! */
      target_size -= TARGET_MEMORY_BUFFER_QUANTUM;  /* decrease it and try
        again */
    }

    if (data->part_buffer)
      data->part_buffer_size = target_size;
    else
      data->part_buffer_size = 0;

    data->part_buffer_fp = 0;
  }

  /* Ok, if at this point we still don't have either kind of buffer, try and
    make a file buffer. */
  if (!data->part_buffer && !data->file_buffer)
  {
    nsCOMPtr <nsIFile> tmpFile;
    nsresult rv = nsMsgCreateTempFile("nsma", getter_AddRefs(tmpFile));
    NS_ENSURE_SUCCESS(rv, MIME_UNABLE_TO_OPEN_TMP_FILE);
    data->file_buffer = do_QueryInterface(tmpFile);

    rv = MsgNewBufferedFileOutputStream(getter_AddRefs(data->output_file_stream), data->file_buffer, PR_WRONLY | PR_CREATE_FILE, 00600);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  NS_ASSERTION(data->part_buffer || data->output_file_stream, "no part_buffer or file_stream");

  /* If this buf will fit in the memory buffer, put it there.
    */
  if (data->part_buffer &&
      data->part_buffer_fp + size < data->part_buffer_size)
  {
    memcpy(data->part_buffer + data->part_buffer_fp,
           buf, size);
    data->part_buffer_fp += size;
  }

  /* Otherwise it won't fit; write it to the file instead. */
  else
  {
    /* If the file isn't open yet, open it, and dump the memory buffer
    to it. */
    if (!data->output_file_stream)
    {
      nsresult rv;
      if (!data->file_buffer)
      {
        nsCOMPtr <nsIFile> tmpFile;
        rv = nsMsgCreateTempFile("nsma", getter_AddRefs(tmpFile));
        NS_ENSURE_SUCCESS(rv, MIME_UNABLE_TO_OPEN_TMP_FILE);
        data->file_buffer = do_QueryInterface(tmpFile);

      }

      rv = MsgNewBufferedFileOutputStream(getter_AddRefs(data->output_file_stream), data->file_buffer, PR_WRONLY | PR_CREATE_FILE, 00600);
      NS_ENSURE_SUCCESS(rv, MIME_UNABLE_TO_OPEN_TMP_FILE);

      if (data->part_buffer && data->part_buffer_fp)
      {
        uint32_t bytesWritten;
        nsresult rv = data->output_file_stream->Write(data->part_buffer,
                                                 data->part_buffer_fp, &bytesWritten);
        NS_ENSURE_SUCCESS(rv, rv);
      }

      PR_FREEIF(data->part_buffer);
      data->part_buffer_fp = 0;
      data->part_buffer_size = 0;
    }

    /* Dump this buf to the file. */
    uint32_t bytesWritten;
    nsresult rv = data->output_file_stream->Write (buf, size, &bytesWritten);
    if (NS_FAILED(rv) || (int32_t) bytesWritten < size)
      return MIME_OUT_OF_MEMORY;
  }

  return 0;
}


int
MimePartBufferRead (MimePartBufferData *data,
          MimeConverterOutputCallback read_fn,
          void *closure)
{
  int status = 0;
  NS_ASSERTION(data, "no data");
  if (!data) return -1;

  if (data->part_buffer)
  {
    // Read it out of memory.
    status = read_fn(data->part_buffer, data->part_buffer_fp, closure);
  }
  else if (data->file_buffer)
  {
    /* Read it off disk.
     */
    char *buf;
    int32_t buf_size = DISK_BUFFER_SIZE;

    NS_ASSERTION(data->part_buffer_size == 0 && data->part_buffer_fp == 0, "buffer size is not null");
    NS_ASSERTION(data->file_buffer, "no file buffer name");
    if (!data->file_buffer)
      return -1;

    buf = (char *) PR_MALLOC(buf_size);
    if (!buf)
      return MIME_OUT_OF_MEMORY;

    // First, close the output file to open the input file!
    if (data->output_file_stream)
      data->output_file_stream->Close();

    nsresult rv = NS_NewLocalFileInputStream(getter_AddRefs(data->input_file_stream), data->file_buffer);
    if (NS_FAILED(rv))
    {
      PR_Free(buf);
      return MIME_UNABLE_TO_OPEN_TMP_FILE;
    }
    while(1)
    {
      uint32_t bytesRead = 0;
      rv = data->input_file_stream->Read(buf, buf_size - 1, &bytesRead);
      if (NS_FAILED(rv) || !bytesRead)
      {
        break;
      }
      else
      {
        /* It would be really nice to be able to yield here, and let
        some user events and other input sources get processed.
        Oh well. */

        status = read_fn (buf, bytesRead, closure);
        if (status < 0) break;
      }
    }
    PR_Free(buf);
  }

  return 0;
}

