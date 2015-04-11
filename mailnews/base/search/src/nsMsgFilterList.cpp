/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// this file implements the nsMsgFilterList interface

#include "nsTextFormatter.h"

#include "msgCore.h"
#include "nsMsgFilterList.h"
#include "nsMsgFilter.h"
#include "nsIMsgFilterHitNotify.h"
#include "nsMsgUtils.h"
#include "nsMsgSearchTerm.h"
#include "nsStringGlue.h"
#include "nsMsgBaseCID.h"
#include "nsIMsgFilterService.h"
#include "nsMsgSearchScopeTerm.h"
#include "nsISupportsObsolete.h"
#include "nsNetUtil.h"
#include "nsMsgI18N.h"
#include "nsMemory.h"
#include "prmem.h"
#include <ctype.h>

// unicode "%s" format string
static const PRUnichar unicodeFormatter[] = {
    (PRUnichar)'%',
    (PRUnichar)'s',
    (PRUnichar)0,
};


nsMsgFilterList::nsMsgFilterList() :
    m_fileVersion(0)
{
  m_loggingEnabled = false;
  m_startWritingToBuffer = false;
  m_temporaryList = false;
  m_curFilter = nullptr;
}

NS_IMPL_ADDREF(nsMsgFilterList)
NS_IMPL_RELEASE(nsMsgFilterList)
NS_IMPL_QUERY_INTERFACE1(nsMsgFilterList, nsIMsgFilterList)

NS_IMETHODIMP nsMsgFilterList::CreateFilter(const nsAString &name,class nsIMsgFilter **aFilter)
{
  NS_ENSURE_ARG_POINTER(aFilter);

  nsMsgFilter *filter = new nsMsgFilter;
  NS_ENSURE_TRUE(filter, NS_ERROR_OUT_OF_MEMORY);

  NS_ADDREF(*aFilter = filter);

  filter->SetFilterName(name);
  filter->SetFilterList(this);

  return NS_OK;
}

NS_IMPL_GETSET(nsMsgFilterList, LoggingEnabled, bool, m_loggingEnabled)

NS_IMETHODIMP nsMsgFilterList::GetFolder(nsIMsgFolder **aFolder)
{
  NS_ENSURE_ARG_POINTER(aFolder);

  *aFolder = m_folder;
  NS_IF_ADDREF(*aFolder);
  return NS_OK;
}

NS_IMETHODIMP nsMsgFilterList::SetFolder(nsIMsgFolder *aFolder)
{
  m_folder = aFolder;
  return NS_OK;
}

NS_IMETHODIMP nsMsgFilterList::SaveToFile(nsIOutputStream *stream)
{
  if (!stream)
    return NS_ERROR_NULL_POINTER;
  return SaveTextFilters(stream);
}

NS_IMETHODIMP nsMsgFilterList::EnsureLogFile()
{
  nsCOMPtr <nsIFile> file;
  nsresult rv = GetLogFile(getter_AddRefs(file));
  NS_ENSURE_SUCCESS(rv,rv);

  bool exists;
  rv = file->Exists(&exists);
  if (NS_SUCCEEDED(rv) && !exists) {
    rv = file->Create(nsIFile::NORMAL_FILE_TYPE, 0644);
    NS_ENSURE_SUCCESS(rv,rv);
  }
  return NS_OK;
}

nsresult nsMsgFilterList::TruncateLog()
{
  // this will flush and close the steam
  nsresult rv = SetLogStream(nullptr);
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr <nsIFile> file;
  rv = GetLogFile(getter_AddRefs(file));
  NS_ENSURE_SUCCESS(rv,rv);

  file->Remove(false);
  rv = file->Create(nsIFile::NORMAL_FILE_TYPE, 0644);
  NS_ENSURE_SUCCESS(rv,rv);
  return rv;
}

NS_IMETHODIMP nsMsgFilterList::ClearLog()
{
  bool loggingEnabled = m_loggingEnabled;

  // disable logging while clearing
  m_loggingEnabled = false;

#ifdef DEBUG
  nsresult rv =
#endif
    TruncateLog();
  NS_ASSERTION(NS_SUCCEEDED(rv), "failed to truncate filter log");

  m_loggingEnabled = loggingEnabled;

  return NS_OK;
}

nsresult
nsMsgFilterList::GetLogFile(nsIFile **aFile)
{
  NS_ENSURE_ARG_POINTER(aFile);

  // XXX todo
  // the path to the log file won't change
  // should we cache it?
  nsCOMPtr <nsIMsgFolder> folder;
  nsresult rv = GetFolder(getter_AddRefs(folder));
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr <nsIMsgIncomingServer> server;
  rv = folder->GetServer(getter_AddRefs(server));
  NS_ENSURE_SUCCESS(rv,rv);

  nsCString type;
  rv = server->GetType(type);
  NS_ENSURE_SUCCESS(rv,rv);

  bool isServer = false;
  rv = folder->GetIsServer(&isServer);
  NS_ENSURE_SUCCESS(rv,rv);

  // for news folders (not servers), the filter file is
  // mcom.test.dat
  // where the summary file is
  // mcom.test.msf
  // since the log is an html file we make it
  // mcom.test.htm
  if (type.Equals("nntp") && !isServer)
  {
    nsCOMPtr<nsIFile> thisFolder;
    rv = m_folder->GetFilePath(getter_AddRefs(thisFolder));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIFile> filterLogFile = do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = filterLogFile->InitWithFile(thisFolder);
    NS_ENSURE_SUCCESS(rv, rv);

    // NOTE:
    // we don't we need to call NS_MsgHashIfNecessary()
    // it's already been hashed, if necessary
    nsAutoString filterLogName;
    rv = filterLogFile->GetLeafName(filterLogName);
    NS_ENSURE_SUCCESS(rv,rv);

    filterLogName.Append(NS_LITERAL_STRING(".htm"));

    rv = filterLogFile->SetLeafName(filterLogName);
    NS_ENSURE_SUCCESS(rv,rv);

    NS_IF_ADDREF(*aFile = filterLogFile);
  }
  else {
    rv = server->GetLocalPath(aFile);
    NS_ENSURE_SUCCESS(rv,rv);

    rv = (*aFile)->AppendNative(NS_LITERAL_CSTRING("filterlog.html"));
    NS_ENSURE_SUCCESS(rv,rv);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsMsgFilterList::GetLogURL(nsACString &aLogURL)
{
  nsCOMPtr <nsIFile> file;
  nsresult rv = GetLogFile(getter_AddRefs(file));
  NS_ENSURE_SUCCESS(rv,rv);

  rv = NS_GetURLSpecFromFile(file, aLogURL);
  NS_ENSURE_SUCCESS(rv,rv);

  return !aLogURL.IsEmpty() ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsMsgFilterList::SetLogStream(nsIOutputStream *aLogStream)
{
  // if there is a log stream already, close it
  if (m_logStream) {
    // will flush
    nsresult rv = m_logStream->Close();
    NS_ENSURE_SUCCESS(rv,rv);
  }

  m_logStream = aLogStream;
  return NS_OK;
}

#define LOG_HEADER "<head><meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\"></head>"
#define LOG_HEADER_LEN (strlen(LOG_HEADER))

NS_IMETHODIMP
nsMsgFilterList::GetLogStream(nsIOutputStream **aLogStream)
{
  NS_ENSURE_ARG_POINTER(aLogStream);

  nsresult rv;

  if (!m_logStream) {
    nsCOMPtr <nsIFile> logFile;
    rv = GetLogFile(getter_AddRefs(logFile));
    NS_ENSURE_SUCCESS(rv,rv);

    // append to the end of the log file
    rv = MsgNewBufferedFileOutputStream(getter_AddRefs(m_logStream),
                                        logFile,
                                        PR_CREATE_FILE | PR_WRONLY | PR_APPEND,
                                        0600);
    NS_ENSURE_SUCCESS(rv,rv);

    if (!m_logStream)
      return NS_ERROR_FAILURE;

    int64_t fileSize;
    rv = logFile->GetFileSize(&fileSize);
    NS_ENSURE_SUCCESS(rv, rv);

    // write the header at the start
    if (fileSize == 0)
    {
      uint32_t writeCount;

      rv = m_logStream->Write(LOG_HEADER, LOG_HEADER_LEN, &writeCount);
      NS_ENSURE_SUCCESS(rv, rv);
      NS_ASSERTION(writeCount == LOG_HEADER_LEN, "failed to write out log header");
    }
  }

  NS_ADDREF(*aLogStream = m_logStream);
  return NS_OK;
}

NS_IMETHODIMP
nsMsgFilterList::ApplyFiltersToHdr(nsMsgFilterTypeType filterType,
                                   nsIMsgDBHdr *msgHdr,
                                   nsIMsgFolder *folder,
                                   nsIMsgDatabase *db,
                                   const char*headers,
                                   uint32_t headersSize,
                                   nsIMsgFilterHitNotify *listener,
                                   nsIMsgWindow *msgWindow)
{
  nsCOMPtr<nsIMsgFilter> filter;
  uint32_t filterCount = 0;
  nsresult rv = GetFilterCount(&filterCount);
  NS_ENSURE_SUCCESS(rv, rv);

  nsMsgSearchScopeTerm* scope = new nsMsgSearchScopeTerm(nullptr, nsMsgSearchScope::offlineMail, folder);
  scope->AddRef();
  if (!scope) return NS_ERROR_OUT_OF_MEMORY;

  for (uint32_t filterIndex = 0; filterIndex < filterCount; filterIndex++)
  {
    if (NS_SUCCEEDED(GetFilterAt(filterIndex, getter_AddRefs(filter))))
    {
      bool isEnabled;
      nsMsgFilterTypeType curFilterType;

      filter->GetEnabled(&isEnabled);
      if (!isEnabled)
        continue;

      filter->GetFilterType(&curFilterType);
      if (curFilterType & filterType)
      {
        nsresult matchTermStatus = NS_OK;
        bool result;

        filter->SetScope(scope);
        matchTermStatus = filter->MatchHdr(msgHdr, folder, db, headers, headersSize, &result);
        filter->SetScope(nullptr);
        if (NS_SUCCEEDED(matchTermStatus) && result && listener)
        {
          bool applyMore = true;

          rv = listener->ApplyFilterHit(filter, msgWindow, &applyMore);
          if (NS_FAILED(rv) || !applyMore)
            break;
        }
      }
    }
  }
  scope->Release();
  return rv;
}

NS_IMETHODIMP
nsMsgFilterList::SetDefaultFile(nsIFile *aFile)
{
  m_defaultFile = aFile;
  return NS_OK;
}

NS_IMETHODIMP
nsMsgFilterList::GetDefaultFile(nsIFile **aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);

    NS_IF_ADDREF(*aResult = m_defaultFile);
    return NS_OK;
}

NS_IMETHODIMP
nsMsgFilterList::SaveToDefaultFile()
{
    nsresult rv;
    nsCOMPtr<nsIMsgFilterService> filterService =
        do_GetService(NS_MSGFILTERSERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    return filterService->SaveFilterList(this, m_defaultFile);
}

typedef struct
{
  nsMsgFilterFileAttribValue  attrib;
  const char      *attribName;
} FilterFileAttribEntry;

static FilterFileAttribEntry FilterFileAttribTable[] =
{
  {nsIMsgFilterList::attribNone,      ""},
  {nsIMsgFilterList::attribVersion,    "version"},
  {nsIMsgFilterList::attribLogging,    "logging"},
  {nsIMsgFilterList::attribName,      "name"},
  {nsIMsgFilterList::attribEnabled,    "enabled"},
  {nsIMsgFilterList::attribDescription,  "description"},
  {nsIMsgFilterList::attribType,      "type"},
  {nsIMsgFilterList::attribScriptFile,  "scriptName"},
  {nsIMsgFilterList::attribAction,    "action"},
  {nsIMsgFilterList::attribActionValue,  "actionValue"},
  {nsIMsgFilterList::attribCondition,    "condition"},
  {nsIMsgFilterList::attribCustomId,  "customId"},
};

static const unsigned int sNumFilterFileAttribTable =
  NS_ARRAY_LENGTH(FilterFileAttribTable);

// If we want to buffer file IO, wrap it in here.
char nsMsgFilterList::ReadChar(nsIInputStream *aStream)
{
  char  newChar;
  uint32_t bytesRead;
  nsresult rv = aStream->Read(&newChar, 1, &bytesRead);
  if (NS_FAILED(rv) || !bytesRead)
    return -1;
  uint64_t bytesAvailable;
  rv = aStream->Available(&bytesAvailable);
  if (NS_FAILED(rv))
    return -1;
  else
  {
    if (m_startWritingToBuffer)
      m_unparsedFilterBuffer.Append(newChar);
    return newChar;
  }
}

char nsMsgFilterList::SkipWhitespace(nsIInputStream *aStream)
{
  char ch;
  do
  {
    ch = ReadChar(aStream);
  } while (!(ch & 0x80) && isspace(ch)); // isspace can crash with non-ascii input

  return ch;
}

bool nsMsgFilterList::StrToBool(nsCString &str)
{
  return str.Equals("yes") ;
}

char nsMsgFilterList::LoadAttrib(nsMsgFilterFileAttribValue &attrib, nsIInputStream *aStream)
{
  char  attribStr[100];
  char  curChar;
  attrib = nsIMsgFilterList::attribNone;

  curChar = SkipWhitespace(aStream);
  int i;
  for (i = 0; i + 1 < (int)(sizeof(attribStr)); )
  {
    if (curChar == (char) -1 || (!(curChar & 0x80) && isspace(curChar)) || curChar == '=')
      break;
    attribStr[i++] = curChar;
    curChar = ReadChar(aStream);
  }
  attribStr[i] = '\0';
  for (unsigned int tableIndex = 0; tableIndex < sNumFilterFileAttribTable; tableIndex++)
  {
    if (!PL_strcasecmp(attribStr, FilterFileAttribTable[tableIndex].attribName))
    {
      attrib = FilterFileAttribTable[tableIndex].attrib;
      break;
    }
  }
  return curChar;
}

const char *nsMsgFilterList::GetStringForAttrib(nsMsgFilterFileAttribValue attrib)
{
  for (unsigned int tableIndex = 0; tableIndex < sNumFilterFileAttribTable; tableIndex++)
  {
    if (attrib == FilterFileAttribTable[tableIndex].attrib)
      return FilterFileAttribTable[tableIndex].attribName;
  }
  return nullptr;
}

nsresult nsMsgFilterList::LoadValue(nsCString &value, nsIInputStream *aStream)
{
  nsCAutoString  valueStr;
  char  curChar;
  value = "";
  curChar = SkipWhitespace(aStream);
  if (curChar != '"')
  {
    NS_ASSERTION(false, "expecting quote as start of value");
    return NS_MSG_FILTER_PARSE_ERROR;
  }
  curChar = ReadChar(aStream);
  do
  {
    if (curChar == '\\')
    {
      char nextChar = ReadChar(aStream);
      if (nextChar == '"')
        curChar = '"';
      else if (nextChar == '\\')  // replace "\\" with "\"
      {
        valueStr += curChar;
        curChar = ReadChar(aStream);
      }
      else
      {
        valueStr += curChar;
        curChar = nextChar;
      }
    }
    else
    {
      if (curChar == (char) -1 || curChar == '"' || curChar == '\n' || curChar == '\r')
      {
        value += valueStr;
        break;
      }
    }
    valueStr += curChar;
    curChar = ReadChar(aStream);
  }
  while (curChar != -1);
  return NS_OK;
}

nsresult nsMsgFilterList::LoadTextFilters(nsIInputStream *aStream)
{
  nsresult  err = NS_OK;
  uint64_t bytesAvailable;

  nsCOMPtr<nsIInputStream> bufStream;
  err = NS_NewBufferedInputStream(getter_AddRefs(bufStream), aStream, 10240);
  NS_ENSURE_SUCCESS(err, err);

  nsMsgFilterFileAttribValue attrib;
  nsCOMPtr<nsIMsgRuleAction> currentFilterAction;
  // We'd really like to move lot's of these into the objects that they refer to.
  do
  {
    nsCAutoString value;
    nsresult intToStringResult;

    char curChar;
    curChar = LoadAttrib(attrib, bufStream);
    if (curChar == (char) -1)  //reached eof
      break;
    err = LoadValue(value, bufStream);
    if (err != NS_OK)
      break;
    switch(attrib)
    {
    case nsIMsgFilterList::attribNone:
      if (m_curFilter)
        m_curFilter->SetUnparseable(true);
      break;
    case nsIMsgFilterList::attribVersion:
      m_fileVersion = value.ToInteger(&intToStringResult);
      if (NS_FAILED(intToStringResult))
      {
        attrib = nsIMsgFilterList::attribNone;
        NS_ASSERTION(false, "error parsing filter file version");
      }
      break;
    case nsIMsgFilterList::attribLogging:
      m_loggingEnabled = StrToBool(value);
      m_unparsedFilterBuffer.Truncate(); //we are going to buffer each filter as we read them, make sure no garbage is there
      m_startWritingToBuffer = true; //filters begin now
      break;
    case nsIMsgFilterList::attribName:  //every filter starts w/ a name
      {
        if (m_curFilter)
        {
          int32_t nextFilterStartPos = m_unparsedFilterBuffer.RFind("name");

          nsCAutoString nextFilterPart;
          nextFilterPart = Substring(m_unparsedFilterBuffer, nextFilterStartPos, m_unparsedFilterBuffer.Length());
          m_unparsedFilterBuffer.SetLength(nextFilterStartPos);

          bool unparseableFilter;
          m_curFilter->GetUnparseable(&unparseableFilter);
          if (unparseableFilter)
          {
            m_curFilter->SetUnparsedBuffer(m_unparsedFilterBuffer);
            m_curFilter->SetEnabled(false); //disable the filter because we don't know how to apply it
          }
          m_unparsedFilterBuffer = nextFilterPart;
        }
        nsMsgFilter *filter = new nsMsgFilter;
        if (filter == nullptr)
        {
          err = NS_ERROR_OUT_OF_MEMORY;
          break;
        }
        filter->SetFilterList(static_cast<nsIMsgFilterList*>(this));
        if (m_fileVersion == k45Version)
        {
          nsAutoString unicodeStr;
          err = nsMsgI18NConvertToUnicode(nsMsgI18NFileSystemCharset(),
                                          value, unicodeStr);
          if (NS_FAILED(err))
              break;

          filter->SetFilterName(unicodeStr);
        }
        else
        {
          // ### fix me - this is silly.
          PRUnichar *unicodeString =
            nsTextFormatter::smprintf(unicodeFormatter, value.get());
          filter->SetFilterName(nsDependentString(unicodeString));
          nsTextFormatter::smprintf_free(unicodeString);
        }
        m_curFilter = filter;
        m_filters.AppendElement(filter);
      }
      break;
    case nsIMsgFilterList::attribEnabled:
      if (m_curFilter)
        m_curFilter->SetEnabled(StrToBool(value));
      break;
    case nsIMsgFilterList::attribDescription:
      if (m_curFilter)
        m_curFilter->SetFilterDesc(value);
      break;
    case nsIMsgFilterList::attribType:
      if (m_curFilter)
      {
        // Older versions of filters didn't have the ability to turn on/off the
        // manual filter context, so default manual to be on in that case
        int32_t filterType = value.ToInteger(&intToStringResult);
        if (m_fileVersion < kManualContextVersion)
          filterType |= nsMsgFilterType::Manual;
        m_curFilter->SetType((nsMsgFilterTypeType) filterType);
      }
      break;
    case nsIMsgFilterList::attribScriptFile:
      if (m_curFilter)
        m_curFilter->SetFilterScript(&value);
      break;
    case nsIMsgFilterList::attribAction:
      if (m_curFilter)
      {
        nsMsgRuleActionType actionType = nsMsgFilter::GetActionForFilingStr(value);
        if (actionType == nsMsgFilterAction::None)
          m_curFilter->SetUnparseable(true);
        else
        {
          err = m_curFilter->CreateAction(getter_AddRefs(currentFilterAction));
          NS_ENSURE_SUCCESS(err, err);
          currentFilterAction->SetType(actionType);
          m_curFilter->AppendAction(currentFilterAction);
        }
      }
      break;
    case nsIMsgFilterList::attribActionValue:
      if (m_curFilter && currentFilterAction)
      {
        nsMsgRuleActionType type;
        currentFilterAction->GetType(&type);
        if (type == nsMsgFilterAction::MoveToFolder ||
              type == nsMsgFilterAction::CopyToFolder)
          err = m_curFilter->ConvertMoveOrCopyToFolderValue(currentFilterAction, value);
        else if (type == nsMsgFilterAction::ChangePriority)
        {
          nsMsgPriorityValue outPriority;
          nsresult res = NS_MsgGetPriorityFromString(value.get(), outPriority);
          if (NS_SUCCEEDED(res))
            currentFilterAction->SetPriority(outPriority);
          else
            NS_ASSERTION(false, "invalid priority in filter file");
        }
        else if (type == nsMsgFilterAction::Label)
        {
          // upgrade label to corresponding tag/keyword
          nsresult res;
          int32_t labelInt = value.ToInteger(&res);
          if (NS_SUCCEEDED(res))
          {
            nsCAutoString keyword("$label");
            keyword.Append('0' + labelInt);
            currentFilterAction->SetType(nsMsgFilterAction::AddTag);
            currentFilterAction->SetStrValue(keyword);
          }
        }
        else if (type == nsMsgFilterAction::JunkScore)
        {
          nsresult res;
          int32_t junkScore = value.ToInteger(&res);
          if (NS_SUCCEEDED(res))
            currentFilterAction->SetJunkScore(junkScore);
        }
        else if (type == nsMsgFilterAction::Forward ||
                 type == nsMsgFilterAction::Reply ||
                 type == nsMsgFilterAction::AddTag ||
                 type == nsMsgFilterAction::Custom)
        {
          currentFilterAction->SetStrValue(value);
        }
      }
      break;
    case nsIMsgFilterList::attribCondition:
      if (m_curFilter)
      {
        if (m_fileVersion == k45Version)
        {
          nsAutoString unicodeStr;
          err = nsMsgI18NConvertToUnicode(nsMsgI18NFileSystemCharset(),
                                          value, unicodeStr);
          if (NS_FAILED(err))
              break;

          char *utf8 = ToNewUTF8String(unicodeStr);
          value.Assign(utf8);
          nsMemory::Free(utf8);
        }
        err = ParseCondition(m_curFilter, value.get());
        if (err == NS_ERROR_INVALID_ARG)
          err = m_curFilter->SetUnparseable(true);
        NS_ENSURE_SUCCESS(err, err);
      }
      break;
    case nsIMsgFilterList::attribCustomId:
      if (m_curFilter && currentFilterAction)
      {
        err = currentFilterAction->SetCustomId(value);
        NS_ENSURE_SUCCESS(err, err);
      }
      break;

    }
  } while (NS_SUCCEEDED(bufStream->Available(&bytesAvailable)));

  if (m_curFilter)
  {
    bool unparseableFilter;
    m_curFilter->GetUnparseable(&unparseableFilter);
    if (unparseableFilter)
    {
      m_curFilter->SetUnparsedBuffer(m_unparsedFilterBuffer);
      m_curFilter->SetEnabled(false);  //disable the filter because we don't know how to apply it
    }
  }

  return err;
}

// parse condition like "(subject, contains, fred) AND (body, isn't, "foo)")"
// values with close parens will be quoted.
// what about values with close parens and quotes? e.g., (body, isn't, "foo")")
// I guess interior quotes will need to be escaped - ("foo\")")
// which will get written out as (\"foo\\")\") and read in as ("foo\")"
// ALL means match all messages.
NS_IMETHODIMP nsMsgFilterList::ParseCondition(nsIMsgFilter *aFilter, const char *aCondition)
{
  NS_ENSURE_ARG_POINTER(aFilter);

  bool    done = false;
  nsresult  err = NS_OK;
  const char *curPtr = aCondition;
  if (!strcmp(aCondition, "ALL"))
  {
    nsMsgSearchTerm *newTerm = new nsMsgSearchTerm;

    if (newTerm)
    {
      newTerm->m_matchAll = true;
      aFilter->AppendTerm(newTerm);
    }
    return (newTerm) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
  }

  while (!done)
  {
    // insert code to save the boolean operator if there is one for this search term....
    const char *openParen = PL_strchr(curPtr, '(');
    const char *orTermPos = PL_strchr(curPtr, 'O');    // determine if an "OR" appears b4 the openParen...
    bool ANDTerm = true;
    if (orTermPos && orTermPos < openParen) // make sure OR term falls before the '('
      ANDTerm = false;

    char *termDup = nullptr;
    if (openParen)
    {
      bool foundEndTerm = false;
      bool inQuote = false;
      for (curPtr = openParen +1; *curPtr; curPtr++)
      {
        if (*curPtr == '\\' && *(curPtr + 1) == '"')
          curPtr++;
        else if (*curPtr == ')' && !inQuote)
        {
          foundEndTerm = true;
          break;
        }
        else if (*curPtr == '"')
          inQuote = !inQuote;
      }
      if (foundEndTerm)
      {
        int termLen = curPtr - openParen - 1;
        termDup = (char *) PR_Malloc(termLen + 1);
        if (termDup)
        {
          PL_strncpy(termDup, openParen + 1, termLen + 1);
          termDup[termLen] = '\0';
        }
        else
        {
          err = NS_ERROR_OUT_OF_MEMORY;
          break;
        }
      }
    }
    else
      break;
    if (termDup)
    {
      nsMsgSearchTerm  *newTerm = new nsMsgSearchTerm;

      if (newTerm)
      {
        /* Invert nsMsgSearchTerm::EscapeQuotesInStr() */
        for (char *to = termDup, *from = termDup;;)
        {
          if (*from == '\\' && from[1] == '"') from++;
          if (!(*to++ = *from++)) break;
        }
        newTerm->m_booleanOp = (ANDTerm) ? nsMsgSearchBooleanOp::BooleanAND
                                         : nsMsgSearchBooleanOp::BooleanOR;

        err = newTerm->DeStreamNew(termDup, PL_strlen(termDup));
        NS_ENSURE_SUCCESS(err, err);
        aFilter->AppendTerm(newTerm);
      }
      PR_FREEIF(termDup);
    }
    else
      break;
  }
  return err;
}

nsresult nsMsgFilterList::WriteIntAttr(nsMsgFilterFileAttribValue attrib, int value, nsIOutputStream *aStream)
{
  nsresult rv = NS_OK;
  const char *attribStr = GetStringForAttrib(attrib);
  if (attribStr)
  {
    uint32_t bytesWritten;
    nsCAutoString writeStr(attribStr);
    writeStr.AppendLiteral("=\"");
    writeStr.AppendInt(value);
    writeStr.AppendLiteral("\"" MSG_LINEBREAK);
    rv = aStream->Write(writeStr.get(), writeStr.Length(), &bytesWritten);
  }
  return rv;
}

NS_IMETHODIMP
nsMsgFilterList::WriteStrAttr(nsMsgFilterFileAttribValue attrib,
                              const char *aStr, nsIOutputStream *aStream)
{
  nsresult rv = NS_OK;
  if (aStr && *aStr && aStream) // only proceed if we actually have a string to write out.
  {
    char *escapedStr = nullptr;
    if (PL_strchr(aStr, '"'))
      escapedStr = nsMsgSearchTerm::EscapeQuotesInStr(aStr);

    const char *attribStr = GetStringForAttrib(attrib);
    if (attribStr)
    {
      uint32_t bytesWritten;
      nsCAutoString writeStr(attribStr);
      writeStr.AppendLiteral("=\"");
      writeStr.Append((escapedStr) ? escapedStr : aStr);
      writeStr.AppendLiteral("\"" MSG_LINEBREAK);
      rv = aStream->Write(writeStr.get(), writeStr.Length(), &bytesWritten);
    }
    PR_Free(escapedStr);
  }
  return rv;
}

nsresult nsMsgFilterList::WriteBoolAttr(nsMsgFilterFileAttribValue attrib, bool boolVal, nsIOutputStream *aStream)
{
  return WriteStrAttr(attrib, (boolVal) ? "yes" : "no", aStream);
}

nsresult
nsMsgFilterList::WriteWstrAttr(nsMsgFilterFileAttribValue attrib,
                               const PRUnichar *aFilterName, nsIOutputStream *aStream)
{
    WriteStrAttr(attrib, NS_ConvertUTF16toUTF8(aFilterName).get(), aStream);
    return NS_OK;
}

nsresult nsMsgFilterList::SaveTextFilters(nsIOutputStream *aStream)
{
  const char *attribStr;
  uint32_t   filterCount = 0;
  nsresult   err = GetFilterCount(&filterCount);
  NS_ENSURE_SUCCESS(err, err);
  err = NS_OK;

  attribStr = GetStringForAttrib(nsIMsgFilterList::attribVersion);
  err = WriteIntAttr(nsIMsgFilterList::attribVersion, kFileVersion, aStream);
  err = WriteBoolAttr(nsIMsgFilterList::attribLogging, m_loggingEnabled, aStream);
  for (uint32_t i = 0; i < filterCount; i ++)
  {
    nsCOMPtr<nsIMsgFilter> filter;
    if (NS_SUCCEEDED(GetFilterAt(i, getter_AddRefs(filter))) && filter)
    {
      filter->SetFilterList(this);

      // if the filter is temporary, don't write it to disk
      bool isTemporary;
      err = filter->GetTemporary(&isTemporary);
      if (NS_SUCCEEDED(err) && !isTemporary) {
        if ((err = filter->SaveToTextFile(aStream)) != NS_OK)
          break;
      }
    }
    else
      break;
  }
  if (NS_SUCCEEDED(err))
    m_arbitraryHeaders.Truncate();
  return err;
}

nsMsgFilterList::~nsMsgFilterList()
{
}

nsresult nsMsgFilterList::Close()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult nsMsgFilterList::GetFilterCount(uint32_t *pCount)
{
  NS_ENSURE_ARG_POINTER(pCount);

  *pCount = m_filters.Length();
  return NS_OK;
}

nsresult nsMsgFilterList::GetFilterAt(uint32_t filterIndex, nsIMsgFilter **filter)
{
  NS_ENSURE_ARG_POINTER(filter);

  uint32_t filterCount = 0;
  GetFilterCount(&filterCount);
  NS_ENSURE_ARG_MAX(filterIndex, filterCount - 1);

  NS_IF_ADDREF(*filter = m_filters[filterIndex]);
  return NS_OK;
}

nsresult
nsMsgFilterList::GetFilterNamed(const nsAString &aName, nsIMsgFilter **aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);

    uint32_t count = 0;
    nsresult rv = GetFilterCount(&count);
    NS_ENSURE_SUCCESS(rv, rv);

    *aResult = nullptr;
    for (uint32_t i = 0; i < count; i++) {
        nsCOMPtr<nsIMsgFilter> filter;
        rv = GetFilterAt(i, getter_AddRefs(filter));
        if (NS_FAILED(rv)) continue;

        nsString filterName;
        filter->GetFilterName(filterName);
        if (filterName.Equals(aName))
        {
            *aResult = filter;
            break;
        }
    }

    NS_IF_ADDREF(*aResult);
    return NS_OK;
}

nsresult nsMsgFilterList::SetFilterAt(uint32_t filterIndex, nsIMsgFilter *filter)
{
  m_filters[filterIndex] = filter;
  return NS_OK;
}


nsresult nsMsgFilterList::RemoveFilterAt(uint32_t filterIndex)
{
  m_filters.RemoveElementAt(filterIndex);
  return NS_OK;
}

nsresult
nsMsgFilterList::RemoveFilter(nsIMsgFilter *aFilter)
{
  m_filters.RemoveElement(aFilter);
  return NS_OK;
}

nsresult nsMsgFilterList::InsertFilterAt(uint32_t filterIndex, nsIMsgFilter *aFilter)
{
  if (!m_temporaryList)
    aFilter->SetFilterList(this);
  m_filters.InsertElementAt(filterIndex, aFilter);

  return NS_OK;
}

// Attempt to move the filter at index filterIndex in the specified direction.
// If motion not possible in that direction, we still return success.
// We could return an error if the FE's want to beep or something.
nsresult nsMsgFilterList::MoveFilterAt(uint32_t filterIndex,
                                       nsMsgFilterMotionValue motion)
{
  NS_ENSURE_ARG((motion == nsMsgFilterMotion::up) ||
                (motion == nsMsgFilterMotion::down));

  uint32_t filterCount = 0;
  nsresult rv = GetFilterCount(&filterCount);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ENSURE_ARG_MAX(filterIndex, filterCount - 1);

  uint32_t newIndex = filterIndex;

  if (motion == nsMsgFilterMotion::up)
  {
    // are we already at the top?
    if (filterIndex == 0)
      return NS_OK;

    newIndex = filterIndex - 1;

  }
  else if (motion == nsMsgFilterMotion::down)
  {
    // are we already at the bottom?
    if (filterIndex == filterCount - 1)
      return NS_OK;

    newIndex = filterIndex + 1;
  }

  nsCOMPtr<nsIMsgFilter> tempFilter1;
  rv = GetFilterAt(newIndex, getter_AddRefs(tempFilter1));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIMsgFilter> tempFilter2;
  rv = GetFilterAt(filterIndex, getter_AddRefs(tempFilter2));
  NS_ENSURE_SUCCESS(rv, rv);

  SetFilterAt(newIndex, tempFilter2);
  SetFilterAt(filterIndex, tempFilter1);

  return NS_OK;
}

nsresult nsMsgFilterList::MoveFilter(nsIMsgFilter *aFilter,
                                     nsMsgFilterMotionValue motion)
{
  int32_t filterIndex = m_filters.IndexOf(aFilter, 0);
  NS_ENSURE_ARG(filterIndex != m_filters.NoIndex);

  return MoveFilterAt(filterIndex, motion);
}

nsresult
nsMsgFilterList::GetVersion(int16_t *aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    *aResult = m_fileVersion;
    return NS_OK;
}

NS_IMETHODIMP nsMsgFilterList::MatchOrChangeFilterTarget(const nsACString &oldFolderUri, const nsACString &newFolderUri, bool caseInsensitive, bool *found)
{
  NS_ENSURE_ARG_POINTER(found);

  uint32_t numFilters = 0;
  nsresult rv = GetFilterCount(&numFilters);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIMsgFilter> filter;
  nsCString folderUri;
  *found = false;
  for (uint32_t index = 0; index < numFilters; index++)
  {
    rv = GetFilterAt(index, getter_AddRefs(filter));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsISupportsArray> filterActionList;
    rv = filter->GetActionList(getter_AddRefs(filterActionList));
    NS_ENSURE_SUCCESS(rv, rv);
    uint32_t numActions;
    filterActionList->Count(&numActions);

    for (uint32_t actionIndex = 0; actionIndex < numActions; actionIndex++)
    {
      nsCOMPtr<nsIMsgRuleAction> filterAction =
          do_QueryElementAt(filterActionList, actionIndex);
      nsMsgRuleActionType actionType;
      if (filterAction)
        filterAction->GetType(&actionType);
      else
        continue;

      if (actionType == nsMsgFilterAction::MoveToFolder ||
          actionType == nsMsgFilterAction::CopyToFolder)
      {
        rv = filterAction->GetTargetFolderUri(folderUri);
        if (NS_SUCCEEDED(rv) && !folderUri.IsEmpty())
        {
          bool matchFound = false;
          if (caseInsensitive)
          {
            if (folderUri.Equals(oldFolderUri, nsCaseInsensitiveCStringComparator())) //local
              matchFound = true;
          }
          else
          {
            if (folderUri.Equals(oldFolderUri))  //imap
              matchFound = true;
          }
          if (matchFound)
          {
            *found = true;
            //if we just want to match the uri's, newFolderUri will be null
            if (!newFolderUri.IsEmpty())
            {
              rv = filterAction->SetTargetFolderUri(newFolderUri);
              NS_ENSURE_SUCCESS(rv, rv);
            }
          }
        }
      }
    }
  }
  return rv;
}

// this would only return true if any filter was on "any header", which we
// don't support in 6.x
NS_IMETHODIMP nsMsgFilterList::GetShouldDownloadAllHeaders(bool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  *aResult = false;
  return NS_OK;
}

// leaves m_arbitraryHeaders filed in with the arbitrary headers.
nsresult nsMsgFilterList::ComputeArbitraryHeaders()
{
  NS_ENSURE_TRUE (m_arbitraryHeaders.IsEmpty(), NS_OK);

  uint32_t numFilters = 0;
  nsresult rv = GetFilterCount(&numFilters);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIMsgFilter> filter;
  nsMsgSearchAttribValue attrib;
  nsCString arbitraryHeader;
  for (uint32_t index = 0; index < numFilters; index++)
  {
    rv = GetFilterAt(index, getter_AddRefs(filter));
    if (!(NS_SUCCEEDED(rv) && filter)) continue;

    nsCOMPtr <nsISupportsArray> searchTerms;
    uint32_t numSearchTerms=0;
    filter->GetSearchTerms(getter_AddRefs(searchTerms));
    if (searchTerms)
      searchTerms->Count(&numSearchTerms);
    for (uint32_t i = 0; i < numSearchTerms; i++)
    {
      filter->GetTerm(i, &attrib, nullptr, nullptr, nullptr, arbitraryHeader);
      if (!arbitraryHeader.IsEmpty())
      {
        if (m_arbitraryHeaders.IsEmpty())
          m_arbitraryHeaders.Assign(arbitraryHeader);
        else if (m_arbitraryHeaders.Find(arbitraryHeader, CaseInsensitiveCompare) == -1)
        {
          m_arbitraryHeaders.Append(" ");
          m_arbitraryHeaders.Append(arbitraryHeader);
        }
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsMsgFilterList::GetArbitraryHeaders(nsACString &aResult)
{
  ComputeArbitraryHeaders();
  aResult = m_arbitraryHeaders;
  return NS_OK;
}

NS_IMETHODIMP nsMsgFilterList::FlushLogIfNecessary()
{
  // only flush the log if we are logging
  bool loggingEnabled = false;
  nsresult rv = GetLoggingEnabled(&loggingEnabled);
  NS_ENSURE_SUCCESS(rv,rv);

  if (loggingEnabled)
  {
    nsCOMPtr <nsIOutputStream> logStream;
    rv = GetLogStream(getter_AddRefs(logStream));
    if (NS_SUCCEEDED(rv) && logStream) {
      rv = logStream->Flush();
      NS_ENSURE_SUCCESS(rv,rv);
    }
  }
  return rv;
}
// ------------ End FilterList methods ------------------
