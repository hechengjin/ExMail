/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "msgCore.h"
#include "nsMsgXFVirtualFolderDBView.h"
#include "nsIMsgHdr.h"
#include "nsIMsgThread.h"
#include "nsQuickSort.h"
#include "nsIDBFolderInfo.h"
#include "nsMsgBaseCID.h"
#include "nsIMsgCopyService.h"
#include "nsICopyMsgStreamListener.h"
#include "nsMsgUtils.h"
#include "nsITreeColumns.h"
#include "nsIMsgSearchSession.h"
#include "nsMsgDBCID.h"
#include "nsMsgMessageFlags.h"
#include "nsServiceManagerUtils.h"

nsMsgXFVirtualFolderDBView::nsMsgXFVirtualFolderDBView()
{
  mSuppressMsgDisplay = false;
  m_doingSearch = false;
  m_doingQuickSearch = false;
  m_totalMessagesInView = 0;
}

nsMsgXFVirtualFolderDBView::~nsMsgXFVirtualFolderDBView()
{
}

NS_IMETHODIMP nsMsgXFVirtualFolderDBView::Open(nsIMsgFolder *folder, 
                                               nsMsgViewSortTypeValue sortType, 
                                               nsMsgViewSortOrderValue sortOrder, 
                                               nsMsgViewFlagsTypeValue viewFlags, 
                                               int32_t *pCount)
{
  m_viewFolder = folder;
  return nsMsgSearchDBView::Open(folder, sortType, sortOrder, viewFlags, pCount);
}

void nsMsgXFVirtualFolderDBView::RemovePendingDBListeners()
{
  nsresult rv;
  nsCOMPtr<nsIMsgDBService> msgDBService = do_GetService(NS_MSGDB_SERVICE_CONTRACTID, &rv);
  // UnregisterPendingListener will return an error when there are no more instances
  // of this object registered as pending listeners.
  while (NS_SUCCEEDED(rv))
    rv = msgDBService->UnregisterPendingListener(this);
}

NS_IMETHODIMP nsMsgXFVirtualFolderDBView::Close()
{
  RemovePendingDBListeners();
  return nsMsgSearchDBView::Close();
}

NS_IMETHODIMP
nsMsgXFVirtualFolderDBView::CloneDBView(nsIMessenger *aMessengerInstance, nsIMsgWindow *aMsgWindow,
                                        nsIMsgDBViewCommandUpdater *aCmdUpdater, nsIMsgDBView **_retval)
{
  nsMsgXFVirtualFolderDBView* newMsgDBView = new nsMsgXFVirtualFolderDBView();

  if (!newMsgDBView)
    return NS_ERROR_OUT_OF_MEMORY;

  nsresult rv = CopyDBView(newMsgDBView, aMessengerInstance, aMsgWindow, aCmdUpdater);
  NS_ENSURE_SUCCESS(rv,rv);

  NS_IF_ADDREF(*_retval = newMsgDBView);
  return NS_OK;
}

NS_IMETHODIMP
nsMsgXFVirtualFolderDBView::CopyDBView(nsMsgDBView *aNewMsgDBView, nsIMessenger *aMessengerInstance,
                                       nsIMsgWindow *aMsgWindow, nsIMsgDBViewCommandUpdater *aCmdUpdater)
{
  nsMsgSearchDBView::CopyDBView(aNewMsgDBView, aMessengerInstance, aMsgWindow, aCmdUpdater);

  nsMsgXFVirtualFolderDBView* newMsgDBView = (nsMsgXFVirtualFolderDBView *) aNewMsgDBView;

  newMsgDBView->m_viewFolder = m_viewFolder;
  newMsgDBView->m_searchSession = m_searchSession;

  int32_t scopeCount;
  nsresult rv;
  nsCOMPtr <nsIMsgSearchSession> searchSession =
    do_QueryReferent(m_searchSession, &rv);
  // It's OK not to have a search session.
  NS_ENSURE_SUCCESS(rv, NS_OK);
  nsCOMPtr<nsIMsgDBService> msgDBService =
    do_GetService(NS_MSGDB_SERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  searchSession->CountSearchScopes(&scopeCount);
  for (int32_t i = 0; i < scopeCount; i++)
  {
    nsMsgSearchScopeValue scopeId;
    nsCOMPtr<nsIMsgFolder> searchFolder;
    searchSession->GetNthSearchScope(i, &scopeId, getter_AddRefs(searchFolder));
    if (searchFolder)
      msgDBService->RegisterPendingListener(searchFolder, newMsgDBView);
  }

  return NS_OK;
}

NS_IMETHODIMP nsMsgXFVirtualFolderDBView::GetViewType(nsMsgViewTypeValue *aViewType)
{
    NS_ENSURE_ARG_POINTER(aViewType);
    *aViewType = nsMsgViewType::eShowVirtualFolderResults;
    return NS_OK;
}

nsresult nsMsgXFVirtualFolderDBView::OnNewHeader(nsIMsgDBHdr *newHdr, nsMsgKey aParentKey, bool /*ensureListed*/)
{
  if (newHdr)
  {
    bool match = false;
    nsCOMPtr <nsIMsgSearchSession> searchSession = do_QueryReferent(m_searchSession);
    if (searchSession)
      searchSession->MatchHdr(newHdr, m_db, &match);
    if (!match)
      match = WasHdrRecentlyDeleted(newHdr);
    if (match)
    {
      nsCOMPtr <nsIMsgFolder> folder;
      newHdr->GetFolder(getter_AddRefs(folder));
      bool saveDoingSearch = m_doingSearch;
      m_doingSearch = false;
      OnSearchHit(newHdr, folder);
      m_doingSearch = saveDoingSearch;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP nsMsgXFVirtualFolderDBView::OnHdrPropertyChanged(nsIMsgDBHdr *aHdrChanged,
                bool aPreChange, uint32_t *aStatus, nsIDBChangeListener *aInstigator)
{
  // If the junk mail plugin just activated on a message, then
  // we'll allow filters to remove from view.
  // Otherwise, just update the view line.
  //
  // Note this will not add newly matched headers to the view. This is
  // probably a bug that needs fixing.

  NS_ENSURE_ARG_POINTER(aStatus);
  NS_ENSURE_ARG_POINTER(aHdrChanged);

  nsMsgViewIndex index = FindHdr(aHdrChanged);
  if (index == nsMsgViewIndex_None) // message does not appear in view
    return NS_OK;

  nsCString originStr;
  (void) aHdrChanged->GetStringProperty("junkscoreorigin", getter_Copies(originStr));
  // check for "plugin" with only first character for performance
  bool plugin = (originStr.get()[0] == 'p');

  if (aPreChange)
  {
    // first call, done prior to the change
    *aStatus = plugin;
    return NS_OK;
  }

  // second call, done after the change
  bool wasPlugin = *aStatus;

  bool match = true;
  nsCOMPtr<nsIMsgSearchSession> searchSession(do_QueryReferent(m_searchSession));
  if (searchSession)
    searchSession->MatchHdr(aHdrChanged, m_db, &match);

  if (!match && plugin && !wasPlugin)
    RemoveByIndex(index); // remove hdr from view
  else
    NoteChange(index, 1, nsMsgViewNotificationCode::changed);

  return NS_OK;
}

void nsMsgXFVirtualFolderDBView::UpdateCacheAndViewForFolder(nsIMsgFolder *folder, nsMsgKey *newHits, uint32_t numNewHits)
{
  nsCOMPtr <nsIMsgDatabase> db;
  nsresult rv = folder->GetMsgDatabase(getter_AddRefs(db));
  if (NS_SUCCEEDED(rv) && db)
  {
    nsCString searchUri;
    m_viewFolder->GetURI(searchUri);
    uint32_t numBadHits;
    nsMsgKey *badHits;
    rv = db->RefreshCache(searchUri.get(), numNewHits, newHits,
                     &numBadHits, &badHits);
    if (NS_SUCCEEDED(rv))
    {
      nsCOMPtr<nsIMsgDBHdr> badHdr;
      for (uint32_t badHitIndex = 0; badHitIndex < numBadHits; badHitIndex++)
      {
        // ### of course, this isn't quite right, since we should be 
        // using FindHdr, and we shouldn't be expanding the threads.
        db->GetMsgHdrForKey(badHits[badHitIndex], getter_AddRefs(badHdr));
        // let nsMsgSearchDBView decide what to do about this header
        // getting removed.
        if (badHdr)
          OnHdrDeleted(badHdr, nsMsgKey_None, 0, this);
      }
      delete [] badHits;
    }
  }
}

void nsMsgXFVirtualFolderDBView::UpdateCacheAndViewForPrevSearchedFolders(nsIMsgFolder *curSearchFolder)
{
  // Handle the most recent folder with hits, if any.
  if (m_curFolderGettingHits)
  {
    uint32_t count = m_hdrHits.Count();
    nsTArray<nsMsgKey> newHits;
    newHits.SetLength(count);
    for (uint32_t i = 0; i < count; i++)
      m_hdrHits[i]->GetMessageKey(&newHits[i]);

    newHits.Sort();
    UpdateCacheAndViewForFolder(m_curFolderGettingHits, newHits.Elements(), newHits.Length());
    m_foldersSearchingOver.RemoveObject(m_curFolderGettingHits);
  }

  while (m_foldersSearchingOver.Count() > 0)
  {
    // this new folder has cached hits.
    if (m_foldersSearchingOver[0] == curSearchFolder)
    {
      m_curFolderHasCachedHits = true;
      m_foldersSearchingOver.RemoveObjectAt(0);
      break;
    }
    else
    {
      // this must be a folder that had no hits with the current search.
      // So all cached hits, if any, need to be removed.
      UpdateCacheAndViewForFolder(m_foldersSearchingOver[0], 0, nullptr);
      m_foldersSearchingOver.RemoveObjectAt(0);
    }
  }
}
NS_IMETHODIMP
nsMsgXFVirtualFolderDBView::OnSearchHit(nsIMsgDBHdr* aMsgHdr, nsIMsgFolder *aFolder)
{
  NS_ENSURE_ARG(aMsgHdr);
  NS_ENSURE_ARG(aFolder);

  nsCOMPtr<nsIMsgDatabase> dbToUse;
  nsCOMPtr<nsIDBFolderInfo> folderInfo;
  aFolder->GetDBFolderInfoAndDB(getter_AddRefs(folderInfo), getter_AddRefs(dbToUse));

  if (m_curFolderGettingHits != aFolder && m_doingSearch && !m_doingQuickSearch)
  {
    m_curFolderHasCachedHits = false;
    // since we've gotten a hit for a new folder, the searches for
    // any previous folders are done, so deal with stale cached hits
    // for those folders now.
    UpdateCacheAndViewForPrevSearchedFolders(aFolder);
    m_curFolderGettingHits = aFolder;
    m_hdrHits.Clear();
    m_curFolderStartKeyIndex = m_keys.Length();
  }
  bool hdrInCache = false;
  nsCString searchUri;
  if (!m_doingQuickSearch)
  {
    m_viewFolder->GetURI(searchUri);
    dbToUse->HdrIsInCache(searchUri.get(), aMsgHdr, &hdrInCache);    
  }
  if (!m_doingSearch || !m_curFolderHasCachedHits || !hdrInCache)
  {
    if (m_viewFlags & nsMsgViewFlagsType::kGroupBySort)
      nsMsgGroupView::OnNewHeader(aMsgHdr, nsMsgKey_None, true);
    else if (m_sortValid)
      InsertHdrFromFolder(aMsgHdr, aFolder);
    else
      AddHdrFromFolder(aMsgHdr, aFolder);
  }
  m_hdrHits.AppendObject(aMsgHdr);
  m_totalMessagesInView++;

  return NS_OK;
}

NS_IMETHODIMP
nsMsgXFVirtualFolderDBView::OnSearchDone(nsresult status)
{
  NS_ENSURE_TRUE(m_viewFolder, NS_ERROR_NOT_INITIALIZED);

  // handle any non verified hits we haven't handled yet.
  if (NS_SUCCEEDED(status) && !m_doingQuickSearch && status != NS_MSG_SEARCH_INTERRUPTED)
    UpdateCacheAndViewForPrevSearchedFolders(nullptr);

  m_doingSearch = false;
  //we want to set imap delete model once the search is over because setting next
  //message after deletion will happen before deleting the message and search scope
  //can change with every search.
  mDeleteModel = nsMsgImapDeleteModels::MoveToTrash;  //set to default in case it is non-imap folder
  nsIMsgFolder *curFolder = m_folders.SafeObjectAt(0);
  if (curFolder)
    GetImapDeleteModel(curFolder);

  nsCOMPtr<nsIMsgDatabase> virtDatabase;
  nsCOMPtr<nsIDBFolderInfo> dbFolderInfo;
  nsresult rv = m_viewFolder->GetDBFolderInfoAndDB(getter_AddRefs(dbFolderInfo), getter_AddRefs(virtDatabase));
  NS_ENSURE_SUCCESS(rv, rv);
  // count up the number of unread and total messages from the view, and set those in the
  // folder - easier than trying to keep the count up to date in the face of
  // search hits coming in while the user is reading/deleting messages.
  uint32_t numUnread = 0;
  for (uint32_t i = 0; i < m_flags.Length(); i++)
    if (m_flags[i] & nsMsgMessageFlags::Elided)
    {
      nsCOMPtr<nsIMsgThread> thread;
      GetThreadContainingIndex(i, getter_AddRefs(thread));
      if (thread)
      {
        uint32_t unreadInThread;
        thread->GetNumUnreadChildren(&unreadInThread);
        numUnread += unreadInThread;
      }
    }
    else
    {
      if (!(m_flags[i] & nsMsgMessageFlags::Read))
        numUnread++;
    }
  dbFolderInfo->SetNumUnreadMessages(numUnread);
  dbFolderInfo->SetNumMessages(m_totalMessagesInView);
  m_viewFolder->UpdateSummaryTotals(true); // force update from db.
  virtDatabase->Commit(nsMsgDBCommitType::kLargeCommit);
  if (!m_sortValid && m_sortType != nsMsgViewSortType::byThread && 
      !(m_viewFlags & nsMsgViewFlagsType::kThreadedDisplay))
  {
    m_sortValid = false;       //sort the results
    Sort(m_sortType, m_sortOrder);
  }
  m_foldersSearchingOver.Clear();
  m_curFolderGettingHits = nullptr;
  return rv;
}


NS_IMETHODIMP
nsMsgXFVirtualFolderDBView::OnNewSearch()
{
  int32_t oldSize = GetSize();

  RemovePendingDBListeners();
  m_doingSearch = true;
  m_totalMessagesInView = 0;
  m_folders.Clear();
  m_keys.Clear();
  m_levels.Clear();
  m_flags.Clear();

  // needs to happen after we remove the keys, since RowCountChanged() will call our GetRowCount()
  if (mTree)
    mTree->RowCountChanged(0, -oldSize);

  // to use the search results cache, we'll need to iterate over the scopes in the
  // search session, calling getNthSearchScope for i = 0; i < searchSession.countSearchScopes; i++
  // and for each folder, then open the db and pull out the cached hits, add them to the view.
  // For each hit in a new folder, we'll then clean up the stale hits from the previous folder(s).

  int32_t scopeCount;
  nsCOMPtr<nsIMsgSearchSession> searchSession = do_QueryReferent(m_searchSession);
  NS_ENSURE_TRUE(searchSession, NS_OK); // just ignore
  nsCOMPtr<nsIMsgDBService> msgDBService = do_GetService(NS_MSGDB_SERVICE_CONTRACTID);
  searchSession->CountSearchScopes(&scopeCount);

  // Figure out how many search terms the virtual folder has.
  nsCOMPtr<nsIMsgDatabase> virtDatabase;
  nsCOMPtr<nsIDBFolderInfo> dbFolderInfo;
  nsresult rv = m_viewFolder->GetDBFolderInfoAndDB(getter_AddRefs(dbFolderInfo), getter_AddRefs(virtDatabase));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCString terms;
  dbFolderInfo->GetCharProperty("searchStr", terms);
  nsCOMPtr<nsISupportsArray> searchTerms;
  rv = searchSession->GetSearchTerms(getter_AddRefs(searchTerms));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCString curSearchAsString;

  rv = MsgTermListToString(searchTerms, curSearchAsString);
  // Trim off the initial AND/OR, which is irrelevant and inconsistent between
  // what searchSpec.js generates, and what's in virtualFolders.dat.
  curSearchAsString.Cut(0, StringBeginsWith(curSearchAsString, NS_LITERAL_CSTRING("AND")) ? 3 : 2);
  terms.Cut(0, StringBeginsWith(terms, NS_LITERAL_CSTRING("AND")) ? 3 : 2);

  NS_ENSURE_SUCCESS(rv, rv);
  // If the search session search string doesn't match the vf search str, then we're doing
  // quick search, which means we don't want to invalidate cached results, or
  // used cached results.
  m_doingQuickSearch = !curSearchAsString.Equals(terms);

  if (mTree && !m_doingQuickSearch)
    mTree->BeginUpdateBatch();

  for (int32_t i = 0; i < scopeCount; i++)
  {
    nsMsgSearchScopeValue scopeId;
    nsCOMPtr<nsIMsgFolder> searchFolder;
    searchSession->GetNthSearchScope(i, &scopeId, getter_AddRefs(searchFolder));
    if (searchFolder)
    {
      nsCOMPtr<nsISimpleEnumerator> cachedHits;
      nsCOMPtr<nsIMsgDatabase> searchDB;
      nsCString searchUri;
      m_viewFolder->GetURI(searchUri);
      nsresult rv = searchFolder->GetMsgDatabase(getter_AddRefs(searchDB));
      if (NS_SUCCEEDED(rv) && searchDB)
      {
        if (msgDBService)
          msgDBService->RegisterPendingListener(searchFolder, this);

        m_foldersSearchingOver.AppendObject(searchFolder);
        if (m_doingQuickSearch) // ignore cached hits in quick search case.
          continue;
        searchDB->GetCachedHits(searchUri.get(), getter_AddRefs(cachedHits));
        bool hasMore;
        if (cachedHits)
        {
          cachedHits->HasMoreElements(&hasMore);
          if (hasMore)
          {
            nsMsgKey prevKey = nsMsgKey_None;
            while (hasMore)
            {
              nsCOMPtr <nsIMsgDBHdr> pHeader;
              nsresult rv = cachedHits->GetNext(getter_AddRefs(pHeader));
              NS_ASSERTION(NS_SUCCEEDED(rv), "nsMsgDBEnumerator broken");
              if (pHeader && NS_SUCCEEDED(rv))
              {
                nsMsgKey msgKey;
                pHeader->GetMessageKey(&msgKey);
                NS_ASSERTION(prevKey == nsMsgKey_None || msgKey > prevKey, "cached Hits not sorted");
                prevKey = msgKey;
                AddHdrFromFolder(pHeader, searchFolder);
              }
              else
                break;
              cachedHits->HasMoreElements(&hasMore);
            }
          }
        }
      }
    }
  }
  if (mTree && !m_doingQuickSearch)
    mTree->EndUpdateBatch();

  m_curFolderStartKeyIndex = 0;
  m_curFolderGettingHits = nullptr;
  m_curFolderHasCachedHits = false;

  // if we have cached hits, sort them.
  if (GetSize() > 0)
  {
    // currently, we keep threaded views sorted while we build them.
    if (m_sortType != nsMsgViewSortType::byThread &&
      !(m_viewFlags & nsMsgViewFlagsType::kThreadedDisplay))
    {
      m_sortValid = false;       //sort the results
      Sort(m_sortType, m_sortOrder);
    }
  }
  return NS_OK;
}


NS_IMETHODIMP nsMsgXFVirtualFolderDBView::DoCommand(nsMsgViewCommandTypeValue command)
{
    return nsMsgSearchDBView::DoCommand(command);
}



NS_IMETHODIMP nsMsgXFVirtualFolderDBView::GetMsgFolder(nsIMsgFolder **aMsgFolder)
{
  NS_ENSURE_ARG_POINTER(aMsgFolder);
  NS_IF_ADDREF(*aMsgFolder = m_viewFolder);
  return NS_OK;
}

NS_IMETHODIMP nsMsgXFVirtualFolderDBView::SetViewFlags(nsMsgViewFlagsTypeValue aViewFlags)
{
  nsresult rv = NS_OK;
  // if the grouping/threading has changed, rebuild the view
  if ((m_viewFlags & (nsMsgViewFlagsType::kGroupBySort |
                      nsMsgViewFlagsType::kThreadedDisplay)) !=
      (aViewFlags & (nsMsgViewFlagsType::kGroupBySort |
                     nsMsgViewFlagsType::kThreadedDisplay)))
    rv = RebuildView(aViewFlags);
  nsMsgDBView::SetViewFlags(aViewFlags);
  return rv;
}


nsresult 
nsMsgXFVirtualFolderDBView::GetMessageEnumerator(nsISimpleEnumerator **enumerator)
{
  return GetViewEnumerator(enumerator);
}
