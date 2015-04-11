/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 * This Original Code has been modified by IBM Corporation. Modifications made by IBM
 * described herein are Copyright (c) International Business Machines Corporation, 2000.
 * Modifications to Mozilla code or documentation identified per MPL Section 3.3
 *
 * Date             Modified by     Description of modification
 * 04/20/2000       IBM Corp.      OS/2 VisualAge build.
 */
#include "nsCOMPtr.h"
#include "modmimee.h"
#include "mimeobj.h"
#include "modlmime.h"
#include "mimei.h"
#include "mimebuf.h"
#include "mimemoz2.h"
#include "mimemsg.h"
#include "nsMimeTypes.h"
#include <ctype.h>

#include "prmem.h"
#include "plstr.h"
#include "prprf.h"
#include "prio.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "msgCore.h"
#include "nsIMsgSend.h"
#include "nsMimeStringResources.h"
#include "nsIIOService.h"
#include "nsNetUtil.h"
#include "comi18n.h"
#include "nsIMsgCompFields.h"
#include "nsMsgCompCID.h"
#include "nsIMsgComposeService.h"
#include "nsMsgAttachmentData.h"
#include "nsMsgI18N.h"
#include "nsNativeCharsetUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIMsgMessageService.h"
#include "nsMsgUtils.h"
#include "nsCExternalHandlerService.h"
#include "nsIMIMEService.h"
#include "nsIMsgHeaderParser.h"
#include "nsIMsgAccountManager.h"
#include "nsMsgBaseCID.h"
#include "nsIMimeConverter.h" // for MimeConverterOutputCallback

//
// Header strings...
//
#define HEADER_NNTP_POSTING_HOST      "NNTP-Posting-Host"
#define MIME_HEADER_TABLE             "<TABLE CELLPADDING=0 CELLSPACING=0 BORDER=0 class=\"moz-email-headers-table\">"
#define HEADER_START_JUNK             "<TR><TH VALIGN=BASELINE ALIGN=RIGHT NOWRAP>"
#define HEADER_MIDDLE_JUNK            ": </TH><TD>"
#define HEADER_END_JUNK               "</TD></TR>"

//
// Forward declarations...
//
extern "C" char     *MIME_StripContinuations(char *original);
int                 mime_decompose_file_init_fn ( void *stream_closure, MimeHeaders *headers );
int                 mime_decompose_file_output_fn ( const char *buf, int32_t size, void *stream_closure );
int                 mime_decompose_file_close_fn ( void *stream_closure );
extern int          MimeHeaders_build_heads_list(MimeHeaders *hdrs);

// CID's
static NS_DEFINE_CID(kCMsgComposeServiceCID,  NS_MSGCOMPOSESERVICE_CID);

mime_draft_data::mime_draft_data() : url_name(nullptr), format_out(0),
  stream(nullptr), obj(nullptr), options(nullptr), headers(nullptr),
  messageBody(nullptr), curAttachment(nullptr),
  decoder_data(nullptr), mailcharset(nullptr), forwardInline(false),
  forwardInlineFilter(false), overrideComposeFormat(false),
  originalMsgURI(nullptr)
{
}
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// THIS SHOULD ALL MOVE TO ANOTHER FILE AFTER LANDING!
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////

// safe filename for all OSes
#define SAFE_TMP_FILENAME "nsmime.tmp"

//
// Create a file for the a unique temp file
// on the local machine. Caller must free memory
//
nsresult
nsMsgCreateTempFile(const char *tFileName, nsIFile **tFile)
{
  if ((!tFileName) || (!*tFileName))
    tFileName = SAFE_TMP_FILENAME;

  nsresult rv = GetSpecialDirectoryWithFileName(NS_OS_TEMP_DIR,
                                                tFileName,
                                                tFile);

  NS_ENSURE_SUCCESS(rv, rv);

  rv = (*tFile)->CreateUnique(nsIFile::NORMAL_FILE_TYPE, 00600);
  if (NS_FAILED(rv))
    NS_RELEASE(*tFile);

  return rv;
}


////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// END OF - THIS SHOULD ALL MOVE TO ANOTHER FILE AFTER LANDING!
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////

typedef enum {
  nsMsg_RETURN_RECEIPT_BOOL_HEADER_MASK = 0,
  nsMsg_ENCRYPTED_BOOL_HEADER_MASK,
  nsMsg_SIGNED_BOOL_HEADER_MASK,
  nsMsg_UUENCODE_BINARY_BOOL_HEADER_MASK,
  nsMsg_ATTACH_VCARD_BOOL_HEADER_MASK,
  nsMsg_LAST_BOOL_HEADER_MASK     // last boolean header mask; must be the last one
                                  // DON'T remove.
} nsMsgBoolHeaderSet;

#ifdef NS_DEBUG
extern "C" void
mime_dump_attachments ( nsMsgAttachmentData *attachData )
{
  int32_t     i = 0;
  class nsMsgAttachmentData  *tmp = attachData;

  while ( (tmp) && (tmp->m_url) )
  {
    printf("Real Name         : %s\n", tmp->m_realName.get());

    if ( tmp->m_url )
    {
      nsCAutoString spec;
      tmp->m_url->GetSpec(spec);
      printf("URL               : %s\n", spec.get());
    }

    printf("Desired Type      : %s\n", tmp->m_desiredType.get());
    printf("Real Type         : %s\n", tmp->m_realType.get());
    printf("Real Encoding     : %s\n", tmp->m_realEncoding.get());
    printf("Description       : %s\n", tmp->m_description.get());
    printf("Mac Type          : %s\n", tmp->m_xMacType.get());
    printf("Mac Creator       : %s\n", tmp->m_xMacCreator.get());
    printf("Size in bytes     : %d\n", tmp->m_size);
    i++;
    tmp++;
  }
}
#endif

nsresult CreateComposeParams(nsCOMPtr<nsIMsgComposeParams> &pMsgComposeParams,
                             nsIMsgCompFields * compFields,
                             nsMsgAttachmentData *attachmentList,
                             MSG_ComposeType composeType,
                             MSG_ComposeFormat composeFormat,
                             nsIMsgIdentity * identity,
                             const char *originalMsgURI,
                             nsIMsgDBHdr *origMsgHdr)
{
#ifdef NS_DEBUG
  mime_dump_attachments ( attachmentList );
#endif

  nsresult rv;
  nsMsgAttachmentData *curAttachment = attachmentList;
  if (curAttachment)
  {
    nsCAutoString spec;

    while (curAttachment && curAttachment->m_url)
    {
      rv = curAttachment->m_url->GetSpec(spec);
      if (NS_SUCCEEDED(rv))
      {
        nsCOMPtr<nsIMsgAttachment> attachment = do_CreateInstance(NS_MSGATTACHMENT_CONTRACTID, &rv);
        if (NS_SUCCEEDED(rv) && attachment)
        {
          nsAutoString nameStr;
          rv = ConvertToUnicode("UTF-8", curAttachment->m_realName.get(), nameStr);
          if (NS_FAILED(rv))
            CopyASCIItoUTF16(curAttachment->m_realName, nameStr);
          attachment->SetName(nameStr);
          attachment->SetUrl(spec);
          attachment->SetTemporary(true);
          attachment->SetContentType(curAttachment->m_realType.get());
          attachment->SetMacType(curAttachment->m_xMacType.get());
          attachment->SetMacCreator(curAttachment->m_xMacCreator.get());
          attachment->SetSize(curAttachment->m_size);
          if (!curAttachment->m_cloudPartInfo.IsEmpty())
          {
            nsCString provider;
            nsCString cloudUrl;
            attachment->SetSendViaCloud(true);
            provider.Adopt(
              MimeHeaders_get_parameter(curAttachment->m_cloudPartInfo.get(),
                                        "provider", nullptr, nullptr));
            cloudUrl.Adopt(
              MimeHeaders_get_parameter(curAttachment->m_cloudPartInfo.get(),
                                        "url", nullptr, nullptr));
            attachment->SetCloudProviderKey(provider);
            attachment->SetContentLocation(cloudUrl);
          }
          compFields->AddAttachment(attachment);
        }
      }
      curAttachment++;
    }
  }

  MSG_ComposeFormat format = composeFormat; // Format to actually use.
  if (identity && composeType == nsIMsgCompType::ForwardInline)
  {
    bool composeHtml = false;
    identity->GetComposeHtml(&composeHtml);
    if (composeHtml)
      format = (composeFormat == nsIMsgCompFormat::OppositeOfDefault) ?
                 nsIMsgCompFormat::PlainText : nsIMsgCompFormat::HTML;
    else
      format = (composeFormat == nsIMsgCompFormat::OppositeOfDefault) ?
                 nsIMsgCompFormat::HTML : nsIMsgCompFormat::PlainText;
  }

  pMsgComposeParams = do_CreateInstance(NS_MSGCOMPOSEPARAMS_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  pMsgComposeParams->SetType(composeType);
  pMsgComposeParams->SetFormat(format);
  pMsgComposeParams->SetIdentity(identity);
  pMsgComposeParams->SetComposeFields(compFields);
  if (originalMsgURI)
    pMsgComposeParams->SetOriginalMsgURI(originalMsgURI);
  if (origMsgHdr)
    pMsgComposeParams->SetOrigMsgHdr(origMsgHdr);
  return NS_OK;
}

nsresult
CreateTheComposeWindow(nsIMsgCompFields *   compFields,
                       nsMsgAttachmentData *attachmentList,
                       MSG_ComposeType      composeType,
                       MSG_ComposeFormat    composeFormat,
                       nsIMsgIdentity *     identity,
                       const char *         originalMsgURI,
                       nsIMsgDBHdr *        origMsgHdr
                       )
{
  nsCOMPtr<nsIMsgComposeParams> pMsgComposeParams;
  nsresult rv = CreateComposeParams(pMsgComposeParams, compFields,
                       attachmentList,
                       composeType,
                       composeFormat,
                       identity,
                       originalMsgURI,
                       origMsgHdr);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIMsgComposeService> msgComposeService =
           do_GetService(kCMsgComposeServiceCID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  return msgComposeService->OpenComposeWindowWithParams(nullptr /* default chrome */, pMsgComposeParams);
}

nsresult
ForwardMsgInline(nsIMsgCompFields *compFields,
                 nsMsgAttachmentData *attachmentList,
                 MSG_ComposeFormat composeFormat,
                 nsIMsgIdentity *identity,
                 const char *originalMsgURI,
                 nsIMsgDBHdr *origMsgHdr)
{
  nsCOMPtr<nsIMsgComposeParams> pMsgComposeParams;
  nsresult rv = CreateComposeParams(pMsgComposeParams, compFields,
                                    attachmentList,
                                    nsIMsgCompType::ForwardInline,
                                    composeFormat,
                                    identity,
                                    originalMsgURI,
                                    origMsgHdr);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIMsgComposeService> msgComposeService =
           do_GetService(kCMsgComposeServiceCID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  // create the nsIMsgCompose object to send the object
  nsCOMPtr<nsIMsgCompose> pMsgCompose (do_CreateInstance(NS_MSGCOMPOSE_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  /** initialize nsIMsgCompose, Send the message, wait for send completion response **/
  rv = pMsgCompose->Initialize(pMsgComposeParams, nullptr, nullptr);
  NS_ENSURE_SUCCESS(rv,rv);

  rv = pMsgCompose->SendMsg(nsIMsgSend::nsMsgDeliverNow, identity, nullptr, nullptr, nullptr);
  if (NS_SUCCEEDED(rv))
  {
    nsCOMPtr<nsIMsgFolder> origFolder;
    origMsgHdr->GetFolder(getter_AddRefs(origFolder));
    if (origFolder)
      origFolder->AddMessageDispositionState(
                  origMsgHdr, nsIMsgFolder::nsMsgDispositionState_Forwarded);
  }
  return rv;
}

nsresult
CreateCompositionFields(const char        *from,
                        const char        *reply_to,
                        const char        *to,
                        const char        *cc,
                        const char        *bcc,
                        const char        *fcc,
                        const char        *newsgroups,
                        const char        *followup_to,
                        const char        *organization,
                        const char        *subject,
                        const char        *references,
                        const char        *other_random_headers,
                        const char        *priority,
                        const char        *newspost_url,
                        char              *charset,
                        nsIMsgCompFields  **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  nsresult rv;
  *_retval = nullptr;

  nsCOMPtr<nsIMsgCompFields> cFields = do_CreateInstance(NS_MSGCOMPFIELDS_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(cFields, NS_ERROR_OUT_OF_MEMORY);

  // Now set all of the passed in stuff...
  cFields->SetCharacterSet(!PL_strcasecmp("us-ascii", charset) ? "ISO-8859-1" : charset);

  char *val;
  nsAutoString outString;

  if (from) {
    ConvertRawBytesToUTF16(from, charset, outString);
    cFields->SetFrom(outString);
  }

  if (subject) {
    val = MIME_DecodeMimeHeader(subject, charset, false, true);
    cFields->SetSubject(NS_ConvertUTF8toUTF16(val ? val : subject));
    PR_FREEIF(val);
  }

  if (reply_to) {
    ConvertRawBytesToUTF16(reply_to, charset, outString);
    cFields->SetReplyTo(outString);
  }

  if (to) {
    ConvertRawBytesToUTF16(to, charset, outString);
    cFields->SetTo(outString);
  }

  if (cc) {
    ConvertRawBytesToUTF16(cc, charset, outString);
    cFields->SetCc(outString);
  }

  if (bcc) {
    ConvertRawBytesToUTF16(bcc, charset, outString);
    cFields->SetBcc(outString);
  }

  if (fcc) {
    val = MIME_DecodeMimeHeader(fcc, charset, false, true);
    cFields->SetFcc(NS_ConvertUTF8toUTF16(val ? val : fcc));
    PR_FREEIF(val);
  }

  if (newsgroups) {
    // fixme: the newsgroups header had better be decoded using the server-side
    // character encoding,but this |charset| might be different from it.
    val = MIME_DecodeMimeHeader(newsgroups, charset, false, true);
    cFields->SetNewsgroups(NS_ConvertUTF8toUTF16(val ? val : newsgroups));
    PR_FREEIF(val);
  }

  if (followup_to) {
    val = MIME_DecodeMimeHeader(followup_to, charset, false, true);
    cFields->SetFollowupTo(NS_ConvertUTF8toUTF16(val ? val : followup_to));
    PR_FREEIF(val);
  }

  if (organization) {
    val = MIME_DecodeMimeHeader(organization, charset, false, true);
    cFields->SetOrganization(NS_ConvertUTF8toUTF16(val ? val : organization));
    PR_FREEIF(val);
  }

  if (references) {
    val = MIME_DecodeMimeHeader(references, charset, false, true);
    cFields->SetReferences(val ? val : references);
    PR_FREEIF(val);
  }

  if (other_random_headers) {
    val = MIME_DecodeMimeHeader(other_random_headers, charset, false, true);
    cFields->SetOtherRandomHeaders(NS_ConvertUTF8toUTF16(val ? val : other_random_headers));
    PR_FREEIF(val);
  }

  if (priority) {
    val = MIME_DecodeMimeHeader(priority, charset, false, true);
    nsMsgPriorityValue priorityValue;
    NS_MsgGetPriorityFromString(val ? val : priority, priorityValue);
    PR_FREEIF(val);
    nsCAutoString priorityName;
    NS_MsgGetUntranslatedPriorityName(priorityValue, priorityName);
    cFields->SetPriority(priorityName.get());
  }

  if (newspost_url) {
    val = MIME_DecodeMimeHeader(newspost_url, charset, false, true);
    cFields->SetNewspostUrl(val ? val : newspost_url);
    PR_FREEIF(val);
  }

  *_retval = cFields;
  NS_IF_ADDREF(*_retval);

  return rv;
}

static int
dummy_file_write( char *buf, int32_t size, void *fileHandle )
{
  if (!fileHandle)
    return NS_ERROR_FAILURE;

  nsIOutputStream  *tStream = (nsIOutputStream *) fileHandle;
  uint32_t bytesWritten;
  tStream->Write(buf, size, &bytesWritten);
  return (int) bytesWritten;
}

static int
mime_parse_stream_write ( nsMIMESession *stream, const char *buf, int32_t size )
{
  mime_draft_data *mdd = (mime_draft_data *) stream->data_object;
  NS_ASSERTION ( mdd, "null mime draft data!" );

  if ( !mdd || !mdd->obj )
    return -1;

  return mdd->obj->clazz->parse_buffer ((char *) buf, size, mdd->obj);
}

static void
mime_free_attachments(nsTArray<nsMsgAttachedFile *> &attachments)
{
  if (attachments.Length() <= 0)
    return;

  for (int i = 0; i < attachments.Length(); i++)
  {
    if (attachments[i]->m_tmpFile)
    {
      attachments[i]->m_tmpFile->Remove(false);
      attachments[i]->m_tmpFile = nullptr;
    }
    delete attachments[i];
  }
}

static nsMsgAttachmentData *
mime_draft_process_attachments(mime_draft_data *mdd)
{
  if (!mdd)
    return nullptr;

  nsMsgAttachmentData         *attachData = NULL, *tmp = NULL;
  nsMsgAttachedFile           *tmpFile = NULL;

  //It's possible we must treat the message body as attachment!
  bool bodyAsAttachment = false;
  if (mdd->messageBody &&
      mdd->messageBody->m_type.Find("text/html", CaseInsensitiveCompare) == -1 &&
      mdd->messageBody->m_type.Find("text/plain", CaseInsensitiveCompare) == -1 &&
      !mdd->messageBody->m_type.LowerCaseEqualsLiteral("text"))
     bodyAsAttachment = true;

  if (!mdd->attachments.Length() && !bodyAsAttachment)
    return nullptr;

  int32_t totalCount = mdd->attachments.Length();
  if (bodyAsAttachment)
    totalCount++;
  attachData = new nsMsgAttachmentData[totalCount + 1];
  if ( !attachData )
    return nullptr;

  tmp = attachData;

  for (int i = 0, attachmentsIndex = 0; i < totalCount; i++, tmp++)
  {
    if (bodyAsAttachment && i == 0)
      tmpFile = mdd->messageBody;
    else
      tmpFile = mdd->attachments[attachmentsIndex++];

    if (tmpFile->m_type.LowerCaseEqualsLiteral("text/x-vcard"))
      tmp->m_realName = tmpFile->m_description;

    if ( tmpFile->m_origUrl )
    {
      nsCAutoString tmpSpec;
      if (NS_FAILED(tmpFile->m_origUrl->GetSpec(tmpSpec)))
        goto FAIL;

      if (NS_FAILED(nsMimeNewURI(getter_AddRefs(tmp->m_url), tmpSpec.get(), nullptr)))
        goto FAIL;

      if (tmp->m_realName.IsEmpty())
      {
        if (!tmpFile->m_realName.IsEmpty())
          tmp->m_realName = tmpFile->m_realName;
        else {
          if (tmpFile->m_type.Find(MESSAGE_RFC822, CaseInsensitiveCompare) != -1)
            // we have the odd case of processing an e-mail that had an unnamed
            // eml message attached
            tmp->m_realName = "ForwardedMessage.eml";

          else
            tmp->m_realName = tmpSpec.get();
        }
      }
    }

    tmp->m_desiredType = tmpFile->m_type;
    tmp->m_realType = tmpFile->m_type;
    tmp->m_realEncoding = tmpFile->m_encoding;
    tmp->m_description = tmpFile->m_description;
    tmp->m_cloudPartInfo = tmpFile->m_cloudPartInfo;
    tmp->m_xMacType = tmpFile->m_xMacType;
    tmp->m_xMacCreator = tmpFile->m_xMacCreator;
    tmp->m_size = tmpFile->m_size;
  }
  return attachData;

FAIL:
  delete [] attachData;
  return nullptr;
}

static void
mime_intl_insert_message_header_1(char        **body,
                                  char        **hdr_value,
                                  const char  *hdr_str,
                                  const char  *html_hdr_str,
                                  const char  *mailcharset,
                                  bool        htmlEdit)
{
  if (!body || !hdr_value || !hdr_str)
    return;

  if (htmlEdit)
  {
    NS_MsgSACat(body, HEADER_START_JUNK);
  }
  else
  {
    NS_MsgSACat(body, MSG_LINEBREAK);
  }
  if (!html_hdr_str)
    html_hdr_str = hdr_str;
  NS_MsgSACat(body, html_hdr_str);
  if (htmlEdit)
  {
    NS_MsgSACat(body, HEADER_MIDDLE_JUNK);
  }
  else
    NS_MsgSACat(body, ": ");

    // MIME decode header
    char* utf8 = MIME_DecodeMimeHeader(*hdr_value, mailcharset, false,
                                       true);
    if (NULL != utf8) {
      char *escaped = nullptr;
      if (htmlEdit)
        escaped = MsgEscapeHTML(utf8);
      NS_MsgSACat(body, escaped ? escaped : utf8);
      NS_Free(escaped);
      PR_Free(utf8);
    } else {
        NS_MsgSACat(body, *hdr_value); // raw MIME encoded string
    }

  if (htmlEdit)
    NS_MsgSACat(body, HEADER_END_JUNK);
}

char *
MimeGetNamedString(int32_t id)
{
  static char   retString[256];

  retString[0] = '\0';
  char *tString = MimeGetStringByID(id);
  if (tString)
  {
    PL_strncpy(retString, tString, sizeof(retString));
    PR_Free(tString);
  }
  return retString;
}

void
MimeGetReplyHeaderOriginalMessage(nsACString &retString)
{
  nsCString defaultValue;
  defaultValue.Adopt(MimeGetStringByID(MIME_FORWARDED_MESSAGE_HTML_USER_WROTE));

  nsString tmpRetString;
  NS_GetLocalizedUnicharPreferenceWithDefault(nullptr,
    "mailnews.reply_header_originalmessage",
    NS_ConvertUTF8toUTF16(defaultValue),
    tmpRetString);

  CopyUTF16toUTF8(tmpRetString, retString);
}

/* given an address string passed though parameter "address", this one will be converted
   and returned through the same parameter. The original string will be destroyed
*/
static void UnquoteMimeAddress(nsIMsgHeaderParser* parser, char** address)
{
  if (parser && address && *address && **address)
  {
    char *result;
    if (NS_SUCCEEDED(parser->UnquotePhraseOrAddr(*address, false, &result)))
    {
      if (result && *result)
      {
        PR_Free(*address);
        *address = result;
        return;
      }
      PR_FREEIF(result);
    }
  }

  return;
}

static void
mime_insert_all_headers(char            **body,
                        MimeHeaders     *headers,
                        MSG_ComposeFormat composeFormat,
                        char            *mailcharset)
{
  nsCOMPtr<nsIMsgHeaderParser> parser = do_GetService(NS_MAILNEWS_MIME_HEADER_PARSER_CONTRACTID);

  bool htmlEdit = (composeFormat == nsIMsgCompFormat::HTML);
  char *newBody = NULL;
  char *html_tag = nullptr;
  if (*body)
    html_tag = PL_strcasestr(*body, "<HTML>");
  int i;

  if (!headers->done_p)
  {
    MimeHeaders_build_heads_list(headers);
    headers->done_p = true;
  }

  nsCString replyHeader;
  MimeGetReplyHeaderOriginalMessage(replyHeader);
  if (htmlEdit)
  {
    NS_MsgSACopy(&(newBody), "<HTML><BODY><BR><BR>");
    NS_MsgSACat(&newBody, replyHeader.get());
    NS_MsgSACat(&newBody, MIME_HEADER_TABLE);
  }
  else
  {
    NS_MsgSACopy(&(newBody), MSG_LINEBREAK MSG_LINEBREAK);
    NS_MsgSACat(&newBody, replyHeader.get());
  }

  for (i = 0; i < headers->heads_size; i++)
  {
    char *head = headers->heads[i];
    char *end = (i == headers->heads_size-1
           ? headers->all_headers + headers->all_headers_fp
           : headers->heads[i+1]);
    char *colon, *ocolon;
    char *contents;
    char *name = 0;
    char *c2 = 0;

    // Hack for BSD Mailbox delimiter.
    if (i == 0 && head[0] == 'F' && !strncmp(head, "From ", 5))
    {
      colon = head + 4;
      contents = colon + 1;
    }
    else
    {
      /* Find the colon. */
      for (colon = head; colon < end; colon++)
      if (*colon == ':') break;

      if (colon >= end) continue;   /* junk */

      /* Back up over whitespace before the colon. */
      ocolon = colon;
      for (; colon > head && IS_SPACE(colon[-1]); colon--)
      ;

      contents = ocolon + 1;
    }

    /* Skip over whitespace after colon. */
    while (contents <= end && IS_SPACE(*contents))
    contents++;

    /* Take off trailing whitespace... */
    while (end > contents && IS_SPACE(end[-1]))
    end--;

    name = (char *)PR_MALLOC(colon - head + 1);
    if (!name)
      return /* MIME_OUT_OF_MEMORY */;
    memcpy(name, head, colon - head);
    name[colon - head] = 0;

    c2 = (char *)PR_MALLOC(end - contents + 1);
    if (!c2)
    {
      PR_Free(name);
      return /* MIME_OUT_OF_MEMORY */;
    }
    memcpy(c2, contents, end - contents);
    c2[end - contents] = 0;

    /* Do not reveal bcc recipients when forwarding a message!
       See http://bugzilla.mozilla.org/show_bug.cgi?id=41150
    */
    if (PL_strcasecmp(name, "bcc") != 0)
    {
      if (!PL_strcasecmp(name, "resent-from") || !PL_strcasecmp(name, "from") ||
          !PL_strcasecmp(name, "resent-to") || !PL_strcasecmp(name, "to") ||
          !PL_strcasecmp(name, "resent-cc") || !PL_strcasecmp(name, "cc") ||
          !PL_strcasecmp(name, "reply-to"))
        UnquoteMimeAddress(parser, &c2);

      mime_intl_insert_message_header_1(&newBody, &c2, name, name, mailcharset, htmlEdit);
    }
    PR_Free(name);
    PR_Free(c2);
  }

  if (htmlEdit)
  {
    NS_MsgSACat(&newBody, "</TABLE>");
    NS_MsgSACat(&newBody, MSG_LINEBREAK "<BR><BR>");
    if (html_tag)
      NS_MsgSACat(&newBody, html_tag+6);
    else if (*body)
        NS_MsgSACat(&newBody, *body);
  }
  else
  {
    NS_MsgSACat(&newBody, MSG_LINEBREAK MSG_LINEBREAK);
    if (*body)
      NS_MsgSACat(&newBody, *body);
  }

  if (newBody)
  {
    PR_FREEIF(*body);
    *body = newBody;
  }
}

static void
mime_insert_normal_headers(char             **body,
                           MimeHeaders      *headers,
                           MSG_ComposeFormat  composeFormat,
                           char             *mailcharset)
{
  char *newBody = nullptr;
  char *subject = MimeHeaders_get(headers, HEADER_SUBJECT, false, false);
  char *resent_comments = MimeHeaders_get(headers, HEADER_RESENT_COMMENTS, false, false);
  char *resent_date = MimeHeaders_get(headers, HEADER_RESENT_DATE, false, true);
  char *resent_from = MimeHeaders_get(headers, HEADER_RESENT_FROM, false, true);
  char *resent_to = MimeHeaders_get(headers, HEADER_RESENT_TO, false, true);
  char *resent_cc = MimeHeaders_get(headers, HEADER_RESENT_CC, false, true);
  char *date = MimeHeaders_get(headers, HEADER_DATE, false, true);
  char *from = MimeHeaders_get(headers, HEADER_FROM, false, true);
  char *reply_to = MimeHeaders_get(headers, HEADER_REPLY_TO, false, true);
  char *organization = MimeHeaders_get(headers, HEADER_ORGANIZATION, false, false);
  char *to = MimeHeaders_get(headers, HEADER_TO, false, true);
  char *cc = MimeHeaders_get(headers, HEADER_CC, false, true);
  char *newsgroups = MimeHeaders_get(headers, HEADER_NEWSGROUPS, false, true);
  char *followup_to = MimeHeaders_get(headers, HEADER_FOLLOWUP_TO, false, true);
  char *references = MimeHeaders_get(headers, HEADER_REFERENCES, false, true);
  const char *html_tag = nullptr;
  if (*body)
    html_tag = PL_strcasestr(*body, "<HTML>");
  bool htmlEdit = composeFormat == nsIMsgCompFormat::HTML;

  if (!from)
    from = MimeHeaders_get(headers, HEADER_SENDER, false, true);
  if (!resent_from)
    resent_from = MimeHeaders_get(headers, HEADER_RESENT_SENDER, false,
                    true);

  nsCOMPtr<nsIMsgHeaderParser> parser = do_GetService(NS_MAILNEWS_MIME_HEADER_PARSER_CONTRACTID);
  UnquoteMimeAddress(parser, &resent_from);
  UnquoteMimeAddress(parser, &resent_to);
  UnquoteMimeAddress(parser, &resent_cc);
  UnquoteMimeAddress(parser, &reply_to);
  UnquoteMimeAddress(parser, &from);
  UnquoteMimeAddress(parser, &to);
  UnquoteMimeAddress(parser, &cc);

  nsCString replyHeader;
  MimeGetReplyHeaderOriginalMessage(replyHeader);
  if (htmlEdit)
  {
    NS_MsgSACopy(&(newBody), "<HTML><BODY><BR><BR>");
    NS_MsgSACat(&newBody, replyHeader.get());
    NS_MsgSACat(&newBody, MIME_HEADER_TABLE);
  }
  else
  {
    NS_MsgSACopy(&(newBody), MSG_LINEBREAK MSG_LINEBREAK);
    NS_MsgSACat(&newBody, replyHeader.get());
  }
  if (subject)
    mime_intl_insert_message_header_1(&newBody, &subject, HEADER_SUBJECT,
                      MimeGetNamedString(MIME_MHTML_SUBJECT),
                      mailcharset, htmlEdit);
  if (resent_comments)
    mime_intl_insert_message_header_1(&newBody, &resent_comments,
                      HEADER_RESENT_COMMENTS,
                      MimeGetNamedString(MIME_MHTML_RESENT_COMMENTS),
                      mailcharset, htmlEdit);
  if (resent_date)
    mime_intl_insert_message_header_1(&newBody, &resent_date,
                      HEADER_RESENT_DATE,
                      MimeGetNamedString(MIME_MHTML_RESENT_DATE),
                      mailcharset, htmlEdit);
  if (resent_from)
  {
    mime_intl_insert_message_header_1(&newBody, &resent_from,
                      HEADER_RESENT_FROM,
                      MimeGetNamedString(MIME_MHTML_RESENT_FROM),
                      mailcharset, htmlEdit);
  }
  if (resent_to)
  {
    mime_intl_insert_message_header_1(&newBody, &resent_to,
                      HEADER_RESENT_TO,
                      MimeGetNamedString(MIME_MHTML_RESENT_TO),
                      mailcharset, htmlEdit);
  }
  if (resent_cc)
  {
    mime_intl_insert_message_header_1(&newBody, &resent_cc,
                      HEADER_RESENT_CC,
                      MimeGetNamedString(MIME_MHTML_RESENT_CC),
                      mailcharset, htmlEdit);
  }
  if (date)
    mime_intl_insert_message_header_1(&newBody, &date, HEADER_DATE,
                      MimeGetNamedString(MIME_MHTML_DATE),
                      mailcharset, htmlEdit);
  if (from)
  {
    mime_intl_insert_message_header_1(&newBody, &from, HEADER_FROM,
                      MimeGetNamedString(MIME_MHTML_FROM),
                      mailcharset, htmlEdit);
  }
  if (reply_to)
  {
    mime_intl_insert_message_header_1(&newBody, &reply_to, HEADER_REPLY_TO,
                      MimeGetNamedString(MIME_MHTML_REPLY_TO),
                      mailcharset, htmlEdit);
  }
  if (organization)
    mime_intl_insert_message_header_1(&newBody, &organization,
                      HEADER_ORGANIZATION,
                      MimeGetNamedString(MIME_MHTML_ORGANIZATION),
                      mailcharset, htmlEdit);
  if (to)
  {
    mime_intl_insert_message_header_1(&newBody, &to, HEADER_TO,
                      MimeGetNamedString(MIME_MHTML_TO),
                      mailcharset, htmlEdit);
  }
  if (cc)
  {
    mime_intl_insert_message_header_1(&newBody, &cc, HEADER_CC,
                      MimeGetNamedString(MIME_MHTML_CC),
                      mailcharset, htmlEdit);
  }
    /*
      Do not reveal bcc recipients when forwarding a message!
      See http://bugzilla.mozilla.org/show_bug.cgi?id=41150
    */
  if (newsgroups)
    mime_intl_insert_message_header_1(&newBody, &newsgroups, HEADER_NEWSGROUPS,
                      MimeGetNamedString(MIME_MHTML_NEWSGROUPS),
                      mailcharset, htmlEdit);
  if (followup_to)
  {
    mime_intl_insert_message_header_1(&newBody, &followup_to,
                      HEADER_FOLLOWUP_TO,
                      MimeGetNamedString(MIME_MHTML_FOLLOWUP_TO),
                      mailcharset, htmlEdit);
  }
  // only show references for newsgroups
  if (newsgroups && references)
  {
    mime_intl_insert_message_header_1(&newBody, &references,
                      HEADER_REFERENCES,
                      MimeGetNamedString(MIME_MHTML_REFERENCES),
                      mailcharset, htmlEdit);
  }
  if (htmlEdit)
  {
    NS_MsgSACat(&newBody, "</TABLE>");
    NS_MsgSACat(&newBody, MSG_LINEBREAK "<BR><BR>");
    if (html_tag)
      NS_MsgSACat(&newBody, html_tag+6);
    else if (*body)
        NS_MsgSACat(&newBody, *body);
  }
  else
  {
    NS_MsgSACat(&newBody, MSG_LINEBREAK MSG_LINEBREAK);
    if (*body)
      NS_MsgSACat(&newBody, *body);
  }
  if (newBody)
  {
    PR_FREEIF(*body);
    *body = newBody;
  }
  PR_FREEIF(subject);
  PR_FREEIF(resent_comments);
  PR_FREEIF(resent_date);
  PR_FREEIF(resent_from);
  PR_FREEIF(resent_to);
  PR_FREEIF(resent_cc);
  PR_FREEIF(date);
  PR_FREEIF(from);
  PR_FREEIF(reply_to);
  PR_FREEIF(organization);
  PR_FREEIF(to);
  PR_FREEIF(cc);
  PR_FREEIF(newsgroups);
  PR_FREEIF(followup_to);
  PR_FREEIF(references);
}

static void
mime_insert_micro_headers(char            **body,
                          MimeHeaders     *headers,
                          MSG_ComposeFormat composeFormat,
                          char            *mailcharset)
{
  char *newBody = NULL;
  char *subject = MimeHeaders_get(headers, HEADER_SUBJECT, false, false);
  char *from = MimeHeaders_get(headers, HEADER_FROM, false, true);
  char *resent_from = MimeHeaders_get(headers, HEADER_RESENT_FROM, false,
                    true);
  char *date = MimeHeaders_get(headers, HEADER_DATE, false, true);
  char *to = MimeHeaders_get(headers, HEADER_TO, false, true);
  char *cc = MimeHeaders_get(headers, HEADER_CC, false, true);
  char *newsgroups = MimeHeaders_get(headers, HEADER_NEWSGROUPS, false,
                     true);
  const char *html_tag = nullptr;
  if (*body)
    html_tag = PL_strcasestr(*body, "<HTML>");
  bool htmlEdit = composeFormat == nsIMsgCompFormat::HTML;

  if (!from)
    from = MimeHeaders_get(headers, HEADER_SENDER, false, true);
  if (!resent_from)
    resent_from = MimeHeaders_get(headers, HEADER_RESENT_SENDER, false,
                    true);
  if (!date)
    date = MimeHeaders_get(headers, HEADER_RESENT_DATE, false, true);

  nsCOMPtr<nsIMsgHeaderParser> parser = do_GetService(NS_MAILNEWS_MIME_HEADER_PARSER_CONTRACTID);
  UnquoteMimeAddress(parser, &resent_from);
  UnquoteMimeAddress(parser, &from);
  UnquoteMimeAddress(parser, &to);
  UnquoteMimeAddress(parser, &cc);

  nsCString replyHeader;
  MimeGetReplyHeaderOriginalMessage(replyHeader);

  if (htmlEdit)
  {
    NS_MsgSACopy(&(newBody), "<HTML><BODY><BR><BR>");
    NS_MsgSACat(&newBody, replyHeader.get());
    NS_MsgSACat(&newBody, MIME_HEADER_TABLE);
  }
  else
  {
    NS_MsgSACopy(&(newBody), MSG_LINEBREAK MSG_LINEBREAK);
    NS_MsgSACat(&newBody, replyHeader.get());
  }

  if (from)
  {
    mime_intl_insert_message_header_1(&newBody, &from, HEADER_FROM,
                    MimeGetNamedString(MIME_MHTML_FROM),
                    mailcharset, htmlEdit);
  }
  if (subject)
    mime_intl_insert_message_header_1(&newBody, &subject, HEADER_SUBJECT,
                    MimeGetNamedString(MIME_MHTML_SUBJECT),
                      mailcharset, htmlEdit);
/*
  if (date)
    mime_intl_insert_message_header_1(&newBody, &date, HEADER_DATE,
                    MimeGetNamedString(MIME_MHTML_DATE),
                    mailcharset, htmlEdit);
*/
  if (resent_from)
  {
    mime_intl_insert_message_header_1(&newBody, &resent_from,
                    HEADER_RESENT_FROM,
                    MimeGetNamedString(MIME_MHTML_RESENT_FROM),
                    mailcharset, htmlEdit);
  }
  if (to)
  {
    mime_intl_insert_message_header_1(&newBody, &to, HEADER_TO,
                    MimeGetNamedString(MIME_MHTML_TO),
                    mailcharset, htmlEdit);
  }
  if (cc)
  {
    mime_intl_insert_message_header_1(&newBody, &cc, HEADER_CC,
                    MimeGetNamedString(MIME_MHTML_CC),
                    mailcharset, htmlEdit);
  }
  /*
    Do not reveal bcc recipients when forwarding a message!
    See http://bugzilla.mozilla.org/show_bug.cgi?id=41150
  */
  if (newsgroups)
    mime_intl_insert_message_header_1(&newBody, &newsgroups, HEADER_NEWSGROUPS,
                    MimeGetNamedString(MIME_MHTML_NEWSGROUPS),
                    mailcharset, htmlEdit);
  if (htmlEdit)
  {
    NS_MsgSACat(&newBody, "</TABLE>");
    NS_MsgSACat(&newBody, MSG_LINEBREAK "<BR><BR>");
    if (html_tag)
      NS_MsgSACat(&newBody, html_tag+6);
    else if (*body)
        NS_MsgSACat(&newBody, *body);
  }
  else
  {
    NS_MsgSACat(&newBody, MSG_LINEBREAK MSG_LINEBREAK);
    if (*body)
      NS_MsgSACat(&newBody, *body);
  }
  if (newBody)
  {
    PR_FREEIF(*body);
    *body = newBody;
  }
  PR_FREEIF(subject);
  PR_FREEIF(from);
  PR_FREEIF(resent_from);
  PR_FREEIF(date);
  PR_FREEIF(to);
  PR_FREEIF(cc);
  PR_FREEIF(newsgroups);

}

// body has to be encoded in UTF-8
static void
mime_insert_forwarded_message_headers(char            **body,
                                      MimeHeaders     *headers,
                                      MSG_ComposeFormat composeFormat,
                                      char            *mailcharset)
{
  if (!body || !headers)
    return;

  int32_t     show_headers = 0;
  nsresult    res;

  nsCOMPtr<nsIPrefBranch> prefBranch(do_GetService(NS_PREFSERVICE_CONTRACTID, &res));
  if (NS_SUCCEEDED(res))
    prefBranch->GetIntPref("mail.show_headers", &show_headers);

  switch (show_headers)
  {
  case 0:
    mime_insert_micro_headers(body, headers, composeFormat, mailcharset);
    break;
  default:
  case 1:
    mime_insert_normal_headers(body, headers, composeFormat, mailcharset);
    break;
  case 2:
    mime_insert_all_headers(body, headers, composeFormat, mailcharset);
    break;
  }
}

static void
mime_parse_stream_complete (nsMIMESession *stream)
{
  mime_draft_data *mdd = (mime_draft_data *) stream->data_object;
  nsCOMPtr<nsIMsgCompFields> fields;
  int htmlAction = 0;
  int lineWidth = 0;

  char *host = 0;
  char *news_host = 0;
  char *to_and_cc = 0;
  char *re_subject = 0;
  char *new_refs = 0;
  char *from = 0;
  char *repl = 0;
  char *subj = 0;
  char *id = 0;
  char *refs = 0;
  char *to = 0;
  char *cc = 0;
  char *bcc = 0;
  char *fcc = 0;
  char *org = 0;
  char *grps = 0;
  char *foll = 0;
  char *priority = 0;
  char *draftInfo = 0;
  char *identityKey = 0;

  bool forward_inline = false;
  bool bodyAsAttachment = false;
  bool charsetOverride = false;

  NS_ASSERTION (mdd, "null mime draft data");

  if (!mdd) return;

  if (mdd->obj)
  {
    int status;

    status = mdd->obj->clazz->parse_eof ( mdd->obj, false );
    mdd->obj->clazz->parse_end( mdd->obj, status < 0 ? true : false );

    // RICHIE
    // We need to figure out how to pass the forwarded flag along with this
    // operation.

    //forward_inline = (mdd->format_out != FO_CMDLINE_ATTACHMENTS);
    forward_inline = mdd->forwardInline;

    NS_ASSERTION ( mdd->options == mdd->obj->options, "mime draft options not same as obj->options" );
    mime_free (mdd->obj);
    mdd->obj = 0;
    if (mdd->options)
    {
      // save the override flag before it's unavailable
      charsetOverride = mdd->options->override_charset;
      if ((!mdd->mailcharset || charsetOverride) && mdd->options->default_charset)
      {
        PR_Free(mdd->mailcharset);
        mdd->mailcharset = strdup(mdd->options->default_charset);
      }

      // mscott: aren't we leaking a bunch of strings here like the charset strings and such?
      delete mdd->options;
      mdd->options = 0;
    }
    if (mdd->stream)
    {
      mdd->stream->complete ((nsMIMESession *)mdd->stream->data_object);
      PR_Free( mdd->stream );
      mdd->stream = 0;
    }
  }

  //
  // Now, process the attachments that we have gathered from the message
  // on disk
  //
  nsMsgAttachmentData *newAttachData = mime_draft_process_attachments(mdd);

  //
  // time to bring up the compose windows with all the info gathered
  //
  if ( mdd->headers )
  {
    subj = MimeHeaders_get(mdd->headers, HEADER_SUBJECT,  false, false);
    if (forward_inline)
    {
      if (subj)
      {
        nsresult rv;
        nsCOMPtr<nsIPrefBranch> prefBranch(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
        if (NS_SUCCEEDED(rv))
        {
          nsCAutoString fwdPrefix;
          prefBranch->GetCharPref("mail.forward_subject_prefix",
                                  getter_Copies(fwdPrefix));
          char *newSubj = PR_smprintf("%s: %s", !fwdPrefix.IsEmpty() ?
                                                fwdPrefix.get(): "Fwd", subj);
          if (newSubj)
          {
            PR_Free(subj);
            subj = newSubj;
          }
        }
      }
    }
    else
    {
      repl = MimeHeaders_get(mdd->headers, HEADER_REPLY_TO, false, false);
      to   = MimeHeaders_get(mdd->headers, HEADER_TO,       false, true);
      cc   = MimeHeaders_get(mdd->headers, HEADER_CC,       false, true);
      bcc   = MimeHeaders_get(mdd->headers, HEADER_BCC,       false, true);

      /* These headers should not be RFC-1522-decoded. */
      grps = MimeHeaders_get(mdd->headers, HEADER_NEWSGROUPS,  false, true);
      foll = MimeHeaders_get(mdd->headers, HEADER_FOLLOWUP_TO, false, true);

      host = MimeHeaders_get(mdd->headers, HEADER_X_MOZILLA_NEWSHOST, false, false);
      if (!host)
        host = MimeHeaders_get(mdd->headers, HEADER_NNTP_POSTING_HOST, false, false);

      id   = MimeHeaders_get(mdd->headers, HEADER_MESSAGE_ID,  false, false);
      refs = MimeHeaders_get(mdd->headers, HEADER_REFERENCES,  false, true);
      priority = MimeHeaders_get(mdd->headers, HEADER_X_PRIORITY, false, false);


      if (host)
      {
        char *secure = NULL;

        secure = PL_strcasestr(host, "secure");
        if (secure)
        {
          *secure = 0;
          news_host = PR_smprintf ("snews://%s", host);
        }
        else
        {
          news_host = PR_smprintf ("news://%s", host);
        }
      }
    }


    CreateCompositionFields( from, repl, to, cc, bcc, fcc, grps, foll,
      org, subj, refs, 0, priority, news_host,
      mdd->mailcharset,
      getter_AddRefs(fields));

    draftInfo = MimeHeaders_get(mdd->headers, HEADER_X_MOZILLA_DRAFT_INFO, false, false);
    if (draftInfo && fields && !forward_inline)
    {
      char *parm = 0;
      parm = MimeHeaders_get_parameter(draftInfo, "vcard", NULL, NULL);
      fields->SetAttachVCard(parm && !strcmp(parm, "1"));

      fields->SetMessageId(id); // keep same message id for editing template.
      PR_FREEIF(parm);
      parm = MimeHeaders_get_parameter(draftInfo, "receipt", NULL, NULL);
      if (parm && !strcmp(parm, "0"))
        fields->SetReturnReceipt(false);
      else
      {
        int receiptType = 0;
        fields->SetReturnReceipt(true);
        sscanf(parm, "%d", &receiptType);
        // slight change compared to 4.x; we used to use receipt= to tell
        // whether the draft/template has request for either MDN or DNS or both
        // return receipt; since the DNS is out of the picture we now use the
        // header type - 1 to tell whether user has requested the return receipt
        fields->SetReceiptHeaderType(((int32_t)receiptType) - 1);
      }
      PR_FREEIF(parm);
      parm = MimeHeaders_get_parameter(draftInfo, "DSN", NULL, NULL);
      fields->SetDSN(parm && !strcmp(parm, "1"));
      PR_Free(parm);
      parm = MimeHeaders_get_parameter(draftInfo, "html", NULL, NULL);
      if (parm)
        sscanf(parm, "%d", &htmlAction);
      PR_FREEIF(parm);
      parm = MimeHeaders_get_parameter(draftInfo, "linewidth", NULL, NULL);
      if (parm)
        sscanf(parm, "%d", &lineWidth);
      PR_FREEIF(parm);

    }

  // identity to prefer when opening the message in the compose window?
    identityKey = MimeHeaders_get(mdd->headers, HEADER_X_MOZILLA_IDENTITY_KEY, false, false);
    if ( identityKey && *identityKey )
    {
        nsresult rv = NS_OK;
        nsCOMPtr< nsIMsgAccountManager > accountManager =
                do_GetService( NS_MSGACCOUNTMANAGER_CONTRACTID, &rv );
        if ( NS_SUCCEEDED(rv) && accountManager )
        {
            nsCOMPtr< nsIMsgIdentity > overrulingIdentity;
            rv = accountManager->GetIdentity( nsDependentCString(identityKey), getter_AddRefs( overrulingIdentity ) );

            if ( NS_SUCCEEDED(rv) && overrulingIdentity )
                mdd->identity = overrulingIdentity;
        }
    }

    if (mdd->messageBody)
    {
      MSG_ComposeFormat composeFormat = nsIMsgCompFormat::Default;
      if (!mdd->messageBody->m_type.IsEmpty())
      {
        if(mdd->messageBody->m_type.Find("text/html", CaseInsensitiveCompare) != -1)
          composeFormat = nsIMsgCompFormat::HTML;
        else if (mdd->messageBody->m_type.Find("text/plain", CaseInsensitiveCompare) != -1 ||
                 mdd->messageBody->m_type.LowerCaseEqualsLiteral("text"))
          composeFormat = nsIMsgCompFormat::PlainText;
        else
          //We cannot use this kind of data for the message body! Therefore, move it as attachment
          bodyAsAttachment = true;
      }
      else
        composeFormat = nsIMsgCompFormat::PlainText;

      char *body = nullptr;
      uint32_t bodyLen = 0;

      if (!bodyAsAttachment)
      {
        int64_t fileSize;
        nsCOMPtr<nsIFile> tempFileCopy;
        mdd->messageBody->m_tmpFile->Clone(getter_AddRefs(tempFileCopy));
        mdd->messageBody->m_tmpFile = do_QueryInterface(tempFileCopy);
        tempFileCopy = nullptr;
        mdd->messageBody->m_tmpFile->GetFileSize(&fileSize);
        bodyLen = fileSize;
        body = (char *)PR_MALLOC (bodyLen + 1);
        if (body)
        {
          memset (body, 0, bodyLen+1);

          uint32_t bytesRead;
          nsCOMPtr <nsIInputStream> inputStream;

          nsresult rv = NS_NewLocalFileInputStream(getter_AddRefs(inputStream), mdd->messageBody->m_tmpFile);
          if (NS_FAILED(rv))
            return;

          inputStream->Read(body, bodyLen, &bytesRead);

          inputStream->Close();

          // Convert the body to UTF-8
          char *mimeCharset = nullptr;
          // Get a charset from the header if no override is set.
          if (!charsetOverride)
            mimeCharset = MimeHeaders_get_parameter (mdd->messageBody->m_type.get(), "charset", nullptr, nullptr);
          // If no charset is specified in the header then use the default.
          char *bodyCharset = mimeCharset ? mimeCharset : mdd->mailcharset;
          if (bodyCharset)
          {
            nsAutoString tmpUnicodeBody;
            rv = ConvertToUnicode(bodyCharset, body, tmpUnicodeBody);
            if (NS_FAILED(rv)) // Tough luck, ASCII/ISO-8859-1 then...
              CopyASCIItoUTF16(nsDependentCString(body), tmpUnicodeBody);

            char *newBody = ToNewUTF8String(tmpUnicodeBody);
            if (newBody)
            {
              PR_Free(body);
              body = newBody;
              bodyLen = strlen(newBody);
            }
          }
          PR_FREEIF(mimeCharset);
        }
      }

      bool convertToPlainText = false;
      if (forward_inline)
      {
        if (mdd->identity)
        {
          bool identityComposeHTML;
          mdd->identity->GetComposeHtml(&identityComposeHTML);
          if ((identityComposeHTML && !mdd->overrideComposeFormat) ||
              (!identityComposeHTML && mdd->overrideComposeFormat))
          {
            // In the end, we're going to compose in HTML mode...

            if (body && composeFormat == nsIMsgCompFormat::PlainText)
            {
              // ... but the message body is currently plain text.

              //We need to convert the plain/text to HTML in order to escape any HTML markup
              char *escapedBody = MsgEscapeHTML(body);
              if (escapedBody)
              {
                PR_Free(body);
                body = escapedBody;
                bodyLen = strlen(body);
              }

               //+13 chars for <pre> & </pre> tags and CRLF
              uint32_t newbodylen = bodyLen + 14;
              char* newbody = (char *)PR_MALLOC (newbodylen);
              if (newbody)
              {
                *newbody = 0;
                PL_strcatn(newbody, newbodylen, "<PRE>");
                PL_strcatn(newbody, newbodylen, body);
                PL_strcatn(newbody, newbodylen, "</PRE>" CRLF);
                PR_Free(body);
                body = newbody;
              }
            }
            // Body is now HTML, set the format too (so headers are inserted in
            // correct format).
            composeFormat = nsIMsgCompFormat::HTML;
          }
          else if ((identityComposeHTML && mdd->overrideComposeFormat) || !identityComposeHTML)
          {
            // In the end, we're going to compose in plain text mode...

            if (composeFormat == nsIMsgCompFormat::HTML)
            {
              // ... but the message body is currently HTML.
              // We'll do the conversion later on when headers have been
              // inserted, body has been set and converted to unicode.
              convertToPlainText = true;
            }
          }
        }

        mime_insert_forwarded_message_headers(&body, mdd->headers, composeFormat,
          mdd->mailcharset);

      }

      // convert from UTF-8 to UTF-16
      if (body)
      {
        fields->SetBody(NS_ConvertUTF8toUTF16(body));
        PR_Free(body);
      }

      //
      // At this point, we need to create a message compose window or editor
      // window via XP-COM with the information that we have retrieved from
      // the message store.
      //
      if (mdd->format_out == nsMimeOutput::nsMimeMessageEditorTemplate)
      {
        fields->SetDraftId(mdd->url_name);
        MSG_ComposeType msgComposeType = PL_strstr(mdd->url_name,
                                                   "&redirect=true") ?
                                         nsIMsgCompType::Redirect :
                                         nsIMsgCompType::Template;
        CreateTheComposeWindow(fields, newAttachData, msgComposeType,
                               composeFormat, mdd->identity,
                               mdd->originalMsgURI, mdd->origMsgHdr);
      }
      else
      {
        if (mdd->forwardInline)
        {
          if (convertToPlainText)
            fields->ConvertBodyToPlainText();
          if (mdd->overrideComposeFormat)
            composeFormat = nsIMsgCompFormat::OppositeOfDefault;
          if (mdd->forwardInlineFilter)
          {
            fields->SetTo(mdd->forwardToAddress);
            ForwardMsgInline(fields, newAttachData, composeFormat,
                             mdd->identity, mdd->originalMsgURI,
                             mdd->origMsgHdr);
          }
          else
            CreateTheComposeWindow(fields, newAttachData,
                                   nsIMsgCompType::ForwardInline, composeFormat,
                                   mdd->identity, mdd->originalMsgURI,
                                   mdd->origMsgHdr);
        }
        else
        {
          fields->SetDraftId(mdd->url_name);
          CreateTheComposeWindow(fields, newAttachData, nsIMsgCompType::Draft, composeFormat, mdd->identity, mdd->originalMsgURI, mdd->origMsgHdr);
        }
      }
    }
    else
    {
      //
      // At this point, we need to create a message compose window via
      // XP-COM with the information that we have retrieved from the message store.
      //
      if (mdd->format_out == nsMimeOutput::nsMimeMessageEditorTemplate)
      {
#ifdef NS_DEBUG
        printf("RICHIE: Time to create the EDITOR with this template - NO body!!!!\n");
#endif
        CreateTheComposeWindow(fields, newAttachData, nsIMsgCompType::Template, nsIMsgCompFormat::Default, mdd->identity, nullptr, mdd->origMsgHdr);
      }
      else
      {
#ifdef NS_DEBUG
        printf("Time to create the composition window WITHOUT a body!!!!\n");
#endif
        if (mdd->forwardInline)
        {
          MSG_ComposeFormat composeFormat = (mdd->overrideComposeFormat) ?
            nsIMsgCompFormat::OppositeOfDefault : nsIMsgCompFormat::Default;
          CreateTheComposeWindow(fields, newAttachData,
                                 nsIMsgCompType::ForwardInline, composeFormat,
                                 mdd->identity, mdd->originalMsgURI,
                                 mdd->origMsgHdr);
        }
        else
        {
          fields->SetDraftId(mdd->url_name);
          CreateTheComposeWindow(fields, newAttachData, nsIMsgCompType::Draft, nsIMsgCompFormat::Default, mdd->identity, nullptr, mdd->origMsgHdr);
        }
      }
    }
  }
  else
  {
    CreateCompositionFields( from, repl, to, cc, bcc, fcc, grps, foll,
      org, subj, refs, 0, priority, news_host,
      mdd->mailcharset,
      getter_AddRefs(fields));
    if (fields)
      CreateTheComposeWindow(fields, newAttachData, nsIMsgCompType::New, nsIMsgCompFormat::Default, mdd->identity, nullptr, mdd->origMsgHdr);
  }

  if ( mdd->headers )
    MimeHeaders_free ( mdd->headers );

  //
  // Free the original attachment structure...
  // Make sure we only cleanup the local copy of the memory and not kill
  // files we need on disk
  //
  if (bodyAsAttachment)
    mdd->messageBody->m_tmpFile = nullptr;
  else if (mdd->messageBody && mdd->messageBody->m_tmpFile)
    mdd->messageBody->m_tmpFile->Remove(false);

  delete mdd->messageBody;

  for (int i = 0; i < mdd->attachments.Length(); i++)
    mdd->attachments[i]->m_tmpFile = nullptr;

  PR_FREEIF(mdd->mailcharset);

  mdd->identity = nullptr;
  PR_Free(mdd->url_name);
  PR_Free(mdd->originalMsgURI);
  mdd->origMsgHdr = nullptr;
  PR_Free(mdd);

  PR_FREEIF(host);
  PR_FREEIF(to_and_cc);
  PR_FREEIF(re_subject);
  PR_FREEIF(new_refs);
  PR_FREEIF(from);
  PR_FREEIF(repl);
  PR_FREEIF(subj);
  PR_FREEIF(id);
  PR_FREEIF(refs);
  PR_FREEIF(to);
  PR_FREEIF(cc);
  PR_FREEIF(grps);
  PR_FREEIF(foll);
  PR_FREEIF(priority);
  PR_FREEIF(draftInfo);
  PR_Free(identityKey);

  delete [] newAttachData;
}

static void
mime_parse_stream_abort (nsMIMESession *stream, int status )
{
  mime_draft_data *mdd = (mime_draft_data *) stream->data_object;
  NS_ASSERTION (mdd, "null mime draft data");

  if (!mdd)
    return;

  if (mdd->obj)
  {
    int status=0;

    if ( !mdd->obj->closed_p )
      status = mdd->obj->clazz->parse_eof ( mdd->obj, true );
    if ( !mdd->obj->parsed_p )
      mdd->obj->clazz->parse_end( mdd->obj, true );

    NS_ASSERTION ( mdd->options == mdd->obj->options, "draft display options not same as mime obj" );
    mime_free (mdd->obj);
    mdd->obj = 0;
    if (mdd->options)
    {
      delete mdd->options;
      mdd->options = 0;
    }

    if (mdd->stream)
    {
      mdd->stream->abort ((nsMIMESession *)mdd->stream->data_object, status);
      PR_Free( mdd->stream );
      mdd->stream = 0;
    }
  }

  if ( mdd->headers )
    MimeHeaders_free (mdd->headers);


  mime_free_attachments(mdd->attachments);

  PR_FREEIF(mdd->mailcharset);

  PR_Free (mdd);
}

static int
make_mime_headers_copy ( void *closure, MimeHeaders *headers )
{
  mime_draft_data *mdd = (mime_draft_data *) closure;

  NS_ASSERTION ( mdd && headers, "null mime draft data and/or headers" );

  if ( !mdd || ! headers )
    return 0;

  NS_ASSERTION ( mdd->headers == NULL , "non null mime draft data headers");

  mdd->headers = MimeHeaders_copy ( headers );
  mdd->options->done_parsing_outer_headers = true;

  return 0;
}

int
mime_decompose_file_init_fn ( void *stream_closure, MimeHeaders *headers )
{
  mime_draft_data *mdd = (mime_draft_data *) stream_closure;
  nsMsgAttachedFile *attachments = 0, *newAttachment = 0;
  int nAttachments = 0;
  //char *hdr_value = NULL;
  char *parm_value = NULL;
  bool needURL = false;
  bool creatingMsgBody = true;
  bool bodyPart = false;

  NS_ASSERTION (mdd && headers, "null mime draft data and/or headers");
  if (!mdd || !headers)
    return -1;

  if (mdd->options->decompose_init_count)
  {
    mdd->options->decompose_init_count++;
    NS_ASSERTION(mdd->curAttachment, "missing attachment in mime_decompose_file_init_fn");
    if (mdd->curAttachment)
      mdd->curAttachment->m_type.Adopt(MimeHeaders_get(headers,
                                                       HEADER_CONTENT_TYPE,
                                                       true, false));
    return 0;
  }
  else
    mdd->options->decompose_init_count++;

  nAttachments = mdd->attachments.Length();

  if (!nAttachments && !mdd->messageBody)
  {
    // if we've been told to use an override charset then do so....otherwise use the charset
    // inside the message header...
    if (mdd->options && mdd->options->override_charset)
        mdd->mailcharset = strdup(mdd->options->default_charset);
    else
    {
      char *contentType;
      contentType = MimeHeaders_get(headers, HEADER_CONTENT_TYPE, false, false);
      if (contentType)
      {
        mdd->mailcharset = MimeHeaders_get_parameter(contentType, "charset", NULL, NULL);
        PR_FREEIF(contentType);
      }
    }

    mdd->messageBody = new nsMsgAttachedFile;
    if (!mdd->messageBody)
      return MIME_OUT_OF_MEMORY;
    newAttachment = mdd->messageBody;
    creatingMsgBody = true;
    bodyPart = true;
  }
  else
  {
    /* always allocate one more extra; don't ask me why */
    needURL = true;
    newAttachment = new nsMsgAttachedFile;
    if (!newAttachment)
      return MIME_OUT_OF_MEMORY;
    mdd->attachments.AppendElement(newAttachment);
  }

  char *workURLSpec = nullptr;
  char *contLoc = nullptr;

  newAttachment->m_realName.Adopt(MimeHeaders_get_name(headers, mdd->options));
  contLoc = MimeHeaders_get( headers, HEADER_CONTENT_LOCATION, false, false );
  if (!contLoc)
      contLoc = MimeHeaders_get( headers, HEADER_CONTENT_BASE, false, false );

  if (!contLoc && !newAttachment->m_realName.IsEmpty())
    workURLSpec = ToNewCString(newAttachment->m_realName);
  if ( (contLoc) && (!workURLSpec) )
    workURLSpec = strdup(contLoc);

  PR_FREEIF(contLoc);

  mdd->curAttachment = newAttachment;
  newAttachment->m_type.Adopt(MimeHeaders_get ( headers, HEADER_CONTENT_TYPE, false, false ));

  //
  // This is to handle the degenerated Apple Double attachment.
  //
  parm_value = MimeHeaders_get( headers, HEADER_CONTENT_TYPE, false, false );
  if (parm_value)
  {
    char *boundary = NULL;
    char *tmp_value = NULL;
    boundary = MimeHeaders_get_parameter(parm_value, "boundary", NULL, NULL);
    if (boundary)
      tmp_value = PR_smprintf("; boundary=\"%s\"", boundary);
    if (tmp_value)
      newAttachment->m_type = tmp_value;
    newAttachment->m_xMacType.Adopt(
      MimeHeaders_get_parameter(parm_value, "x-mac-type", NULL, NULL));
    newAttachment->m_xMacCreator.Adopt(
      MimeHeaders_get_parameter(parm_value, "x-mac-creator", NULL, NULL));
    PR_FREEIF(parm_value);
    PR_FREEIF(boundary);
    PR_FREEIF(tmp_value);
  }

  newAttachment->m_size = 0;
  newAttachment->m_encoding.Adopt(MimeHeaders_get (headers, HEADER_CONTENT_TRANSFER_ENCODING,
                                                   false, false));
  newAttachment->m_description.Adopt(MimeHeaders_get(headers, HEADER_CONTENT_DESCRIPTION,
                                                     false, false ));
  //
  // If we came up empty for description or the orig URL, we should do something about it.
  //
  if (newAttachment->m_description.IsEmpty() && workURLSpec)
    newAttachment->m_description = workURLSpec;

  newAttachment->m_cloudPartInfo.Adopt(MimeHeaders_get(headers,
                                       HEADER_X_MOZILLA_CLOUD_PART,
                                       false, false));

  // There's no file in the message if it's a cloud part.
  if (!newAttachment->m_cloudPartInfo.IsEmpty())
  {
    nsCAutoString fileURL;
    fileURL.Adopt(
      MimeHeaders_get_parameter(newAttachment->m_cloudPartInfo.get(), "file",
                                nullptr, nullptr));
    if (!fileURL.IsEmpty())
      nsMimeNewURI(getter_AddRefs(newAttachment->m_origUrl), fileURL.get(),
                   nullptr);
    mdd->tmpFile = nullptr;
    return 0;
  }

  nsCOMPtr <nsIFile> tmpFile = nullptr;
  {
    // Let's build a temp file with an extension based on the content-type: nsmail.<extension>

    nsCAutoString  newAttachName ("nsmail");
    bool extensionAdded = false;
    // the content type may contain a charset. i.e. text/html; ISO-2022-JP...we want to strip off the charset
    // before we ask the mime service for a mime info for this content type.
    nsCAutoString contentType (newAttachment->m_type);
    int32_t pos = contentType.FindChar(';');
    if (pos > 0)
      contentType.SetLength(pos);
    nsresult  rv = NS_OK;
    nsCOMPtr<nsIMIMEService> mimeFinder (do_GetService(NS_MIMESERVICE_CONTRACTID, &rv));
    if (NS_SUCCEEDED(rv) && mimeFinder)
    {
      nsCAutoString fileExtension;
      rv = mimeFinder->GetPrimaryExtension(contentType, EmptyCString(), fileExtension);

      if (NS_SUCCEEDED(rv) && !fileExtension.IsEmpty())
      {
        newAttachName.Append(".");
        newAttachName.Append(fileExtension);
        extensionAdded = true;
      }
    }

    if (!extensionAdded)
    {
      newAttachName.Append(".tmp");
    }

    nsMsgCreateTempFile(newAttachName.get(), getter_AddRefs(tmpFile));
  }
  nsresult rv;

  // This needs to be done so the attachment structure has a handle
  // on the temp file for this attachment...
  // if ( (tmpFile) && (!bodyPart) )
  if (tmpFile)
  {
      nsCAutoString fileURL;
      rv = NS_GetURLSpecFromFile(tmpFile, fileURL);
      if (NS_SUCCEEDED(rv))
        nsMimeNewURI(getter_AddRefs(newAttachment->m_origUrl),
                     fileURL.get(), nullptr);
  }

  PR_FREEIF(workURLSpec);
  if (!tmpFile)
    return MIME_OUT_OF_MEMORY;

  mdd->tmpFile = do_QueryInterface(tmpFile);

  newAttachment->m_tmpFile = mdd->tmpFile;

  rv = MsgNewBufferedFileOutputStream(getter_AddRefs(mdd->tmpFileStream), tmpFile,PR_WRONLY | PR_CREATE_FILE, 00600);
  if (NS_FAILED(rv))
    return MIME_UNABLE_TO_OPEN_TMP_FILE;

  // For now, we are always going to decode all of the attachments
  // for the message. This way, we have native data
  if (creatingMsgBody)
  {
    MimeDecoderData *(*fn) (MimeConverterOutputCallback, void*) = 0;

    //
    // Initialize a decoder if necessary.
    //
    if (newAttachment->m_encoding.LowerCaseEqualsLiteral(ENCODING_BASE64))
      fn = &MimeB64DecoderInit;
    else if (newAttachment->m_encoding.LowerCaseEqualsLiteral(ENCODING_QUOTED_PRINTABLE))
    {
      mdd->decoder_data = MimeQPDecoderInit (/* The (MimeConverterOutputCallback) cast is to turn the `void' argument into `MimeObject'. */
                              ((MimeConverterOutputCallback) dummy_file_write),
                              mdd->tmpFileStream);
      if (!mdd->decoder_data)
        return MIME_OUT_OF_MEMORY;
    }
    else if (newAttachment->m_encoding.LowerCaseEqualsLiteral(ENCODING_UUENCODE) ||
             newAttachment->m_encoding.LowerCaseEqualsLiteral(ENCODING_UUENCODE2) ||
             newAttachment->m_encoding.LowerCaseEqualsLiteral(ENCODING_UUENCODE3) ||
             newAttachment->m_encoding.LowerCaseEqualsLiteral(ENCODING_UUENCODE4))
      fn = &MimeUUDecoderInit;
    else if (newAttachment->m_encoding.LowerCaseEqualsLiteral(ENCODING_YENCODE))
      fn = &MimeYDecoderInit;

    if (fn)
    {
      mdd->decoder_data = fn (/* The (MimeConverterOutputCallback) cast is to
                                 turn the `void' argument into `MimeObject'. */
                              ((MimeConverterOutputCallback) dummy_file_write),
                              mdd->tmpFileStream);
      if (!mdd->decoder_data)
        return MIME_OUT_OF_MEMORY;
    }
  }

  return 0;
}

int
mime_decompose_file_output_fn (const char     *buf,
                               int32_t  size,
                               void     *stream_closure )
{
  mime_draft_data *mdd = (mime_draft_data *) stream_closure;
  int ret = 0;

  NS_ASSERTION (mdd && buf, "missing mime draft data and/or buf");
  if (!mdd || !buf) return -1;
  if (!size) return 0;

  if ( !mdd->tmpFileStream )
    return 0;

  if (mdd->decoder_data) {
    int32_t outsize;
    ret = MimeDecoderWrite(mdd->decoder_data, buf, size, &outsize);
    if (ret == -1) return -1;
    mdd->curAttachment->m_size += outsize;
  }
  else
  {
    uint32_t bytesWritten;
    mdd->tmpFileStream->Write(buf, size, &bytesWritten);
    if (bytesWritten < size)
      return MIME_ERROR_WRITING_FILE;
    mdd->curAttachment->m_size += size;
  }

  return 0;
}

int
mime_decompose_file_close_fn ( void *stream_closure )
{
  mime_draft_data *mdd = (mime_draft_data *) stream_closure;

  if (!mdd)
    return -1;

  if ( --mdd->options->decompose_init_count > 0 )
      return 0;

  if (mdd->decoder_data) {
    MimeDecoderDestroy(mdd->decoder_data, false);
    mdd->decoder_data = 0;
  }

  if (!mdd->tmpFileStream) {
    // it's ok to have a null tmpFileStream if there's no tmpFile.
    // This happens for cloud file attachments.
    NS_ASSERTION(!mdd->tmpFile, "shouldn't have a tmp file bu no stream");
    return 0;
  }
  mdd->tmpFileStream->Close();

  mdd->tmpFileStream = nullptr;

  mdd->tmpFile = nullptr;

  return 0;
}

extern "C" void  *
mime_bridge_create_draft_stream(
                          nsIMimeEmitter      *newEmitter,
                          nsStreamConverter   *newPluginObj2,
                          nsIURI              *uri,
                          nsMimeOutputType    format_out)
{
  int                     status = 0;
  nsMIMESession           *stream = nullptr;
  mime_draft_data  *mdd = nullptr;
  MimeObject              *obj = nullptr;

  if ( !uri )
    return nullptr;

  mdd = new mime_draft_data;
  if (!mdd)
    return nullptr;

  nsCAutoString turl;
  nsCOMPtr <nsIMsgMessageService> msgService;
  nsCOMPtr<nsIURI> aURL;
  nsCAutoString urlString;
  nsresult rv;

  // first, convert the rdf msg uri into a url that represents the message...
  if (NS_FAILED(uri->GetSpec(turl)))
    goto FAIL;

  rv = GetMessageServiceFromURI(turl, getter_AddRefs(msgService));
  if (NS_FAILED(rv))
    goto FAIL;

  rv = msgService->GetUrlForUri(turl.get(), getter_AddRefs(aURL), nullptr);
  if (NS_FAILED(rv))
    goto FAIL;

  if (NS_SUCCEEDED(aURL->GetSpec(urlString)))
  {
    int32_t typeIndex = urlString.Find("&type=application/x-message-display");
    if (typeIndex != -1)
      urlString.Cut(typeIndex, sizeof("&type=application/x-message-display") - 1);

    mdd->url_name = ToNewCString(urlString);
    if (!(mdd->url_name))
      goto FAIL;
  }

  newPluginObj2->GetForwardInline(&mdd->forwardInline);
  newPluginObj2->GetForwardInlineFilter(&mdd->forwardInlineFilter);
  newPluginObj2->GetForwardToAddress(mdd->forwardToAddress);
  newPluginObj2->GetOverrideComposeFormat(&mdd->overrideComposeFormat);
  newPluginObj2->GetIdentity(getter_AddRefs(mdd->identity));
  newPluginObj2->GetOriginalMsgURI(&mdd->originalMsgURI);
  newPluginObj2->GetOrigMsgHdr(getter_AddRefs(mdd->origMsgHdr));
  mdd->format_out = format_out;
  mdd->options = new  MimeDisplayOptions ;
  if (!mdd->options)
    goto FAIL;

  mdd->options->url = strdup(mdd->url_name);
  mdd->options->format_out = format_out;     // output format
  mdd->options->decompose_file_p = true; /* new field in MimeDisplayOptions */
  mdd->options->stream_closure = mdd;
  mdd->options->html_closure = mdd;
  mdd->options->decompose_headers_info_fn = make_mime_headers_copy;
  mdd->options->decompose_file_init_fn = mime_decompose_file_init_fn;
  mdd->options->decompose_file_output_fn = mime_decompose_file_output_fn;
  mdd->options->decompose_file_close_fn = mime_decompose_file_close_fn;

  mdd->options->m_prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  if (NS_FAILED(rv))
    goto FAIL;

#ifdef ENABLE_SMIME
  /* If we're attaching a message (for forwarding) then we must eradicate all
   traces of xlateion from it, since forwarding someone else a message
   that wasn't xlated for them doesn't work.  We have to dexlate it
   before sending it.
   */
  mdd->options->decrypt_p = true;
#endif /* ENABLE_SMIME */

  obj = mime_new ( (MimeObjectClass *) &mimeMessageClass, (MimeHeaders *) NULL, MESSAGE_RFC822 );
  if ( !obj )
    goto FAIL;

  obj->options = mdd->options;
  mdd->obj = obj;

  stream = PR_NEWZAP ( nsMIMESession );
  if ( !stream )
    goto FAIL;

  stream->name = "MIME To Draft Converter Stream";
  stream->complete = mime_parse_stream_complete;
  stream->abort = mime_parse_stream_abort;
  stream->put_block = mime_parse_stream_write;
  stream->data_object = mdd;

  status = obj->clazz->initialize ( obj );
  if ( status >= 0 )
    status = obj->clazz->parse_begin ( obj );
  if ( status < 0 )
    goto FAIL;

  return stream;

FAIL:
  if (mdd)
  {
    PR_Free(mdd->url_name);
    PR_Free(mdd->originalMsgURI);
    if (mdd->options)
      delete mdd->options;
    PR_Free ( mdd );
  }
  PR_Free ( stream );
  PR_Free ( obj );

  return nullptr;
}
