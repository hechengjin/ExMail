/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "msgCore.h"
#include "nsMsgGroupThread.h"
#include "nsMsgDBView.h"
#include "nsMsgMessageFlags.h"
#include "nsMsgUtils.h"

NS_IMPL_ISUPPORTS1(nsMsgGroupThread, nsIMsgThread)

nsMsgGroupThread::nsMsgGroupThread()
{
  Init();
}
nsMsgGroupThread::nsMsgGroupThread(nsIMsgDatabase *db)
{
  m_db = db;
  Init();
}

void nsMsgGroupThread::Init()
{
  m_threadKey = nsMsgKey_None; 
  m_threadRootKey = nsMsgKey_None;
  m_numUnreadChildren = 0;	
  m_flags = 0;
  m_newestMsgDate = 0;
  m_dummy = false;
}

nsMsgGroupThread::~nsMsgGroupThread()
{
}

NS_IMETHODIMP nsMsgGroupThread::SetThreadKey(nsMsgKey threadKey)
{
  m_threadKey = threadKey;
  // by definition, the initial thread key is also the thread root key.
  m_threadRootKey = threadKey;
  return NS_OK;
}

NS_IMETHODIMP nsMsgGroupThread::GetThreadKey(nsMsgKey *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = m_threadKey;
  return NS_OK;
}

NS_IMETHODIMP nsMsgGroupThread::GetFlags(uint32_t *aFlags)
{
  NS_ENSURE_ARG_POINTER(aFlags);
  *aFlags = m_flags;
  return NS_OK;
}

NS_IMETHODIMP nsMsgGroupThread::SetFlags(uint32_t aFlags)
{
  m_flags = aFlags;
  return NS_OK;
}

NS_IMETHODIMP nsMsgGroupThread::SetSubject(const nsACString& aSubject)
{
  NS_ASSERTION(false, "shouldn't call this");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgGroupThread::GetSubject(nsACString& result)
{
  NS_ASSERTION(false, "shouldn't call this");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgGroupThread::GetNumChildren(uint32_t *aNumChildren)
{
  NS_ENSURE_ARG_POINTER(aNumChildren);
  *aNumChildren = m_keys.Length(); // - ((m_dummy) ? 1 : 0);
  return NS_OK;
}

uint32_t nsMsgGroupThread::NumRealChildren()
{
  return m_keys.Length() - ((m_dummy) ? 1 : 0);
}

NS_IMETHODIMP nsMsgGroupThread::GetNumUnreadChildren (uint32_t *aNumUnreadChildren)
{
  NS_ENSURE_ARG_POINTER(aNumUnreadChildren);
  *aNumUnreadChildren = m_numUnreadChildren;
  return NS_OK;
}

void nsMsgGroupThread::InsertMsgHdrAt(nsMsgViewIndex index, nsIMsgDBHdr *hdr)
{
  nsMsgKey msgKey;
  hdr->GetMessageKey(&msgKey);
  m_keys.InsertElementAt(index, msgKey);
}

void nsMsgGroupThread::SetMsgHdrAt(nsMsgViewIndex index, nsIMsgDBHdr *hdr)
{
  nsMsgKey msgKey;
  hdr->GetMessageKey(&msgKey);
  m_keys[index] = msgKey;
}

nsMsgViewIndex nsMsgGroupThread::FindMsgHdr(nsIMsgDBHdr *hdr)
{
  nsMsgKey msgKey;
  hdr->GetMessageKey(&msgKey);
  return (nsMsgViewIndex)m_keys.IndexOf(msgKey);
}

NS_IMETHODIMP nsMsgGroupThread::AddChild(nsIMsgDBHdr *child, nsIMsgDBHdr *inReplyTo, bool threadInThread, 
                                    nsIDBChangeAnnouncer *announcer)
{
  NS_ASSERTION(false, "shouldn't call this");
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsMsgViewIndex nsMsgGroupThread::AddMsgHdrInDateOrder(nsIMsgDBHdr *child, nsMsgDBView *view)
{
  nsMsgKey newHdrKey;
  child->GetMessageKey(&newHdrKey);
  uint32_t insertIndex = 0;
  // since we're sorted by date, we could do a binary search for the 
  // insert point. Or, we could start at the end...
  if (m_keys.Length() > 0)
  {
    nsMsgViewSortTypeValue  sortType;
    nsMsgViewSortOrderValue sortOrder;
    (void) view->GetSortType(&sortType);
    (void) view->GetSortOrder(&sortOrder);
    // historical behavior is ascending date order unless our primary sort is
    //  on date
    nsMsgViewSortOrderValue threadSortOrder = 
      (sortType == nsMsgViewSortType::byDate
        && sortOrder == nsMsgViewSortOrder::descending) ? 
          nsMsgViewSortOrder::descending : nsMsgViewSortOrder::ascending;
    // new behavior is tricky and uses the secondary sort order if the secondary
    //  sort is on the date
    nsMsgViewSortTypeValue  secondarySortType;
    nsMsgViewSortOrderValue secondarySortOrder;
    (void) view->GetSecondarySortType(&secondarySortType);
    (void) view->GetSecondarySortOrder(&secondarySortOrder);
    if (secondarySortType == nsMsgViewSortType::byDate)
      threadSortOrder = secondarySortOrder;
    // sort by date within group.
    insertIndex = GetInsertIndexFromView(view, child, threadSortOrder);
  }
  m_keys.InsertElementAt(insertIndex, newHdrKey);
  if (!insertIndex)
    m_threadRootKey = newHdrKey;
  return insertIndex;
}

nsMsgViewIndex 
nsMsgGroupThread::GetInsertIndexFromView(nsMsgDBView *view, 
                                          nsIMsgDBHdr *child, 
                                          nsMsgViewSortOrderValue threadSortOrder)
{
   return view->GetInsertIndexHelper(child, m_keys, nullptr, threadSortOrder, nsMsgViewSortType::byDate);
}

nsMsgViewIndex nsMsgGroupThread::AddChildFromGroupView(nsIMsgDBHdr *child, nsMsgDBView *view)
{
  uint32_t newHdrFlags = 0;
  uint32_t msgDate;
  nsMsgKey newHdrKey = 0;
  
  child->GetFlags(&newHdrFlags);
  child->GetMessageKey(&newHdrKey);
  child->GetDateInSeconds(&msgDate);
  if (msgDate > m_newestMsgDate)
    SetNewestMsgDate(msgDate);

  child->AndFlags(~(nsMsgMessageFlags::Watched), &newHdrFlags);
  uint32_t numChildren;
  
  // get the num children before we add the new header.
  GetNumChildren(&numChildren);
  
  // if this is an empty thread, set the root key to this header's key
  if (numChildren == 0)
    m_threadRootKey = newHdrKey;
  
  if (! (newHdrFlags & nsMsgMessageFlags::Read))
    ChangeUnreadChildCount(1);

  return AddMsgHdrInDateOrder(child, view);
}

nsresult nsMsgGroupThread::ReparentNonReferenceChildrenOf(nsIMsgDBHdr *topLevelHdr, nsMsgKey newParentKey,
                                                            nsIDBChangeAnnouncer *announcer)
{
  return NS_OK;
}

NS_IMETHODIMP nsMsgGroupThread::GetChildKeyAt(int32_t aIndex, nsMsgKey *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  if (aIndex < 0 || (uint32_t)aIndex >= m_keys.Length())
    return NS_ERROR_INVALID_ARG;
  *aResult = m_keys[aIndex];
  return NS_OK;
}

NS_IMETHODIMP nsMsgGroupThread::GetChildHdrAt(int32_t aIndex, nsIMsgDBHdr **aResult)
{
  if (aIndex < 0 || (uint32_t)aIndex >= m_keys.Length())
    return NS_MSG_MESSAGE_NOT_FOUND;
  return m_db->GetMsgHdrForKey(m_keys[aIndex], aResult);
}


NS_IMETHODIMP nsMsgGroupThread::GetChild(nsMsgKey msgKey, nsIMsgDBHdr **aResult)
{
  return GetChildHdrAt((int32_t) m_keys.IndexOf(msgKey), aResult);
}

NS_IMETHODIMP nsMsgGroupThread::RemoveChildAt(int32_t aIndex)
{
  m_keys.RemoveElementAt(aIndex);
  return NS_OK;
}

nsresult nsMsgGroupThread::RemoveChild(nsMsgKey msgKey)
{
  m_keys.RemoveElement(msgKey);
  return NS_OK;
}

NS_IMETHODIMP nsMsgGroupThread::RemoveChildHdr(nsIMsgDBHdr *child, nsIDBChangeAnnouncer *announcer)
{
  NS_ENSURE_ARG_POINTER(child);

  uint32_t flags;
  nsMsgKey key;

  child->GetFlags(&flags);
  child->GetMessageKey(&key);

  // if this was the newest msg, clear the newest msg date so we'll recalc.
  uint32_t date;
  child->GetDateInSeconds(&date);
  if (date == m_newestMsgDate)
    SetNewestMsgDate(0);

  if (!(flags & nsMsgMessageFlags::Read))
    ChangeUnreadChildCount(-1);
  nsMsgViewIndex threadIndex = FindMsgHdr(child);
  bool wasFirstChild = threadIndex == 0;
  nsresult rv = RemoveChildAt(threadIndex);
  // if we're deleting the root of a dummy thread, need to update the threadKey
  // and the dummy header at position 0
  if (m_dummy && wasFirstChild && m_keys.Length() > 1)
  {
    nsIMsgDBHdr *newRootChild;
    rv = GetChildHdrAt(1, &newRootChild);
    NS_ENSURE_SUCCESS(rv, rv);
    SetMsgHdrAt(0, newRootChild);
  }

  return rv;
}

nsresult nsMsgGroupThread::ReparentChildrenOf(nsMsgKey oldParent, nsMsgKey newParent, nsIDBChangeAnnouncer *announcer)
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
            m_threadRootKey = curKey;
            newParent = curKey;
          }
        }
      }
    }
  }
  return rv;
}

NS_IMETHODIMP nsMsgGroupThread::MarkChildRead(bool bRead)
{
  ChangeUnreadChildCount(bRead ? -1 : 1);
  return NS_OK;
}

// this could be moved into utils, because I think it's the same as the db impl.
class nsMsgGroupThreadEnumerator : public nsISimpleEnumerator {
public:
  NS_DECL_ISUPPORTS
    
  // nsISimpleEnumerator methods:
  NS_DECL_NSISIMPLEENUMERATOR
    
  // nsMsgGroupThreadEnumerator methods:
  typedef nsresult (*nsMsgGroupThreadEnumeratorFilter)(nsIMsgDBHdr* hdr, void* closure);
  
  nsMsgGroupThreadEnumerator(nsMsgGroupThread *thread, nsMsgKey startKey,
  nsMsgGroupThreadEnumeratorFilter filter, void* closure);
  int32_t MsgKeyFirstChildIndex(nsMsgKey inMsgKey);
  virtual ~nsMsgGroupThreadEnumerator();
  
protected:
  
  nsresult                Prefetch();
  
  nsCOMPtr <nsIMsgDBHdr>  mResultHdr;
  nsMsgGroupThread*       mThread;
  nsMsgKey                mThreadParentKey;
  nsMsgKey                mFirstMsgKey;
  int32_t                 mChildIndex;
  bool                    mDone;
  bool                    mNeedToPrefetch;
  nsMsgGroupThreadEnumeratorFilter     mFilter;
  void*                   mClosure;
  bool                    mFoundChildren;
};

nsMsgGroupThreadEnumerator::nsMsgGroupThreadEnumerator(nsMsgGroupThread *thread, nsMsgKey startKey,
                                             nsMsgGroupThreadEnumeratorFilter filter, void* closure)
                                             : mDone(false),
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

nsMsgGroupThreadEnumerator::~nsMsgGroupThreadEnumerator()
{
    NS_RELEASE(mThread);
}

NS_IMPL_ISUPPORTS1(nsMsgGroupThreadEnumerator, nsISimpleEnumerator)


int32_t nsMsgGroupThreadEnumerator::MsgKeyFirstChildIndex(nsMsgKey inMsgKey)
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
  //	if (inMsgKey == mThread->m_threadRootKey)
  //		return (numChildren > 1) ? 1 : -1;
  
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

NS_IMETHODIMP nsMsgGroupThreadEnumerator::GetNext(nsISupports **aItem)
{
  if (!aItem)
    return NS_ERROR_NULL_POINTER;
  nsresult rv = NS_OK;
  
  if (mNeedToPrefetch)
    rv = Prefetch();
  
  if (NS_SUCCEEDED(rv) && mResultHdr) 
  {
    *aItem = mResultHdr;
    NS_ADDREF(*aItem);
    mNeedToPrefetch = true;
  }
  return rv;
}

nsresult nsMsgGroupThreadEnumerator::Prefetch()
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
    {
//      mThread->ReparentMsgsWithInvalidParent(numChildren, mThreadParentKey);
    }
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

NS_IMETHODIMP nsMsgGroupThreadEnumerator::HasMoreElements(bool *aResult)
{
  if (!aResult)
    return NS_ERROR_NULL_POINTER;
  if (mNeedToPrefetch)
    Prefetch();
  *aResult = !mDone;
  return NS_OK;
}

NS_IMETHODIMP nsMsgGroupThread::EnumerateMessages(nsMsgKey parentKey, nsISimpleEnumerator* *result)
{
    nsMsgGroupThreadEnumerator* e = new nsMsgGroupThreadEnumerator(this, parentKey, nullptr, nullptr);
    if (e == nullptr)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(e);
    *result = e;

    return NS_OK;
}
#if 0
nsresult nsMsgGroupThread::ReparentMsgsWithInvalidParent(uint32_t numChildren, nsMsgKey threadParentKey)
{
  nsresult ret = NS_OK;
  // run through looking for messages that don't have a correct parent, 
  // i.e., a parent that's in the thread!
  for (int32_t childIndex = 0; childIndex < (int32_t) numChildren; childIndex++)
  {
    nsCOMPtr <nsIMsgDBHdr> curChild;
    ret  = GetChildHdrAt(childIndex, getter_AddRefs(curChild));
    if (NS_SUCCEEDED(ret) && curChild)
    {
      nsMsgKey parentKey;
      nsCOMPtr <nsIMsgDBHdr> parent;
      
      curChild->GetThreadParent(&parentKey);
      
      if (parentKey != nsMsgKey_None)
      {
        GetChild(parentKey, getter_AddRefs(parent));
        if (!parent)
          curChild->SetThreadParent(threadParentKey);
      }
    }
  }
  return ret;
}
#endif
NS_IMETHODIMP nsMsgGroupThread::GetRootHdr(int32_t *resultIndex, nsIMsgDBHdr **result)
{
  if (!result)
    return NS_ERROR_NULL_POINTER;
  
  *result = nullptr;
  
  if (m_threadRootKey != nsMsgKey_None)
  {
    nsresult ret = GetChildHdrForKey(m_threadRootKey, result, resultIndex);
    if (NS_SUCCEEDED(ret) && *result)
      return ret;
    else
    {
      printf("need to reset thread root key\n");
      uint32_t numChildren;
      nsMsgKey threadParentKey = nsMsgKey_None;
      GetNumChildren(&numChildren);
      
      for (int32_t childIndex = 0; childIndex < (int32_t) numChildren; childIndex++)
      {
        nsCOMPtr <nsIMsgDBHdr> curChild;
        ret  = GetChildHdrAt(childIndex, getter_AddRefs(curChild));
        if (NS_SUCCEEDED(ret) && curChild)
        {
          nsMsgKey parentKey;
          
          curChild->GetThreadParent(&parentKey);
          if (parentKey == nsMsgKey_None)
          {
            NS_ASSERTION(!(*result), "two top level msgs, not good");
            curChild->GetMessageKey(&threadParentKey);
            m_threadRootKey = threadParentKey;
            if (resultIndex)
              *resultIndex = childIndex;
            *result = curChild;
            NS_ADDREF(*result);
//            ReparentMsgsWithInvalidParent(numChildren, threadParentKey);
            //            return NS_OK;
          }
        }
      }
      if (*result)
      {
        return NS_OK;
      }
    }
    // if we can't get the thread root key, we'll just get the first hdr.
    // there's a bug where sometimes we weren't resetting the thread root key 
    // when removing the thread root key.
  }
  if (resultIndex)
    *resultIndex = 0;
  return GetChildHdrAt(0, result);
}

nsresult nsMsgGroupThread::ChangeUnreadChildCount(int32_t delta)
{
  m_numUnreadChildren += delta;
  return NS_OK;
}

nsresult nsMsgGroupThread::GetChildHdrForKey(nsMsgKey desiredKey, nsIMsgDBHdr **result, int32_t *resultIndex)
{
  uint32_t numChildren;
  uint32_t childIndex = 0;
  nsresult rv = NS_OK;        // XXX or should this default to an error?
  
  if (!result)
    return NS_ERROR_NULL_POINTER;
  
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
        break;
      NS_RELEASE(*result);
    }
  }
  if (resultIndex)
    *resultIndex = childIndex;
  
  return rv;
}

NS_IMETHODIMP nsMsgGroupThread::GetFirstUnreadChild(nsIMsgDBHdr **result)
{
  NS_ENSURE_ARG(result);
  uint32_t numChildren;
  nsresult rv = NS_OK;
  
  GetNumChildren(&numChildren);
  
  if ((int32_t) numChildren < 0)
    numChildren = 0;
  
  for (uint32_t childIndex = 0; childIndex < numChildren; childIndex++)
  {
    nsCOMPtr <nsIMsgDBHdr> child;
    rv = GetChildHdrAt(childIndex, getter_AddRefs(child));
    if (NS_SUCCEEDED(rv) && child)
    {
      nsMsgKey msgKey;
      child->GetMessageKey(&msgKey);
      
      bool isRead;
      rv = m_db->IsRead(msgKey, &isRead);
      if (NS_SUCCEEDED(rv) && !isRead)
      {
        *result = child;
        NS_ADDREF(*result);
        break;
      }
    }
  }
  
  return rv;
}

NS_IMETHODIMP nsMsgGroupThread::GetNewestMsgDate(uint32_t *aResult) 
{
  // if this hasn't been set, figure it out by enumerating the msgs in the thread.
  if (!m_newestMsgDate)
  {
    uint32_t numChildren;
    nsresult rv = NS_OK;
  
    GetNumChildren(&numChildren);
  
    if ((int32_t) numChildren < 0)
      numChildren = 0;
  
    for (uint32_t childIndex = 0; childIndex < numChildren; childIndex++)
    {
      nsCOMPtr <nsIMsgDBHdr> child;
      rv = GetChildHdrAt(childIndex, getter_AddRefs(child));
      if (NS_SUCCEEDED(rv) && child)
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


NS_IMETHODIMP nsMsgGroupThread::SetNewestMsgDate(uint32_t aNewestMsgDate) 
{
  m_newestMsgDate = aNewestMsgDate;
  return NS_OK;
}

nsMsgXFGroupThread::nsMsgXFGroupThread()
{
}

nsMsgXFGroupThread::~nsMsgXFGroupThread()
{
}

NS_IMETHODIMP nsMsgXFGroupThread::GetNumChildren(uint32_t *aNumChildren)
{
  NS_ENSURE_ARG_POINTER(aNumChildren);
  *aNumChildren = m_folders.Count();
  return NS_OK;
}

NS_IMETHODIMP nsMsgXFGroupThread::GetChildHdrAt(int32_t aIndex, nsIMsgDBHdr **aResult)
{
  if (aIndex < 0 || aIndex >= m_folders.Count())
    return NS_MSG_MESSAGE_NOT_FOUND;
  return m_folders.ObjectAt(aIndex)->GetMessageHeader(m_keys[aIndex], aResult);
}

NS_IMETHODIMP nsMsgXFGroupThread::GetChildKeyAt(int32_t aIndex, nsMsgKey *aResult)
{
  NS_ASSERTION(false, "shouldn't call this");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgXFGroupThread::RemoveChildAt(int32_t aIndex)
{
  nsMsgGroupThread::RemoveChildAt(aIndex);
  m_folders.RemoveObjectAt(aIndex);
  return NS_OK;
}

void nsMsgXFGroupThread::InsertMsgHdrAt(nsMsgViewIndex index, nsIMsgDBHdr *hdr)
{
  nsCOMPtr<nsIMsgFolder> folder;
  hdr->GetFolder(getter_AddRefs(folder));
  m_folders.InsertObjectAt(folder, index);
  nsMsgGroupThread::InsertMsgHdrAt(index, hdr);
}

void nsMsgXFGroupThread::SetMsgHdrAt(nsMsgViewIndex index, nsIMsgDBHdr *hdr)
{
  nsCOMPtr<nsIMsgFolder> folder;
  hdr->GetFolder(getter_AddRefs(folder));
  m_folders.ReplaceObjectAt(folder, index);
  nsMsgGroupThread::SetMsgHdrAt(index, hdr);
}

nsMsgViewIndex nsMsgXFGroupThread::FindMsgHdr(nsIMsgDBHdr *hdr)
{
  nsMsgKey msgKey;
  hdr->GetMessageKey(&msgKey);
  nsCOMPtr<nsIMsgFolder> folder;
  hdr->GetFolder(getter_AddRefs(folder));
  uint32_t index = 0;
  while (true) {
    index = m_keys.IndexOf(msgKey, index);
    if (index == m_keys.NoIndex || m_folders[index] == folder)
      break;
    index++;
  }
  return (nsMsgViewIndex)index;
}

nsMsgViewIndex nsMsgXFGroupThread::AddMsgHdrInDateOrder(nsIMsgDBHdr *child, nsMsgDBView *view)
{
  nsMsgViewIndex insertIndex = nsMsgGroupThread::AddMsgHdrInDateOrder(child, view);
  nsCOMPtr<nsIMsgFolder> folder;
  child->GetFolder(getter_AddRefs(folder));
  m_folders.InsertObjectAt(folder, insertIndex);
  return insertIndex;
}
nsMsgViewIndex 
nsMsgXFGroupThread::GetInsertIndexFromView(nsMsgDBView *view, 
                                          nsIMsgDBHdr *child, 
                                          nsMsgViewSortOrderValue threadSortOrder)
{
   return view->GetInsertIndexHelper(child, m_keys, &m_folders, threadSortOrder, nsMsgViewSortType::byDate);
}

