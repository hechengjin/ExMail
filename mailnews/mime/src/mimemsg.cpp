/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsIMimeEmitter.h"
#include "mimemsg.h"
#include "mimemoz2.h"
#include "prmem.h"
#include "prio.h"
#include "plstr.h"
#include "msgCore.h"
#include "prlog.h"
#include "prprf.h"
#include "nsMimeStringResources.h"
#include "nsMimeTypes.h"
#include "nsMsgMessageFlags.h"
#include "nsStringGlue.h"
#include "mimetext.h"
#include "mimecryp.h"
#include "mimetpfl.h"
#include "nsINetUtil.h"
#include "nsMsgUtils.h"
#include "nsMsgI18N.h"

#define MIME_SUPERCLASS mimeContainerClass
MimeDefClass(MimeMessage, MimeMessageClass, mimeMessageClass,
       &MIME_SUPERCLASS);

static int MimeMessage_initialize (MimeObject *);
static void MimeMessage_finalize (MimeObject *);
static int MimeMessage_add_child (MimeObject *, MimeObject *);
static int MimeMessage_parse_begin (MimeObject *);
static int MimeMessage_parse_line (const char *, int32_t, MimeObject *);
static int MimeMessage_parse_eof (MimeObject *, bool);
static int MimeMessage_close_headers (MimeObject *obj);
static int MimeMessage_write_headers_html (MimeObject *);
static char *MimeMessage_partial_message_html(const char *data,
                        void *closure,
                        MimeHeaders *headers);

#ifdef XP_UNIX
extern void MimeHeaders_do_unix_display_hook_hack(MimeHeaders *);
#endif /* XP_UNIX */

#if defined(DEBUG) && defined(XP_UNIX)
static int MimeMessage_debug_print (MimeObject *, PRFileDesc *, int32_t depth);
#endif

extern MimeObjectClass mimeMultipartClass;

static int
MimeMessageClassInitialize(MimeMessageClass *clazz)
{
  MimeObjectClass    *oclass = (MimeObjectClass *)    clazz;
  MimeContainerClass *cclass = (MimeContainerClass *) clazz;

  PR_ASSERT(!oclass->class_initialized);
  oclass->initialize  = MimeMessage_initialize;
  oclass->finalize    = MimeMessage_finalize;
  oclass->parse_begin = MimeMessage_parse_begin;
  oclass->parse_line  = MimeMessage_parse_line;
  oclass->parse_eof   = MimeMessage_parse_eof;
  cclass->add_child   = MimeMessage_add_child;

#if defined(DEBUG) && defined(XP_UNIX)
  oclass->debug_print = MimeMessage_debug_print;
#endif
  return 0;
}


static int
MimeMessage_initialize (MimeObject *object)
{
  MimeMessage *msg = (MimeMessage *)object;
  msg->grabSubject = false;
  msg->bodyLength = 0;
  msg->sizeSoFar = 0;

  return ((MimeObjectClass*)&MIME_SUPERCLASS)->initialize(object);
}

static void
MimeMessage_finalize (MimeObject *object)
{
  MimeMessage *msg = (MimeMessage *)object;
  if (msg->hdrs)
  MimeHeaders_free(msg->hdrs);
  msg->hdrs = 0;
  ((MimeObjectClass*)&MIME_SUPERCLASS)->finalize(object);
}

static int
MimeMessage_parse_begin (MimeObject *obj)
{
  MimeMessage *msg = (MimeMessage *)obj;

  int status = ((MimeObjectClass*)&MIME_SUPERCLASS)->parse_begin(obj);
  if (status < 0) return status;

  if (obj->parent)
  {
    msg->grabSubject = true;
  }

  /* Messages have separators before the headers, except for the outermost
   message. */
  return MimeObject_write_separator(obj);
}


static int
MimeMessage_parse_line (const char *aLine, int32_t aLength, MimeObject *obj)
{
  const char * line = aLine;
  int32_t length = aLength;

  MimeMessage *msg = (MimeMessage *) obj;
  int status = 0;

  NS_ASSERTION(line && *line, "empty line in mime msg parse_line");
  if (!line || !*line) return -1;

  msg->sizeSoFar += length;

  if (msg->grabSubject)
  {
    if ( (!PL_strncasecmp(line, "Subject: ", 9)) && (obj->parent) )
    {
      if ( (obj->headers) && (!obj->headers->munged_subject) )
      {
        obj->headers->munged_subject = (char *) PL_strndup(line + 9, length - 9);
        char *tPtr = obj->headers->munged_subject;
        while (*tPtr)
        {
          if ( (*tPtr == '\r') || (*tPtr == '\n') )
          {
            *tPtr = '\0';
            break;
          }
          tPtr++;
        }
      }
    }
  }

  /* If we already have a child object, then we're done parsing headers,
   and all subsequent lines get passed to the inferior object without
   further processing by us.  (Our parent will stop feeding us lines
   when this MimeMessage part is out of data.)
   */
  if (msg->container.nchildren)
  {
    MimeObject *kid = msg->container.children[0];
    bool nl;
    PR_ASSERT(kid);
    if (!kid) return -1;

    msg->bodyLength += length;

    /* Don't allow MimeMessage objects to not end in a newline, since it
     would be inappropriate for any following part to appear on the same
     line as the last line of the message.

     #### This assumes that the only time the `parse_line' method is
     called with a line that doesn't end in a newline is when that line
     is the last line.
     */
    nl = (length > 0 && (line[length-1] == '\r' || line[length-1] == '\n'));

#ifdef MIME_DRAFTS
    if ( !mime_typep (kid, (MimeObjectClass*) &mimeMessageClass) &&
       obj->options &&
       obj->options->decompose_file_p &&
       ! obj->options->is_multipart_msg &&
       obj->options->decompose_file_output_fn )
    {
      if (!obj->options->decrypt_p) {
        //if we are processing a flowed plain text line, we need to remove any stuffed space
        if (length > 0 && ' ' == *line && mime_typep(kid, (MimeObjectClass *)&mimeInlineTextPlainFlowedClass))
        {
          line ++;
          length --;
        }
        status = obj->options->decompose_file_output_fn (line,
                                 length,
                           obj->options->stream_closure);
        if (status < 0) return status;
        if (!nl) {
        status = obj->options->decompose_file_output_fn (MSG_LINEBREAK,
                                 MSG_LINEBREAK_LEN,
                           obj->options->stream_closure);
        if (status < 0) return status;
        }
        return status;
      }
    }
#endif /* MIME_DRAFTS */


    if (nl)
    return kid->clazz->parse_buffer (line, length, kid);
    else
    {
      /* Hack a newline onto the end. */
      char *s = (char *)PR_MALLOC(length + MSG_LINEBREAK_LEN + 1);
      if (!s) return MIME_OUT_OF_MEMORY;
      memcpy(s, line, length);
      PL_strncpyz(s + length, MSG_LINEBREAK, MSG_LINEBREAK_LEN + 1);
      status = kid->clazz->parse_buffer (s, length + MSG_LINEBREAK_LEN, kid);
      PR_Free(s);
      return status;
    }
  }

  /* Otherwise we don't yet have a child object, which means we're not
   done parsing our headers yet.
   */
  if (!msg->hdrs)
  {
    msg->hdrs = MimeHeaders_new();
    if (!msg->hdrs) return MIME_OUT_OF_MEMORY;
  }

#ifdef MIME_DRAFTS
  if ( obj->options &&
       obj->options->decompose_file_p &&
       ! obj->options->is_multipart_msg &&
       obj->options->done_parsing_outer_headers &&
       obj->options->decompose_file_output_fn )
  {
    status =  obj->options->decompose_file_output_fn( line, length,
                                                      obj->options->stream_closure );
    if (status < 0)
      return status;
  }
#endif /* MIME_DRAFTS */

  status = MimeHeaders_parse_line(line, length, msg->hdrs);
  if (status < 0) return status;

  /* If this line is blank, we're now done parsing headers, and should
   examine our content-type to create our "body" part.
   */
  if (*line == '\r' || *line == '\n')
  {
    status = MimeMessage_close_headers(obj);
    if (status < 0) return status;
  }

  return 0;
}

static int
MimeMessage_close_headers (MimeObject *obj)
{
  MimeMessage *msg = (MimeMessage *) obj;
  int status = 0;
  char *ct = 0;      /* Content-Type header */
  MimeObject *body;

  // Do a proper decoding of the munged subject.
  if (obj->headers && msg->hdrs && msg->grabSubject && obj->headers->munged_subject) {
    // nsMsgI18NConvertToUnicode wants nsAStrings...
    nsDependentCString orig(obj->headers->munged_subject);
    nsAutoString dest;
    // First, get the Content-Type, then extract the charset="whatever" part of
    // it.
    nsCString charset;
    nsCString contentType;
    contentType.Adopt(MimeHeaders_get(msg->hdrs, HEADER_CONTENT_TYPE, false, false));
    if (!contentType.IsEmpty())
      charset.Adopt(MimeHeaders_get_parameter(contentType.get(), "charset", nullptr, nullptr));

    // If we've got a charset, use nsMsgI18NConvertToUnicode to magically decode
    // the munged subject.
    if (!charset.IsEmpty()) {
      nsresult rv = nsMsgI18NConvertToUnicode(charset.get(), orig, dest);
      // If we managed to convert the string, replace munged_subject with the
      // UTF8 version of it, otherwise, just forget about it (maybe there was an
      // improperly encoded string in there).
      PR_Free(obj->headers->munged_subject);
      if (NS_SUCCEEDED(rv))
        obj->headers->munged_subject = ToNewUTF8String(dest);
      else
        obj->headers->munged_subject = nullptr;
    } else {
      PR_Free(obj->headers->munged_subject);
      obj->headers->munged_subject = nullptr;
    }
  }

  if (msg->hdrs)
  {
    bool outer_p = !obj->headers; /* is this the outermost message? */


#ifdef MIME_DRAFTS
    if (outer_p &&
      obj->options &&
          (obj->options->decompose_file_p || obj->options->caller_need_root_headers) &&
      obj->options->decompose_headers_info_fn)
    {
#ifdef ENABLE_SMIME
      if (obj->options->decrypt_p && !mime_crypto_object_p (msg->hdrs, false))
        obj->options->decrypt_p = false;
#endif /* ENABLE_SMIME */
      if (!obj->options->caller_need_root_headers || (obj == obj->options->state->root))
        status = obj->options->decompose_headers_info_fn (
                         obj->options->stream_closure,
                               msg->hdrs );
    }
#endif /* MIME_DRAFTS */


    /* If this is the outermost message, we need to run the
     `generate_header' callback.  This happens here instead of
     in `parse_begin', because it's only now that we've parsed
     our headers.  However, since this is the outermost message,
     we have yet to write any HTML, so that's fine.
     */
    if (outer_p &&
      obj->output_p &&
      obj->options &&
      obj->options->write_html_p &&
      obj->options->generate_header_html_fn)
    {
      int lstatus = 0;
      char *html = 0;

      /* The generate_header_html_fn might return HTML, so it's important
       that the output stream be set up with the proper type before we
       make the MimeObject_write() call below. */
      if (!obj->options->state->first_data_written_p)
      {
        lstatus = MimeObject_output_init (obj, TEXT_HTML);
        if (lstatus < 0) return lstatus;
        PR_ASSERT(obj->options->state->first_data_written_p);
      }

      html = obj->options->generate_header_html_fn(NULL,
                           obj->options->html_closure,
                             msg->hdrs);
      if (html)
      {
        lstatus = MimeObject_write(obj, html, strlen(html), false);
        PR_Free(html);
        if (lstatus < 0) return lstatus;
      }
    }


    /* Find the content-type of the body of this message.
     */
    {
    bool ok = true;
    char *mv = MimeHeaders_get (msg->hdrs, HEADER_MIME_VERSION,
                  true, false);

#ifdef REQUIRE_MIME_VERSION_HEADER
    /* If this is the outermost message, it must have a MIME-Version
       header with the value 1.0 for us to believe what might be in
       the Content-Type header.  If the MIME-Version header is not
       present, we must treat this message as untyped.
     */
    ok = (mv && !strcmp(mv, "1.0"));
#else
    /* #### actually, we didn't check this in Mozilla 2.0, and checking
       it now could cause some compatibility nonsense, so for now, let's
       just believe any Content-Type header we see.
     */
    ok = true;
#endif

    if (ok)
      {
      ct = MimeHeaders_get (msg->hdrs, HEADER_CONTENT_TYPE, true, false);

      /* If there is no Content-Type header, but there is a MIME-Version
         header, then assume that this *is* in fact a MIME message.
         (I've seen messages with

          MIME-Version: 1.0
          Content-Transfer-Encoding: quoted-printable

         and no Content-Type, and we should treat those as being of type
         MimeInlineTextPlain rather than MimeUntypedText.)
       */
      if (mv && !ct)
        ct = strdup(TEXT_PLAIN);
      }

    PR_FREEIF(mv);  /* done with this now. */
    }

    /* If this message has a body which is encrypted and we're going to
       decrypt it (whithout converting it to HTML, since decrypt_p and
       write_html_p are never true at the same time)
    */
    if (obj->output_p &&
        obj->options &&
        obj->options->decrypt_p
#ifdef ENABLE_SMIME
        && !mime_crypto_object_p (msg->hdrs, false)
#endif /* ENABLE_SMIME */
        )
    {
      /* The body of this message is not an encrypted object, so we need
         to turn off the decrypt_p flag (to prevent us from s#$%ing the
         body of the internal object up into one.) In this case,
         our output will end up being identical to our input.
      */
      obj->options->decrypt_p = false;
    }

    /* Emit the HTML for this message's headers.  Do this before
     creating the object representing the body.
     */
    if (obj->output_p &&
      obj->options &&
      obj->options->write_html_p)
    {
      /* If citation headers are on, and this is not the outermost message,
       turn them off. */
      if (obj->options->headers == MimeHeadersCitation && !outer_p)
      obj->options->headers = MimeHeadersSome;

      /* Emit a normal header block. */
      status = MimeMessage_write_headers_html(obj);
      if (status < 0) return status;
    }
    else if (obj->output_p)
    {
      /* Dump the headers, raw. */
      status = MimeObject_write(obj, "", 0, false);  /* initialize */
      if (status < 0) return status;
      status = MimeHeaders_write_raw_headers(msg->hdrs, obj->options,
                         obj->options->decrypt_p);
      if (status < 0) return status;
    }

#ifdef XP_UNIX
    if (outer_p && obj->output_p)
    /* Kludge from mimehdrs.c */
    MimeHeaders_do_unix_display_hook_hack(msg->hdrs);
#endif /* XP_UNIX */
  }

  /* Never put out a separator after a message header block. */
  if (obj->options && obj->options->state)
  obj->options->state->separator_suppressed_p = true;

#ifdef MIME_DRAFTS
  if ( !obj->headers &&    /* outer most message header */
     obj->options &&
     obj->options->decompose_file_p &&
     ct )
  obj->options->is_multipart_msg = PL_strcasestr(ct, "multipart/") != NULL;
#endif /* MIME_DRAFTS */


  body = mime_create(ct, msg->hdrs, obj->options);

  PR_FREEIF(ct);
  if (!body) return MIME_OUT_OF_MEMORY;
  status = ((MimeContainerClass *) obj->clazz)->add_child (obj, body);
  if (status < 0)
  {
    mime_free(body);
    return status;
  }

  // Only do this if this is a Text Object!
  if ( mime_typep(body, (MimeObjectClass *) &mimeInlineTextClass) )
  {
    ((MimeInlineText *) body)->needUpdateMsgWinCharset = true;
  }

  /* Now that we've added this new object to our list of children,
   start its parser going. */
  status = body->clazz->parse_begin(body);
  if (status < 0) return status;

  // Now notify the emitter if this is the outer most message, unless
  // it is a part that is not the head of the message. If it's a part,
  // we need to figure out the content type/charset of the part
  //
  bool outer_p = !obj->headers;  /* is this the outermost message? */

  if ( (outer_p || obj->options->notify_nested_bodies) &&
       (!obj->options->part_to_load || obj->options->format_out == nsMimeOutput::nsMimeMessageBodyDisplay))
  {
    // call SetMailCharacterSetToMsgWindow() to set a menu charset
    if (mime_typep(body, (MimeObjectClass *) &mimeInlineTextClass))
    {
      MimeInlineText  *text = (MimeInlineText *) body;
      if (text && text->charset && *text->charset)
        SetMailCharacterSetToMsgWindow(body, text->charset);
    }

    char  *msgID = MimeHeaders_get (msg->hdrs, HEADER_MESSAGE_ID,
                                    false, false);

    const char  *outCharset = NULL;
    if (!obj->options->force_user_charset)  /* Only convert if the user prefs is false */
      outCharset = "UTF-8";

    mimeEmitterStartBody(obj->options, (obj->options->headers == MimeHeadersNone), msgID, outCharset);
    PR_FREEIF(msgID);

  // setting up truncated message html fotter function
  char *xmoz = MimeHeaders_get(msg->hdrs, HEADER_X_MOZILLA_STATUS, false,
                 false);
  if (xmoz)
  {
    uint32_t flags = 0;
    char dummy = 0;
    if (sscanf(xmoz, " %x %c", &flags, &dummy) == 1 &&
      flags & nsMsgMessageFlags::Partial)
    {
      obj->options->html_closure = obj;
      obj->options->generate_footer_html_fn =
        MimeMessage_partial_message_html;
    }
    PR_FREEIF(xmoz);
  }
  }

  return 0;
}



static int
MimeMessage_parse_eof (MimeObject *obj, bool abort_p)
{
  int status;
  bool outer_p;
  MimeMessage *msg = (MimeMessage *)obj;
  if (obj->closed_p) return 0;

  /* Run parent method first, to flush out any buffered data. */
  status = ((MimeObjectClass*)&MIME_SUPERCLASS)->parse_eof(obj, abort_p);
  if (status < 0) return status;

  outer_p = !obj->headers;  /* is this the outermost message? */

  // Hack for messages with truncated headers (bug 244722)
  // If there is no empty line in a message, the parser can't figure out where
  // the headers end, causing parsing to hang. So we insert an extra newline
  // to keep it happy. This is OK, since a message without any empty lines is
  // broken anyway...
  if(outer_p && msg->hdrs && ! msg->hdrs->done_p) {
    MimeMessage_parse_line("\n", 1, obj);
  }

  // Once we get to the end of parsing the message, we will notify
  // the emitter that we are done the the body.

  // Mark the end of the mail body if we are actually emitting the
  // body of the message (i.e. not Header ONLY)
  if ((outer_p || obj->options->notify_nested_bodies) && obj->options &&
      obj->options->write_html_p)
  {
    if (obj->options->generate_footer_html_fn)
    {
      mime_stream_data *msd =
        (mime_stream_data *) obj->options->stream_closure;
      if (msd)
      {
        char *html = obj->options->generate_footer_html_fn
          (msd->orig_url_name, obj->options->html_closure, msg->hdrs);
        if (html)
        {
          int lstatus = MimeObject_write(obj, html,
                         strlen(html),
                         false);
          PR_Free(html);
          if (lstatus < 0) return lstatus;
        }
      }
    }
    if ((!obj->options->part_to_load  || obj->options->format_out == nsMimeOutput::nsMimeMessageBodyDisplay) &&
      obj->options->headers != MimeHeadersOnly)
      mimeEmitterEndBody(obj->options);
  }

#ifdef MIME_DRAFTS
  if ( obj->options &&
     obj->options->decompose_file_p &&
     obj->options->done_parsing_outer_headers &&
     ! obj->options->is_multipart_msg &&
     ! mime_typep(obj, (MimeObjectClass*) &mimeEncryptedClass) &&
     obj->options->decompose_file_close_fn ) {
  status = obj->options->decompose_file_close_fn (
                         obj->options->stream_closure );

  if ( status < 0 ) return status;
  }
#endif /* MIME_DRAFTS */


  /* Put out a separator after every message/rfc822 object. */
  if (!abort_p && !outer_p)
  {
    status = MimeObject_write_separator(obj);
    if (status < 0) return status;
  }

  return 0;
}


static int
MimeMessage_add_child (MimeObject *parent, MimeObject *child)
{
  MimeContainer *cont = (MimeContainer *) parent;
  PR_ASSERT(parent && child);
  if (!parent || !child) return -1;

  /* message/rfc822 containers can only have one child. */
  PR_ASSERT(cont->nchildren == 0);
  if (cont->nchildren != 0) return -1;

#ifdef MIME_DRAFTS
  if ( parent->options &&
     parent->options->decompose_file_p &&
     ! parent->options->is_multipart_msg &&
     ! mime_typep(child, (MimeObjectClass*) &mimeEncryptedClass) &&
     parent->options->decompose_file_init_fn ) {
  int status = 0;
  status = parent->options->decompose_file_init_fn (
                        parent->options->stream_closure,
                        ((MimeMessage*)parent)->hdrs );
  if ( status < 0 ) return status;
  }
#endif /* MIME_DRAFTS */

  return ((MimeContainerClass*)&MIME_SUPERCLASS)->add_child (parent, child);
}

// This is necessary to determine which charset to use for a reply/forward
char *
DetermineMailCharset(MimeMessage *msg)
{
  char          *retCharset = nullptr;

  if ( (msg) && (msg->hdrs) )
  {
    char *ct = MimeHeaders_get (msg->hdrs, HEADER_CONTENT_TYPE,
                                false, false);
    if (ct)
    {
      retCharset = MimeHeaders_get_parameter (ct, "charset", NULL, NULL);
      PR_Free(ct);
    }

    if (!retCharset)
    {
      // If we didn't find "Content-Type: ...; charset=XX" then look
      // for "X-Sun-Charset: XX" instead.  (Maybe this should be done
      // in MimeSunAttachmentClass, but it's harder there than here.)
      retCharset = MimeHeaders_get (msg->hdrs, HEADER_X_SUN_CHARSET,
                                    false, false);
    }
  }

  if (!retCharset)
    return strdup("ISO-8859-1");
  else
    return retCharset;
}

static int
MimeMessage_write_headers_html (MimeObject *obj)
{
  MimeMessage     *msg = (MimeMessage *) obj;
  int             status;

  if (!obj->options || !obj->options->output_fn)
    return 0;

  PR_ASSERT(obj->output_p && obj->options->write_html_p);

  // To support the no header option! Make sure we are not
  // suppressing headers on included email messages...
  if ( (obj->options->headers == MimeHeadersNone) &&
       (obj == obj->options->state->root) )
  {
    // Ok, we are going to kick the Emitter for a StartHeader
    // operation ONLY WHEN THE CHARSET OF THE ORIGINAL MESSAGE IS
    // NOT US-ASCII ("ISO-8859-1")
    //
    // This is only to notify the emitter of the charset of the
    // original message
    char    *mailCharset = DetermineMailCharset(msg);

    if ( (mailCharset) && (PL_strcasecmp(mailCharset, "US-ASCII")) &&
         (PL_strcasecmp(mailCharset, "ISO-8859-1")) )
      mimeEmitterUpdateCharacterSet(obj->options, mailCharset);
    PR_FREEIF(mailCharset);
    return 0;
  }

  if (!obj->options->state->first_data_written_p)
  {
    status = MimeObject_output_init (obj, TEXT_HTML);
    if (status < 0)
    {
      mimeEmitterEndHeader(obj->options, obj);
      return status;
    }
    PR_ASSERT(obj->options->state->first_data_written_p);
  }

  // Start the header parsing by the emitter
  char *msgID = MimeHeaders_get (msg->hdrs, HEADER_MESSAGE_ID,
                                    false, false);
  bool outer_p = !obj->headers; /* is this the outermost message? */
  if (!outer_p && obj->options->format_out == nsMimeOutput::nsMimeMessageBodyDisplay &&
      obj->options->part_to_load)
  {
    //Maybe we are displaying a embedded message as outer part!
    char *id = mime_part_address(obj);
    if (id)
    {
      outer_p = !strcmp(id, obj->options->part_to_load);
      PR_Free(id);
    }
  }

  // Ok, we should really find out the charset of this part. We always
  // output UTF-8 for display, but the original charset is necessary for
  // reply and forward operations.
  //
  char    *mailCharset = DetermineMailCharset(msg);
  mimeEmitterStartHeader(obj->options,
                            outer_p,
                            (obj->options->headers == MimeHeadersOnly),
                            msgID,
                            mailCharset);

  // Change the default_charset by the charset of the original message
  // ONLY WHEN THE CHARSET OF THE ORIGINAL MESSAGE IS NOT US-ASCII
  // ("ISO-8859-1") and default_charset and mailCharset are different,
  // or when there is no default_charset (this can happen with saved messages).
  if ( (!obj->options->default_charset ||
        ((mailCharset) && (PL_strcasecmp(mailCharset, "US-ASCII")) &&
         (PL_strcasecmp(mailCharset, "ISO-8859-1")) &&
         (PL_strcasecmp(obj->options->default_charset, mailCharset)))) &&
       !obj->options->override_charset )
  {
    PR_FREEIF(obj->options->default_charset);
    obj->options->default_charset = strdup(mailCharset);
  }

  PR_FREEIF(msgID);
  PR_FREEIF(mailCharset);

  status = MimeHeaders_write_all_headers (msg->hdrs, obj->options, false);
  if (status < 0)
  {
    mimeEmitterEndHeader(obj->options, obj);
    return status;
  }

  if (!msg->crypto_stamped_p)
  {
  /* If we're not writing a xlation stamp, and this is the outermost
  message, then now is the time to run the post_header_html_fn.
  (Otherwise, it will be run when the xlation-stamp is finally
  closed off, in MimeXlateed_emit_buffered_child() or
  MimeMultipartSigned_emit_child().)
     */
    if (obj->options &&
      obj->options->state &&
      obj->options->generate_post_header_html_fn &&
      !obj->options->state->post_header_html_run_p)
    {
      char *html = 0;
      PR_ASSERT(obj->options->state->first_data_written_p);
      html = obj->options->generate_post_header_html_fn(NULL,
                                          obj->options->html_closure,
                                          msg->hdrs);
      obj->options->state->post_header_html_run_p = true;
      if (html)
      {
        status = MimeObject_write(obj, html, strlen(html), false);
        PR_Free(html);
        if (status < 0)
        {
          mimeEmitterEndHeader(obj->options, obj);
          return status;
        }
      }
    }
  }

  mimeEmitterEndHeader(obj->options, obj);

  // rhp:
  // For now, we are going to parse the entire message, even if we are
  // only interested in headers...why? Well, because this is the only
  // way to build the attachment list. Now we will have the attachment
  // list in the output being created by the XML emitter. If we ever
  // want to go back to where we were before, just uncomment the conditional
  // and it will stop at header parsing.
  //
  // if (obj->options->headers == MimeHeadersOnly)
  //   return -1;
  // else

  return 0;
}

static char *
MimeMessage_partial_message_html(const char *data, void *closure,
                                 MimeHeaders *headers)
{
  MimeMessage *msg = (MimeMessage *)closure;
  nsCAutoString orig_url(data);
  char *uidl = MimeHeaders_get(headers, HEADER_X_UIDL, false, false);
  char *msgId = MimeHeaders_get(headers, HEADER_MESSAGE_ID, false,
                  false);
  char *msgIdPtr = PL_strchr(msgId, '<');

  int32_t pos = orig_url.Find("mailbox-message");
  if (pos != -1)
    orig_url.Cut(pos + 7, 8);

  pos = orig_url.FindChar('#');
  if (pos != -1)
    orig_url.Replace(pos, 1, "?number=", 8);

  if (msgIdPtr)
    msgIdPtr++;
  else
    msgIdPtr = msgId;
  char *gtPtr = PL_strchr(msgIdPtr, '>');
  if (gtPtr)
    *gtPtr = 0;

  bool msgBaseTruncated = (msg->bodyLength > MSG_LINEBREAK_LEN);

  nsCString partialMsgHtml;
  nsCString item;

  partialMsgHtml.AppendLiteral("<div style=\"margin: 1em auto; border: 1px solid black; width: 80%\">");
  partialMsgHtml.AppendLiteral("<div style=\"margin: 5px; padding: 10px; border: 1px solid gray; font-weight: bold; text-align: center;\">");

  partialMsgHtml.AppendLiteral("<span style=\"font-size: 120%;\">");
  if (msgBaseTruncated)
    item.Adopt(MimeGetStringByName(NS_LITERAL_STRING("MIME_MSG_PARTIAL_TRUNCATED").get()));
  else
    item.Adopt(MimeGetStringByName(NS_LITERAL_STRING("MIME_MSG_PARTIAL_NOT_DOWNLOADED").get()));
  partialMsgHtml += item;
  partialMsgHtml.AppendLiteral("</span><hr>");

  if (msgBaseTruncated)
    item.Adopt(MimeGetStringByName(NS_LITERAL_STRING("MIME_MSG_PARTIAL_TRUNCATED_EXPLANATION").get()));
  else
    item.Adopt(MimeGetStringByName(NS_LITERAL_STRING("MIME_MSG_PARTIAL_NOT_DOWNLOADED_EXPLANATION").get()));
  partialMsgHtml += item;
  partialMsgHtml.AppendLiteral("<br><br>");

  partialMsgHtml.AppendLiteral("<a href=\"");
  partialMsgHtml.Append(orig_url);

  if (msgIdPtr) {
    partialMsgHtml.AppendLiteral("&messageid=");

    MsgEscapeString(nsDependentCString(msgIdPtr), nsINetUtil::ESCAPE_URL_PATH,
                    item);

    partialMsgHtml.Append(item);
  }

  if (uidl) {
    partialMsgHtml.AppendLiteral("&uidl=");

    MsgEscapeString(nsDependentCString(uidl), nsINetUtil::ESCAPE_XALPHAS,
                    item);

    partialMsgHtml.Append(item);
  }

  partialMsgHtml.AppendLiteral("\">");
  item.Adopt(MimeGetStringByName(NS_LITERAL_STRING("MIME_MSG_PARTIAL_CLICK_FOR_REST").get()));
  partialMsgHtml += item;
  partialMsgHtml.AppendLiteral("</a>");

  partialMsgHtml.AppendLiteral("</div></div>");

  return ToNewCString(partialMsgHtml);
}

#if defined(DEBUG) && defined(XP_UNIX)
static int
MimeMessage_debug_print (MimeObject *obj, PRFileDesc *stream, int32_t depth)
{
  MimeMessage *msg = (MimeMessage *) obj;
  char *addr = mime_part_address(obj);
  int i;
  for (i=0; i < depth; i++)
  PR_Write(stream, "  ", 2);
/*
  fprintf(stream, "<%s %s%s 0x%08X>\n",
      obj->clazz->class_name,
      addr ? addr : "???",
      (msg->container.nchildren == 0 ? " (no body)" : ""),
      (uint32_t) msg);
*/
  PR_FREEIF(addr);

#if 0
  if (msg->hdrs)
  {
    char *s;

    depth++;

# define DUMP(HEADER) \
    for (i=0; i < depth; i++)                        \
        PR_Write(stream, "  ", 2);                        \
    s = MimeHeaders_get (msg->hdrs, HEADER, false, true);
/**
    \
    PR_Write(stream, HEADER ": %s\n", s ? s : "");              \
**/

    PR_FREEIF(s)

      DUMP(HEADER_SUBJECT);
      DUMP(HEADER_DATE);
      DUMP(HEADER_FROM);
      DUMP(HEADER_TO);
      /* DUMP(HEADER_CC); */
      DUMP(HEADER_NEWSGROUPS);
      DUMP(HEADER_MESSAGE_ID);
# undef DUMP

    PR_Write(stream, "\n", 1);
  }
#endif

  PR_ASSERT(msg->container.nchildren <= 1);
  if (msg->container.nchildren == 1)
  {
    MimeObject *kid = msg->container.children[0];
    int status = kid->clazz->debug_print (kid, stream, depth+1);
    if (status < 0) return status;
  }
  return 0;
}
#endif
