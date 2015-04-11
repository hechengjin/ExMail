/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 
#ifdef MOZ_LOGGING
// sorry, this has to be before the pre-compiled header
#define FORCE_PR_LOG /* Allow logging in the release build */
#endif
#include "nsAutoSyncManager.h"
#include "nsAutoSyncState.h"
#include "nsIIdleService.h"
#include "nsImapMailFolder.h"
#include "nsMsgImapCID.h"
#include "nsIObserverService.h"
#include "nsIMsgMailNewsUrl.h"
#include "nsIMsgAccountManager.h"
#include "nsIMsgIncomingServer.h"
#include "nsIMsgMailSession.h"
#include "nsMsgFolderFlags.h"
#include "nsImapIncomingServer.h"
#include "nsMsgUtils.h"
#include "nsIIOService.h"
#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"
#include "mozilla/Services.h"

NS_IMPL_ISUPPORTS1(nsDefaultAutoSyncMsgStrategy, nsIAutoSyncMsgStrategy)

const char* kAppIdleNotification = "mail:appIdle";
const char* kStartupDoneNotification = "mail-startup-done";
PRLogModuleInfo *gAutoSyncLog;

// recommended size of each group of messages per download
static const uint32_t kDefaultGroupSize = 50U*1024U /* 50K */;

nsDefaultAutoSyncMsgStrategy::nsDefaultAutoSyncMsgStrategy()
{
}

nsDefaultAutoSyncMsgStrategy::~nsDefaultAutoSyncMsgStrategy()
{
}

NS_IMETHODIMP nsDefaultAutoSyncMsgStrategy::Sort(nsIMsgFolder *aFolder, 
  nsIMsgDBHdr *aMsgHdr1, nsIMsgDBHdr *aMsgHdr2, nsAutoSyncStrategyDecisionType *aDecision)
{
  NS_ENSURE_ARG_POINTER(aDecision);

  uint32_t msgSize1 = 0, msgSize2 = 0;
  PRTime msgDate1 = 0, msgDate2 = 0;
  
  if (!aMsgHdr1 || !aMsgHdr2)
  {
    *aDecision = nsAutoSyncStrategyDecisions::Same;
    return NS_OK;
  }

  aMsgHdr1->GetMessageSize(&msgSize1);
  aMsgHdr1->GetDate(&msgDate1);
  
  aMsgHdr2->GetMessageSize(&msgSize2);
  aMsgHdr2->GetDate(&msgDate2);
  
  //Special case: if message size is larger than a 
  // certain size, then place it to the bottom of the q
  if (msgSize2 > kFirstPassMessageSize && msgSize1 > kFirstPassMessageSize)
    *aDecision = msgSize2 > msgSize1 ? 
        nsAutoSyncStrategyDecisions::Lower : nsAutoSyncStrategyDecisions::Higher;
  else if (msgSize2 > kFirstPassMessageSize)
    *aDecision = nsAutoSyncStrategyDecisions::Lower;
  else if (msgSize1 > kFirstPassMessageSize)
    *aDecision = nsAutoSyncStrategyDecisions::Higher;
  else
  {
    // Most recent and smallest first
    if (msgDate1 < msgDate2)
      *aDecision = nsAutoSyncStrategyDecisions::Higher;
    else if (msgDate1 > msgDate2)
      *aDecision = nsAutoSyncStrategyDecisions::Lower;
    else 
    {
      if (msgSize1 > msgSize2)
        *aDecision = nsAutoSyncStrategyDecisions::Higher;
      else if (msgSize1 < msgSize2)
        *aDecision = nsAutoSyncStrategyDecisions::Lower;
      else
        *aDecision = nsAutoSyncStrategyDecisions::Same;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP nsDefaultAutoSyncMsgStrategy::IsExcluded(nsIMsgFolder *aFolder, 
  nsIMsgDBHdr *aMsgHdr, bool *aDecision)
{
  NS_ENSURE_ARG_POINTER(aDecision);
  NS_ENSURE_ARG_POINTER(aMsgHdr);
  NS_ENSURE_ARG_POINTER(aFolder);
  nsCOMPtr<nsIMsgIncomingServer> server;

  nsresult rv = aFolder->GetServer(getter_AddRefs(server));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIImapIncomingServer> imapServer(do_QueryInterface(server, &rv));
  int32_t offlineMsgAgeLimit = -1;
  imapServer->GetAutoSyncMaxAgeDays(&offlineMsgAgeLimit);
  NS_ENSURE_SUCCESS(rv, rv);
  PRTime msgDate;
  aMsgHdr->GetDate(&msgDate);
  *aDecision = offlineMsgAgeLimit > 0 &&
    msgDate < MsgConvertAgeInDaysToCutoffDate(offlineMsgAgeLimit);
  return NS_OK;
}

NS_IMPL_ISUPPORTS1(nsDefaultAutoSyncFolderStrategy, nsIAutoSyncFolderStrategy)

nsDefaultAutoSyncFolderStrategy::nsDefaultAutoSyncFolderStrategy()
{
}

nsDefaultAutoSyncFolderStrategy::~nsDefaultAutoSyncFolderStrategy()
{
}

NS_IMETHODIMP nsDefaultAutoSyncFolderStrategy::Sort(nsIMsgFolder *aFolderA, 
  nsIMsgFolder *aFolderB, nsAutoSyncStrategyDecisionType *aDecision)
{
  NS_ENSURE_ARG_POINTER(aDecision);

  if (!aFolderA || !aFolderB)
  {
    *aDecision = nsAutoSyncStrategyDecisions::Same;
    return NS_OK;
  }
  
  bool isInbox1, isInbox2, isDrafts1, isDrafts2, isTrash1, isTrash2;
  aFolderA->GetFlag(nsMsgFolderFlags::Inbox, &isInbox1);
  aFolderB->GetFlag(nsMsgFolderFlags::Inbox, &isInbox2);
  //
  aFolderA->GetFlag(nsMsgFolderFlags::Drafts, &isDrafts1);
  aFolderB->GetFlag(nsMsgFolderFlags::Drafts, &isDrafts2);
  //
  aFolderA->GetFlag(nsMsgFolderFlags::Trash, &isTrash1);
  aFolderB->GetFlag(nsMsgFolderFlags::Trash, &isTrash2);
  
  //Follow this order;
  // INBOX > DRAFTS > SUBFOLDERS > TRASH

  // test whether the folder is opened by the user.
  // we give high priority to the folders explicitly opened by 
  // the user.
  nsresult rv;
  bool folderAOpen = false;
  bool folderBOpen = false;
  nsCOMPtr<nsIMsgMailSession> session =
           do_GetService(NS_MSGMAILSESSION_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv) && session) 
  {
    session->IsFolderOpenInWindow(aFolderA, &folderAOpen);
    session->IsFolderOpenInWindow(aFolderB, &folderBOpen);
  }

  if (folderAOpen == folderBOpen)
  {
    // if both of them or none of them are opened by the user 
    // make your decision based on the folder type
    if (isInbox2 || (isDrafts2 && !isInbox1) || isTrash1)
      *aDecision = nsAutoSyncStrategyDecisions::Higher;
    else if (isInbox1 || (isDrafts1 && !isDrafts2) || isTrash2)
      *aDecision = nsAutoSyncStrategyDecisions::Lower;
    else
      *aDecision = nsAutoSyncStrategyDecisions::Same;
  }
  else
  {
    // otherwise give higher priority to opened one
    *aDecision = folderBOpen ? nsAutoSyncStrategyDecisions::Higher :
                               nsAutoSyncStrategyDecisions::Lower;
  }
    
  return NS_OK;
}

NS_IMETHODIMP 
nsDefaultAutoSyncFolderStrategy::IsExcluded(nsIMsgFolder *aFolder, bool *aDecision)
{
  NS_ENSURE_ARG_POINTER(aDecision);
  NS_ENSURE_ARG_POINTER(aFolder);
  uint32_t folderFlags;
  aFolder->GetFlags(&folderFlags);
  // exclude saved search
  *aDecision = (folderFlags & nsMsgFolderFlags::Virtual);
  if (!*aDecision)
  {
    // Exclude orphans
    nsCOMPtr<nsIMsgFolder> parent;
    aFolder->GetParent(getter_AddRefs(parent));
    if (!parent)
      *aDecision = true;
  }
  return NS_OK;
}

#define NOTIFY_LISTENERS_STATIC(obj_, propertyfunc_, params_) \
  PR_BEGIN_MACRO \
  nsTObserverArray<nsCOMPtr<nsIAutoSyncMgrListener> >::ForwardIterator iter(obj_->mListeners); \
  nsCOMPtr<nsIAutoSyncMgrListener> listener; \
  while (iter.HasMore()) { \
    listener = iter.GetNext(); \
    listener->propertyfunc_ params_; \
  } \
  PR_END_MACRO

#define NOTIFY_LISTENERS(propertyfunc_, params_) \
  NOTIFY_LISTENERS_STATIC(this, propertyfunc_, params_)

nsAutoSyncManager::nsAutoSyncManager()
{
  mGroupSize = kDefaultGroupSize;

  mIdleState = notIdle;
  mStartupDone = false;
  mDownloadModel = dmChained;
  mUpdateState = completed;
  mPaused = false;

  nsresult rv;
  mIdleService = do_GetService("@mozilla.org/widget/idleservice;1", &rv);
  if (mIdleService)
    mIdleService->AddIdleObserver(this, kIdleTimeInSec);

  // Observe xpcom-shutdown event and app-idle changes
  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();

  rv = observerService->AddObserver(this,
                                    NS_XPCOM_SHUTDOWN_OBSERVER_ID,
                                    false);
  observerService->AddObserver(this, kAppIdleNotification, false);
  observerService->AddObserver(this, NS_IOSERVICE_OFFLINE_STATUS_TOPIC, false);
  observerService->AddObserver(this, NS_IOSERVICE_GOING_OFFLINE_TOPIC, false);
  observerService->AddObserver(this, kStartupDoneNotification, false);
  gAutoSyncLog = PR_NewLogModule("ImapAutoSync");
}

nsAutoSyncManager::~nsAutoSyncManager()
{
}

void nsAutoSyncManager::InitTimer()
{
  if (!mTimer)
  {
    nsresult rv;
    mTimer = do_CreateInstance(NS_TIMER_CONTRACTID, &rv);
    NS_ASSERTION(NS_SUCCEEDED(rv), "failed to create timer in nsAutoSyncManager");

    mTimer->InitWithFuncCallback(TimerCallback, (void *) this, 
                                 kTimerIntervalInMs, nsITimer::TYPE_REPEATING_SLACK);
  }
}

void nsAutoSyncManager::StopTimer()
{
  if (mTimer)
  {
    mTimer->Cancel();
    mTimer = nullptr;
  }
}

void nsAutoSyncManager::StartTimerIfNeeded()
{
  if ((mUpdateQ.Count() > 0 || mDiscoveryQ.Count() > 0) && !mTimer)
    InitTimer();
}

void nsAutoSyncManager::TimerCallback(nsITimer *aTimer, void *aClosure)
{
  if (!aClosure)
    return;
  
  nsAutoSyncManager *autoSyncMgr = static_cast<nsAutoSyncManager*>(aClosure);
  if (autoSyncMgr->GetIdleState() == notIdle ||
    (autoSyncMgr->mDiscoveryQ.Count() <= 0 && autoSyncMgr->mUpdateQ.Count() <= 0))
  {
    // Idle will create a new timer automatically if discovery Q or update Q is not empty
    autoSyncMgr->StopTimer();
  }

  // process folders within the discovery queue
  if (autoSyncMgr->mDiscoveryQ.Count() > 0)
  {
    nsCOMPtr<nsIAutoSyncState> autoSyncStateObj(autoSyncMgr->mDiscoveryQ[0]);
    if (autoSyncStateObj)
    {
      uint32_t leftToProcess;
      nsresult rv = autoSyncStateObj->ProcessExistingHeaders(kNumberOfHeadersToProcess, &leftToProcess);

      nsCOMPtr<nsIMsgFolder> folder;
      autoSyncStateObj->GetOwnerFolder(getter_AddRefs(folder));
      if (folder)
        NOTIFY_LISTENERS_STATIC(autoSyncMgr, OnDiscoveryQProcessed, (folder, kNumberOfHeadersToProcess, leftToProcess));

      if (NS_SUCCEEDED(rv) && 0 == leftToProcess)
      {
        autoSyncMgr->mDiscoveryQ.RemoveObjectAt(0);
        if (folder)
          NOTIFY_LISTENERS_STATIC(autoSyncMgr, OnFolderRemovedFromQ, (nsIAutoSyncMgrListener::DiscoveryQueue, folder));
      }
    }
  }

  if (autoSyncMgr->mUpdateQ.Count() > 0)
  {
    if (autoSyncMgr->mUpdateState == completed)
    {
      nsCOMPtr<nsIAutoSyncState> autoSyncStateObj(autoSyncMgr->mUpdateQ[0]);
      if (autoSyncStateObj)
      {
        int32_t state;
        nsresult rv = autoSyncStateObj->GetState(&state);
        if (NS_SUCCEEDED(rv) && (state == nsAutoSyncState::stCompletedIdle ||
                                 state == nsAutoSyncState::stUpdateNeeded))
        {
          nsCOMPtr<nsIMsgFolder> folder; 
          autoSyncStateObj->GetOwnerFolder(getter_AddRefs(folder));
          if (folder)
          {
            nsCOMPtr <nsIMsgImapMailFolder> imapFolder = do_QueryInterface(folder, &rv);
            NS_ENSURE_SUCCESS(rv,);
            rv = imapFolder->InitiateAutoSync(autoSyncMgr);
            if (NS_SUCCEEDED(rv))
            {
              autoSyncMgr->mUpdateState = initiated;
              NOTIFY_LISTENERS_STATIC(autoSyncMgr, OnAutoSyncInitiated, (folder));
            }
          }
        }
      } 
    }
    // if initiation is not successful for some reason, or 
    // if there is an on going download for this folder, 
    // remove it from q and continue with the next one  
    if (autoSyncMgr->mUpdateState != initiated)
    {
      nsCOMPtr<nsIMsgFolder> folder;
      autoSyncMgr->mUpdateQ[0]->GetOwnerFolder(getter_AddRefs(folder));
      
      autoSyncMgr->mUpdateQ.RemoveObjectAt(0);
      
      if (folder)
        NOTIFY_LISTENERS_STATIC(autoSyncMgr, OnFolderRemovedFromQ, (nsIAutoSyncMgrListener::UpdateQueue, folder));
    }
      
  }//endif

}

/**
 * Populates aChainedQ with the auto-sync state objects that are not owned by 
 * the same imap server. 
 * Assumes that aChainedQ initially empty.
 */
void nsAutoSyncManager::ChainFoldersInQ(const nsCOMArray<nsIAutoSyncState> &aQueue,
      nsCOMArray<nsIAutoSyncState> &aChainedQ)
{
  if (aQueue.Count() > 0)
    aChainedQ.AppendObject(aQueue[0]);
  
  int32_t pqElemCount = aQueue.Count();
  for (int32_t pqidx = 1; pqidx < pqElemCount; pqidx++)
  {
    bool chained = false;
    int32_t needToBeReplacedWith = -1;
    int32_t elemCount = aChainedQ.Count();
    for (int32_t idx = 0; idx < elemCount; idx++)
    {
      bool isSibling;
      nsresult rv = aChainedQ[idx]->IsSibling(aQueue[pqidx], &isSibling);
      
      if (NS_SUCCEEDED(rv) && isSibling)
      {
        // this prevent us to overwrite a lower priority sibling in
        // download-in-progress state with a higher priority one. 
        // we have to wait until its download is completed before 
        // switching to new one. 
        int32_t state;
        aQueue[pqidx]->GetState(&state);
        if (aQueue[pqidx] != aChainedQ[idx] && 
            state == nsAutoSyncState::stDownloadInProgress)
          needToBeReplacedWith = idx;
        else
          chained = true;
          
        break;
      }
    }//endfor
    
    if (needToBeReplacedWith > -1)
      aChainedQ.ReplaceObjectAt(aQueue[pqidx], needToBeReplacedWith);
    else if (!chained)
      aChainedQ.AppendObject(aQueue[pqidx]);
      
  }//endfor
}

/**
 * Searches the given queue for another folder owned by the same imap server.
 */
nsIAutoSyncState* 
nsAutoSyncManager::SearchQForSibling(const nsCOMArray<nsIAutoSyncState> &aQueue, 
                          nsIAutoSyncState *aAutoSyncStateObj, int32_t aStartIdx, int32_t *aIndex)
{
  if (aIndex)
    *aIndex = -1;
  
  if (aAutoSyncStateObj)
  {
    bool isSibling;
    int32_t elemCount = aQueue.Count();
    for (int32_t idx = aStartIdx; idx < elemCount; idx++)
    {
      nsresult rv = aAutoSyncStateObj->IsSibling(aQueue[idx], &isSibling);
      
      if (NS_SUCCEEDED(rv) && isSibling && aAutoSyncStateObj != aQueue[idx])
      {
        if (aIndex) 
          *aIndex = idx;
        
        return aQueue[idx];
      }
    }
  }
  return nullptr;  
}

/**
 * Searches for the next folder owned by the same imap server in the given queue,
 * starting from the index of the given folder.
 */
nsIAutoSyncState* 
nsAutoSyncManager::GetNextSibling(const nsCOMArray<nsIAutoSyncState> &aQueue, 
                                          nsIAutoSyncState *aAutoSyncStateObj, int32_t *aIndex)
{ 

  if (aIndex)
    *aIndex = -1;
  
  if (aAutoSyncStateObj)
  {
    bool located = false;
    bool isSibling;
    int32_t elemCount = aQueue.Count();
    for (int32_t idx = 0; idx < elemCount; idx++)
    {
      if (!located)
      {
        located = (aAutoSyncStateObj == aQueue[idx]);
        continue;
      }
      
      nsresult rv = aAutoSyncStateObj->IsSibling(aQueue[idx], &isSibling);
      if (NS_SUCCEEDED(rv) && isSibling)
      {
        if (aIndex) 
          *aIndex = idx;
        
        return aQueue[idx];
      }
    }
  }
  return nullptr;  
}

/** 
 * Checks whether there is another folder in the given q that is owned 
 * by the same imap server or not.
 *
 * @param aQueue the queue that will be searched for a sibling
 * @param aAutoSyncStateObj the auto-sync state object that we are looking
 *                          a sibling for
 * @param aState the state of the sibling. -1 means "any state"
 * @param aIndex [out] the index of the found sibling, if it is provided by the
 *               caller (not null)
 * @return true if found, false otherwise
 */
bool nsAutoSyncManager::DoesQContainAnySiblingOf(const nsCOMArray<nsIAutoSyncState> &aQueue, 
                                                   nsIAutoSyncState *aAutoSyncStateObj,
                                                   const int32_t aState, int32_t *aIndex)
{
  if (aState == -1)
    return (nullptr != SearchQForSibling(aQueue, aAutoSyncStateObj, 0, aIndex));
    
  int32_t offset = 0;
  nsIAutoSyncState *autoSyncState;
  while ((autoSyncState = SearchQForSibling(aQueue, aAutoSyncStateObj, offset, &offset)))
  {
    int32_t state;
    nsresult rv = autoSyncState->GetState(&state);
    if (NS_SUCCEEDED(rv) && aState == state)
      break;
    else
      offset++;
  }
  if (aIndex)
    *aIndex = offset;
    
  return (nullptr != autoSyncState);
}

/**
 * Searches the given queue for the highest priority folder owned by the
 * same imap server.
 */
nsIAutoSyncState* 
nsAutoSyncManager::GetHighestPrioSibling(const nsCOMArray<nsIAutoSyncState> &aQueue, 
                                      nsIAutoSyncState *aAutoSyncStateObj, int32_t *aIndex)
{
  return SearchQForSibling(aQueue, aAutoSyncStateObj, 0, aIndex);
}

// to chain update folder actions
NS_IMETHODIMP nsAutoSyncManager::OnStartRunningUrl(nsIURI* aUrl)
{
  return NS_OK;
}


NS_IMETHODIMP nsAutoSyncManager::OnStopRunningUrl(nsIURI* aUrl, nsresult aExitCode)
{
  mUpdateState = completed;
  if (mUpdateQ.Count() > 0)
    mUpdateQ.RemoveObjectAt(0);

  return aExitCode;
}

NS_IMETHODIMP nsAutoSyncManager::Pause()
{
  StopTimer();
  mPaused = true;
  PR_LOG(gAutoSyncLog, PR_LOG_DEBUG, ("autosync paused\n"));
  return NS_OK;
}

NS_IMETHODIMP nsAutoSyncManager::Resume()
{
  mPaused = false;
  StartTimerIfNeeded();
  PR_LOG(gAutoSyncLog, PR_LOG_DEBUG, ("autosync resumed\n"));
  return NS_OK;
}

NS_IMETHODIMP nsAutoSyncManager::Observe(nsISupports*, const char *aTopic, const PRUnichar *aSomeData)
{
  if (!PL_strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID))
  {
    nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
    if (observerService)
    {
      observerService->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
      observerService->RemoveObserver(this, kAppIdleNotification);
      observerService->RemoveObserver(this, NS_IOSERVICE_OFFLINE_STATUS_TOPIC);
      observerService->RemoveObserver(this, NS_IOSERVICE_GOING_OFFLINE_TOPIC);
      observerService->RemoveObserver(this, kStartupDoneNotification);
    }

    // cancel and release the timer
    if (mTimer)
    {
       mTimer->Cancel();
       mTimer = nullptr;
    }
    // unsubscribe from idle service
    if (mIdleService)
       mIdleService->RemoveIdleObserver(this, kIdleTimeInSec);

    return NS_OK;
  }
  else if (!PL_strcmp(aTopic, kStartupDoneNotification))
  {
    mStartupDone = true; 
  }
  else if (!PL_strcmp(aTopic, kAppIdleNotification))
  {
    if (nsDependentString(aSomeData).EqualsLiteral("idle"))
    {
      IdleState prevIdleState = GetIdleState();

      // we were already idle (either system or app), so
      // just remember that we're app idle and return.
      SetIdleState(appIdle);
      if (prevIdleState != notIdle)
        return NS_OK;

       return StartIdleProcessing();
     }
     // we're back from appIdle - if already notIdle, just return;
     else if (GetIdleState() == notIdle)
       return NS_OK;

    SetIdleState(notIdle);
    NOTIFY_LISTENERS(OnStateChanged, (false));
    return NS_OK;
  }
  else if (!PL_strcmp(aTopic, NS_IOSERVICE_OFFLINE_STATUS_TOPIC))
  {
    if (nsDependentString(aSomeData).EqualsLiteral(NS_IOSERVICE_ONLINE))
      Resume();
  }
  else if (!PL_strcmp(aTopic, NS_IOSERVICE_GOING_OFFLINE_TOPIC))
  {
    Pause();
  }
  // we're back from system idle
  else if (!PL_strcmp(aTopic, "back"))
  {
    // if we're app idle when we get back from system idle, we ignore
    // it, since we'll keep doing our idle stuff.
    if (GetIdleState() != appIdle)
    {
      SetIdleState(notIdle);
      NOTIFY_LISTENERS(OnStateChanged, (false));
    }
    return NS_OK;
  }
  else // we've gone system idle
  {
    // Check if we were already idle. We may have gotten
    // multiple system idle notificatons. In that case,
    // just remember that we're systemIdle and return;
    if (GetIdleState() != notIdle)
      return NS_OK;

    // we might want to remember if we were app idle, because
    // coming back from system idle while app idle shouldn't stop
    // app indexing. But I think it's OK for now just leave ourselves
    // in appIdle state.
    if (GetIdleState() != appIdle)
      SetIdleState(systemIdle);
    if (WeAreOffline())
      return NS_OK;
    return StartIdleProcessing();
  }
  return NS_OK;
}

nsresult nsAutoSyncManager::StartIdleProcessing()
{
  if (mPaused)
    return NS_OK;
    
  StartTimerIfNeeded();
  
  // Ignore idle events sent during the startup
  if (!mStartupDone)
    return NS_OK;
    
  // notify listeners that auto-sync is running
  NOTIFY_LISTENERS(OnStateChanged, (true));
    
  nsCOMArray<nsIAutoSyncState> chainedQ;
  nsCOMArray<nsIAutoSyncState> *queue = &mPriorityQ;
  if (mDownloadModel == dmChained) 
  {
    ChainFoldersInQ(mPriorityQ, chainedQ);
    queue = &chainedQ;
  }
  
  // to store the folders that should be removed from the priority
  // queue at the end of the iteration.
  nsCOMArray<nsIAutoSyncState> foldersToBeRemoved;
  
  // process folders in the priority queue 
  int32_t elemCount = queue->Count();
  for (int32_t idx = 0; idx < elemCount; idx++)
  {
    nsCOMPtr<nsIAutoSyncState> autoSyncStateObj((*queue)[idx]);
    if (!autoSyncStateObj)
      continue;
    
    int32_t state;
    autoSyncStateObj->GetState(&state);
    
    //TODO: Test cached-connection availability in parallel mode
    // and do not exceed (cached-connection count - 1)
    
    if (state != nsAutoSyncState::stReadyToDownload)
      continue;
    
    nsresult rv = DownloadMessagesForOffline(autoSyncStateObj);
    if (NS_FAILED(rv))
    {
      // special case: this folder does not have any message to download
      // (see bug 457342), remove it explicitly from the queue when iteration
      // is over.
      // Note that in normal execution flow, folders are removed from priority
      // queue only in OnDownloadCompleted when all messages are downloaded
      // successfully. This is the only place we change this flow.
      if (NS_ERROR_NOT_AVAILABLE == rv)
        foldersToBeRemoved.AppendObject(autoSyncStateObj);
      
      HandleDownloadErrorFor(autoSyncStateObj, rv);
    }// endif
  }//endfor
  
  // remove folders with no pending messages from the priority queue
  elemCount = foldersToBeRemoved.Count();
  for (int32_t idx = 0; idx < elemCount; idx++)
  {
    nsCOMPtr<nsIAutoSyncState> autoSyncStateObj(foldersToBeRemoved[idx]);
    if (!autoSyncStateObj)
      continue;
    
    nsCOMPtr<nsIMsgFolder> folder;
    autoSyncStateObj->GetOwnerFolder(getter_AddRefs(folder));
    if (folder)
      NOTIFY_LISTENERS(OnDownloadCompleted, (folder));

    autoSyncStateObj->SetState(nsAutoSyncState::stCompletedIdle);

    if (mPriorityQ.RemoveObject(autoSyncStateObj))
      NOTIFY_LISTENERS(OnFolderRemovedFromQ,
                      (nsIAutoSyncMgrListener::PriorityQueue, folder));
  }
    
  return AutoUpdateFolders();
}

/**
 * Updates offline imap folders that are not synchronized recently. This is
 * called whenever we're idle.
 */
nsresult nsAutoSyncManager::AutoUpdateFolders()
{
  nsresult rv;

  // iterate through each imap account and update offline folders automatically

  nsCOMPtr<nsIMsgAccountManager> accountManager = do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr<nsISupportsArray> accounts;
  rv = accountManager->GetAccounts(getter_AddRefs(accounts));
  NS_ENSURE_SUCCESS(rv,rv);

  uint32_t accountCount;
  accounts->Count(&accountCount);

  for (uint32_t i = 0; i < accountCount; ++i) 
  {
    nsCOMPtr<nsIMsgAccount> account(do_QueryElementAt(accounts, i, &rv));
    if (!account)
      continue;

    nsCOMPtr<nsIMsgIncomingServer> incomingServer;
    rv = account->GetIncomingServer(getter_AddRefs(incomingServer));
    if (!incomingServer)
      continue;

    nsCString type;
    rv = incomingServer->GetType(type);

    if (!type.EqualsLiteral("imap"))
      continue;

    // if we haven't logged onto this server yet, then skip this server.
    bool passwordRequired;
    incomingServer->GetServerRequiresPasswordForBiff(&passwordRequired);
    if (passwordRequired)
      continue;

    nsCOMPtr<nsIMsgFolder> rootFolder;
    nsCOMPtr<nsISupportsArray> allDescendents;

    rv = incomingServer->GetRootFolder(getter_AddRefs(rootFolder));
    if (rootFolder)
    {
      allDescendents = do_CreateInstance(NS_SUPPORTSARRAY_CONTRACTID, &rv);
      if (NS_FAILED(rv))
        continue;

      rv = rootFolder->ListDescendents(allDescendents);
      if (!allDescendents)
        continue;

      uint32_t cnt = 0;
      rv = allDescendents->Count(&cnt);
      if (NS_FAILED(rv))
        continue;

      for (uint32_t i = 0; i < cnt; i++)
      {
        nsCOMPtr<nsIMsgFolder> folder(do_QueryElementAt(allDescendents, i, &rv));
        if (NS_FAILED(rv))
          continue;

        uint32_t folderFlags;
        rv = folder->GetFlags(&folderFlags);
        // Skip this folder if not offline or is a saved search or is no select.
        if (NS_FAILED(rv) || !(folderFlags & nsMsgFolderFlags::Offline) ||
            folderFlags & (nsMsgFolderFlags::Virtual |
                           nsMsgFolderFlags::ImapNoselect))
          continue;

        nsCOMPtr<nsIMsgImapMailFolder> imapFolder = do_QueryInterface(folder, &rv);
        if (NS_FAILED(rv))
          continue;

        nsCOMPtr<nsIImapIncomingServer> imapServer;
        rv = imapFolder->GetImapIncomingServer(getter_AddRefs(imapServer));
        if (imapServer)
        {
          bool autoSyncOfflineStores = false;
          rv = imapServer->GetAutoSyncOfflineStores(&autoSyncOfflineStores);

          // skip if AutoSyncOfflineStores pref is not set for this folder
          if (NS_FAILED(rv) || !autoSyncOfflineStores)
            continue;
        }

        nsCOMPtr<nsIAutoSyncState> autoSyncState;
        rv = imapFolder->GetAutoSyncStateObj(getter_AddRefs(autoSyncState));
        NS_ASSERTION(autoSyncState, "*** nsAutoSyncState shouldn't be NULL, check owner folder");

        // shouldn't happen but let's be defensive here
        if (!autoSyncState)
          continue;

        int32_t state;
        rv = autoSyncState->GetState(&state);

        if (NS_SUCCEEDED(rv) && nsAutoSyncState::stCompletedIdle == state)
        {
          // ensure that we wait for at least nsMsgIncomingServer::BiffMinutes between
          // each update of the same folder
          PRTime lastUpdateTime;
          rv = autoSyncState->GetLastUpdateTime(&lastUpdateTime);
          PRTime span = GetUpdateIntervalFor(autoSyncState) * (PR_USEC_PER_SEC * 60UL);
          if ( NS_SUCCEEDED(rv) && ((lastUpdateTime + span) < PR_Now()) )
          {          
            if (mUpdateQ.IndexOf(autoSyncState) == -1)
            {
              mUpdateQ.AppendObject(autoSyncState);
              if (folder)
                NOTIFY_LISTENERS(OnFolderAddedIntoQ, (nsIAutoSyncMgrListener::UpdateQueue, folder));
            }
          }
        }

        // check last sync time
        PRTime lastSyncTime;
        rv = autoSyncState->GetLastSyncTime(&lastSyncTime);
        if ( NS_SUCCEEDED(rv) && ((lastSyncTime + kAutoSyncFreq) < PR_Now()) )
        {
          // add this folder into discovery queue to process existing headers
          // and discover messages not downloaded yet
          if (mDiscoveryQ.IndexOf(autoSyncState) == -1)
          {
            mDiscoveryQ.AppendObject(autoSyncState);
            if (folder)
              NOTIFY_LISTENERS(OnFolderAddedIntoQ, (nsIAutoSyncMgrListener::DiscoveryQueue, folder));
          }
        }
      }//endfor
    }//endif
  }//endfor

  // lazily create the timer if there is something to process in the queue
  // when timer is done, it will self destruct
  StartTimerIfNeeded();
  
  return rv;
}

/**
 * Places the given folder into the priority queue based on active
 * strategy function.
 */
void nsAutoSyncManager::ScheduleFolderForOfflineDownload(nsIAutoSyncState *aAutoSyncStateObj)
{
  if (aAutoSyncStateObj && (mPriorityQ.IndexOf(aAutoSyncStateObj) == -1))
  {
    nsCOMPtr<nsIAutoSyncFolderStrategy> folStrategy;
    GetFolderStrategy(getter_AddRefs(folStrategy));

    if (mPriorityQ.Count() <= 0)
    {
      // make sure that we don't insert a folder excluded by the given strategy
      nsCOMPtr<nsIMsgFolder> folder;
      aAutoSyncStateObj->GetOwnerFolder(getter_AddRefs(folder));
      if (folder)
      {
        bool excluded = false;
        if (folStrategy)
          folStrategy->IsExcluded(folder, &excluded);

        if (!excluded)
        {
          mPriorityQ.AppendObject(aAutoSyncStateObj); // insert into the first spot
          NOTIFY_LISTENERS(OnFolderAddedIntoQ, (nsIAutoSyncMgrListener::PriorityQueue, folder));
        }
      }
    }
    else
    {
      // find the right spot for the given folder      
      uint32_t qidx = mPriorityQ.Count();
      while (qidx > 0) 
      {
        --qidx;

        nsCOMPtr<nsIMsgFolder> folderA, folderB;
        mPriorityQ[qidx]->GetOwnerFolder(getter_AddRefs(folderA));
        aAutoSyncStateObj->GetOwnerFolder(getter_AddRefs(folderB));
        
        bool excluded = false;
        if (folderB && folStrategy)
          folStrategy->IsExcluded(folderB, &excluded);

        if (excluded)
          break;

        nsAutoSyncStrategyDecisionType decision = nsAutoSyncStrategyDecisions::Same;
        if (folderA && folderB && folStrategy)
          folStrategy->Sort(folderA, folderB, &decision);

        if (decision == nsAutoSyncStrategyDecisions::Higher && 0 == qidx)
          mPriorityQ.InsertObjectAt(aAutoSyncStateObj, 0);
        else if (decision == nsAutoSyncStrategyDecisions::Higher)
          continue;
        else if (decision == nsAutoSyncStrategyDecisions::Lower)
          mPriorityQ.InsertObjectAt(aAutoSyncStateObj, qidx+1);
        else //  decision == nsAutoSyncStrategyDecisions::Same
          mPriorityQ.InsertObjectAt(aAutoSyncStateObj, qidx);

        NOTIFY_LISTENERS(OnFolderAddedIntoQ, (nsIAutoSyncMgrListener::PriorityQueue, folderB));
        break;
      }//endwhile
    }
  }//endif
}

/**
 * Zero aSizeLimit means no limit 
 */
nsresult nsAutoSyncManager::DownloadMessagesForOffline(nsIAutoSyncState *aAutoSyncStateObj, uint32_t aSizeLimit)
{
  if (!aAutoSyncStateObj)
    return NS_ERROR_INVALID_ARG;

  int32_t count;
  nsresult rv = aAutoSyncStateObj->GetPendingMessageCount(&count);
  NS_ENSURE_SUCCESS(rv, rv);

  // special case: no more message to download for this folder:
  // see HandleDownloadErrorFor for recovery policy
  if (!count)
    return NS_ERROR_NOT_AVAILABLE;

  nsCOMPtr<nsIMutableArray> messagesToDownload;
  uint32_t totalSize = 0;
  rv = aAutoSyncStateObj->GetNextGroupOfMessages(mGroupSize, &totalSize, getter_AddRefs(messagesToDownload));
  NS_ENSURE_SUCCESS(rv,rv);

  // there are pending messages but the cumulative size is zero:
  // treat as special case.
  // Note that although it shouldn't happen, we know that sometimes
  // imap servers manifest messages as zero length. By returning
  // NS_ERROR_NOT_AVAILABLE we cause this folder to be removed from
  // the priority queue temporarily (until the next idle or next update)
  // in an effort to prevent it blocking other folders of the same account
  // being synced.
  if (!totalSize)
    return NS_ERROR_NOT_AVAILABLE;

  // ensure that we don't exceed the given size limit for this particular group
  if (aSizeLimit && aSizeLimit < totalSize)
    return NS_ERROR_FAILURE;

  uint32_t length;
  rv = messagesToDownload->GetLength(&length);
  if (NS_SUCCEEDED(rv) && length > 0)
  {
    rv = aAutoSyncStateObj->DownloadMessagesForOffline(messagesToDownload);

    int32_t totalCount;
    (void) aAutoSyncStateObj->GetTotalMessageCount(&totalCount);

    nsCOMPtr<nsIMsgFolder> folder;
    aAutoSyncStateObj->GetOwnerFolder(getter_AddRefs(folder));
    if (NS_SUCCEEDED(rv) && folder)
      NOTIFY_LISTENERS(OnDownloadStarted, (folder, length, totalCount));
  }

  return rv;
}

/**
 * Assuming that the download operation on the given folder has been failed at least once, 
 * execute these steps:
 *  - put the auto-sync state into ready-to-download mode
 *  - rollback the message offset so we can try the same group again (unless the retry
 *     count is reached to the given limit)
 *  - if parallel model is active, wait to be resumed by the next idle
 *  - if chained model is active, search the priority queue to find a sibling to continue 
 *    with.
 */
nsresult nsAutoSyncManager::HandleDownloadErrorFor(nsIAutoSyncState *aAutoSyncStateObj,
                                                   const nsresult error)
{
  if (!aAutoSyncStateObj)
    return NS_ERROR_INVALID_ARG;
  
  // ensure that an error occured
  if (NS_SUCCEEDED(error))
    return NS_OK;
    
  // NS_ERROR_NOT_AVAILABLE is a special case/error happens when the queued folder
  // doesn't have any message to download (see bug 457342). In such case we shouldn't
  // retry the current message group, nor notify listeners. Simply continuing with the
  // next sibling in the priority queue would suffice.
    
  if (NS_ERROR_NOT_AVAILABLE != error)
  {
    // force the auto-sync state to try downloading the same group at least
    // kGroupRetryCount times before it moves to the next one
    aAutoSyncStateObj->TryCurrentGroupAgain(kGroupRetryCount);
    
    nsCOMPtr<nsIMsgFolder> folder;
    aAutoSyncStateObj->GetOwnerFolder(getter_AddRefs(folder));
    if (folder)
      NOTIFY_LISTENERS(OnDownloadError, (folder));
  }
  
  // if parallel model, don't do anything else
  
  if (mDownloadModel == dmChained)
  {
    // switch to the next folder in the chain and continue downloading
    nsIAutoSyncState *autoSyncStateObj = aAutoSyncStateObj;
    nsIAutoSyncState *nextAutoSyncStateObj = nullptr;
    while ( (nextAutoSyncStateObj = GetNextSibling(mPriorityQ, autoSyncStateObj)) )
    {
      autoSyncStateObj = nextAutoSyncStateObj;
      nsresult rv = DownloadMessagesForOffline(autoSyncStateObj);
      if (NS_SUCCEEDED(rv))
        break;
      else if (rv == NS_ERROR_NOT_AVAILABLE)
        // next folder in the chain also doesn't have any message to download
        // switch to next one if any
        continue;
      else
        autoSyncStateObj->TryCurrentGroupAgain(kGroupRetryCount);
    }
  }
  
  return NS_OK;
}

uint32_t nsAutoSyncManager::GetUpdateIntervalFor(nsIAutoSyncState *aAutoSyncStateObj)
{
  nsCOMPtr<nsIMsgFolder> folder;
  nsresult rv = aAutoSyncStateObj->GetOwnerFolder(getter_AddRefs(folder));
  if (NS_FAILED(rv))
    return kDefaultUpdateInterval;
  
  nsCOMPtr<nsIMsgIncomingServer> server;
  rv = folder->GetServer(getter_AddRefs(server));
  if (NS_FAILED(rv))
    return kDefaultUpdateInterval;

  if (server)
  {
    int32_t interval;
    rv = server->GetBiffMinutes(&interval);
    
    if (NS_SUCCEEDED(rv))
      return (uint32_t)interval;
  }

  return kDefaultUpdateInterval;
}

NS_IMETHODIMP nsAutoSyncManager::GetGroupSize(uint32_t *aGroupSize)
{
  NS_ENSURE_ARG_POINTER(aGroupSize);
  *aGroupSize = mGroupSize;
  return NS_OK;
}
NS_IMETHODIMP nsAutoSyncManager::SetGroupSize(uint32_t aGroupSize)
{
  mGroupSize = aGroupSize ? aGroupSize : kDefaultGroupSize;
  return NS_OK;
}

NS_IMETHODIMP nsAutoSyncManager::GetMsgStrategy(nsIAutoSyncMsgStrategy * *aMsgStrategy)
{
  NS_ENSURE_ARG_POINTER(aMsgStrategy);
  
  // lazily create if it is not done already
  if (!mMsgStrategyImpl)
  {
    mMsgStrategyImpl = new nsDefaultAutoSyncMsgStrategy;
    if (!mMsgStrategyImpl)
      return NS_ERROR_OUT_OF_MEMORY;
  }
  
  NS_IF_ADDREF(*aMsgStrategy = mMsgStrategyImpl);
  return NS_OK;
}
NS_IMETHODIMP nsAutoSyncManager::SetMsgStrategy(nsIAutoSyncMsgStrategy * aMsgStrategy)
{
  mMsgStrategyImpl = aMsgStrategy;
  return NS_OK;
}

NS_IMETHODIMP nsAutoSyncManager::GetFolderStrategy(nsIAutoSyncFolderStrategy * *aFolderStrategy)
{
  NS_ENSURE_ARG_POINTER(aFolderStrategy);
  
  // lazily create if it is not done already
  if (!mFolderStrategyImpl)
  {
    mFolderStrategyImpl = new nsDefaultAutoSyncFolderStrategy;
    if (!mFolderStrategyImpl)
      return NS_ERROR_OUT_OF_MEMORY;
  }
    
  NS_IF_ADDREF(*aFolderStrategy = mFolderStrategyImpl);
  return NS_OK;
}
NS_IMETHODIMP nsAutoSyncManager::SetFolderStrategy(nsIAutoSyncFolderStrategy * aFolderStrategy)
{
  mFolderStrategyImpl = aFolderStrategy;
  return NS_OK;
}

NS_IMETHODIMP 
nsAutoSyncManager::DoesMsgFitDownloadCriteria(nsIMsgDBHdr *aMsgHdr, bool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  uint32_t msgFlags = 0;
  aMsgHdr->GetFlags(&msgFlags);

  // check whether this message is marked imap deleted or not 
  *aResult = !(msgFlags & nsMsgMessageFlags::IMAPDeleted);
  if (!(*aResult))
    return NS_OK;

  bool shouldStoreMsgOffline = true;
  nsCOMPtr<nsIMsgFolder> folder;
  aMsgHdr->GetFolder(getter_AddRefs(folder));
  if (folder)
  {
    nsMsgKey msgKey;
    nsresult rv = aMsgHdr->GetMessageKey(&msgKey);
    // a cheap way to get the size limit for this folder and make
    // sure that we don't have this message offline already
    if (NS_SUCCEEDED(rv))
      folder->ShouldStoreMsgOffline(msgKey, &shouldStoreMsgOffline);
  }

  *aResult &= shouldStoreMsgOffline;

  return NS_OK;
}

NS_IMETHODIMP nsAutoSyncManager::OnDownloadQChanged(nsIAutoSyncState *aAutoSyncStateObj)
{
  nsCOMPtr<nsIAutoSyncState> autoSyncStateObj(aAutoSyncStateObj);
  if (!autoSyncStateObj)
    return NS_ERROR_INVALID_ARG;

  if (mPaused)
    return NS_OK;
  // We want to start downloading immediately unless the folder is excluded.
  bool excluded = false;
  nsCOMPtr<nsIAutoSyncFolderStrategy> folStrategy;
  nsCOMPtr<nsIMsgFolder> folder;

  GetFolderStrategy(getter_AddRefs(folStrategy));
  autoSyncStateObj->GetOwnerFolder(getter_AddRefs(folder));

  if (folder && folStrategy)
    folStrategy->IsExcluded(folder, &excluded);

  nsresult rv = NS_OK;

  if (!excluded)
  {
    // Add this folder into the priority queue.
    autoSyncStateObj->SetState(nsAutoSyncState::stReadyToDownload);
    ScheduleFolderForOfflineDownload(autoSyncStateObj);

    // If we operate in parallel mode or if there is no sibling downloading messages at the moment,
    // we can download the first group of the messages for this folder
    if (mDownloadModel == dmParallel ||
        !DoesQContainAnySiblingOf(mPriorityQ, autoSyncStateObj, nsAutoSyncState::stDownloadInProgress))
    {
      // this will download the first group of messages immediately;
      // to ensure that we don't end up downloading a large single message in not-idle time, 
      // we enforce a limit. If there is no message fits into this limit we postpone the 
      // download until the next idle.
      if (GetIdleState() == notIdle)
        rv =  DownloadMessagesForOffline(autoSyncStateObj, kFirstGroupSizeLimit);
      else
        rv = DownloadMessagesForOffline(autoSyncStateObj);
      
      if (NS_FAILED(rv))
        autoSyncStateObj->TryCurrentGroupAgain(kGroupRetryCount);
    }
  }
  return rv;
}

NS_IMETHODIMP 
nsAutoSyncManager::OnDownloadStarted(nsIAutoSyncState *aAutoSyncStateObj, nsresult aStartCode)
{
  nsCOMPtr<nsIAutoSyncState> autoSyncStateObj(aAutoSyncStateObj);
  if (!autoSyncStateObj)
    return NS_ERROR_INVALID_ARG;

  // resume downloads during next idle time
  if (NS_FAILED(aStartCode))
    autoSyncStateObj->SetState(nsAutoSyncState::stReadyToDownload);  
  
  return aStartCode;
}

NS_IMETHODIMP 
nsAutoSyncManager::OnDownloadCompleted(nsIAutoSyncState *aAutoSyncStateObj, nsresult aExitCode)
{
  nsCOMPtr<nsIAutoSyncState> autoSyncStateObj(aAutoSyncStateObj);
  if (!autoSyncStateObj)
    return NS_ERROR_INVALID_ARG;    

  nsresult rv = aExitCode;

  if (NS_FAILED(aExitCode))
  {
    // retry the same group kGroupRetryCount times
    // try again if TB still idle, otherwise wait for the next idle time
    autoSyncStateObj->TryCurrentGroupAgain(kGroupRetryCount);
    if (GetIdleState() != notIdle)
    {
      rv = DownloadMessagesForOffline(autoSyncStateObj);
      if (NS_FAILED(rv))
        rv = HandleDownloadErrorFor(autoSyncStateObj, rv);
    }
    return rv;
  }
      
  // download is successful, reset the retry counter of the folder
  autoSyncStateObj->ResetRetryCounter();
  
  nsCOMPtr<nsIMsgFolder> folder;
  aAutoSyncStateObj->GetOwnerFolder(getter_AddRefs(folder));
  if (folder)
    NOTIFY_LISTENERS(OnDownloadCompleted, (folder));
      
  int32_t count;
  rv = autoSyncStateObj->GetPendingMessageCount(&count);
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsIAutoSyncState *nextFolderToDownload = nullptr;
  if (count > 0)
  {
    autoSyncStateObj->SetState(nsAutoSyncState::stReadyToDownload);
    
    // in parallel model, we continue downloading the same folder as long as it has
    // more pending messages
    nextFolderToDownload = autoSyncStateObj;
    
    // in chained model, ensure that we are always downloading the highest priority 
    // folder first 
    if (mDownloadModel == dmChained)
    {
      // switch to higher priority folder and continue to download, 
      // if any added recently
      int32_t myIndex = mPriorityQ.IndexOf(autoSyncStateObj);
      
      int32_t siblingIndex;
      nsIAutoSyncState *sibling = GetHighestPrioSibling(mPriorityQ, autoSyncStateObj, &siblingIndex);
      
      // lesser index = higher priority
      if (sibling && myIndex > -1 && siblingIndex < myIndex) 
        nextFolderToDownload = sibling;
    }
  }
  else 
  {
    autoSyncStateObj->SetState(nsAutoSyncState::stCompletedIdle);
    
    nsCOMPtr<nsIMsgFolder> folder;
    nsresult rv = autoSyncStateObj->GetOwnerFolder(getter_AddRefs(folder));
    
    if (NS_SUCCEEDED(rv) && mPriorityQ.RemoveObject(autoSyncStateObj))
      NOTIFY_LISTENERS(OnFolderRemovedFromQ, (nsIAutoSyncMgrListener::PriorityQueue, folder));

    //find the next folder owned by the same server in the queue and continue downloading
    if (mDownloadModel == dmChained)
      nextFolderToDownload = GetHighestPrioSibling(mPriorityQ, autoSyncStateObj);
      
  }//endif
  
  // continue downloading if TB is still in idle state
  if (nextFolderToDownload && GetIdleState() != notIdle)
  {
    rv = DownloadMessagesForOffline(nextFolderToDownload);
    if (NS_FAILED(rv))
      rv = HandleDownloadErrorFor(nextFolderToDownload, rv);
  }

  return rv;
}

NS_IMETHODIMP nsAutoSyncManager::GetDownloadModel(int32_t *aDownloadModel)
{
  NS_ENSURE_ARG_POINTER(aDownloadModel);
  *aDownloadModel = mDownloadModel;
  return NS_OK;
}
NS_IMETHODIMP nsAutoSyncManager::SetDownloadModel(int32_t aDownloadModel)
{
  mDownloadModel = aDownloadModel;
  return NS_OK;
}

NS_IMETHODIMP nsAutoSyncManager::AddListener(nsIAutoSyncMgrListener *aListener)
{
  NS_ENSURE_ARG_POINTER(aListener);
  mListeners.AppendElementUnlessExists(aListener);
  return NS_OK;
}

NS_IMETHODIMP nsAutoSyncManager::RemoveListener(nsIAutoSyncMgrListener *aListener)
{
  NS_ENSURE_ARG_POINTER(aListener);
  mListeners.RemoveElement(aListener);
  return NS_OK;
}

/* readonly attribute unsigned long discoveryQLength; */
NS_IMETHODIMP nsAutoSyncManager::GetDiscoveryQLength(uint32_t *aDiscoveryQLength)
{
  NS_ENSURE_ARG_POINTER(aDiscoveryQLength);
  *aDiscoveryQLength = mDiscoveryQ.Count();
  return NS_OK;
}

/* readonly attribute unsigned long uploadQLength; */
NS_IMETHODIMP nsAutoSyncManager::GetUpdateQLength(uint32_t *aUpdateQLength)
{
  NS_ENSURE_ARG_POINTER(aUpdateQLength);
  *aUpdateQLength = mUpdateQ.Count();
  return NS_OK;
}

/* readonly attribute unsigned long downloadQLength; */
NS_IMETHODIMP nsAutoSyncManager::GetDownloadQLength(uint32_t *aDownloadQLength)
{
  NS_ENSURE_ARG_POINTER(aDownloadQLength);
  *aDownloadQLength = mPriorityQ.Count();
  return NS_OK;
}

NS_IMETHODIMP
nsAutoSyncManager::OnFolderHasPendingMsgs(nsIAutoSyncState *aAutoSyncStateObj)
{
  NS_ENSURE_ARG_POINTER(aAutoSyncStateObj);
  if (mUpdateQ.IndexOf(aAutoSyncStateObj) == -1)
  {
    nsCOMPtr<nsIMsgFolder> folder;
    aAutoSyncStateObj->GetOwnerFolder(getter_AddRefs(folder));
    // If this folder isn't the trash, add it to the update q.
    if (folder)
    {
      bool isTrash;
      folder->GetFlag(nsMsgFolderFlags::Trash, &isTrash);
      if (!isTrash)
      {
        bool isSentOrArchive;
        folder->IsSpecialFolder(nsMsgFolderFlags::SentMail|
                                nsMsgFolderFlags::Archive,
                                true, &isSentOrArchive);
        // Sent or archive folders go to the q front, the rest to the end.
        if (isSentOrArchive)
          mUpdateQ.InsertObjectAt(aAutoSyncStateObj, 0);
        else
          mUpdateQ.AppendObject(aAutoSyncStateObj);
        aAutoSyncStateObj->SetState(nsAutoSyncState::stUpdateNeeded);
        NOTIFY_LISTENERS(OnFolderAddedIntoQ,
                        (nsIAutoSyncMgrListener::UpdateQueue, folder));
      }
    }
  }
  return NS_OK;
}

void nsAutoSyncManager::SetIdleState(IdleState st) 
{ 
  mIdleState = st;
}

nsAutoSyncManager::IdleState nsAutoSyncManager::GetIdleState() const 
{ 
  return mIdleState; 
}

NS_IMPL_ISUPPORTS3(nsAutoSyncManager, nsIObserver, nsIUrlListener, nsIAutoSyncManager)
