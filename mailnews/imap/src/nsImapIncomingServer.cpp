/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "msgCore.h"
#include "nsMsgImapCID.h"

#include "netCore.h"
#include "nsISupportsObsolete.h"
#include "nsIMAPHostSessionList.h"
#include "nsImapIncomingServer.h"
#include "nsIMsgAccountManager.h"
#include "nsIMsgIdentity.h"
#include "nsIImapUrl.h"
#include "nsIUrlListener.h"
#include "nsThreadUtils.h"
#include "nsImapProtocol.h"
#include "nsCOMPtr.h"
#include "nsImapStringBundle.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsMsgFolderFlags.h"
#include "prmem.h"
#include "plstr.h"
#include "nsIMsgFolder.h"
#include "nsIMsgWindow.h"
#include "nsImapMailFolder.h"
#include "nsImapUtils.h"
#include "nsIRDFService.h"
#include "nsRDFCID.h"
#include "nsIMsgMailNewsUrl.h"
#include "nsIImapService.h"
#include "nsMsgI18N.h"
#include "nsIImapMockChannel.h"
// for the memory cache...
#include "nsICacheEntryDescriptor.h"
#include "nsImapUrl.h"
#include "nsIMsgProtocolInfo.h"
#include "nsIMsgMailSession.h"
#include "nsIMAPNamespace.h"
#include "nsISignatureVerifier.h"
#include "nsArrayUtils.h"
#include "nsITimer.h"
#include "nsMsgUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsCRTGlue.h"
#include "mozilla/Services.h"

using namespace mozilla;

#define PREF_TRASH_FOLDER_NAME "trash_folder_name"
#define DEFAULT_TRASH_FOLDER_NAME "Trash"

static NS_DEFINE_CID(kImapProtocolCID, NS_IMAPPROTOCOL_CID);
static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kSubscribableServerCID, NS_SUBSCRIBABLESERVER_CID);
static NS_DEFINE_CID(kCImapHostSessionListCID, NS_IIMAPHOSTSESSIONLIST_CID);

NS_IMPL_ADDREF_INHERITED(nsImapIncomingServer, nsMsgIncomingServer)
NS_IMPL_RELEASE_INHERITED(nsImapIncomingServer, nsMsgIncomingServer)

NS_INTERFACE_MAP_BEGIN(nsImapIncomingServer)
  NS_INTERFACE_MAP_ENTRY(nsIImapServerSink)
  NS_INTERFACE_MAP_ENTRY(nsIImapIncomingServer)
  NS_INTERFACE_MAP_ENTRY(nsISubscribableServer)
  NS_INTERFACE_MAP_ENTRY(nsIUrlListener)
NS_INTERFACE_MAP_END_INHERITING(nsMsgIncomingServer)

nsImapIncomingServer::nsImapIncomingServer()
  : mLock("nsImapIncomingServer.mLock")
{
  m_capability = kCapabilityUndefined;
  mDoingSubscribeDialog = false;
  mDoingLsub = false;
  m_canHaveFilters = true;
  m_userAuthenticated = false;
  m_shuttingDown = false;
}

nsImapIncomingServer::~nsImapIncomingServer()
{
  nsresult rv = ClearInner();
  NS_ASSERTION(NS_SUCCEEDED(rv), "ClearInner failed");
  CloseCachedConnections();
}

NS_IMETHODIMP nsImapIncomingServer::SetKey(const nsACString& aKey)  // override nsMsgIncomingServer's implementation...
{
  nsMsgIncomingServer::SetKey(aKey);

  // okay now that the key has been set, we need to add ourselves to the
  // host session list...

  // every time we create an imap incoming server, we need to add it to the
  // host session list!!

  nsresult rv;
  nsCOMPtr<nsIImapHostSessionList> hostSession = do_GetService(kCImapHostSessionListCID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCString key(aKey);
  hostSession->AddHostToList(key.get(), this);
  nsMsgImapDeleteModel deleteModel = nsMsgImapDeleteModels::MoveToTrash; // default to trash
  GetDeleteModel(&deleteModel);
  hostSession->SetDeleteIsMoveToTrashForHost(key.get(), deleteModel == nsMsgImapDeleteModels::MoveToTrash);
  hostSession->SetShowDeletedMessagesForHost(key.get(), deleteModel == nsMsgImapDeleteModels::IMAPDelete);

  nsCAutoString onlineDir;
  rv = GetServerDirectory(onlineDir);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!onlineDir.IsEmpty())
    hostSession->SetOnlineDirForHost(key.get(), onlineDir.get());

  nsCString personalNamespace;
  nsCString publicNamespace;
  nsCString otherUsersNamespace;

  rv = GetPersonalNamespace(personalNamespace);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = GetPublicNamespace(publicNamespace);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = GetOtherUsersNamespace(otherUsersNamespace);
  NS_ENSURE_SUCCESS(rv, rv);

  if (personalNamespace.IsEmpty() && publicNamespace.IsEmpty() && otherUsersNamespace.IsEmpty())
      personalNamespace.AssignLiteral("\"\"");

  hostSession->SetNamespaceFromPrefForHost(key.get(), personalNamespace.get(),
                                           kPersonalNamespace);

  if (!publicNamespace.IsEmpty())
      hostSession->SetNamespaceFromPrefForHost(key.get(), publicNamespace.get(),
                                               kPublicNamespace);

  if (!otherUsersNamespace.IsEmpty())
      hostSession->SetNamespaceFromPrefForHost(key.get(), otherUsersNamespace.get(),
                                               kOtherUsersNamespace);
  return rv;
}

// construct the pretty name to show to the user if they haven't
// specified one. This should be overridden for news and mail.
NS_IMETHODIMP
nsImapIncomingServer::GetConstructedPrettyName(nsAString& retval)
{
  nsCAutoString username;
  nsCAutoString hostName;
  nsresult rv;

  nsCOMPtr<nsIMsgAccountManager> accountManager =
           do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr<nsIMsgIdentity> identity;
  rv = accountManager->GetFirstIdentityForServer(this, getter_AddRefs(identity));
  NS_ENSURE_SUCCESS(rv,rv);

  nsAutoString emailAddress;

  if (NS_SUCCEEDED(rv) && identity)
  {
    nsCString identityEmailAddress;
    identity->GetEmail(identityEmailAddress);
    CopyASCIItoUTF16(identityEmailAddress, emailAddress);
  }
  else
  {
    rv = GetRealUsername(username);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = GetRealHostName(hostName);
    NS_ENSURE_SUCCESS(rv,rv);
    if (!username.IsEmpty() && !hostName.IsEmpty()) {
      CopyASCIItoUTF16(username, emailAddress);
      emailAddress.Append('@');
      emailAddress.Append(NS_ConvertASCIItoUTF16(hostName));
    }
  }

  return GetFormattedStringFromID(emailAddress, IMAP_DEFAULT_ACCOUNT_NAME, retval);
}


NS_IMETHODIMP nsImapIncomingServer::GetLocalStoreType(nsACString& type)
{
  type.AssignLiteral("imap");
  return NS_OK;
}

NS_IMETHODIMP
nsImapIncomingServer::GetServerDirectory(nsACString& serverDirectory)
{
  return GetCharValue("server_sub_directory", serverDirectory);
}

NS_IMETHODIMP
nsImapIncomingServer::SetServerDirectory(const nsACString& serverDirectory)
{
  nsCString serverKey;
  nsresult rv = GetKey(serverKey);
  if (NS_SUCCEEDED(rv))
  {
    nsCOMPtr<nsIImapHostSessionList> hostSession = do_GetService(kCImapHostSessionListCID, &rv);
    if (NS_SUCCEEDED(rv))
      hostSession->SetOnlineDirForHost(serverKey.get(), PromiseFlatCString(serverDirectory).get());
  }
  return SetCharValue("server_sub_directory", serverDirectory);
}

NS_IMETHODIMP
nsImapIncomingServer::GetOverrideNamespaces(bool *bVal)
{
  return GetBoolValue("override_namespaces", bVal);
}

NS_IMETHODIMP
nsImapIncomingServer::SetOverrideNamespaces(bool bVal)
{
  nsCString serverKey;
  GetKey(serverKey);
  if (!serverKey.IsEmpty())
  {
    nsresult rv;
    nsCOMPtr<nsIImapHostSessionList> hostSession = do_GetService(kCImapHostSessionListCID, &rv);
    if (NS_SUCCEEDED(rv))
      hostSession->SetNamespacesOverridableForHost(serverKey.get(), bVal);
  }
  return SetBoolValue("override_namespaces", bVal);
}

NS_IMETHODIMP
nsImapIncomingServer::GetUsingSubscription(bool *bVal)
{
  return GetBoolValue("using_subscription", bVal);
}

NS_IMETHODIMP
nsImapIncomingServer::SetUsingSubscription(bool bVal)
{
  nsCString serverKey;
  GetKey(serverKey);
  if (!serverKey.IsEmpty())
  {
    nsresult rv;
    nsCOMPtr<nsIImapHostSessionList> hostSession = do_GetService(kCImapHostSessionListCID, &rv);
    if (NS_SUCCEEDED(rv))
      hostSession->SetHostIsUsingSubscription(serverKey.get(), bVal);
  }
  return SetBoolValue("using_subscription", bVal);
}

NS_IMPL_SERVERPREF_BOOL(nsImapIncomingServer, DualUseFolders,
                        "dual_use_folders")

NS_IMPL_SERVERPREF_STR(nsImapIncomingServer, AdminUrl,
                       "admin_url")

NS_IMPL_SERVERPREF_BOOL(nsImapIncomingServer, CleanupInboxOnExit,
                        "cleanup_inbox_on_exit")

NS_IMPL_SERVERPREF_BOOL(nsImapIncomingServer, OfflineDownload,
                        "offline_download")

NS_IMPL_SERVERPREF_INT(nsImapIncomingServer, MaximumConnectionsNumber,
                       "max_cached_connections")

NS_IMPL_SERVERPREF_INT(nsImapIncomingServer, EmptyTrashThreshhold,
                       "empty_trash_threshhold")

NS_IMPL_SERVERPREF_BOOL(nsImapIncomingServer, DownloadBodiesOnGetNewMail,
                        "download_bodies_on_get_new_mail")

NS_IMPL_SERVERPREF_BOOL(nsImapIncomingServer, AutoSyncOfflineStores,
                        "autosync_offline_stores")

NS_IMPL_SERVERPREF_BOOL(nsImapIncomingServer, UseIdle,
                        "use_idle")

NS_IMPL_SERVERPREF_BOOL(nsImapIncomingServer, CheckAllFoldersForNew,
                        "check_all_folders_for_new")

NS_IMPL_SERVERPREF_BOOL(nsImapIncomingServer, UseCondStore,
                        "use_condstore")

NS_IMPL_SERVERPREF_BOOL(nsImapIncomingServer, IsGMailServer,
                        "is_gmail")

NS_IMPL_SERVERPREF_BOOL(nsImapIncomingServer, UseCompressDeflate,
                        "use_compress_deflate")

NS_IMPL_SERVERPREF_INT(nsImapIncomingServer, AutoSyncMaxAgeDays,
                        "autosync_max_age_days")

NS_IMETHODIMP
nsImapIncomingServer::GetShuttingDown(bool *retval)
{
  NS_ENSURE_ARG_POINTER(retval);
  *retval = m_shuttingDown;
  return NS_OK;
}

NS_IMETHODIMP
nsImapIncomingServer::SetShuttingDown(bool val)
{
  m_shuttingDown = val;
  return NS_OK;
}

NS_IMETHODIMP
nsImapIncomingServer::GetDeleteModel(int32_t *retval)
{
  NS_ENSURE_ARG(retval);
  return GetIntValue("delete_model", retval);
}

NS_IMETHODIMP
nsImapIncomingServer::SetDeleteModel(int32_t ivalue)
{
  nsresult rv = SetIntValue("delete_model", ivalue);
  if (NS_SUCCEEDED(rv))
  {
    nsCOMPtr<nsIImapHostSessionList> hostSession =
        do_GetService(kCImapHostSessionListCID, &rv);
    NS_ENSURE_SUCCESS(rv,rv);
    hostSession->SetDeleteIsMoveToTrashForHost(m_serverKey.get(), ivalue == nsMsgImapDeleteModels::MoveToTrash);
    hostSession->SetShowDeletedMessagesForHost(m_serverKey.get(), ivalue == nsMsgImapDeleteModels::IMAPDelete);

    nsAutoString trashFolderName;
    nsresult rv = GetTrashFolderName(trashFolderName);
    if (NS_SUCCEEDED(rv))
    {
      nsCAutoString trashFolderNameUtf7;
      rv = CopyUTF16toMUTF7(trashFolderName, trashFolderNameUtf7);
      if (NS_SUCCEEDED(rv))
      {
        nsCOMPtr<nsIMsgFolder> trashFolder;
        rv = GetFolder(trashFolderNameUtf7, getter_AddRefs(trashFolder));
        NS_ENSURE_SUCCESS(rv, rv);
        nsCString trashURI;
        trashFolder->GetURI(trashURI);
        GetMsgFolderFromURI(trashFolder, trashURI, getter_AddRefs(trashFolder));
        if (NS_SUCCEEDED(rv) && trashFolder)
        {
           // If the trash folder is used, set the flag, otherwise clear it.
          if (ivalue == nsMsgImapDeleteModels::MoveToTrash)
            trashFolder->SetFlag(nsMsgFolderFlags::Trash);
          else
            trashFolder->ClearFlag(nsMsgFolderFlags::Trash);
        }
      }
    }
  }
  return rv;
}

NS_IMPL_SERVERPREF_INT(nsImapIncomingServer, TimeOutLimits,
                       "timeout")

NS_IMPL_SERVERPREF_STR(nsImapIncomingServer, ServerIDPref,
                       "serverIDResponse")

NS_IMPL_SERVERPREF_STR(nsImapIncomingServer, PersonalNamespace,
                       "namespace.personal")

NS_IMPL_SERVERPREF_STR(nsImapIncomingServer, PublicNamespace,
                       "namespace.public")

NS_IMPL_SERVERPREF_STR(nsImapIncomingServer, OtherUsersNamespace,
                       "namespace.other_users")

NS_IMPL_SERVERPREF_BOOL(nsImapIncomingServer, FetchByChunks,
                       "fetch_by_chunks")

NS_IMPL_SERVERPREF_BOOL(nsImapIncomingServer, MimePartsOnDemand,
                       "mime_parts_on_demand")

NS_IMPL_SERVERPREF_BOOL(nsImapIncomingServer, SendID,
                       "send_client_info")

NS_IMPL_SERVERPREF_BOOL(nsImapIncomingServer, CapabilityACL,
                       "cacheCapa.acl")
NS_IMPL_SERVERPREF_BOOL(nsImapIncomingServer, CapabilityQuota,
                       "cacheCapa.quota")

NS_IMETHODIMP
nsImapIncomingServer::GetIsAOLServer(bool *aBool)
{
  NS_ENSURE_ARG_POINTER(aBool);
  *aBool = ((m_capability & kAOLImapCapability) != 0);
  return NS_OK;
}

NS_IMETHODIMP
nsImapIncomingServer::SetIsAOLServer(bool aBool)
{
  if (aBool)
    m_capability |= kAOLImapCapability;
  else
    m_capability &= ~kAOLImapCapability;
  return NS_OK;
}

NS_IMETHODIMP
nsImapIncomingServer::UpdateTrySTARTTLSPref(bool aStartTLSSucceeded)
{
  SetSocketType(aStartTLSSucceeded ? nsMsgSocketType::alwaysSTARTTLS :
                                     nsMsgSocketType::plain);
  return NS_OK;
}

NS_IMETHODIMP
nsImapIncomingServer::GetImapConnectionAndLoadUrl(nsIImapUrl* aImapUrl,
                                                  nsISupports* aConsumer)
{
  nsCOMPtr<nsIImapProtocol> aProtocol;

  nsresult rv = GetImapConnection(aImapUrl, getter_AddRefs(aProtocol));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIMsgMailNewsUrl> mailnewsurl = do_QueryInterface(aImapUrl, &rv);
  if (aProtocol)
  {
    rv = aProtocol->LoadImapUrl(mailnewsurl, aConsumer);
    // *** jt - in case of the time out situation or the connection gets
    // terminated by some unforseen problems let's give it a second chance
    // to run the url
    if (NS_FAILED(rv))
    {
      NS_ASSERTION(false, "shouldn't get an error loading url");
      rv = aProtocol->LoadImapUrl(mailnewsurl, aConsumer);
    }
  }
  else
  {   // unable to get an imap connection to run the url; add to the url
     // queue
    nsImapProtocol::LogImapUrl("queuing url", aImapUrl);
    PR_CEnterMonitor(this);
    m_urlQueue.AppendObject(aImapUrl);
    m_urlConsumers.AppendElement((void*)aConsumer);
    NS_IF_ADDREF(aConsumer);
    PR_CExitMonitor(this);
    // let's try running it now - maybe the connection is free now.
    bool urlRun;
    rv = LoadNextQueuedUrl(nullptr, &urlRun);
  }

  return rv;
}


NS_IMETHODIMP
nsImapIncomingServer::PrepareToRetryUrl(nsIImapUrl *aImapUrl, nsIImapMockChannel **aChannel)
{
  NS_ENSURE_ARG_POINTER(aChannel);
  NS_ENSURE_ARG_POINTER(aImapUrl);
  // maybe there's more we could do here, but this is all we need now.
  return aImapUrl->GetMockChannel(aChannel);
}

NS_IMETHODIMP
nsImapIncomingServer::SuspendUrl(nsIImapUrl *aImapUrl)
{
  NS_ENSURE_ARG_POINTER(aImapUrl);
  nsImapProtocol::LogImapUrl("suspending url", aImapUrl);
  PR_CEnterMonitor(this);
  m_urlQueue.AppendObject(aImapUrl);
  m_urlConsumers.AppendElement(nullptr);
  PR_CExitMonitor(this);
  return NS_OK;
}

NS_IMETHODIMP
nsImapIncomingServer::RetryUrl(nsIImapUrl *aImapUrl, nsIImapMockChannel *aChannel)
{
  nsresult rv;
  // Get current thread envent queue
  aImapUrl->SetMockChannel(aChannel);
  nsCOMPtr <nsIImapProtocol> protocolInstance;
  nsImapProtocol::LogImapUrl("creating protocol instance to retry queued url", aImapUrl);
  nsCOMPtr<nsIThread> thread(do_GetCurrentThread());
  rv = GetImapConnection(aImapUrl, getter_AddRefs(protocolInstance));
  if (NS_SUCCEEDED(rv) && protocolInstance)
  {
    nsCOMPtr<nsIURI> url = do_QueryInterface(aImapUrl, &rv);
    if (NS_SUCCEEDED(rv) && url)
    {
      nsImapProtocol::LogImapUrl("retrying  url", aImapUrl);
      rv = protocolInstance->LoadImapUrl(url, nullptr); // ### need to save the display consumer.
      NS_ASSERTION(NS_SUCCEEDED(rv), "failed running queued url");
    }
  }
  return rv;
}

// checks to see if there are any queued urls on this incoming server,
// and if so, tries to run the oldest one. Returns true if the url is run
// on the passed in protocol connection.
NS_IMETHODIMP
nsImapIncomingServer::LoadNextQueuedUrl(nsIImapProtocol *aProtocol, bool *aResult)
{
  if (WeAreOffline())
    return NS_MSG_ERROR_OFFLINE;

  nsresult rv = NS_OK;
  bool urlRun = false;
  bool keepGoing = true;
  nsCOMPtr <nsIImapProtocol>  protocolInstance ;

  MutexAutoLock mon(mLock);
  int32_t cnt = m_urlQueue.Count();
  int32_t procIndex = 0;

  while (cnt > 0 && !urlRun && keepGoing)
  {
   

	procIndex = 0;
	for (int32_t i = cnt - 1; i >= 0; i--)
	{
		nsCOMPtr<nsIImapUrl> aImapUrl(m_urlQueue[i]);
		nsImapAction imapActionPriority;
		aImapUrl->GetImapPriority(&imapActionPriority);
		if ( imapActionPriority == nsIImapUrl::ImapPriorityOne)
		{
			nsImapProtocol::LogImapUrl("PriorityOne queued url", aImapUrl);
			procIndex = i;
			break;
		}
	}
	nsCOMPtr<nsIImapUrl> aImapUrl(m_urlQueue[procIndex]);
	nsCOMPtr<nsIMsgMailNewsUrl> aMailNewsUrl(do_QueryInterface(aImapUrl, &rv));

    bool removeUrlFromQueue = false;
    if (aImapUrl)
    {
      nsImapProtocol::LogImapUrl("considering playing queued url", aImapUrl);
      rv = DoomUrlIfChannelHasError(aImapUrl, &removeUrlFromQueue);
      NS_ENSURE_SUCCESS(rv, rv);
      // if we didn't doom the url, lets run it.
      if (!removeUrlFromQueue)
      {
        nsISupports *aConsumer = (nsISupports*)m_urlConsumers.ElementAt(procIndex);
        NS_IF_ADDREF(aConsumer);

        nsImapProtocol::LogImapUrl("creating protocol instance to play queued url", aImapUrl);
        rv = GetImapConnection(aImapUrl, getter_AddRefs(protocolInstance));
        if (NS_SUCCEEDED(rv) && protocolInstance)
        {
          nsCOMPtr<nsIURI> url = do_QueryInterface(aImapUrl, &rv);
          if (NS_SUCCEEDED(rv) && url)
          {
            nsImapProtocol::LogImapUrl("playing queued url", aImapUrl);
            rv = protocolInstance->LoadImapUrl(url, aConsumer);
            NS_ASSERTION(NS_SUCCEEDED(rv), "failed running queued url");
            bool isInbox;
            protocolInstance->IsBusy(&urlRun, &isInbox);
            if (!urlRun)
              nsImapProtocol::LogImapUrl("didn't need to run", aImapUrl);
            removeUrlFromQueue = true;
          }
        }
        else
        {
          nsImapProtocol::LogImapUrl("failed creating protocol instance to play queued url", aImapUrl);
          keepGoing = false;
        }
        NS_IF_RELEASE(aConsumer);
      }
      if (removeUrlFromQueue)
      {
		  //nsImapProtocol::LogImapUrl("removeUrlFromQueue", aImapUrl);
        m_urlQueue.RemoveObjectAt(procIndex);
        m_urlConsumers.RemoveElementAt(procIndex);
      }
    }
    cnt = m_urlQueue.Count();
  }
  if (aResult)
    *aResult = urlRun && aProtocol && aProtocol == protocolInstance;

  return rv;
}

NS_IMETHODIMP
nsImapIncomingServer::AbortQueuedUrls()
{
  nsresult rv = NS_OK;

  MutexAutoLock mon(mLock);
  int32_t cnt = m_urlQueue.Count();

  while (cnt > 0)
  {
    nsCOMPtr<nsIImapUrl> aImapUrl(m_urlQueue[cnt - 1]);
    bool removeUrlFromQueue = false;

    if (aImapUrl)
    {
      rv = DoomUrlIfChannelHasError(aImapUrl, &removeUrlFromQueue);
      NS_ENSURE_SUCCESS(rv, rv);
      if (removeUrlFromQueue)
      {
        m_urlQueue.RemoveObjectAt(cnt - 1);
        m_urlConsumers.RemoveElementAt(cnt - 1);
      }
    }
    cnt--;
  }

  return rv;
}

// if this url has a channel with an error, doom it and its mem cache entries,
// and notify url listeners.
nsresult nsImapIncomingServer::DoomUrlIfChannelHasError(nsIImapUrl *aImapUrl, bool *urlDoomed)
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsIMsgMailNewsUrl> aMailNewsUrl(do_QueryInterface(aImapUrl, &rv));

  if (aMailNewsUrl && aImapUrl)
  {
    nsCOMPtr <nsIImapMockChannel> mockChannel;

    if (NS_SUCCEEDED(aImapUrl->GetMockChannel(getter_AddRefs(mockChannel))) && mockChannel)
    {
      nsresult requestStatus;
      nsCOMPtr<nsIRequest> request = do_QueryInterface(mockChannel);
      if (!request)
        return NS_ERROR_FAILURE;
      request->GetStatus(&requestStatus);
      if (NS_FAILED(requestStatus))
      {
        nsresult res;
        *urlDoomed = true;
        nsImapProtocol::LogImapUrl("dooming url", aImapUrl);

        mockChannel->Close(); // try closing it to get channel listener nulled out.

        if (aMailNewsUrl)
        {
          nsCOMPtr<nsICacheEntryDescriptor>  cacheEntry;
          res = aMailNewsUrl->GetMemCacheEntry(getter_AddRefs(cacheEntry));
          if (NS_SUCCEEDED(res) && cacheEntry)
            cacheEntry->Doom();
          // we're aborting this url - tell listeners
          aMailNewsUrl->SetUrlState(false, NS_MSG_ERROR_URL_ABORTED);
        }
      }
    }
  }
  return rv;
}

NS_IMETHODIMP
nsImapIncomingServer::RemoveConnection(nsIImapProtocol* aImapConnection)
{
  PR_CEnterMonitor(this);
  if (aImapConnection)
    m_connectionCache.RemoveObject(aImapConnection);

  PR_CExitMonitor(this);
  return NS_OK;
}

bool
nsImapIncomingServer::ConnectionTimeOut(nsIImapProtocol* aConnection)
{
  bool retVal = false;
  if (!aConnection) return retVal;
  nsresult rv;

  int32_t timeoutInMinutes = 0;
  rv = GetTimeOutLimits(&timeoutInMinutes);
  if (NS_FAILED(rv) || timeoutInMinutes <= 0 || timeoutInMinutes > 29)
  {
    timeoutInMinutes = 29;
    SetTimeOutLimits(timeoutInMinutes);
  }

  PRTime cacheTimeoutLimits = timeoutInMinutes * 60 * PR_USEC_PER_SEC;
  PRTime lastActiveTimeStamp;
  rv = aConnection->GetLastActiveTimeStamp(&lastActiveTimeStamp);

  if (PR_Now() - lastActiveTimeStamp >= cacheTimeoutLimits)
  {
      nsCOMPtr<nsIImapProtocol> aProtocol(do_QueryInterface(aConnection,
                                                            &rv));
      if (NS_SUCCEEDED(rv) && aProtocol)
      {
        RemoveConnection(aConnection);
        aProtocol->TellThreadToDie(false);
        retVal = true;
      }
  }
  return retVal;
}

nsresult
nsImapIncomingServer::GetImapConnection(nsIImapUrl * aImapUrl,
                                        nsIImapProtocol ** aImapConnection)
{
  NS_ENSURE_ARG_POINTER(aImapUrl);

  nsresult rv = NS_OK;
  bool canRunUrlImmediately = false;
  bool canRunButBusy = false;
  nsCOMPtr<nsIImapProtocol> connection;
  nsCOMPtr<nsIImapProtocol> freeConnection;
  bool isBusy = false;
  bool isInboxConnection = false;

  PR_CEnterMonitor(this);

  int32_t maxConnections = 5; // default to be five
  rv = GetMaximumConnectionsNumber(&maxConnections);
  if (NS_FAILED(rv) || maxConnections == 0)
  {
    maxConnections = 5;
    rv = SetMaximumConnectionsNumber(maxConnections);
  }
  else if (maxConnections < 1)
  { // forced to use at least 1
    maxConnections = 1;
    rv = SetMaximumConnectionsNumber(maxConnections);
  }

  int32_t cnt = m_connectionCache.Count();

  *aImapConnection = nullptr;
  // iterate through the connection cache for a connection that can handle this url.
  bool userCancelled = false;

  // loop until we find a connection that can run the url, or doesn't have to wait?
  for (int32_t i = cnt - 1; i >= 0 && !canRunUrlImmediately && !canRunButBusy; i--)
  {
    connection = m_connectionCache[i];
    if (connection)
    {
      bool badConnection = ConnectionTimeOut(connection);
      if (!badConnection)
      {
        badConnection = NS_FAILED(connection->CanHandleUrl(aImapUrl,
                                                           &canRunUrlImmediately,
                                                           &canRunButBusy));
#ifdef DEBUG_bienvenu
        nsCAutoString curSelectedFolderName;
        if (connection)
          connection->GetSelectedMailboxName(getter_Copies(curSelectedFolderName));
        // check that no other connection is in the same selected state.
        if (!curSelectedFolderName.IsEmpty())
        {
          for (uint32_t j = 0; j < cnt; j++)
          {
            if (j != i)
            {
              nsCOMPtr<nsIImapProtocol> otherConnection = do_QueryElementAt(m_connectionCache, j);
              if (otherConnection)
              {
                nsCAutoString otherSelectedFolderName;
                otherConnection->GetSelectedMailboxName(getter_Copies(otherSelectedFolderName));
                NS_ASSERTION(!curSelectedFolderName.Equals(otherSelectedFolderName), "two connections selected on same folder");
              }

            }
          }
        }
#endif // DEBUG_bienvenu
      }
      if (badConnection)
      {
        connection = nullptr;
        continue;
      }
    }

    // if this connection is wrong, but it's not busy, check if we should designate
    // it as the free connection.
    if (!canRunUrlImmediately && !canRunButBusy && connection)
    {
        rv = connection->IsBusy(&isBusy, &isInboxConnection);
        if (NS_FAILED(rv))
          continue;
        // if max connections is <= 1, we have to re-use the inbox connection.
        if (!isBusy && (!isInboxConnection || maxConnections <= 1))
        {
          if (!freeConnection)
            freeConnection = connection;
          else  // check which is the better free connection to use.
          {     // We prefer one not in the selected state.
            nsCAutoString selectedFolderName;
            connection->GetSelectedMailboxName(getter_Copies(selectedFolderName));
            if (selectedFolderName.IsEmpty())
              freeConnection = connection;
          }
        }
    }
    // don't leave this loop with connection set if we can't use it!
    if (!canRunButBusy && !canRunUrlImmediately)
      connection = nullptr;
  }

  nsImapState requiredState;
  aImapUrl->GetRequiredImapState(&requiredState);
  // refresh cnt in case we killed one or more dead connections. This
  // will prevent us from not spinning up a new connection when all
  // connections were dead.
  cnt = m_connectionCache.Count();
  // if we got here and we have a connection, then we should return it!
  if (canRunUrlImmediately && connection)
  {
    *aImapConnection = connection;
    NS_IF_ADDREF(*aImapConnection);
  }
  else if (canRunButBusy)
  {
    // do nothing; return NS_OK; for queuing
  }
  else if (userCancelled)
  {
    rv = NS_BINDING_ABORTED;  // user cancelled
  }
  // CanHandleUrl will pretend that some types of urls require a selected state url
  // (e.g., a folder delete or msg append) but we shouldn't create new connections
  // for these types of urls if we have a free connection. So we check the actual
  // required state here.
  else if (cnt < maxConnections
      && (!freeConnection || requiredState == nsIImapUrl::nsImapSelectedState))
    rv = CreateProtocolInstance(aImapConnection);
  else if (freeConnection)
  {
    *aImapConnection = freeConnection;
    NS_IF_ADDREF(*aImapConnection);
  }
  else // cannot get anyone to handle the url queue it
  {
    if (cnt >= maxConnections)
      nsImapProtocol::LogImapUrl("exceeded connection cache limit", aImapUrl);
      // caller will queue the url
  }

  PR_CExitMonitor(this);
  return rv;
}

nsresult
nsImapIncomingServer::CreateProtocolInstance(nsIImapProtocol ** aImapConnection)
{
  // create a new connection and add it to the connection cache
  // we may need to flag the protocol connection as busy so we don't get
  // a race condition where someone else goes through this code

  int32_t authMethod;
  GetAuthMethod(&authMethod);
  nsresult rv;
  // pre-flight that we have nss - on the ui thread - for MD5 etc.
  switch (authMethod)
  {
    case nsMsgAuthMethod::passwordEncrypted:
    case nsMsgAuthMethod::secure:
    case nsMsgAuthMethod::anything:
      {
        nsCOMPtr<nsISignatureVerifier> verifier =
            do_GetService(SIGNATURE_VERIFIER_CONTRACTID, &rv);
        NS_ENSURE_SUCCESS(rv, rv);
      }
      break;
    default:
      break;
  }
  nsIImapProtocol * protocolInstance;
  rv = CallCreateInstance(kImapProtocolCID, &protocolInstance);
  if (NS_SUCCEEDED(rv) && protocolInstance)
  {
    nsCOMPtr<nsIImapHostSessionList> hostSession =
      do_GetService(kCImapHostSessionListCID, &rv);
    if (NS_SUCCEEDED(rv))
      rv = protocolInstance->Initialize(hostSession, this);
  }

  // take the protocol instance and add it to the connectionCache
  if (protocolInstance)
    m_connectionCache.AppendObject(protocolInstance);
  *aImapConnection = protocolInstance; // this is already ref counted.
  return rv;
}

NS_IMETHODIMP nsImapIncomingServer::CloseConnectionForFolder(nsIMsgFolder *aMsgFolder)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIImapProtocol> connection;
  bool isBusy = false, isInbox = false;
  nsCString inFolderName;
  nsCString connectionFolderName;
  nsCOMPtr <nsIMsgImapMailFolder> imapFolder = do_QueryInterface(aMsgFolder);

  if (!imapFolder)
    return NS_ERROR_NULL_POINTER;

  int32_t cnt = m_connectionCache.Count();
  NS_ENSURE_SUCCESS(rv, rv);

  imapFolder->GetOnlineName(inFolderName);
  PR_CEnterMonitor(this);

  for (int32_t i = 0; i < cnt; ++i)
  {
    connection = m_connectionCache[i];
    if (connection)
    {
      rv = connection->GetSelectedMailboxName(getter_Copies(connectionFolderName));
      if (connectionFolderName.Equals(inFolderName))
      {
        rv = connection->IsBusy(&isBusy, &isInbox);
        if (!isBusy)
          rv = connection->TellThreadToDie(true);
        break; // found it, end of the loop
      }
    }
  }

  PR_CExitMonitor(this);
  return rv;
}

NS_IMETHODIMP nsImapIncomingServer::ResetConnection(const nsACString& folderName)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIImapProtocol> connection;
  bool isBusy = false, isInbox = false;
  nsCString curFolderName;

  int32_t cnt = m_connectionCache.Count();

  PR_CEnterMonitor(this);

  for (int32_t i = 0; i < cnt; ++i)
  {
    connection = m_connectionCache[i];
    if (connection)
    {
      rv = connection->GetSelectedMailboxName(getter_Copies(curFolderName));
      if (curFolderName.Equals(folderName))
      {
        rv = connection->IsBusy(&isBusy, &isInbox);
        if (!isBusy)
            rv = connection->ResetToAuthenticatedState();
        break; // found it, end of the loop
      }
    }
  }

  PR_CExitMonitor(this);
  return rv;
}

NS_IMETHODIMP
nsImapIncomingServer::PerformExpand(nsIMsgWindow *aMsgWindow)
{
  nsCString password;
  nsresult rv;
  rv = GetPassword(password);
  NS_ENSURE_SUCCESS(rv, rv);

  if (password.IsEmpty())
    return NS_OK;

  rv = ResetFoldersToUnverified(nullptr);

  nsCOMPtr<nsIMsgFolder> rootMsgFolder;
  rv = GetRootFolder(getter_AddRefs(rootMsgFolder));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!rootMsgFolder) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIImapService> imapService = do_GetService(NS_IMAPSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIThread> thread(do_GetCurrentThread());
  rv = imapService->DiscoverAllFolders(rootMsgFolder,
                                       this, aMsgWindow, nullptr);
  return rv;
}

NS_IMETHODIMP
nsImapIncomingServer::VerifyLogon(nsIUrlListener *aUrlListener,
                                  nsIMsgWindow *aMsgWindow, nsIURI **aURL)
{
  nsresult rv;

  nsCOMPtr<nsIImapService> imapService = do_GetService(NS_IMAPSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIMsgFolder> rootFolder;
  // this will create the resource if it doesn't exist, but it shouldn't
  // do anything on disk.
  rv = GetRootFolder(getter_AddRefs(rootFolder));
  NS_ENSURE_SUCCESS(rv, rv);
  return imapService->VerifyLogon(rootFolder, aUrlListener, aMsgWindow, aURL);
}

NS_IMETHODIMP nsImapIncomingServer::PerformBiff(nsIMsgWindow* aMsgWindow)
{
  nsCOMPtr<nsIMsgFolder> rootMsgFolder;
  nsresult rv = GetRootMsgFolder(getter_AddRefs(rootMsgFolder));
  if(NS_SUCCEEDED(rv))
  {
    SetPerformingBiff(true);
    rv = rootMsgFolder->GetNewMessages(aMsgWindow, nullptr);
  }
  return rv;
}


NS_IMETHODIMP
nsImapIncomingServer::CloseCachedConnections()
{
  nsCOMPtr<nsIImapProtocol> connection;
  PR_CEnterMonitor(this);

  // iterate through the connection cache closing open connections.
  int32_t cnt = m_connectionCache.Count();

  for (int32_t i = cnt; i > 0; --i)
  {
    connection = m_connectionCache[i - 1];
    if (connection)
      connection->TellThreadToDie(true);
  }

  PR_CExitMonitor(this);
  return NS_OK;
}

nsresult
nsImapIncomingServer::CreateRootFolderFromUri(const nsCString &serverUri,
                                              nsIMsgFolder **rootFolder)
{
  nsImapMailFolder *newRootFolder = new nsImapMailFolder;
  if (!newRootFolder)
    return NS_ERROR_OUT_OF_MEMORY;
  newRootFolder->Init(serverUri.get());
  NS_ADDREF(*rootFolder = newRootFolder);
  return NS_OK;
}

// nsIImapServerSink impl
// aNewFolder will not be set if we're listing for the subscribe UI, since that's the way 4.x worked.
NS_IMETHODIMP nsImapIncomingServer::PossibleImapMailbox(const nsACString& folderPath,
                                                        char hierarchyDelimiter,
                                                        int32_t boxFlags, bool *aNewFolder)
{
  NS_ENSURE_ARG_POINTER(aNewFolder);
  NS_ENSURE_TRUE(!folderPath.IsEmpty(), NS_ERROR_FAILURE);

  // folderPath is in canonical format, i.e., hierarchy separator has been replaced with '/'
  nsresult rv;
  bool found = false;
  bool haveParent = false;
  nsCOMPtr<nsIMsgImapMailFolder> hostFolder;
  nsCOMPtr<nsIMsgFolder> aFolder;
  bool explicitlyVerify = false;

  *aNewFolder = false;
  nsCOMPtr<nsIMsgFolder> a_nsIFolder;
  rv = GetRootFolder(getter_AddRefs(a_nsIFolder));

  if(NS_FAILED(rv))
    return rv;

  nsCAutoString dupFolderPath(folderPath);
  if (dupFolderPath.Last() == '/')
  {
    dupFolderPath.SetLength(dupFolderPath.Length()-1);
    if (dupFolderPath.IsEmpty())
      return NS_ERROR_FAILURE;
    // *** this is what we did in 4.x in order to list uw folder only
    // mailbox in order to get the \NoSelect flag
    explicitlyVerify = !(boxFlags & kNameSpace);
  }
  if (mDoingSubscribeDialog)
  {
    // Make sure the imapmailfolder object has the right delimiter because the unsubscribed
    // folders (those not in the 'lsub' list) have the delimiter set to the default ('^').
    if (a_nsIFolder && !dupFolderPath.IsEmpty())
    {
      nsCOMPtr<nsIMsgFolder> msgFolder;
      bool isNamespace = false;
      bool noSelect = false;

      rv = a_nsIFolder->FindSubFolder(dupFolderPath, getter_AddRefs(msgFolder));
      NS_ENSURE_SUCCESS(rv,rv);
      m_subscribeFolders.AppendObject(msgFolder);
      noSelect = (boxFlags & kNoselect) != 0;
      nsCOMPtr<nsIMsgImapMailFolder> imapFolder = do_QueryInterface(msgFolder, &rv);
      NS_ENSURE_SUCCESS(rv,rv);
      imapFolder->SetHierarchyDelimiter(hierarchyDelimiter);
      isNamespace = (boxFlags & kNameSpace) != 0;
      if (!isNamespace)
        rv = AddTo(dupFolderPath, mDoingLsub && !noSelect/* add as subscribed */,
                   !noSelect, mDoingLsub /* change if exists */);
      NS_ENSURE_SUCCESS(rv,rv);
      return rv;
    }
  }

  hostFolder = do_QueryInterface(a_nsIFolder, &rv);
  if (NS_FAILED(rv))
    return rv;

  nsCAutoString tempFolderName(dupFolderPath);
  nsCAutoString tokenStr, remStr, changedStr;
  int32_t slashPos = tempFolderName.FindChar('/');
  if (slashPos > 0)
  {
    tokenStr = StringHead(tempFolderName, slashPos);
    remStr = Substring(tempFolderName, slashPos);
  }
  else
    tokenStr.Assign(tempFolderName);

  if ((int32_t(PL_strcasecmp(tokenStr.get(), "INBOX"))==0) && (strcmp(tokenStr.get(), "INBOX") != 0))
    changedStr.Append("INBOX");
  else
    changedStr.Append(tokenStr);

  if (slashPos > 0 )
    changedStr.Append(remStr);

  dupFolderPath.Assign(changedStr);
  nsCAutoString folderName(dupFolderPath);

  nsCAutoString uri;
  nsCString serverUri;
  GetServerURI(serverUri);
  uri.Assign(serverUri);
  int32_t leafPos = folderName.RFindChar('/');
  nsCAutoString parentName(folderName);
  nsCAutoString parentUri(uri);

  if (leafPos > 0)
  {
    // If there is a hierarchy, there is a parent.
    // Don't strip off slash if it's the first character
    parentName.SetLength(leafPos);
    folderName.Cut(0, leafPos + 1);	// get rid of the parent name
    haveParent = true;
    parentUri.Append('/');
    parentUri.Append(parentName);
  }
  if (MsgLowerCaseEqualsLiteral(folderPath, "inbox") &&
    hierarchyDelimiter == kOnlineHierarchySeparatorNil)
  {
    hierarchyDelimiter = '/'; // set to default in this case (as in 4.x)
    hostFolder->SetHierarchyDelimiter(hierarchyDelimiter);
  }

  nsCOMPtr <nsIMsgFolder> child;

  // nsCString possibleName(aSpec->allocatedPathName);
  uri.Append('/');
  uri.Append(dupFolderPath);
  bool caseInsensitive = MsgLowerCaseEqualsLiteral(dupFolderPath, "inbox");
  a_nsIFolder->GetChildWithURI(uri, true, caseInsensitive, getter_AddRefs(child));
  // if we couldn't find this folder by URI, tell the imap code it's a new folder to us
  *aNewFolder = !child;
  if (child)
    found = true;
  if (!found)
  {
    // trying to find/discover the parent
    if (haveParent)
    {
      nsCOMPtr <nsIMsgFolder> parent;
      bool parentIsNew;
      caseInsensitive = MsgLowerCaseEqualsLiteral(parentName, "inbox");
      a_nsIFolder->GetChildWithURI(parentUri, true, caseInsensitive, getter_AddRefs(parent));
      if (!parent /* || parentFolder->GetFolderNeedsAdded()*/)
      {
        PossibleImapMailbox(parentName, hierarchyDelimiter, kNoselect | // be defensive
          ((boxFlags  & //only inherit certain flags from the child
          (kPublicMailbox | kOtherUsersMailbox | kPersonalMailbox))), &parentIsNew);
      }
    }
    rv = hostFolder->CreateClientSubfolderInfo(dupFolderPath, hierarchyDelimiter,boxFlags, false);
    NS_ENSURE_SUCCESS(rv, rv);
    caseInsensitive = MsgLowerCaseEqualsLiteral(dupFolderPath, "inbox");
    a_nsIFolder->GetChildWithURI(uri, true, caseInsensitive, getter_AddRefs(child));
  }
  if (child)
  {
    nsCOMPtr <nsIMsgImapMailFolder> imapFolder = do_QueryInterface(child);
    if (imapFolder)
    {
      nsCAutoString onlineName;
      nsAutoString unicodeName;
      imapFolder->SetVerifiedAsOnlineFolder(true);
      imapFolder->SetHierarchyDelimiter(hierarchyDelimiter);
      if (boxFlags & kImapTrash)
      {
        int32_t deleteModel;
        GetDeleteModel(&deleteModel);
        if (deleteModel == nsMsgImapDeleteModels::MoveToTrash)
          child->SetFlag(nsMsgFolderFlags::Trash);
      }

      imapFolder->SetBoxFlags(boxFlags);
      imapFolder->SetExplicitlyVerify(explicitlyVerify);
      imapFolder->GetOnlineName(onlineName);

      // online name needs to use the correct hierarchy delimiter (I think...)
      // or the canonical path - one or the other, but be consistent.
      MsgReplaceChar(dupFolderPath, '/', hierarchyDelimiter);
      if (hierarchyDelimiter != '/')
        nsImapUrl::UnescapeSlashes(dupFolderPath.BeginWriting());

      // GMail gives us a localized name for the inbox but doesn't let
      // us select that localized name.
      if (boxFlags & kImapInbox)
        imapFolder->SetOnlineName(NS_LITERAL_CSTRING("INBOX"));
      else if (onlineName.IsEmpty() || !onlineName.Equals(dupFolderPath))
        imapFolder->SetOnlineName(dupFolderPath);

      if (hierarchyDelimiter != '/')
        nsImapUrl::UnescapeSlashes(folderName.BeginWriting());
      if (NS_SUCCEEDED(CopyMUTF7toUTF16(folderName, unicodeName)))
        child->SetPrettyName(unicodeName);
    }
  }
  if (!found && child)
    child->SetMsgDatabase(nullptr); // close the db, so we don't hold open all the .msf files for new folders
  return NS_OK;
}

NS_IMETHODIMP nsImapIncomingServer::AddFolderRights(const nsACString& mailboxName, const nsACString& userName,
                                                    const nsACString& rights)
{
  nsCOMPtr <nsIMsgFolder> rootFolder;
  nsresult rv = GetRootFolder(getter_AddRefs(rootFolder));
  if(NS_SUCCEEDED(rv) && rootFolder)
  {
    nsCOMPtr <nsIMsgImapMailFolder> imapRoot = do_QueryInterface(rootFolder);
    if (imapRoot)
    {
      nsCOMPtr <nsIMsgImapMailFolder> foundFolder;
      rv = imapRoot->FindOnlineSubFolder(mailboxName, getter_AddRefs(foundFolder));
      if (NS_SUCCEEDED(rv) && foundFolder)
        return foundFolder->AddFolderRights(userName, rights);
    }
  }
  return rv;
}

NS_IMETHODIMP nsImapIncomingServer::FolderNeedsACLInitialized(const nsACString& folderPath,
                                                              bool *aNeedsACLInitialized)
{
  NS_ENSURE_ARG_POINTER(aNeedsACLInitialized);
  nsCOMPtr <nsIMsgFolder> rootFolder;
  nsresult rv = GetRootFolder(getter_AddRefs(rootFolder));
  if(NS_SUCCEEDED(rv) && rootFolder)
  {
    nsCOMPtr <nsIMsgImapMailFolder> imapRoot = do_QueryInterface(rootFolder);
    if (imapRoot)
    {
      nsCOMPtr <nsIMsgImapMailFolder> foundFolder;
      rv = imapRoot->FindOnlineSubFolder(folderPath, getter_AddRefs(foundFolder));
      if (NS_SUCCEEDED(rv) && foundFolder)
      {
        nsCOMPtr <nsIImapMailFolderSink> folderSink = do_QueryInterface(foundFolder);
        if (folderSink)
          return folderSink->GetFolderNeedsACLListed(aNeedsACLInitialized);
      }
    }
  }
  *aNeedsACLInitialized = false; // maybe we want to say TRUE here...
  return NS_OK;
}

NS_IMETHODIMP nsImapIncomingServer::RefreshFolderRights(const nsACString& folderPath)
{
  nsCOMPtr <nsIMsgFolder> rootFolder;
  nsresult rv = GetRootFolder(getter_AddRefs(rootFolder));
  if(NS_SUCCEEDED(rv) && rootFolder)
  {
    nsCOMPtr <nsIMsgImapMailFolder> imapRoot = do_QueryInterface(rootFolder);
    if (imapRoot)
    {
      nsCOMPtr <nsIMsgImapMailFolder> foundFolder;
      rv = imapRoot->FindOnlineSubFolder(folderPath, getter_AddRefs(foundFolder));
      if (NS_SUCCEEDED(rv) && foundFolder)
        return foundFolder->RefreshFolderRights();
    }
  }
  return rv;
}

nsresult nsImapIncomingServer::GetFolder(const nsACString& name, nsIMsgFolder** pFolder)
{
  NS_ENSURE_ARG_POINTER(pFolder);
  NS_ENSURE_TRUE(!name.IsEmpty(), NS_ERROR_FAILURE);
  nsresult rv;
  *pFolder = nullptr;

  nsCOMPtr<nsIMsgFolder> rootFolder;
  rv = GetRootFolder(getter_AddRefs(rootFolder));
  if (NS_SUCCEEDED(rv) && rootFolder)
  {
    nsCString uri;
    rv = rootFolder->GetURI(uri);
    if (NS_SUCCEEDED(rv) && !uri.IsEmpty())
    {
      nsCAutoString uriString(uri);
      uriString.Append('/');
      uriString.Append(name);
      nsCOMPtr<nsIRDFService> rdf(do_GetService(kRDFServiceCID, &rv));
      NS_ENSURE_SUCCESS(rv, rv);
      nsCOMPtr<nsIRDFResource> res;
      rv = rdf->GetResource(uriString, getter_AddRefs(res));
      if (NS_SUCCEEDED(rv))
      {
        nsCOMPtr<nsIMsgFolder> folder(do_QueryInterface(res, &rv));
        if (NS_SUCCEEDED(rv) && folder)
          folder.swap(*pFolder);
      }
    }
  }
  return rv;
}

NS_IMETHODIMP  nsImapIncomingServer::OnlineFolderDelete(const nsACString& aFolderName)
{
  return NS_OK;
}

NS_IMETHODIMP  nsImapIncomingServer::OnlineFolderCreateFailed(const nsACString& aFolderName)
{
  return NS_OK;
}

NS_IMETHODIMP nsImapIncomingServer::OnlineFolderRename(nsIMsgWindow *msgWindow, const nsACString& oldName,
                                                       const nsACString& newName)
{
  nsresult rv = NS_ERROR_FAILURE;
  if (!newName.IsEmpty())
  {
    nsCOMPtr<nsIMsgFolder> me;
    rv = GetFolder(oldName, getter_AddRefs(me));
    if (NS_FAILED(rv))
      return rv;

    nsCOMPtr<nsIMsgFolder> parent;
    nsCString tmpNewName (newName);
    int32_t folderStart = tmpNewName.RFindChar('/');
    if (folderStart > 0)
    {
      rv = GetFolder(StringHead(tmpNewName, folderStart), getter_AddRefs(parent));
    }
    else  // root is the parent
      rv = GetRootFolder(getter_AddRefs(parent));
    if (NS_SUCCEEDED(rv) && parent)
    {
      nsCOMPtr<nsIMsgImapMailFolder> folder;
      folder = do_QueryInterface(me, &rv);
      if (NS_SUCCEEDED(rv))
      {
        folder->RenameLocal(tmpNewName, parent);
        nsCOMPtr<nsIMsgImapMailFolder> parentImapFolder = do_QueryInterface(parent);

        if (parentImapFolder)
          parentImapFolder->RenameClient(msgWindow, me, oldName, tmpNewName);

        nsCOMPtr <nsIMsgFolder> newFolder;
        nsString unicodeNewName;
        // tmpNewName is imap mod utf7. It needs to be convert to utf8.
        CopyMUTF7toUTF16(tmpNewName, unicodeNewName);
        CopyUTF16toUTF8(unicodeNewName, tmpNewName);
        rv = GetFolder(tmpNewName, getter_AddRefs(newFolder));
        if (NS_SUCCEEDED(rv))
        {
          nsCOMPtr <nsIAtom> folderRenameAtom;
          folderRenameAtom = MsgGetAtom("RenameCompleted");
          newFolder->NotifyFolderEvent(folderRenameAtom);
        }
      }
    }
  }
  return rv;
}

NS_IMETHODIMP  nsImapIncomingServer::FolderIsNoSelect(const nsACString& aFolderName, bool *result)
{
  NS_ENSURE_ARG_POINTER(result);
  nsCOMPtr<nsIMsgFolder> msgFolder;
  nsresult rv = GetFolder(aFolderName, getter_AddRefs(msgFolder));
  if (NS_SUCCEEDED(rv) && msgFolder)
  {
    uint32_t flags;
    msgFolder->GetFlags(&flags);
    *result = ((flags & nsMsgFolderFlags::ImapNoselect) != 0);
  }
  else
   *result = false;
  return NS_OK;
}

NS_IMETHODIMP nsImapIncomingServer::SetFolderAdminURL(const nsACString& aFolderName, const nsACString& aFolderAdminUrl)
{
  nsCOMPtr <nsIMsgFolder> rootFolder;
  nsresult rv = GetRootFolder(getter_AddRefs(rootFolder));
  if(NS_SUCCEEDED(rv) && rootFolder)
  {
    nsCOMPtr <nsIMsgImapMailFolder> imapRoot = do_QueryInterface(rootFolder);
    if (imapRoot)
    {
      nsCOMPtr <nsIMsgImapMailFolder> foundFolder;
      rv = imapRoot->FindOnlineSubFolder(aFolderName, getter_AddRefs(foundFolder));
      if (NS_SUCCEEDED(rv) && foundFolder)
        return foundFolder->SetAdminUrl(aFolderAdminUrl);
    }
  }
  return rv;
}

NS_IMETHODIMP  nsImapIncomingServer::FolderVerifiedOnline(const nsACString& folderName, bool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = false;
  nsCOMPtr<nsIMsgFolder> rootFolder;
  nsresult rv = GetRootFolder(getter_AddRefs(rootFolder));
  if (NS_SUCCEEDED(rv) && rootFolder)
  {
    nsCOMPtr<nsIMsgFolder> folder;
    rv = rootFolder->FindSubFolder(folderName, getter_AddRefs(folder));
    if (NS_SUCCEEDED(rv) && folder)
    {
      nsCOMPtr<nsIMsgImapMailFolder> imapFolder = do_QueryInterface(folder);
      if (imapFolder)
        imapFolder->GetVerifiedAsOnlineFolder(aResult);
    }
  }
  return rv;
}

NS_IMETHODIMP nsImapIncomingServer::DiscoveryDone()
{
  if (mDoingSubscribeDialog)
    return NS_OK;

  nsCOMPtr<nsIMsgFolder> rootMsgFolder;
  nsresult rv = GetRootFolder(getter_AddRefs(rootMsgFolder));
  if (NS_SUCCEEDED(rv) && rootMsgFolder)
  {
    // GetResource() may return a node which is not in the folder
    // tree hierarchy but in the rdf cache in case of the non-existing default
    // Sent, Drafts, and Templates folders. The resouce will be eventually
    // released when the rdf service shuts down. When we create the default
    // folders later on in the imap server, the subsequent GetResource() of the
    // same uri will get us the cached rdf resource which should have the folder
    // flag set appropriately.
    nsCOMPtr<nsIRDFService> rdf(do_GetService("@mozilla.org/rdf/rdf-service;1",
                                              &rv));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIMsgAccountManager> accountMgr = do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIMsgIdentity> identity;
    rv = accountMgr->GetFirstIdentityForServer(this, getter_AddRefs(identity));
    if (NS_SUCCEEDED(rv) && identity)
    {
      nsCString folderUri;
      identity->GetFccFolder(folderUri);
      nsCString existingUri;

      if (CheckSpecialFolder(rdf, folderUri, nsMsgFolderFlags::SentMail,
                             existingUri))
      {
        identity->SetFccFolder(existingUri);
        identity->SetFccFolderPickerMode(NS_LITERAL_CSTRING("1"));
      }
      identity->GetDraftFolder(folderUri);
      if (CheckSpecialFolder(rdf, folderUri, nsMsgFolderFlags::Drafts,
                             existingUri))
      {
        identity->SetDraftFolder(existingUri);
        identity->SetDraftsFolderPickerMode(NS_LITERAL_CSTRING("1"));
      }
      bool archiveEnabled;
      identity->GetArchiveEnabled(&archiveEnabled);
      if (archiveEnabled)
      {
        identity->GetArchiveFolder(folderUri);
        if (CheckSpecialFolder(rdf, folderUri, nsMsgFolderFlags::Archive,
                               existingUri))
        {
          identity->SetArchiveFolder(existingUri);
          identity->SetArchivesFolderPickerMode(NS_LITERAL_CSTRING("1"));
        }
      }
      identity->GetStationeryFolder(folderUri);
      nsCOMPtr<nsIRDFResource> res;
      if (!folderUri.IsEmpty() && NS_SUCCEEDED(rdf->GetResource(folderUri, getter_AddRefs(res))))
      {
        nsCOMPtr<nsIMsgFolder> folder(do_QueryInterface(res, &rv));
        if (NS_SUCCEEDED(rv))
          rv = folder->SetFlag(nsMsgFolderFlags::Templates);
      }
    }

    bool isGMailServer;
    GetIsGMailServer(&isGMailServer);

    // Verify there is only one trash folder. Another might be present if
    // the trash name has been changed. Or we might be a gmail server and
    // want to switch to gmail's trash folder.
    nsCOMPtr<nsIArray> trashFolders;
    rv = rootMsgFolder->GetFoldersWithFlags(nsMsgFolderFlags::Trash,
                                            getter_AddRefs(trashFolders));

    if (NS_SUCCEEDED(rv) && trashFolders)
    {
      uint32_t numFolders;
      trashFolders->GetLength(&numFolders);
      if (numFolders > 1)
      {
        nsAutoString trashName;
        if (NS_SUCCEEDED(GetTrashFolderName(trashName)))
        {
          for (uint32_t i = 0; i < numFolders; i++)
          {
            nsCOMPtr<nsIMsgFolder> trashFolder(do_QueryElementAt(trashFolders, i));
            if (trashFolder)
            {
              // if we're a gmail server, we clear the trash flags from folder(s)
              // without the kImapXListTrash flag. For normal servers, we clear
              // the trash folder flag if the folder name doesn't match the
              // pref trash folder name.
              bool clearFlag;
              if (isGMailServer)
              {
                nsCOMPtr<nsIMsgImapMailFolder> imapFolder(do_QueryInterface(trashFolder));
                int32_t boxFlags;
                imapFolder->GetBoxFlags(&boxFlags);
                clearFlag = !(boxFlags & kImapXListTrash);
              }
              else
              {
                nsAutoString folderName;
                clearFlag = (NS_SUCCEEDED(trashFolder->GetName(folderName))) &&
                 !folderName.Equals(trashName);
              }
              if (clearFlag)
                trashFolder->ClearFlag(nsMsgFolderFlags::Trash);
            }
          }
        }
      }
    }
  }

  bool usingSubscription = true;
  GetUsingSubscription(&usingSubscription);

  nsCOMArray<nsIMsgImapMailFolder> unverifiedFolders;
  GetUnverifiedFolders(unverifiedFolders);

  int32_t count = unverifiedFolders.Count();
  for (int32_t k = 0; k < count; ++k)
  {
    bool explicitlyVerify = false;
    bool hasSubFolders = false;
    uint32_t folderFlags;
    nsCOMPtr<nsIMsgImapMailFolder> currentImapFolder(unverifiedFolders[k]);
    nsCOMPtr<nsIMsgFolder> currentFolder(do_QueryInterface(currentImapFolder, &rv));
    if (NS_FAILED(rv))
      continue;

    currentFolder->GetFlags(&folderFlags);
    if (folderFlags & nsMsgFolderFlags::Virtual) // don't remove virtual folders
      continue;

    if ((!usingSubscription ||
         (NS_SUCCEEDED(currentImapFolder->GetExplicitlyVerify(&explicitlyVerify)) &&
          explicitlyVerify)) ||
        ((NS_SUCCEEDED(currentFolder->GetHasSubFolders(&hasSubFolders)) &&
          hasSubFolders) &&
         !NoDescendentsAreVerified(currentFolder)))
    {
      bool isNamespace;
      currentImapFolder->GetIsNamespace(&isNamespace);
      if (!isNamespace) // don't list namespaces explicitly
      {
        // If there are no subfolders and this is unverified, we don't want to
        // run this url. That is, we want to undiscover the folder.
        // If there are subfolders and no descendants are verified, we want to
        // undiscover all of the folders.
        // Only if there are subfolders and at least one of them is verified
        // do we want to refresh that folder's flags, because it won't be going
        // away.
        currentImapFolder->SetExplicitlyVerify(false);
        currentImapFolder->List();
      }
    }
    else
      DeleteNonVerifiedFolders(currentFolder);
  }

  return rv;
}

// Check if the special folder corresponding to the uri exists. If not, check
// if there already exists a folder with the special folder flag (the server may
// have told us about a folder to use through XLIST). If so, return the uri of
// the existing special folder. If not, set the special flag on the folder so
// it will be there if and when the folder is created.
// Return true if we found an existing special folder different than
// the one specified in prefs, and the one specified by prefs doesn't exist.
bool nsImapIncomingServer::CheckSpecialFolder(nsIRDFService *rdf,
                                                nsCString &folderUri,
                                                uint32_t folderFlag,
                                                nsCString &existingUri)
{
  bool foundExistingFolder = false;
  nsCOMPtr<nsIRDFResource> res;
  nsCOMPtr<nsIMsgFolder> folder;
  nsCOMPtr<nsIMsgFolder> rootMsgFolder;
  nsresult rv = GetRootFolder(getter_AddRefs(rootMsgFolder));
  NS_ENSURE_SUCCESS(rv, false);

  if (!folderUri.IsEmpty() && NS_SUCCEEDED(rdf->GetResource(folderUri, getter_AddRefs(res))))
  {
    folder = do_QueryInterface(res, &rv);
    if (NS_SUCCEEDED(rv))
    {
      nsCOMPtr<nsIMsgFolder> parent;
      folder->GetParent(getter_AddRefs(parent));
      // if the default folder doesn't really exist, check if the server
      // told us about an existing one.
      if (!parent)
      {
        nsCOMPtr<nsIMsgFolder> existingFolder;
        rootMsgFolder->GetFolderWithFlags(folderFlag, getter_AddRefs(existingFolder));
        if (existingFolder)
        {
          existingFolder->GetURI(existingUri);
          foundExistingFolder = true;
        }
      }
      if (!foundExistingFolder)
        folder->SetFlag(folderFlag);

      nsString folderName;
      folder->GetPrettyName(folderName);
      // this will set the localized name based on the folder flag.
      folder->SetPrettyName(folderName);
    }
  }
  return foundExistingFolder;
}

nsresult nsImapIncomingServer::DeleteNonVerifiedFolders(nsIMsgFolder *curFolder)
{
  bool autoUnsubscribeFromNoSelectFolders = true;
  nsresult rv;
  nsCOMPtr<nsIPrefBranch> prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv))
    prefBranch->GetBoolPref("mail.imap.auto_unsubscribe_from_noselect_folders", &autoUnsubscribeFromNoSelectFolders);

  nsCOMPtr<nsISimpleEnumerator> subFolders;

  rv = curFolder->GetSubFolders(getter_AddRefs(subFolders));
  if(NS_SUCCEEDED(rv))
  {
    bool moreFolders;

    while (NS_SUCCEEDED(subFolders->HasMoreElements(&moreFolders)) && moreFolders)
    {
      nsCOMPtr<nsISupports> child;
      rv = subFolders->GetNext(getter_AddRefs(child));
      if (NS_SUCCEEDED(rv) && child)
      {
        bool childVerified = false;
        nsCOMPtr <nsIMsgImapMailFolder> childImapFolder = do_QueryInterface(child, &rv);
        if (NS_SUCCEEDED(rv) && childImapFolder)
        {
          uint32_t flags;

          nsCOMPtr <nsIMsgFolder> childFolder = do_QueryInterface(child, &rv);
          rv = childImapFolder->GetVerifiedAsOnlineFolder(&childVerified);

          rv = childFolder->GetFlags(&flags);
          bool folderIsNoSelectFolder = NS_SUCCEEDED(rv) && ((flags & nsMsgFolderFlags::ImapNoselect) != 0);

          bool usingSubscription = true;
          GetUsingSubscription(&usingSubscription);
          if (usingSubscription)
          {
            bool folderIsNameSpace = false;
            bool noDescendentsAreVerified = NoDescendentsAreVerified(childFolder);
            bool shouldDieBecauseNoSelect = (folderIsNoSelectFolder ?
              ((noDescendentsAreVerified || AllDescendentsAreNoSelect(childFolder)) && !folderIsNameSpace)
              : false);
            if (!childVerified && (noDescendentsAreVerified || shouldDieBecauseNoSelect))
            {
            }
          }
          else
          {
          }
        }
      }
    }
  }

  nsCOMPtr<nsIMsgFolder> parent;
  rv = curFolder->GetParent(getter_AddRefs(parent));

  if (NS_SUCCEEDED(rv) && parent)
  {
    nsCOMPtr<nsIMsgImapMailFolder> imapParent = do_QueryInterface(parent);
    if (imapParent)
      imapParent->RemoveSubFolder(curFolder);
  }

  return rv;
}

bool nsImapIncomingServer::NoDescendentsAreVerified(nsIMsgFolder *parentFolder)
{
  bool nobodyIsVerified = true;
  nsCOMPtr<nsISimpleEnumerator> subFolders;
  nsresult rv = parentFolder->GetSubFolders(getter_AddRefs(subFolders));
  if(NS_SUCCEEDED(rv))
  {
    bool moreFolders;
    while (NS_SUCCEEDED(subFolders->HasMoreElements(&moreFolders)) &&
           moreFolders && nobodyIsVerified)
    {
      nsCOMPtr<nsISupports> child;
      rv = subFolders->GetNext(getter_AddRefs(child));
      if (NS_SUCCEEDED(rv) && child)
      {
        bool childVerified = false;
        nsCOMPtr <nsIMsgImapMailFolder> childImapFolder = do_QueryInterface(child, &rv);
        if (NS_SUCCEEDED(rv) && childImapFolder)
        {
          nsCOMPtr <nsIMsgFolder> childFolder = do_QueryInterface(child, &rv);
          rv = childImapFolder->GetVerifiedAsOnlineFolder(&childVerified);
          nobodyIsVerified = !childVerified && NoDescendentsAreVerified(childFolder);
        }
      }
    }
  }
  return nobodyIsVerified;
}


bool nsImapIncomingServer::AllDescendentsAreNoSelect(nsIMsgFolder *parentFolder)
{
  bool allDescendentsAreNoSelect = true;
  nsCOMPtr<nsISimpleEnumerator> subFolders;
  nsresult rv = parentFolder->GetSubFolders(getter_AddRefs(subFolders));
  if(NS_SUCCEEDED(rv))
  {
    bool moreFolders;
    while (NS_SUCCEEDED(subFolders->HasMoreElements(&moreFolders)) &&
           moreFolders && allDescendentsAreNoSelect)
    {
      nsCOMPtr<nsISupports> child;
      rv = subFolders->GetNext(getter_AddRefs(child));
      if (NS_SUCCEEDED(rv) && child)
      {
        bool childIsNoSelect = false;
        nsCOMPtr <nsIMsgImapMailFolder> childImapFolder = do_QueryInterface(child, &rv);
        if (NS_SUCCEEDED(rv) && childImapFolder)
        {
          uint32_t flags;
          nsCOMPtr <nsIMsgFolder> childFolder = do_QueryInterface(child, &rv);
          rv = childFolder->GetFlags(&flags);
          childIsNoSelect = NS_SUCCEEDED(rv) && (flags & nsMsgFolderFlags::ImapNoselect);
          allDescendentsAreNoSelect = !childIsNoSelect && AllDescendentsAreNoSelect(childFolder);
        }
      }
    }
  }
#if 0
  int numberOfSubfolders = parentFolder->GetNumSubFolders();

  for (int childIndex=0; allDescendantsAreNoSelect && (childIndex < numberOfSubfolders); childIndex++)
  {
    MSG_IMAPFolderInfoMail *currentChild = (MSG_IMAPFolderInfoMail *) parentFolder->GetSubFolder(childIndex);
    allDescendentsAreNoSelect = (currentChild->GetFolderPrefFlags() & MSG_FOLDER_PREF_IMAPNOSELECT) &&
      AllDescendentsAreNoSelect(currentChild);
  }
#endif // 0
  return allDescendentsAreNoSelect;
}

NS_IMETHODIMP
nsImapIncomingServer::PromptLoginFailed(nsIMsgWindow *aMsgWindow,
                                        int32_t *aResult)
{
  nsCAutoString hostName;
  GetRealHostName(hostName);

  return MsgPromptLoginFailed(aMsgWindow, hostName, aResult);
}

NS_IMETHODIMP
nsImapIncomingServer::FEAlert(const nsAString& aString, nsIMsgMailNewsUrl *aUrl)
{
  GetStringBundle();

  if (m_stringBundle)
  {
    nsAutoString hostName;
    nsresult rv = GetPrettyName(hostName);
    if (NS_SUCCEEDED(rv))
    {
      nsString message;
      nsString tempString(aString);
      const PRUnichar *params[] = { hostName.get(), tempString.get() };

      rv = m_stringBundle->FormatStringFromID(IMAP_SERVER_ALERT, params, 2,
                                              getter_Copies(message));
      if (NS_SUCCEEDED(rv))
        return AlertUser(message, aUrl);
    }
  }
  return AlertUser(aString, aUrl);
}

nsresult nsImapIncomingServer::AlertUser(const nsAString& aString,
                                         nsIMsgMailNewsUrl *aUrl)
{
  nsresult rv;
  nsCOMPtr <nsIMsgMailSession> mailSession =
    do_GetService(NS_MSGMAILSESSION_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  return mailSession->AlertUser(aString, aUrl);
}

NS_IMETHODIMP
nsImapIncomingServer::FEAlertWithID(int32_t aMsgId, nsIMsgMailNewsUrl *aUrl)
{
  // don't bother the user if we're shutting down.
  if (m_shuttingDown)
    return NS_OK;

  GetStringBundle();

  nsString message;

  if (m_stringBundle)
  {
    nsAutoString hostName;
    nsresult rv = GetPrettyName(hostName);
    if (NS_SUCCEEDED(rv))
    {
      const PRUnichar *params[] = { hostName.get() };
      rv = m_stringBundle->FormatStringFromID(aMsgId, params, 1,
                                              getter_Copies(message));
      if (NS_SUCCEEDED(rv))
        return AlertUser(message, aUrl);
    }
  }

  // Error condition
  message.AssignLiteral("String ID ");
  message.AppendInt(aMsgId);
  FEAlert(message, aUrl);
  return NS_OK;
}

NS_IMETHODIMP  nsImapIncomingServer::FEAlertFromServer(const nsACString& aString,
                                                       nsIMsgMailNewsUrl *aUrl)
{
  NS_ENSURE_TRUE(!aString.IsEmpty(), NS_OK);

  nsCString message(aString);
  message.Trim(" \t\b\r\n");
  if (message.Last() != '.')
    message.Append('.');

  // Skip over the first two words (the command tag and "NO").
  // Find the first word break.
  int32_t pos = message.FindChar(' ');

  // Find the second word break.
  if (pos != -1)
    pos = message.FindChar(' ', pos + 1);

  // Adjust the message.
  if (pos != -1)
    message = Substring(message, pos + 1);

  nsString hostName;
  GetPrettyName(hostName);

  const PRUnichar *formatStrings[] =
  {
    hostName.get(),
    nullptr,
    nullptr
  };

  uint32_t msgID;
  int32_t numStrings;
  nsString fullMessage;
  nsCOMPtr<nsIImapUrl> imapUrl = do_QueryInterface(aUrl);
  NS_ENSURE_TRUE(imapUrl, NS_ERROR_INVALID_ARG);

  nsImapState imapState;
  nsImapAction imapAction;

  imapUrl->GetRequiredImapState(&imapState);
  imapUrl->GetImapAction(&imapAction);
  nsString folderName;

  NS_ConvertUTF8toUTF16 unicodeMsg(message);

  nsCOMPtr<nsIMsgFolder> folder;
  if (imapState == nsIImapUrl::nsImapSelectedState ||
      imapAction == nsIImapUrl::nsImapFolderStatus)
  {
    aUrl->GetFolder(getter_AddRefs(folder));
    if (folder)
      folder->GetPrettyName(folderName);
    numStrings = 3;
    msgID = IMAP_FOLDER_COMMAND_FAILED;
    formatStrings[1] = folderName.get();
  }
  else
  {
    msgID = IMAP_SERVER_COMMAND_FAILED;
    numStrings = 2;
  }

  formatStrings[numStrings -1] = unicodeMsg.get();

  nsresult rv = GetStringBundle();
  NS_ENSURE_SUCCESS(rv, rv);
  if (m_stringBundle)
  {
    rv = m_stringBundle->FormatStringFromID(msgID,
                                formatStrings, numStrings,
                                getter_Copies(fullMessage));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return AlertUser(fullMessage, aUrl);
}

#define IMAP_MSGS_URL       "chrome://messenger/locale/imapMsgs.properties"

nsresult nsImapIncomingServer::GetStringBundle()
{
  if (m_stringBundle)
    return NS_OK;

  nsCOMPtr<nsIStringBundleService> sBundleService =
    mozilla::services::GetStringBundleService();
  NS_ENSURE_TRUE(sBundleService, NS_ERROR_UNEXPECTED);
  return sBundleService->CreateBundle(IMAP_MSGS_URL, getter_AddRefs(m_stringBundle));
}

NS_IMETHODIMP
nsImapIncomingServer::GetImapStringByID(int32_t aMsgId, nsAString& aString)
{
  nsresult res = NS_OK;
  GetStringBundle();
  if (m_stringBundle)
  {
    nsString res_str;
    res = m_stringBundle->GetStringFromID(aMsgId, getter_Copies(res_str));
    aString.Assign(res_str);
    if (NS_SUCCEEDED(res))
      return res;
  }
  aString.AssignLiteral("String ID ");
  // mscott: FIX ME
  nsString tmpIntStr;
  tmpIntStr.AppendInt(aMsgId);
  aString.Append(tmpIntStr);
  return NS_OK;
}

nsString
nsImapIncomingServer::GetImapStringByName(const nsString &aName)
{
  nsString result;

  GetStringBundle();

  if (m_stringBundle)
  {
    nsresult rv = m_stringBundle->GetStringFromName(aName.get(),
                                                    getter_Copies(result));
    if (NS_SUCCEEDED(rv))
      return result;
  }

  result.AssignLiteral("Failed to get string named ");
  result.Append(aName);
  return result;
}

nsresult nsImapIncomingServer::ResetFoldersToUnverified(nsIMsgFolder *parentFolder)
{
  nsresult rv = NS_OK;
  if (!parentFolder)
  {
    nsCOMPtr<nsIMsgFolder> rootFolder;
    rv = GetRootFolder(getter_AddRefs(rootFolder));
    NS_ENSURE_SUCCESS(rv, rv);
    return ResetFoldersToUnverified(rootFolder);
  }
  else
  {
    nsCOMPtr<nsISimpleEnumerator> subFolders;
    nsCOMPtr<nsIMsgImapMailFolder> imapFolder = do_QueryInterface(parentFolder, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = imapFolder->SetVerifiedAsOnlineFolder(false);
    rv = parentFolder->GetSubFolders(getter_AddRefs(subFolders));
    NS_ENSURE_SUCCESS(rv, rv);

    bool moreFolders = false;
    while (NS_SUCCEEDED(subFolders->HasMoreElements(&moreFolders)) && moreFolders)
    {
      nsCOMPtr<nsISupports> child;
      rv = subFolders->GetNext(getter_AddRefs(child));
      if (NS_SUCCEEDED(rv) && child)
      {
        nsCOMPtr<nsIMsgFolder> childFolder = do_QueryInterface(child, &rv);
        if (NS_SUCCEEDED(rv) && childFolder)
        {
          rv = ResetFoldersToUnverified(childFolder);
          if (NS_FAILED(rv))
            break;
        }
      }
    }
  }
  return rv;
}

void
nsImapIncomingServer::GetUnverifiedFolders(nsCOMArray<nsIMsgImapMailFolder> &aFoldersArray)
{
  nsCOMPtr<nsIMsgFolder> rootFolder;
  if (NS_FAILED(GetRootFolder(getter_AddRefs(rootFolder))) || !rootFolder)
    return;

  nsCOMPtr<nsIMsgImapMailFolder> imapRoot(do_QueryInterface(rootFolder));
  // don't need to verify the root.
  if (imapRoot)
    imapRoot->SetVerifiedAsOnlineFolder(true);

  GetUnverifiedSubFolders(rootFolder, aFoldersArray);
}

void
nsImapIncomingServer::GetUnverifiedSubFolders(nsIMsgFolder *parentFolder,
                                              nsCOMArray<nsIMsgImapMailFolder> &aFoldersArray)
{
  nsCOMPtr<nsIMsgImapMailFolder> imapFolder(do_QueryInterface(parentFolder));

  bool verified = false, explicitlyVerify = false;
  if (imapFolder)
  {
    nsresult rv = imapFolder->GetVerifiedAsOnlineFolder(&verified);
    if (NS_SUCCEEDED(rv))
      rv = imapFolder->GetExplicitlyVerify(&explicitlyVerify);

    if (NS_SUCCEEDED(rv) && (!verified || explicitlyVerify))
      aFoldersArray.AppendObject(imapFolder);
  }

  nsCOMPtr<nsISimpleEnumerator> subFolders;
  if (NS_SUCCEEDED(parentFolder->GetSubFolders(getter_AddRefs(subFolders))))
  {
    bool moreFolders;

    while (NS_SUCCEEDED(subFolders->HasMoreElements(&moreFolders)) && moreFolders)
    {
      nsCOMPtr<nsISupports> child;
      subFolders->GetNext(getter_AddRefs(child));
      if (child)
      {
        nsCOMPtr<nsIMsgFolder> childFolder(do_QueryInterface(child));
        if (childFolder)
          GetUnverifiedSubFolders(childFolder, aFoldersArray);
      }
    }
  }
}

NS_IMETHODIMP nsImapIncomingServer::ForgetSessionPassword()
{
  nsresult rv = nsMsgIncomingServer::ForgetSessionPassword();
  NS_ENSURE_SUCCESS(rv,rv);

  // fix for bugscape bug #15485
  // if we use turbo, and we logout, we need to make sure
  // the server doesn't think it's authenticated.
  // the biff timer continues to fire when you use turbo
  // (see #143848).  if we exited, we've set the password to null
  // but if we're authenticated, and the biff timer goes off
  // we'll still perform biff, because we use m_userAuthenticated
  // to determine if we require a password for biff.
  // (if authenticated, we don't require a password
  // see nsMsgBiffManager::PerformBiff())
  // performing biff without a password will pop up the prompt dialog
  // which is pretty wacky, when it happens after you quit the application
  m_userAuthenticated = false;
  return NS_OK;
}

NS_IMETHODIMP nsImapIncomingServer::GetServerRequiresPasswordForBiff(bool *aServerRequiresPasswordForBiff)
{
  NS_ENSURE_ARG_POINTER(aServerRequiresPasswordForBiff);
  // if the user has already been authenticated, we've got the password
  *aServerRequiresPasswordForBiff = !m_userAuthenticated;
  return NS_OK;
}

NS_IMETHODIMP nsImapIncomingServer::ForgetPassword()
{
  return nsMsgIncomingServer::ForgetPassword();
}


NS_IMETHODIMP
nsImapIncomingServer::AsyncGetPassword(nsIImapProtocol *aProtocol,
                                       bool aNewPasswordRequested,
                                       nsACString &aPassword)
{
  if (m_password.IsEmpty())
  {
    // We're now going to need to do something that will end up with us either
    // poking login manager or prompting the user. We need to ensure we only
    // do one prompt at a time (and login manager could cause a master password
    // prompt), so we need to use the async prompter.
    nsresult rv;
    nsCOMPtr<nsIMsgAsyncPrompter> asyncPrompter =
      do_GetService(NS_MSGASYNCPROMPTER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr<nsIMsgAsyncPromptListener> promptListener(do_QueryInterface(aProtocol));
    rv = asyncPrompter->QueueAsyncAuthPrompt(m_serverKey, aNewPasswordRequested,
                                             promptListener);
    // Explict NS_ENSURE_SUCCESS for debug purposes as errors tend to get
    // hidden.
    NS_ENSURE_SUCCESS(rv, rv);
  }
  if (!m_password.IsEmpty())
    aPassword = m_password;
  return NS_OK;
}

NS_IMETHODIMP
nsImapIncomingServer::PromptPassword(nsIMsgWindow *aMsgWindow,
                                  nsACString &aPassword)
{
  nsString passwordTitle;
  IMAPGetStringByID(IMAP_ENTER_PASSWORD_PROMPT_TITLE, getter_Copies(passwordTitle));
  nsCString promptValue;
  GetRealUsername(promptValue);

  nsCString hostName;
  GetRealHostName(hostName);
  promptValue.Append('@');
  promptValue.Append(hostName);

  nsString passwordText;
  nsresult rv = GetFormattedStringFromID(NS_ConvertASCIItoUTF16(promptValue),
                                         IMAP_ENTER_PASSWORD_PROMPT,
                                         passwordText);
  NS_ENSURE_SUCCESS(rv,rv);

  rv = GetPasswordWithUI(passwordText, passwordTitle, aMsgWindow, aPassword);
  if (NS_SUCCEEDED(rv))
    m_password = aPassword;
  return rv;
}

// for the nsIImapServerSink interface
NS_IMETHODIMP nsImapIncomingServer::SetCapability(eIMAPCapabilityFlags capability)
{
  m_capability = capability;
  SetIsGMailServer((capability & kGmailImapCapability) != 0);
  SetCapabilityACL(capability & kACLCapability);
  SetCapabilityQuota(capability & kQuotaCapability);
  return NS_OK;
}

NS_IMETHODIMP nsImapIncomingServer::SetServerID(const nsACString &aServerID)
{
  return SetServerIDPref(aServerID);
}

NS_IMETHODIMP  nsImapIncomingServer::CommitNamespaces()
{
  nsresult rv;
  nsCOMPtr<nsIImapHostSessionList> hostSession = do_GetService(kCImapHostSessionListCID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  return hostSession->CommitNamespacesForHost(this);
}

NS_IMETHODIMP nsImapIncomingServer::PseudoInterruptMsgLoad(nsIMsgFolder *aImapFolder, nsIMsgWindow *aMsgWindow,
                                                          bool *interrupted)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIImapProtocol> connection;
  PR_CEnterMonitor(this);
  // iterate through the connection cache for a connection that is loading
  // a message in this folder and should be pseudo-interrupted.
  int32_t cnt = m_connectionCache.Count();

  for (int32_t i = 0; i < cnt; ++i)
  {
    connection = m_connectionCache[i];
    if (connection)
      rv = connection->PseudoInterruptMsgLoad(aImapFolder, aMsgWindow, interrupted);
  }

  PR_CExitMonitor(this);
  return rv;
}

NS_IMETHODIMP nsImapIncomingServer::ResetNamespaceReferences()
{
  nsCOMPtr <nsIMsgFolder> rootFolder;
  nsresult rv = GetRootFolder(getter_AddRefs(rootFolder));
  if (NS_SUCCEEDED(rv) && rootFolder)
  {
    nsCOMPtr <nsIMsgImapMailFolder> imapFolder = do_QueryInterface(rootFolder);
    if (imapFolder)
      rv = imapFolder->ResetNamespaceReferences();
  }
  return rv;
}

NS_IMETHODIMP nsImapIncomingServer::SetUserAuthenticated(bool aUserAuthenticated)
{
  m_userAuthenticated = aUserAuthenticated;
  if (aUserAuthenticated)
  {
    nsresult rv;
    nsCOMPtr<nsIMsgAccountManager> accountManager =
      do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    accountManager->SetUserNeedsToAuthenticate(false);
  }
  return NS_OK;
}

NS_IMETHODIMP nsImapIncomingServer::GetUserAuthenticated(bool *aUserAuthenticated)
{
  NS_ENSURE_ARG_POINTER(aUserAuthenticated);
  *aUserAuthenticated = m_userAuthenticated;
  return NS_OK;
}

/* void SetMailServerUrls (in string manageMailAccount, in string manageLists, in string manageFilters); */
NS_IMETHODIMP  nsImapIncomingServer::SetMailServerUrls(const nsACString& manageMailAccount, const nsACString& manageLists,
                                                       const nsACString& manageFilters)
{
  return SetManageMailAccountUrl(manageMailAccount);
}

NS_IMETHODIMP nsImapIncomingServer::SetManageMailAccountUrl(const nsACString& manageMailAccountUrl)
{
  m_manageMailAccountUrl = manageMailAccountUrl;
  return NS_OK;
}

NS_IMETHODIMP nsImapIncomingServer::GetManageMailAccountUrl(nsACString& manageMailAccountUrl)
{
  manageMailAccountUrl = m_manageMailAccountUrl;
  return NS_OK;
}

NS_IMETHODIMP
nsImapIncomingServer::StartPopulatingWithUri(nsIMsgWindow *aMsgWindow, bool aForceToServer /*ignored*/, const char *uri)
{
  NS_ENSURE_ARG_POINTER (uri);

  nsresult rv;
  mDoingSubscribeDialog = true;

  rv = EnsureInner();
  NS_ENSURE_SUCCESS(rv,rv);
  rv = mInner->StartPopulatingWithUri(aMsgWindow, aForceToServer, uri);
  NS_ENSURE_SUCCESS(rv,rv);

  // imap always uses the canonical delimiter form of paths for subscribe ui.
  rv = SetDelimiter('/');
  NS_ENSURE_SUCCESS(rv,rv);

  rv = SetShowFullName(false);
  NS_ENSURE_SUCCESS(rv,rv);

  nsCString serverUri;
  rv = GetServerURI(serverUri);
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr<nsIImapService> imapService = do_GetService(NS_IMAPSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

/*
    if uri = imap://user@host/foo/bar, the serverUri is imap://user@host
    to get path from uri, skip over imap://user@host + 1 (for the /)
*/
  const char *path = uri + serverUri.Length() + 1;
  return imapService->GetListOfFoldersWithPath(this, aMsgWindow, nsDependentCString(path));
}

NS_IMETHODIMP
nsImapIncomingServer::StartPopulating(nsIMsgWindow *aMsgWindow, bool aForceToServer /*ignored*/, bool aGetOnlyNew)
{
  nsresult rv;
  mDoingSubscribeDialog = true;

  rv = EnsureInner();
  NS_ENSURE_SUCCESS(rv,rv);
  rv = mInner->StartPopulating(aMsgWindow, aForceToServer, aGetOnlyNew);
  NS_ENSURE_SUCCESS(rv,rv);

  // imap always uses the canonical delimiter form of paths for subscribe ui.
  rv = SetDelimiter('/');
  NS_ENSURE_SUCCESS(rv,rv);

  rv = SetShowFullName(false);
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr<nsIImapService> imapService = do_GetService(NS_IMAPSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);
  return imapService->GetListOfFoldersOnServer(this, aMsgWindow);
}

NS_IMETHODIMP
nsImapIncomingServer::OnStartRunningUrl(nsIURI *url)
{
  return NS_OK;
}

NS_IMETHODIMP
nsImapIncomingServer::OnStopRunningUrl(nsIURI *url, nsresult exitCode)
{
  nsresult rv = exitCode;

  // xxx todo get msgWindow from url
  nsCOMPtr<nsIMsgWindow> msgWindow;
  nsCOMPtr<nsIImapUrl> imapUrl = do_QueryInterface(url);
  if (imapUrl) {
    nsImapAction imapAction = nsIImapUrl::nsImapTest;
    imapUrl->GetImapAction(&imapAction);
    switch (imapAction) {
    case nsIImapUrl::nsImapDiscoverAllAndSubscribedBoxesUrl:
    case nsIImapUrl::nsImapDiscoverChildrenUrl:
      rv = UpdateSubscribed();
      NS_ENSURE_SUCCESS(rv, rv);
      mDoingSubscribeDialog = false;
      rv = StopPopulating(msgWindow);
      NS_ENSURE_SUCCESS(rv, rv);
      break;
    case nsIImapUrl::nsImapDiscoverAllBoxesUrl:
      if (NS_SUCCEEDED(exitCode))
        DiscoveryDone();
      break;
    case nsIImapUrl::nsImapFolderStatus:
    {
      nsCOMPtr<nsIMsgFolder> msgFolder;
      nsCOMPtr<nsIMsgMailNewsUrl> mailUrl = do_QueryInterface(imapUrl);
      mailUrl->GetFolder(getter_AddRefs(msgFolder));
      if (msgFolder)
      {
        nsresult rv;
        nsCOMPtr<nsIMsgMailSession> session =
                 do_GetService(NS_MSGMAILSESSION_CONTRACTID, &rv);
        NS_ENSURE_SUCCESS(rv, rv);
        bool folderOpen;
        rv = session->IsFolderOpenInWindow(msgFolder, &folderOpen);
        if (NS_SUCCEEDED(rv) && !folderOpen && msgFolder)
          msgFolder->SetMsgDatabase(nullptr);
        nsCOMPtr<nsIMsgImapMailFolder> imapFolder = do_QueryInterface(msgFolder);
        m_foldersToStat.RemoveObject(imapFolder);
      }
      // if we get an error running the url, it's better
      // not to chain the next url.
      if (NS_FAILED(exitCode) && exitCode != NS_MSG_ERROR_IMAP_COMMAND_FAILED)
        m_foldersToStat.Clear();
      if (m_foldersToStat.Count() > 0)
        m_foldersToStat[0]->UpdateStatus(this, nullptr);
      break;
    }
    default:
        break;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsImapIncomingServer::SetIncomingServer(nsIMsgIncomingServer *aServer)
{
  nsresult rv = EnsureInner();
  NS_ENSURE_SUCCESS(rv,rv);
  return mInner->SetIncomingServer(aServer);
}

NS_IMETHODIMP
nsImapIncomingServer::SetShowFullName(bool showFullName)
{
  nsresult rv = EnsureInner();
  NS_ENSURE_SUCCESS(rv,rv);
  return mInner->SetShowFullName(showFullName);
}

NS_IMETHODIMP
nsImapIncomingServer::GetDelimiter(char *aDelimiter)
{
  nsresult rv = EnsureInner();
  NS_ENSURE_SUCCESS(rv,rv);
  return mInner->GetDelimiter(aDelimiter);
}

NS_IMETHODIMP
nsImapIncomingServer::SetDelimiter(char aDelimiter)
{
  nsresult rv = EnsureInner();
  NS_ENSURE_SUCCESS(rv,rv);
  return mInner->SetDelimiter(aDelimiter);
}

NS_IMETHODIMP
nsImapIncomingServer::SetAsSubscribed(const nsACString &path)
{
  nsresult rv = EnsureInner();
  NS_ENSURE_SUCCESS(rv,rv);
  return mInner->SetAsSubscribed(path);
}

NS_IMETHODIMP
nsImapIncomingServer::UpdateSubscribed()
{
  return NS_OK;
}

NS_IMETHODIMP
nsImapIncomingServer::AddTo(const nsACString &aName, bool addAsSubscribed,
                            bool aSubscribable, bool changeIfExists)
{
  nsresult rv = EnsureInner();
  NS_ENSURE_SUCCESS(rv,rv);

  // RFC 3501 allows UTF-8 in addition to modified UTF-7
  // If it's not UTF-8, it cannot be MUTF7, either. We just ignore it.
  // (otherwise we'll crash. see #63186)
  if (!MsgIsUTF8(aName))
    return NS_OK;

  if (!NS_IsAscii(aName.BeginReading(), aName.Length())) {
    nsCAutoString name;
    CopyUTF16toMUTF7(NS_ConvertUTF8toUTF16(aName), name);
    return mInner->AddTo(name, addAsSubscribed, aSubscribable, changeIfExists);
  }
  return mInner->AddTo(aName, addAsSubscribed, aSubscribable, changeIfExists);
}

NS_IMETHODIMP
nsImapIncomingServer::StopPopulating(nsIMsgWindow *aMsgWindow)
{
  nsCOMPtr<nsISubscribeListener> listener;
  (void) GetSubscribeListener(getter_AddRefs(listener));

  if (listener)
    listener->OnDonePopulating();

  nsresult rv = EnsureInner();
  NS_ENSURE_SUCCESS(rv,rv);
  return mInner->StopPopulating(aMsgWindow);
}


NS_IMETHODIMP
nsImapIncomingServer::SubscribeCleanup()
{
  m_subscribeFolders.Clear();
  return ClearInner();
}

NS_IMETHODIMP
nsImapIncomingServer::SetSubscribeListener(nsISubscribeListener *aListener)
{
  nsresult rv = EnsureInner();
  NS_ENSURE_SUCCESS(rv,rv);
  return mInner->SetSubscribeListener(aListener);
}

NS_IMETHODIMP
nsImapIncomingServer::GetSubscribeListener(nsISubscribeListener **aListener)
{
  nsresult rv = EnsureInner();
  NS_ENSURE_SUCCESS(rv,rv);
  return mInner->GetSubscribeListener(aListener);
}

NS_IMETHODIMP
nsImapIncomingServer::Subscribe(const PRUnichar *aName)
{
  NS_ENSURE_ARG_POINTER(aName);
  
  return SubscribeToFolder(nsDependentString(aName), true, nullptr);
}

NS_IMETHODIMP
nsImapIncomingServer::Unsubscribe(const PRUnichar *aName)
{
  NS_ENSURE_ARG_POINTER(aName);

  return SubscribeToFolder(nsDependentString(aName), false, nullptr);
}

NS_IMETHODIMP
nsImapIncomingServer::SubscribeToFolder(const nsAString& aName, bool subscribe, nsIURI **aUri)
{
  nsresult rv;
  nsCOMPtr<nsIImapService> imapService = do_GetService(NS_IMAPSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIMsgFolder> rootMsgFolder;
  rv = GetRootFolder(getter_AddRefs(rootMsgFolder));
  NS_ENSURE_SUCCESS(rv, rv);

  // Locate the folder so that the correct hierarchical delimiter is used in the
  // folder pathnames, otherwise root's (ie, '^') is used and this is wrong.

  // aName is not a genuine UTF-16 but just a zero-padded modified UTF-7
  NS_LossyConvertUTF16toASCII folderCName(aName);
  nsCOMPtr<nsIMsgFolder> msgFolder;
  if (rootMsgFolder && !aName.IsEmpty())
    rv = rootMsgFolder->FindSubFolder(folderCName, getter_AddRefs(msgFolder));

  nsCOMPtr<nsIThread> thread(do_GetCurrentThread());

  nsAutoString unicodeName;
  rv = CopyMUTF7toUTF16(folderCName, unicodeName);
  NS_ENSURE_SUCCESS(rv, rv);

  if (subscribe)
    rv = imapService->SubscribeFolder(msgFolder, unicodeName, nullptr, aUri);
  else
    rv = imapService->UnsubscribeFolder(msgFolder, unicodeName, nullptr, nullptr);
  return rv;
}

NS_IMETHODIMP
nsImapIncomingServer::SetDoingLsub(bool doingLsub)
{
  mDoingLsub = doingLsub;
  return NS_OK;
}

NS_IMETHODIMP
nsImapIncomingServer::GetDoingLsub(bool *doingLsub)
{
  NS_ENSURE_ARG_POINTER(doingLsub);
  *doingLsub = mDoingLsub;
  return NS_OK;
}

NS_IMETHODIMP
nsImapIncomingServer::ReDiscoverAllFolders()
{
  return PerformExpand(nullptr);
}

NS_IMETHODIMP
nsImapIncomingServer::SetState(const nsACString &path, bool state,
                               bool *stateChanged)
{
  nsresult rv = EnsureInner();
  NS_ENSURE_SUCCESS(rv,rv);
  return mInner->SetState(path, state, stateChanged);
}

NS_IMETHODIMP
nsImapIncomingServer::HasChildren(const nsACString &path, bool *aHasChildren)
{
  nsresult rv = EnsureInner();
  NS_ENSURE_SUCCESS(rv,rv);
  return mInner->HasChildren(path, aHasChildren);
}

NS_IMETHODIMP
nsImapIncomingServer::IsSubscribed(const nsACString &path,
                                   bool *aIsSubscribed)
{
  nsresult rv = EnsureInner();
  NS_ENSURE_SUCCESS(rv,rv);
  return mInner->IsSubscribed(path, aIsSubscribed);
}

NS_IMETHODIMP
nsImapIncomingServer::IsSubscribable(const nsACString &path, bool *aIsSubscribable)
{
  nsresult rv = EnsureInner();
  NS_ENSURE_SUCCESS(rv,rv);
  return mInner->IsSubscribable(path, aIsSubscribable);
}

NS_IMETHODIMP
nsImapIncomingServer::GetLeafName(const nsACString &path, nsAString &aLeafName)
{
  nsresult rv = EnsureInner();
  NS_ENSURE_SUCCESS(rv,rv);
  return mInner->GetLeafName(path, aLeafName);
}

NS_IMETHODIMP
nsImapIncomingServer::GetFirstChildURI(const nsACString &path, nsACString &aResult)
{
  nsresult rv = EnsureInner();
  NS_ENSURE_SUCCESS(rv,rv);
  return mInner->GetFirstChildURI(path, aResult);
}


NS_IMETHODIMP
nsImapIncomingServer::GetChildren(const nsACString &aPath,
                                  nsISimpleEnumerator **aResult)
{
  nsresult rv = EnsureInner();
  NS_ENSURE_SUCCESS(rv,rv);
  return mInner->GetChildren(aPath, aResult);
}

nsresult
nsImapIncomingServer::EnsureInner()
{
  nsresult rv = NS_OK;

  if (mInner)
    return NS_OK;

  mInner = do_CreateInstance(kSubscribableServerCID,&rv);
  NS_ENSURE_SUCCESS(rv,rv);
  return SetIncomingServer(this);
}

nsresult
nsImapIncomingServer::ClearInner()
{
  nsresult rv = NS_OK;
  if (mInner)
  {
    rv = mInner->SetSubscribeListener(nullptr);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = mInner->SetIncomingServer(nullptr);
    NS_ENSURE_SUCCESS(rv,rv);
    mInner = nullptr;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsImapIncomingServer::CommitSubscribeChanges()
{
  return ReDiscoverAllFolders();
}

NS_IMETHODIMP
nsImapIncomingServer::GetCanBeDefaultServer(bool *canBeDefaultServer)
{
  NS_ENSURE_ARG_POINTER(canBeDefaultServer);
  *canBeDefaultServer = true;
  return NS_OK;
}

NS_IMETHODIMP
nsImapIncomingServer::GetCanCompactFoldersOnServer(bool *canCompactFoldersOnServer)
{
  NS_ENSURE_ARG_POINTER(canCompactFoldersOnServer);
  // Initialize canCompactFoldersOnServer true, a default value for IMAP
  *canCompactFoldersOnServer = true;
  GetPrefForServerAttribute("canCompactFoldersOnServer", canCompactFoldersOnServer);
  return NS_OK;
}

NS_IMETHODIMP
nsImapIncomingServer::GetCanUndoDeleteOnServer(bool *canUndoDeleteOnServer)
{
  NS_ENSURE_ARG_POINTER(canUndoDeleteOnServer);
  // Initialize canUndoDeleteOnServer true, a default value for IMAP
  *canUndoDeleteOnServer = true;
  GetPrefForServerAttribute("canUndoDeleteOnServer", canUndoDeleteOnServer);
  return NS_OK;
}

NS_IMETHODIMP
nsImapIncomingServer::GetCanSearchMessages(bool *canSearchMessages)
{
  NS_ENSURE_ARG_POINTER(canSearchMessages);
  // Initialize canSearchMessages true, a default value for IMAP
  *canSearchMessages = true;
  GetPrefForServerAttribute("canSearchMessages", canSearchMessages);
  return NS_OK;
}

NS_IMETHODIMP
nsImapIncomingServer::GetCanEmptyTrashOnExit(bool *canEmptyTrashOnExit)
{
  NS_ENSURE_ARG_POINTER(canEmptyTrashOnExit);
  // Initialize canEmptyTrashOnExit true, a default value for IMAP
  *canEmptyTrashOnExit = true;
  GetPrefForServerAttribute("canEmptyTrashOnExit", canEmptyTrashOnExit);
  return NS_OK;
}

nsresult
nsImapIncomingServer::CreateHostSpecificPrefName(const char *prefPrefix, nsCAutoString &prefName)
{
  NS_ENSURE_ARG_POINTER(prefPrefix);

  nsCString hostName;
  nsresult rv = GetHostName(hostName);
  NS_ENSURE_SUCCESS(rv,rv);

  prefName = prefPrefix;
  prefName.Append('.');
  prefName.Append(hostName);
  return NS_OK;
}

NS_IMETHODIMP
nsImapIncomingServer::GetSupportsDiskSpace(bool *aSupportsDiskSpace)
{
  NS_ENSURE_ARG_POINTER(aSupportsDiskSpace);
  nsCAutoString prefName;
  nsresult rv = CreateHostSpecificPrefName("default_supports_diskspace", prefName);
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr<nsIPrefBranch> prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv))
     rv = prefBranch->GetBoolPref(prefName.get(), aSupportsDiskSpace);

  // Couldn't get the default value with the hostname.
  // Fall back on IMAP default value
  if (NS_FAILED(rv)) // set default value
     *aSupportsDiskSpace = true;
  return NS_OK;
}

// count number of non-busy connections in cache
NS_IMETHODIMP
nsImapIncomingServer::GetNumIdleConnections(int32_t *aNumIdleConnections)
{
  NS_ENSURE_ARG_POINTER(aNumIdleConnections);
  *aNumIdleConnections = 0;

  nsresult rv = NS_OK;
  nsCOMPtr<nsIImapProtocol> connection;
  bool isBusy = false;
  bool isInboxConnection;
  PR_CEnterMonitor(this);

  int32_t cnt = m_connectionCache.Count();

  // loop counting idle connections
  for (int32_t i = 0; i < cnt; ++i)
  {
    connection = m_connectionCache[i];
    if (connection)
    {
      rv = connection->IsBusy(&isBusy, &isInboxConnection);
      if (NS_FAILED(rv))
        continue;
      if (!isBusy)
        (*aNumIdleConnections)++;
    }
  }
  PR_CExitMonitor(this);
  return rv;
}


/**
 * Get the preference that tells us whether the imap server in question allows
 * us to create subfolders. Some ISPs might not want users to create any folders
 * besides the existing ones.
 * We do want to identify all those servers that don't allow creation of subfolders
 * and take them out of the account picker in the Copies and Folder panel.
 */
NS_IMETHODIMP
nsImapIncomingServer::GetCanCreateFoldersOnServer(bool *aCanCreateFoldersOnServer)
{
  NS_ENSURE_ARG_POINTER(aCanCreateFoldersOnServer);
  // Initialize aCanCreateFoldersOnServer true, a default value for IMAP
  *aCanCreateFoldersOnServer = true;
  GetPrefForServerAttribute("canCreateFolders", aCanCreateFoldersOnServer);
  return NS_OK;
}

NS_IMETHODIMP
nsImapIncomingServer::GetOfflineSupportLevel(int32_t *aSupportLevel)
{
  NS_ENSURE_ARG_POINTER(aSupportLevel);
  nsresult rv = NS_OK;

  rv = GetIntValue("offline_support_level", aSupportLevel);
  if (*aSupportLevel != OFFLINE_SUPPORT_LEVEL_UNDEFINED)
    return rv;

  nsCAutoString prefName;
  rv = CreateHostSpecificPrefName("default_offline_support_level", prefName);
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr<nsIPrefBranch> prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv))
    rv = prefBranch->GetIntPref(prefName.get(), aSupportLevel);

  // Couldn't get the pref value with the hostname.
  // Fall back on IMAP default value
  if (NS_FAILED(rv)) // set default value
    *aSupportLevel = OFFLINE_SUPPORT_LEVEL_REGULAR;
  return NS_OK;
}

// Called only during the migration process. This routine enables the generation of
// unique account name based on the username, hostname and the port. If the port
// is valid and not a default one, it will be appended to the account name.
NS_IMETHODIMP
nsImapIncomingServer::GeneratePrettyNameForMigration(nsAString& aPrettyName)
{
  nsCString userName;
  nsCString hostName;

/**
  * Pretty name for migrated account is of format username@hostname:<port>,
  * provided the port is valid and not the default
*/
  // Get user name to construct pretty name
  nsresult rv = GetUsername(userName);
  NS_ENSURE_SUCCESS(rv, rv);

  // Get host name to construct pretty name
  rv = GetHostName(hostName);
  NS_ENSURE_SUCCESS(rv, rv);

  int32_t defaultServerPort;
  int32_t defaultSecureServerPort;

  nsCOMPtr <nsIMsgProtocolInfo> protocolInfo = do_GetService("@mozilla.org/messenger/protocol/info;1?type=imap", &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  // Get the default port
  rv = protocolInfo->GetDefaultServerPort(false, &defaultServerPort);
  NS_ENSURE_SUCCESS(rv,rv);

  // Get the default secure port
  rv = protocolInfo->GetDefaultServerPort(true, &defaultSecureServerPort);
  NS_ENSURE_SUCCESS(rv,rv);

  // Get the current server port
  int32_t serverPort = PORT_NOT_SET;
  rv = GetPort(&serverPort);
  NS_ENSURE_SUCCESS(rv,rv);

  // Is the server secure ?
  int32_t socketType;
  rv = GetSocketType(&socketType);
  NS_ENSURE_SUCCESS(rv,rv);
  bool isSecure = (socketType == nsMsgSocketType::SSL);

  // Is server port a default port ?
  bool isItDefaultPort = false;
  if (((serverPort == defaultServerPort) && !isSecure)||
      ((serverPort == defaultSecureServerPort) && isSecure))
      isItDefaultPort = true;

  // Construct pretty name from username and hostname
  nsAutoString constructedPrettyName;
  CopyASCIItoUTF16(userName,constructedPrettyName);
  constructedPrettyName.Append('@');
  constructedPrettyName.Append(NS_ConvertASCIItoUTF16(hostName));

  // If the port is valid and not default, add port value to the pretty name
  if ((serverPort > 0) && (!isItDefaultPort)) {
    constructedPrettyName.Append(':');
    constructedPrettyName.AppendInt(serverPort);
  }

  // Format the pretty name
  return GetFormattedStringFromID(constructedPrettyName, IMAP_DEFAULT_ACCOUNT_NAME, aPrettyName);
}

nsresult
nsImapIncomingServer::GetFormattedStringFromID(const nsAString& aValue, int32_t aID, nsAString& aResult)
{
  nsresult rv = GetStringBundle();
  if (m_stringBundle)
  {
    nsString tmpVal (aValue);
    const PRUnichar *formatStrings[] =
    {
      tmpVal.get(),
    };

    nsString result;
    rv = m_stringBundle->FormatStringFromID(aID,
                                            formatStrings, 1,
                                            getter_Copies(result));
    aResult.Assign(result);
  }
  return rv;
}

nsresult
nsImapIncomingServer::GetPrefForServerAttribute(const char *prefSuffix, bool *prefValue)
{
  // Any caller of this function must initialize prefValue with a default value
  // as this code will not set prefValue when the pref does not exist and return
  // NS_OK anyway

  if (!mPrefBranch)
    return NS_ERROR_NOT_INITIALIZED;

  NS_ENSURE_ARG_POINTER(prefValue);

  if (NS_FAILED(mPrefBranch->GetBoolPref(prefSuffix, prefValue)))
    mDefPrefBranch->GetBoolPref(prefSuffix, prefValue);

  return NS_OK;
}

NS_IMETHODIMP
nsImapIncomingServer::GetCanFileMessagesOnServer(bool *aCanFileMessagesOnServer)
{
  NS_ENSURE_ARG_POINTER(aCanFileMessagesOnServer);
  // Initialize aCanFileMessagesOnServer true, a default value for IMAP
  *aCanFileMessagesOnServer = true;
  GetPrefForServerAttribute("canFileMessages", aCanFileMessagesOnServer);
  return NS_OK;
}

NS_IMETHODIMP
nsImapIncomingServer::SetSearchValue(const nsAString &searchValue)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsImapIncomingServer::GetSupportsSubscribeSearch(bool *retVal)
{
  NS_ENSURE_ARG_POINTER(retVal);
  *retVal = false;
  return NS_OK;
}

NS_IMETHODIMP
nsImapIncomingServer::GetFilterScope(nsMsgSearchScopeValue *filterScope)
{
  NS_ENSURE_ARG_POINTER(filterScope);
  // If the inbox is enabled for offline use, then use the offline filter
  // scope, else use the online filter scope.
  //
  // XXX We use the same scope for all folders with the same incoming server,
  // yet it is possible to set the offline flag separately for each folder.
  // Manual filters could perhaps check the offline status of each folder,
  // though it's hard to see how to make that work since we only store filters
  // per server.
  //
  nsCOMPtr<nsIMsgFolder> rootMsgFolder;
  nsresult rv = GetRootMsgFolder(getter_AddRefs(rootMsgFolder));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIMsgFolder> offlineInboxMsgFolder;
  rv = rootMsgFolder->GetFolderWithFlags(nsMsgFolderFlags::Inbox |
                                           nsMsgFolderFlags::Offline,
                                         getter_AddRefs(offlineInboxMsgFolder));

  *filterScope = offlineInboxMsgFolder ? nsMsgSearchScope::offlineMailFilter
                                       : nsMsgSearchScope::onlineMailFilter;
  return NS_OK;
}

NS_IMETHODIMP
nsImapIncomingServer::GetSearchScope(nsMsgSearchScopeValue *searchScope)
{
  NS_ENSURE_ARG_POINTER(searchScope);
 *searchScope = WeAreOffline() ? nsMsgSearchScope::offlineMail : nsMsgSearchScope::onlineMail;
  return NS_OK;
}

// This is a recursive function. It gets new messages for current folder
// first if it is marked, then calls itself recursively for each subfolder.
NS_IMETHODIMP
nsImapIncomingServer::GetNewMessagesForNonInboxFolders(nsIMsgFolder *aFolder,
                                                       nsIMsgWindow *aWindow,
                                                       bool forceAllFolders,
                                                       bool performingBiff)
{
  NS_ENSURE_ARG_POINTER(aFolder);
  static bool gGotStatusPref = false;
  static bool gUseStatus = false;

  bool isServer;
  (void) aFolder->GetIsServer(&isServer);
  // Check this folder for new messages if it is marked to be checked
  // or if we are forced to check all folders
  uint32_t flags = 0;
  aFolder->GetFlags(&flags);
  nsresult rv;
  nsCOMPtr<nsIMsgImapMailFolder> imapFolder = do_QueryInterface(aFolder, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  bool canOpen;
  imapFolder->GetCanOpenFolder(&canOpen);
  if (canOpen && ((forceAllFolders &&
                 !(flags & (nsMsgFolderFlags::Inbox | nsMsgFolderFlags::Trash |
                   nsMsgFolderFlags::Junk | nsMsgFolderFlags::Virtual))) ||
                flags & nsMsgFolderFlags::CheckNew))
  {
    // Get new messages for this folder.
    aFolder->SetGettingNewMessages(true);
    if (performingBiff)
      imapFolder->SetPerformingBiff(true);
    bool isOpen = false;
    nsCOMPtr <nsIMsgMailSession> mailSession = do_GetService(NS_MSGMAILSESSION_CONTRACTID);
    if (mailSession && aFolder)
      mailSession->IsFolderOpenInWindow(aFolder, &isOpen);
    // eventually, the gGotStatusPref should go away, once we work out the kinks
    // from using STATUS.
    if (!gGotStatusPref)
    {
      nsCOMPtr<nsIPrefBranch> prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID);
      if(prefBranch)
        prefBranch->GetBoolPref("mail.imap.use_status_for_biff", &gUseStatus);
      gGotStatusPref = true;
    }
    if (gUseStatus && !isOpen)
    {
      if (!isServer && m_foldersToStat.IndexOf(imapFolder) == -1)
        m_foldersToStat.AppendObject(imapFolder);
    }
    else
      aFolder->UpdateFolder(aWindow);
  }

  // Loop through all subfolders to get new messages for them.
  nsCOMPtr<nsISimpleEnumerator> enumerator;
  rv = aFolder->GetSubFolders(getter_AddRefs(enumerator));
  if (NS_FAILED(rv))
    return rv;

  bool hasMore;
  while (NS_SUCCEEDED(enumerator->HasMoreElements(&hasMore)) && hasMore)
  {
    nsCOMPtr<nsISupports> item;
    enumerator->GetNext(getter_AddRefs(item));

    nsCOMPtr<nsIMsgFolder> msgFolder(do_QueryInterface(item));
    if (!msgFolder)
    {
      NS_WARNING("Not an nsIMsgFolder");
      continue;
    }
    GetNewMessagesForNonInboxFolders(msgFolder, aWindow, forceAllFolders,
                                     performingBiff);
  }
  if (isServer && m_foldersToStat.Count() > 0)
    m_foldersToStat[0]->UpdateStatus(this, nullptr);
  return NS_OK;
}

NS_IMETHODIMP
nsImapIncomingServer::GetArbitraryHeaders(nsACString &aResult)
{
  nsCOMPtr <nsIMsgFilterList> filterList;
  nsresult rv = GetFilterList(nullptr, getter_AddRefs(filterList));
  NS_ENSURE_SUCCESS(rv,rv);
  return filterList->GetArbitraryHeaders(aResult);
}

NS_IMETHODIMP
nsImapIncomingServer::GetShowAttachmentsInline(bool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = true; // true per default
  nsresult rv;
  nsCOMPtr<nsIPrefBranch> prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  prefBranch->GetBoolPref("mail.inline_attachments", aResult);
  return NS_OK; // In case this pref is not set we need to return NS_OK.
}

NS_IMETHODIMP nsImapIncomingServer::SetSocketType(int32_t aSocketType)
{
  int32_t oldSocketType;
  nsresult rv = GetSocketType(&oldSocketType);
  if (NS_SUCCEEDED(rv) && oldSocketType != aSocketType)
    CloseCachedConnections();
  return nsMsgIncomingServer::SetSocketType(aSocketType);
}

NS_IMETHODIMP
nsImapIncomingServer::OnUserOrHostNameChanged(const nsACString& oldName,
                                              const nsACString& newName,
                                              bool hostnameChanged)
{
  nsresult rv;
  // 1. Do common things in the base class.
  rv = nsMsgIncomingServer::OnUserOrHostNameChanged(oldName, newName, hostnameChanged);
  NS_ENSURE_SUCCESS(rv,rv);

  // 2. Reset 'HaveWeEverDiscoveredFolders' flag so the new folder list can be
  //    reloaded (ie, DiscoverMailboxList() will be invoked in nsImapProtocol).
  nsCOMPtr<nsIImapHostSessionList> hostSessionList = do_GetService(kCImapHostSessionListCID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCAutoString serverKey;
  rv = GetKey(serverKey);
  NS_ENSURE_SUCCESS(rv, rv);
  hostSessionList->SetHaveWeEverDiscoveredFoldersForHost(serverKey.get(), false);
  // 3. Make all the existing folders 'unverified' so that they can be
  //    removed from the folder pane after users log into the new server.
  ResetFoldersToUnverified(nullptr);
  return NS_OK;
}

// use canonical format in originalUri & convertedUri
NS_IMETHODIMP
nsImapIncomingServer::GetUriWithNamespacePrefixIfNecessary(int32_t namespaceType,
                                                           const nsACString& originalUri,
                                                           nsACString& convertedUri)
{
  nsresult rv = NS_OK;
  nsCAutoString serverKey;
  rv = GetKey(serverKey);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIImapHostSessionList> hostSessionList = do_GetService(kCImapHostSessionListCID, &rv);
  nsIMAPNamespace *ns = nullptr;
  rv = hostSessionList->GetDefaultNamespaceOfTypeForHost(serverKey.get(), (EIMAPNamespaceType)namespaceType, ns);
  if (ns)
  {
    nsCAutoString namespacePrefix(ns->GetPrefix());
    if (!namespacePrefix.IsEmpty())
    {
      // check if namespacePrefix is the same as the online directory; if so, ignore it.
      nsCAutoString onlineDir;
      rv = GetServerDirectory(onlineDir);
      NS_ENSURE_SUCCESS(rv, rv);
      if (!onlineDir.IsEmpty())
      {
        char delimiter = ns->GetDelimiter();
          if ( onlineDir.Last() != delimiter )
            onlineDir += delimiter;
          if (onlineDir.Equals(namespacePrefix))
            return NS_OK;
      }

      MsgReplaceChar(namespacePrefix, ns->GetDelimiter(), '/'); // use canonical format
      nsCString uri(originalUri);
      int32_t index = uri.Find("//");           // find scheme
      index = uri.FindChar('/', index + 2);       // find '/' after scheme
      // it may be the case that this is the INBOX uri, in which case
      // we don't want to prepend the namespace. In that case, the uri ends with "INBOX",
      // but the namespace is "INBOX/", so they don't match.
      if (MsgFind(uri, namespacePrefix, false, index + 1) != index + 1 &&
          !MsgLowerCaseEqualsLiteral(Substring(uri, index + 1), "inbox"))
        uri.Insert(namespacePrefix, index + 1);   // insert namespace prefix
      convertedUri = uri;
    }
  }
  return rv;
}

NS_IMETHODIMP nsImapIncomingServer::GetTrashFolderName(nsAString& retval)
{
  nsresult rv = GetUnicharValue(PREF_TRASH_FOLDER_NAME, retval);
  if (NS_FAILED(rv))
    return rv;
  if (retval.IsEmpty())
    retval = NS_LITERAL_STRING(DEFAULT_TRASH_FOLDER_NAME);
  return NS_OK;
}

NS_IMETHODIMP nsImapIncomingServer::SetTrashFolderName(const nsAString& chvalue)
{
  // clear trash flag from the old pref
  nsAutoString oldTrashName;
  nsresult rv = GetTrashFolderName(oldTrashName);
  if (NS_SUCCEEDED(rv))
  {
    nsCAutoString oldTrashNameUtf7;
    rv = CopyUTF16toMUTF7(oldTrashName, oldTrashNameUtf7);
    if (NS_SUCCEEDED(rv))
    {
      nsCOMPtr<nsIMsgFolder> oldFolder;
      rv = GetFolder(oldTrashNameUtf7, getter_AddRefs(oldFolder));
      if (NS_SUCCEEDED(rv) && oldFolder)
        oldFolder->ClearFlag(nsMsgFolderFlags::Trash);
    }
  }
  return SetUnicharValue(PREF_TRASH_FOLDER_NAME, chvalue);
}

NS_IMETHODIMP
nsImapIncomingServer::GetMsgFolderFromURI(nsIMsgFolder *aFolderResource,
                                          const nsACString& aURI,
                                          nsIMsgFolder **aFolder)
{
  nsCOMPtr<nsIMsgFolder> msgFolder;
  bool namespacePrefixAdded = false;
  nsCString folderUriWithNamespace;

  // Check if the folder exists as is...
  nsresult rv = GetExistingMsgFolder(aURI, folderUriWithNamespace,
                                     namespacePrefixAdded, false,
                                     getter_AddRefs(msgFolder));

  // Or try again with a case-insensitive lookup
  if (NS_FAILED(rv) || !msgFolder)
    rv = GetExistingMsgFolder(aURI, folderUriWithNamespace,
                              namespacePrefixAdded, true,
                              getter_AddRefs(msgFolder));

  if (NS_FAILED(rv) || !msgFolder) {
    // we didn't find the folder so we will have to create a new one.
    if (namespacePrefixAdded)
    {
      nsCOMPtr <nsIRDFService> rdf = do_GetService("@mozilla.org/rdf/rdf-service;1", &rv);
      NS_ENSURE_SUCCESS(rv,rv);

      nsCOMPtr<nsIRDFResource> resource;
      rv = rdf->GetResource(folderUriWithNamespace, getter_AddRefs(resource));
      NS_ENSURE_SUCCESS(rv,rv);

      nsCOMPtr <nsIMsgFolder> folderResource;
      folderResource = do_QueryInterface(resource, &rv);
      NS_ENSURE_SUCCESS(rv,rv);
      msgFolder = folderResource;
    }
    else
      msgFolder = aFolderResource;
  }

  msgFolder.swap(*aFolder);
  return NS_OK;
}

nsresult
nsImapIncomingServer::GetExistingMsgFolder(const nsACString& aURI,
                                           nsACString& aFolderUriWithNamespace,
                                           bool& aNamespacePrefixAdded,
                                           bool aCaseInsensitive,
                                           nsIMsgFolder **aFolder)
{
  nsCOMPtr<nsIMsgFolder> rootMsgFolder;
  nsresult rv = GetRootMsgFolder(getter_AddRefs(rootMsgFolder));
  NS_ENSURE_SUCCESS(rv, rv);

  aNamespacePrefixAdded = false;
  // Check if the folder exists as is...Even if we have a personal namespace,
  // it might be in another namespace (e.g., shared) and this will catch that.
  rv = rootMsgFolder->GetChildWithURI(aURI, true, aCaseInsensitive, aFolder);

  // If we couldn't find the folder as is, check if we need to prepend the
  // personal namespace
  if (!*aFolder)
  {
    GetUriWithNamespacePrefixIfNecessary(kPersonalNamespace, aURI,
                                         aFolderUriWithNamespace);
    if (!aFolderUriWithNamespace.IsEmpty())
    {
      aNamespacePrefixAdded = true;
      rv = rootMsgFolder->GetChildWithURI(aFolderUriWithNamespace, true,
                                          aCaseInsensitive, aFolder);
    }
  }
  return rv;
}

NS_IMETHODIMP
nsImapIncomingServer::CramMD5Hash(const char *decodedChallenge, const char *key, char **result)
{
  NS_ENSURE_ARG_POINTER(decodedChallenge);
  NS_ENSURE_ARG_POINTER(key);

  unsigned char resultDigest[DIGEST_LENGTH];
  nsresult rv = MSGCramMD5(decodedChallenge, strlen(decodedChallenge), key, strlen(key), resultDigest);
  NS_ENSURE_SUCCESS(rv, rv);
  *result = (char *) malloc(DIGEST_LENGTH);
  if (*result)
    memcpy(*result, resultDigest, DIGEST_LENGTH);
  return (*result) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsImapIncomingServer::GetLoginUsername(nsACString &aLoginUsername)
{
  return GetRealUsername(aLoginUsername);
}
