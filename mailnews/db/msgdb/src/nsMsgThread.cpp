/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "msgCore.h"
#include "nsMsgDatabase.h"
#include "nsCOMPtr.h"
#include "nsMsgThread.h"
#include "MailNewsTypes2.h"

NS_IMPL_ISUPPORTS1(nsMsgThread, nsIMsgThread)

nsMsgThread::nsMsgThread()
{
  MOZ_COUNT_CTOR(nsMsgThread);
  Init();
}
nsMsgThread::nsMsgThread(nsMsgDatabase *db, nsIMdbTable *table)
{
  MOZ_COUNT_CTOR(nsMsgThread);
  Init();
  m_mdbTable = table;
  m_mdbDB = db;
  if (db)
    db->m_threads.AppendElement(this);
  else
    NS_ERROR("no db for thread");
#ifdef DEBUG_David_Bienvenu
  if (m_mdbDB->m_threads.Length() > 5)
    printf("more than five outstanding threads\n");
#endif
  if (table && db)
  {
    table->GetMetaRow(db->GetEnv(), nullptr, nullptr, getter_AddRefs(m_metaRow));
    InitCachedValues();
  }
}

void nsMsgThread::Init()
{
  m_threadKey = nsMsgKey_None;
  m_threadRootKey = nsMsgKey_None;
  m_numChildren = 0;
  m_numUnreadChildren = 0;
  m_flags = 0;
  m_newestMsgDate = 0;
  m_cachedValuesInitialized = false;
}

nsMsgThread::~nsMsgThread()
{
  MOZ_COUNT_DTOR(nsMsgThread);
  if (m_mdbDB)
  {
    bool found = m_mdbDB->m_threads.RemoveElement(this);
    NS_ASSERTION(found, "removing thread not in threads array");
  }
  else
    NS_ERROR("null db in thread");
  Clear();
}

void nsMsgThread::Clear()
{
  m_mdbTable = nullptr;
  m_metaRow = nullptr;
  m_mdbDB = nullptr;
}

nsresult nsMsgThread::InitCachedValues()
{
  nsresult err = NS_OK;

  NS_ENSURE_TRUE(m_mdbDB && m_metaRow, NS_ERROR_INVALID_POINTER);

  if (!m_cachedValuesInitialized)
  {
    err = m_mdbDB->RowCellColumnToUInt32(m_metaRow, m_mdbDB->m_threadFlagsColumnToken, &m_flags);
    err = m_mdbDB->RowCellColumnToUInt32(m_metaRow, m_mdbDB->m_threadChildrenColumnToken, &m_numChildren);
    err = m_mdbDB->RowCellColumnToUInt32(m_metaRow, m_mdbDB->m_threadIdColumnToken, &m_threadKey, nsMsgKey_None);
    err = m_mdbDB->RowCellColumnToUInt32(m_metaRow, m_mdbDB->m_threadUnreadChildrenColumnToken, &m_numUnreadChildren);
    err = m_mdbDB->RowCellColumnToUInt32(m_metaRow, m_mdbDB->m_threadRootKeyColumnToken, &m_threadRootKey, nsMsgKey_None);
    err = m_mdbDB->RowCellColumnToUInt32(m_metaRow, m_mdbDB->m_threadNewestMsgDateColumnToken, &m_newestMsgDate, 0);
    // fix num children if it's wrong. this doesn't work - some DB's have a bogus thread table
    // that is full of bogus headers - don't know why.
    uint32_t rowCount = 0;
    m_mdbTable->GetCount(m_mdbDB->GetEnv(), &rowCount);
    //    NS_ASSERTION(m_numChildren <= rowCount, "num children wrong - fixing");
    if (m_numChildren > rowCount)
      ChangeChildCount((int32_t) rowCount - (int32_t) m_numChildren);
    if ((int32_t) m_numUnreadChildren < 0)
      ChangeUnreadChildCount(- (int32_t) m_numUnreadChildren);
    if (NS_SUCCEEDED(err))
      m_cachedValuesInitialized = true;
  }
  return err;
}

NS_IMETHODIMP nsMsgThread::SetThreadKey(nsMsgKey threadKey)
{
  NS_ASSERTION(m_threadKey == nsMsgKey_None || m_threadKey == threadKey,
               "shouldn't be changing thread key");
  m_threadKey = threadKey;
  // by definition, the initial thread key is also the thread root key.
  SetThreadRootKey(threadKey);
  // gotta set column in meta row here.
  return m_mdbDB->UInt32ToRowCellColumn(
                    m_metaRow, m_mdbDB->m_threadIdColumnToken, threadKey);
}

NS_IMETHODIMP nsMsgThread::GetThreadKey(nsMsgKey *result)
{
  NS_ENSURE_ARG_POINTER(result);
  nsresult res = m_mdbDB->RowCellColumnToUInt32(m_metaRow, m_mdbDB->m_threadIdColumnToken, &m_threadKey);
  *result = m_threadKey;
  return res;
}

NS_IMETHODIMP nsMsgThread::GetFlags(uint32_t *result)
{
  NS_ENSURE_ARG_POINTER(result);
  nsresult res = m_mdbDB->RowCellColumnToUInt32(m_metaRow, m_mdbDB->m_threadFlagsColumnToken, &m_flags);
  *result = m_flags;
  return res;
}

NS_IMETHODIMP nsMsgThread::SetFlags(uint32_t flags)
{
  m_flags = flags;
  return m_mdbDB->UInt32ToRowCellColumn(
                    m_metaRow, m_mdbDB->m_threadFlagsColumnToken, m_flags);
}

NS_IMETHODIMP nsMsgThread::SetSubject(const nsACString& aSubject)
{
  return m_mdbDB->CharPtrToRowCellColumn(m_metaRow, m_mdbDB->m_threadSubjectColumnToken, nsCString(aSubject).get());
}

NS_IMETHODIMP nsMsgThread::GetSubject(nsACString& aSubject)
{
  nsCString subjectStr;
  nsresult rv = m_mdbDB->RowCellColumnToCharPtr(m_metaRow, m_mdbDB->m_threadSubjectColumnToken, 
                                                getter_Copies(subjectStr));

  aSubject.Assign(subjectStr);
  return rv;
}

NS_IMETHODIMP nsMsgThread::GetNumChildren(uint32_t *result)
{
  NS_ENSURE_ARG_POINTER(result);
  *result = m_numChildren;
  return NS_OK;
}


NS_IMETHODIMP nsMsgThread::GetNumUnreadChildren (uint32_t *result)
{
  NS_ENSURE_ARG_POINTER(result);
  *result = m_numUnreadChildren;
  return NS_OK;
}

nsresult nsMsgThread::RerootThread(nsIMsgDBHdr *newParentOfOldRoot, nsIMsgDBHdr *oldRoot, nsIDBChangeAnnouncer *announcer)
{
  nsresult rv = NS_OK;
  mdb_pos outPos;
  nsMsgKey newHdrAncestor;
  nsCOMPtr <nsIMsgDBHdr> ancestorHdr = newParentOfOldRoot;
  nsMsgKey newRoot;

  ancestorHdr->GetMessageKey(&newRoot);
  // loop trying to find the oldest ancestor of this msg
  // that is a parent of the root. The oldest ancestor will
  // become the root of the thread.
  do
  {
    ancestorHdr->GetThreadParent(&newHdrAncestor);
    if (newHdrAncestor != nsMsgKey_None && newHdrAncestor != m_threadRootKey && newHdrAncestor != newRoot)
    {
      newRoot = newHdrAncestor;
      rv = m_mdbDB->GetMsgHdrForKey(newRoot, getter_AddRefs(ancestorHdr));
    }
  }
  while (NS_SUCCEEDED(rv) && ancestorHdr && newHdrAncestor != nsMsgKey_None && newHdrAncestor != m_threadRootKey
    && newHdrAncestor != newRoot);
  SetThreadRootKey(newRoot);
  ReparentNonReferenceChildrenOf(oldRoot, newRoot, announcer);
  if (ancestorHdr)
  {
    nsIMsgDBHdr *msgHdr = ancestorHdr;
    nsMsgHdr* rootMsgHdr = static_cast<nsMsgHdr*>(msgHdr);          // closed system, cast ok
    nsIMdbRow *newRootHdrRow = rootMsgHdr->GetMDBRow();
    // move the  root hdr to pos 0.
    m_mdbTable->MoveRow(m_mdbDB->GetEnv(), newRootHdrRow, -1, 0, &outPos);
    ancestorHdr->SetThreadParent(nsMsgKey_None);
  }
  return rv;
}

NS_IMETHODIMP nsMsgThread::AddChild(nsIMsgDBHdr *child, nsIMsgDBHdr *inReplyTo, bool threadInThread,
                                    nsIDBChangeAnnouncer *announcer)
{
  nsresult rv = NS_OK;
  nsMsgHdr* hdr = static_cast<nsMsgHdr*>(child);          // closed system, cast ok
  uint32_t newHdrFlags = 0;
  uint32_t msgDate;
  nsMsgKey newHdrKey = 0;
  bool parentKeyNeedsSetting = true;

  nsIMdbRow *hdrRow = hdr->GetMDBRow();
  hdr->GetRawFlags(&newHdrFlags);
  hdr->GetMessageKey(&newHdrKey);
  hdr->GetDateInSeconds(&msgDate);
  if (msgDate > m_newestMsgDate)
    SetNewestMsgDate(msgDate);

  if (newHdrFlags & nsMsgMessageFlags::Watched)
    SetFlags(m_flags | nsMsgMessageFlags::Watched);

  child->AndFlags(~(nsMsgMessageFlags::Watched), &newHdrFlags);
  
  // These are threading flags that the child may have set before being added
  // to the database.
  uint32_t protoThreadFlags;
  child->GetUint32Property("ProtoThreadFlags", &protoThreadFlags);
  SetFlags(m_flags | protoThreadFlags);
  // Clear the flag so that it doesn't fudge anywhere else
  child->SetUint32Property("ProtoThreadFlags", 0);

  uint32_t numChildren;
  uint32_t childIndex = 0;

  // get the num children before we add the new header.
  GetNumChildren(&numChildren);

  // if this is an empty thread, set the root key to this header's key
  if (numChildren == 0)
    SetThreadRootKey(newHdrKey);

  if (m_mdbTable)
  {
    m_mdbTable->AddRow(m_mdbDB->GetEnv(), hdrRow);
    ChangeChildCount(1);
    if (! (newHdrFlags & nsMsgMessageFlags::Read))
      ChangeUnreadChildCount(1);
  }
  if (inReplyTo)
  {
    nsMsgKey parentKey;
    inReplyTo->GetMessageKey(&parentKey);
    child->SetThreadParent(parentKey);
    parentKeyNeedsSetting = false;
  }
  // check if this header is a parent of one of the messages in this thread

  bool hdrMoved = false;
  nsCOMPtr <nsIMsgDBHdr> curHdr;
  uint32_t moveIndex = 0;

  PRTime newHdrDate;
  child->GetDate(&newHdrDate);

  // This is an ugly but simple fix for a difficult problem. Basically, when we add
  // a message to a thread, we have to run through the thread to see if the new
  // message is a parent of an existing message in the thread, and adjust things
  // accordingly. If you thread by subject, and you have a large folder with
  // messages w/ all the same subject, this code can take a really long time. So the
  // pragmatic thing is to say that for threads with more than 1000 messages, it's
  // simply not worth dealing with the case where the parent comes in after the
  // child. Threads with more than 1000 messages are pretty unwieldy anyway.
  // See Bug 90452

  if (numChildren < 1000)
  {
    for (childIndex = 0; childIndex < numChildren; childIndex++)
    {
      nsMsgKey msgKey;

      rv = GetChildHdrAt(childIndex, getter_AddRefs(curHdr));
      if (NS_SUCCEEDED(rv) && curHdr)
      {
        if (hdr->IsParentOf(curHdr))
        {
          nsMsgKey oldThreadParent;
          mdb_pos outPos;
          // move this hdr before the current header.
          if (!hdrMoved)
          {
            m_mdbTable->MoveRow(m_mdbDB->GetEnv(), hdrRow, -1, childIndex, &outPos);
            hdrMoved = true;
            curHdr->GetThreadParent(&oldThreadParent);
            curHdr->GetMessageKey(&msgKey);
            nsCOMPtr <nsIMsgDBHdr> curParent;
            m_mdbDB->GetMsgHdrForKey(oldThreadParent, getter_AddRefs(curParent));
            if (curParent && hdr->IsAncestorOf(curParent))
            {
              nsMsgKey curParentKey;
              curParent->GetMessageKey(&curParentKey);
              if (curParentKey == m_threadRootKey)
              {
                m_mdbTable->MoveRow(m_mdbDB->GetEnv(), hdrRow, -1, 0, &outPos);
                RerootThread(child, curParent, announcer);
                parentKeyNeedsSetting = false;
              }
            }
            else if (msgKey == m_threadRootKey)
            {
              RerootThread(child, curHdr, announcer);
              parentKeyNeedsSetting = false;
            }
          }
          curHdr->SetThreadParent(newHdrKey);
          if (msgKey == newHdrKey)
            parentKeyNeedsSetting = false;

          // OK, this is a reparenting - need to send notification
          if (announcer)
            announcer->NotifyParentChangedAll(msgKey, oldThreadParent, newHdrKey, nullptr);
#ifdef DEBUG_bienvenu1
          if (newHdrKey != m_threadKey)
            printf("adding second level child\n");
#endif
        }
        // Calculate a position for this child in date order
        else if (!hdrMoved && childIndex > 0 && moveIndex == 0)
        {
          PRTime curHdrDate;

          curHdr->GetDate(&curHdrDate);
          if (newHdrDate < curHdrDate)
            moveIndex = childIndex;
        }
      }
    }
  }
  // If this header is not a reply to a header in the thread, and isn't a parent
  // check to see if it starts with Re: - if not, and the first header does start
  // with re, should we make this header the top level header?
  // If it's date is less (or it's ID?), then yes.
  if (numChildren > 0 && !(newHdrFlags & nsMsgMessageFlags::HasRe) && !inReplyTo)
  {
    PRTime topLevelHdrDate;

    nsCOMPtr <nsIMsgDBHdr> topLevelHdr;
    rv = GetRootHdr(nullptr, getter_AddRefs(topLevelHdr));
    if (NS_SUCCEEDED(rv) && topLevelHdr)
    {
      topLevelHdr->GetDate(&topLevelHdrDate);
      if (newHdrDate < topLevelHdrDate)
      {
        RerootThread(child, topLevelHdr, announcer);
        mdb_pos outPos;
        m_mdbTable->MoveRow(m_mdbDB->GetEnv(), hdrRow, -1, 0, &outPos);
        hdrMoved = true;
        topLevelHdr->SetThreadParent(newHdrKey);
        parentKeyNeedsSetting = false;
        // ### need to get ancestor of new hdr here too.
        SetThreadRootKey(newHdrKey);
        child->SetThreadParent(nsMsgKey_None);
        // argh, here we'd need to adjust all the headers that listed
        // the demoted header as their thread parent, but only because
        // of subject threading. Adjust them to point to the new parent,
        // that is.
        ReparentNonReferenceChildrenOf(topLevelHdr, newHdrKey, announcer);
      }
    }
  }
  // OK, check to see if we added this header, and didn't parent it.

  if (numChildren > 0 && parentKeyNeedsSetting)
    child->SetThreadParent(m_threadRootKey);

  // Move child to keep thread sorted in ascending date order
  if (!hdrMoved && moveIndex > 0)
  {
    mdb_pos outPos;
    m_mdbTable->MoveRow(m_mdbDB->GetEnv(), hdrRow, -1, moveIndex, &outPos);
  }

  // do this after we've put the new hdr in the thread
  bool isKilled;
  child->GetIsKilled(&isKilled);
  if ((m_flags & nsMsgMessageFlags::Ignored || isKilled) && m_mdbDB)
    m_mdbDB->MarkHdrRead(child, true, nullptr);
#ifdef DEBUG_David_Bienvenu
  nsMsgKey msgHdrThreadKey;
  child->GetThreadId(&msgHdrThreadKey);
  NS_ASSERTION(msgHdrThreadKey == m_threadKey, "adding msg to thread it doesn't belong to");
#endif
#ifdef DEBUG_bienvenu1
  nsMsgDatabase *msgDB = static_cast<nsMsgDatabase*>(m_mdbDB);
  msgDB->DumpThread(m_threadRootKey);
#endif
  return rv;
}

nsresult nsMsgThread::ReparentNonReferenceChildrenOf(nsIMsgDBHdr *oldTopLevelHdr, nsMsgKey newParentKey,
                                                            nsIDBChangeAnnouncer *announcer)
{
  nsCOMPtr <nsIMsgDBHdr> curHdr;
  uint32_t numChildren;
  uint32_t childIndex = 0;

  GetNumChildren(&numChildren);
  for (childIndex = 0; childIndex < numChildren; childIndex++)
  {
    nsMsgKey oldTopLevelHdrKey;

    oldTopLevelHdr->GetMessageKey(&oldTopLevelHdrKey);
    nsresult rv = GetChildHdrAt(childIndex, getter_AddRefs(curHdr));
    if (NS_SUCCEEDED(rv) && curHdr)
    {
      nsMsgKey oldThreadParent, curHdrKey;
      nsMsgHdr* oldTopLevelMsgHdr = static_cast<nsMsgHdr*>(oldTopLevelHdr);      // closed system, cast ok
      curHdr->GetThreadParent(&oldThreadParent);
      curHdr->GetMessageKey(&curHdrKey);
      if (oldThreadParent == oldTopLevelHdrKey && curHdrKey != newParentKey && !oldTopLevelMsgHdr->IsParentOf(curHdr))
      {
        curHdr->GetThreadParent(&oldThreadParent);
        curHdr->SetThreadParent(newParentKey);
        // OK, this is a reparenting - need to send notification
        if (announcer)
          announcer->NotifyParentChangedAll(curHdrKey, oldThreadParent, newParentKey, nullptr);
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP nsMsgThread::GetChildKeyAt(int32_t aIndex, nsMsgKey *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  nsresult rv;

  if (aIndex >= (int32_t) m_numChildren)
  {
    *aResult = nsMsgKey_None;
    return NS_ERROR_ILLEGAL_VALUE;
  }
  mdbOid oid;
  rv = m_mdbTable->PosToOid( m_mdbDB->GetEnv(), aIndex, &oid);
  NS_ENSURE_SUCCESS(rv, rv);

  *aResult = oid.mOid_Id;
  return NS_OK;
}

NS_IMETHODIMP nsMsgThread::GetChildHdrAt(int32_t aIndex, nsIMsgDBHdr **result)
{
  // mork doesn't seem to handle this correctly, so deal with going off
  // the end here.
  if (aIndex < 0 || uint32_t(aIndex) >= m_numChildren)
    return NS_MSG_MESSAGE_NOT_FOUND;
  mdbOid oid;
  nsresult rv = m_mdbTable->PosToOid( m_mdbDB->GetEnv(), aIndex, &oid);
  NS_ENSURE_SUCCESS(rv, NS_MSG_MESSAGE_NOT_FOUND);
  nsIMdbRow *hdrRow = nullptr;
  rv = m_mdbTable->PosToRow(m_mdbDB->GetEnv(), aIndex, &hdrRow);
  NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && hdrRow, NS_ERROR_FAILURE);
  // CreateMsgHdr takes ownership of the hdrRow reference.
  rv = m_mdbDB->CreateMsgHdr(hdrRow,  oid.mOid_Id , result);
  return (NS_SUCCEEDED(rv)) ? NS_OK : NS_MSG_MESSAGE_NOT_FOUND;
}

NS_IMETHODIMP nsMsgThread::GetChild(nsMsgKey msgKey, nsIMsgDBHdr **result)
{
  nsresult rv;

  mdb_bool hasOid;
  mdbOid rowObjectId;

  NS_ENSURE_ARG_POINTER(result);
  NS_ENSURE_TRUE(m_mdbTable, NS_ERROR_INVALID_POINTER);

  *result = NULL;
  rowObjectId.mOid_Id = msgKey;
  rowObjectId.mOid_Scope = m_mdbDB->m_hdrRowScopeToken;
  rv = m_mdbTable->HasOid(m_mdbDB->GetEnv(), &rowObjectId, &hasOid);

  if (NS_SUCCEEDED(rv) && hasOid && m_mdbDB && m_mdbDB->m_mdbStore)
  {
    nsIMdbRow *hdrRow = nullptr;
    rv = m_mdbDB->m_mdbStore->GetRow(m_mdbDB->GetEnv(), &rowObjectId,  &hdrRow);
    NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && hdrRow, NS_ERROR_FAILURE);
    rv = m_mdbDB->CreateMsgHdr(hdrRow,  msgKey, result);
  }

  return rv;
}

NS_IMETHODIMP nsMsgThread::RemoveChildAt(int32_t aIndex)
{
  return NS_OK;
}

nsresult nsMsgThread::RemoveChild(nsMsgKey msgKey)
{
  nsresult rv;

  mdbOid  rowObjectId;
  rowObjectId.mOid_Id = msgKey;
  rowObjectId.mOid_Scope = m_mdbDB->m_hdrRowScopeToken;
  rv = m_mdbTable->CutOid(m_mdbDB->GetEnv(), &rowObjectId);
  // if this thread is empty, remove it from the all threads table.
  if (m_numChildren == 0 && m_mdbDB->m_mdbAllThreadsTable)
  {
    mdbOid rowID;
    rowID.mOid_Id = m_threadKey;
    rowID.mOid_Scope = m_mdbDB->m_threadRowScopeToken;

    m_mdbDB->m_mdbAllThreadsTable->CutOid(m_mdbDB->GetEnv(), &rowID);
  }
#if 0 // this seems to cause problems
  if (m_numChildren == 0 && m_metaRow && m_mdbDB)
    m_metaRow->CutAllColumns(m_mdbDB->GetEnv());
#endif

  return rv;
}

NS_IMETHODIMP nsMsgThread::RemoveChildHdr(nsIMsgDBHdr *child, nsIDBChangeAnnouncer *announcer)
{
  uint32_t flags;
  nsMsgKey key;
  nsMsgKey threadParent;

  NS_ENSURE_ARG_POINTER(child);

  child->GetFlags(&flags);
  child->GetMessageKey(&key);

  child->GetThreadParent(&threadParent);
  ReparentChildrenOf(key, threadParent, announcer);

  // if this was the newest msg, clear the newest msg date so we'll recalc.
  uint32_t date;
  child->GetDateInSeconds(&date);
  if (date == m_newestMsgDate)
    SetNewestMsgDate(0);

 if (!(flags & nsMsgMessageFlags::Read))
    ChangeUnreadChildCount(-1);
  ChangeChildCount(-1);
  return RemoveChild(key);
}

nsresult nsMsgThread::ReparentChildrenOf(nsMsgKey oldParent, nsMsgKey newParent, nsIDBChangeAnnouncer *announcer)
{
  nsresult rv = NS_OK;

  uint32_t numChildren;
  uint32_t childIndex = 0;

  GetNumChildren(&numChildren);

  nsCOMPtr <nsIMsgDBHdr> curHdr;
  if (numChildren > 0)
  {
    for (childIndex = 0; childIndex < numChildren; childIndex++)
    {
      rv = GetChildHdrAt(childIndex, getter_AddRefs(curHdr));
      if (NS_SUCCEEDED(rv) && curHdr)
      {
        nsMsgKey threadParent;

        curHdr->GetThreadParent(&threadParent);
        if (threadParent == oldParent)
        {
          nsMsgKey curKey;

          curHdr->SetThreadParent(newParent);
          curHdr->GetMessageKey(&curKey);
          if (announcer)
            announcer->NotifyParentChangedAll(curKey, oldParent, newParent, nullptr);
          // if the old parent was the root of the thread, then only the first child gets
          // promoted to root, and other children become children of the new root.
          if (newParent == nsMsgKey_None)
          {
            SetThreadRootKey(curKey);
            newParent = curKey;
          }
        }
      }
    }
  }
  return rv;
}

NS_IMETHODIMP nsMsgThread::MarkChildRead(bool bRead)
{
  ChangeUnreadChildCount(bRead ? -1 : 1);
  return NS_OK;
}

class nsMsgThreadEnumerator : public nsISimpleEnumerator {
public:
  NS_DECL_ISUPPORTS

  // nsISimpleEnumerator methods:
  NS_DECL_NSISIMPLEENUMERATOR

  // nsMsgThreadEnumerator methods:
  typedef nsresult (*nsMsgThreadEnumeratorFilter)(nsIMsgDBHdr* hdr, void* closure);

  nsMsgThreadEnumerator(nsMsgThread *thread, nsMsgKey startKey,
  nsMsgThreadEnumeratorFilter filter, void* closure);
  int32_t MsgKeyFirstChildIndex(nsMsgKey inMsgKey);
  virtual ~nsMsgThreadEnumerator();

protected:

  nsresult                Prefetch();

  nsIMdbTableRowCursor*   mRowCursor;
  nsCOMPtr <nsIMsgDBHdr>  mResultHdr;
  nsMsgThread*            mThread;
  nsMsgKey                mThreadParentKey;
  nsMsgKey                mFirstMsgKey;
  int32_t                 mChildIndex;
  bool                    mDone;
  bool                    mNeedToPrefetch;
  nsMsgThreadEnumeratorFilter     mFilter;
  void*                   mClosure;
  bool                    mFoundChildren;
};

nsMsgThreadEnumerator::nsMsgThreadEnumerator(nsMsgThread *thread, nsMsgKey startKey,
                                             nsMsgThreadEnumeratorFilter filter, void* closure)
                                             : mRowCursor(nullptr), mDone(false),
                                             mFilter(filter), mClosure(closure), mFoundChildren(false)
{
  mThreadParentKey = startKey;
  mChildIndex = 0;
  mThread = thread;
  mNeedToPrefetch = true;
  mFirstMsgKey = nsMsgKey_None;

  nsresult rv = mThread->GetRootHdr(nullptr, getter_AddRefs(mResultHdr));

  if (NS_SUCCEEDED(rv) && mResultHdr)
    mResultHdr->GetMessageKey(&mFirstMsgKey);

  uint32_t numChildren;
  mThread->GetNumChildren(&numChildren);

  if (mThreadParentKey != nsMsgKey_None)
  {
    nsMsgKey msgKey = nsMsgKey_None;
    uint32_t childIndex = 0;


    for (childIndex = 0; childIndex < numChildren; childIndex++)
    {
      rv = mThread->GetChildHdrAt(childIndex, getter_AddRefs(mResultHdr));
      if (NS_SUCCEEDED(rv) && mResultHdr)
      {
        mResultHdr->GetMessageKey(&msgKey);

        if (msgKey == startKey)
        {
          mChildIndex = MsgKeyFirstChildIndex(msgKey);
          mDone = (mChildIndex < 0);
          break;
        }

        if (mDone)
          break;

      }
      else
        NS_ASSERTION(false, "couldn't get child from thread");
    }
  }

#ifdef DEBUG_bienvenu1
  nsCOMPtr <nsIMsgDBHdr> child;
  for (uint32_t childIndex = 0; childIndex < numChildren; childIndex++)
  {
    rv = mThread->GetChildHdrAt(childIndex, getter_AddRefs(child));
    if (NS_SUCCEEDED(rv) && child)
    {
      nsMsgKey threadParent;
      nsMsgKey msgKey;
      // we're only doing one level of threading, so check if caller is
      // asking for children of the first message in the thread or not.
      // if not, we will tell him there are no children.
      child->GetMessageKey(&msgKey);
      child->GetThreadParent(&threadParent);

      printf("index = %ld key = %ld parent = %lx\n", childIndex, msgKey, threadParent);
    }
  }
#endif
  NS_ADDREF(thread);
}

nsMsgThreadEnumerator::~nsMsgThreadEnumerator()
{
    NS_RELEASE(mThread);
}

NS_IMPL_ISUPPORTS1(nsMsgThreadEnumerator, nsISimpleEnumerator)


int32_t nsMsgThreadEnumerator::MsgKeyFirstChildIndex(nsMsgKey inMsgKey)
{
  //	if (msgKey != mThreadParentKey)
  //		mDone = true;
  // look through rest of thread looking for a child of this message.
  // If the inMsgKey is the first message in the thread, then all children
  // without parents are considered to be children of inMsgKey.
  // Otherwise, only true children qualify.
  uint32_t numChildren;
  nsCOMPtr <nsIMsgDBHdr> curHdr;
  int32_t firstChildIndex = -1;

  mThread->GetNumChildren(&numChildren);

  // if this is the first message in the thread, just check if there's more than
  // one message in the thread.
  // if (inMsgKey == mThread->m_threadRootKey)
  // return (numChildren > 1) ? 1 : -1;

  for (uint32_t curChildIndex = 0; curChildIndex < numChildren; curChildIndex++)
  {
    nsresult rv = mThread->GetChildHdrAt(curChildIndex, getter_AddRefs(curHdr));
    if (NS_SUCCEEDED(rv) && curHdr)
    {
      nsMsgKey parentKey;

      curHdr->GetThreadParent(&parentKey);
      if (parentKey == inMsgKey)
      {
        firstChildIndex = curChildIndex;
        break;
      }
    }
  }
#ifdef DEBUG_bienvenu1
  printf("first child index of %ld = %ld\n", inMsgKey, firstChildIndex);
#endif
  return firstChildIndex;
}

NS_IMETHODIMP nsMsgThreadEnumerator::GetNext(nsISupports **aItem)
{
  NS_ENSURE_ARG_POINTER(aItem);
  nsresult rv;

  if (mNeedToPrefetch)
  {
    rv = Prefetch();
    NS_ENSURE_SUCCESS(rv, rv);
  }  

  if (mResultHdr)
  {
    *aItem = mResultHdr;
    NS_ADDREF(*aItem);
    mNeedToPrefetch = true;
  }
  return NS_OK;
}

nsresult nsMsgThreadEnumerator::Prefetch()
{
  nsresult rv=NS_OK;          // XXX or should this default to an error?
  mResultHdr = nullptr;
  if (mThreadParentKey == nsMsgKey_None)
  {
    rv = mThread->GetRootHdr(&mChildIndex, getter_AddRefs(mResultHdr));
    NS_ASSERTION(NS_SUCCEEDED(rv) && mResultHdr, "better be able to get root hdr");
    mChildIndex = 0; // since root can be anywhere, set mChildIndex to 0.
  }
  else if (!mDone)
  {
    uint32_t numChildren;
    mThread->GetNumChildren(&numChildren);

    while (mChildIndex < (int32_t) numChildren)
    {
      rv  = mThread->GetChildHdrAt(mChildIndex++, getter_AddRefs(mResultHdr));
      if (NS_SUCCEEDED(rv) && mResultHdr)
      {
        nsMsgKey parentKey;
        nsMsgKey curKey;

        if (mFilter && NS_FAILED(mFilter(mResultHdr, mClosure))) {
          mResultHdr = nullptr;
          continue;
        }

        mResultHdr->GetThreadParent(&parentKey);
        mResultHdr->GetMessageKey(&curKey);
        // if the parent is the same as the msg we're enumerating over,
        // or the parentKey isn't set, and we're iterating over the top
        // level message in the thread, then leave mResultHdr set to cur msg.
        if (parentKey == mThreadParentKey ||
          (parentKey == nsMsgKey_None
          && mThreadParentKey == mFirstMsgKey && curKey != mThreadParentKey))
          break;
        mResultHdr = nullptr;
      }
      else
        NS_ASSERTION(false, "better be able to get child");
    }
    if (!mResultHdr && mThreadParentKey == mFirstMsgKey && !mFoundChildren && numChildren > 1)
      mThread->ReparentMsgsWithInvalidParent(numChildren, mThreadParentKey);
  }
  if (!mResultHdr)
  {
    mDone = true;
    return NS_ERROR_FAILURE;
  }
  if (NS_FAILED(rv))
  {
    mDone = true;
    return rv;
  }
  else
    mNeedToPrefetch = false;
  mFoundChildren = true;

#ifdef DEBUG_bienvenu1
	nsMsgKey debugMsgKey;
	mResultHdr->GetMessageKey(&debugMsgKey);
	printf("next for %ld = %ld\n", mThreadParentKey, debugMsgKey);
#endif

  return rv;
}

NS_IMETHODIMP nsMsgThreadEnumerator::HasMoreElements(bool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  if (mNeedToPrefetch)
    Prefetch();
  *aResult = !mDone;
  return NS_OK;
}

NS_IMETHODIMP nsMsgThread::EnumerateMessages(nsMsgKey parentKey, nsISimpleEnumerator* *result)
{
  nsMsgThreadEnumerator* e = new nsMsgThreadEnumerator(this, parentKey, nullptr, nullptr);
  NS_ENSURE_TRUE(e, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(*result = e);
  return NS_OK;
}

nsresult nsMsgThread::ReparentMsgsWithInvalidParent(uint32_t numChildren, nsMsgKey threadParentKey)
{
  nsresult rv = NS_OK;
  // run through looking for messages that don't have a correct parent,
  // i.e., a parent that's in the thread!
  for (int32_t childIndex = 0; childIndex < (int32_t) numChildren; childIndex++)
  {
    nsCOMPtr <nsIMsgDBHdr> curChild;
    rv  = GetChildHdrAt(childIndex, getter_AddRefs(curChild));
    if (NS_SUCCEEDED(rv) && curChild)
    {
      nsMsgKey parentKey;
      nsCOMPtr <nsIMsgDBHdr> parent;

      curChild->GetThreadParent(&parentKey);

      if (parentKey != nsMsgKey_None)
      {
        GetChild(parentKey, getter_AddRefs(parent));
        if (!parent)
          curChild->SetThreadParent(threadParentKey);
        else
        {
          nsMsgKey childKey;
          curChild->GetMessageKey(&childKey);
          // can't be your own parent; set parent to thread parent,
          // or make ourselves the root if we are the root.
          if (childKey == parentKey)
            curChild->SetThreadParent(m_threadRootKey == childKey ? 
                                      nsMsgKey_None : m_threadRootKey);
        }
      }
    }
  }
  return rv;
}

NS_IMETHODIMP nsMsgThread::GetRootHdr(int32_t *resultIndex, nsIMsgDBHdr **result)
{
  NS_ENSURE_ARG_POINTER(result);

  *result = nullptr;
  nsresult rv = NS_OK;

  if (m_threadRootKey != nsMsgKey_None)
  {
    rv = GetChildHdrForKey(m_threadRootKey, result, resultIndex);
    if (NS_SUCCEEDED(rv) && *result)
    {
      // check that we're really the root key.
      nsMsgKey parentKey;
      (*result)->GetThreadParent(&parentKey);
      if (parentKey == nsMsgKey_None)
        return rv;
      NS_RELEASE(*result);
    }
#ifdef DEBUG_David_Bienvenu
    printf("need to reset thread root key\n");
#endif
    uint32_t numChildren;
    nsMsgKey threadParentKey = nsMsgKey_None;
    GetNumChildren(&numChildren);

    for (int32_t childIndex = 0; childIndex < (int32_t) numChildren; childIndex++)
    {
      nsCOMPtr <nsIMsgDBHdr> curChild;
      rv  = GetChildHdrAt(childIndex, getter_AddRefs(curChild));
      if (NS_SUCCEEDED(rv) && curChild)
      {
        nsMsgKey parentKey;

        curChild->GetThreadParent(&parentKey);
        if (parentKey == nsMsgKey_None)
        {
          curChild->GetMessageKey(&threadParentKey);
          if (*result)
          {
            NS_WARNING("two top level msgs, not good");
            continue;
          }
          SetThreadRootKey(threadParentKey);
          if (resultIndex)
            *resultIndex = childIndex;
          NS_ADDREF(*result = curChild);
          ReparentMsgsWithInvalidParent(numChildren, threadParentKey);
          //            return NS_OK;
        }
      }
    }
  }
  if (!*result)
  {
    // if we can't get the thread root key, we'll just get the first hdr.
    // there's a bug where sometimes we weren't resetting the thread root key
    // when removing the thread root key.
    if (resultIndex)
      *resultIndex = 0;
    rv = GetChildHdrAt(0, result);
  }
  if (!*result)
    return rv;
  // Check that the thread id of the message is this thread.
  nsMsgKey threadId = nsMsgKey_None;
  (void)(*result)->GetThreadId(&threadId);
  if (threadId != m_threadKey)
    (*result)->SetThreadId(m_threadKey);
  return rv;
}

nsresult nsMsgThread::ChangeChildCount(int32_t delta)
{
  nsresult rv;

  uint32_t childCount = 0;
  m_mdbDB->RowCellColumnToUInt32(m_metaRow, m_mdbDB->m_threadChildrenColumnToken, childCount);

  NS_WARN_IF_FALSE(childCount != 0 || delta > 0, "child count gone negative");
  childCount += delta;

  NS_WARN_IF_FALSE((int32_t) childCount >= 0, "child count gone to 0 or below");
  if ((int32_t) childCount < 0)	// force child count to >= 0
    childCount = 0;

  rv = m_mdbDB->UInt32ToRowCellColumn(m_metaRow, m_mdbDB->m_threadChildrenColumnToken, childCount);
  m_numChildren = childCount;
  return rv;
}

nsresult nsMsgThread::ChangeUnreadChildCount(int32_t delta)
{
  nsresult rv;

  uint32_t childCount = 0;
  m_mdbDB->RowCellColumnToUInt32(m_metaRow, m_mdbDB->m_threadUnreadChildrenColumnToken, childCount);
  childCount += delta;
  if ((int32_t) childCount < 0)
  {
#ifdef DEBUG_bienvenu1
    NS_ASSERTION(false, "negative unread child count");
#endif
    childCount = 0;
  }
  rv = m_mdbDB->UInt32ToRowCellColumn(m_metaRow, m_mdbDB->m_threadUnreadChildrenColumnToken, childCount);
  m_numUnreadChildren = childCount;
  return rv;
}

nsresult nsMsgThread::SetThreadRootKey(nsMsgKey threadRootKey)
{
  m_threadRootKey = threadRootKey;
  return m_mdbDB->UInt32ToRowCellColumn(m_metaRow, m_mdbDB->m_threadRootKeyColumnToken, threadRootKey);
}

nsresult nsMsgThread::GetChildHdrForKey(nsMsgKey desiredKey, nsIMsgDBHdr **result, int32_t *resultIndex)
{
  uint32_t numChildren;
  uint32_t childIndex = 0;
  nsresult rv = NS_OK;        // XXX or should this default to an error?

  NS_ENSURE_ARG_POINTER(result);

  GetNumChildren(&numChildren);

  if ((int32_t) numChildren < 0)
    numChildren = 0;

  for (childIndex = 0; childIndex < numChildren; childIndex++)
  {
    rv = GetChildHdrAt(childIndex, result);
    if (NS_SUCCEEDED(rv) && *result)
    {
      nsMsgKey msgKey;
      // we're only doing one level of threading, so check if caller is
      // asking for children of the first message in the thread or not.
      // if not, we will tell him there are no children.
      (*result)->GetMessageKey(&msgKey);

      if (msgKey == desiredKey)
      {
        nsMsgKey threadKey;
        (*result)->GetThreadId(&threadKey);
        if (threadKey != m_threadKey) // this msg isn't in this thread
        {
          NS_WARNING("msg in wrong thread - this shouldn't happen");
          uint32_t msgSize;
          (*result)->GetMessageSize(&msgSize);
          if (msgSize == 0) // this is a phantom message - let's get rid of it.
          {
            RemoveChild(msgKey);
            rv = NS_ERROR_UNEXPECTED;
          }
          else
          {
            // otherwise, let's try to figure out which thread
            // this message really belongs to.
            nsCOMPtr<nsIMsgThread> threadKeyThread = 
                  dont_AddRef(m_mdbDB->GetThreadForThreadId(threadKey));
            if (threadKeyThread)
            {
              nsCOMPtr<nsIMsgDBHdr> otherThreadHdr;
              threadKeyThread->GetChild(msgKey, getter_AddRefs(otherThreadHdr));
              if (otherThreadHdr)
              {
                // Message is in one thread but has a different thread id.
                // Remove it from the thread and then rethread it.
                RemoveChild(msgKey);
                threadKeyThread->RemoveChildHdr(otherThreadHdr, nullptr);
                bool newThread;
                nsMsgHdr* msgHdr = static_cast<nsMsgHdr*>(otherThreadHdr.get());
                m_mdbDB->ThreadNewHdr(msgHdr, newThread);
              }
              else
              {
                (*result)->SetThreadId(m_threadKey);
              }
            }
          }
        }
        break;
      }
      NS_RELEASE(*result);
    }
  }
  if (resultIndex)
    *resultIndex = childIndex;

  return rv;
}

NS_IMETHODIMP nsMsgThread::GetFirstUnreadChild(nsIMsgDBHdr **result)
{
  NS_ENSURE_ARG_POINTER(result);
  uint32_t numChildren;
  nsresult rv = NS_OK;
  uint8_t minLevel = 0xff;

  GetNumChildren(&numChildren);

  if ((int32_t) numChildren < 0)
    numChildren = 0;

  nsCOMPtr <nsIMsgDBHdr> retHdr;

  for (uint32_t childIndex = 0; childIndex < numChildren; childIndex++)
  {
    nsCOMPtr <nsIMsgDBHdr> child;
    rv = GetChildHdrAt(childIndex, getter_AddRefs(child));
    if (NS_SUCCEEDED(rv) && child)
    {
      nsMsgKey msgKey;
      child->GetMessageKey(&msgKey);

      bool isRead;
      rv = m_mdbDB->IsRead(msgKey, &isRead);
      if (NS_SUCCEEDED(rv) && !isRead)
      {
        // this is the root, so it's the best we're going to do.
        if (msgKey == m_threadRootKey)
        {
          retHdr = child;
          break;
        }
        uint8_t level = 0;
        nsMsgKey parentId;
        child->GetThreadParent(&parentId);
        nsCOMPtr <nsIMsgDBHdr> parent;
        // count number of ancestors - that's our level
        while (parentId != nsMsgKey_None)
        {
          rv = m_mdbDB->GetMsgHdrForKey(parentId, getter_AddRefs(parent));
          if (parent)
          {
            parent->GetThreadParent(&parentId);
            level++;
          }
        }
        if (level < minLevel)
        {
          minLevel = level;
          retHdr = child;
        }
      }
    }
  }

  NS_IF_ADDREF(*result = retHdr);
  return rv;
}

NS_IMETHODIMP nsMsgThread::GetNewestMsgDate(uint32_t *aResult)
{
  // if this hasn't been set, figure it out by enumerating the msgs in the thread.
  if (!m_newestMsgDate)
  {
    uint32_t numChildren;
    nsresult rv;

    GetNumChildren(&numChildren);

    if ((int32_t) numChildren < 0)
      numChildren = 0;

    for (uint32_t childIndex = 0; childIndex < numChildren; childIndex++)
    {
      nsCOMPtr <nsIMsgDBHdr> child;
      rv = GetChildHdrAt(childIndex, getter_AddRefs(child));
      if (NS_SUCCEEDED(rv))
      {
        uint32_t msgDate;
        child->GetDateInSeconds(&msgDate);
        if (msgDate > m_newestMsgDate)
          m_newestMsgDate = msgDate;
      }
    }

  }
  *aResult = m_newestMsgDate;
  return NS_OK;
}


NS_IMETHODIMP nsMsgThread::SetNewestMsgDate(uint32_t aNewestMsgDate)
{
  m_newestMsgDate = aNewestMsgDate;
  return m_mdbDB->UInt32ToRowCellColumn(m_metaRow, m_mdbDB->m_threadNewestMsgDateColumnToken, aNewestMsgDate);
}
