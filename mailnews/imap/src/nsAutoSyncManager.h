/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAutoSyncManager_h__
#define nsAutoSyncManager_h__

#include "nsAutoPtr.h"
#include "nsStringGlue.h"
#include "nsCOMArray.h"
#include "nsIObserver.h"
#include "nsIUrlListener.h"
#include "nsITimer.h"
#include "nsTObserverArray.h"
#include "nsIAutoSyncManager.h"
#include "nsIAutoSyncMsgStrategy.h"
#include "nsIAutoSyncFolderStrategy.h"

class nsImapMailFolder;
class nsIMsgDBHdr;
class nsIIdleService;
class nsIMsgFolder;

/* Auto-Sync
 *
 * Background:
 *  it works only with offline imap folders. "autosync_offline_stores" pref
 *  enables/disables auto-sync mechanism. Note that setting "autosync_offline_stores"
 *  to false, or setting folder to not-offline doesn't stop synchronization
 *  process for already queued folders.
 *
 * Auto-Sync policy:
 *  o It kicks in during system idle time, and tries to download as much messages
 *    as possible based on given folder and message prioritization strategies/rules.
 *    Default folder prioritization strategy dictates to sort the folders based on the
 *    following order:  INBOX > DRAFTS > SUBFOLDERS > TRASH.
 *    Similarly, default message prioritization strategy dictates to download the most
 *    recent and smallest message first. Also, by sorting the messages by size in the 
 *    queue, it tries to maximize the number of messages downloaded.
 *  o It downloads the messages in groups. Default groups size is defined by |kDefaultGroupSize|. 
 *  o It downloads the messages larger than the group size one-by-one.
 *  o If new messages arrive when not idle, it downloads the messages that do fit into
 *    |kFirstGroupSizeLimit| size limit immediately, without waiting for idle time, unless there is
 *    a sibling (a folder owned by the same imap server) in stDownloadInProgress state in the q
 *  o If new messages arrive when idle, it downloads all the messages without any restriction.
 *  o If new messages arrive into a folder while auto-sync is downloading other messages of the
 *    same folder, it simply puts the new messages into the folder's download queue, and
 *    re-prioritize the messages. That behavior makes sure that the high priority
 *    (defined by the message strategy) get downloaded first always.
 *  o If new messages arrive into a folder while auto-sync is downloading messages of a lower
 *    priority folder, auto-sync switches the folders in the queue and starts downloading the
 *    messages of the higher priority folder next time it downloads a message group.
 *  o Currently there is no way to stop/pause/cancel a message download. The smallest
 *    granularity is the message group size.
 *  o Auto-Sync manager periodically (kAutoSyncFreq) checks folder for existing messages
 *    w/o bodies. It persists the last time the folder is checked in the local database of the
 *    folder. We call this process 'Discovery'. This process is asynchronous and processes
 *    |kNumberOfHeadersToProcess| number of headers at each cycle. Since it works on local data,
 *    it doesn't consume lots of system resources, it does its job fast.
 *  o Discovery is necessary especially when the user makes a transition from not-offline 
 *    to offline mode.
 *  o Update frequency is defined by nsMsgIncomingServer::BiffMinutes.
 *
 * Error Handling:
 *  o if the user moves/deletes/filters all messages of a folder already queued, auto-sync
 *    deals with that situation by skipping the folder in question, and continuing with the
 *    next in chain.
 *  o If the message size is zero, auto-sync ignores the message.
 *  o If the download of the message group fails for some reason, auto-sync tries to
 *    download the same group |kGroupRetryCount| times. If it still fails, continues with the
 *    next group of messages.
 *
 * Download Model:
 *  Parallel model should be used with the imap servers that do not have any "max number of sessions
 *  per IP" limit, and when the bandwidth is significantly large.
 *
 * How it really works:
 * The AutoSyncManager gets an idle notification. First it processes any
 * folders in the discovery queue (which means it schedules message download
 * for any messages it previously determined it should download). Then it sets
 * a timer, and in the timer callback, it processes the update q, by calling 
 * InitiateAutoSync on the first folder in the update q.
 */
 
/**
 * Default strategy implementation to prioritize messages in the download queue.   
 */
class nsDefaultAutoSyncMsgStrategy : public nsIAutoSyncMsgStrategy
{
  static const uint32_t kFirstPassMessageSize = 60U*1024U; // 60K

  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIAUTOSYNCMSGSTRATEGY

    nsDefaultAutoSyncMsgStrategy();

  private:
    ~nsDefaultAutoSyncMsgStrategy();
};

/**
 * Default strategy implementation to prioritize folders in the download queue.  
 */
class nsDefaultAutoSyncFolderStrategy : public nsIAutoSyncFolderStrategy
{
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIAUTOSYNCFOLDERSTRATEGY

    nsDefaultAutoSyncFolderStrategy();

  private:
    ~nsDefaultAutoSyncFolderStrategy();
};

// see the end of the page for auto-sync internals

/**
 * Manages background message download operations for offline imap folders. 
 */
class nsAutoSyncManager : public nsIObserver, 
                          public nsIUrlListener,
                          public nsIAutoSyncManager
{
  static const PRTime kAutoSyncFreq = 60UL * (PR_USEC_PER_SEC * 60UL);  // 1hr
  static const uint32_t kDefaultUpdateInterval = 10UL;                  // 10min
  static const int32_t kTimerIntervalInMs = 400;
  static const uint32_t kNumberOfHeadersToProcess = 250U;
  // enforced size of the first group that will be downloaded before idle time
  static const uint32_t kFirstGroupSizeLimit = 60U*1024U /* 60K */; 
  static const int32_t kIdleTimeInSec = 10;
  static const uint32_t kGroupRetryCount = 3;

  enum IdleState { systemIdle, appIdle, notIdle };
  enum UpdateState { initiated, completed };
      
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER
    NS_DECL_NSIURLLISTENER
    NS_DECL_NSIAUTOSYNCMANAGER

    nsAutoSyncManager();

  private:
    ~nsAutoSyncManager();

    void SetIdleState(IdleState st);
    IdleState GetIdleState() const;
    nsresult StartIdleProcessing();
    nsresult AutoUpdateFolders(); 
    void ScheduleFolderForOfflineDownload(nsIAutoSyncState *aAutoSyncStateObj);
    nsresult DownloadMessagesForOffline(nsIAutoSyncState *aAutoSyncStateObj, uint32_t aSizeLimit = 0);
    nsresult HandleDownloadErrorFor(nsIAutoSyncState *aAutoSyncStateObj, const nsresult error);

    // Helper methods for priority Q operations
    static
    void ChainFoldersInQ(const nsCOMArray<nsIAutoSyncState> &aQueue,
                          nsCOMArray<nsIAutoSyncState> &aChainedQ);
    static
    nsIAutoSyncState* SearchQForSibling(const nsCOMArray<nsIAutoSyncState> &aQueue,
                          nsIAutoSyncState *aAutoSyncStateObj, int32_t aStartIdx, int32_t *aIndex = nullptr);
    static
    bool DoesQContainAnySiblingOf(const nsCOMArray<nsIAutoSyncState> &aQueue, 
                          nsIAutoSyncState *aAutoSyncStateObj, const int32_t aState, 
                          int32_t *aIndex = nullptr);
    static 
    nsIAutoSyncState* GetNextSibling(const nsCOMArray<nsIAutoSyncState> &aQueue, 
                          nsIAutoSyncState *aAutoSyncStateObj, int32_t *aIndex = nullptr);
    static 
    nsIAutoSyncState* GetHighestPrioSibling(const nsCOMArray<nsIAutoSyncState> &aQueue, 
                          nsIAutoSyncState *aAutoSyncStateObj, int32_t *aIndex = nullptr);

    /// timer to process existing keys and updates
    void InitTimer();
    static void TimerCallback(nsITimer *aTimer, void *aClosure);
    void StopTimer();
    void StartTimerIfNeeded();

    /// pref helpers
    uint32_t GetUpdateIntervalFor(nsIAutoSyncState *aAutoSyncStateObj);

  protected:
    nsCOMPtr<nsIAutoSyncMsgStrategy> mMsgStrategyImpl;
    nsCOMPtr<nsIAutoSyncFolderStrategy> mFolderStrategyImpl;
    // contains the folders that will be downloaded on background
    nsCOMArray<nsIAutoSyncState> mPriorityQ;
    // contains the folders that will be examined for existing headers and
    // adds the headers we don't have offline into the autosyncState
    // object's download queue.
    nsCOMArray<nsIAutoSyncState> mDiscoveryQ;
    // contains the folders that will be checked for new messages with STATUS,
    // and if there are any, we'll call UpdateFolder on them.
    nsCOMArray<nsIAutoSyncState> mUpdateQ;
    // this is the update state for the current folder.
    UpdateState mUpdateState;

    // This is set if auto sync has been completely paused.
    bool mPaused;
    // This is set if we've finished startup and should start
    // paying attention to idle notifications.
    bool mStartupDone;

  private:
    uint32_t mGroupSize;
    IdleState mIdleState;
    int32_t mDownloadModel;
    nsCOMPtr<nsIIdleService> mIdleService;
    nsCOMPtr<nsITimer> mTimer;
    nsTObserverArray<nsCOMPtr<nsIAutoSyncMgrListener> > mListeners;
};

#endif

/*
How queues inter-relate:

nsAutoSyncState has an internal priority queue to store messages waiting to be
downloaded. nsAutoSyncMsgStrategy object determines the order in this queue,
nsAutoSyncManager uses this queue to manage downloads. Two events cause a
change in this queue: 

1) nsImapMailFolder::HeaderFetchCompleted: is triggered when TB notices that
there are pending messages on the server -- via IDLE command from the server, 
via explicit select from the user, or via automatic Update during idle time. If 
it turns out that there are pending messages on the server, it adds them into 
nsAutoSyncState's download queue.

2) nsAutoSyncState::ProcessExistingHeaders: is triggered for every imap folder 
every hour or so (see kAutoSyncFreq). nsAutoSyncManager uses an internal queue called Discovery 
queue to keep track of this task. The purpose of ProcessExistingHeaders() 
method is to check existing headers of a given folder in batches and discover 
the messages without bodies, in asynchronous fashion. This process is 
sequential, one and only one folder at any given time, very similar to 
indexing. Again, if it turns out that the folder in hand has messages w/o 
bodies, ProcessExistingHeaders adds them into nsAutoSyncState's download queue.

Any change in nsAutoSyncState's download queue, notifies nsAutoSyncManager and 
nsAutoSyncManager puts the requesting  nsAutoSyncState into its internal 
priority queue (called mPriorityQ) -- if the folder is not already there. 
nsAutoSyncFolderStrategy object determines the order in this queue. This queue 
is processed in two modes: chained and parallel. 

i) Chained: One folder per imap server any given time. Folders owned by 
different imap servers are simultaneous.

ii) Parallel: All folders at the same time, using all cached-connections - 
a.k.a 'Folders gone wild' mode.

The order the folders are added into the mPriorityQ doesn't matter since every
time a batch completed for an imap server, nsAutoSyncManager adjusts the order.
So, lets say that updating a sub-folder starts downloading message immediately,
when an higher priority folder is added into the queue, nsAutoSyncManager
switches to this higher priority folder instead of processing the next group of
messages of the lower priority one. Setting group size too high might delay
this switch at worst. 

And finally, Update queue helps nsAutoSyncManager to keep track of folders 
waiting to be updated. With the latest change, we update one and only one
folder at any given time. Default frequency of updating is 10 min (kDefaultUpdateInterval). 
We add folders into the update queue during idle time, if they are not in mPriorityQ already.

*/
