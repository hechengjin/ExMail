/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _MIMEOBJ_H_
#define _MIMEOBJ_H_

#include "mimei.h"
#include "prio.h"
/* MimeObject is the base-class for the objects representing all other
   MIME types.  It provides several methods:

   int initialize (MimeObject *obj)

     This is called from mime_new() when a new instance is allocated.
     Subclasses should do whatever setup is necessary from this method,
     and should call the superclass's initialize method, unless there's
     a specific reason not to.

   void finalize (MimeObject *obj)

     This is called from mime_free() and should free all data associated
     with the object.  If the object points to other MIME objects, they
     should be finalized as well (by calling mime_free(), not by calling
     their finalize() methods directly.)

   int parse_buffer (const char *buf, int32_t size, MimeObject *obj)

     This is the method by which you feed arbitrary data into the parser
     for this object.  Most subclasses will probably inherit this method
     from the MimeObject base-class, which line-buffers the data and then
     hands it off to the parse_line() method.

   If this object uses a Content-Transfer-Encoding (base64, qp, uue)
   then the data may be decoded by parse_buffer() before parse_line()
   is called.  (The MimeLeaf class provides this functionality.)

   int parse_begin (MimeObject *obj)
     Called after `init' but before `parse_line' or `parse_buffer'.
   Can be used to initialize various parsing machinery.

   int parse_line (const char *line, int32_t length, MimeObject *obj)

     This method is called (by parse_buffer()) for each complete line of
     data handed to the parser, and is the method which most subclasses
     will override to implement their parsers.

     When handing data off to a MIME object for parsing, one should always
     call the parse_buffer() method, and not call the parse_line() method
     directly, since the parse_buffer() method may do other transformations
     on the data (like base64 decoding.)

   One should generally not call parse_line() directly, since that could
   bypass decoding.  One should call parse_buffer() instead.

   int parse_eof (MimeObject *obj, bool abort_p)

     This is called when there is no more data to be handed to the object:
   when the parent object is done feeding data to an object being parsed.
   Implementors of this method should be sure to also call the parse_eof()
   methods of any sub-objects to which they have pointers.

     This is also called by the finalize() method, just before object
     destruction, if it has not already been called.

     The `closed_p' instance variable is used to prevent multiple calls to
     `parse_eof'.

   int parse_end (MimeObject *obj)
     Called after `parse_eof' but before `finalize'.
   This can be used to free up any memory no longer needed now that parsing
   is done (to avoid surprises due to unexpected method combination, it's
   best to free things in this method in preference to `parse_eof'.)
   Implementors of this method should be sure to also call the parse_end()
   methods of any sub-objects to which they have pointers.

     This is also called by the finalize() method, just before object
     destruction, if it has not already been called.

     The `parsed_p' instance variable is used to prevent multiple calls to
     `parse_end'.


   bool displayable_inline_p (MimeObjectClass *class, MimeHeaders *hdrs)

     This method should return true if this class of object will be displayed
     directly, as opposed to being displayed as a link.  This information is
     used by the "multipart/alternative" parser to decide which of its children
     is the ``best'' one to display.   Note that this is a class method, not
     an object method -- there is not yet an instance of this class at the time
     that it is called.  The `hdrs' provided are the headers of the object that
   might be instantiated -- from this, the method may extract additional
   infomation that it might need to make its decision.
 */


/* this one is typdedef'ed in mimei.h, since it is the base-class. */
struct MimeObjectClass {

  /* Note: the order of these first five slots is known by MimeDefClass().
   Technically, these are part of the object system, not the MIME code.
   */
  const char *class_name;
  int instance_size;
  struct MimeObjectClass *superclass;
  int (*class_initialize) (MimeObjectClass *clazz);
  bool class_initialized;

  /* These are the methods shared by all MIME objects.  See comment above.
   */
  int (*initialize) (MimeObject *obj);
  void (*finalize) (MimeObject *obj);
  int (*parse_begin) (MimeObject *obj);
  int (*parse_buffer) (const char *buf, int32_t size, MimeObject *obj);
  int (*parse_line) (const char *line, int32_t length, MimeObject *obj);
  int (*parse_eof) (MimeObject *obj, bool abort_p);
  int (*parse_end) (MimeObject *obj, bool abort_p);

  bool (*displayable_inline_p) (MimeObjectClass *clazz, MimeHeaders *hdrs);

#if defined(DEBUG) && defined(XP_UNIX)
  int (*debug_print) (MimeObject *obj, PRFileDesc *stream, int32_t depth);
#endif
};

extern "C" MimeObjectClass mimeObjectClass;

/* this one is typdedef'ed in mimei.h, since it is the base-class. */
struct MimeObject {
  MimeObjectClass *clazz;  /* Pointer to class object, for `type-of' */

  MimeHeaders *headers;    /* The header data associated with this object;
                 this is where the content-type, disposition,
                 description, and other meta-data live.

                 For example, the outermost message/rfc822 object
                 would have NULL here (since it has no parent,
                 thus no headers to describe it.)  However, a
                 multipart/mixed object, which was the sole
                 child of that message/rfc822 object, would have
                 here a copy of the headers which began the
                 parent object (the headers which describe the
                 child.)
               */

  char *content_type;    /* The MIME content-type and encoding.  */
  char *encoding;      /* In most cases, these will be the same as the
                 values to be found in the `headers' object,
                 but in some cases, the values in these slots
                 will be more correct than the headers.
               */


  MimeObject *parent;    /* Backpointer to a MimeContainer object. */

  MimeDisplayOptions *options;  /* Display preferences set by caller. */

  bool closed_p;      /* Whether it's done being written to. */
  bool parsed_p;      /* Whether the parser has been shut down. */
  bool output_p;      /* Whether it should be written. */
  bool dontShowAsAttachment; /* Force an object to not be shown as attachment,
                                but when is false, it doesn't mean it will be
                                shown as attachment; specifically, body parts
                                are never shown as attachments. */

  /* Read-buffer and write-buffer (on input, `parse_buffer' uses ibuffer to
   compose calls to `parse_line'; on output, `obuffer' is used in various
   ways by various routines.)  These buffers are created and grow as needed.
   `ibuffer' should be generally be considered hands-off, and `obuffer'
   should generally be considered fair game.
   */
  char *ibuffer, *obuffer;
  int32_t ibuffer_size, obuffer_size;
  int32_t ibuffer_fp, obuffer_fp;
};


#define MimeObject_grow_obuffer(obj, desired_size) \
  (((desired_size) >= (obj)->obuffer_size) ? \
   mime_GrowBuffer ((uint32_t)(desired_size), (uint32_t)sizeof(char), 1024, \
           &(obj)->obuffer, (int32_t*)&(obj)->obuffer_size) \
   : 0)


#endif /* _MIMEOBJ_H_ */
