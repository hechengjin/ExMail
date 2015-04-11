/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsMsgUtils.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIMsgAccountManager.h"
#include "nsIMsgAccount.h"
#include "nsIPop3IncomingServer.h"
#include "nsMsgBaseCID.h"
#include "nsMsgCompCID.h"
#include "nsISmtpService.h"
#include "nsISmtpServer.h"
#include "nsEudoraWin32.h"
#include "nsIImportService.h"
#include "nsIImportMailboxDescriptor.h"
#include "nsIImportABDescriptor.h"
#include "nsEudoraStringBundle.h"
#include "nsEudoraImport.h"
#include "nsUnicharUtils.h"
#include "nsCRT.h"
#include "nsNetUtil.h"
#include "nsILineInputStream.h"
#include "EudoraDebugLog.h"
#include "prmem.h"
#include "plstr.h"

static NS_DEFINE_IID(kISupportsIID,      NS_ISUPPORTS_IID);

static const char *  kWhitespace = "\b\t\r\n ";

BYTE * nsEudoraWin32::GetValueBytes(HKEY hKey, const char *pValueName)
{
  LONG err;
  DWORD bufSz;
  LPBYTE pBytes = NULL;

  err = ::RegQueryValueEx(hKey, pValueName, NULL, NULL, NULL, &bufSz);
  if (err == ERROR_SUCCESS)
  {
    pBytes = new BYTE[bufSz];
    err = ::RegQueryValueEx(hKey, pValueName, NULL, NULL, pBytes, &bufSz);
    if (err != ERROR_SUCCESS)
    {
      delete [] pBytes;
      pBytes = NULL;
    }
  }

  return pBytes;
}


nsEudoraWin32::nsEudoraWin32()
{
  m_pMimeSection = nullptr;
}

nsEudoraWin32::~nsEudoraWin32()
{
  delete [] m_pMimeSection;
}

bool nsEudoraWin32::FindMailFolder(nsIFile **pFolder)
{
  return FindEudoraLocation(pFolder);
}

bool nsEudoraWin32::FindEudoraLocation(nsIFile **pFolder, bool findIni)
{
  bool result = false;
  bool    exists = false;
  nsCOMPtr <nsIFile> eudoraPath = do_CreateInstance(NS_LOCAL_FILE_CONTRACTID);
  // look in the registry to see where eudora is installed?
  HKEY  sKey;
  if (::RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Qualcomm\\Eudora\\CommandLine", 0, KEY_QUERY_VALUE, &sKey) == ERROR_SUCCESS)
  {
    // get the value of "Current"
    BYTE *pBytes = GetValueBytes(sKey, "Current");
    if (pBytes)
    {
      nsCString str((const char *)pBytes);
      delete [] pBytes;

      MsgCompressWhitespace(str);

      // Command line is Eudora mailfolder eudora.ini
      if (findIni)
      {
        // find the string coming after the last space
        int32_t index = str.RFind(" ");
        if (index != -1)
        {
          index++; // skip the space
          eudoraPath->InitWithNativePath(Substring(str, index));
          eudoraPath->IsFile(&exists);
          if (exists)
            result = exists;
          else // it may just be the mailbox location....guess that there will be a eudora.ini file there
          {
            eudoraPath->AppendNative(NS_LITERAL_CSTRING("eudora.ini"));
            eudoraPath->IsFile(&exists);
            result = exists;
          }
        }
      } // if findIni
      else
      {
        int  idx = -1;
        if (str.CharAt(0) == '"')
        {
          idx = str.FindChar('"', 1);
          if (idx != -1)
            idx++;
        }
        else
          idx = str.FindChar(' ');

        if (idx != -1)
        {
          idx++;
          while (str.CharAt(idx) == ' ') idx++;
          int endIdx = -1;
          if (str.CharAt(idx) == '"')
            endIdx = str.FindChar('"', idx);
          else {
            endIdx = str.FindChar(' ', idx);
            if (endIdx == -1)
              endIdx = str.Length();
          }
          if (endIdx != -1)
          {
            eudoraPath->InitWithNativePath(Substring(str, idx, endIdx - idx));

            if (NS_SUCCEEDED(eudoraPath->IsDirectory(&exists)))
              result = exists;
          }
        }
      }
    } // if pBytes
    ::RegCloseKey(sKey);
  }

  NS_IF_ADDREF(*pFolder = eudoraPath);
  return result;
}

nsresult nsEudoraWin32::FindMailboxes(nsIFile *pRoot, nsISupportsArray **ppArray)
{
  nsresult rv = NS_NewISupportsArray(ppArray);
  if (NS_FAILED(rv))
  {
    IMPORT_LOG0("FAILED to allocate the nsISupportsArray\n");
    return rv;
  }

  nsCOMPtr<nsIImportService> impSvc(do_GetService(NS_IMPORTSERVICE_CONTRACTID, &rv));
  if (NS_FAILED(rv))
    return rv;

  m_depth = 0;
  m_mailImportLocation = do_QueryInterface(pRoot);
  return ScanMailDir(pRoot, *ppArray, impSvc);
}


nsresult nsEudoraWin32::ScanMailDir(nsIFile *pFolder, nsISupportsArray *pArray, nsIImportService *pImport)
{
  bool            exists = false;
  bool            isFile = false;
  char *          pContents = nullptr;
  int32_t          len = 0;
  nsCOMPtr<nsIFile>  descMap;
  nsresult        rv;

  if (NS_FAILED(rv = pFolder->Clone(getter_AddRefs(descMap))))
    return rv;

  m_depth++;

  rv = descMap->AppendNative(NS_LITERAL_CSTRING("descmap.pce"));
  if (NS_SUCCEEDED(rv))
    rv = descMap->IsFile(&isFile);
  if (NS_SUCCEEDED(rv))
    rv = descMap->Exists(&exists);
  if (NS_SUCCEEDED(rv) && exists && isFile)
  {
    nsCOMPtr<nsIInputStream> inputStream;
    nsresult rv = NS_NewLocalFileInputStream(getter_AddRefs(inputStream), descMap);
    if (NS_FAILED(rv))
      return rv;

    uint64_t bytesLeft64;
    rv = inputStream->Available(&bytesLeft64);
    if (NS_FAILED(rv))
    {
      IMPORT_LOG0("*** Error checking address file for eof\n");
      inputStream->Close();
      return rv;
    }
    uint32_t bytesLeft = NS_MIN(PR_UINT32_MAX - 1, bytesLeft64);
    pContents = (char *) PR_Malloc(bytesLeft + 1);
    if (!pContents)
      return NS_ERROR_OUT_OF_MEMORY;
    uint32_t bytesRead;
    rv = inputStream->Read(pContents, bytesLeft, &bytesRead);
    if (bytesRead != bytesLeft)
      return NS_ERROR_FAILURE;
    pContents[bytesRead] = '\0';
    if (NS_SUCCEEDED(rv) && pContents)
    {
      len = bytesRead;
      if (NS_SUCCEEDED(rv))
        rv = ScanDescmap(pFolder, pArray, pImport, pContents, len);
      PR_Free(pContents);
    }
    else
      rv = NS_ERROR_FAILURE;
  }

  if (NS_FAILED(rv) || !isFile || !exists)
    rv = IterateMailDir(pFolder, pArray, pImport);

  m_depth--;

  return rv;
}

nsresult nsEudoraWin32::IterateMailDir(nsIFile *pFolder, nsISupportsArray *pArray, nsIImportService *pImport)
{
  bool hasMore;
  nsCOMPtr<nsISimpleEnumerator> directoryEnumerator;
  nsresult rv = pFolder->GetDirectoryEntries(getter_AddRefs(directoryEnumerator));
  NS_ENSURE_SUCCESS(rv, rv);

  directoryEnumerator->HasMoreElements(&hasMore);
  bool              isFolder;
  bool              isFile;
  nsCOMPtr<nsIFile> entry;
  nsCString         fName;
  nsCString         ext;
  nsCString         name;

  while (hasMore && NS_SUCCEEDED(rv))
  {
    nsCOMPtr<nsISupports> aSupport;
    rv = directoryEnumerator->GetNext(getter_AddRefs(aSupport));
    nsCOMPtr<nsIFile> entry(do_QueryInterface(aSupport, &rv));
    directoryEnumerator->HasMoreElements(&hasMore);

    if (NS_SUCCEEDED(rv))
    {
      rv = entry->GetNativeLeafName(fName);
      if (NS_SUCCEEDED(rv) && !fName.IsEmpty())
      {
        if (fName.Length() > 4)
        {
          ext = StringTail(fName, 4);
          name = StringHead(fName, fName.Length() - 4);
        }
        else
        {
          ext.Truncate();
          name = fName;
        }
        ToLowerCase(ext);
        if (ext.EqualsLiteral(".fol"))
        {
          isFolder = false;
          entry->IsDirectory(&isFolder);
          if (isFolder)
          {
            // add the folder
            rv = FoundMailFolder(entry, name.get(), pArray, pImport);
            if (NS_SUCCEEDED(rv))
            {
              rv = ScanMailDir(entry, pArray, pImport);
              if (NS_FAILED(rv))
                IMPORT_LOG0("*** Error scanning mail directory\n");
            }
          }
        }
        else if (ext.EqualsLiteral(".mbx"))
        {
          isFile = false;
          entry->IsFile(&isFile);
          if (isFile)
            rv = FoundMailbox(entry, name.get(), pArray, pImport);
        }
      }
    }
  }
  return rv;
}

nsresult nsEudoraWin32::ScanDescmap(nsIFile *pFolder, nsISupportsArray *pArray, nsIImportService *pImport, const char *pData, int32_t len)
{
  // use this to find stuff in the directory.

  nsCOMPtr<nsIFile>  entry;
  nsresult        rv;

  if (NS_FAILED(rv = pFolder->Clone(getter_AddRefs(entry))))
    return rv;
  entry->AppendNative(NS_LITERAL_CSTRING("dummy"));
  // format is Name,FileName,Type,Flag?
  //  Type = M or S for mailbox
  //       = F for folder

  int32_t      fieldLen;
  int32_t      pos = 0;
  const char * pStart;
  nsCString    name;
  nsCString    fName;
  nsCString    type;
  nsCString    flag;
  bool         isFile;
  bool         isFolder;
  while (pos < len)
  {
    pStart = pData;
    fieldLen = 0;
    while ((pos < len) && (*pData != ','))
    {
      pos++;
      pData++;
      fieldLen++;
    }
    name.Truncate();
    if (fieldLen)
      name.Append(pStart, fieldLen);
    name.Trim(kWhitespace);
    pos++;
    pData++;
    pStart = pData;
    fieldLen = 0;
    while ((pos < len) && (*pData != ','))
    {
      pos++;
      pData++;
      fieldLen++;
    }
    fName.Truncate();
    if (fieldLen)
      fName.Append(pStart, fieldLen);
    // Descmap file name is written without any extraneous white space - i.e.
    // if there's whitespace present it's intentional and important. Don't
    // strip whitespace from the fName.
    pos++;
    pData++;
    pStart = pData;
    fieldLen = 0;
    while ((pos < len) && (*pData != ','))
    {
      pos++;
      pData++;
      fieldLen++;
    }
    type.Truncate();
    if (fieldLen)
      type.Append(pStart, fieldLen);
    type.Trim(kWhitespace);
    pos++;
    pData++;
    pStart = pData;
    fieldLen = 0;
    // Skip to next end of line, or ',' separator.
    while ((pos < len) &&
           (*pData != nsCRT::CR) && (*pData != nsCRT::LF) && (*pData != ','))
    {
      pos++;
      pData++;
      fieldLen++;
    }
    flag.Truncate();
    if (fieldLen)
      flag.Append(pStart, fieldLen);
    flag.Trim(kWhitespace);
    // Skip over end of line(s).
    while ((pos < len) && ((*pData == nsCRT::CR) || (*pData == nsCRT::LF)))
    {
      pos++;
      pData++;
    }

    IMPORT_LOG2("name: %s, fName: %s\n", name.get(), fName.get());

    if (!fName.IsEmpty() && !name.IsEmpty() && (type.Length() == 1))
    {
      rv = entry->SetNativeLeafName(fName);
      if (NS_SUCCEEDED(rv))
      {
        if (type.CharAt(0) == 'F')
        {
          isFolder = false;
          entry->IsDirectory(&isFolder);
          if (isFolder)
          {
            rv = FoundMailFolder(entry, name.get(), pArray, pImport);
            if (NS_SUCCEEDED(rv))
            {
              rv = ScanMailDir(entry, pArray, pImport);
              if (NS_FAILED(rv))
                IMPORT_LOG0("*** Error scanning mail directory\n");
            }
          }
        }
        else if ((type.CharAt(0) == 'M') || (type.CharAt(0) == 'S'))
        {
          isFile = false;
          entry->IsFile(&isFile);
          if (isFile)
            FoundMailbox(entry, name.get(), pArray, pImport);
        }
      }
    }
  }

  return NS_OK;
}


nsresult nsEudoraWin32::FoundMailbox(nsIFile *mailFile, const char *pName, nsISupportsArray *pArray, nsIImportService *pImport)
{
  nsString displayName;
  nsCOMPtr<nsIImportMailboxDescriptor> desc;
  nsISupports * pInterface;

  NS_CopyNativeToUnicode(nsDependentCString(pName), displayName);

#ifdef IMPORT_DEBUG
  nsCAutoString path;
  mailFile->GetNativePath(path);
  if (!path.IsEmpty())
    IMPORT_LOG2("Found eudora mailbox, %s: %s\n", path.get(), pName);
  else
    IMPORT_LOG1("Found eudora mailbox, %s\n", pName);
  IMPORT_LOG1("\tm_depth = %d\n", (int)m_depth);
#endif

  nsresult rv = pImport->CreateNewMailboxDescriptor(getter_AddRefs(desc));
  if (NS_SUCCEEDED(rv))
  {
    int64_t sz = 0;
    mailFile->GetFileSize(&sz);
    desc->SetDisplayName(displayName.get());
    desc->SetDepth(m_depth);
    desc->SetSize(sz);
    nsCOMPtr <nsIFile> pFile = nullptr;
    desc->GetFile(getter_AddRefs(pFile));
    if (pFile)
    {
      pFile->InitWithFile(mailFile);
    }
    rv = desc->QueryInterface(kISupportsIID, (void **) &pInterface);
    pArray->AppendElement(pInterface);
    pInterface->Release();
  }

  return NS_OK;
}


nsresult nsEudoraWin32::FoundMailFolder(nsIFile *mailFolder, const char *pName, nsISupportsArray *pArray, nsIImportService *pImport)
{
  nsString                displayName;
  nsCOMPtr<nsIImportMailboxDescriptor>  desc;
  nsISupports *              pInterface;

  NS_CopyNativeToUnicode(nsDependentCString(pName), displayName);

#ifdef IMPORT_DEBUG
  nsCAutoString path;
  mailFolder->GetNativePath(path);
  if (!path.IsEmpty())
    IMPORT_LOG2("Found eudora folder, %s: %s\n", path.get(), pName);
  else
    IMPORT_LOG1("Found eudora folder, %s\n", pName);
  IMPORT_LOG1("\tm_depth = %d\n", (int)m_depth);
#endif

  nsresult rv = pImport->CreateNewMailboxDescriptor(getter_AddRefs(desc));
  if (NS_SUCCEEDED(rv))
  {
    uint32_t    sz = 0;
    desc->SetDisplayName(displayName.get());
    desc->SetDepth(m_depth);
    desc->SetSize(sz);
    nsCOMPtr <nsIFile> pFile = nullptr;
    desc->GetFile(getter_AddRefs(pFile));
    if (pFile)
    {
      pFile->InitWithFile(mailFolder);
    }
    rv = desc->QueryInterface(kISupportsIID, (void **) &pInterface);
    pArray->AppendElement(pInterface);
    pInterface->Release();
  }

  return NS_OK;
}


nsresult nsEudoraWin32::FindTOCFile(nsIFile *pMailFile, nsIFile **ppTOCFile, bool *pDeleteToc)
{
  nsresult    rv;
  nsCAutoString  leaf;

  *pDeleteToc = false;
  *ppTOCFile = nullptr;
  rv = pMailFile->GetNativeLeafName(leaf);
  if (NS_FAILED(rv))
    return rv;
  rv = pMailFile->GetParent(ppTOCFile);
  if (NS_FAILED(rv))
    return rv;

  nsCString  name;
  if ((leaf.Length() > 4) && (leaf.CharAt(leaf.Length() - 4) == '.'))
    name = StringHead(leaf, leaf.Length() - 4);
  else
    name = leaf;
  name.Append(".toc");
  rv = (*ppTOCFile)->AppendNative(name);
  if (NS_FAILED(rv))
    return rv;

  bool    exists = false;
  rv = (*ppTOCFile)->Exists(&exists);
  if (NS_FAILED(rv))
    return rv;
  bool    isFile = false;
  rv = (*ppTOCFile)->IsFile(&isFile);
  if (NS_FAILED(rv))
    return rv;
  if (exists && isFile)
    return NS_OK;

  return NS_ERROR_FAILURE;
}


bool nsEudoraWin32::ImportSettings(nsIFile *pIniFile, nsIMsgAccount **localMailAccount)
{
  bool      result = false;
  nsresult  rv;

  nsCOMPtr<nsIMsgAccountManager> accMgr =
           do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
  if (NS_FAILED(rv))
  {
    IMPORT_LOG0("*** Failed to create a account manager!\n");
    return false;
  }

  // Eudora info is arranged by key, 1 for the default, then persona's for additional
  // accounts.
  // Start with the first one, then do each persona
  nsCString iniPath;
  pIniFile->GetNativePath(iniPath);
  if (iniPath.IsEmpty())
    return false;
  UINT       valInt;
  SimpleBufferTonyRCopiedOnce  section;
  DWORD      sSize;
  DWORD      sOffset = 0;
  DWORD      start;
  nsCString  sectionName("Settings");
  int        popCount = 0;
  int        accounts = 0;

  DWORD  allocSize = 0;
  do
  {
    allocSize += 2048;
    section.Allocate(allocSize);
    sSize = ::GetPrivateProfileSection("Personalities", section.m_pBuffer, allocSize, iniPath.get());
  } while (sSize == (allocSize - 2));

  nsIMsgAccount *  pAccount;

  do
  {
    if (!sectionName.IsEmpty())
    {
      pAccount = nullptr;
      valInt = ::GetPrivateProfileInt(sectionName.get(), "UsesPOP", 1, iniPath.get());
      if (valInt)
      {
        // This is a POP account
        if (BuildPOPAccount(accMgr, sectionName.get(), iniPath.get(), &pAccount))
        {
          accounts++;
          popCount++;
          if (popCount > 1)
          {
            if (localMailAccount && *localMailAccount)
              NS_RELEASE(*localMailAccount);
          }
          else if (localMailAccount)
          {
            *localMailAccount = pAccount;
            NS_IF_ADDREF(pAccount);
          }
        }
      }
      else
      {
        valInt = ::GetPrivateProfileInt(sectionName.get(), "UsesIMAP", 0, iniPath.get());
        if (valInt)
        {
          // This is an IMAP account
          if (BuildIMAPAccount(accMgr, sectionName.get(), iniPath.get(), &pAccount))
            accounts++;
        }
      }
      if (pAccount && (sOffset == 0))
        accMgr->SetDefaultAccount(pAccount);

      NS_IF_RELEASE(pAccount);
    }

    sectionName.Truncate();
    while ((sOffset < sSize) && (section.m_pBuffer[sOffset] != '='))
      sOffset++;
    sOffset++;
    start = sOffset;
    while ((sOffset < sSize) && (section.m_pBuffer[sOffset] != 0))
      sOffset++;
    if (sOffset > start)
    {
      sectionName.Append(section.m_pBuffer + start, sOffset - start);
      sectionName.Trim(kWhitespace);
    }

  } while (sOffset < sSize);

  // Now save the new acct info to pref file.
  rv = accMgr->SaveAccountInfo();
  NS_ASSERTION(NS_SUCCEEDED(rv), "Can't save account info to pref file");


  return accounts != 0;
}

bool nsEudoraWin32::FindFiltersFile(nsIFile **pFiltersFile)
{
  bool result = FindEudoraLocation(pFiltersFile, false);

  if (result)
  {
    (*pFiltersFile)->AppendNative(NS_LITERAL_CSTRING("Filters.pce"));
    (*pFiltersFile)->IsFile(&result);
  }

  return result;
}

bool nsEudoraWin32::GetMailboxNameHierarchy(const nsACString& pEudoraLocation, const char* pEudoraFilePath, nsCString& nameHierarchy)
{
  if (pEudoraLocation.IsEmpty() || !pEudoraFilePath || !*pEudoraFilePath)
    return false;

  nsresult rv;
  nsCOMPtr <nsIFile> descMap = do_CreateInstance(NS_LOCAL_FILE_CONTRACTID);

  rv = descMap->InitWithNativePath(pEudoraLocation);
  NS_ENSURE_SUCCESS(rv, false);
  rv = descMap->AppendNative(NS_LITERAL_CSTRING("descmap.pce"));
  NS_ENSURE_SUCCESS(rv, false);

  nsCOMPtr <nsIInputStream> inputStream;
  rv = NS_NewLocalFileInputStream(getter_AddRefs(inputStream), descMap);
  NS_ENSURE_SUCCESS(rv, false);

  nsCOMPtr<nsILineInputStream> lineStream(do_QueryInterface(inputStream, &rv));
  NS_ENSURE_SUCCESS(rv, false);

  int32_t pathLength;
  const char* backslash = strchr(pEudoraFilePath, '\\');
  if (backslash)
    pathLength = backslash - pEudoraFilePath;
  else
    pathLength = strlen(pEudoraFilePath);

  bool more = true;
  nsCAutoString buf;
  while (more)
  {
    rv = lineStream->ReadLine(buf, &more);
    NS_ENSURE_SUCCESS(rv, false);

    int32_t iNameEnd = buf.FindChar(',');
    if (iNameEnd < 0)
      continue;
    const nsACString& name = Substring(buf, 0, iNameEnd);
    int32_t iPathEnd = buf.FindChar(',', iNameEnd + 1);
    if (iPathEnd < 0)
      continue;
    const nsACString& path = Substring(buf, iNameEnd + 1, iPathEnd - iNameEnd - 1);
    const char type = buf[iPathEnd + 1];
    if (strnicmp(path.BeginReading(), pEudoraFilePath, pathLength) == 0 && path.Length() == pathLength)
    {
      nameHierarchy += "\\";
      nameHierarchy += name;
      if (pEudoraFilePath[pathLength] == 0)
        return true;
      if (type != 'F')
      {
        // Something went wrong.  We've matched a mailbox, but the
        // hierarchical name says we've got more folders to traverse.
        return false;
      }

      nsCString newLocation(pEudoraLocation);
      newLocation += '\\';
      newLocation += path;
      return GetMailboxNameHierarchy(newLocation, pEudoraFilePath + pathLength + 1, nameHierarchy);
    }
  }

  return false;
}

// maximium size of settings strings
#define  kIniValueSize    384


void nsEudoraWin32::GetServerAndUserName(const char *pSection, const char *pIni, nsCString& serverName, nsCString& userName, char *pBuff)
{
  DWORD    valSize;
  int      idx;
  nsCString  tStr;

  serverName.Truncate();
  userName.Truncate();

  valSize = ::GetPrivateProfileString(pSection, "PopServer", "", pBuff, kIniValueSize, pIni);
  if (valSize)
    serverName = pBuff;
  else
  {
    valSize = ::GetPrivateProfileString(pSection, "POPAccount", "", pBuff, kIniValueSize, pIni);
    if (valSize)
    {
      serverName = pBuff;
      idx = serverName.FindChar('@');
      if (idx != -1)
        serverName = Substring(serverName, idx + 1);
    }
  }
  valSize = ::GetPrivateProfileString(pSection, "LoginName", "", pBuff, kIniValueSize, pIni);
  if (valSize)
    userName = pBuff;
  else
  {
    valSize = ::GetPrivateProfileString(pSection, "POPAccount", "", pBuff, kIniValueSize, pIni);
    if (valSize)
    {
      userName = pBuff;
      idx = userName.FindChar('@');
      if (idx != -1)
        userName.SetLength(idx);
    }
  }
}

void nsEudoraWin32::GetAccountName(const char *pSection, nsString& str)
{
  str.Truncate();

  nsCString s(pSection);

  if (s.LowerCaseEqualsLiteral("settings"))
  {
    str.AssignLiteral("Eudora ");
    str.Append(NS_ConvertASCIItoUTF16(pSection));
  }
  else
  {
    str.AssignASCII(pSection);
    if (StringBeginsWith(s, NS_LITERAL_CSTRING("persona-"), nsCaseInsensitiveCStringComparator()))
      CopyASCIItoUTF16(Substring(s, 8), str);
  }
}


bool nsEudoraWin32::BuildPOPAccount(nsIMsgAccountManager *accMgr, const char *pSection, const char *pIni, nsIMsgAccount **ppAccount)
{
  if (ppAccount)
    *ppAccount = nullptr;

  char valBuff[kIniValueSize];
  nsCString serverName;
  nsCString userName;

  GetServerAndUserName(pSection, pIni, serverName, userName, valBuff);

  if (serverName.IsEmpty() || userName.IsEmpty())
    return false;

  bool result = false;

  // I now have a user name/server name pair, find out if it already exists?
  nsCOMPtr<nsIMsgIncomingServer> in;
  nsresult rv = accMgr->FindServer(userName, serverName, NS_LITERAL_CSTRING("pop3"), getter_AddRefs(in));
  if (NS_FAILED(rv) || !in)
  {
    // Create the incoming server and an account for it?
    rv = accMgr->CreateIncomingServer(userName, serverName, NS_LITERAL_CSTRING("pop3"), getter_AddRefs(in));
    if (NS_SUCCEEDED(rv) && in)
    {
      rv = in->SetType(NS_LITERAL_CSTRING("pop3"));
      // rv = in->SetHostName(serverName);
      // rv = in->SetUsername(userName);

      IMPORT_LOG2("Created POP3 server named: %s, userName: %s\n", serverName.get(), userName.get());

      nsString prettyName;
      GetAccountName(pSection, prettyName);
      IMPORT_LOG1("\tSet pretty name to: %S\n", prettyName.get());
      rv = in->SetPrettyName(prettyName);

      // We have a server, create an account.
      nsCOMPtr<nsIMsgAccount>  account;
      rv = accMgr->CreateAccount(getter_AddRefs(account));
      if (NS_SUCCEEDED(rv) && account)
      {
        rv = account->SetIncomingServer(in);

        IMPORT_LOG0("Created a new account and set the incoming server to the POP3 server.\n");

        nsCOMPtr<nsIPop3IncomingServer> pop3Server = do_QueryInterface(in, &rv);
        NS_ENSURE_SUCCESS(rv,rv);
        UINT valInt = ::GetPrivateProfileInt(pSection, "LeaveMailOnServer", 0, pIni);
        pop3Server->SetLeaveMessagesOnServer(valInt ? true : false);

        // Fiddle with the identities
        SetIdentities(accMgr, account, pSection, pIni, userName.get(), serverName.get(), valBuff);
        result = true;
        if (ppAccount)
          account->QueryInterface(NS_GET_IID(nsIMsgAccount), (void **)ppAccount);
      }
    }
  }
  else
    result = true;

  return result;
}

bool nsEudoraWin32::BuildIMAPAccount(nsIMsgAccountManager *accMgr, const char *pSection, const char *pIni, nsIMsgAccount **ppAccount)
{
  char valBuff[kIniValueSize];
  nsCString serverName;
  nsCString userName;

  GetServerAndUserName(pSection, pIni, serverName, userName, valBuff);

  if (serverName.IsEmpty() || userName.IsEmpty())
    return false;

  bool result = false;

  nsCOMPtr<nsIMsgIncomingServer> in;
  nsresult rv = accMgr->FindServer(userName, serverName, NS_LITERAL_CSTRING("imap"), getter_AddRefs(in));
  if (NS_FAILED(rv) || (in == nullptr))
  {
    // Create the incoming server and an account for it?
    rv = accMgr->CreateIncomingServer(userName, serverName, NS_LITERAL_CSTRING("imap"), getter_AddRefs(in));
    if (NS_SUCCEEDED(rv) && in)
    {
      rv = in->SetType(NS_LITERAL_CSTRING("imap"));
      // rv = in->SetHostName(serverName);
      // rv = in->SetUsername(userName);

      IMPORT_LOG2("Created IMAP server named: %s, userName: %s\n", serverName.get(), userName.get());

      nsString prettyName;
      GetAccountName(pSection, prettyName);
      IMPORT_LOG1("\tSet pretty name to: %S\n", prettyName.get());
      rv = in->SetPrettyName(prettyName);

      // We have a server, create an account.
      nsCOMPtr<nsIMsgAccount> account;
      rv = accMgr->CreateAccount(getter_AddRefs(account));
      if (NS_SUCCEEDED(rv) && account)
      {
        rv = account->SetIncomingServer(in);

        IMPORT_LOG0("Created an account and set the IMAP server as the incoming server\n");

        // Fiddle with the identities
        SetIdentities(accMgr, account, pSection, pIni, userName.get(), serverName.get(), valBuff);
        result = true;
        if (ppAccount)
          account->QueryInterface(NS_GET_IID(nsIMsgAccount), (void **)ppAccount);
      }
    }
  }
  else
    result = true;

  return result;
}

void nsEudoraWin32::SetIdentities(nsIMsgAccountManager *accMgr, nsIMsgAccount *acc, const char *pSection, const char *pIniFile, const char *userName, const char *serverName, char *pBuff)
{
  nsCAutoString realName;
  nsCAutoString email;
  nsCAutoString server;
  DWORD valSize;
  nsresult rv;

  valSize = ::GetPrivateProfileString(pSection, "RealName", "", pBuff, kIniValueSize, pIniFile);
  if (valSize)
    realName = pBuff;
  valSize = ::GetPrivateProfileString(pSection, "SMTPServer", "", pBuff, kIniValueSize, pIniFile);
  if (valSize)
    server = pBuff;
  valSize = ::GetPrivateProfileString(pSection, "ReturnAddress", "", pBuff, kIniValueSize, pIniFile);
  if (valSize)
    email = pBuff;

  nsCOMPtr<nsIMsgIdentity> id;
  rv = accMgr->CreateIdentity(getter_AddRefs(id));
  if (id)
  {
    nsAutoString fullName;
    fullName.Assign(NS_ConvertASCIItoUTF16(realName));
    id->SetFullName(fullName);
    id->SetIdentityName(fullName);
    if (email.IsEmpty())
    {
      email = userName;
      email += "@";
      email += serverName;
    }
    id->SetEmail(email);
    acc->AddIdentity(id);

    IMPORT_LOG0("Created identity and added to the account\n");
    IMPORT_LOG1("\tname: %s\n", realName.get());
    IMPORT_LOG1("\temail: %s\n", email.get());
  }
  SetSmtpServer(accMgr, acc, server.get(), userName);
}


void nsEudoraWin32::SetSmtpServer(nsIMsgAccountManager *pMgr, nsIMsgAccount *pAcc, const char *pServer, const char *pUser)
{
  nsresult  rv;

  nsCOMPtr<nsISmtpService> smtpService(do_GetService(NS_SMTPSERVICE_CONTRACTID, &rv));
  if (NS_SUCCEEDED(rv) && smtpService)
  {
    nsCOMPtr<nsISmtpServer>    foundServer;

    rv = smtpService->FindServer(pUser, pServer, getter_AddRefs(foundServer));
    if (NS_SUCCEEDED(rv) && foundServer)
    {
      IMPORT_LOG1("SMTP server already exists: %s\n", pServer);
      return;
    }
    nsCOMPtr<nsISmtpServer>    smtpServer;

    rv = smtpService->CreateSmtpServer(getter_AddRefs(smtpServer));
    if (NS_SUCCEEDED(rv) && smtpServer)
    {
      smtpServer->SetHostname(nsDependentCString(pServer));
      if (pUser)
        smtpServer->SetUsername(nsDependentCString(pUser));

      IMPORT_LOG1("Created new SMTP server: %s\n", pServer);
    }
  }
}

nsresult nsEudoraWin32::GetAttachmentInfo(const char *pFileName, nsIFile *pFile, nsCString& mimeType, nsCString& aAttachmentName)
{
  nsresult  rv;
  mimeType.Truncate();
  pFile->InitWithNativePath(nsDependentCString(pFileName));
  bool      isFile = false;
  bool      exists = false;
  if (NS_FAILED(rv = pFile->Exists(&exists)))
    return rv;
  if (exists && NS_FAILED(rv = pFile->IsFile(&isFile)))
    return rv;

  if (!exists || !isFile)
  {
    // Windows Eudora writes the full path to the attachment when the message
    // is received, but doesn't update that path if the attachment directory
    // changes (e.g. if email directory is moved). When operating on an
    // attachment (opening, etc.) Eudora will first check the full path
    // and then if the file doesn't exist there Eudora will check the
    // current attachment directory for a file with the same name.
    //
    // Check to see if we have any better luck looking for the attachment
    // in the current attachment directory.
    nsCAutoString name;
    pFile->GetNativeLeafName(name);
    if (name.IsEmpty())
      return NS_ERROR_FAILURE;

    nsCOMPtr <nsIFile> altFile;
    rv = m_mailImportLocation->Clone(getter_AddRefs(altFile));
    NS_ENSURE_SUCCESS(rv, rv);
    // For now, we'll do that using a hard coded name and location for the
    // attachment directory on Windows (the default location) "attach" inside
    // of the Eudora mail directory. Once settings from Eudora are imported
    // better, we'll want to check where the settings say attachments are stored.
    altFile->AppendNative(NS_LITERAL_CSTRING("attach"));
    altFile->AppendNative(name);

    // Did we come up with a different path or was the original path already
    // in the current attachment directory?
    bool    isSamePath = true;
    rv = altFile->Equals(pFile, &isSamePath);

    if (NS_SUCCEEDED(rv) && !isSamePath)
    {
      // We came up with a different path - check the new path.
      if (NS_FAILED(rv = altFile->Exists(&exists)))
        return rv;
      if (exists && NS_FAILED(rv = altFile->IsFile(&isFile)))
        return rv;

      // Keep the new path if it helped us.
      if (exists && isFile)
      {
        nsCString   nativePath;
        altFile->GetNativePath(nativePath);
        pFile->InitWithNativePath(nativePath);
      }
    }
  }

  if (exists && isFile)
  {
    nsCAutoString name;
    pFile->GetNativeLeafName(name);
    if (name.IsEmpty())
      return NS_ERROR_FAILURE;
    if (name.Length() > 4)
    {
      nsCString ext;
      int32_t idx = name.RFindChar('.');
      if (idx != -1)
      {
        ext = Substring(name, idx);
        GetMimeTypeFromExtension(ext, mimeType);
      }
    }
    if (mimeType.IsEmpty())
      mimeType = "application/octet-stream";

    nsAutoString description;
    rv = NS_CopyNativeToUnicode(name, description);
 
    if (NS_SUCCEEDED(rv))
      aAttachmentName = NS_ConvertUTF16toUTF8(description);
    else
      aAttachmentName = name;

    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

bool nsEudoraWin32::FindMimeIniFile(nsIFile *pFile)
{
  bool hasMore;
  nsCOMPtr<nsISimpleEnumerator> directoryEnumerator;
  nsresult rv = pFile->GetDirectoryEntries(getter_AddRefs(directoryEnumerator));
  NS_ENSURE_SUCCESS(rv, rv);

  directoryEnumerator->HasMoreElements(&hasMore);
  bool              isFile;
  nsCOMPtr<nsIFile> entry;
  nsCString         fName;
  nsCString         ext;
  nsCString         name;

  bool found = false;

  while (hasMore && NS_SUCCEEDED(rv))
  {
    nsCOMPtr<nsISupports> aSupport;
    rv = directoryEnumerator->GetNext(getter_AddRefs(aSupport));
    nsCOMPtr<nsIFile> entry(do_QueryInterface(aSupport, &rv));
    directoryEnumerator->HasMoreElements(&hasMore);


    if (NS_SUCCEEDED(rv))
    {
      rv = entry->GetNativeLeafName(fName);
      if (NS_SUCCEEDED(rv) && !fName.IsEmpty())
      {
        if (fName.Length() > 4)
        {
          ext = StringTail(fName, 4);
          name = StringHead(fName, fName.Length() - 4);
        }
        else
        {
          ext.Truncate();
          name = fName;
        }
        ToLowerCase(ext);
        if (ext.EqualsLiteral(".ini"))
        {
          isFile = false;
          entry->IsFile(&isFile);
          if (isFile)
          {
            if (found)
            {
              // which one of these files is newer?
              int64_t  modDate1, modDate2;
              entry->GetLastModifiedTime(&modDate2);
              pFile->GetLastModifiedTime(&modDate1);
              if (modDate2 > modDate1)
                pFile->InitWithFile(entry);
            }
            else
            {
              pFile->InitWithFile(entry);
              found = true;
            }
          }
        }
      }
    }
  }
  return found;
}


void nsEudoraWin32::GetMimeTypeFromExtension(nsCString& ext, nsCString& mimeType)
{
  HKEY  sKey;
  if (::RegOpenKeyEx(HKEY_CLASSES_ROOT, ext.get(), 0, KEY_QUERY_VALUE, &sKey) == ERROR_SUCCESS)
  {
    // get the value of "Current"
    BYTE *pBytes = GetValueBytes(sKey, "Content Type");
    if (pBytes)
    {
      mimeType = (const char *)pBytes;
      delete [] pBytes;
    }
    ::RegCloseKey(sKey);
  }

  if (!mimeType.IsEmpty() || !m_mailImportLocation || (ext.Length() > 10))
    return;

  // TLR: FIXME: We should/could cache the extension to mime type maps we find
  // and check the cache first before scanning all of eudora's mappings?

  // It's not in the registry, try and find the .ini file for Eudora's private
  // mime type list
  if (!m_pMimeSection)
  {
    nsCOMPtr <nsIFile> pFile = do_CreateInstance(NS_LOCAL_FILE_CONTRACTID);
    if (!pFile)
      return;

    pFile->InitWithFile(m_mailImportLocation);

    pFile->AppendNative(NS_LITERAL_CSTRING("eudora.ini"));
    bool exists = false;
    bool isFile = false;
    nsresult rv = pFile->Exists(&exists);
    if (NS_SUCCEEDED(rv))
      rv = pFile->IsFile(&isFile);
    if (!isFile || !exists)
    {
      rv = pFile->InitWithFile(m_mailImportLocation);
      if (NS_FAILED(rv))
        return;
      if (!FindMimeIniFile(pFile))
        return;
    }

    nsCAutoString fileName;
    pFile->GetNativePath(fileName);
    if (fileName.IsEmpty())
      return;
    // Read the mime map section
    DWORD  size = 1024;
    DWORD  dSize = size - 2;
    while (dSize == (size - 2))
    {
      if (m_pMimeSection)
        delete [] m_pMimeSection;
      size += 1024;
      m_pMimeSection = new char[size];
      dSize = ::GetPrivateProfileSection("Mappings", m_pMimeSection, size, fileName.get());
    }
  }

  if (!m_pMimeSection)
    return;

  IMPORT_LOG1("Looking for mime type for extension: %s\n", ext.get());

  // out/in/both=extension,mac creator,mac type,mime type1,mime type2
  const char *pExt = ext.get();
  pExt++;

  char  *  pChar = m_pMimeSection;
  char  *  pStart;
  int      len;
  nsCString  tStr;
  for(;;)
  {
    while (*pChar != '=')
    {
      if (!(*pChar) && !(*(pChar + 1)))
        return;
      pChar++;
    }
    if (*pChar)
      pChar++;
    pStart = pChar;
    len = 0;
    while (*pChar && (*pChar != ','))
    {
      pChar++;
      len++;
    }
    if (!*pChar)
      return;

    tStr.Truncate();
    tStr.Append(pStart, len);
    tStr.Trim(kWhitespace);
    if (!PL_strcasecmp(tStr.get(), pExt))
    {
      // skip the mac creator and type
      pChar++;
      while (*pChar && (*pChar != ','))
        pChar++;
      if (!*pChar)
        return;

      pChar++;
      while (*pChar && (*pChar != ','))
        pChar++;
      if (!*pChar)
        return;

      pChar++;
      // Get the first mime type
      len = 0;
      pStart = pChar;
      while (*pChar && (*pChar != ','))
      {
        pChar++;
        len++;
      }
      if (!*pChar)
        return;

      pChar++;
      if (!len)
        continue;

      tStr.Truncate();
      tStr.Append(pStart, len);
      tStr.Trim(kWhitespace);
      if (tStr.IsEmpty())
        continue;

      mimeType.Truncate();
      mimeType.Append(tStr.get());
      mimeType.Append("/");
      pStart = pChar;
      len = 0;
      // Skip to next end of line.
      while (*pChar && (*pChar != nsCRT::CR) && (*pChar != nsCRT::LF))
      {
        pChar++;
        len++;
      }
      if (!len)
        continue;

      tStr.Truncate();
      tStr.Append(pStart, len);
      tStr.Trim(kWhitespace);
      if (tStr.IsEmpty())
        continue;

      mimeType.Append(tStr.get());

      IMPORT_LOG1("Found Mime Type: %s\n", mimeType.get());

      return;
    }
  }
}

bool nsEudoraWin32::FindAddressFolder(nsIFile **pFolder)
{
  IMPORT_LOG0("*** Looking for Eudora address folder\n");

  return FindEudoraLocation(pFolder);
}

nsresult nsEudoraWin32::FindAddressBooks(nsIFile *pRoot, nsISupportsArray **ppArray)
{
  nsresult rv;
  nsCOMPtr<nsIFile> file = do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = file->InitWithFile(pRoot);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = NS_NewISupportsArray(ppArray);
  if (NS_FAILED(rv))
  {
    IMPORT_LOG0("FAILED to allocate the nsISupportsArray\n");
    return rv;
  }

  nsCOMPtr<nsIImportService> impSvc(do_GetService(NS_IMPORTSERVICE_CONTRACTID, &rv));
  if (NS_FAILED(rv))
    return rv;
  m_addressImportFolder = pRoot;
  nsString    displayName;
  nsEudoraStringBundle::GetStringByID(EUDORAIMPORT_NICKNAMES_NAME, displayName);

  // First off, get the default nndbase.txt, then scan the default nicknames subdir,
  // then look in the .ini file for additional directories to scan for address books!
  bool exists = false;
  bool isFile = false;

  rv = file->AppendNative(NS_LITERAL_CSTRING("nndbase.txt"));
  bool checkedBoth = false;
  do
  {
    if (NS_SUCCEEDED(rv))
      rv = file->Exists(&exists);
    if (NS_SUCCEEDED(rv) && exists)
      rv = file->IsFile(&isFile);

    // Stop if we already checked the second default name possibility or
    // if we found the default address book name.
    if (checkedBoth || (exists && isFile))
      break;

    // Check for alternate file extension ".nnt" which Windows Eudora uses as an option
    // to hide from simple minded viruses that scan ".txt" files for addresses.
    rv = file->SetNativeLeafName(NS_LITERAL_CSTRING("nndbase.nnt"));
    checkedBoth = true;
  } while (NS_SUCCEEDED(rv));

  if (exists && isFile)
  {
    if (NS_FAILED(rv = FoundAddressBook(file, displayName.get(), *ppArray, impSvc)))
      return rv;
  }

  // Try the default directory
  rv = file->InitWithFile(pRoot);
  if (NS_FAILED(rv))
    return rv;
  rv = file->AppendNative(NS_LITERAL_CSTRING("Nickname"));
  bool isDir = false;
  exists = false;
  if (NS_SUCCEEDED(rv))
    rv = file->Exists(&exists);
  if (NS_SUCCEEDED(rv) && exists)
    rv = file->IsDirectory(&isDir);
  if (exists && isDir)
  {
    if (NS_FAILED(rv = ScanAddressDir(file, *ppArray, impSvc)))
      return rv;
  }

  // Try the ini file to find other directories!
  rv = file->InitWithFile(pRoot);
  if (NS_FAILED(rv))
    return rv;
  rv = file->AppendNative(NS_LITERAL_CSTRING("eudora.ini"));
  exists = false;
  isFile = false;
  if (NS_SUCCEEDED(rv))
    rv = file->Exists(&exists);
  if (NS_SUCCEEDED(rv) && exists)
    rv = file->IsFile(&isFile);

  if (!isFile || !exists)
  {
    rv = file->InitWithFile(pRoot);
    if (NS_FAILED(rv))
      return NS_OK;
    if (!FindMimeIniFile(file))
      return NS_OK;
  }

  nsCString fileName;
  file->GetNativePath(fileName);
  // This is the supposed ini file name!
  // Get the extra directories for nicknames and parse it for valid nickname directories
  // to look into...
  char *pBuffer = new char[2048];
  DWORD len = ::GetPrivateProfileString("Settings", "ExtraNicknameDirs", "", pBuffer, 2048, fileName.get());
  if (len == 2047)
  {
    // If the value is really that large then don't bother!
    delete [] pBuffer;
    return NS_OK;
  }
  nsCString  dirs(pBuffer);
  delete [] pBuffer;
  dirs.Trim(kWhitespace);
  int32_t  idx = 0;
  nsCString  currentDir;
  while ((idx = dirs.FindChar(';')) != -1)
  {
    currentDir = StringHead(dirs, idx);
    currentDir.Trim(kWhitespace);
    if (!currentDir.IsEmpty())
    {
      rv = file->InitWithNativePath(currentDir);
      exists = false;
      isDir = false;
      if (NS_SUCCEEDED(rv))
        rv = file->Exists(&exists);
      if (NS_SUCCEEDED(rv) && exists)
        rv = file->IsDirectory(&isDir);
      if (exists && isDir)
      {
        if (NS_FAILED(rv = ScanAddressDir(file, *ppArray, impSvc)))
          return rv;
      }
    }
    dirs = Substring(dirs, idx + 1);
    dirs.Trim(kWhitespace);
  }
  if (!dirs.IsEmpty())
  {
    rv = file->InitWithNativePath(dirs);
    exists = false;
    isDir = false;
    if (NS_SUCCEEDED(rv))
      rv = file->Exists(&exists);
    if (NS_SUCCEEDED(rv) && exists)
      rv = file->IsDirectory(&isDir);
    if (exists && isDir)
    {
      if (NS_FAILED(rv = ScanAddressDir(file, *ppArray, impSvc)))
        return rv;
    }
  }

  return NS_OK;
}


nsresult nsEudoraWin32::ScanAddressDir(nsIFile *pDir, nsISupportsArray *pArray, nsIImportService *impSvc)
{
  bool hasMore;
  nsCOMPtr<nsISimpleEnumerator> directoryEnumerator;
  nsresult rv = pDir->GetDirectoryEntries(getter_AddRefs(directoryEnumerator));
  NS_ENSURE_SUCCESS(rv, rv);

  directoryEnumerator->HasMoreElements(&hasMore);
  bool              isFile;
  nsCOMPtr<nsIFile> entry;
  nsCString         fName;
  nsCString         ext;
  nsCString         name;

  bool found = false;

  while (hasMore && NS_SUCCEEDED(rv))
  {
    nsCOMPtr<nsISupports> aSupport;
    rv = directoryEnumerator->GetNext(getter_AddRefs(aSupport));
    nsCOMPtr<nsIFile> entry(do_QueryInterface(aSupport, &rv));
    directoryEnumerator->HasMoreElements(&hasMore);

    if (NS_SUCCEEDED(rv))
    {
      rv = entry->GetNativeLeafName(fName);
      if (NS_SUCCEEDED(rv) && !fName.IsEmpty())
      {
        if (fName.Length() > 4)
        {
          ext = StringTail(fName, 4);
          name = StringHead(fName, fName.Length() - 4);
        }
        else
        {
          ext.Truncate();
          name = fName;
        }
        ToLowerCase(ext);
        if (ext.EqualsLiteral(".txt"))
        {
          isFile = false;
          entry->IsFile(&isFile);
          if (isFile)
          {
            rv = FoundAddressBook(entry, nullptr, pArray, impSvc);
            if (NS_FAILED(rv))
              return rv;
          }
        }
      }
    }
  }

  return rv;
}


nsresult nsEudoraWin32::FoundAddressBook(nsIFile *file, const PRUnichar *pName, nsISupportsArray *pArray, nsIImportService *impSvc)
{
  nsCOMPtr<nsIImportABDescriptor> desc;
  nsISupports *  pInterface;
  nsString name;
  nsresult rv;

  if (pName)
    name = pName;
  else {
    nsAutoString leaf;
    rv = file->GetLeafName(leaf);
    if (NS_FAILED(rv))
      return rv;
    if (leaf.IsEmpty())
      return NS_ERROR_FAILURE;
    nsString  tStr;
    tStr = StringTail(leaf, 4);
    if (tStr.LowerCaseEqualsLiteral(".txt")  || tStr.LowerCaseEqualsLiteral(".nnt"))
      leaf.SetLength(leaf.Length() - 4);
  }

  rv = impSvc->CreateNewABDescriptor(getter_AddRefs(desc));
  if (NS_SUCCEEDED(rv))
  {
    int64_t sz = 0;
    file->GetFileSize(&sz);
    desc->SetPreferredName(name);
    desc->SetSize((uint32_t) sz);
    desc->SetAbFile(file);
    rv = desc->QueryInterface(kISupportsIID, (void **) &pInterface);
    pArray->AppendElement(pInterface);
    pInterface->Release();
  }
  if (NS_FAILED(rv))
  {
    IMPORT_LOG0("*** Error creating address book descriptor for eudora\n");
    return rv;
  }

  return NS_OK;
}


void nsEudoraWin32::ConvertPath(nsCString& str)
{
  nsCString  temp;
  nsCString  path;
  int32_t    idx = 0;
  int32_t    start = 0;
  nsCString  search;

  idx = str.FindChar('\\', idx);
  if ((idx == 2) && (str.CharAt(1) == ':'))
  {
    path = StringHead(str, 3);
    idx++;
    idx = str.FindChar('\\', idx);
    start = 3;
    if ((idx == -1) && (str.Length() > 3))
      path.Append(Substring(str, start));
  }

  WIN32_FIND_DATA findFileData;
  while (idx != -1)
  {
    search = path;
    search.Append(Substring(str, start, idx - start));
    HANDLE h = FindFirstFile(search.get(), &findFileData);
    if (h == INVALID_HANDLE_VALUE)
      return;
    path.Append(findFileData.cFileName);
    idx++;
    start = idx;
    idx = str.FindChar('\\', idx);
    FindClose(h);
    if (idx != -1)
      path.Append('\\');
    else
    {
      path.Append('\\');
      path.Append(Substring(str, start));
    }
  }

  str = path;
}

