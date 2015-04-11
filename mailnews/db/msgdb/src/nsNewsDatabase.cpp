/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "msgCore.h"
#include "nsNewsDatabase.h"
#include "nsMsgKeySet.h"
#include "nsCOMPtr.h"
#include "prlog.h"

#if defined(DEBUG_sspitzer_) || defined(DEBUG_seth_)
#define DEBUG_NEWS_DATABASE 1
#endif

nsNewsDatabase::nsNewsDatabase()
{
  m_readSet = nullptr;
}

nsNewsDatabase::~nsNewsDatabase()
{
}

NS_IMPL_ADDREF_INHERITED(nsNewsDatabase, nsMsgDatabase)
NS_IMPL_RELEASE_INHERITED(nsNewsDatabase, nsMsgDatabase)

NS_IMETHODIMP nsNewsDatabase::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (!aInstancePtr) return NS_ERROR_NULL_POINTER;
  *aInstancePtr = nullptr;

  if (aIID.Equals(NS_GET_IID(nsINewsDatabase)))
  {
    *aInstancePtr = static_cast<nsINewsDatabase *>(this);
  }

  if(*aInstancePtr)
  {
    AddRef();
    return NS_OK;
  }

  return nsMsgDatabase::QueryInterface(aIID, aInstancePtr);
}

nsresult nsNewsDatabase::Close(bool forceCommit)
{
  return nsMsgDatabase::Close(forceCommit);
}

nsresult nsNewsDatabase::ForceClosed()
{
  return nsMsgDatabase::ForceClosed();
}

nsresult nsNewsDatabase::Commit(nsMsgDBCommit commitType)
{
  if (m_dbFolderInfo && m_readSet)
  {
    // let's write out our idea of the read set so we can compare it with that of
    // the .rc file next time we start up.
    nsCString readSet;
    m_readSet->Output(getter_Copies(readSet));
    m_dbFolderInfo->SetCharProperty("readSet", readSet);
  }
  return nsMsgDatabase::Commit(commitType);
}


uint32_t nsNewsDatabase::GetCurVersion()
{
  return kMsgDBVersion;
}

NS_IMETHODIMP nsNewsDatabase::IsRead(nsMsgKey key, bool *pRead)
{
  NS_ASSERTION(pRead, "null out param in IsRead");
  if (!pRead) return NS_ERROR_NULL_POINTER;

  if (!m_readSet) return NS_ERROR_FAILURE;

  *pRead = m_readSet->IsMember(key);
  return NS_OK;
}

nsresult nsNewsDatabase::IsHeaderRead(nsIMsgDBHdr *msgHdr, bool *pRead)
{
    nsresult rv;
    nsMsgKey messageKey;

    if (!msgHdr || !pRead) return NS_ERROR_NULL_POINTER;

    rv = msgHdr->GetMessageKey(&messageKey);
    if (NS_FAILED(rv)) return rv;

    rv = IsRead(messageKey,pRead);
    return rv;
}

// return highest article number we've seen.
NS_IMETHODIMP nsNewsDatabase::GetHighWaterArticleNum(nsMsgKey *key)
{
  NS_ASSERTION(m_dbFolderInfo, "null db folder info");
  if (!m_dbFolderInfo)
    return NS_ERROR_FAILURE;
  return m_dbFolderInfo->GetHighWater(key);
}

// return the key of the first article number we know about.
// Since the iterator iterates in id order, we can just grab the
// messagekey of the first header it returns.
// ### dmb
// This will not deal with the situation where we get holes in
// the headers we know about. Need to figure out how and when
// to solve that. This could happen if a transfer is interrupted.
// Do we need to keep track of known arts permanently?
NS_IMETHODIMP nsNewsDatabase::GetLowWaterArticleNum(nsMsgKey *key)
{
  nsresult  rv;
  nsMsgHdr  *pHeader;

  nsCOMPtr<nsISimpleEnumerator> hdrs;
  rv = EnumerateMessages(getter_AddRefs(hdrs));
  if (NS_FAILED(rv))
    return rv;

  rv = hdrs->GetNext((nsISupports**)&pHeader);
  NS_ASSERTION(NS_SUCCEEDED(rv), "nsMsgDBEnumerator broken");
  if (NS_FAILED(rv))
    return rv;

  return pHeader->GetMessageKey(key);
}

nsresult  nsNewsDatabase::ExpireUpTo(nsMsgKey expireKey)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
nsresult		nsNewsDatabase::ExpireRange(nsMsgKey startRange, nsMsgKey endRange)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP nsNewsDatabase::GetReadSet(nsMsgKeySet **pSet)
{
    if (!pSet) return NS_ERROR_NULL_POINTER;
    *pSet = m_readSet;
    return NS_OK;
}

NS_IMETHODIMP nsNewsDatabase::SetReadSet(nsMsgKeySet *pSet)
{
  m_readSet = pSet;

  if (m_readSet)
  {
    // compare this read set with the one in the db folder info.
    // If not equivalent, sync with this one.
    nsCString dbReadSet;
    if (m_dbFolderInfo)
      m_dbFolderInfo->GetCharProperty("readSet", dbReadSet);
    nsCString newsrcReadSet;
    m_readSet->Output(getter_Copies(newsrcReadSet));
    if (!dbReadSet.Equals(newsrcReadSet))
      SyncWithReadSet();
  }
  return NS_OK;
}


bool nsNewsDatabase::SetHdrReadFlag(nsIMsgDBHdr *msgHdr, bool bRead)
{
    nsresult rv;
    bool isRead;
    rv = IsHeaderRead(msgHdr, &isRead);

    if (isRead == bRead)
    {
      // give the base class a chance to update m_flags.
      nsMsgDatabase::SetHdrReadFlag(msgHdr, bRead);
      return false;
    }
    else {
      nsMsgKey messageKey;

      // give the base class a chance to update m_flags.
      nsMsgDatabase::SetHdrReadFlag(msgHdr, bRead);
      rv = msgHdr->GetMessageKey(&messageKey);
      if (NS_FAILED(rv)) return false;

      NS_ASSERTION(m_readSet, "m_readSet is null");
      if (!m_readSet) return false;

      if (!bRead) {
#ifdef DEBUG_NEWS_DATABASE
        printf("remove %d from the set\n",messageKey);
#endif

        m_readSet->Remove(messageKey);

        rv = NotifyReadChanged(nullptr);
        if (NS_FAILED(rv)) return false;
      }
      else {
#ifdef DEBUG_NEWS_DATABASE
        printf("add %d to the set\n",messageKey);
#endif

        if (m_readSet->Add(messageKey) < 0) return false;

        rv = NotifyReadChanged(nullptr);
        if (NS_FAILED(rv)) return false;
      }
    }
    return true;
}

NS_IMETHODIMP nsNewsDatabase::MarkAllRead(uint32_t *aNumMarked,
                                          nsMsgKey **aThoseMarked)
{
  nsMsgKey lowWater = nsMsgKey_None, highWater;
  nsCString knownArts;
  if (m_dbFolderInfo)
  {
    m_dbFolderInfo->GetKnownArtsSet(getter_Copies(knownArts));
    nsMsgKeySet *knownKeys = nsMsgKeySet::Create(knownArts.get());
    if (knownKeys)
      lowWater = knownKeys->GetFirstMember();

    delete knownKeys;
  }
  if (lowWater == nsMsgKey_None)
    GetLowWaterArticleNum(&lowWater);
  GetHighWaterArticleNum(&highWater);
  if (lowWater > 2)
    m_readSet->AddRange(1, lowWater - 1);
  nsresult err = nsMsgDatabase::MarkAllRead(aNumMarked, aThoseMarked);
  if (NS_SUCCEEDED(err) && 1 <= highWater)
    m_readSet->AddRange(1, highWater); // mark everything read in newsrc.

  return err;
}

nsresult nsNewsDatabase::SyncWithReadSet()
{

  // The code below attempts to update the underlying nsMsgDatabase's idea
  // of read/unread flags to match the read set in the .newsrc file. It should
  // only be called when they don't match, e.g., we crashed after committing the
  // db but before writing out the .newsrc
  nsCOMPtr <nsISimpleEnumerator> hdrs;
  nsresult rv = EnumerateMessages(getter_AddRefs(hdrs));
  NS_ENSURE_SUCCESS(rv, rv);

  bool hasMore = false, readInNewsrc, isReadInDB, changed = false;
  nsCOMPtr <nsIMsgDBHdr> header;
  int32_t numMessages = 0, numUnreadMessages = 0;
  nsMsgKey messageKey;
  nsCOMPtr <nsIMsgThread> threadHdr;

  // Scan all messages in DB
  while (NS_SUCCEEDED(rv = hdrs->HasMoreElements(&hasMore)) && hasMore)
  {
      rv = hdrs->GetNext(getter_AddRefs(header));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = nsMsgDatabase::IsHeaderRead(header, &isReadInDB);
      NS_ENSURE_SUCCESS(rv, rv);

      header->GetMessageKey(&messageKey);
      IsRead(messageKey,&readInNewsrc);

      numMessages++;
      if (!readInNewsrc)
        numUnreadMessages++;

      // If DB and readSet disagree on Read/Unread, fix DB
      if (readInNewsrc!=isReadInDB)
      {
        MarkHdrRead(header, readInNewsrc, nullptr);
        changed = true;
      }
  }

  // Update FolderInfo Counters
  int32_t oldMessages, oldUnreadMessages;
  rv = m_dbFolderInfo->GetNumMessages(&oldMessages);
  if (NS_SUCCEEDED(rv) && oldMessages!=numMessages)
  {
      changed = true;
      m_dbFolderInfo->ChangeNumMessages(numMessages-oldMessages);
  }
  rv = m_dbFolderInfo->GetNumUnreadMessages(&oldUnreadMessages);
  if (NS_SUCCEEDED(rv) && oldUnreadMessages!=numUnreadMessages)
  {
      changed = true;
      m_dbFolderInfo->ChangeNumUnreadMessages(numUnreadMessages-oldUnreadMessages);
  }

  if (changed)
      Commit(nsMsgDBCommitType::kLargeCommit);

  return rv;
}

nsresult nsNewsDatabase::AdjustExpungedBytesOnDelete(nsIMsgDBHdr *msgHdr)
{
  uint32_t msgFlags;
  msgHdr->GetFlags(&msgFlags);
  if (msgFlags & nsMsgMessageFlags::Offline && m_dbFolderInfo)
  {
    uint32_t size = 0;
    (void)msgHdr->GetOfflineMessageSize(&size);
    return m_dbFolderInfo->ChangeExpungedBytes (size);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNewsDatabase::GetDefaultViewFlags(nsMsgViewFlagsTypeValue *aDefaultViewFlags)
{
  NS_ENSURE_ARG_POINTER(aDefaultViewFlags);
  GetIntPref("mailnews.default_news_view_flags", aDefaultViewFlags);
  if (*aDefaultViewFlags < nsMsgViewFlagsType::kNone ||
      *aDefaultViewFlags > (nsMsgViewFlagsType::kThreadedDisplay |
                            nsMsgViewFlagsType::kShowIgnored |
                            nsMsgViewFlagsType::kUnreadOnly |
                            nsMsgViewFlagsType::kExpandAll |
                            nsMsgViewFlagsType::kGroupBySort))
    *aDefaultViewFlags = nsMsgViewFlagsType::kThreadedDisplay;
  return NS_OK;
}

NS_IMETHODIMP
nsNewsDatabase::GetDefaultSortType(nsMsgViewSortTypeValue *aDefaultSortType)
{
  NS_ENSURE_ARG_POINTER(aDefaultSortType);
  GetIntPref("mailnews.default_news_sort_type", aDefaultSortType);
  if (*aDefaultSortType < nsMsgViewSortType::byDate ||
      *aDefaultSortType > nsMsgViewSortType::byAccount)
    *aDefaultSortType = nsMsgViewSortType::byThread;
  return NS_OK;
}

NS_IMETHODIMP
nsNewsDatabase::GetDefaultSortOrder(nsMsgViewSortOrderValue *aDefaultSortOrder)
{
  NS_ENSURE_ARG_POINTER(aDefaultSortOrder);
  GetIntPref("mailnews.default_news_sort_order", aDefaultSortOrder);
  if (*aDefaultSortOrder != nsMsgViewSortOrder::descending)
    *aDefaultSortOrder = nsMsgViewSortOrder::ascending;
  return NS_OK;
}
