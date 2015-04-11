/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMsgIncomingServer.h"
#include "nscore.h"
#include "plstr.h"
#include "prmem.h"
#include "prprf.h"

#include "nsIServiceManager.h"
#include "nsCOMPtr.h"
#include "nsStringGlue.h"
#include "nsISupportsObsolete.h"
#include "nsISupportsPrimitives.h"

#include "nsMsgBaseCID.h"
#include "nsMsgDBCID.h"
#include "nsIMsgFolder.h"
#include "nsIMsgFolderCache.h"
#include "nsIMsgPluggableStore.h"
#include "nsIMsgFolderCacheElement.h"
#include "nsIMsgWindow.h"
#include "nsIMsgFilterService.h"
#include "nsIMsgProtocolInfo.h"
#include "nsIPrefService.h"
#include "nsIRelativeFilePref.h"
#include "nsIDocShell.h"
#include "nsIAuthPrompt.h"
#include "nsNetUtil.h"
#include "nsIWindowWatcher.h"
#include "nsIStringBundle.h"
#include "nsIMsgHdr.h"
#include "nsIRDFService.h"
#include "nsRDFCID.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsILoginInfo.h"
#include "nsILoginManager.h"
#include "nsIMsgAccountManager.h"
#include "nsIMsgMdnGenerator.h"
#include "nsMsgFolderFlags.h"
#include "nsMsgUtils.h"
#include "nsMsgMessageFlags.h"
#include "nsAppDirectoryServiceDefs.h"
#include "mozilla/Services.h"

#define PORT_NOT_SET -1

nsMsgIncomingServer::nsMsgIncomingServer():
    m_rootFolder(0),
    m_numMsgsDownloaded(0),
    m_biffState(nsIMsgFolder::nsMsgBiffState_Unknown),
    m_serverBusy(false),
    m_canHaveFilters(true),
    m_displayStartupPage(true),
    mPerformingBiff(false)
{ 
  m_downloadedHdrs.Init(50);
}

nsMsgIncomingServer::~nsMsgIncomingServer()
{
}

NS_IMPL_THREADSAFE_ADDREF(nsMsgIncomingServer)
NS_IMPL_THREADSAFE_RELEASE(nsMsgIncomingServer)
NS_INTERFACE_MAP_BEGIN(nsMsgIncomingServer)
    NS_INTERFACE_MAP_ENTRY(nsIMsgIncomingServer)
    NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIMsgIncomingServer)
NS_INTERFACE_MAP_END_THREADSAFE

NS_IMETHODIMP
nsMsgIncomingServer::SetServerBusy(bool aServerBusy)
{
  m_serverBusy = aServerBusy;
  return NS_OK;
}

NS_IMETHODIMP
nsMsgIncomingServer::GetServerBusy(bool * aServerBusy)
{
  NS_ENSURE_ARG_POINTER(aServerBusy);
  *aServerBusy = m_serverBusy;
  return NS_OK;
}

NS_IMETHODIMP
nsMsgIncomingServer::GetKey(nsACString& serverKey)
{
  serverKey = m_serverKey;
  return NS_OK;
}

NS_IMETHODIMP
nsMsgIncomingServer::SetKey(const nsACString& serverKey)
{
  m_serverKey.Assign(serverKey);

  // in order to actually make use of the key, we need the prefs
  nsresult rv;
  nsCOMPtr<nsIPrefService> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString branchName;
  branchName.AssignLiteral("mail.server.");
  branchName.Append(m_serverKey);
  branchName.Append('.');
  rv = prefs->GetBranch(branchName.get(), getter_AddRefs(mPrefBranch));
  NS_ENSURE_SUCCESS(rv, rv);

  return prefs->GetBranch("mail.server.default.", getter_AddRefs(mDefPrefBranch));
}

NS_IMETHODIMP
nsMsgIncomingServer::SetRootFolder(nsIMsgFolder * aRootFolder)
{
  m_rootFolder = aRootFolder;
  return NS_OK;
}

// this will return the root folder of this account,
// even if this server is deferred.
NS_IMETHODIMP
nsMsgIncomingServer::GetRootFolder(nsIMsgFolder * *aRootFolder)
{
  NS_ENSURE_ARG_POINTER(aRootFolder);
  if (!m_rootFolder)
  {
    nsresult rv = CreateRootFolder();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  NS_IF_ADDREF(*aRootFolder = m_rootFolder);
  return NS_OK;
}

// this will return the root folder of the deferred to account,
// if this server is deferred.
NS_IMETHODIMP
nsMsgIncomingServer::GetRootMsgFolder(nsIMsgFolder **aRootMsgFolder)
{
  return GetRootFolder(aRootMsgFolder);
}

NS_IMETHODIMP
nsMsgIncomingServer::PerformExpand(nsIMsgWindow *aMsgWindow)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMsgIncomingServer::VerifyLogon(nsIUrlListener *aUrlListener, nsIMsgWindow *aMsgWindow,
                                 nsIURI **aURL)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMsgIncomingServer::PerformBiff(nsIMsgWindow* aMsgWindow)
{
  //This has to be implemented in the derived class, but in case someone doesn't implement it
  //just return not implemented.
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMsgIncomingServer::GetNewMessages(nsIMsgFolder *aFolder, nsIMsgWindow *aMsgWindow,
                      nsIUrlListener *aUrlListener)
{
  NS_ENSURE_ARG_POINTER(aFolder);
  return aFolder->GetNewMessages(aMsgWindow, aUrlListener);
}

NS_IMETHODIMP nsMsgIncomingServer::GetPerformingBiff(bool *aPerformingBiff)
{
  NS_ENSURE_ARG_POINTER(aPerformingBiff);
  *aPerformingBiff = mPerformingBiff;
  return NS_OK;
}

NS_IMETHODIMP nsMsgIncomingServer::SetPerformingBiff(bool aPerformingBiff)
{
  mPerformingBiff = aPerformingBiff;
  return NS_OK;
}

NS_IMPL_GETSET(nsMsgIncomingServer, BiffState, uint32_t, m_biffState)

NS_IMETHODIMP nsMsgIncomingServer::WriteToFolderCache(nsIMsgFolderCache *folderCache)
{
  nsresult rv = NS_OK;
  if (m_rootFolder)
  {
    nsCOMPtr <nsIMsgFolder> msgFolder = do_QueryInterface(m_rootFolder, &rv);
    if (NS_SUCCEEDED(rv) && msgFolder)
      rv = msgFolder->WriteToFolderCache(folderCache, true /* deep */);
  }
  return rv;
}

NS_IMETHODIMP
nsMsgIncomingServer::Shutdown()
{
  nsresult rv = CloseCachedConnections();
  mFilterPlugin = nullptr;
  NS_ENSURE_SUCCESS(rv,rv);

  if (mFilterList)
  {
    // close the filter log stream
    rv = mFilterList->SetLogStream(nullptr);
    NS_ENSURE_SUCCESS(rv,rv);
    mFilterList = nullptr;
  }

  if (mSpamSettings)
  {
    // close the spam log stream
    rv = mSpamSettings->SetLogStream(nullptr);
    NS_ENSURE_SUCCESS(rv,rv);
    mSpamSettings = nullptr;
  }
  return rv;
}

NS_IMETHODIMP
nsMsgIncomingServer::CloseCachedConnections()
{
  // derived class should override if they cache connections.
  return NS_OK;
}

NS_IMETHODIMP
nsMsgIncomingServer::GetDownloadMessagesAtStartup(bool *getMessagesAtStartup)
{
  // derived class should override if they need to do this.
  *getMessagesAtStartup = false;
  return NS_OK;
}

NS_IMETHODIMP
nsMsgIncomingServer::GetCanHaveFilters(bool *canHaveFilters)
{
  NS_ENSURE_ARG_POINTER(canHaveFilters);
  *canHaveFilters = m_canHaveFilters;
  return NS_OK;
}

NS_IMETHODIMP
nsMsgIncomingServer::SetCanHaveFilters(bool aCanHaveFilters)
{
  m_canHaveFilters = aCanHaveFilters;
  return NS_OK;
}

NS_IMETHODIMP
nsMsgIncomingServer::GetCanBeDefaultServer(bool *canBeDefaultServer)
{
  // derived class should override if they need to do this.
  *canBeDefaultServer = false;
  return NS_OK;
}

NS_IMETHODIMP
nsMsgIncomingServer::GetCanSearchMessages(bool *canSearchMessages)
{
  // derived class should override if they need to do this.
  NS_ENSURE_ARG_POINTER(canSearchMessages);
  *canSearchMessages = false;
  return NS_OK;
}

NS_IMETHODIMP
nsMsgIncomingServer::GetCanCompactFoldersOnServer(bool *canCompactFoldersOnServer)
{
  // derived class should override if they need to do this.
  NS_ENSURE_ARG_POINTER(canCompactFoldersOnServer);
  *canCompactFoldersOnServer = true;
  return NS_OK;
}

NS_IMETHODIMP
nsMsgIncomingServer::GetCanUndoDeleteOnServer(bool *canUndoDeleteOnServer)
{
  // derived class should override if they need to do this.
  NS_ENSURE_ARG_POINTER(canUndoDeleteOnServer);
  *canUndoDeleteOnServer = true;
  return NS_OK;
}

NS_IMETHODIMP
nsMsgIncomingServer::GetCanEmptyTrashOnExit(bool *canEmptyTrashOnExit)
{
  // derived class should override if they need to do this.
  NS_ENSURE_ARG_POINTER(canEmptyTrashOnExit);
  *canEmptyTrashOnExit = true;
  return NS_OK;
}

// construct <localStoreType>://[<username>@]<hostname
NS_IMETHODIMP
nsMsgIncomingServer::GetServerURI(nsACString& aResult)
{
  nsresult rv;
  rv = GetLocalStoreType(aResult);
  NS_ENSURE_SUCCESS(rv, rv);
  aResult.AppendLiteral("://");

  nsCString username;
  rv = GetUsername(username);
  if (NS_SUCCEEDED(rv) && !username.IsEmpty()) {
      nsCString escapedUsername;
      MsgEscapeString(username, nsINetUtil::ESCAPE_XALPHAS, escapedUsername);
      // not all servers have a username
      aResult.Append(escapedUsername);
      aResult.Append('@');
  }

  nsCString hostname;
  rv = GetHostName(hostname);
  if (NS_SUCCEEDED(rv) && !hostname.IsEmpty()) {
      nsCString escapedHostname;
      MsgEscapeString(hostname, nsINetUtil::ESCAPE_URL_PATH, escapedHostname);
      // not all servers have a hostname
      aResult.Append(escapedHostname);
  }
  return NS_OK;
}

// helper routine to create local folder on disk, if it doesn't exist.
nsresult
nsMsgIncomingServer::CreateLocalFolder(const nsAString& folderName)
{
  nsCOMPtr<nsIMsgFolder> rootFolder;
  nsresult rv = GetRootFolder(getter_AddRefs(rootFolder));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIMsgFolder> child;
  rv = rootFolder->GetChildNamed(folderName, getter_AddRefs(child));
  if (child)
    return NS_OK;
  nsCOMPtr<nsIMsgPluggableStore> msgStore;
  rv = GetMsgStore(getter_AddRefs(msgStore));
  NS_ENSURE_SUCCESS(rv, rv);
  return msgStore->CreateFolder(rootFolder, folderName, getter_AddRefs(child));
}

nsresult
nsMsgIncomingServer::CreateRootFolder()
{
  nsresult rv;
  // get the URI from the incoming server
  nsCString serverUri;
  rv = GetServerURI(serverUri);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIRDFService> rdf = do_GetService("@mozilla.org/rdf/rdf-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // get the corresponding RDF resource
  // RDF will create the server resource if it doesn't already exist
  nsCOMPtr<nsIRDFResource> serverResource;
  rv = rdf->GetResource(serverUri, getter_AddRefs(serverResource));
  NS_ENSURE_SUCCESS(rv, rv);

  // make incoming server know about its root server folder so we
  // can find sub-folders given an incoming server.
  m_rootFolder = do_QueryInterface(serverResource, &rv);
  return rv;
}

NS_IMETHODIMP
nsMsgIncomingServer::GetBoolValue(const char *prefname,
                                 bool *val)
{
  if (!mPrefBranch)
    return NS_ERROR_NOT_INITIALIZED;

  NS_ENSURE_ARG_POINTER(val);
  *val = false;

  if (NS_FAILED(mPrefBranch->GetBoolPref(prefname, val)))
    mDefPrefBranch->GetBoolPref(prefname, val);

  return NS_OK;
}

NS_IMETHODIMP
nsMsgIncomingServer::SetBoolValue(const char *prefname,
                                 bool val)
{
  if (!mPrefBranch)
    return NS_ERROR_NOT_INITIALIZED;

  bool defaultValue;
  nsresult rv = mDefPrefBranch->GetBoolPref(prefname, &defaultValue);

  if (NS_SUCCEEDED(rv) && val == defaultValue)
    mPrefBranch->ClearUserPref(prefname);
  else
    rv = mPrefBranch->SetBoolPref(prefname, val);

  return rv;
}

NS_IMETHODIMP
nsMsgIncomingServer::GetIntValue(const char *prefname,
                                int32_t *val)
{
  if (!mPrefBranch)
    return NS_ERROR_NOT_INITIALIZED;

  NS_ENSURE_ARG_POINTER(val);
  *val = 0;

  if (NS_FAILED(mPrefBranch->GetIntPref(prefname, val)))
    mDefPrefBranch->GetIntPref(prefname, val);

  return NS_OK;
}

NS_IMETHODIMP
nsMsgIncomingServer::GetFileValue(const char* aRelPrefName,
                                  const char* aAbsPrefName,
                                  nsIFile** aLocalFile)
{
  if (!mPrefBranch)
    return NS_ERROR_NOT_INITIALIZED;

  // Get the relative first
  nsCOMPtr<nsIRelativeFilePref> relFilePref;
  nsresult rv = mPrefBranch->GetComplexValue(aRelPrefName,
                                             NS_GET_IID(nsIRelativeFilePref),
                                             getter_AddRefs(relFilePref));
  if (relFilePref) {
    rv = relFilePref->GetFile(aLocalFile);
    NS_ASSERTION(*aLocalFile, "An nsIRelativeFilePref has no file.");
    if (NS_SUCCEEDED(rv))
      (*aLocalFile)->Normalize();
  } else {
    rv = mPrefBranch->GetComplexValue(aAbsPrefName,
                                      NS_GET_IID(nsIFile),
                                      reinterpret_cast<void**>(aLocalFile));
    if (NS_FAILED(rv))
      return rv;

    rv = NS_NewRelativeFilePref(*aLocalFile,
                                NS_LITERAL_CSTRING(NS_APP_USER_PROFILE_50_DIR),
                                getter_AddRefs(relFilePref));
    if (relFilePref)
      rv = mPrefBranch->SetComplexValue(aRelPrefName,
                                        NS_GET_IID(nsIRelativeFilePref),
                                        relFilePref);
  }

  return rv;
}

NS_IMETHODIMP
nsMsgIncomingServer::SetFileValue(const char* aRelPrefName,
                                  const char* aAbsPrefName,
                                  nsIFile* aLocalFile)
{
  if (!mPrefBranch)
    return NS_ERROR_NOT_INITIALIZED;

  // Write the relative path.
  nsCOMPtr<nsIRelativeFilePref> relFilePref;
  NS_NewRelativeFilePref(aLocalFile,
                         NS_LITERAL_CSTRING(NS_APP_USER_PROFILE_50_DIR),
                         getter_AddRefs(relFilePref));
  if (relFilePref) {
    nsresult rv = mPrefBranch->SetComplexValue(aRelPrefName,
                                               NS_GET_IID(nsIRelativeFilePref),
                                               relFilePref);
    if (NS_FAILED(rv))
      return rv;
  }
  return mPrefBranch->SetComplexValue(aAbsPrefName, NS_GET_IID(nsIFile), aLocalFile);
}

NS_IMETHODIMP
nsMsgIncomingServer::SetIntValue(const char *prefname,
                                 int32_t val)
{
  if (!mPrefBranch)
    return NS_ERROR_NOT_INITIALIZED;

  int32_t defaultVal;
  nsresult rv = mDefPrefBranch->GetIntPref(prefname, &defaultVal);

  if (NS_SUCCEEDED(rv) && defaultVal == val)
    mPrefBranch->ClearUserPref(prefname);
  else
    rv = mPrefBranch->SetIntPref(prefname, val);

  return rv;
}

NS_IMETHODIMP
nsMsgIncomingServer::GetCharValue(const char *prefname,
                                  nsACString& val)
{
  if (!mPrefBranch)
    return NS_ERROR_NOT_INITIALIZED;

  nsCString tmpVal;
  if (NS_FAILED(mPrefBranch->GetCharPref(prefname, getter_Copies(tmpVal))))
    mDefPrefBranch->GetCharPref(prefname, getter_Copies(tmpVal));
  val = tmpVal;
  return NS_OK;
}

NS_IMETHODIMP
nsMsgIncomingServer::GetUnicharValue(const char *prefname,
                                     nsAString& val)
{
  if (!mPrefBranch)
    return NS_ERROR_NOT_INITIALIZED;

  nsCOMPtr<nsISupportsString> supportsString;
  if (NS_FAILED(mPrefBranch->GetComplexValue(prefname,
                                             NS_GET_IID(nsISupportsString),
                                             getter_AddRefs(supportsString))))
    mDefPrefBranch->GetComplexValue(prefname,
                                    NS_GET_IID(nsISupportsString),
                                    getter_AddRefs(supportsString));

  if (supportsString)
    return supportsString->GetData(val);
  val.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
nsMsgIncomingServer::SetCharValue(const char *prefname,
                                  const nsACString& val)
{
  if (!mPrefBranch)
    return NS_ERROR_NOT_INITIALIZED;

  if (val.IsEmpty()) {
    mPrefBranch->ClearUserPref(prefname);
    return NS_OK;
  }

  nsCString defaultVal;
  nsresult rv = mDefPrefBranch->GetCharPref(prefname, getter_Copies(defaultVal));

  if (NS_SUCCEEDED(rv) && defaultVal.Equals(val))
    mPrefBranch->ClearUserPref(prefname);
  else
    rv = mPrefBranch->SetCharPref(prefname, nsCString(val).get());

  return rv;
}

NS_IMETHODIMP
nsMsgIncomingServer::SetUnicharValue(const char *prefname,
                                     const nsAString& val)
{
  if (!mPrefBranch)
    return NS_ERROR_NOT_INITIALIZED;

  if (val.IsEmpty()) {
    mPrefBranch->ClearUserPref(prefname);
    return NS_OK;
  }

  nsCOMPtr<nsISupportsString> supportsString;
  nsresult rv = mDefPrefBranch->GetComplexValue(prefname,
                                                NS_GET_IID(nsISupportsString),
                                                getter_AddRefs(supportsString));
  nsString defaultVal;
  if (NS_SUCCEEDED(rv) &&
      NS_SUCCEEDED(supportsString->GetData(defaultVal)) &&
      defaultVal.Equals(val))
    mPrefBranch->ClearUserPref(prefname);
  else {
    supportsString = do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID, &rv);
    if (supportsString) {
      supportsString->SetData(val);
      rv = mPrefBranch->SetComplexValue(prefname,
                                        NS_GET_IID(nsISupportsString),
                                        supportsString);
    }
  }

  return rv;
}

// pretty name is the display name to show to the user
NS_IMETHODIMP
nsMsgIncomingServer::GetPrettyName(nsAString& retval)
{
  nsresult rv = GetUnicharValue("name", retval);
  NS_ENSURE_SUCCESS(rv, rv);

  // if there's no name, then just return the hostname
  return retval.IsEmpty() ? GetConstructedPrettyName(retval) : rv;
}

NS_IMETHODIMP
nsMsgIncomingServer::SetPrettyName(const nsAString& value)
{
  SetUnicharValue("name", value);
  nsCOMPtr<nsIMsgFolder> rootFolder;
  GetRootFolder(getter_AddRefs(rootFolder));
  if (rootFolder)
    rootFolder->SetPrettyName(value);
  return NS_OK;
}


// construct the pretty name to show to the user if they haven't
// specified one. This should be overridden for news and mail.
NS_IMETHODIMP
nsMsgIncomingServer::GetConstructedPrettyName(nsAString& retval)
{
  nsCString username;
  nsresult rv = GetUsername(username);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!username.IsEmpty()) {
    CopyASCIItoUTF16(username, retval);
    retval.AppendLiteral(" on ");
  }

  nsCString hostname;
  rv = GetHostName(hostname);
  NS_ENSURE_SUCCESS(rv, rv);

  retval.Append(NS_ConvertASCIItoUTF16(hostname));
  return NS_OK;
}

NS_IMETHODIMP
nsMsgIncomingServer::ToString(nsAString& aResult)
{
  aResult.AssignLiteral("[nsIMsgIncomingServer: ");
  aResult.Append(NS_ConvertASCIItoUTF16(m_serverKey));
  aResult.AppendLiteral("]");
  return NS_OK;
}

NS_IMETHODIMP nsMsgIncomingServer::SetPassword(const nsACString& aPassword)
{
  m_password = aPassword;
  return NS_OK;
}

NS_IMETHODIMP nsMsgIncomingServer::GetPassword(nsACString& aPassword)
{
  aPassword = m_password;
  return NS_OK;
}

NS_IMETHODIMP nsMsgIncomingServer::GetServerRequiresPasswordForBiff(bool *aServerRequiresPasswordForBiff)
{
  NS_ENSURE_ARG_POINTER(aServerRequiresPasswordForBiff);
  *aServerRequiresPasswordForBiff = true;
  return NS_OK;
}

// This sets m_password if we find a password in the pw mgr.
nsresult nsMsgIncomingServer::GetPasswordWithoutUI()
{
  nsresult rv;
  nsCOMPtr<nsILoginManager> loginMgr(do_GetService(NS_LOGINMANAGER_CONTRACTID,
                                                   &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  // Get the current server URI
  nsCString currServerUri;
  rv = GetLocalStoreType(currServerUri);
  NS_ENSURE_SUCCESS(rv, rv);

  currServerUri.AppendLiteral("://");

  nsCString temp;
  rv = GetHostName(temp);
  NS_ENSURE_SUCCESS(rv, rv);

  currServerUri.Append(temp);

  NS_ConvertUTF8toUTF16 currServer(currServerUri);

  uint32_t numLogins = 0;
  nsILoginInfo** logins = nullptr;
  rv = loginMgr->FindLogins(&numLogins, currServer, EmptyString(),
                            currServer, &logins);

  // Login manager can produce valid fails, e.g. NS_ERROR_ABORT when a user
  // cancels the master password dialog. Therefore handle that here, but don't
  // warn about it.
  if (NS_FAILED(rv))
    return rv;

  // Don't abort here, if we didn't find any or failed, then we'll just have
  // to prompt.
  if (numLogins > 0)
  {
    nsCString serverCUsername;
    rv = GetUsername(serverCUsername);
    NS_ENSURE_SUCCESS(rv, rv);

    NS_ConvertUTF8toUTF16 serverUsername(serverCUsername);

    nsString username;
    for (uint32_t i = 0; i < numLogins; ++i)
    {
      rv = logins[i]->GetUsername(username);
      NS_ENSURE_SUCCESS(rv, rv);

      if (username.Equals(serverUsername))
      {
        nsString password;
        rv = logins[i]->GetPassword(password);
        NS_ENSURE_SUCCESS(rv, rv);

        m_password = NS_LossyConvertUTF16toASCII(password);
        break;
      }
    }
    NS_FREE_XPCOM_ISUPPORTS_POINTER_ARRAY(numLogins, logins);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsMsgIncomingServer::GetPasswordWithUI(const nsAString& aPromptMessage, const
                                       nsAString& aPromptTitle,
                                       nsIMsgWindow* aMsgWindow,
                                       nsACString& aPassword)
{
  nsresult rv = NS_OK;

  if (m_password.IsEmpty())
  {
    // let's see if we have the password in the password manager and
    // can avoid this prompting thing. This makes it easier to get embedders
    // to get up and running w/o a password prompting UI.
    rv = GetPasswordWithoutUI();
    // If GetPasswordWithoutUI returns NS_ERROR_ABORT, the most likely case
    // is the user canceled getting the master password, so just return
    // straight away, as they won't want to get prompted again.
    if (rv == NS_ERROR_ABORT)
      return NS_MSG_PASSWORD_PROMPT_CANCELLED;
  }
  if (m_password.IsEmpty())
  {
    nsCOMPtr<nsIAuthPrompt> dialog;
    // aMsgWindow is required if we need to prompt
    if (aMsgWindow)
    {
      rv = aMsgWindow->GetAuthPrompt(getter_AddRefs(dialog));
      NS_ENSURE_SUCCESS(rv, rv);
    }
    if (dialog)
    {
      // prompt the user for the password
      nsCString serverUri;
      rv = GetLocalStoreType(serverUri);
      NS_ENSURE_SUCCESS(rv, rv);

      serverUri.AppendLiteral("://");
      nsCString temp;
      rv = GetUsername(temp);
      NS_ENSURE_SUCCESS(rv, rv);

      if (!temp.IsEmpty())
      {
        nsCString escapedUsername;
        MsgEscapeString(temp, nsINetUtil::ESCAPE_XALPHAS, escapedUsername);
        serverUri.Append(escapedUsername);
        serverUri.Append('@');
      }

      rv = GetHostName(temp);
      NS_ENSURE_SUCCESS(rv, rv);

      serverUri.Append(temp);

      // we pass in the previously used password, if any, into PromptPassword
      // so that it will appear as ******. This means we can't use an nsString
      // and getter_Copies.
      PRUnichar *uniPassword = nullptr;
      if (!aPassword.IsEmpty())
        uniPassword = ToNewUnicode(NS_ConvertASCIItoUTF16(aPassword));

      bool okayValue = true;
      rv = dialog->PromptPassword(PromiseFlatString(aPromptTitle).get(),
                                  PromiseFlatString(aPromptMessage).get(),
                                  NS_ConvertASCIItoUTF16(serverUri).get(),
                                  nsIAuthPrompt::SAVE_PASSWORD_PERMANENTLY,
                                  &uniPassword, &okayValue);
      nsAutoString uniPasswordAdopted;
      uniPasswordAdopted.Adopt(uniPassword);
      NS_ENSURE_SUCCESS(rv, rv);

      if (!okayValue) // if the user pressed cancel, just return an empty string;
      {
        aPassword.Truncate();
        return NS_MSG_PASSWORD_PROMPT_CANCELLED;
      }

      // we got a password back...so remember it
      rv = SetPassword(NS_LossyConvertUTF16toASCII(uniPasswordAdopted));
      NS_ENSURE_SUCCESS(rv, rv);
    } // if we got a prompt dialog
    else
      return NS_ERROR_FAILURE;
  } // if the password is empty
  return GetPassword(aPassword);
}

NS_IMETHODIMP
nsMsgIncomingServer::ForgetPassword()
{
  nsresult rv;
  nsCOMPtr<nsILoginManager> loginMgr =
    do_GetService(NS_LOGINMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // Get the current server URI
  nsCString currServerUri;
  rv = GetLocalStoreType(currServerUri);
  NS_ENSURE_SUCCESS(rv, rv);

  currServerUri.AppendLiteral("://");

  nsCString temp;
  rv = GetHostName(temp);
  NS_ENSURE_SUCCESS(rv, rv);

  currServerUri.Append(temp);

  uint32_t count;
  nsILoginInfo** logins;

  NS_ConvertUTF8toUTF16 currServer(currServerUri);

  nsCString serverCUsername;
  rv = GetUsername(serverCUsername);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ConvertUTF8toUTF16 serverUsername(serverCUsername);

  rv = loginMgr->FindLogins(&count, currServer, EmptyString(),
                            currServer, &logins);
  NS_ENSURE_SUCCESS(rv, rv);

  // There should only be one-login stored for this url, however just in case
  // there isn't.
  nsString username;
  for (uint32_t i = 0; i < count; ++i)
  {
    if (NS_SUCCEEDED(logins[i]->GetUsername(username)) &&
        username.Equals(serverUsername))
    {
      // If this fails, just continue, we'll still want to remove the password
      // from our local cache.
      loginMgr->RemoveLogin(logins[i]);
    }
  }
  NS_FREE_XPCOM_ISUPPORTS_POINTER_ARRAY(count, logins);

  return SetPassword(EmptyCString());
}

NS_IMETHODIMP
nsMsgIncomingServer::ForgetSessionPassword()
{
  m_password.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
nsMsgIncomingServer::SetDefaultLocalPath(nsIFile *aDefaultLocalPath)
{
  nsresult rv;
  nsCOMPtr<nsIMsgProtocolInfo> protocolInfo;
  rv = getProtocolInfo(getter_AddRefs(protocolInfo));
  NS_ENSURE_SUCCESS(rv, rv);
  return protocolInfo->SetDefaultLocalPath(aDefaultLocalPath);
}

NS_IMETHODIMP
nsMsgIncomingServer::GetLocalPath(nsIFile **aLocalPath)
{
  nsresult rv;

  // if the local path has already been set, use it
  rv = GetFileValue("directory-rel", "directory", aLocalPath);
  if (NS_SUCCEEDED(rv) && *aLocalPath)
    return rv;

  // otherwise, create the path using the protocol info.
  // note we are using the
  // hostname, unless that directory exists.
// this should prevent all collisions.
  nsCOMPtr<nsIMsgProtocolInfo> protocolInfo;
  rv = getProtocolInfo(getter_AddRefs(protocolInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIFile> localPath;
  rv = protocolInfo->GetDefaultLocalPath(getter_AddRefs(localPath));
  NS_ENSURE_SUCCESS(rv, rv);
  localPath->Create(nsIFile::DIRECTORY_TYPE, 0755);

  nsCString hostname;
  rv = GetHostName(hostname);
  NS_ENSURE_SUCCESS(rv, rv);
  
  // set the leaf name to "dummy", and then call MakeUnique with a suggested leaf name
  rv = localPath->AppendNative(hostname);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = localPath->CreateUnique(nsIFile::DIRECTORY_TYPE, 0755);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = SetLocalPath(localPath);
  NS_ENSURE_SUCCESS(rv, rv);

  localPath.swap(*aLocalPath);
  return NS_OK;
}

NS_IMETHODIMP
nsMsgIncomingServer::GetMsgStore(nsIMsgPluggableStore **aMsgStore)
{
  NS_ENSURE_ARG_POINTER(aMsgStore);
  if (!m_msgStore)
  {
    nsCString storeContractID;
    nsresult rv;
    // We don't want there to be a default pref, I think, since
    // we can't change the default. We may want no pref to mean
    // berkeley store, and then set the store pref off of some sort
    // of default when creating a server. But we need to make sure
    // that we do always write a store pref.
    GetCharValue("storeContractID", storeContractID);
    if (storeContractID.IsEmpty())
    {
      storeContractID.Assign("@mozilla.org/msgstore/berkeleystore;1");
      SetCharValue("storeContractID", storeContractID);
    }
    // Right now, we just have one pluggable store per server. If we want
    // to support multiple, this pref could be a list of pluggable store
    // contract id's.
    m_msgStore = do_CreateInstance(storeContractID.get(), &rv);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  NS_IF_ADDREF(*aMsgStore = m_msgStore);
  return NS_OK;
}


NS_IMETHODIMP
nsMsgIncomingServer::SetLocalPath(nsIFile *aLocalPath)
{
  NS_ENSURE_ARG_POINTER(aLocalPath);
  aLocalPath->Create(nsIFile::DIRECTORY_TYPE, 0755);
  return SetFileValue("directory-rel", "directory", aLocalPath);
}

NS_IMETHODIMP
nsMsgIncomingServer::GetLocalStoreType(nsACString& aResult)
{
  NS_NOTYETIMPLEMENTED("nsMsgIncomingServer superclass not implementing GetLocalStoreType!");
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsMsgIncomingServer::GetAccountManagerChrome(nsAString& aResult)
{
  aResult.AssignLiteral("am-main.xul");
  return NS_OK;
}

NS_IMETHODIMP
nsMsgIncomingServer::Equals(nsIMsgIncomingServer *server, bool *_retval)
{
  nsresult rv;

  NS_ENSURE_ARG_POINTER(server);
  NS_ENSURE_ARG_POINTER(_retval);

  nsCString key1;
  nsCString key2;

  rv = GetKey(key1);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = server->GetKey(key2);
  NS_ENSURE_SUCCESS(rv, rv);

  // compare the server keys
  *_retval = key1.Equals(key2, nsCaseInsensitiveCStringComparator());

  return rv;
}

NS_IMETHODIMP
nsMsgIncomingServer::ClearAllValues()
{
  if (!mPrefBranch)
    return NS_ERROR_NOT_INITIALIZED;

  return mPrefBranch->DeleteBranch("");
}

NS_IMETHODIMP
nsMsgIncomingServer::RemoveFiles()
{
  // IMPORTANT, see bug #77652
  // don't turn this code on yet.  we don't inform the user that
  // we are going to be deleting the directory, and they might have
  // tweaked their localPath pref for this server to point to
  // somewhere they didn't want deleted.
  // until we tell them, we shouldn't do the delete.
  nsCString deferredToAccount;
  GetCharValue("deferred_to_account", deferredToAccount);
  bool isDeferredTo = true;
  GetIsDeferredTo(&isDeferredTo);
  if (!deferredToAccount.IsEmpty() || isDeferredTo)
  {
    NS_ASSERTION(false, "shouldn't remove files for a deferred account");
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr <nsIFile> localPath;
  nsresult rv = GetLocalPath(getter_AddRefs(localPath));
  NS_ENSURE_SUCCESS(rv, rv);
  return localPath->Remove(true);
}

NS_IMETHODIMP
nsMsgIncomingServer::SetFilterList(nsIMsgFilterList *aFilterList)
{
  mFilterList = aFilterList;
  return NS_OK;
}

NS_IMETHODIMP
nsMsgIncomingServer::GetFilterList(nsIMsgWindow *aMsgWindow, nsIMsgFilterList **aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  if (!mFilterList)
  {
      nsCOMPtr<nsIMsgFolder> msgFolder;
      // use GetRootFolder so for deferred pop3 accounts, we'll get the filters
      // file from the deferred account, not the deferred to account,
      // so that filters will still be per-server.
      nsresult rv = GetRootFolder(getter_AddRefs(msgFolder));
      NS_ENSURE_SUCCESS(rv, rv);

      nsCString filterType;
      rv = GetCharValue("filter.type", filterType);
      NS_ENSURE_SUCCESS(rv, rv);

      if (!filterType.IsEmpty() && !filterType.EqualsLiteral("default"))
      {
        nsCAutoString contractID("@mozilla.org/filterlist;1?type=");
        contractID += filterType;
        ToLowerCase(contractID);
        mFilterList = do_CreateInstance(contractID.get(), &rv);
        NS_ENSURE_SUCCESS(rv, rv);

        rv = mFilterList->SetFolder(msgFolder);
        NS_ENSURE_SUCCESS(rv, rv);

        NS_ADDREF(*aResult = mFilterList);
        return NS_OK;
      }

      // The default case, a local folder, is a bit special. It requires
      // more initialization.

      nsCOMPtr<nsIFile> thisFolder;
      rv = msgFolder->GetFilePath(getter_AddRefs(thisFolder));
      NS_ENSURE_SUCCESS(rv, rv);

      mFilterFile = do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = mFilterFile->InitWithFile(thisFolder);
      NS_ENSURE_SUCCESS(rv, rv);

      mFilterFile->AppendNative(NS_LITERAL_CSTRING("msgFilterRules.dat"));

      bool fileExists;
      mFilterFile->Exists(&fileExists);
      if (!fileExists)
      {
        nsCOMPtr<nsIFile> oldFilterFile = do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv);
        NS_ENSURE_SUCCESS(rv, rv);
        rv = oldFilterFile->InitWithFile(thisFolder);
        NS_ENSURE_SUCCESS(rv, rv);
        oldFilterFile->AppendNative(NS_LITERAL_CSTRING("rules.dat"));

        oldFilterFile->Exists(&fileExists);
        if (fileExists)  //copy rules.dat --> msgFilterRules.dat
        {
          rv = oldFilterFile->CopyToNative(thisFolder, NS_LITERAL_CSTRING("msgFilterRules.dat"));
          NS_ENSURE_SUCCESS(rv, rv);
        }
      }
      nsCOMPtr<nsIMsgFilterService> filterService =
          do_GetService(NS_MSGFILTERSERVICE_CONTRACTID, &rv);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = filterService->OpenFilterList(mFilterFile, msgFolder, aMsgWindow, getter_AddRefs(mFilterList));
      NS_ENSURE_SUCCESS(rv, rv);
  }

  NS_IF_ADDREF(*aResult = mFilterList);
  return NS_OK;
}

NS_IMETHODIMP
nsMsgIncomingServer::SetEditableFilterList(nsIMsgFilterList *aEditableFilterList)
{
  mEditableFilterList = aEditableFilterList;
  return NS_OK;
}

NS_IMETHODIMP
nsMsgIncomingServer::GetEditableFilterList(nsIMsgWindow *aMsgWindow, nsIMsgFilterList **aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  if (!mEditableFilterList)
  {
    bool editSeparate;
    nsresult rv = GetBoolValue("filter.editable.separate", &editSeparate);
    if (NS_FAILED(rv) || !editSeparate)
      return GetFilterList(aMsgWindow, aResult);

    nsCString filterType;
    rv = GetCharValue("filter.editable.type", filterType);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCAutoString contractID("@mozilla.org/filterlist;1?type=");
    contractID += filterType;
    ToLowerCase(contractID);
    mEditableFilterList = do_CreateInstance(contractID.get(), &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIMsgFolder> msgFolder;
    // use GetRootFolder so for deferred pop3 accounts, we'll get the filters
    // file from the deferred account, not the deferred to account,
    // so that filters will still be per-server.
    rv = GetRootFolder(getter_AddRefs(msgFolder));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mEditableFilterList->SetFolder(msgFolder);
    NS_ENSURE_SUCCESS(rv, rv);

    NS_ADDREF(*aResult = mEditableFilterList);
    return NS_OK;
  }

  NS_IF_ADDREF(*aResult = mEditableFilterList);
  return NS_OK;
}

// If the hostname contains ':' (like hostname:1431)
// then parse and set the port number.
nsresult
nsMsgIncomingServer::InternalSetHostName(const nsACString& aHostname, const char * prefName)
{
  nsCString hostname;
  hostname = aHostname;
  if (MsgCountChar(hostname, ':') == 1)
  {
    int32_t colonPos = hostname.FindChar(':');
    nsCAutoString portString(Substring(hostname, colonPos));
    hostname.SetLength(colonPos);
    nsresult err;
    int32_t port = portString.ToInteger(&err);
    if (NS_SUCCEEDED(err))
      SetPort(port);
  }
  return SetCharValue(prefName, hostname);
}

NS_IMETHODIMP
nsMsgIncomingServer::OnUserOrHostNameChanged(const nsACString& oldName,
                                             const nsACString& newName,
                                             bool hostnameChanged)
{
  nsresult rv;

  // 1. Reset password so that users are prompted for new password for the new user/host.
  ForgetPassword();

  // 2. Let the derived class close all cached connection to the old host.
  CloseCachedConnections();

  // 3. Notify any listeners for account server changes.
  nsCOMPtr<nsIMsgAccountManager> accountManager = do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = accountManager->NotifyServerChanged(this);
  NS_ENSURE_SUCCESS(rv, rv);

  // 4. Lastly, replace all occurrences of old name in the acct name with the new one.
  nsString acctName;
  rv = GetPrettyName(acctName);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ENSURE_FALSE(acctName.IsEmpty(), NS_OK);

  // exit if new name contains @ then better do not update the account name
  if (!hostnameChanged && (newName.FindChar('@') != kNotFound))
    return NS_OK;

  int32_t atPos = acctName.FindChar('@');

  // get previous username and hostname
  nsCString userName, hostName;
  if (hostnameChanged)
  {
    rv = GetRealUsername(userName);
    NS_ENSURE_SUCCESS(rv, rv);
    hostName.Assign(oldName);
  }
  else
  {
    userName.Assign(oldName);
    rv = GetRealHostName(hostName);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // switch corresponding part of the account name to the new name...
  if (!hostnameChanged && (atPos != kNotFound))
  {
    // ...if username changed and the previous username was equal to the part
    // of the account name before @
    if (StringHead(acctName, atPos).Equals(NS_ConvertASCIItoUTF16(userName)))
      acctName.Replace(0, userName.Length(), NS_ConvertASCIItoUTF16(newName));
  }
  if (hostnameChanged)
  {
    // ...if hostname changed and the previous hostname was equal to the part
    // of the account name after @, or to the whole account name
    if (atPos == kNotFound)
      atPos = 0;
    else
      atPos += 1;
    if (Substring(acctName, atPos).Equals(NS_ConvertASCIItoUTF16(hostName))) {
      acctName.Replace(atPos, acctName.Length() - atPos,
                       NS_ConvertASCIItoUTF16(newName));
    }
  }

  return SetPrettyName(acctName);
}

NS_IMETHODIMP
nsMsgIncomingServer::SetHostName(const nsACString& aHostname)
{
  return (InternalSetHostName(aHostname, "hostname"));
}

// SetRealHostName() is called only when the server name is changed from the
// UI (Account Settings page).  No one should call it in any circumstances.
NS_IMETHODIMP
nsMsgIncomingServer::SetRealHostName(const nsACString& aHostname)
{
  nsCString oldName;
  nsresult rv = GetRealHostName(oldName);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = InternalSetHostName(aHostname, "realhostname");

  // A few things to take care of if we're changing the hostname.
  if (!aHostname.Equals(oldName, nsCaseInsensitiveCStringComparator()))
    rv = OnUserOrHostNameChanged(oldName, aHostname, true);
  return rv;
}

NS_IMETHODIMP
nsMsgIncomingServer::GetHostName(nsACString& aResult)
{
  nsresult rv;
  rv = GetCharValue("hostname", aResult);
  if (MsgCountChar(aResult, ':') == 1)
  {
    // gack, we need to reformat the hostname - SetHostName will do that
    SetHostName(aResult);
    rv = GetCharValue("hostname", aResult);
  }
  return rv;
}

NS_IMETHODIMP
nsMsgIncomingServer::GetRealHostName(nsACString& aResult)
{
  // If 'realhostname' is set (was changed) then use it, otherwise use 'hostname'
  nsresult rv;
  rv = GetCharValue("realhostname", aResult);
  NS_ENSURE_SUCCESS(rv, rv);
  
  if (aResult.IsEmpty())
    return GetHostName(aResult);

  if (MsgCountChar(aResult, ':') == 1)
  {
    SetRealHostName(aResult);
    rv = GetCharValue("realhostname", aResult);
  }

  return rv;
}

NS_IMETHODIMP
nsMsgIncomingServer::GetRealUsername(nsACString& aResult)
{
  // If 'realuserName' is set (was changed) then use it, otherwise use 'userName'
  nsresult rv;
  rv = GetCharValue("realuserName", aResult);
  NS_ENSURE_SUCCESS(rv, rv);
  return aResult.IsEmpty() ? GetUsername(aResult) : rv;
}

NS_IMETHODIMP
nsMsgIncomingServer::SetRealUsername(const nsACString& aUsername)
{
  // Need to take care of few things if we're changing the username.
  nsCString oldName;
  nsresult rv = GetRealUsername(oldName);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = SetCharValue("realuserName", aUsername);
  if (!oldName.Equals(aUsername))
    rv = OnUserOrHostNameChanged(oldName, aUsername, false);
  return rv;
}

#define BIFF_PREF_NAME "check_new_mail"

NS_IMETHODIMP
nsMsgIncomingServer::GetDoBiff(bool *aDoBiff)
{
  NS_ENSURE_ARG_POINTER(aDoBiff);

  if (!mPrefBranch)
    return NS_ERROR_NOT_INITIALIZED;

  nsresult rv;

  rv = mPrefBranch->GetBoolPref(BIFF_PREF_NAME, aDoBiff);
  if (NS_SUCCEEDED(rv))
    return rv;

  // if the pref isn't set, use the default
  // value based on the protocol
  nsCOMPtr<nsIMsgProtocolInfo> protocolInfo;
  rv = getProtocolInfo(getter_AddRefs(protocolInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = protocolInfo->GetDefaultDoBiff(aDoBiff);
  // note, don't call SetDoBiff()
  // since we keep changing our minds on
  // if biff should be on or off, let's keep the ability
  // to change the default in future builds.
  // if we call SetDoBiff() here, it will be in the users prefs.
  // and we can't do anything after that.
  return rv;
}

NS_IMETHODIMP
nsMsgIncomingServer::SetDoBiff(bool aDoBiff)
{
  if (!mPrefBranch)
    return NS_ERROR_NOT_INITIALIZED;

  return mPrefBranch->SetBoolPref(BIFF_PREF_NAME, aDoBiff);
}

NS_IMETHODIMP
nsMsgIncomingServer::GetPort(int32_t *aPort)
{
  NS_ENSURE_ARG_POINTER(aPort);

  nsresult rv;
  rv = GetIntValue("port", aPort);
  // We can't use a port of 0, because the URI parsing code fails.
  if (*aPort != PORT_NOT_SET && *aPort)
    return rv;

  // if the port isn't set, use the default
  // port based on the protocol
  nsCOMPtr<nsIMsgProtocolInfo> protocolInfo;
  rv = getProtocolInfo(getter_AddRefs(protocolInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  int32_t socketType;
  rv = GetSocketType(&socketType);
  NS_ENSURE_SUCCESS(rv, rv);
  bool useSSLPort = (socketType == nsMsgSocketType::SSL);
  return protocolInfo->GetDefaultServerPort(useSSLPort, aPort);
}

NS_IMETHODIMP
nsMsgIncomingServer::SetPort(int32_t aPort)
{
  nsresult rv;

  nsCOMPtr<nsIMsgProtocolInfo> protocolInfo;
  rv = getProtocolInfo(getter_AddRefs(protocolInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  int32_t socketType;
  rv = GetSocketType(&socketType);
  NS_ENSURE_SUCCESS(rv, rv);
  bool useSSLPort = (socketType == nsMsgSocketType::SSL);

  int32_t defaultPort;
  protocolInfo->GetDefaultServerPort(useSSLPort, &defaultPort);
  return SetIntValue("port", aPort == defaultPort ? PORT_NOT_SET : aPort);
}

nsresult
nsMsgIncomingServer::getProtocolInfo(nsIMsgProtocolInfo **aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  nsresult rv;

  nsCString type;
  rv = GetType(type);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString contractid(NS_MSGPROTOCOLINFO_CONTRACTID_PREFIX);
  contractid.Append(type);

  nsCOMPtr<nsIMsgProtocolInfo> protocolInfo = do_GetService(contractid.get(), &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  protocolInfo.swap(*aResult);
  return NS_OK;
}

NS_IMETHODIMP nsMsgIncomingServer::GetRetentionSettings(nsIMsgRetentionSettings **settings)
{
  NS_ENSURE_ARG_POINTER(settings);
  nsMsgRetainByPreference retainByPreference;
  int32_t daysToKeepHdrs = 0;
  int32_t numHeadersToKeep = 0;
  bool keepUnreadMessagesOnly = false;
  int32_t daysToKeepBodies = 0;
  bool cleanupBodiesByDays = false;
  bool applyToFlaggedMessages = false;
  nsresult rv = NS_OK;
  // Create an empty retention settings object,
  // get the settings from the server prefs, and init the object from the prefs.
  nsCOMPtr <nsIMsgRetentionSettings> retentionSettings =
     do_CreateInstance(NS_MSG_RETENTIONSETTINGS_CONTRACTID);
  if (retentionSettings)
  {
    rv = GetBoolValue("keepUnreadOnly", &keepUnreadMessagesOnly);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = GetIntValue("retainBy", (int32_t*) &retainByPreference);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = GetIntValue("numHdrsToKeep", &numHeadersToKeep);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = GetIntValue("daysToKeepHdrs", &daysToKeepHdrs);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = GetIntValue("daysToKeepBodies", &daysToKeepBodies);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = GetBoolValue("cleanupBodies", &cleanupBodiesByDays);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = GetBoolValue("applyToFlaggedMessages", &applyToFlaggedMessages);
    NS_ENSURE_SUCCESS(rv, rv);
    retentionSettings->SetRetainByPreference(retainByPreference);
    retentionSettings->SetNumHeadersToKeep((uint32_t) numHeadersToKeep);
    retentionSettings->SetKeepUnreadMessagesOnly(keepUnreadMessagesOnly);
    retentionSettings->SetDaysToKeepBodies(daysToKeepBodies);
    retentionSettings->SetDaysToKeepHdrs(daysToKeepHdrs);
    retentionSettings->SetCleanupBodiesByDays(cleanupBodiesByDays);
    retentionSettings->SetApplyToFlaggedMessages(applyToFlaggedMessages);
  }
  else
    rv = NS_ERROR_OUT_OF_MEMORY;
  NS_IF_ADDREF(*settings = retentionSettings);
  return rv;
}

NS_IMETHODIMP nsMsgIncomingServer::SetRetentionSettings(nsIMsgRetentionSettings *settings)
{
  nsMsgRetainByPreference retainByPreference;
  uint32_t daysToKeepHdrs = 0;
  uint32_t numHeadersToKeep = 0;
  bool keepUnreadMessagesOnly = false;
  uint32_t daysToKeepBodies = 0;
  bool cleanupBodiesByDays = false;
  bool applyToFlaggedMessages = false;
  settings->GetRetainByPreference(&retainByPreference);
  settings->GetNumHeadersToKeep(&numHeadersToKeep);
  settings->GetKeepUnreadMessagesOnly(&keepUnreadMessagesOnly);
  settings->GetDaysToKeepBodies(&daysToKeepBodies);
  settings->GetDaysToKeepHdrs(&daysToKeepHdrs);
  settings->GetCleanupBodiesByDays(&cleanupBodiesByDays);
  settings->GetApplyToFlaggedMessages(&applyToFlaggedMessages);
  nsresult rv = SetBoolValue("keepUnreadOnly", keepUnreadMessagesOnly);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = SetIntValue("retainBy", retainByPreference);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = SetIntValue("numHdrsToKeep", numHeadersToKeep);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = SetIntValue("daysToKeepHdrs", daysToKeepHdrs);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = SetIntValue("daysToKeepBodies", daysToKeepBodies);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = SetBoolValue("cleanupBodies", cleanupBodiesByDays);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = SetBoolValue("applyToFlaggedMessages", applyToFlaggedMessages);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

NS_IMETHODIMP
nsMsgIncomingServer::GetDisplayStartupPage(bool *displayStartupPage)
{
  NS_ENSURE_ARG_POINTER(displayStartupPage);
  *displayStartupPage = m_displayStartupPage;
  return NS_OK;
}

NS_IMETHODIMP
nsMsgIncomingServer::SetDisplayStartupPage(bool displayStartupPage)
{
  m_displayStartupPage = displayStartupPage;
  return NS_OK;
}


NS_IMETHODIMP nsMsgIncomingServer::GetDownloadSettings(nsIMsgDownloadSettings **settings)
{
  NS_ENSURE_ARG_POINTER(settings);
  bool downloadUnreadOnly = false;
  bool downloadByDate = false;
  uint32_t ageLimitOfMsgsToDownload = 0;
  nsresult rv = NS_OK;
  if (!m_downloadSettings)
  {
    m_downloadSettings = do_CreateInstance(NS_MSG_DOWNLOADSETTINGS_CONTRACTID);
    if (m_downloadSettings)
    {
      rv = GetBoolValue("downloadUnreadOnly", &downloadUnreadOnly);
      rv = GetBoolValue("downloadByDate", &downloadByDate);
      rv = GetIntValue("ageLimit", (int32_t *) &ageLimitOfMsgsToDownload);
      m_downloadSettings->SetDownloadUnreadOnly(downloadUnreadOnly);
      m_downloadSettings->SetDownloadByDate(downloadByDate);
      m_downloadSettings->SetAgeLimitOfMsgsToDownload(ageLimitOfMsgsToDownload);
    }
    else
      rv = NS_ERROR_OUT_OF_MEMORY;
    // Create an empty download settings object,
    // get the settings from the server prefs, and init the object from the prefs.
  }
  NS_IF_ADDREF(*settings = m_downloadSettings);
  return rv;
}

NS_IMETHODIMP nsMsgIncomingServer::SetDownloadSettings(nsIMsgDownloadSettings *settings)
{
  m_downloadSettings = settings;
  bool downloadUnreadOnly = false;
  bool downloadByDate = false;
  uint32_t ageLimitOfMsgsToDownload = 0;
  m_downloadSettings->GetDownloadUnreadOnly(&downloadUnreadOnly);
  m_downloadSettings->GetDownloadByDate(&downloadByDate);
  m_downloadSettings->GetAgeLimitOfMsgsToDownload(&ageLimitOfMsgsToDownload);
  nsresult rv = SetBoolValue("downloadUnreadOnly", downloadUnreadOnly);
  NS_ENSURE_SUCCESS(rv, rv);
  SetBoolValue("downloadByDate", downloadByDate);
  return SetIntValue("ageLimit", ageLimitOfMsgsToDownload);
}

NS_IMETHODIMP
nsMsgIncomingServer::GetSupportsDiskSpace(bool *aSupportsDiskSpace)
{
  NS_ENSURE_ARG_POINTER(aSupportsDiskSpace);
  *aSupportsDiskSpace = true;
  return NS_OK;
}

NS_IMETHODIMP
nsMsgIncomingServer::GetOfflineSupportLevel(int32_t *aSupportLevel)
{
  NS_ENSURE_ARG_POINTER(aSupportLevel);
  nsresult rv;

  rv = GetIntValue("offline_support_level", aSupportLevel);
  if (*aSupportLevel == OFFLINE_SUPPORT_LEVEL_UNDEFINED)
    *aSupportLevel = OFFLINE_SUPPORT_LEVEL_NONE;
  return NS_OK;
}

NS_IMETHODIMP
nsMsgIncomingServer::SetOfflineSupportLevel(int32_t aSupportLevel)
{
  SetIntValue("offline_support_level", aSupportLevel);
  return NS_OK;
}
#define BASE_MSGS_URL       "chrome://messenger/locale/messenger.properties"

NS_IMETHODIMP nsMsgIncomingServer::DisplayOfflineMsg(nsIMsgWindow *aMsgWindow)
{
  NS_ENSURE_ARG_POINTER(aMsgWindow);

  nsCOMPtr<nsIStringBundleService> bundleService =
    mozilla::services::GetStringBundleService();
  NS_ENSURE_TRUE(bundleService, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsIStringBundle> bundle;
  nsresult rv = bundleService->CreateBundle(BASE_MSGS_URL, getter_AddRefs(bundle));
  NS_ENSURE_SUCCESS(rv, rv);
  if (bundle)
  {
    nsString errorMsgTitle;
    nsString errorMsgBody;
    bundle->GetStringFromName(NS_LITERAL_STRING("nocachedbodybody").get(), getter_Copies(errorMsgBody));
    bundle->GetStringFromName(NS_LITERAL_STRING("nocachedbodytitle").get(),  getter_Copies(errorMsgTitle));
    aMsgWindow->DisplayHTMLInMessagePane(errorMsgTitle, errorMsgBody, true);
  }
  
  return NS_OK;
}

// Called only during the migration process. A unique name is generated for the
// migrated account.
NS_IMETHODIMP
nsMsgIncomingServer::GeneratePrettyNameForMigration(nsAString& aPrettyName)
{
/**
   * 4.x had provisions for multiple imap servers to be maintained under
   * single identity. So, when migrated each of those server accounts need
   * to be represented by unique account name. nsImapIncomingServer will
   * override the implementation for this to do the right thing.
*/
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMsgIncomingServer::GetFilterScope(nsMsgSearchScopeValue *filterScope)
{
  NS_ENSURE_ARG_POINTER(filterScope);
  *filterScope = nsMsgSearchScope::offlineMailFilter;
  return NS_OK;
}

NS_IMETHODIMP
nsMsgIncomingServer::GetSearchScope(nsMsgSearchScopeValue *searchScope)
{
  NS_ENSURE_ARG_POINTER(searchScope);
  *searchScope = nsMsgSearchScope::offlineMail;
  return NS_OK;
}

NS_IMETHODIMP
nsMsgIncomingServer::GetIsSecure(bool *aIsSecure)
{
  NS_ENSURE_ARG_POINTER(aIsSecure);
  int32_t socketType;
  nsresult rv = GetSocketType(&socketType);
  NS_ENSURE_SUCCESS(rv,rv);
  *aIsSecure = (socketType == nsMsgSocketType::alwaysSTARTTLS ||
                socketType == nsMsgSocketType::SSL);
  return NS_OK;
}

// use the convenience macros to implement the accessors
NS_IMPL_SERVERPREF_STR(nsMsgIncomingServer, Username, "userName")
NS_IMPL_SERVERPREF_INT(nsMsgIncomingServer, AuthMethod, "authMethod")
NS_IMPL_SERVERPREF_INT(nsMsgIncomingServer, BiffMinutes, "check_time")
NS_IMPL_SERVERPREF_STR(nsMsgIncomingServer, Type, "type")
NS_IMPL_SERVERPREF_BOOL(nsMsgIncomingServer, DownloadOnBiff, "download_on_biff")
NS_IMPL_SERVERPREF_BOOL(nsMsgIncomingServer, Valid, "valid")
NS_IMPL_SERVERPREF_BOOL(nsMsgIncomingServer, EmptyTrashOnExit,
                        "empty_trash_on_exit")
NS_IMPL_SERVERPREF_BOOL(nsMsgIncomingServer, CanDelete, "canDelete")
NS_IMPL_SERVERPREF_BOOL(nsMsgIncomingServer, LoginAtStartUp, "login_at_startup")
NS_IMPL_SERVERPREF_BOOL(nsMsgIncomingServer,
                        DefaultCopiesAndFoldersPrefsToServer,
                        "allows_specialfolders_usage")

NS_IMPL_SERVERPREF_BOOL(nsMsgIncomingServer,
                        CanCreateFoldersOnServer,
                        "canCreateFolders")

NS_IMPL_SERVERPREF_BOOL(nsMsgIncomingServer,
                        CanFileMessagesOnServer,
                        "canFileMessages")

NS_IMPL_SERVERPREF_BOOL(nsMsgIncomingServer,
      LimitOfflineMessageSize,
      "limit_offline_message_size")

NS_IMPL_SERVERPREF_INT(nsMsgIncomingServer, MaxMessageSize, "max_size")

NS_IMPL_SERVERPREF_INT(nsMsgIncomingServer, IncomingDuplicateAction, "dup_action")

NS_IMPL_SERVERPREF_BOOL(nsMsgIncomingServer, Hidden, "hidden")

NS_IMETHODIMP nsMsgIncomingServer::GetSocketType(int32_t *aSocketType)
{
  if (!mPrefBranch)
    return NS_ERROR_NOT_INITIALIZED;

  nsresult rv = mPrefBranch->GetIntPref("socketType", aSocketType);

  // socketType is set to default value. Look at isSecure setting
  if (NS_FAILED(rv))
  {
    bool isSecure;
    rv = mPrefBranch->GetBoolPref("isSecure", &isSecure);
    if (NS_SUCCEEDED(rv) && isSecure)
    {
      *aSocketType = nsMsgSocketType::SSL;
      // don't call virtual method in case overrides call GetSocketType
      nsMsgIncomingServer::SetSocketType(*aSocketType);
    }
    else
    {
      if (!mDefPrefBranch)
        return NS_ERROR_NOT_INITIALIZED;
      rv = mDefPrefBranch->GetIntPref("socketType", aSocketType);
      if (NS_FAILED(rv))
        *aSocketType = nsMsgSocketType::plain;
    }
  }
  return rv;
}

NS_IMETHODIMP nsMsgIncomingServer::SetSocketType(int32_t aSocketType)
{
  if (!mPrefBranch)
    return NS_ERROR_NOT_INITIALIZED;

  int32_t socketType = nsMsgSocketType::plain;
  mPrefBranch->GetIntPref("socketType", &socketType);

  nsresult rv = mPrefBranch->SetIntPref("socketType", aSocketType);
  NS_ENSURE_SUCCESS(rv, rv);

  bool isSecureOld = (socketType == nsMsgSocketType::alwaysSTARTTLS ||
                        socketType == nsMsgSocketType::SSL);
  bool isSecureNew = (aSocketType == nsMsgSocketType::alwaysSTARTTLS ||
                        aSocketType == nsMsgSocketType::SSL);
  if ((isSecureOld != isSecureNew) && m_rootFolder) {
    nsCOMPtr <nsIAtom> isSecureAtom = MsgGetAtom("isSecure");
    m_rootFolder->NotifyBoolPropertyChanged(isSecureAtom,
                                            isSecureOld, isSecureNew);
  }
  return NS_OK;
}

// Check if the password is available and return a boolean indicating whether
// it is being authenticated or not.
NS_IMETHODIMP
nsMsgIncomingServer::GetPasswordPromptRequired(bool *aPasswordIsRequired)
{
  NS_ENSURE_ARG_POINTER(aPasswordIsRequired);
  *aPasswordIsRequired = true;

  // If the password is not even required for biff we don't need to check any further
  nsresult rv = GetServerRequiresPasswordForBiff(aPasswordIsRequired);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!*aPasswordIsRequired)
    return NS_OK;

  // If the password is empty, check to see if it is stored and to be retrieved
  if (m_password.IsEmpty())
    (void)GetPasswordWithoutUI();

  *aPasswordIsRequired = m_password.IsEmpty();
  return rv;
}

NS_IMETHODIMP nsMsgIncomingServer::ConfigureTemporaryFilters(nsIMsgFilterList *aFilterList)
{
  nsresult rv = ConfigureTemporaryReturnReceiptsFilter(aFilterList);
  if (NS_FAILED(rv)) // shut up warnings...
    return rv;
  return ConfigureTemporaryServerSpamFilters(aFilterList);
}

nsresult
nsMsgIncomingServer::ConfigureTemporaryServerSpamFilters(nsIMsgFilterList *filterList)
{
  nsCOMPtr<nsISpamSettings> spamSettings;
  nsresult rv = GetSpamSettings(getter_AddRefs(spamSettings));
  NS_ENSURE_SUCCESS(rv, rv);

  bool useServerFilter;
  rv = spamSettings->GetUseServerFilter(&useServerFilter);
  NS_ENSURE_SUCCESS(rv, rv);

  // if we aren't configured to use server filters, then return early.
  if (!useServerFilter)
    return NS_OK;

  // For performance reasons, we'll handle clearing of filters if the user turns
  // off the server-side filters from the junk mail controls, in the junk mail controls.
  nsCAutoString serverFilterName;
  spamSettings->GetServerFilterName(serverFilterName);
  if (serverFilterName.IsEmpty())
    return NS_OK;
  int32_t serverFilterTrustFlags = 0;
  (void) spamSettings->GetServerFilterTrustFlags(&serverFilterTrustFlags);
  if (!serverFilterTrustFlags)
    return NS_OK;
  // check if filters have been setup already.
  nsAutoString yesFilterName, noFilterName;
  CopyASCIItoUTF16(serverFilterName, yesFilterName);
  yesFilterName.AppendLiteral("Yes");

  CopyASCIItoUTF16(serverFilterName, noFilterName);
  noFilterName.AppendLiteral("No");

  nsCOMPtr<nsIMsgFilter> newFilter;
  (void) filterList->GetFilterNamed(yesFilterName,
                                  getter_AddRefs(newFilter));

  if (!newFilter)
    (void) filterList->GetFilterNamed(noFilterName,
                                  getter_AddRefs(newFilter));
  if (newFilter)
    return NS_OK;

  nsCOMPtr<nsIFile> file;
  spamSettings->GetServerFilterFile(getter_AddRefs(file));

  // it's possible that we can no longer find the sfd file (i.e. the user disabled an extnsion that
  // was supplying the .sfd file.
  if (!file)
    return NS_OK;

  nsCOMPtr<nsIMsgFilterService> filterService = do_GetService(NS_MSGFILTERSERVICE_CONTRACTID, &rv);
  nsCOMPtr<nsIMsgFilterList> serverFilterList;

  rv = filterService->OpenFilterList(file, NULL, NULL, getter_AddRefs(serverFilterList));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = serverFilterList->GetFilterNamed(yesFilterName,
                                  getter_AddRefs(newFilter));
  if (newFilter && serverFilterTrustFlags & nsISpamSettings::TRUST_POSITIVES)
  {
    newFilter->SetTemporary(true);
    // check if we're supposed to move junk mail to junk folder; if so,
    // add filter action to do so.

    /*
     * We don't want this filter to activate on messages that have
     *  been marked by the user as not spam. This occurs when messages that
     *  were marked as good are moved back into the inbox. But to
     *  do this with a filter, we have to add a boolean term. That requires
     *  that we rewrite the existing filter search terms to group them.
     */

    // get the list of search terms from the filter
    nsCOMPtr<nsISupportsArray> searchTerms;
    rv = newFilter->GetSearchTerms(getter_AddRefs(searchTerms));
    NS_ENSURE_SUCCESS(rv, rv);
    uint32_t count = 0;
    searchTerms->Count(&count);
    if (count > 1) // don't need to group a single term
    {
      // beginGrouping the first term, and endGrouping the last term
      nsCOMPtr<nsIMsgSearchTerm> firstTerm(do_QueryElementAt(searchTerms,
                                                             0, &rv));
      NS_ENSURE_SUCCESS(rv,rv);
      firstTerm->SetBeginsGrouping(true);

      nsCOMPtr<nsIMsgSearchTerm> lastTerm(do_QueryElementAt(searchTerms,
                                                            count - 1, &rv));
      NS_ENSURE_SUCCESS(rv,rv);
      lastTerm->SetEndsGrouping(true);
    }

    // Create a new term, checking if the user set junk status. The term will
    // search for junkscoreorigin != "user"
    nsCOMPtr<nsIMsgSearchTerm> searchTerm;
    rv = newFilter->CreateTerm(getter_AddRefs(searchTerm));
    NS_ENSURE_SUCCESS(rv, rv);

    searchTerm->SetAttrib(nsMsgSearchAttrib::JunkScoreOrigin);
    searchTerm->SetOp(nsMsgSearchOp::Isnt);
    searchTerm->SetBooleanAnd(true);

    nsCOMPtr<nsIMsgSearchValue> searchValue;
    searchTerm->GetValue(getter_AddRefs(searchValue));
    NS_ENSURE_SUCCESS(rv, rv);
    searchValue->SetAttrib(nsMsgSearchAttrib::JunkScoreOrigin);
    searchValue->SetStr(NS_LITERAL_STRING("user"));
    searchTerm->SetValue(searchValue);

    searchTerms->InsertElementAt(searchTerm, count);

    bool moveOnSpam, markAsReadOnSpam;
    spamSettings->GetMoveOnSpam(&moveOnSpam);
    if (moveOnSpam)
    {
      nsCString spamFolderURI;
      rv = spamSettings->GetSpamFolderURI(getter_Copies(spamFolderURI));
      if (NS_SUCCEEDED(rv) && (!spamFolderURI.IsEmpty()))
      {
        nsCOMPtr <nsIMsgRuleAction> moveAction;
        rv = newFilter->CreateAction(getter_AddRefs(moveAction));
        if (NS_SUCCEEDED(rv))
        {
          moveAction->SetType(nsMsgFilterAction::MoveToFolder);
          moveAction->SetTargetFolderUri(spamFolderURI);
          newFilter->AppendAction(moveAction);
        }
      }
    }
    spamSettings->GetMarkAsReadOnSpam(&markAsReadOnSpam);
    if (markAsReadOnSpam)
    {
      nsCOMPtr <nsIMsgRuleAction> markAsReadAction;
      rv = newFilter->CreateAction(getter_AddRefs(markAsReadAction));
      if (NS_SUCCEEDED(rv))
      {
        markAsReadAction->SetType(nsMsgFilterAction::MarkRead);
        newFilter->AppendAction(markAsReadAction);
      }
    }
    filterList->InsertFilterAt(0, newFilter);
  }

  rv = serverFilterList->GetFilterNamed(noFilterName,
                                  getter_AddRefs(newFilter));
  if (newFilter && serverFilterTrustFlags & nsISpamSettings::TRUST_NEGATIVES)
  {
    newFilter->SetTemporary(true);
    filterList->InsertFilterAt(0, newFilter);
  }

  return rv;
}

nsresult
nsMsgIncomingServer::ConfigureTemporaryReturnReceiptsFilter(nsIMsgFilterList *filterList)
{
  nsresult rv;

  nsCOMPtr<nsIMsgAccountManager> accountMgr = do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIMsgIdentity> identity;
  rv = accountMgr->GetFirstIdentityForServer(this, getter_AddRefs(identity));
  NS_ENSURE_SUCCESS(rv, rv);
  // this can return success and a null identity...

  bool useCustomPrefs = false;
  int32_t incorp = nsIMsgMdnGenerator::eIncorporateInbox;
  NS_ENSURE_TRUE(identity, NS_ERROR_NULL_POINTER);

  identity->GetBoolAttribute("use_custom_prefs", &useCustomPrefs);
  if (useCustomPrefs)
    rv = GetIntValue("incorporate_return_receipt", &incorp);
  else
  {
    nsCOMPtr<nsIPrefBranch> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID));
    if (prefs)
      prefs->GetIntPref("mail.incorporate.return_receipt", &incorp);
  }

  bool enable = (incorp == nsIMsgMdnGenerator::eIncorporateSent);

  // this is a temporary, internal mozilla filter
  // it will not show up in the UI, it will not be written to disk
  NS_NAMED_LITERAL_STRING(internalReturnReceiptFilterName, "mozilla-temporary-internal-MDN-receipt-filter");

  nsCOMPtr<nsIMsgFilter> newFilter;
  rv = filterList->GetFilterNamed(internalReturnReceiptFilterName,
                                  getter_AddRefs(newFilter));
  if (newFilter)
    newFilter->SetEnabled(enable);
  else if (enable)
  {
    nsCString actionTargetFolderUri;
    rv = identity->GetFccFolder(actionTargetFolderUri);
    if (!actionTargetFolderUri.IsEmpty())
    {
      filterList->CreateFilter(internalReturnReceiptFilterName,
                               getter_AddRefs(newFilter));
      if (newFilter)
      {
        newFilter->SetEnabled(true);
        // this internal filter is temporary
        // and should not show up in the UI or be written to disk
        newFilter->SetTemporary(true);

        nsCOMPtr<nsIMsgSearchTerm> term;
        nsCOMPtr<nsIMsgSearchValue> value;

        rv = newFilter->CreateTerm(getter_AddRefs(term));
        if (NS_SUCCEEDED(rv))
        {
          rv = term->GetValue(getter_AddRefs(value));
          if (NS_SUCCEEDED(rv))
          {
            // we need to use OtherHeader + 1 so nsMsgFilter::GetTerm will
            // return our custom header.
            value->SetAttrib(nsMsgSearchAttrib::OtherHeader + 1);
            value->SetStr(NS_LITERAL_STRING("multipart/report"));
            term->SetAttrib(nsMsgSearchAttrib::OtherHeader + 1);
            term->SetOp(nsMsgSearchOp::Contains);
            term->SetBooleanAnd(true);
            term->SetArbitraryHeader(NS_LITERAL_CSTRING("Content-Type"));
            term->SetValue(value);
            newFilter->AppendTerm(term);
          }
        }
        rv = newFilter->CreateTerm(getter_AddRefs(term));
        if (NS_SUCCEEDED(rv))
        {
          rv = term->GetValue(getter_AddRefs(value));
          if (NS_SUCCEEDED(rv))
          {
            // XXX todo
            // determine if ::OtherHeader is the best way to do this.
            // see nsMsgSearchOfflineMail::MatchTerms()
            value->SetAttrib(nsMsgSearchAttrib::OtherHeader + 1);
            value->SetStr(NS_LITERAL_STRING("disposition-notification"));
            term->SetAttrib(nsMsgSearchAttrib::OtherHeader + 1);
            term->SetOp(nsMsgSearchOp::Contains);
            term->SetBooleanAnd(true);
            term->SetArbitraryHeader(NS_LITERAL_CSTRING("Content-Type"));
            term->SetValue(value);
            newFilter->AppendTerm(term);
          }
        }
        nsCOMPtr<nsIMsgRuleAction> filterAction;
        rv = newFilter->CreateAction(getter_AddRefs(filterAction));
        if (NS_SUCCEEDED(rv))
        {
          filterAction->SetType(nsMsgFilterAction::MoveToFolder);
          filterAction->SetTargetFolderUri(actionTargetFolderUri);
          newFilter->AppendAction(filterAction);
          filterList->InsertFilterAt(0, newFilter);
        }
      }
    }
  }
  return rv;
}

NS_IMETHODIMP
nsMsgIncomingServer::ClearTemporaryReturnReceiptsFilter()
{
  if (mFilterList)
  {
    nsCOMPtr<nsIMsgFilter> mdnFilter;
    nsresult rv = mFilterList->GetFilterNamed(NS_LITERAL_STRING("mozilla-temporary-internal-MDN-receipt-filter"),
                                              getter_AddRefs(mdnFilter));
   if (NS_SUCCEEDED(rv) && mdnFilter)
     return mFilterList->RemoveFilter(mdnFilter);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsMsgIncomingServer::GetMsgFolderFromURI(nsIMsgFolder *aFolderResource, const nsACString& aURI, nsIMsgFolder **aFolder)
{
  nsCOMPtr<nsIMsgFolder> rootMsgFolder;
  nsresult rv = GetRootMsgFolder(getter_AddRefs(rootMsgFolder));
  NS_ENSURE_TRUE(rootMsgFolder, NS_ERROR_UNEXPECTED);

  nsCOMPtr <nsIMsgFolder> msgFolder;
  rv = rootMsgFolder->GetChildWithURI(aURI, true, true /*caseInsensitive*/, getter_AddRefs(msgFolder));
  if (NS_FAILED(rv) || !msgFolder)
    msgFolder = aFolderResource;
  NS_IF_ADDREF(*aFolder = msgFolder);
  return NS_OK;
}

NS_IMETHODIMP
nsMsgIncomingServer::GetSpamSettings(nsISpamSettings **aSpamSettings)
{
  NS_ENSURE_ARG_POINTER(aSpamSettings);

  nsCAutoString spamActionTargetAccount;
  GetCharValue("spamActionTargetAccount", spamActionTargetAccount);
  if (spamActionTargetAccount.IsEmpty())
  {
    GetServerURI(spamActionTargetAccount);
    SetCharValue("spamActionTargetAccount", spamActionTargetAccount);
  }

  if (!mSpamSettings) {
    nsresult rv;
    mSpamSettings = do_CreateInstance(NS_SPAMSETTINGS_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv,rv);
    mSpamSettings->Initialize(this);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  NS_ADDREF(*aSpamSettings = mSpamSettings);
  return NS_OK;
}

NS_IMETHODIMP
nsMsgIncomingServer::GetSpamFilterPlugin(nsIMsgFilterPlugin **aFilterPlugin)
{
  NS_ENSURE_ARG_POINTER(aFilterPlugin);
  if (!mFilterPlugin)
  {
    nsresult rv;
    mFilterPlugin = do_GetService("@mozilla.org/messenger/filter-plugin;1?name=bayesianfilter", &rv);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  NS_IF_ADDREF(*aFilterPlugin = mFilterPlugin);
  return NS_OK;
}

// get all the servers that defer to the account for the passed in server. Note that
// destServer may not be "this"
nsresult nsMsgIncomingServer::GetDeferredServers(nsIMsgIncomingServer *destServer, nsISupportsArray **_retval)
{
  nsresult rv;
  nsCOMPtr<nsIMsgAccountManager> accountManager
    = do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsISupportsArray> servers;
  rv = NS_NewISupportsArray(getter_AddRefs(servers));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr <nsIMsgAccount> thisAccount;
  accountManager->FindAccountForServer(destServer, getter_AddRefs(thisAccount));
  if (thisAccount)
  {
    nsCOMPtr <nsISupportsArray> allServers;
    nsCString accountKey;
    thisAccount->GetKey(accountKey);
    accountManager->GetAllServers(getter_AddRefs(allServers));
    if (allServers)
    {
      uint32_t serverCount;
      allServers->Count(&serverCount);
      for (uint32_t i = 0; i < serverCount; i++)
      {
        nsCOMPtr <nsIMsgIncomingServer> server (do_QueryElementAt(allServers, i));
        if (server)
        {
          nsCString deferredToAccount;
          server->GetCharValue("deferred_to_account", deferredToAccount);
          if (deferredToAccount.Equals(accountKey))
            servers->AppendElement(server);
        }
      }
    }
  }
  servers.swap(*_retval);
  return rv;
}

NS_IMETHODIMP nsMsgIncomingServer::GetIsDeferredTo(bool *aIsDeferredTo)
{
  NS_ENSURE_ARG_POINTER(aIsDeferredTo);
  nsCOMPtr<nsIMsgAccountManager> accountManager
    = do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID);
  if (accountManager)
  {
    nsCOMPtr <nsIMsgAccount> thisAccount;
    accountManager->FindAccountForServer(this, getter_AddRefs(thisAccount));
    if (thisAccount)
    {
      nsCOMPtr <nsISupportsArray> allServers;
      nsCString accountKey;
      thisAccount->GetKey(accountKey);
      accountManager->GetAllServers(getter_AddRefs(allServers));
      if (allServers)
      {
        uint32_t serverCount;
        allServers->Count(&serverCount);
        for (uint32_t i = 0; i < serverCount; i++)
        {
          nsCOMPtr <nsIMsgIncomingServer> server (do_QueryElementAt(allServers, i));
          if (server)
          {
            nsCString deferredToAccount;
            server->GetCharValue("deferred_to_account", deferredToAccount);
            if (deferredToAccount.Equals(accountKey))
            {
              *aIsDeferredTo = true;
              return NS_OK;
            }
          }
        }
      }
    }
  }
  *aIsDeferredTo = false;
  return NS_OK;
}

const long kMaxDownloadTableSize = 500;

// aClosure is the server, from that we get the cutoff point, below which we evict
// aData is the arrival index of the msg.
/* static */PLDHashOperator nsMsgIncomingServer::evictOldEntries(nsCStringHashKey::KeyType aKey, int32_t &aData, void *aClosure)
{
  nsMsgIncomingServer *server = (nsMsgIncomingServer *) aClosure;
  if (aData < server->m_numMsgsDownloaded - kMaxDownloadTableSize/2)
    return PL_DHASH_REMOVE;
  return server->m_downloadedHdrs.Count() > kMaxDownloadTableSize/2 ? PL_DHASH_NEXT : PL_DHASH_STOP;
}

// hash the concatenation of the message-id and subject as the hash table key,
// and store the arrival index as the value. To limit the size of the hash table,
// we just throw out ones with a lower ordinal value than the cut-off point.
NS_IMETHODIMP nsMsgIncomingServer::IsNewHdrDuplicate(nsIMsgDBHdr *aNewHdr, bool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  NS_ENSURE_ARG_POINTER(aNewHdr);
  *aResult = false;

  // If the message has been partially downloaded, the message should not
  // be considered a duplicated message. See bug 714090.
  uint32_t flags;
  aNewHdr->GetFlags(&flags);
  if (flags & nsMsgMessageFlags::Partial)
    return NS_OK;

  nsCAutoString strHashKey;
  nsCString messageId, subject;
  aNewHdr->GetMessageId(getter_Copies(messageId));
  strHashKey.Append(messageId);
  aNewHdr->GetSubject(getter_Copies(subject));
  // err on the side of caution and ignore messages w/o subject or messageid.
  if (subject.IsEmpty() || messageId.IsEmpty())
    return NS_OK;
  strHashKey.Append(subject);
  int32_t hashValue = 0;
  m_downloadedHdrs.Get(strHashKey, &hashValue);
  if (hashValue)
    *aResult = true;
  else
  {
    // we store the current size of the hash table as the hash
    // value - this allows us to delete older entries.
    m_downloadedHdrs.Put(strHashKey, ++m_numMsgsDownloaded);
    // Check if hash table is larger than some reasonable size
    // and if is it, iterate over hash table deleting messages
    // with an arrival index < number of msgs downloaded - half the reasonable size.
    if (m_downloadedHdrs.Count() >= kMaxDownloadTableSize)
      m_downloadedHdrs.Enumerate(evictOldEntries, this);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsMsgIncomingServer::GetForcePropertyEmpty(const char *aPropertyName, bool *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  nsCAutoString nameEmpty(aPropertyName);
  nameEmpty.Append(NS_LITERAL_CSTRING(".empty"));
  nsCString value;
  GetCharValue(nameEmpty.get(), value);
  *_retval = value.EqualsLiteral("true");
  return NS_OK;
}

NS_IMETHODIMP
nsMsgIncomingServer::SetForcePropertyEmpty(const char *aPropertyName, bool aValue)
{
 nsCAutoString nameEmpty(aPropertyName);
 nameEmpty.Append(NS_LITERAL_CSTRING(".empty"));
 return SetCharValue(nameEmpty.get(),
   aValue ? NS_LITERAL_CSTRING("true") : NS_LITERAL_CSTRING(""));
}
