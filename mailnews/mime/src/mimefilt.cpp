/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*   mimefilt.c --- test harness for libmime.a

   This program reads a message from stdin and writes the output of the MIME
   parser on stdout.

   Parameters can be passed to the parser through the usual URL mechanism:

     mimefilt BASE-URL?headers=all&rot13 < in > out

   Some parameters can't be affected that way, so some additional switches
   may be passed on the command line after the URL:

     -fancy    whether fancy headers should be generated (default)

     -no-fancy  opposite; this uses the headers used in the cases of
        FO_SAVE_AS_TEXT or FO_QUOTE_MESSAGE

     -html    whether we should convert to HTML (like FO_PRESENT);
        this is the default if no ?part= is specified.

     -raw    don't convert to HTML (FO_SAVE_AS);
        this is the default if a ?part= is specified.

     -outline  at the end, print a debugging overview of the MIME structure

   Before any output comes a blurb listing the content-type, charset, and
   various other info that would have been put in the generated URL struct.
   It's printed to the beginning of the output because otherwise this out-
   of-band data would have been lost.  (So the output of this program is,
   in fact, a raw HTTP response.)
 */

#include "mimemsg.h"
#include "prglobal.h"

#include "key.h"
#include "cert.h"
#include "secrng.h"
#include "secmod.h"
#include "pk11func.h"
#include "nsMimeStringResources.h"

#ifndef XP_UNIX
ERROR!  This is a unix-only file for the "mimefilt" standalone program.
    This does not go into libmime.a.
#endif


static char *
test_file_type (const char *filename, void *stream_closure)
{
  const char *suf = PL_strrchr(filename, '.');
  if (!suf)
  return 0;
  suf++;

  if (!PL_strcasecmp(suf, "txt") ||
    !PL_strcasecmp(suf, "text"))
  return strdup("text/plain");
  else if (!PL_strcasecmp(suf, "htm") ||
       !PL_strcasecmp(suf, "html"))
  return strdup("text/html");
  else if (!PL_strcasecmp(suf, "gif"))
  return strdup("image/gif");
  else if (!PL_strcasecmp(suf, "jpg") ||
       !PL_strcasecmp(suf, "jpeg"))
  return strdup("image/jpeg");
  else if (!PL_strcasecmp(suf, "pjpg") ||
       !PL_strcasecmp(suf, "pjpeg"))
  return strdup("image/pjpeg");
  else if (!PL_strcasecmp(suf, "xbm"))
  return strdup("image/x-xbitmap");
  else if (!PL_strcasecmp(suf, "xpm"))
  return strdup("image/x-xpixmap");
  else if (!PL_strcasecmp(suf, "xwd"))
  return strdup("image/x-xwindowdump");
  else if (!PL_strcasecmp(suf, "bmp"))
  return strdup("image/x-MS-bmp");
  else if (!PL_strcasecmp(suf, "au"))
  return strdup("audio/basic");
  else if (!PL_strcasecmp(suf, "aif") ||
       !PL_strcasecmp(suf, "aiff") ||
       !PL_strcasecmp(suf, "aifc"))
  return strdup("audio/x-aiff");
  else if (!PL_strcasecmp(suf, "ps"))
  return strdup("application/postscript");
  else
  return 0;
}

static int
test_output_fn(char *buf, int32_t size, void *closure)
{
  FILE *out = (FILE *) closure;
  if (out)
  return fwrite(buf, sizeof(*buf), size, out);
  else
  return 0;
}

static int
test_output_init_fn (const char *type,
           const char *charset,
           const char *name,
           const char *x_mac_type,
           const char *x_mac_creator,
           void *stream_closure)
{
  FILE *out = (FILE *) stream_closure;
  fprintf(out, "CONTENT-TYPE: %s", type);
  if (charset)
  fprintf(out, "; charset=\"%s\"", charset);
  if (name)
  fprintf(out, "; name=\"%s\"", name);
  if (x_mac_type || x_mac_creator)
  fprintf(out, "; x-mac-type=\"%s\"; x-mac-creator=\"%s\"",
      x_mac_type ? x_mac_type : "",
      x_mac_creator ? x_mac_type : "");
  fprintf(out, CRLF CRLF);
  return 0;
}

static void *
test_image_begin(const char *image_url, const char *content_type,
         void *stream_closure)
{
  return ((void *) strdup(image_url));
}

static void
test_image_end(void *image_closure, int status)
{
  char *url = (char *) image_closure;
  if (url) PR_Free(url);
}

static char *
test_image_make_image_html(void *image_data)
{
  char *url = (char *) image_data;
#if 0
  const char *prefix = "<P><CENTER><IMG SRC=\"";
  const char *suffix = "\"></CENTER><P>";
#else
  const char *prefix = ("<P><CENTER><TABLE BORDER=2 CELLPADDING=20"
            " BGCOLOR=WHITE>"
            "<TR><TD ALIGN=CENTER>"
            "an inlined image would have gone here for<BR>");
  const char *suffix = "</TD></TR></TABLE></CENTER><P>";
#endif
  uint32_t buflen = strlen (prefix) + strlen (suffix) + strlen (url) + 20;
  char *buf = (char *) PR_MALLOC (buflen);
  if (!buf) return 0;
  *buf = 0;
  PL_strcatn (buf, buflen, prefix);
  PL_strcatn (buf, buflen, url);
  PL_strcatn (buf, buflen, suffix);
  return buf;
}

static int test_image_write_buffer(const char *buf, int32_t size, void *image_closure)
{
  return 0;
}

static char *
test_passwd_prompt (PK11SlotInfo *slot, void *wincx)
{
  char buf[2048], *s;
  fprintf(stdout, "#### Password required: ");
  s = fgets(buf, sizeof(buf)-1, stdin);
  if (!s) return s;
  if (s[strlen(s)-1] == '\r' ||
    s[strlen(s)-1] == '\n')
  s[strlen(s)-1] = '\0';
  return s;
}


int
test(FILE *in, FILE *out,
   const char *url,
   bool fancy_headers_p,
   bool html_p,
   bool outline_p,
   bool dexlate_p,
   bool variable_width_plaintext_p)
{
  int status = 0;
  MimeObject *obj = 0;
  MimeDisplayOptions *opt = new MimeDisplayOptions;
//  memset(opt, 0, sizeof(*opt));

  if (dexlate_p) html_p = false;

  opt->fancy_headers_p = fancy_headers_p;
  opt->headers = MimeHeadersSome;
  opt->rot13_p = false;

  status = mime_parse_url_options(url, opt);
  if (status < 0)
  {
    PR_Free(opt);
    return MIME_OUT_OF_MEMORY;
  }

  opt->url          = url;
  opt->write_html_p      = html_p;
  opt->dexlate_p      = dexlate_p;
  opt->output_init_fn    = test_output_init_fn;
  opt->output_fn      = test_output_fn;
  opt->charset_conversion_fn= 0;
  opt->rfc1522_conversion_p = false;
  opt->file_type_fn      = test_file_type;
  opt->stream_closure    = out;

  opt->image_begin      = test_image_begin;
  opt->image_end      = test_image_end;
  opt->make_image_html    = test_image_make_image_html;
  opt->image_write_buffer  = test_image_write_buffer;

  opt->variable_width_plaintext_p = variable_width_plaintext_p;

  obj = mime_new ((MimeObjectClass *)&mimeMessageClass,
          (MimeHeaders *) NULL,
          MESSAGE_RFC822);
  if (!obj)
  {
    PR_Free(opt);
    return MIME_OUT_OF_MEMORY;
  }
  obj->options = opt;

  status = obj->class->initialize(obj);
  if (status >= 0)
  status = obj->class->parse_begin(obj);
  if (status < 0)
  {
    PR_Free(opt);
    PR_Free(obj);
    return MIME_OUT_OF_MEMORY;
  }

  while (1)
  {
    char buf[255];
    int size = fread(buf, sizeof(*buf), sizeof(buf), stdin);
    if (size <= 0) break;
    status = obj->class->parse_buffer(buf, size, obj);
    if (status < 0)
    {
      mime_free(obj);
      PR_Free(opt);
      return status;
    }
  }

  status = obj->class->parse_eof(obj, false);
  if (status >= 0)
  status = obj->class->parse_end(obj, false);
  if (status < 0)
  {
    mime_free(obj);
    PR_Free(opt);
    return status;
  }

  if (outline_p)
  {
    fprintf(out, "\n\n"
      "###############################################################\n");
    obj->class->debug_print(obj, stderr, 0);
    fprintf(out,
      "###############################################################\n");
  }

  mime_free (obj);
  PR_Free(opt);
  return 0;
}


static char *
test_cdb_name_cb (void *arg, int vers)
{
  static char f[1024];
  if (vers <= 4)
  sprintf(f, "%s/.netscape/cert.db", getenv("HOME"));
  else
  sprintf(f, "%s/.netscape/cert%d.db", getenv("HOME"), vers);
  return f;
}

static char *
test_kdb_name_cb (void *arg, int vers)
{
  static char f[1024];
  if (vers <= 2)
  sprintf(f, "%s/.netscape/key.db", getenv("HOME"));
  else
  sprintf(f, "%s/.netscape/key%d.db", getenv("HOME"), vers);
  return f;
}

extern void SEC_Init(void);

int
main (int argc, char **argv)
{
  int32_t i = 1;
  char *url = "";
  bool fancy_p = true;
  bool html_p = true;
  bool outline_p = false;
  bool dexlate_p = false;
  char filename[1000];
  CERTCertDBHandle *cdb_handle;
  SECKEYKeyDBHandle *kdb_handle;

  PR_Init("mimefilt", 24, 1, 0);

  cdb_handle = (CERTCertDBHandle *)  calloc(1, sizeof(*cdb_handle));

  if (SECSuccess != CERT_OpenCertDB(cdb_handle, false, test_cdb_name_cb, NULL))
  CERT_OpenVolatileCertDB(cdb_handle);
  CERT_SetDefaultCertDB(cdb_handle);

  RNG_RNGInit();

  kdb_handle = SECKEY_OpenKeyDB(false, test_kdb_name_cb, NULL);
  SECKEY_SetDefaultKeyDB(kdb_handle);

  PK11_SetPasswordFunc(test_passwd_prompt);

  sprintf(filename, "%s/.netscape/secmodule.db", getenv("HOME"));
  SECMOD_init(filename);

  SEC_Init();


  if (i < argc)
  {
    if (argv[i][0] == '-')
    url = strdup("");
    else
    url = argv[i++];
  }

  if (url &&
    (PL_strstr(url, "?part=") ||
     PL_strstr(url, "&part=")))
  html_p = false;

  while (i < argc)
  {
    if (!strcmp(argv[i], "-fancy"))
    fancy_p = true;
    else if (!strcmp(argv[i], "-no-fancy"))
    fancy_p = false;
    else if (!strcmp(argv[i], "-html"))
    html_p = true;
    else if (!strcmp(argv[i], "-raw"))
    html_p = false;
    else if (!strcmp(argv[i], "-outline"))
    outline_p = true;
    else if (!strcmp(argv[i], "-dexlate"))
    dexlate_p = true;
    else
    {
      fprintf(stderr,
      "usage: %s [ URL [ -fancy | -no-fancy | -html | -raw | -outline | -dexlate ]]\n"
          "     < message/rfc822 > output\n",
          (PL_strrchr(argv[0], '/') ?
           PL_strrchr(argv[0], '/') + 1 :
           argv[0]));
      i = 1;
      goto FAIL;
    }
    i++;
  }

  i = test(stdin, stdout, url, fancy_p, html_p, outline_p, dexlate_p, true);
  fprintf(stdout, "\n");
  fflush(stdout);

 FAIL:

  CERT_ClosePermCertDB(cdb_handle);
  SECKEY_CloseKeyDB(kdb_handle);

  exit(i);
}
