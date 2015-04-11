/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "msgCore.h"
#include "prmem.h"
#include "nsMsgSearchCore.h"
#include "nsIMsgSearchSession.h"
#include "nsMsgUtils.h"
#include "nsIMsgDatabase.h"
#include "nsIMsgHdr.h"
#include "nsMsgSearchTerm.h"
#include "nsMsgSearchScopeTerm.h"
#include "nsMsgBodyHandler.h"
#include "nsMsgResultElement.h"
#include "nsIMsgImapMailFolder.h"
#include "nsMsgSearchImap.h"
#include "nsMsgLocalSearch.h"
#include "nsMsgSearchNews.h"
#include "nsMsgSearchValue.h"
#include "nsMsgI18N.h"
#include "nsIMimeConverter.h"
#include "nsMsgMimeCID.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsIMsgFilterPlugin.h"
#include "nsIFile.h"
#include "nsISupportsObsolete.h"
#include "nsISeekableStream.h"
#include "nsNetCID.h"
#include "nsIFileStreams.h"
#include "nsUnicharUtils.h"
#include "nsIAbCard.h"
#include "nsServiceManagerUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsMemory.h"
#include <ctype.h>
#include "nsMsgBaseCID.h"
#include "nsIMsgTagService.h"
#include "nsMsgMessageFlags.h"
#include "nsIMsgFilterService.h"
#include "nsIMsgPluggableStore.h"
#include "nsAbBaseCID.h"
#include "nsIAbManager.h"

//---------------------------------------------------------------------------
// nsMsgSearchTerm specifies one criterion, e.g. name contains phil
//---------------------------------------------------------------------------


//-----------------------------------------------------------------------------
//-------------------- Implementation of nsMsgSearchTerm -----------------------
//-----------------------------------------------------------------------------
#define MAILNEWS_CUSTOM_HEADERS "mailnews.customHeaders"

typedef struct
{
  nsMsgSearchAttribValue  attrib;
  const char      *attribName;
} nsMsgSearchAttribEntry;

nsMsgSearchAttribEntry SearchAttribEntryTable[] =
{
    {nsMsgSearchAttrib::Subject,    "subject"},
    {nsMsgSearchAttrib::Sender,     "from"},
    {nsMsgSearchAttrib::Body,       "body"},
    {nsMsgSearchAttrib::Date,       "date"},
    {nsMsgSearchAttrib::Priority,   "priority"},
    {nsMsgSearchAttrib::MsgStatus,  "status"},
    {nsMsgSearchAttrib::To,         "to"},
    {nsMsgSearchAttrib::CC,         "cc"},
    {nsMsgSearchAttrib::ToOrCC,     "to or cc"},
    {nsMsgSearchAttrib::AllAddresses, "all addresses"},
    {nsMsgSearchAttrib::AgeInDays,  "age in days"},
    {nsMsgSearchAttrib::Label,      "label"},
    {nsMsgSearchAttrib::Keywords,   "tag"},
    {nsMsgSearchAttrib::Size,       "size"},
    // this used to be nsMsgSearchAttrib::SenderInAddressBook
    // we used to have two Sender menuitems
    // for backward compatability, we can still parse
    // the old style.  see bug #179803
    {nsMsgSearchAttrib::Sender,     "from in ab"},
    {nsMsgSearchAttrib::JunkStatus, "junk status"},
    {nsMsgSearchAttrib::JunkPercent, "junk percent"},
    {nsMsgSearchAttrib::JunkScoreOrigin, "junk score origin"},
    {nsMsgSearchAttrib::HasAttachmentStatus, "has attachment status"},
};

static const unsigned int sNumSearchAttribEntryTable =
  NS_ARRAY_LENGTH(SearchAttribEntryTable);

// Take a string which starts off with an attribute
// and return the matching attribute. If the string is not in the table, and it
// begins with a quote, then we can conclude that it is an arbitrary header.
// Otherwise if not in the table, it is the id for a custom search term.
nsresult NS_MsgGetAttributeFromString(const char *string, int16_t *attrib, nsACString &aCustomId)
{
  NS_ENSURE_ARG_POINTER(string);
  NS_ENSURE_ARG_POINTER(attrib);

  bool found = false;
  bool isArbitraryHeader = false;
  // arbitrary headers have a leading quote
  if (*string != '"')
  {
    for (unsigned int idxAttrib = 0; idxAttrib < sNumSearchAttribEntryTable; idxAttrib++)
    {
      if (!PL_strcasecmp(string, SearchAttribEntryTable[idxAttrib].attribName))
      {
        found = true;
        *attrib = SearchAttribEntryTable[idxAttrib].attrib;
        break;
      }
    }
  }
  else // remove the leading quote
  {
    string++;
    isArbitraryHeader = true;
  }

  if (!found && !isArbitraryHeader)
  {
    // must be a custom attribute
    *attrib = nsMsgSearchAttrib::Custom;
    aCustomId.Assign(string);
    return NS_OK;
  }

  if (!found)
  {
    nsresult rv;
    bool goodHdr;
    IsRFC822HeaderFieldName(string, &goodHdr);
    if (!goodHdr)
      return NS_MSG_INVALID_CUSTOM_HEADER;
    //49 is for showing customize... in ui, headers start from 50 onwards up until 99.
    *attrib = nsMsgSearchAttrib::OtherHeader+1;

    nsCOMPtr<nsIPrefService> prefService = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIPrefBranch> prefBranch;
    rv = prefService->GetBranch(nullptr, getter_AddRefs(prefBranch));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCString headers;
    prefBranch->GetCharPref(MAILNEWS_CUSTOM_HEADERS, getter_Copies(headers));

    if (!headers.IsEmpty())
    {
      nsCAutoString hdrStr(headers);
      hdrStr.StripWhitespace();  //remove whitespace before parsing

      char *newStr= hdrStr.BeginWriting();
      char *token = NS_strtok(":", &newStr);
      uint32_t i=0;
      while (token)
      {
        if (PL_strcasecmp(token, string) == 0)
        {
          *attrib += i; //we found custom header in the pref
          found = true;
          break;
        }
        token = NS_strtok(":", &newStr);
        i++;
      }
    }
  }
  // If we didn't find the header in MAILNEWS_CUSTOM_HEADERS, we're
  // going to return NS_OK and an attrib of nsMsgSearchAttrib::OtherHeader+1.
  // in case it's a client side spam filter description filter,
  // which doesn't add its headers to mailnews.customMailHeaders.
  // We've already checked that it's a valid header and returned
  // an error if so.

  return NS_OK;
}

nsresult NS_MsgGetStringForAttribute(int16_t attrib, const char **string)
{
  NS_ENSURE_ARG_POINTER(string);

  bool found = false;
  for (unsigned int idxAttrib = 0; idxAttrib < sNumSearchAttribEntryTable; idxAttrib++)
  {
    // I'm using the idx's as aliases into MSG_SearchAttribute and
    // MSG_SearchOperator enums which is legal because of the way the
    // enums are defined (starts at 0, numItems at end)
    if (attrib == SearchAttribEntryTable[idxAttrib].attrib)
    {
      found = true;
      *string = SearchAttribEntryTable[idxAttrib].attribName;
      break;
    }
  }
  if (!found)
    *string = '\0'; // don't leave the string uninitialized

  // we no longer return invalid attribute. If we cannot find the string in the table,
  // then it is an arbitrary header. Return success regardless if found or not
  return NS_OK;
}

typedef struct
{
  nsMsgSearchOpValue  op;
  const char      *opName;
} nsMsgSearchOperatorEntry;

nsMsgSearchOperatorEntry SearchOperatorEntryTable[] =
{
  {nsMsgSearchOp::Contains, "contains"},
  {nsMsgSearchOp::DoesntContain,"doesn't contain"},
  {nsMsgSearchOp::Is,"is"},
  {nsMsgSearchOp::Isnt,  "isn't"},
  {nsMsgSearchOp::IsEmpty, "is empty"},
  {nsMsgSearchOp::IsntEmpty, "isn't empty"},
  {nsMsgSearchOp::IsBefore, "is before"},
  {nsMsgSearchOp::IsAfter, "is after"},
  {nsMsgSearchOp::IsHigherThan, "is higher than"},
  {nsMsgSearchOp::IsLowerThan, "is lower than"},
  {nsMsgSearchOp::BeginsWith, "begins with"},
  {nsMsgSearchOp::EndsWith, "ends with"},
  {nsMsgSearchOp::IsInAB, "is in ab"},
  {nsMsgSearchOp::IsntInAB, "isn't in ab"},
  {nsMsgSearchOp::IsGreaterThan, "is greater than"},
  {nsMsgSearchOp::IsLessThan, "is less than"},
  {nsMsgSearchOp::Matches, "matches"},
  {nsMsgSearchOp::DoesntMatch, "doesn't match"}
};

static const unsigned int sNumSearchOperatorEntryTable =
  NS_ARRAY_LENGTH(SearchOperatorEntryTable);

nsresult NS_MsgGetOperatorFromString(const char *string, int16_t *op)
{
  NS_ENSURE_ARG_POINTER(string);
  NS_ENSURE_ARG_POINTER(op);

  bool found = false;
  for (unsigned int idxOp = 0; idxOp < sNumSearchOperatorEntryTable; idxOp++)
  {
    // I'm using the idx's as aliases into MSG_SearchAttribute and
    // MSG_SearchOperator enums which is legal because of the way the
    // enums are defined (starts at 0, numItems at end)
    if (!PL_strcasecmp(string, SearchOperatorEntryTable[idxOp].opName))
    {
      found = true;
      *op = SearchOperatorEntryTable[idxOp].op;
      break;
    }
  }
  return (found) ? NS_OK : NS_ERROR_INVALID_ARG;
}

nsresult NS_MsgGetStringForOperator(int16_t op, const char **string)
{
  NS_ENSURE_ARG_POINTER(string);

  bool found = false;
  for (unsigned int idxOp = 0; idxOp < sNumSearchOperatorEntryTable; idxOp++)
  {
    // I'm using the idx's as aliases into MSG_SearchAttribute and
    // MSG_SearchOperator enums which is legal because of the way the
    // enums are defined (starts at 0, numItems at end)
    if (op == SearchOperatorEntryTable[idxOp].op)
    {
      found = true;
      *string = SearchOperatorEntryTable[idxOp].opName;
      break;
    }
  }

  return (found) ? NS_OK : NS_ERROR_INVALID_ARG;
}

void NS_MsgGetUntranslatedStatusName (uint32 s, nsCString *outName)
{
  const char *tmpOutName = NULL;
#define MSG_STATUS_MASK (nsMsgMessageFlags::Read | nsMsgMessageFlags::Replied |\
  nsMsgMessageFlags::Forwarded | nsMsgMessageFlags::New | nsMsgMessageFlags::Marked)
  uint32_t maskOut = (s & MSG_STATUS_MASK);

  // diddle the flags to pay attention to the most important ones first, if multiple
  // flags are set. Should remove this code from the winfe.
  if (maskOut & nsMsgMessageFlags::New)
    maskOut = nsMsgMessageFlags::New;
  if (maskOut & nsMsgMessageFlags::Replied &&
      maskOut & nsMsgMessageFlags::Forwarded)
    maskOut = nsMsgMessageFlags::Replied | nsMsgMessageFlags::Forwarded;
  else if (maskOut & nsMsgMessageFlags::Forwarded)
    maskOut = nsMsgMessageFlags::Forwarded;
  else if (maskOut & nsMsgMessageFlags::Replied)
    maskOut = nsMsgMessageFlags::Replied;

  switch (maskOut)
  {
  case nsMsgMessageFlags::Read:
    tmpOutName = "read";
    break;
  case nsMsgMessageFlags::Replied:
    tmpOutName = "replied";
    break;
  case nsMsgMessageFlags::Forwarded:
    tmpOutName = "forwarded";
    break;
  case nsMsgMessageFlags::Forwarded | nsMsgMessageFlags::Replied:
    tmpOutName = "replied and forwarded";
    break;
  case nsMsgMessageFlags::New:
    tmpOutName = "new";
    break;
  case nsMsgMessageFlags::Marked:
    tmpOutName = "flagged";
    break;
  default:
    // This is fine, status may be "unread" for example
    break;
  }

  if (tmpOutName)
    *outName = tmpOutName;
}


int32_t NS_MsgGetStatusValueFromName(char *name)
{
  if (!strcmp("read", name))
    return nsMsgMessageFlags::Read;
  if (!strcmp("replied", name))
    return nsMsgMessageFlags::Replied;
  if (!strcmp("forwarded", name))
    return nsMsgMessageFlags::Forwarded;
  if (!strcmp("replied and forwarded", name))
    return nsMsgMessageFlags::Forwarded | nsMsgMessageFlags::Replied;
  if (!strcmp("new", name))
    return nsMsgMessageFlags::New;
  if (!strcmp("flagged", name))
    return nsMsgMessageFlags::Marked;
  return 0;
}


// Needed for DeStream method.
nsMsgSearchTerm::nsMsgSearchTerm()
{
    // initialize this to zero
    m_value.string=nullptr;
    m_value.attribute=0;
    m_value.u.priority=0;
    m_attribute = nsMsgSearchAttrib::Default;
    m_operator = nsMsgSearchOp::Contains;
    mBeginsGrouping = false;
    mEndsGrouping = false;
    m_matchAll = false;
}

nsMsgSearchTerm::nsMsgSearchTerm (
                                  nsMsgSearchAttribValue attrib,
                                  nsMsgSearchOpValue op,
                                  nsIMsgSearchValue *val,
                                  nsMsgSearchBooleanOperator boolOp,
                                  const char * aCustomString)
{
  m_operator = op;
  m_attribute = attrib;
  m_booleanOp = boolOp;
  if (attrib > nsMsgSearchAttrib::OtherHeader  && attrib < nsMsgSearchAttrib::kNumMsgSearchAttributes && aCustomString)
  {
    m_arbitraryHeader = aCustomString;
    ToLowerCaseExceptSpecials(m_arbitraryHeader);
  }
  else if (attrib == nsMsgSearchAttrib::Custom)
  {
    m_customId = aCustomString;
  }

  nsMsgResultElement::AssignValues (val, &m_value);
  m_matchAll = false;
}



nsMsgSearchTerm::~nsMsgSearchTerm ()
{
  if (IS_STRING_ATTRIBUTE (m_attribute) && m_value.string)
    NS_Free(m_value.string);
}

NS_IMPL_ISUPPORTS1(nsMsgSearchTerm, nsIMsgSearchTerm)


// Perhaps we could find a better place for this?
// Caller needs to free.
/* static */char *nsMsgSearchTerm::EscapeQuotesInStr(const char *str)
{
  int  numQuotes = 0;
  for (const char *strPtr = str; *strPtr; strPtr++)
    if (*strPtr == '"')
      numQuotes++;
    int escapedStrLen = PL_strlen(str) + numQuotes;
    char  *escapedStr = (char *) PR_Malloc(escapedStrLen + 1);
    if (escapedStr)
    {
      char *destPtr;
      for (destPtr = escapedStr; *str; str++)
      {
        if (*str == '"')
          *destPtr++ = '\\';
        *destPtr++ = *str;
      }
      *destPtr = '\0';
    }
    return escapedStr;
}


nsresult nsMsgSearchTerm::OutputValue(nsCString &outputStr)
{
  if (IS_STRING_ATTRIBUTE(m_attribute) && m_value.string)
  {
    bool    quoteVal = false;
    // need to quote strings with ')' and strings starting with '"' or ' '
    // filter code will escape quotes
    if (PL_strchr(m_value.string, ')') ||
      (m_value.string[0] == ' ') ||
      (m_value.string[0] == '"'))
    {
      quoteVal = true;
      outputStr += "\"";
    }
    if (PL_strchr(m_value.string, '"'))
    {
      char *escapedString = nsMsgSearchTerm::EscapeQuotesInStr(m_value.string);
      if (escapedString)
      {
        outputStr += escapedString;
        PR_Free(escapedString);
      }

    }
    else
    {
      outputStr += m_value.string;
    }
    if (quoteVal)
      outputStr += "\"";
  }
  else
  {
    switch (m_attribute)
    {
    case nsMsgSearchAttrib::Date:
      {
        PRExplodedTime exploded;
        PR_ExplodeTime(m_value.u.date, PR_LocalTimeParameters, &exploded);

        // wow, so tm_mon is 0 based, tm_mday is 1 based.
        char dateBuf[100];
        PR_FormatTimeUSEnglish (dateBuf, sizeof(dateBuf), "%d-%b-%Y", &exploded);
        outputStr += dateBuf;
        break;
      }
    case nsMsgSearchAttrib::AgeInDays:
      {
        outputStr.AppendInt(m_value.u.age);
        break;
      }
    case nsMsgSearchAttrib::Label:
      {
        outputStr.AppendInt(m_value.u.label);
        break;
      }
    case nsMsgSearchAttrib::JunkStatus:
      {
        outputStr.AppendInt(m_value.u.junkStatus); // only if we write to disk, right?
        break;
      }
    case nsMsgSearchAttrib::JunkPercent:
      {
        outputStr.AppendInt(m_value.u.junkPercent);
        break;
      }
    case nsMsgSearchAttrib::MsgStatus:
      {
        nsCAutoString status;
        NS_MsgGetUntranslatedStatusName (m_value.u.msgStatus, &status);
        outputStr += status;
        break;
      }
    case nsMsgSearchAttrib::Priority:
      {
        nsCAutoString priority;
        NS_MsgGetUntranslatedPriorityName(m_value.u.priority, priority);
        outputStr += priority;
        break;
      }
    case nsMsgSearchAttrib::HasAttachmentStatus:
      {
        outputStr.Append("true");  // don't need anything here, really
        break;
      }
    case nsMsgSearchAttrib::Size:
      {
        outputStr.AppendInt(m_value.u.size);
        break;
      }
    case nsMsgSearchAttrib::Uint32HdrProperty:
      {
        outputStr.AppendInt(m_value.u.msgStatus);
        break;
      }
    default:
      NS_ASSERTION(false, "trying to output invalid attribute");
      break;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP nsMsgSearchTerm::GetTermAsString (nsACString &outStream)
{
  const char *operatorStr;
  nsCAutoString  outputStr;
  nsresult  ret;

  if (m_matchAll)
  {
    outStream = "ALL";
    return NS_OK;
  }

  if (m_attribute > nsMsgSearchAttrib::OtherHeader && m_attribute < nsMsgSearchAttrib::kNumMsgSearchAttributes)  // if arbitrary header, use it instead!
  {
    outputStr = "\"";
    outputStr += m_arbitraryHeader;
    outputStr += "\"";
  }

  else if (m_attribute == nsMsgSearchAttrib::Custom)
  {
    // use the custom id as the string
    outputStr = m_customId;
  }

  else if (m_attribute == nsMsgSearchAttrib::Uint32HdrProperty)
  {
    outputStr = m_hdrProperty;
  }

  else {
    const char *attrib;
    ret = NS_MsgGetStringForAttribute(m_attribute, &attrib);
    if (ret != NS_OK)
      return ret;
    outputStr = attrib;
  }

  outputStr += ',';

  ret = NS_MsgGetStringForOperator(m_operator, &operatorStr);
  if (ret != NS_OK)
    return ret;

  outputStr += operatorStr;
  outputStr += ',';

  OutputValue(outputStr);
  outStream = outputStr;
  return NS_OK;
}

// fill in m_value from the input stream.
nsresult nsMsgSearchTerm::ParseValue(char *inStream)
{
  if (IS_STRING_ATTRIBUTE(m_attribute))
  {
    bool quoteVal = false;
    while (isspace(*inStream))
      inStream++;
    // need to remove pair of '"', if present
    if (*inStream == '"')
    {
      quoteVal = true;
      inStream++;
    }
    int valueLen = PL_strlen(inStream);
    if (quoteVal && inStream[valueLen - 1] == '"')
      valueLen--;

    m_value.string = (char *) PR_Malloc(valueLen + 1);
    PL_strncpy(m_value.string, inStream, valueLen + 1);
    m_value.string[valueLen] = '\0';
  }
  else
  {
    switch (m_attribute)
    {
    case nsMsgSearchAttrib::Date:
      PR_ParseTimeString (inStream, false, &m_value.u.date);
      break;
    case nsMsgSearchAttrib::MsgStatus:
      m_value.u.msgStatus = NS_MsgGetStatusValueFromName(inStream);
      break;
    case nsMsgSearchAttrib::Priority:
      NS_MsgGetPriorityFromString(inStream, m_value.u.priority);
      break;
    case nsMsgSearchAttrib::AgeInDays:
      m_value.u.age = atoi(inStream);
      break;
    case nsMsgSearchAttrib::Label:
      m_value.u.label = atoi(inStream);
      break;
    case nsMsgSearchAttrib::JunkStatus:
      m_value.u.junkStatus = atoi(inStream); // only if we read from disk, right?
      break;
    case nsMsgSearchAttrib::JunkPercent:
      m_value.u.junkPercent = atoi(inStream);
      break;
    case nsMsgSearchAttrib::HasAttachmentStatus:
      m_value.u.msgStatus = nsMsgMessageFlags::Attachment;
      break; // this should always be true.
    case nsMsgSearchAttrib::Size:
      m_value.u.size = atoi(inStream);
      break;
    default:
      NS_ASSERTION(false, "invalid attribute parsing search term value");
      break;
    }
  }
  m_value.attribute = m_attribute;
  return NS_OK;
}

// find the operator code for this operator string.
nsresult
nsMsgSearchTerm::ParseOperator(char *inStream, nsMsgSearchOpValue *value)
{
  NS_ENSURE_ARG_POINTER(value);
  int16_t operatorVal;
  while (isspace(*inStream))
    inStream++;

  char *commaSep = PL_strchr(inStream, ',');

  if (commaSep)
    *commaSep = '\0';

  nsresult err = NS_MsgGetOperatorFromString(inStream, &operatorVal);
  *value = (nsMsgSearchOpValue) operatorVal;
  return err;
}

// find the attribute code for this comma-delimited attribute.
nsresult
nsMsgSearchTerm::ParseAttribute(char *inStream, nsMsgSearchAttribValue *attrib)
{
    while (isspace(*inStream))
        inStream++;

    // if we are dealing with an arbitrary header, it will be quoted....
    // it seems like a kludge, but to distinguish arbitrary headers from
    // standard headers with the same name, like "Date", we'll use the
    // presence of the quote to recognize arbitrary headers. We leave the
    // leading quote as a flag, but remove the trailing quote.
    bool quoteVal = false;
    if (*inStream == '"')
        quoteVal = true;

    // arbitrary headers are quoted. Skip first character, which will be the
    // first quote for arbitrary headers
    char *separator = strchr(inStream + 1, quoteVal ? '"' : ',');

    if (separator)
        *separator = '\0';

    int16_t attributeVal;
    nsCAutoString customId;
    nsresult rv = NS_MsgGetAttributeFromString(inStream, &attributeVal, m_customId);
    NS_ENSURE_SUCCESS(rv, rv);

    *attrib = (nsMsgSearchAttribValue) attributeVal;

    if (*attrib > nsMsgSearchAttrib::OtherHeader && *attrib < nsMsgSearchAttrib::kNumMsgSearchAttributes)  // if we are dealing with an arbitrary header....
    {
      m_arbitraryHeader = inStream + 1; // remove the leading quote
      ToLowerCaseExceptSpecials(m_arbitraryHeader);
    }
    return rv;
}

// De stream one search term. If the condition looks like
// condition = "(to or cc, contains, r-thompson) AND (body, doesn't contain, fred)"
// This routine should get called twice, the first time
// with "to or cc, contains, r-thompson", the second time with
// "body, doesn't contain, fred"

nsresult nsMsgSearchTerm::DeStreamNew (char *inStream, int16_t /*length*/)
{
  if (!strcmp(inStream, "ALL"))
  {
    m_matchAll = true;
    return NS_OK;
  }
  char *commaSep = PL_strchr(inStream, ',');
  nsresult rv = ParseAttribute(inStream, &m_attribute);  // will allocate space for arbitrary header if necessary
  NS_ENSURE_SUCCESS(rv, rv);
  if (!commaSep)
    return NS_ERROR_INVALID_ARG;
  char *secondCommaSep = PL_strchr(commaSep + 1, ',');
  if (commaSep)
    rv = ParseOperator(commaSep + 1, &m_operator);
  NS_ENSURE_SUCCESS(rv, rv);
  // convert label filters and saved searches to keyword equivalents
  if (secondCommaSep)
    ParseValue(secondCommaSep + 1);
  if (m_attribute == nsMsgSearchAttrib::Label)
  {
    nsCAutoString keyword("$label");
    m_value.attribute = m_attribute = nsMsgSearchAttrib::Keywords;
    keyword.Append('0' + m_value.u.label);
    m_value.string = PL_strdup(keyword.get());
  }
  return NS_OK;
}


// Looks in the MessageDB for the user specified arbitrary header, if it finds the header, it then looks for a match against
// the value for the header.
nsresult nsMsgSearchTerm::MatchArbitraryHeader (nsIMsgSearchScopeTerm *scope,
                                                uint32_t length /* in lines*/,
                                                const char *charset,
                                                bool charsetOverride,
                                                nsIMsgDBHdr *msg,
                                                nsIMsgDatabase* db,
                                                const char * headers,
                                                uint32_t headersSize,
                                                bool ForFiltering,
                                                bool *pResult)
{
  NS_ENSURE_ARG_POINTER(pResult);
  *pResult = false;
  nsresult err = NS_OK;
  bool matchExpected = m_operator == nsMsgSearchOp::Contains ||
                         m_operator == nsMsgSearchOp::Is ||
                         m_operator == nsMsgSearchOp::BeginsWith ||
                         m_operator == nsMsgSearchOp::EndsWith;
  // init result to what we want if we don't find the header at all
  bool result = !matchExpected;

  nsCString dbHdrValue;
  msg->GetStringProperty(m_arbitraryHeader.get(), getter_Copies(dbHdrValue));
  if (!dbHdrValue.IsEmpty())
    // match value with the other info.
    return MatchRfc2047String(dbHdrValue.get(), charset, charsetOverride, pResult);

  nsMsgBodyHandler * bodyHandler =
    new nsMsgBodyHandler (scope, length, msg, db, headers, headersSize,
                          ForFiltering);
  if (!bodyHandler)
    return NS_ERROR_OUT_OF_MEMORY;

  bodyHandler->SetStripHeaders (false);

  nsCString headerFullValue; // contains matched header value accumulated over multiple lines.
  nsCAutoString buf;
  nsCAutoString curMsgHeader;
  bool searchingHeaders = true;

  // We will allow accumulation of received headers;
  bool isReceivedHeader = m_arbitraryHeader.EqualsLiteral("received");
  
  while (searchingHeaders)
  {
    if (bodyHandler->GetNextLine(buf) < 0 || EMPTY_MESSAGE_LINE(buf))
      searchingHeaders = false;
    bool isContinuationHeader = searchingHeaders ? NS_IsAsciiWhitespace(buf.CharAt(0))
                                                   : false;

    // We try to match the header from the last time through the loop, which should now
    //  have accumulated over possible multiple lines. For all headers except received,
    //  we process a single accumulation, but process accumulated received at the end.
    if (!searchingHeaders || (!isContinuationHeader &&
         (!headerFullValue.IsEmpty() && !isReceivedHeader)))
    {
      // Make sure buf has info besides just the header.
      // Otherwise, it's either an empty header, or header not found.
      if (!headerFullValue.IsEmpty())
      {
        bool stringMatches;
        // match value with the other info.
        err = MatchRfc2047String(headerFullValue.get(), charset, charsetOverride, &stringMatches);
        if (matchExpected == stringMatches) // if we found a match
        {
          searchingHeaders = false;   // then stop examining the headers
          result = stringMatches;
        }
      }
      break;
    }

    char * buf_end = (char *) (buf.get() + buf.Length());
    int headerLength = m_arbitraryHeader.Length();

    // If the line starts with whitespace, then we use the current header.
    if (!isContinuationHeader)
    {
      // here we start a new header
      uint32_t colonPos = buf.FindChar(':');
      curMsgHeader = StringHead(buf, colonPos);
    }

    if (curMsgHeader.Equals(m_arbitraryHeader, nsCaseInsensitiveCStringComparator()))
    {
      // process the value
      // value occurs after the header name or whitespace continuation char.
      const char * headerValue = buf.get() + (isContinuationHeader ? 1 : headerLength);
      if (headerValue < buf_end && headerValue[0] == ':')  // + 1 to account for the colon which is MANDATORY
        headerValue++;

      // strip leading white space
      while (headerValue < buf_end && isspace(*headerValue))
        headerValue++; // advance to next character

      // strip trailing white space
      char * end = buf_end - 1;
      while (end > headerValue && isspace(*end)) // while we haven't gone back past the start and we are white space....
      {
        *end = '\0';  // eat up the white space
        end--;      // move back and examine the previous character....
      }

      // any continuation whitespace is converted to a single space. This includes both a continuation line, or a
      //  second value of the same header (eg the received header)
      if (!headerFullValue.IsEmpty())
        headerFullValue.AppendLiteral(" ");
      headerFullValue.Append(nsDependentCString(headerValue));
    }
  }
  delete bodyHandler;
  *pResult = result;
  return err;
}

NS_IMETHODIMP nsMsgSearchTerm::MatchHdrProperty(nsIMsgDBHdr *aHdr, bool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  NS_ENSURE_ARG_POINTER(aHdr);

  nsCString dbHdrValue;
  aHdr->GetStringProperty(m_hdrProperty.get(), getter_Copies(dbHdrValue));
  nsresult rv = MatchString(dbHdrValue.get(), nullptr, aResult);
  return rv;
}

NS_IMETHODIMP nsMsgSearchTerm::MatchFolderFlag(nsIMsgDBHdr *aMsgToMatch, bool *aResult)
{
  NS_ENSURE_ARG_POINTER(aMsgToMatch);
  NS_ENSURE_ARG_POINTER(aResult);
  nsCOMPtr<nsIMsgFolder> msgFolder;
  nsresult rv = aMsgToMatch->GetFolder(getter_AddRefs(msgFolder));
  NS_ENSURE_SUCCESS(rv, rv);
  uint32_t folderFlags;
  msgFolder->GetFlags(&folderFlags);
  return MatchStatus(folderFlags, aResult);
}

NS_IMETHODIMP nsMsgSearchTerm::MatchUint32HdrProperty(nsIMsgDBHdr *aHdr, bool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  NS_ENSURE_ARG_POINTER(aHdr);

  uint32_t dbHdrValue;
  aHdr->GetUint32Property(m_hdrProperty.get(), &dbHdrValue);

  bool result = false;
  switch (m_operator)
  {
  case nsMsgSearchOp::IsGreaterThan:
    if (dbHdrValue > m_value.u.msgStatus)
      result = true;
    break;
  case nsMsgSearchOp::IsLessThan:
    if (dbHdrValue < m_value.u.msgStatus)
      result = true;
    break;
  case nsMsgSearchOp::Is:
    if (dbHdrValue == m_value.u.msgStatus)
      result = true;
    break;
  case nsMsgSearchOp::Isnt:
    if (dbHdrValue != m_value.u.msgStatus)
      result = true;
    break;
  default:
    break;
  }
  *aResult = result;
  return NS_OK;
}

nsresult nsMsgSearchTerm::MatchBody (nsIMsgSearchScopeTerm *scope, uint64_t offset, uint32_t length /*in lines*/, const char *folderCharset,
                                      nsIMsgDBHdr *msg, nsIMsgDatabase* db, bool *pResult)
{
  NS_ENSURE_ARG_POINTER(pResult);
  nsresult err = NS_OK;

  bool result = false;
  *pResult = false;

  // Small hack so we don't look all through a message when someone has
  // specified "BODY IS foo". ### Since length is in lines, this is not quite right.
  if ((length > 0) && (m_operator == nsMsgSearchOp::Is || m_operator == nsMsgSearchOp::Isnt))
    length = PL_strlen (m_value.string);

  nsMsgBodyHandler * bodyHan  = new nsMsgBodyHandler (scope, length, msg, db);
  if (!bodyHan)
    return NS_ERROR_OUT_OF_MEMORY;

  nsCAutoString buf;
  bool endOfFile = false;  // if retValue == 0, we've hit the end of the file
  uint32 lines = 0;

  // Change the sense of the loop so we don't bail out prematurely
  // on negative terms. i.e. opDoesntContain must look at all lines
  bool boolContinueLoop;
  GetMatchAllBeforeDeciding(&boolContinueLoop);
  result = boolContinueLoop;

  // If there's a '=' in the search term, then we're not going to do
  // quoted printable decoding. Otherwise we assume everything is
  // quoted printable. Obviously everything isn't quoted printable, but
  // since we don't have a MIME parser handy, and we want to err on the
  // side of too many hits rather than not enough, we'll assume in that
  // general direction. Blech. ### FIX ME
  // bug fix #314637: for stateful charsets like ISO-2022-JP, we don't
  // want to decode quoted printable since it contains '='.
  bool isQuotedPrintable = !nsMsgI18Nstateful_charset(folderCharset) &&
    (PL_strchr (m_value.string, '=') == nullptr);

  nsCString compare;
  while (!endOfFile && result == boolContinueLoop)
  {
    if (bodyHan->GetNextLine(buf) >= 0)
    {
      bool softLineBreak = false;
      // Do in-place decoding of quoted printable
      if (isQuotedPrintable)
      {
        softLineBreak = StringEndsWith(buf, NS_LITERAL_CSTRING("="));
        MsgStripQuotedPrintable ((unsigned char*)buf.get());
        // in case the string shrunk, reset the length. If soft line break,
        // chop off the last char as well.
        size_t bufLength = strlen(buf.get());
        if ((bufLength > 0) && softLineBreak)
          --bufLength;
        buf.SetLength(bufLength);
      }
      compare.Append(buf);
      // If this line ends with a soft line break, loop around
      // and get the next line before looking for the search string.
      // This assumes the message can't end on a QP soft-line break.
      // That seems like a pretty safe assumption.
      if (softLineBreak)
        continue;
      if (!compare.IsEmpty())
      {
        char startChar = (char) compare.CharAt(0);
        if (startChar != '\r' && startChar != '\n')
        {
          err = MatchString (compare.get(), folderCharset, &result);
          lines++;
        }
        compare.Truncate();
      }
    }
    else
      endOfFile = true;
  }
#ifdef DO_I18N
  if(conv)
    INTL_DestroyCharCodeConverter(conv);
#endif
  delete bodyHan;
  *pResult = result;
  return err;
}

nsresult nsMsgSearchTerm::InitializeAddressBook()
{
  // the search attribute value has the URI for the address book we need to load.
  // we need both the database and the directory.
  nsresult rv = NS_OK;

  if (mDirectory)
  {
    nsCString uri;
    rv = mDirectory->GetURI(uri);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!uri.Equals(m_value.string))
      // clear out the directory....we are no longer pointing to the right one
      mDirectory = nullptr;
  }
  if (!mDirectory)
  {
    nsCOMPtr<nsIAbManager> abManager = do_GetService(NS_ABMANAGER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = abManager->GetDirectory(nsDependentCString(m_value.string), getter_AddRefs(mDirectory));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult nsMsgSearchTerm::MatchInAddressBook(const char * aAddress, bool *pResult)
{
  nsresult rv = InitializeAddressBook();
  *pResult = false;

  // Some junkmails have empty From: fields.
  if (aAddress == NULL || strlen(aAddress) == 0)
    return rv;

  if (mDirectory)
  {
    nsIAbCard* cardForAddress = nullptr;
    rv = mDirectory->CardForEmailAddress(nsDependentCString(aAddress),
                                         &cardForAddress);
    if (NS_FAILED(rv) && rv != NS_ERROR_NOT_IMPLEMENTED)
      return rv;
    if ((m_operator == nsMsgSearchOp::IsInAB && cardForAddress) || (m_operator == nsMsgSearchOp::IsntInAB && !cardForAddress))
      *pResult = true;
    NS_IF_RELEASE(cardForAddress);
  }

  return rv;
}

// *pResult is false when strings don't match, true if they do.
nsresult nsMsgSearchTerm::MatchRfc2047String (const char * rfc2047string,
                                       const char *charset,
                                       bool charsetOverride,
                                       bool *pResult)
{
  NS_ENSURE_ARG_POINTER(pResult);
  NS_ENSURE_ARG_POINTER(rfc2047string);

    nsCOMPtr<nsIMimeConverter> mimeConverter = do_GetService(NS_MIME_CONVERTER_CONTRACTID);
  char *stringToMatch = 0;
    nsresult res = mimeConverter->DecodeMimeHeaderToCharPtr(
        rfc2047string, charset, charsetOverride, false, &stringToMatch);

    if ( m_operator == nsMsgSearchOp::IsInAB ||
         m_operator == nsMsgSearchOp::IsntInAB)
    {
      res = MatchInAddressBook(stringToMatch ? stringToMatch : rfc2047string, pResult);
    }
    else
    res = MatchString(stringToMatch ? stringToMatch : rfc2047string,
                      nullptr, pResult);

    PR_Free(stringToMatch);

  return res;
}

// *pResult is false when strings don't match, true if they do.
nsresult nsMsgSearchTerm::MatchString (const char *stringToMatch,
                                       const char *charset,
                                       bool *pResult)
{
  NS_ENSURE_ARG_POINTER(pResult);
  bool result = false;

  nsresult err = NS_OK;
  nsAutoString utf16StrToMatch;
  nsAutoString needle;

  // Save some performance for opIsEmpty / opIsntEmpty
  if(nsMsgSearchOp::IsEmpty != m_operator && nsMsgSearchOp::IsntEmpty != m_operator)
  {
    NS_ASSERTION(MsgIsUTF8(nsDependentCString(m_value.string)),
                 "m_value.string is not UTF-8");
    CopyUTF8toUTF16(nsDependentCString(m_value.string), needle);

    if (charset != nullptr)
    {
      ConvertToUnicode(charset, stringToMatch ? stringToMatch : "",
                       utf16StrToMatch);
    }
    else {
      NS_ASSERTION(MsgIsUTF8(nsDependentCString(stringToMatch)),
                   "stringToMatch is not UTF-8");
      CopyUTF8toUTF16(nsDependentCString(stringToMatch), utf16StrToMatch);
    }
  }

  switch (m_operator)
  {
  case nsMsgSearchOp::Contains:
    if (CaseInsensitiveFindInReadable(needle, utf16StrToMatch))
      result = true;
    break;
  case nsMsgSearchOp::DoesntContain:
    if (!CaseInsensitiveFindInReadable(needle, utf16StrToMatch))
      result = true;
    break;
  case nsMsgSearchOp::Is:
    if(needle.Equals(utf16StrToMatch, nsCaseInsensitiveStringComparator()))
      result = true;
    break;
  case nsMsgSearchOp::Isnt:
    if(!needle.Equals(utf16StrToMatch, nsCaseInsensitiveStringComparator()))
      result = true;
    break;
  case nsMsgSearchOp::IsEmpty:
    // For IsEmpty, we didn't copy stringToMatch to utf16StrToMatch.
    if (!PL_strlen(stringToMatch))
      result = true;
    break;
  case nsMsgSearchOp::IsntEmpty:
    // For IsntEmpty, we didn't copy stringToMatch to utf16StrToMatch.
    if (PL_strlen(stringToMatch))
      result = true;
    break;
  case nsMsgSearchOp::BeginsWith:
    if (StringBeginsWith(utf16StrToMatch, needle,
                         nsCaseInsensitiveStringComparator()))
      result = true;
    break;
  case nsMsgSearchOp::EndsWith:
    if (StringEndsWith(utf16StrToMatch, needle,
                       nsCaseInsensitiveStringComparator()))
      result = true;
    break;
  default:
    NS_ASSERTION(false, "invalid operator matching search results");
  }

  *pResult = result;
  return err;
}

NS_IMETHODIMP nsMsgSearchTerm::GetMatchAllBeforeDeciding (bool *aResult)
{
 *aResult = (m_operator == nsMsgSearchOp::DoesntContain || m_operator == nsMsgSearchOp::Isnt);
 return NS_OK;
}

 nsresult nsMsgSearchTerm::MatchRfc822String (const char *string, const char *charset, bool charsetOverride, bool *pResult)
 {
   NS_ENSURE_ARG_POINTER(pResult);
   *pResult = false;
   bool result;
   nsresult err = InitHeaderAddressParser();
   if (NS_FAILED(err))
     return err;
   // Isolate the RFC 822 parsing weirdnesses here. MSG_ParseRFC822Addresses
   // returns a catenated string of null-terminated strings, which we walk
   // across, tring to match the target string to either the name OR the address

   char *names = nullptr, *addresses = nullptr;

   // Change the sense of the loop so we don't bail out prematurely
   // on negative terms. i.e. opDoesntContain must look at all recipients
   bool boolContinueLoop;
   GetMatchAllBeforeDeciding(&boolContinueLoop);
   result = boolContinueLoop;

   uint32_t count;
   nsresult parseErr = m_headerAddressParser->ParseHeaderAddresses(string,
                                                                   &names,
                                                                   &addresses,
                                                                   &count);

   if (NS_SUCCEEDED(parseErr) && count > 0)
   {
     NS_ASSERTION(names, "couldn't get names");
     NS_ASSERTION(addresses, "couldn't get addresses");
     if (!names || !addresses)
       return err;

     nsCAutoString walkNames;
     nsCAutoString walkAddresses;
     int32_t namePos = 0;
     int32_t addressPos = 0;
     for (uint32_t i = 0; i < count && result == boolContinueLoop; i++)
     {
       walkNames = names + namePos;
       walkAddresses = addresses + addressPos;
       if ( m_operator == nsMsgSearchOp::IsInAB ||
            m_operator == nsMsgSearchOp::IsntInAB)
       {
         err = MatchRfc2047String (walkAddresses.get(), charset, charsetOverride, &result);
       }
       else
       {
         err = MatchRfc2047String (walkNames.get(), charset, charsetOverride, &result);
         if (boolContinueLoop == result)
           err = MatchRfc2047String (walkAddresses.get(), charset, charsetOverride, &result);
       }

       namePos += walkNames.Length() + 1;
       addressPos += walkAddresses.Length() + 1;
     }

     PR_Free(names);
     PR_Free(addresses);
   }
   *pResult = result;
   return err;
 }


nsresult nsMsgSearchTerm::GetLocalTimes (PRTime a, PRTime b, PRExplodedTime &aExploded, PRExplodedTime &bExploded)
{
  PR_ExplodeTime(a, PR_LocalTimeParameters, &aExploded);
  PR_ExplodeTime(b, PR_LocalTimeParameters, &bExploded);
  return NS_OK;
}


nsresult nsMsgSearchTerm::MatchDate (PRTime dateToMatch, bool *pResult)
{
  NS_ENSURE_ARG_POINTER(pResult);

  nsresult err = NS_OK;
  bool result = false;

  switch (m_operator)
  {
  case nsMsgSearchOp::IsBefore:
    if (dateToMatch < m_value.u.date)
      result = true;
    break;
  case nsMsgSearchOp::IsAfter:
    {
      PRTime adjustedDate = m_value.u.date;
      adjustedDate += 60*60*24; // we want to be greater than the next day....
      if (dateToMatch > adjustedDate)
        result = true;
    }
    break;
  case nsMsgSearchOp::Is:
    {
      PRExplodedTime tmToMatch, tmThis;
      if (NS_OK == GetLocalTimes (dateToMatch, m_value.u.date, tmToMatch, tmThis))
      {
        if (tmThis.tm_year == tmToMatch.tm_year &&
          tmThis.tm_month == tmToMatch.tm_month &&
          tmThis.tm_mday == tmToMatch.tm_mday)
          result = true;
      }
    }
    break;
  case nsMsgSearchOp::Isnt:
    {
      PRExplodedTime tmToMatch, tmThis;
      if (NS_OK == GetLocalTimes (dateToMatch, m_value.u.date, tmToMatch, tmThis))
      {
        if (tmThis.tm_year != tmToMatch.tm_year ||
          tmThis.tm_month != tmToMatch.tm_month ||
          tmThis.tm_mday != tmToMatch.tm_mday)
          result = true;
      }
    }
    break;
  default:
    NS_ASSERTION(false, "invalid compare op for dates");
  }
  *pResult = result;
  return err;
}


nsresult nsMsgSearchTerm::MatchAge (PRTime msgDate, bool *pResult)
{
  NS_ENSURE_ARG_POINTER(pResult);

  bool result = false;
  nsresult err = NS_OK;

  PRTime now = PR_Now();
  PRTime cutOffDay = now - m_value.u.age * PR_USEC_PER_DAY;

  bool cutOffDayInTheFuture = m_value.u.age < 0;

  // So now cutOffDay is the PRTime cut-off point.
  // Any msg with a time less than that will be past the age.

  switch (m_operator)
  {
    case nsMsgSearchOp::IsGreaterThan: // is older than, or more in the future
      if ((!cutOffDayInTheFuture && msgDate < cutOffDay) ||
          (cutOffDayInTheFuture && msgDate > cutOffDay))
        result = true;
      break;
    case nsMsgSearchOp::IsLessThan: // is younger than, or less in the future
      if ((!cutOffDayInTheFuture && msgDate > cutOffDay) ||
          (cutOffDayInTheFuture && msgDate < cutOffDay))
        result = true;
      break;
    case nsMsgSearchOp::Is:
      PRExplodedTime msgDateExploded;
      PRExplodedTime cutOffDayExploded;
      if (NS_SUCCEEDED(GetLocalTimes(msgDate, cutOffDay, msgDateExploded, cutOffDayExploded)))
      {
        if ((msgDateExploded.tm_mday == cutOffDayExploded.tm_mday) &&
            (msgDateExploded.tm_month == cutOffDayExploded.tm_month) &&
            (msgDateExploded.tm_year == cutOffDayExploded.tm_year))
          result = true;
      }
      break;
    default:
      NS_ASSERTION(false, "invalid compare op for msg age");
  }
  *pResult = result;
  return err;
}


nsresult nsMsgSearchTerm::MatchSize (uint32_t sizeToMatch, bool *pResult)
{
  NS_ENSURE_ARG_POINTER(pResult);

  bool result = false;
  // We reduce the sizeToMatch rather than supplied size
  // as then we can do an exact match on the displayed value
  // which will be less confusing to the user.
  uint32_t sizeToMatchKB = sizeToMatch;

  if (sizeToMatchKB < 1024)
    sizeToMatchKB = 1024;

  sizeToMatchKB /= 1024;

  switch (m_operator)
  {
  case nsMsgSearchOp::IsGreaterThan:
    if (sizeToMatchKB > m_value.u.size)
      result = true;
    break;
  case nsMsgSearchOp::IsLessThan:
    if (sizeToMatchKB < m_value.u.size)
      result = true;
    break;
  case nsMsgSearchOp::Is:
    if (sizeToMatchKB == m_value.u.size)
      result = true;
    break;
  default:
    break;
  }
  *pResult = result;
  return NS_OK;
}

nsresult nsMsgSearchTerm::MatchJunkStatus(const char *aJunkScore, bool *pResult)
{
  NS_ENSURE_ARG_POINTER(pResult);

  if (m_operator == nsMsgSearchOp::IsEmpty)
  {
    *pResult = !(aJunkScore && *aJunkScore);
    return NS_OK;
  }
  else if (m_operator == nsMsgSearchOp::IsntEmpty)
  {
    *pResult = (aJunkScore && *aJunkScore);
    return NS_OK;
  }

  nsMsgJunkStatus junkStatus;
  if (aJunkScore && *aJunkScore) {
      junkStatus = (atoi(aJunkScore) == nsIJunkMailPlugin::IS_SPAM_SCORE) ?
          nsIJunkMailPlugin::JUNK :
          nsIJunkMailPlugin::GOOD;

  }
  else {
    // the in UI, we only show "junk" or "not junk"
    // unknown, or nsIJunkMailPlugin::UNCLASSIFIED is shown as not junk
    // so for the search to work as expected, treat unknown as not junk
    junkStatus = nsIJunkMailPlugin::GOOD;
  }

  nsresult rv = NS_OK;
  bool matches = (junkStatus == m_value.u.junkStatus);

  switch (m_operator)
  {
    case nsMsgSearchOp::Is:
      break;
    case nsMsgSearchOp::Isnt:
      matches = !matches;
      break;
    default:
      rv = NS_ERROR_FAILURE;
      NS_ASSERTION(false, "invalid compare op for junk status");
  }

  *pResult = matches;
  return rv;
}

nsresult nsMsgSearchTerm::MatchJunkScoreOrigin(const char *aJunkScoreOrigin, bool *pResult)
{
  NS_ENSURE_ARG_POINTER(pResult);
  bool matches = false;
  nsresult rv = NS_OK;

  switch (m_operator)
  {
  case nsMsgSearchOp::Is:
    matches = aJunkScoreOrigin && !strcmp(aJunkScoreOrigin, m_value.string);
    break;
  case nsMsgSearchOp::Isnt:
    matches = !aJunkScoreOrigin || strcmp(aJunkScoreOrigin, m_value.string);
    break;
  default:
    rv = NS_ERROR_FAILURE;
    NS_ASSERTION(false, "invalid compare op for junk score origin");
  }

  *pResult = matches;
  return rv;
}

nsresult nsMsgSearchTerm::MatchJunkPercent(uint32_t aJunkPercent, bool *pResult)
{
  NS_ENSURE_ARG_POINTER(pResult);
  bool result = false;
  switch (m_operator)
  {
  case nsMsgSearchOp::IsGreaterThan:
    if (aJunkPercent > m_value.u.junkPercent)
      result = true;
    break;
  case nsMsgSearchOp::IsLessThan:
    if (aJunkPercent < m_value.u.junkPercent)
      result = true;
    break;
  case nsMsgSearchOp::Is:
    if (aJunkPercent == m_value.u.junkPercent)
      result = true;
    break;
  default:
    break;
  }
  *pResult = result;
  return NS_OK;
}
  

nsresult nsMsgSearchTerm::MatchLabel(nsMsgLabelValue aLabelValue, bool *pResult)
{
  NS_ENSURE_ARG_POINTER(pResult);
  bool result = false;
  switch (m_operator)
  {
  case nsMsgSearchOp::Is:
    if (m_value.u.label == aLabelValue)
      result = true;
    break;
  default:
    if (m_value.u.label != aLabelValue)
      result = true;
    break;
  }

  *pResult = result;
  return NS_OK;
}

nsresult nsMsgSearchTerm::MatchStatus(uint32_t statusToMatch, bool *pResult)
{
  NS_ENSURE_ARG_POINTER(pResult);

  nsresult rv = NS_OK;
  bool matches = (statusToMatch & m_value.u.msgStatus);

  switch (m_operator)
  {
  case nsMsgSearchOp::Is:
    break;
  case nsMsgSearchOp::Isnt:
    matches = !matches;
    break;
  default:
    rv = NS_ERROR_FAILURE;
    NS_ERROR("invalid compare op for msg status");
  }

  *pResult = matches;
  return rv;
}

/*
 * MatchKeyword Logic table (*pResult: + is true, - is false)
 *
 *         # Valid Tokens IsEmpty IsntEmpty Contains DoesntContain Is     Isnt
 *                0           +         -      -         +         -       +
 * Term found?                               N   Y     N   Y     N   Y   N   Y
 *                1           -         +    -   +     +   -     -   +   +   -
 *               >1           -         +    -   +     +   -     -   -   +   +
 */
// look up nsMsgSearchTerm::m_value in space-delimited keywordList
nsresult nsMsgSearchTerm::MatchKeyword(const nsACString& keywordList, bool *pResult)
{
  NS_ENSURE_ARG_POINTER(pResult);
  bool matches = false;

  // special-case empty for performance reasons
  if (keywordList.IsEmpty())
  {
    *pResult =  m_operator != nsMsgSearchOp::Contains &&
                m_operator != nsMsgSearchOp::Is &&
                m_operator != nsMsgSearchOp::IsntEmpty;
    return NS_OK;
  }

  // check if we can skip expensive valid keywordList test
  if (m_operator == nsMsgSearchOp::DoesntContain ||
      m_operator == nsMsgSearchOp::Contains)
  {
    nsCString keywordString(keywordList);
    const uint32_t kKeywordLen = PL_strlen(m_value.string);
    const char* matchStart = PL_strstr(keywordString.get(), m_value.string);
    while (matchStart)
    {
      // For a real match, matchStart must be the start of the keywordList or
      // preceded by a space and matchEnd must point to a \0 or space.
      const char* matchEnd = matchStart + kKeywordLen;
      if ((matchStart == keywordString.get() || matchStart[-1] == ' ') &&
          (!*matchEnd || *matchEnd == ' '))
      {
        // found the keyword
        *pResult = m_operator == nsMsgSearchOp::Contains;
        return NS_OK;
      }
      // no match yet, so search on
      matchStart = PL_strstr(matchEnd, m_value.string);
    }
    // keyword not found
    *pResult = m_operator == nsMsgSearchOp::DoesntContain;
    return NS_OK;
  }

  // Only accept valid keys in tokens.
  nsresult rv = NS_OK;
  nsTArray<nsCString> keywordArray;
  ParseString(keywordList, ' ', keywordArray);
  nsCOMPtr<nsIMsgTagService> tagService(do_GetService(NS_MSGTAGSERVICE_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  // Loop through tokens in keywords
  uint32_t count = keywordArray.Length();
  for (uint32_t i = 0; i < count; i++)
  {
    // is this token a valid tag? Otherwise ignore it
    bool isValid;
    rv = tagService->IsValidKey(keywordArray[i], &isValid);
    NS_ENSURE_SUCCESS(rv, rv);

    if (isValid)
    {
      // IsEmpty fails on any valid token
      if (m_operator == nsMsgSearchOp::IsEmpty)
      {
        *pResult = false;
        return rv;
      }

      // IsntEmpty succeeds on any valid token
      if (m_operator == nsMsgSearchOp::IsntEmpty)
      {
        *pResult = true;
        return rv;
      }

      // Does this valid tag key match our search term?
      matches = keywordArray[i].Equals(m_value.string);

      // Is or Isn't partly determined on a single unmatched token
      if (!matches)
      {
        if (m_operator == nsMsgSearchOp::Is)
        {
          *pResult = false;
          return rv;
        }
        if (m_operator == nsMsgSearchOp::Isnt)
        {
          *pResult = true;
          return rv;
        }
      }
    }
  }

  if (m_operator == nsMsgSearchOp::Is)
  {
    *pResult = matches;
    return NS_OK;
  }

  if (m_operator == nsMsgSearchOp::Isnt)
  {
    *pResult = !matches;
    return NS_OK;
  }

  if (m_operator == nsMsgSearchOp::IsEmpty)
  {
    *pResult = true;
    return NS_OK;
  }

  if (m_operator == nsMsgSearchOp::IsntEmpty)
  {
    *pResult = false;
    return NS_OK;
  }


  // no valid match operator found
  NS_ERROR("invalid compare op for msg status");
  return NS_ERROR_FAILURE;
}

nsresult
nsMsgSearchTerm::MatchPriority (nsMsgPriorityValue priorityToMatch,
                                bool *pResult)
{
  NS_ENSURE_ARG_POINTER(pResult);

  nsresult err = NS_OK;
  bool result=NS_OK;

  // Use this ugly little hack to get around the fact that enums don't have
  // integer compare operators
  int p1 = (priorityToMatch == nsMsgPriority::none) ? (int) nsMsgPriority::normal : (int) priorityToMatch;
  int p2 = (int) m_value.u.priority;

  switch (m_operator)
  {
  case nsMsgSearchOp::IsHigherThan:
    if (p1 > p2)
      result = true;
    break;
  case nsMsgSearchOp::IsLowerThan:
    if (p1 < p2)
      result = true;
    break;
  case nsMsgSearchOp::Is:
    if (p1 == p2)
      result = true;
    break;
  default:
    result = false;
    err = NS_ERROR_FAILURE;
    NS_ASSERTION(false, "invalid match operator");
  }
  *pResult = result;
  return err;
}

// match a custom search term
NS_IMETHODIMP nsMsgSearchTerm::MatchCustom(nsIMsgDBHdr* aHdr, bool *pResult)
{
  NS_ENSURE_ARG_POINTER(pResult);
  nsresult rv;
  nsCOMPtr<nsIMsgFilterService> filterService =
      do_GetService(NS_MSGFILTERSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIMsgSearchCustomTerm> customTerm;
  rv = filterService->GetCustomTerm(m_customId, getter_AddRefs(customTerm));
  NS_ENSURE_SUCCESS(rv, rv);

  if (customTerm)
    return customTerm->Match(aHdr, nsDependentCString(m_value.string),
                             m_operator, pResult);
  *pResult = false;     // default to no match if term is missing
  return NS_ERROR_FAILURE; // missing custom term
}

// set the id of a custom search term
NS_IMETHODIMP nsMsgSearchTerm::SetCustomId(const nsACString &aId)
{
  m_customId = aId;
  return NS_OK;
}

// get the id of a custom search term
NS_IMETHODIMP nsMsgSearchTerm::GetCustomId(nsACString &aResult)
{
  aResult = m_customId;
  return NS_OK;
}

// Lazily initialize the rfc822 header parser we're going to use to do
// header matching.
nsresult nsMsgSearchTerm::InitHeaderAddressParser()
{
  nsresult res = NS_OK;

  if (!m_headerAddressParser)
  {
    m_headerAddressParser = do_GetService(NS_MAILNEWS_MIME_HEADER_PARSER_CONTRACTID, &res);
  }
  return res;
}

NS_IMPL_GETSET(nsMsgSearchTerm, Attrib, nsMsgSearchAttribValue, m_attribute)
NS_IMPL_GETSET(nsMsgSearchTerm, Op, nsMsgSearchOpValue, m_operator)
NS_IMPL_GETSET(nsMsgSearchTerm, MatchAll, bool, m_matchAll)

NS_IMETHODIMP
nsMsgSearchTerm::GetValue(nsIMsgSearchValue **aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    *aResult = new nsMsgSearchValueImpl(&m_value);
    NS_IF_ADDREF(*aResult);
    return NS_OK;
}

NS_IMETHODIMP
nsMsgSearchTerm::SetValue(nsIMsgSearchValue* aValue)
{
  nsMsgResultElement::AssignValues (aValue, &m_value);
  return NS_OK;
}

NS_IMETHODIMP
nsMsgSearchTerm::GetBooleanAnd(bool *aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    *aResult = (m_booleanOp == nsMsgSearchBooleanOp::BooleanAND);
    return NS_OK;
}

NS_IMETHODIMP
nsMsgSearchTerm::SetBooleanAnd(bool aValue)
{
    if (aValue)
        m_booleanOp = nsMsgSearchBooleanOperator(nsMsgSearchBooleanOp::BooleanAND);
    else
        m_booleanOp = nsMsgSearchBooleanOperator(nsMsgSearchBooleanOp::BooleanOR);
    return NS_OK;
}

NS_IMETHODIMP
nsMsgSearchTerm::GetArbitraryHeader(nsACString &aResult)
{
    aResult = m_arbitraryHeader;
    return NS_OK;
}

NS_IMETHODIMP
nsMsgSearchTerm::SetArbitraryHeader(const nsACString &aValue)
{
    m_arbitraryHeader = aValue;
    ToLowerCaseExceptSpecials(m_arbitraryHeader);
    return NS_OK;
}

NS_IMETHODIMP
nsMsgSearchTerm::GetHdrProperty(nsACString &aResult)
{
  aResult = m_hdrProperty;
  return NS_OK;
}

NS_IMETHODIMP
nsMsgSearchTerm::SetHdrProperty(const nsACString &aValue)
{
  m_hdrProperty = aValue;
  ToLowerCaseExceptSpecials(m_hdrProperty);
  return NS_OK;
}

NS_IMPL_GETSET(nsMsgSearchTerm, BeginsGrouping, bool, mBeginsGrouping)
NS_IMPL_GETSET(nsMsgSearchTerm, EndsGrouping, bool, mEndsGrouping)

//
// Certain possible standard values of a message database row also sometimes
// appear as header values. To prevent a naming collision, we use all
// lower case for the standard headers, and first capital when those
// same strings are requested as arbitrary headers. This routine is used
// when setting arbitrary headers.
//
void nsMsgSearchTerm::ToLowerCaseExceptSpecials(nsACString &aValue)
{
  if (aValue.LowerCaseEqualsLiteral("sender"))
    aValue.Assign(NS_LITERAL_CSTRING("Sender"));
  else if (aValue.LowerCaseEqualsLiteral("date"))
    aValue.Assign(NS_LITERAL_CSTRING("Date"));
  else if (aValue.LowerCaseEqualsLiteral("status"))
    aValue.Assign(NS_LITERAL_CSTRING("Status"));
  else
    ToLowerCase(aValue);
}


//-----------------------------------------------------------------------------
// nsMsgSearchScopeTerm implementation
//-----------------------------------------------------------------------------
nsMsgSearchScopeTerm::nsMsgSearchScopeTerm (nsIMsgSearchSession *session,
                                            nsMsgSearchScopeValue attribute,
                                            nsIMsgFolder *folder)
{
  m_attribute = attribute;
  m_folder = folder;
  m_searchServer = true;
  m_searchSession = do_GetWeakReference(session);
}

nsMsgSearchScopeTerm::nsMsgSearchScopeTerm ()
{
  m_searchServer = true;
}

nsMsgSearchScopeTerm::~nsMsgSearchScopeTerm ()
{
  if (m_inputStream)
    m_inputStream->Close();
  m_inputStream = nullptr;
}

NS_IMPL_ISUPPORTS1(nsMsgSearchScopeTerm, nsIMsgSearchScopeTerm)

NS_IMETHODIMP
nsMsgSearchScopeTerm::GetFolder(nsIMsgFolder **aResult)
{
    NS_IF_ADDREF(*aResult = m_folder);
    return NS_OK;
}

NS_IMETHODIMP
nsMsgSearchScopeTerm::GetSearchSession(nsIMsgSearchSession** aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    nsCOMPtr<nsIMsgSearchSession> searchSession = do_QueryReferent (m_searchSession);
    NS_IF_ADDREF(*aResult = searchSession);
    return NS_OK;
}

NS_IMETHODIMP
nsMsgSearchScopeTerm::GetInputStream(nsIMsgDBHdr *aMsgHdr,
                                     nsIInputStream **aInputStream)
{
  NS_ENSURE_ARG_POINTER(aInputStream);
  NS_ENSURE_ARG_POINTER(aMsgHdr);
  NS_ENSURE_TRUE(m_folder, NS_ERROR_NULL_POINTER);
  bool reusable;
  nsresult rv = m_folder->GetMsgInputStream(aMsgHdr, &reusable,
                                            getter_AddRefs(m_inputStream));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_IF_ADDREF(*aInputStream = m_inputStream);
  return rv;
}

NS_IMETHODIMP nsMsgSearchScopeTerm::CloseInputStream()
{
  if (m_inputStream)
{
    m_inputStream->Close();
    m_inputStream = nullptr;
  }
  return NS_OK;
}

nsresult nsMsgSearchScopeTerm::TimeSlice (bool *aDone)
{
  return m_adapter->Search(aDone);
}

nsresult nsMsgSearchScopeTerm::InitializeAdapter (nsISupportsArray *termList)
{
  if (m_adapter)
    return NS_OK;

  nsresult err = NS_OK;

  switch (m_attribute)
  {
    case nsMsgSearchScope::onlineMail:
        m_adapter = new nsMsgSearchOnlineMail (this, termList);
      break;
    case nsMsgSearchScope::offlineMail:
    case nsMsgSearchScope::onlineManual:
        m_adapter = new nsMsgSearchOfflineMail (this, termList);
      break;
    case nsMsgSearchScope::newsEx:
      NS_ASSERTION(false, "not supporting newsEx yet");
      break;
    case nsMsgSearchScope::news:
          m_adapter = new nsMsgSearchNews (this, termList);
        break;
    case nsMsgSearchScope::allSearchableGroups:
      NS_ASSERTION(false, "not supporting allSearchableGroups yet");
      break;
    case nsMsgSearchScope::LDAP:
      NS_ASSERTION(false, "not supporting LDAP yet");
      break;
    case nsMsgSearchScope::localNews:
    case nsMsgSearchScope::localNewsJunk:
    case nsMsgSearchScope::localNewsBody:
    case nsMsgSearchScope::localNewsJunkBody:
      m_adapter = new nsMsgSearchOfflineNews (this, termList);
      break;
    default:
      NS_ASSERTION(false, "invalid scope");
      err = NS_ERROR_FAILURE;
  }

  if (m_adapter)
    err = m_adapter->ValidateTerms ();

  return err;
}


char *nsMsgSearchScopeTerm::GetStatusBarName ()
{
  return nullptr;
}


//-----------------------------------------------------------------------------
// nsMsgResultElement implementation
//-----------------------------------------------------------------------------


nsMsgResultElement::nsMsgResultElement(nsIMsgSearchAdapter *adapter)
{
  NS_NewISupportsArray(getter_AddRefs(m_valueList));
  m_adapter = adapter;
}


nsMsgResultElement::~nsMsgResultElement ()
{
}


nsresult nsMsgResultElement::AddValue (nsIMsgSearchValue *value)
{
  m_valueList->AppendElement (value);
  return NS_OK;
}

nsresult nsMsgResultElement::AddValue (nsMsgSearchValue *value)
{
  nsMsgSearchValueImpl* valueImpl = new nsMsgSearchValueImpl(value);
  delete value;               // we keep the nsIMsgSearchValue, not
                              // the nsMsgSearchValue
  return AddValue(valueImpl);
}


nsresult nsMsgResultElement::AssignValues (nsIMsgSearchValue *src, nsMsgSearchValue *dst)
{
  NS_ENSURE_ARG_POINTER(src);
  NS_ENSURE_ARG_POINTER(dst);

  // Yes, this could be an operator overload, but nsMsgSearchValue is totally public, so I'd
  // have to define a derived class with nothing by operator=, and that seems like a bit much
  nsresult err = NS_OK;
  src->GetAttrib(&dst->attribute);
  switch (dst->attribute)
  {
  case nsMsgSearchAttrib::Priority:
    err = src->GetPriority(&dst->u.priority);
    break;
  case nsMsgSearchAttrib::Date:
    err = src->GetDate(&dst->u.date);
    break;
  case nsMsgSearchAttrib::HasAttachmentStatus:
  case nsMsgSearchAttrib::MsgStatus:
  case nsMsgSearchAttrib::FolderFlag:
  case nsMsgSearchAttrib::Uint32HdrProperty:
    err = src->GetStatus(&dst->u.msgStatus);
    break;
  case nsMsgSearchAttrib::MessageKey:
    err = src->GetMsgKey(&dst->u.key);
    break;
  case nsMsgSearchAttrib::AgeInDays:
    err = src->GetAge(&dst->u.age);
    break;
  case nsMsgSearchAttrib::Label:
    err = src->GetLabel(&dst->u.label);
    break;
  case nsMsgSearchAttrib::JunkStatus:
    err = src->GetJunkStatus(&dst->u.junkStatus);
    break;
  case nsMsgSearchAttrib::JunkPercent:
    err = src->GetJunkPercent(&dst->u.junkPercent);
    break;
  case nsMsgSearchAttrib::Size:
    err = src->GetSize(&dst->u.size);
    break;
  default:
    if (dst->attribute < nsMsgSearchAttrib::kNumMsgSearchAttributes)
    {
      NS_ASSERTION(IS_STRING_ATTRIBUTE(dst->attribute), "assigning non-string result");
      nsString unicodeString;
      err = src->GetStr(unicodeString);
      dst->string = ToNewUTF8String(unicodeString);
    }
    else
      err = NS_ERROR_INVALID_ARG;
  }
  return err;
}


nsresult nsMsgResultElement::GetValue (nsMsgSearchAttribValue attrib,
                                       nsMsgSearchValue **outValue) const
{
  nsresult err = NS_OK;
  nsCOMPtr<nsIMsgSearchValue> value;
  *outValue = NULL;

  uint32_t count;
  m_valueList->Count(&count);
  for (uint32_t i = 0; i < count && err != NS_OK; i++)
  {
    m_valueList->QueryElementAt(i, NS_GET_IID(nsIMsgSearchValue),
      (void **)getter_AddRefs(value));

    nsMsgSearchAttribValue valueAttribute;
    value->GetAttrib(&valueAttribute);
    if (attrib == valueAttribute)
    {
      *outValue = new nsMsgSearchValue;
      if (*outValue)
      {
        err = AssignValues (value, *outValue);
        err = NS_OK;
      }
      else
        err = NS_ERROR_OUT_OF_MEMORY;
    }
  }
  return err;
}

nsresult nsMsgResultElement::GetPrettyName (nsMsgSearchValue **value)
{
  return GetValue (nsMsgSearchAttrib::Location, value);
}

nsresult nsMsgResultElement::Open (void *window)
{
  return NS_ERROR_NULL_POINTER;
}


