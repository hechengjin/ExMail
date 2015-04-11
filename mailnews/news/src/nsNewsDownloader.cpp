/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "msgCore.h"
#include "nntpCore.h"
#include "netCore.h"
#include "nsIMsgNewsFolder.h"
#include "nsIStringBundle.h"
#include "nsNewsDownloader.h"
#include "nsINntpService.h"
#include "nsMsgNewsCID.h"
#include "nsIMsgSearchSession.h"
#include "nsIMsgSearchTerm.h"
#include "nsIMsgSearchValidityManager.h"
#include "nsRDFCID.h"
#include "nsIMsgAccountManager.h"
#include "nsMsgFolderFlags.h"
#include "nsIRequestObserver.h"
#include "nsIMsgMailSession.h"
#include "nsMsgMessageFlags.h"
#include "nsIMsgStatusFeedback.h"
#include "nsServiceManagerUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsMsgUtils.h"
#include "mozilla/Services.h"

// This file contains the news article download state machine.

// if pIds is not null, download the articles whose id's are passed in. Otherwise,
// which articles to download is determined by nsNewsDownloader object,
// or subclasses thereof. News can download marked objects, for example.
nsresult nsNewsDownloader::DownloadArticles(nsIMsgWindow *window, nsIMsgFolder *folder, nsTArray<nsMsgKey> *pIds)
{
  if (pIds != nullptr)
    m_keysToDownload.InsertElementsAt(0, pIds->Elements(), pIds->Length());

  if (!m_keysToDownload.IsEmpty())
    m_downloadFromKeys = true;

  m_folder = folder;
  m_window = window;
  m_numwrote = 0;

  bool headersToDownload = GetNextHdrToRetrieve();
  // should we have a special error code for failure here?
  return (headersToDownload) ? DownloadNext(true) : NS_ERROR_FAILURE;
}

/* Saving news messages
 */

NS_IMPL_ISUPPORTS2(nsNewsDownloader, nsIUrlListener, nsIMsgSearchNotify)

nsNewsDownloader::nsNewsDownloader(nsIMsgWindow *window, nsIMsgDatabase *msgDB, nsIUrlListener *listener)
{
  m_numwrote = 0;
  m_downloadFromKeys = false;
  m_newsDB = msgDB;
  m_abort = false;
  m_listener = listener;
  m_window = window;
  m_lastPercent = -1;
  m_lastProgressTime = 0;
  // not the perfect place for this, but I think it will work.
  if (m_window)
    m_window->SetStopped(false);
}

nsNewsDownloader::~nsNewsDownloader()
{
  if (m_listener)
    m_listener->OnStopRunningUrl(/* don't have a url */nullptr, m_status);
  if (m_newsDB)
  {
    m_newsDB->Commit(nsMsgDBCommitType::kLargeCommit);
    m_newsDB = nullptr;
  }
}

NS_IMETHODIMP nsNewsDownloader::OnStartRunningUrl(nsIURI* url)
{
  return NS_OK;
}

NS_IMETHODIMP nsNewsDownloader::OnStopRunningUrl(nsIURI* url, nsresult exitCode)
{
  bool stopped = false;
  if (m_window)
    m_window->GetStopped(&stopped);
  if (stopped)
    exitCode = NS_BINDING_ABORTED;

 nsresult rv = exitCode;
  if (NS_SUCCEEDED(exitCode) || exitCode == NS_MSG_NEWS_ARTICLE_NOT_FOUND)
    rv = DownloadNext(false);

  return rv;
}

nsresult nsNewsDownloader::DownloadNext(bool firstTimeP)
{
  nsresult rv;
  if (!firstTimeP)
  {
    bool moreHeaders = GetNextHdrToRetrieve();
    if (!moreHeaders)
    {
      if (m_listener)
        m_listener->OnStopRunningUrl(nullptr, NS_OK);
      return NS_OK;
    }
  }
  StartDownload();
  m_wroteAnyP = false;
  nsCOMPtr <nsINntpService> nntpService = do_GetService(NS_NNTPSERVICE_CONTRACTID,&rv);
  NS_ENSURE_SUCCESS(rv, rv);

  return nntpService->FetchMessage(m_folder, m_keyToDownload, m_window, nullptr, this, nullptr);
}

bool DownloadNewsArticlesToOfflineStore::GetNextHdrToRetrieve()
{
  nsresult rv;

  if (m_downloadFromKeys)
    return nsNewsDownloader::GetNextHdrToRetrieve();

  if (m_headerEnumerator == nullptr)
    rv = m_newsDB->EnumerateMessages(getter_AddRefs(m_headerEnumerator));

  bool hasMore = false;

  while (NS_SUCCEEDED(rv = m_headerEnumerator->HasMoreElements(&hasMore)) && hasMore)
  {
    nsCOMPtr <nsISupports> supports;
    rv = m_headerEnumerator->GetNext(getter_AddRefs(supports));
    m_newsHeader = do_QueryInterface(supports);
    NS_ENSURE_SUCCESS(rv, false);
    uint32_t hdrFlags;
    m_newsHeader->GetFlags(&hdrFlags);
    if (hdrFlags & nsMsgMessageFlags::Marked)
    {
      m_newsHeader->GetMessageKey(&m_keyToDownload);
      break;
    }
    else
    {
      m_newsHeader = nullptr;
    }
  }
  return hasMore;
}

void nsNewsDownloader::Abort() {}
void nsNewsDownloader::Complete() {}

bool nsNewsDownloader::GetNextHdrToRetrieve()
{
  nsresult rv;
  if (m_downloadFromKeys)
  {
    if (m_numwrote >= (int32_t) m_keysToDownload.Length())
      return false;

    m_keyToDownload = m_keysToDownload[m_numwrote++];
    int32_t percent;
    percent = (100 * m_numwrote) / (int32_t) m_keysToDownload.Length();

    int64_t nowMS = 0;
    if (percent < 100)  // always need to do 100%
    {
      nowMS = PR_IntervalToMilliseconds(PR_IntervalNow());
      if (nowMS - m_lastProgressTime < 750)
        return true;
    }

    m_lastProgressTime = nowMS;
    nsCOMPtr <nsIMsgNewsFolder> newsFolder = do_QueryInterface(m_folder);
    nsCOMPtr<nsIStringBundleService> bundleService =
      mozilla::services::GetStringBundleService();
    NS_ENSURE_TRUE(bundleService, false);
    nsCOMPtr<nsIStringBundle> bundle;
    rv = bundleService->CreateBundle(NEWS_MSGS_URL, getter_AddRefs(bundle));
    NS_ENSURE_SUCCESS(rv, false);

    nsAutoString firstStr;
    firstStr.AppendInt(m_numwrote);
    nsAutoString totalStr;
    totalStr.AppendInt(m_keysToDownload.Length());
    nsString prettiestName;
    nsString statusString;

    m_folder->GetPrettiestName(prettiestName);

    const PRUnichar *formatStrings[3] = { firstStr.get(), totalStr.get(), prettiestName.get() };
    rv = bundle->FormatStringFromName(NS_LITERAL_STRING("downloadingArticlesForOffline").get(),
                                      formatStrings, 3, getter_Copies(statusString));
    NS_ENSURE_SUCCESS(rv, false);
    ShowProgress(statusString.get(), percent);
    return true;
  }
  NS_ASSERTION(false, "shouldn't get here if we're not downloading from keys.");
  return false;  // shouldn't get here if we're not downloading from keys.
}

nsresult nsNewsDownloader::ShowProgress(const PRUnichar *progressString, int32_t percent)
{
  if (!m_statusFeedback)
  {
    if (m_window)
      m_window->GetStatusFeedback(getter_AddRefs(m_statusFeedback));
  }
  if (m_statusFeedback)
  {
    m_statusFeedback->ShowStatusString(nsDependentString(progressString));
    if (percent != m_lastPercent)
    {
      m_statusFeedback->ShowProgress(percent);
      m_lastPercent = percent;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP DownloadNewsArticlesToOfflineStore::OnStartRunningUrl(nsIURI* url)
{
  return NS_OK;
}


NS_IMETHODIMP DownloadNewsArticlesToOfflineStore::OnStopRunningUrl(nsIURI* url, nsresult exitCode)
{
  m_status = exitCode;
  if (m_newsHeader != nullptr)
  {
#ifdef DEBUG_bienvenu
    //    XP_Trace("finished retrieving %ld\n", m_newsHeader->GetMessageKey());
#endif
    if (m_newsDB)
    {
      nsMsgKey msgKey;
      m_newsHeader->GetMessageKey(&msgKey);
      m_newsDB->MarkMarked(msgKey, false, nullptr);
    }
  }
  m_newsHeader = nullptr;
  return nsNewsDownloader::OnStopRunningUrl(url, exitCode);
}

int DownloadNewsArticlesToOfflineStore::FinishDownload()
{
  return 0;
}


NS_IMETHODIMP nsNewsDownloader::OnSearchHit(nsIMsgDBHdr *header, nsIMsgFolder *folder)
{
  NS_ENSURE_ARG(header);


  uint32_t msgFlags;
  header->GetFlags(&msgFlags);
  // only need to download articles we don't already have...
  if (! (msgFlags & nsMsgMessageFlags::Offline))
  {
    nsMsgKey key;
    header->GetMessageKey(&key);
    m_keysToDownload.AppendElement(key);
  }
  return NS_OK;
}

NS_IMETHODIMP nsNewsDownloader::OnSearchDone(nsresult status)
{
  if (m_keysToDownload.IsEmpty())
  {
    if (m_listener)
      return m_listener->OnStopRunningUrl(nullptr, NS_OK);
  }
  nsresult rv = DownloadArticles(m_window, m_folder,
                  /* we've already set m_keysToDownload, so don't pass it in */ nullptr);
  if (NS_FAILED(rv))
    if (m_listener)
      m_listener->OnStopRunningUrl(nullptr, rv);

  return rv;
}
NS_IMETHODIMP nsNewsDownloader::OnNewSearch()
{
  return NS_OK;
}

int DownloadNewsArticlesToOfflineStore::StartDownload()
{
  m_newsDB->GetMsgHdrForKey(m_keyToDownload, getter_AddRefs(m_newsHeader));
  return 0;
}

DownloadNewsArticlesToOfflineStore::DownloadNewsArticlesToOfflineStore(nsIMsgWindow *window, nsIMsgDatabase *db, nsIUrlListener *listener)
  : nsNewsDownloader(window, db, listener)
{
  m_newsDB = db;
}

DownloadNewsArticlesToOfflineStore::~DownloadNewsArticlesToOfflineStore()
{
}

DownloadMatchingNewsArticlesToNewsDB::DownloadMatchingNewsArticlesToNewsDB
  (nsIMsgWindow *window, nsIMsgFolder *folder, nsIMsgDatabase *newsDB,
   nsIUrlListener *listener) :
   DownloadNewsArticlesToOfflineStore(window, newsDB, listener)
{
  m_window = window;
  m_folder = folder;
  m_newsDB = newsDB;
  m_downloadFromKeys = true;  // search term matching means downloadFromKeys.
}

DownloadMatchingNewsArticlesToNewsDB::~DownloadMatchingNewsArticlesToNewsDB()
{
}


NS_IMPL_ISUPPORTS1(nsMsgDownloadAllNewsgroups, nsIUrlListener)


nsMsgDownloadAllNewsgroups::nsMsgDownloadAllNewsgroups(nsIMsgWindow *window, nsIUrlListener *listener)
{
  m_window = window;
  m_listener = listener;
  m_downloaderForGroup = new DownloadMatchingNewsArticlesToNewsDB(window, nullptr, nullptr, this);
  NS_IF_ADDREF(m_downloaderForGroup);
  m_downloadedHdrsForCurGroup = false;
}

nsMsgDownloadAllNewsgroups::~nsMsgDownloadAllNewsgroups()
{
  NS_IF_RELEASE(m_downloaderForGroup);
}

NS_IMETHODIMP nsMsgDownloadAllNewsgroups::OnStartRunningUrl(nsIURI* url)
{
    return NS_OK;
}

NS_IMETHODIMP
nsMsgDownloadAllNewsgroups::OnStopRunningUrl(nsIURI* url, nsresult exitCode)
{
  nsresult rv = exitCode;
  if (NS_SUCCEEDED(exitCode) || exitCode == NS_MSG_NEWS_ARTICLE_NOT_FOUND)
  {
    if (m_downloadedHdrsForCurGroup)
    {
      bool savingArticlesOffline = false;
      nsCOMPtr <nsIMsgNewsFolder> newsFolder = do_QueryInterface(m_currentFolder);
      if (newsFolder)
        newsFolder->GetSaveArticleOffline(&savingArticlesOffline);

      m_downloadedHdrsForCurGroup = false;
      if (savingArticlesOffline) // skip this group - we're saving to it already
        rv = ProcessNextGroup();
      else
        rv = DownloadMsgsForCurrentGroup();
    }
    else
    {
      rv = ProcessNextGroup();
    }
  }
  else if (m_listener)  // notify main observer.
    m_listener->OnStopRunningUrl(url, exitCode);

  return rv;
}

// leaves m_currentServer at the next nntp "server" that
// might have folders to download for offline use. If no more servers,
// m_currentServer will be left at nullptr.
// Also, sets up m_serverEnumerator to enumerate over the server
// If no servers found, m_serverEnumerator will be left at null,
nsresult nsMsgDownloadAllNewsgroups::AdvanceToNextServer(bool *done)
{
  nsresult rv;

  NS_ENSURE_ARG(done);

  *done = true;
  if (!m_allServers)
  {
    nsCOMPtr<nsIMsgAccountManager> accountManager =
             do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
    NS_ASSERTION(accountManager && NS_SUCCEEDED(rv), "couldn't get account mgr");
    if (!accountManager || NS_FAILED(rv)) return rv;

    rv = accountManager->GetAllServers(getter_AddRefs(m_allServers));
    NS_ENSURE_SUCCESS(rv, rv);
  }
  uint32_t serverIndex = (m_currentServer) ? m_allServers->IndexOf(m_currentServer) + 1 : 0;
  m_currentServer = nullptr;
  uint32_t numServers;
  m_allServers->Count(&numServers);
  nsCOMPtr <nsIMsgFolder> rootFolder;

  while (serverIndex < numServers)
  {
    nsCOMPtr<nsIMsgIncomingServer> server = do_QueryElementAt(m_allServers, serverIndex);
    serverIndex++;

    nsCOMPtr <nsINntpIncomingServer> newsServer = do_QueryInterface(server);
    if (!newsServer) // we're only looking for news servers
      continue;
    if (server)
    {
      m_currentServer = server;
      server->GetRootFolder(getter_AddRefs(rootFolder));
      if (rootFolder)
      {
        NS_NewISupportsArray(getter_AddRefs(m_allFolders));
        rv = rootFolder->ListDescendents(m_allFolders);
        if (NS_SUCCEEDED(rv))
          m_allFolders->Enumerate(getter_AddRefs(m_serverEnumerator));
        if (NS_SUCCEEDED(rv) && m_serverEnumerator)
        {
          rv = m_serverEnumerator->First();
          if (NS_SUCCEEDED(rv))
          {
            *done = false;
            break;
          }
        }
      }
    }
  }
  return rv;
}

nsresult nsMsgDownloadAllNewsgroups::AdvanceToNextGroup(bool *done)
{
  nsresult rv;
  NS_ENSURE_ARG(done);
  *done = true;

  if (m_currentFolder)
  {
    nsCOMPtr <nsIMsgNewsFolder> newsFolder = do_QueryInterface(m_currentFolder);
    if (newsFolder)
      newsFolder->SetSaveArticleOffline(false);

    nsCOMPtr<nsIMsgMailSession> session =
             do_GetService(NS_MSGMAILSESSION_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv) && session)
    {
      bool folderOpen;
      uint32_t folderFlags;
      m_currentFolder->GetFlags(&folderFlags);
      session->IsFolderOpenInWindow(m_currentFolder, &folderOpen);
      if (!folderOpen && ! (folderFlags & (nsMsgFolderFlags::Trash | nsMsgFolderFlags::Inbox)))
        m_currentFolder->SetMsgDatabase(nullptr);
    }
    m_currentFolder = nullptr;
  }

  *done = false;

  if (!m_currentServer)
     rv = AdvanceToNextServer(done);
  else
     rv = m_serverEnumerator->Next();
  if (NS_FAILED(rv))
    rv = AdvanceToNextServer(done);

  if (NS_SUCCEEDED(rv) && !*done && m_serverEnumerator)
  {
    nsCOMPtr <nsISupports> supports;
    rv = m_serverEnumerator->CurrentItem(getter_AddRefs(supports));
    m_currentFolder = do_QueryInterface(supports);
    *done = false;
  }
  return rv;
}

nsresult DownloadMatchingNewsArticlesToNewsDB::RunSearch(nsIMsgFolder *folder, nsIMsgDatabase *newsDB, nsIMsgSearchSession *searchSession)
{
  m_folder = folder;
  m_newsDB = newsDB;
  m_searchSession = searchSession;

  m_keysToDownload.Clear();
  nsresult rv;
  NS_ENSURE_ARG(searchSession);
  NS_ENSURE_ARG(folder);

  searchSession->RegisterListener(this,
                                  nsIMsgSearchSession::allNotifications);
  rv = searchSession->AddScopeTerm(nsMsgSearchScope::localNews, folder);
  return searchSession->Search(m_window);
}

nsresult nsMsgDownloadAllNewsgroups::ProcessNextGroup()
{
  nsresult rv = NS_OK;
  bool done = false;

  while (NS_SUCCEEDED(rv) && !done)
  {
    rv = AdvanceToNextGroup(&done);
    if (m_currentFolder)
    {
      uint32_t folderFlags;
      m_currentFolder->GetFlags(&folderFlags);
      if (folderFlags & nsMsgFolderFlags::Offline)
        break;
    }
  }
  if (NS_FAILED(rv) || done)
  {
    if (m_listener)
      return m_listener->OnStopRunningUrl(nullptr, NS_OK);
  }
  m_downloadedHdrsForCurGroup = true;
  return m_currentFolder ? m_currentFolder->GetNewMessages(m_window, this) : NS_ERROR_NOT_INITIALIZED;
}

nsresult nsMsgDownloadAllNewsgroups::DownloadMsgsForCurrentGroup()
{
  NS_ENSURE_TRUE(m_downloaderForGroup, NS_ERROR_OUT_OF_MEMORY);
  nsCOMPtr <nsIMsgDatabase> db;
  nsCOMPtr <nsIMsgDownloadSettings> downloadSettings;
  m_currentFolder->GetMsgDatabase(getter_AddRefs(db));
  nsresult rv = m_currentFolder->GetDownloadSettings(getter_AddRefs(downloadSettings));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr <nsIMsgNewsFolder> newsFolder = do_QueryInterface(m_currentFolder);
  if (newsFolder)
    newsFolder->SetSaveArticleOffline(true);

  nsCOMPtr <nsIMsgSearchSession> searchSession = do_CreateInstance(NS_MSGSEARCHSESSION_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  bool downloadByDate, downloadUnreadOnly;
  uint32_t ageLimitOfMsgsToDownload;

  downloadSettings->GetDownloadByDate(&downloadByDate);
  downloadSettings->GetDownloadUnreadOnly(&downloadUnreadOnly);
  downloadSettings->GetAgeLimitOfMsgsToDownload(&ageLimitOfMsgsToDownload);

  nsCOMPtr <nsIMsgSearchTerm> term;
  nsCOMPtr <nsIMsgSearchValue>    value;

  rv = searchSession->CreateTerm(getter_AddRefs(term));
  NS_ENSURE_SUCCESS(rv, rv);
  term->GetValue(getter_AddRefs(value));

  if (downloadUnreadOnly)
  {
    value->SetAttrib(nsMsgSearchAttrib::MsgStatus);
    value->SetStatus(nsMsgMessageFlags::Read);
    searchSession->AddSearchTerm(nsMsgSearchAttrib::MsgStatus, nsMsgSearchOp::Isnt, value, true, nullptr);
  }
  if (downloadByDate)
  {
    value->SetAttrib(nsMsgSearchAttrib::AgeInDays);
    value->SetAge(ageLimitOfMsgsToDownload);
    searchSession->AddSearchTerm(nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::IsLessThan, value, nsMsgSearchBooleanOp::BooleanAND, nullptr);
  }
  value->SetAttrib(nsMsgSearchAttrib::MsgStatus);
  value->SetStatus(nsMsgMessageFlags::Offline);
  searchSession->AddSearchTerm(nsMsgSearchAttrib::MsgStatus, nsMsgSearchOp::Isnt, value, nsMsgSearchBooleanOp::BooleanAND, nullptr);

  m_downloaderForGroup->RunSearch(m_currentFolder, db, searchSession);
  return rv;
}
