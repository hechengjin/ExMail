/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <stdio.h>
#include "modmimee.h"
#include "mimei.h"
#include "prmem.h"
#include "plstr.h"
#include "prlog.h"
#include "prprf.h"
#include "mimeobj.h"
#include "nsIMimeConverter.h" // for MimeConverterOutputCallback

typedef enum mime_encoding {
  mime_Base64, mime_QuotedPrintable, mime_uuencode, mime_yencode
} mime_encoding;

typedef enum mime_decoder_state {
  DS_BEGIN, DS_BODY, DS_END
} mime_decoder_state;

struct MimeDecoderData {
  mime_encoding encoding;    /* Which encoding to use */

  /* A read-buffer used for QP and B64. */
  char token[4];
  int token_size;

  /* State and read-buffer used for uudecode and yencode. */
  mime_decoder_state ds_state;
  char *line_buffer;
  int line_buffer_size;

  MimeObject *objectToDecode; // might be null, only used for QP currently
  /* Where to write the decoded data */
  MimeConverterOutputCallback write_buffer;
  void *closure;
};


static int
mime_decode_qp_buffer (MimeDecoderData *data, const char *buffer,
          int32_t length, int32_t *outSize)
{
  /* Warning, we are overwriting the buffer which was passed in.
   This is ok, because decoding these formats will never result
   in larger data than the input, only smaller. */
  const char *in  = buffer;
  char *out = (char *) buffer;
  char token [3];
  int i;

  NS_ASSERTION(data->encoding == mime_QuotedPrintable, "1.1 <rhp@netscape.com> 19 Mar 1999 12:00");
  if (data->encoding != mime_QuotedPrintable) return -1;

  /* For the first pass, initialize the token from the unread-buffer. */
  i = 0;
  while (i < 3 && data->token_size > 0)
  {
    token [i] = data->token[i];
    data->token_size--;
    i++;
  }

  /* #### BUG: when decoding quoted-printable, we are required to
   strip trailing whitespace from lines -- since when encoding in
   qp, one is required to quote such trailing whitespace, any
   trailing whitespace which remains must have been introduced
   by a stupid gateway. */

  /* Treat null bytes as spaces when format_out is
   nsMimeOutput::nsMimeMessageBodyDisplay (see bug 243199 comment 7) */
  bool treatNullAsSpace = data->objectToDecode &&
                            data->objectToDecode->options->format_out == nsMimeOutput::nsMimeMessageBodyDisplay;

  while (length > 0 || i != 0)
  {
    while (i < 3 && length > 0)
    {
      token [i++] = *in;
      in++;
      length--;
    }

    if (i < 3)
    {
      /* Didn't get enough for a complete token.
       If it might be a token, unread it.
       Otherwise, just dump it.
       */
      memcpy (data->token, token, i);
      data->token_size = i;
      i = 0;
      length = 0;
      break;
    }
    i = 0;

    if (token [0] == '=')
    {
      unsigned char c = 0;
      if (token[1] >= '0' && token[1] <= '9')
      c = token[1] - '0';
      else if (token[1] >= 'A' && token[1] <= 'F')
      c = token[1] - ('A' - 10);
      else if (token[1] >= 'a' && token[1] <= 'f')
      c = token[1] - ('a' - 10);
      else if (token[1] == '\r' || token[1] == '\n')
      {
        /* =\n means ignore the newline. */
        if (token[1] == '\r' && token[2] == '\n')
        ;    /* swallow all three chars */
        else
        {
          in--;  /* put the third char back */
          length++;
        }
        continue;
      }
      else
      {
        /* = followed by something other than hex or newline -
         pass it through unaltered, I guess.  (But, if
         this bogus token happened to occur over a buffer
         boundary, we can't do this, since we don't have
         space for it.  Oh well.  Screw it.)  */
        if (in > out) *out++ = token[0];
        if (in > out) *out++ = token[1];
        if (in > out) *out++ = token[2];
        continue;
      }

      /* Second hex digit */
      c = (c << 4);
      if (token[2] >= '0' && token[2] <= '9')
      c += token[2] - '0';
      else if (token[2] >= 'A' && token[2] <= 'F')
      c += token[2] - ('A' - 10);
      else if (token[2] >= 'a' && token[2] <= 'f')
      c += token[2] - ('a' - 10);
      else
      {
        /* We got =xy where "x" was hex and "y" was not, so
         treat that as a literal "=", x, and y.  (But, if
         this bogus token happened to occur over a buffer
         boundary, we can't do this, since we don't have
         space for it.  Oh well.  Screw it.) */
        if (in > out) *out++ = token[0];
        if (in > out) *out++ = token[1];
        if (in > out) *out++ = token[2];
        continue;
      }

      *out++ = c ? (char) c : ((treatNullAsSpace) ? ' ' : (char) c);
    }
    else
    {
      *out++ = token[0];

      token[0] = token[1];
      token[1] = token[2];
      i = 2;
    }
  }

  // Fill the size
  if (outSize)
    *outSize = out - buffer;

  /* Now that we've altered the data in place, write it. */
  if (out > buffer)
  return data->write_buffer (buffer, (out - buffer), data->closure);
  else
  return 1;
}


static int
mime_decode_base64_token (const char *in, char *out)
{
  /* reads 4, writes 0-3.  Returns bytes written.
   (Writes less than 3 only at EOF.) */
  int j;
  int eq_count = 0;
  unsigned long num = 0;

  for (j = 0; j < 4; j++)
  {
    unsigned char c = 0;
    if (in[j] >= 'A' && in[j] <= 'Z')     c = in[j] - 'A';
    else if (in[j] >= 'a' && in[j] <= 'z') c = in[j] - ('a' - 26);
    else if (in[j] >= '0' && in[j] <= '9') c = in[j] - ('0' - 52);
    else if (in[j] == '+')         c = 62;
    else if (in[j] == '/')         c = 63;
    else if (in[j] == '=')         c = 0, eq_count++;
    else
    NS_ERROR("Invalid character");
    num = (num << 6) | c;
  }

  *out++ = (char) (num >> 16);
  *out++ = (char) ((num >> 8) & 0xFF);
  *out++ = (char) (num & 0xFF);

  if (eq_count == 0)
  return 3;        /* No "=" padding means 4 bytes mapped to 3. */
  else if (eq_count == 1)
  return 2;        /* "xxx=" means 3 bytes mapped to 2. */
  else if (eq_count == 2)
  return 1;        /* "xx==" means 2 bytes mapped to 1. */
  else
  {
    // "x===" can't happen, because "x" would then be encoding only
    // 6 bits, not the min of 8.
    NS_ERROR("Count is 6 bits, should be at least 8");
    return 1;
  }
}


static int
mime_decode_base64_buffer (MimeDecoderData *data,
               const char *buffer, int32_t length, int32_t *outSize)
{
  /* Warning, we are overwriting the buffer which was passed in.
   This is ok, because decoding these formats will never result
   in larger data than the input, only smaller. */
  const char *in  = buffer;
  char *out = (char *) buffer;
  char token [4];
  int i;
  bool leftover = (data->token_size > 0);

  NS_ASSERTION(data->encoding == mime_Base64, "1.1 <rhp@netscape.com> 19 Mar 1999 12:00");

  /* For the first pass, initialize the token from the unread-buffer. */
  i = 0;
  while (i < 4 && data->token_size > 0)
  {
    token [i] = data->token[i];
    data->token_size--;
    i++;
  }

  while (length > 0)
  {
    while (i < 4 && length > 0)
    {
      if ((*in >= 'A' && *in <= 'Z') ||
        (*in >= 'a' && *in <= 'z') ||
        (*in >= '0' && *in <= '9') ||
        *in == '+' || *in == '/' || *in == '=')
      token [i++] = *in;
      in++;
      length--;
    }

    if (i < 4)
    {
      /* Didn't get enough for a complete token. */
      memcpy (data->token, token, i);
      data->token_size = i;
      length = 0;
      break;
    }
    i = 0;

    if (leftover)
    {
      /* If there are characters left over from the last time around,
       we might not have space in the buffer to do our dirty work
       (if there were 2 or 3 left over, then there is only room for
       1 or 2 in the buffer right now, and we need 3.)  This is only
       a problem for the first chunk in each buffer, so in that
       case, just write prematurely. */
      int n;
      n = mime_decode_base64_token (token, token);
      n = data->write_buffer (token, n, data->closure);
      if (n < 0) /* abort */
      return n;

      /* increment buffer so that we don't write the 1 or 2 unused
       characters now at the front. */
      buffer = in;
      out = (char *) buffer;

      leftover = false;
    }
    else
    {
      int n = mime_decode_base64_token (token, out);
      /* Advance "out" by the number of bytes just written to it. */
      out += n;
    }
  }

  if (outSize)
    *outSize = out - buffer;
  /* Now that we've altered the data in place, write it. */
  if (out > buffer)
  return data->write_buffer (buffer, (out - buffer), data->closure);
  else
  return 1;
}


static int
mime_decode_uue_buffer (MimeDecoderData *data,
            const char *input_buffer, int32_t input_length, int32_t *outSize)
{
  /* First, copy input_buffer into state->line_buffer until we have
   a complete line.

   Then decode that line in place (in the line_buffer) and write
   it out.

   Then pull the next line into line_buffer and continue.
   */
  if (!data->line_buffer)
  {
    data->line_buffer_size = 128;
    data->line_buffer = (char *)PR_MALLOC(data->line_buffer_size);
    if (!data->line_buffer)
      return -1;
    data->line_buffer[0] = 0;
  }

  int status = 0;
  char *line = data->line_buffer;
  char *line_end = data->line_buffer + data->line_buffer_size - 1;

  NS_ASSERTION(data->encoding == mime_uuencode, "1.1 <rhp@netscape.com> 19 Mar 1999 12:00");
  if (data->encoding != mime_uuencode) return -1;

  if (data->ds_state == DS_END)
  {
    status = 0;
    goto DONE;
  }

  while (input_length > 0)
  {
    /* Copy data from input_buffer to `line' until we have a complete line,
     or until we've run out of input.

     (line may have data in it already if the last time we were called,
     we weren't called with a buffer that ended on a line boundary.)
     */
    {
    char *out = line + strlen(line);
    while (input_length > 0 &&
         out < line_end)
      {
      *out++ = *input_buffer++;
      input_length--;

      if (out[-1] == '\r' || out[-1] == '\n')
        {
        /* If we just copied a CR, and an LF is waiting, grab it too.
         */
        if (out[-1] == '\r' &&
          input_length > 0 &&
          *input_buffer == '\n')
          input_buffer++, input_length--;

        /* We have a line. */
        break;
        }
      }
    *out = 0;

    /* Ignore blank lines.
     */
    if (*line == '\r' || *line == '\n')
      {
      *line = 0;
      continue;
      }

    /* If this line was bigger than our buffer, truncate it.
       (This means the data was way corrupted, and there's basically
       no chance of decoding it properly, but give it a shot anyway.)
     */
    if (out == line_end)
      {
      out--;
      out[-1] = '\r';
      out[0] = 0;
      }

    /* If we didn't get a complete line, simply return; we'll be called
       with the rest of this line next time.
     */
    if (out[-1] != '\r' && out[-1] != '\n')
      {
      NS_ASSERTION (input_length == 0, "1.1 <rhp@netscape.com> 19 Mar 1999 12:00");
      break;
      }
    }


    /* Now we have a complete line.  Deal with it.
     */


    if (data->ds_state == DS_BODY &&
      line[0] == 'e' &&
      line[1] == 'n' &&
      line[2] == 'd' &&
      (line[3] == '\r' ||
       line[3] == '\n'))
    {
      /* done! */
      data->ds_state = DS_END;
      *line = 0;
      break;
    }
    else if (data->ds_state == DS_BEGIN)
    {
      if (!strncmp (line, "begin ", 6))
      data->ds_state = DS_BODY;
      *line = 0;
      continue;
    }
    else
    {
      /* We're in DS_BODY.  Decode the line. */
      char *in, *out;
      int32_t i;
      long lost;

      NS_ASSERTION (data->ds_state == DS_BODY, "1.1 <rhp@netscape.com> 19 Mar 1999 12:00");

      /* We map down `line', reading four bytes and writing three.
       That means that `out' always stays safely behind `in'.
       */
      in = line;
      out = line;

# undef DEC
# define DEC(c) (((c) - ' ') & 077)
      i = DEC (*in); /* get length */

      /* all the parens and casts are because gcc was doing something evil.
       */
      lost = ((long) i) - (((((long) strlen (in)) - 2L) * 3L) / 4L);

      if (lost > 0) /* Short line!! */
      {
        /* If we get here, then the line is shorter than the length byte
         at the beginning says it should be.  However, the case where
         the line is short because it was at the end of the buffer and
         we didn't get the whole line was handled earlier (up by the
         "didn't get a complete line" comment.)  So if we've gotten
         here, then this is a complete line which is internally
         inconsistent.  We will parse from it what we can...

         This probably happened because some gateway stripped trailing
         whitespace from the end of the line -- so pretend the line
         was padded with spaces (which map to \000.)
         */
        i -= lost;
      }

      for (++in; i > 0; in += 4, i -= 3)
      {
        char ch;
        NS_ASSERTION(out <= in, "1.1 <rhp@netscape.com> 19 Mar 1999 12:00");

        if (i >= 3)
        {
          /* We read four; write three. */
          ch = DEC (in[0]) << 2 | DEC (in[1]) >> 4;
          *out++ = ch;

          NS_ASSERTION(out <= in+1, "1.1 <rhp@netscape.com> 19 Mar 1999 12:00");

          ch = DEC (in[1]) << 4 | DEC (in[2]) >> 2;
          *out++ = ch;

          NS_ASSERTION(out <= in+2, "1.1 <rhp@netscape.com> 19 Mar 1999 12:00");

          ch = DEC (in[2]) << 6 | DEC (in[3]);
          *out++ = ch;

          NS_ASSERTION(out <= in+3, "1.1 <rhp@netscape.com> 19 Mar 1999 12:00");
        }
        else
        {
          /* Handle a line that isn't a multiple of 4 long.
           (We read 1, 2, or 3, and will write 1 or 2.)
           */
          NS_ASSERTION (i > 0 && i < 3, "1.1 <rhp@netscape.com> 19 Mar 1999 12:00");

          ch = DEC (in[0]) << 2 | DEC (in[1]) >> 4;
          *out++ = ch;

          NS_ASSERTION(out <= in+1, "1.1 <rhp@netscape.com> 19 Mar 1999 12:00");

          if (i == 2)
          {
            ch = DEC (in[1]) << 4 | DEC (in[2]) >> 2;
            *out++ = ch;

            NS_ASSERTION(out <= in+2, "1.1 <rhp@netscape.com> 19 Mar 1999 12:00");
          }
        }
      }

      /* If the line was truncated, pad the missing bytes with 0 (SPC). */
      while (lost > 0)
      {
        *out++ = 0;
        lost--;
        in = out+1; /* just to prevent the assert, below. */
      }
# undef DEC

      /* Now write out what we decoded for this line.
       */
      NS_ASSERTION(out >= line && out < in, "1.1 <rhp@netscape.com> 19 Mar 1999 12:00");
      if (out > line)
      status = data->write_buffer (line, (out - line), data->closure);

      // The assertion above tells us this is >= 0
      if (outSize)
        *outSize = out - line;

      /* Reset the line so that we don't think it's partial next time. */
      *line = 0;

      if (status < 0) /* abort */
      goto DONE;
    }
  }

  status = 1;

 DONE:

  return status;
}

static int
mime_decode_yenc_buffer (MimeDecoderData *data,
            const char *input_buffer, int32_t input_length, int32_t *outSize)
{
  /* First, copy input_buffer into state->line_buffer until we have
   a complete line.

   Then decode that line in place (in the line_buffer) and write
   it out.

   Then pull the next line into line_buffer and continue.
   */
  if (!data->line_buffer)
  {
    data->line_buffer_size = 1000; // let make sure we have plenty of space for the header line
    data->line_buffer = (char *)PR_MALLOC(data->line_buffer_size);
    if (!data->line_buffer)
      return -1;
    data->line_buffer[0] = 0;
  }

  int status = 0;
  char *line = data->line_buffer;
  char *line_end = data->line_buffer + data->line_buffer_size - 1;

  NS_ASSERTION(data->encoding == mime_yencode, "wrong decoder!");
  if (data->encoding != mime_yencode) return -1;

  if (data->ds_state == DS_END)
    return 0;

  while (input_length > 0)
  {
    /* Copy data from input_buffer to `line' until we have a complete line,
       or until we've run out of input.

       (line may have data in it already if the last time we were called,
       we weren't called with a buffer that ended on a line boundary.)
    */
    {
      char *out = line + strlen(line);
      while (input_length > 0 && out < line_end)
      {
        *out++ = *input_buffer++;
        input_length--;

        if (out[-1] == '\r' || out[-1] == '\n')
        {
          /* If we just copied a CR, and an LF is waiting, grab it too. */
          if (out[-1] == '\r' &&
                  input_length > 0 &&
                  *input_buffer == '\n')
            input_buffer++, input_length--;

           /* We have a line. */
           break;
        }
      }
      *out = 0;

      /* Ignore blank lines. */
      if (*line == '\r' || *line == '\n')
      {
        *line = 0;
        continue;
      }

      /* If this line was bigger than our buffer, truncate it.
         (This means the data was way corrupted, and there's basically
         no chance of decoding it properly, but give it a shot anyway.)
      */
      if (out == line_end)
      {
        out--;
        out[-1] = '\r';
        out[0] = 0;
      }

      /* If we didn't get a complete line, simply return; we'll be called
         with the rest of this line next time.
      */
      if (out[-1] != '\r' && out[-1] != '\n')
      {
        NS_ASSERTION (input_length == 0, "empty buffer!");
        break;
      }
    }


    /* Now we have a complete line.  Deal with it.
     */
    const char * endOfLine = line + strlen(line);

    if (data->ds_state == DS_BEGIN)
    {
      int new_line_size = 0;
      /* this yenc decoder does not support yenc v2 or multipart yenc.
         Therefore, we are looking first for "=ybegin line="
      */
      if ((endOfLine - line) >= 13 && !strncmp (line, "=ybegin line=", 13))
      {
        /* ...then couple digits. */
        for (line += 13; line < endOfLine; line ++)
        {
          if (*line < '0' || *line > '9')
            break;
          new_line_size = (new_line_size * 10) + *line - '0';
        }

        /* ...next, look for <space>size= */
        if ((endOfLine - line) >= 6 && !strncmp (line, " size=", 6))
        {
          /* ...then couple digits. */
          for (line += 6; line < endOfLine; line ++)
            if (*line < '0' || *line > '9')
              break;

          /* ...next, look for <space>name= */
          if ((endOfLine - line) >= 6 && !strncmp (line, " name=", 6))
          {
            /* we have found the yenc header line.
               Now check if we need to grow our buffer line
            */
            data->ds_state = DS_BODY;
            if (new_line_size > data->line_buffer_size && new_line_size <= 997) /* don't let bad value hurt us! */
            {
              PR_Free(data->line_buffer);
              data->line_buffer_size = new_line_size + 4; //extra chars for line ending and potential escape char
              data->line_buffer = (char *)PR_MALLOC(data->line_buffer_size);
              if (!data->line_buffer)
                return -1;
            }
          }
        }

      }
      *data->line_buffer = 0;
      continue;
    }

    if (data->ds_state == DS_BODY && line[0] == '=')
    {
      /* look if this this the final line */
      if (!strncmp (line, "=yend size=", 11))
      {
        /* done! */
        data->ds_state = DS_END;
        *line = 0;
        break;
      }
    }

    /* We're in DS_BODY.  Decode the line in place. */
    {
      char *src = line;
      char *dest = src;
      char c;
      for (; src < line_end; src ++)
      {
        c = *src;
        if (!c || c == '\r' || c == '\n')
          break;

        if (c == '=')
        {
          src++;
          c = *src;
          if (c == 0)
            return -1;  /* last character cannot be escape char */
          c -= 64;
        }
        c -= 42;
        *dest = c;
        dest ++;
      }

      // The assertion below is helpful, too
      if (outSize)
        *outSize = dest - line;

      /* Now write out what we decoded for this line. */
      NS_ASSERTION(dest >= line && dest <= src, "nothing to write!");
      if (dest > line)
      {
        status = data->write_buffer (line, dest - line, data->closure);
        if (status < 0) /* abort */
          return status;
      }

      /* Reset the line so that we don't think it's partial next time. */
      *line = 0;
    }
  }

  return 1;
}

int
MimeDecoderDestroy (MimeDecoderData *data, bool abort_p)
{
  int status = 0;
  /* Flush out the last few buffered characters. */
  if (!abort_p &&
    data->token_size > 0 &&
    data->token[0] != '=')
  {
    if (data->encoding == mime_Base64)
    while ((unsigned int)data->token_size < sizeof (data->token))
      data->token [data->token_size++] = '=';

    status = data->write_buffer (data->token, data->token_size,
                   data->closure);
  }

  if (data->line_buffer)
    PR_Free(data->line_buffer);
  PR_Free (data);
  return status;
}


static MimeDecoderData *
mime_decoder_init (mime_encoding which,
           MimeConverterOutputCallback output_fn,
           void *closure)
{
  MimeDecoderData *data = PR_NEW(MimeDecoderData);
  if (!data) return 0;
  memset(data, 0, sizeof(*data));
  data->encoding = which;
  data->write_buffer = output_fn;
  data->closure = closure;
  data->line_buffer_size = 0;
  data->line_buffer = nullptr;

  return data;
}

MimeDecoderData *
MimeB64DecoderInit (MimeConverterOutputCallback output_fn, void *closure)
{
  return mime_decoder_init (mime_Base64, output_fn, closure);
}

MimeDecoderData *
MimeQPDecoderInit (MimeConverterOutputCallback output_fn,
           void *closure, MimeObject *object)
{
  MimeDecoderData *retData = mime_decoder_init (mime_QuotedPrintable, output_fn, closure);
  if (retData)
    retData->objectToDecode = object;
  return retData;
}

MimeDecoderData *
MimeUUDecoderInit (MimeConverterOutputCallback output_fn,
           void *closure)
{
  return mime_decoder_init (mime_uuencode, output_fn, closure);
}

MimeDecoderData *
MimeYDecoderInit (MimeConverterOutputCallback output_fn,
           void *closure)
{
  return mime_decoder_init (mime_yencode, output_fn, closure);
}

int
MimeDecoderWrite (MimeDecoderData *data, const char *buffer, int32_t size,
          int32_t *outSize)
{
  NS_ASSERTION(data, "1.1 <rhp@netscape.com> 19 Mar 1999 12:00");
  if (!data) return -1;
  switch(data->encoding)
  {
  case mime_Base64:
    return mime_decode_base64_buffer (data, buffer, size, outSize);
  case mime_QuotedPrintable:
    return mime_decode_qp_buffer (data, buffer, size, outSize);
  case mime_uuencode:
    return mime_decode_uue_buffer (data, buffer, size, outSize);
  case mime_yencode:
    return mime_decode_yenc_buffer (data, buffer, size, outSize);
  default:
    NS_ERROR("Invalid decoding");
    return -1;
  }
}


/* ================== Encoders.
 */

struct MimeEncoderData {
  mime_encoding encoding;    /* Which encoding to use */

  /* Buffer for the base64 encoder. */
  unsigned char in_buffer[3];
  int32_t in_buffer_count;

  /* Buffer for uuencoded data. (Need a line because of the length byte.) */
  unsigned char uue_line_buf[128];
  bool uue_wrote_begin;

  int32_t current_column, line_byte_count;

  char *filename; /* filename for use with uuencoding */

  /* Where to write the encoded data */
  MimeConverterOutputCallback write_buffer;
  void *closure;
};


int
mime_encode_base64_buffer (MimeEncoderData *data,
               const char *buffer, int32_t size)
{
  int status = 0;
  const unsigned char *in = (unsigned char *) buffer;
  const unsigned char *end = in + size;
  char out_buffer[80];
  char *out = out_buffer;
  uint32_t i = 0, n = 0;
  uint32_t off;

  if (size == 0)
  return 0;
  else if (size < 0)
  {
    NS_ERROR("Size is less than 0");
    return -1;
  }


  /* If this input buffer is too small, wait until next time. */
  if (size < (3 - data->in_buffer_count))
  {
    NS_ASSERTION(size < 3 && size > 0, "1.1 <rhp@netscape.com> 19 Mar 1999 12:00");
    data->in_buffer[data->in_buffer_count++] = buffer[0];
    if (size > 1)
    data->in_buffer[data->in_buffer_count++] = buffer[1];
    NS_ASSERTION(data->in_buffer_count < 3, "1.1 <rhp@netscape.com> 19 Mar 1999 12:00");
    return 0;
  }


  /* If there are bytes that were put back last time, take them now.
   */
  i = 0;
  if (data->in_buffer_count > 0) n = data->in_buffer[0];
  if (data->in_buffer_count > 1) n = (n << 8) + data->in_buffer[1];
  i = data->in_buffer_count;
  data->in_buffer_count = 0;

  /* If this buffer is not a multiple of three, put one or two bytes back.
   */
  off = ((size + i) % 3);
  if (off)
  {
    data->in_buffer[0] = buffer [size - off];
    if (off > 1)
    data->in_buffer [1] = buffer [size - off + 1];
    data->in_buffer_count = off;
    size -= off;
    NS_ASSERTION (! ((size + i) % 3), "1.1 <rhp@netscape.com> 19 Mar 1999 12:00");
    end = (unsigned char *) (buffer + size);
  }

  /* Populate the out_buffer with base64 data, one line at a time.
   */
  while (in < end)
  {
    int32_t j;

    while (i < 3)
    {
      n = (n << 8) | *in++;
      i++;
    }
    i = 0;

    for (j = 18; j >= 0; j -= 6)
    {
      unsigned int k = (n >> j) & 0x3F;
      if (k < 26)       *out++ = k      + 'A';
      else if (k < 52)  *out++ = k - 26 + 'a';
      else if (k < 62)  *out++ = k - 52 + '0';
      else if (k == 62) *out++ = '+';
      else if (k == 63) *out++ = '/';
      else abort ();
    }

    data->current_column += 4;
    if (data->current_column >= 72)
    {
      /* Do a linebreak before column 76.  Flush out the line buffer. */
      data->current_column = 0;
      *out++ = '\015';
      *out++ = '\012';
      status = data->write_buffer (out_buffer, (out - out_buffer),
                     data->closure);
      out = out_buffer;
      if (status < 0) return status;
    }
  }

  /* Write out the unwritten portion of the last line buffer. */
  if (out > out_buffer)
  {
    status = data->write_buffer (out_buffer, (out - out_buffer),
                   data->closure);
    if (status < 0) return status;
  }

  return 0;
}


int
mime_encode_qp_buffer (MimeEncoderData *data, const char *buffer, int32_t size)
{
  int status = 0;
  static const char hexdigits[] = "0123456789ABCDEF";
  const unsigned char *in = (unsigned char *) buffer;
  const unsigned char *end = in + size;
  char out_buffer[80];
  char *out = out_buffer;
  bool white = false;
  bool mb_p = false;

  /*
  #### I don't know how to hook this back up:
  ####  mb_p = INTL_DefaultWinCharSetID(state->context) & 0x300 ;
  */


  NS_ASSERTION(data->in_buffer_count == 0, "1.1 <rhp@netscape.com> 19 Mar 1999 12:00");

  /* Populate the out_buffer with quoted-printable data, one line at a time.
  */
  for (; in < end; in++)
  {
    if (*in == '\r' || *in == '\n')
    {

    /* Whitespace cannot be allowed to occur at the end of
    the line, so we backup and replace the whitespace with
    its code.
      */
      if (white)
      {
        out--;
        char whitespace_char = *out;
        *out++ = '=';
        *out++ = hexdigits[whitespace_char >> 4];
        *out++ = hexdigits[whitespace_char & 0xF];
      }

      /* Now write out the newline. */
      *out++ = '\r';
      *out++ = '\n';
      white = false;

      status = data->write_buffer (out_buffer, (out - out_buffer),
        data->closure);
      if (status < 0) return status;
      out = out_buffer;

      /* If it's CRLF, swallow two chars instead of one. */
      if (in + 1 < end && in[0] == '\r' && in[1] == '\n')
        in++;

      out = out_buffer;
      white = false;
      data->current_column = 0;
    }
    else if (data->current_column == 0 && *in == '.')
    {
      /* Just to be SMTP-safe, if "." appears in column 0, encode it.
                  (mmencode does this too.)
      */
      goto HEX;
    }
    else if (data->current_column == 0 && *in == 'F'
         && (in >= end-1 || in[1] == 'r')
                           && (in >= end-2 || in[2] == 'o')
                           && (in >= end-3 || in[3] == 'm')
                           && (in >= end-4 || in[4] == ' '))
    {
      /* If this line begins with 'F' and we cannot determine that
                  this line does not begin with "From " then do the safe thing
                  and assume that it does, and encode the 'F' in hex to avoid
                  BSD mailbox lossage.  (We might not be able to tell that it
                  is really "From " if the end of the buffer was early.  So
                  this means that "\nFoot" will have the F encoded if the end of
                  the buffer happens to fall just after the F; but will not have
                  it encoded if it's after the first "o" or later.  Oh well.
                  It's a little inconsistent, but it errs on the safe side.)
      */
      goto HEX;
    }
    else if ((*in >= 33 && *in <= 60) ||    /* safe printing chars */
         (*in >= 62 && *in <= 126) ||
                           (mb_p && (*in == 61 || *in == 127 || *in == 0x1B)))
    {
      white = false;
      *out++ = *in;
      data->current_column++;
    }
    else if (*in == ' ' || *in == '\t')    /* whitespace */
    {
      white = true;
      *out++ = *in;
      data->current_column++;
    }
    else                    /* print as =FF */
    {
HEX:
      white = false;
                  *out++ = '=';
                  *out++ = hexdigits[*in >> 4];
                  *out++ = hexdigits[*in & 0xF];
                  data->current_column += 3;
    }

    NS_ASSERTION (data->current_column <= 76, "1.1 <rhp@netscape.com> 19 Mar 1999 12:00"); /* Hard limit required by spec */

    if (data->current_column >= 73)    /* soft line break: "=\r\n" */
    {
      *out++ = '=';
      *out++ = '\r';
      *out++ = '\n';

      status = data->write_buffer (out_buffer, (out - out_buffer),
        data->closure);
      if (status < 0) return status;
      out = out_buffer;
      white = false;
      data->current_column = 0;
    }
  }

  /* Write out the unwritten portion of the last line buffer. */
  if (out > out_buffer)
  {
    status = data->write_buffer (out_buffer, (out - out_buffer),
      data->closure);
    if (status < 0) return status;
  }

  return 0;
}



int
MimeEncoderDestroy (MimeEncoderData *data, bool abort_p)
{
  int status = 0;

  /* Since Base64 output needs to do some buffering to get
   a multiple of three bytes on each block, there may be a few bytes
   left in the buffer after the last block has been written.  We need to
   flush those out now.
   */

  NS_ASSERTION (data->encoding == mime_Base64 ||
       data->in_buffer_count == 0, "1.1 <rhp@netscape.com> 19 Mar 1999 12:00");

  if (!abort_p &&
    data->in_buffer_count > 0)
  {
    char buf2 [6];
    char *buf = buf2 + 2;
    char *out = buf;
    int j;
    /* fixed bug 55998, 61302, 61866
     * type casting to uint32_t before shifting
     */
    uint32_t n = ((uint32_t) data->in_buffer[0]) << 16;
    if (data->in_buffer_count > 1)
    n = n | (((uint32_t) data->in_buffer[1]) << 8);

    buf2[0] = '\r';
    buf2[1] = '\n';

    for (j = 18; j >= 0; j -= 6)
    {
      unsigned int k = (n >> j) & 0x3F;
      if (k < 26)       *out++ = k      + 'A';
      else if (k < 52)  *out++ = k - 26 + 'a';
      else if (k < 62)  *out++ = k - 52 + '0';
      else if (k == 62) *out++ = '+';
      else if (k == 63) *out++ = '/';
      else abort ();
    }

    /* Pad with equal-signs. */
    if (data->in_buffer_count == 1)
    buf[2] = '=';
    buf[3] = '=';

    if (data->current_column >= 72)
    status = data->write_buffer (buf2, 6, data->closure);
    else
    status = data->write_buffer (buf,  4, data->closure);
  }

  PR_FREEIF(data->filename);
  PR_Free (data);
  return status;
}


static MimeEncoderData *
mime_encoder_init (mime_encoding which,
           MimeConverterOutputCallback output_fn,
           void *closure)
{
  MimeEncoderData *data = PR_NEW(MimeEncoderData);
  if (!data) return 0;
  memset(data, 0, sizeof(*data));
  data->encoding = which;
  data->write_buffer = output_fn;
  data->closure = closure;
  return data;
}

MimeEncoderData *
MimeB64EncoderInit (MimeConverterOutputCallback output_fn, void *closure)
{
  return mime_encoder_init (mime_Base64, output_fn, closure);
}

MimeEncoderData *
MimeQPEncoderInit (MimeConverterOutputCallback output_fn,
           void *closure)
{
  return mime_encoder_init (mime_QuotedPrintable, output_fn, closure);
}

MimeEncoderData *
MimeUUEncoderInit (const char *filename,
          MimeConverterOutputCallback output_fn,
          void *closure)
{
  MimeEncoderData *enc = mime_encoder_init (mime_uuencode, output_fn, closure);

  if (filename)
    enc->filename = strdup(filename);

  return enc;
}

int
MimeEncoderWrite (MimeEncoderData *data, const char *buffer, int32_t size)
{
  NS_ASSERTION(data, "1.1 <rhp@netscape.com> 19 Mar 1999 12:00");
  if (!data) return -1;
  switch(data->encoding)
  {
  case mime_Base64:
    return mime_encode_base64_buffer (data, buffer, size);
  case mime_QuotedPrintable:
    return mime_encode_qp_buffer (data, buffer, size);
  default:
    NS_ERROR("Invalid encoding");
    return -1;
  }
}
