/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsMsgSendLater.h"
#include "nsMsgCopy.h"
#include "nsIMsgSend.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIMsgMessageService.h"
#include "nsIMsgAccountManager.h"
#include "nsMsgBaseCID.h"
#include "nsMsgCompCID.h"
#include "nsMsgCompUtils.h"
#include "nsMsgUtils.h"
#include "nsMailHeaders.h"
#include "nsMsgPrompts.h"
#include "nsISmtpUrl.h"
#include "nsIChannel.h"
#include "nsNetUtil.h"
#include "prlog.h"
#include "prmem.h"
#include "nsIMimeConverter.h"
#include "nsMsgMimeCID.h"
#include "nsComposeStrings.h"
#include "nsIMutableArray.h"
#include "nsArrayEnumerator.h"
#include "nsIObserverService.h"
#include "nsIMsgLocalMailFolder.h"
#include "nsIMsgDatabase.h"
#include "mozilla/Services.h"

// Consts for checking and sending mail in milliseconds

// 1 second from mail into the unsent messages folder to initially trying to
// send it.
const uint32_t kInitialMessageSendTime = 1000;

NS_IMPL_ISUPPORTS7(nsMsgSendLater,
                   nsIMsgSendLater,
                   nsIFolderListener,
                   nsIRequestObserver,
                   nsIStreamListener,
                   nsIObserver,
                   nsIUrlListener,
                   nsIMsgShutdownTask)

nsMsgSendLater::nsMsgSendLater()
{
  mSendingMessages = false;
  mTimerSet = false;
  mTotalSentSuccessfully = 0;
  mTotalSendCount = 0;
  mLeftoverBuffer = nullptr;

  m_to = nullptr;
  m_bcc = nullptr;
  m_fcc = nullptr;
  m_newsgroups = nullptr;
  m_newshost = nullptr;
  m_headers = nullptr;
  m_flags = 0;
  m_headersFP = 0;
  m_inhead = true;
  m_headersPosition = 0;

  m_bytesRead = 0;
  m_position = 0;
  m_flagsPosition = 0;
  m_headersSize = 0;

  mIdentityKey = nullptr;
  mAccountKey = nullptr;
}

nsMsgSendLater::~nsMsgSendLater()
{
  PR_Free(m_to);
  PR_Free(m_fcc);
  PR_Free(m_bcc);
  PR_Free(m_newsgroups);
  PR_Free(m_newshost);
  PR_Free(m_headers);
  PR_Free(mLeftoverBuffer);
  PR_Free(mIdentityKey);
  PR_Free(mAccountKey);
}

nsresult
nsMsgSendLater::Init()
{
  nsresult rv;
  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  bool sendInBackground;
  rv = prefs->GetBoolPref("mailnews.sendInBackground", &sendInBackground);
  // If we're not sending in the background, don't do anything else
  if (NS_FAILED(rv) || !sendInBackground)
    return NS_OK;

  // We need to know when we're shutting down.
  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  NS_ENSURE_TRUE(observerService, NS_ERROR_UNEXPECTED);

  rv = observerService->AddObserver(this, "xpcom-shutdown", false);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = observerService->AddObserver(this, "quit-application", false);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = observerService->AddObserver(this, "msg-shutdown", false);
  NS_ENSURE_SUCCESS(rv, rv);

  // Subscribe to the unsent messages folder
  // XXX This code should be set up for multiple unsent folders, however we
  // don't support that at the moment, so for now just assume one folder.
  rv = GetUnsentMessagesFolder(nullptr, getter_AddRefs(mMessageFolder));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mMessageFolder->AddFolderListener(this);
  NS_ENSURE_SUCCESS(rv, rv);

  // XXX may want to send messages X seconds after startup if there are any.

  return NS_OK;
}

NS_IMETHODIMP
nsMsgSendLater::Observe(nsISupports *aSubject, const char* aTopic,
                        const PRUnichar *aData)
{
  if (aSubject == mTimer && !strcmp(aTopic, "timer-callback"))
  {
    if (mTimer)
      mTimer->Cancel();
    else
      NS_ERROR("mTimer was null in nsMsgSendLater::Observe");

    mTimerSet = false;
    // If we've already started a send since the timer fired, don't start
    // another
    if (!mSendingMessages)
      InternalSendMessages(false, nullptr);
  }
  else if (!strcmp(aTopic, "quit-application"))
  {
    // If the timer is set, cancel it - we're quitting, the shutdown service
    // interfaces will sort out sending etc.
    if (mTimer)
      mTimer->Cancel();

    mTimerSet = false;
  }
  else if (!strcmp(aTopic, "xpcom-shutdown"))
  {
    // We're shutting down. Unsubscribe from the unsentFolder notifications
    // they aren't any use to us now, we don't want to start sending more
    // messages.
    nsresult rv;
    if (mMessageFolder)
    {
      rv = mMessageFolder->RemoveFolderListener(this);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    // Now remove ourselves from the observer service as well.
    nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
    NS_ENSURE_TRUE(observerService, NS_ERROR_UNEXPECTED);

    rv = observerService->RemoveObserver(this, "xpcom-shutdown");
    NS_ENSURE_SUCCESS(rv, rv);

    rv = observerService->RemoveObserver(this, "quit-application");
    NS_ENSURE_SUCCESS(rv, rv);

    rv = observerService->RemoveObserver(this, "msg-shutdown");
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsMsgSendLater::SetStatusFeedback(nsIMsgStatusFeedback *aFeedback)
{
  mFeedback = aFeedback;
  return NS_OK;
}

NS_IMETHODIMP
nsMsgSendLater::GetStatusFeedback(nsIMsgStatusFeedback **aFeedback)
{
  NS_ENSURE_ARG_POINTER(aFeedback);
  NS_IF_ADDREF(*aFeedback = mFeedback);
  return NS_OK;
}

// Stream is done...drive on!
NS_IMETHODIMP
nsMsgSendLater::OnStopRequest(nsIRequest *request, nsISupports *ctxt, nsresult status)
{
  nsresult    rv;

  // First, this shouldn't happen, but if
  // it does, flush the buffer and move on.
  if (mLeftoverBuffer)
  {
    DeliverQueuedLine(mLeftoverBuffer, PL_strlen(mLeftoverBuffer));
  }

  if (mOutFile)
    mOutFile->Close();

  // See if we succeeded on reading the message from the message store?
  //
  if (NS_SUCCEEDED(status))
  {
    // Message is done...send it!
    rv = CompleteMailFileSend();

#ifdef NS_DEBUG
    printf("nsMsgSendLater: Success on getting message...\n");
#endif
    
    // If the send operation failed..try the next one...
    if (NS_FAILED(rv))
    {
      rv = StartNextMailFileSend(rv);
      if (NS_FAILED(rv))
        EndSendMessages(rv, nullptr, mTotalSendCount, mTotalSentSuccessfully);
    }
  }
  else
  {
    nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
    if(!channel) return NS_ERROR_FAILURE;

    // extract the prompt object to use for the alert from the url....
    nsCOMPtr<nsIURI> uri; 
    nsCOMPtr<nsIPrompt> promptObject;
    if (channel)
    {
      channel->GetURI(getter_AddRefs(uri));
      nsCOMPtr<nsISmtpUrl> smtpUrl (do_QueryInterface(uri));
      if (smtpUrl)
        smtpUrl->GetPrompt(getter_AddRefs(promptObject));
    } 
    nsMsgDisplayMessageByID(promptObject, NS_ERROR_QUEUED_DELIVERY_FAILED);
    
    // Getting the data failed, but we will still keep trying to send the rest...
    rv = StartNextMailFileSend(status);
    if (NS_FAILED(rv))
      EndSendMessages(rv, nullptr, mTotalSendCount, mTotalSentSuccessfully);
  }

  return rv;
}

char *
FindEOL(char *inBuf, char *buf_end)
{
  char *buf = inBuf;
  char *findLoc = nullptr;

  while (buf <= buf_end)
    if (*buf == 0) 
      return buf;
    else if ( (*buf == '\n') || (*buf == '\r') )
    {
      findLoc = buf;
      break;
    }
    else
      ++buf;

  if (!findLoc)
    return nullptr;
  else if ((findLoc + 1) > buf_end)
    return buf;

  if ( (*findLoc == '\n' && *(findLoc+1) == '\r') || 
       (*findLoc == '\r' && *(findLoc+1) == '\n'))
    findLoc++; // possibly a pair.       
  return findLoc;
}

nsresult
nsMsgSendLater::RebufferLeftovers(char *startBuf, uint32_t aLen)
{
  PR_FREEIF(mLeftoverBuffer);
  mLeftoverBuffer = (char *)PR_Malloc(aLen + 1);
  if (!mLeftoverBuffer)
    return NS_ERROR_OUT_OF_MEMORY;

  memcpy(mLeftoverBuffer, startBuf, aLen);
  mLeftoverBuffer[aLen] = '\0';
  return NS_OK;
}

nsresult
nsMsgSendLater::BuildNewBuffer(const char* aBuf, uint32_t aCount, uint32_t *totalBufSize)
{
  // Only build a buffer when there are leftovers...
  if (!mLeftoverBuffer)
    return NS_ERROR_FAILURE;

  int32_t leftoverSize = PL_strlen(mLeftoverBuffer);
  mLeftoverBuffer = (char *)PR_Realloc(mLeftoverBuffer, aCount + leftoverSize);
  if (!mLeftoverBuffer)
    return NS_ERROR_FAILURE;

  memcpy(mLeftoverBuffer + leftoverSize, aBuf, aCount);
  *totalBufSize = aCount + leftoverSize;
  return NS_OK;
}

// Got data?
NS_IMETHODIMP
nsMsgSendLater::OnDataAvailable(nsIRequest *request, nsISupports *ctxt, nsIInputStream *inStr, uint32_t sourceOffset, uint32_t count)
{
  NS_ENSURE_ARG_POINTER(inStr);

  // This is a little bit tricky since we have to chop random 
  // buffers into lines and deliver the lines...plus keeping the
  // leftovers for next time...some fun, eh?
  //
  nsresult    rv = NS_OK;
  char        *startBuf;
  char        *endBuf;
  char        *lineEnd;
  char        *newbuf = nullptr;
  uint32_t    size;

  uint32_t    aCount = count;
  char        *aBuf = (char *)PR_Malloc(aCount + 1);

  inStr->Read(aBuf, count, &aCount);

  // First, create a new work buffer that will
  if (NS_FAILED(BuildNewBuffer(aBuf, aCount, &size))) // no leftovers...
  {
    startBuf = (char *)aBuf;
    endBuf = (char *)(aBuf + aCount - 1);
  }
  else  // yum, leftovers...new buffer created...sitting in mLeftoverBuffer
  {
    newbuf = mLeftoverBuffer;
    startBuf = newbuf;
    endBuf = startBuf + size - 1;
    mLeftoverBuffer = nullptr; // null out this
  }

  while (startBuf <= endBuf)
  {
    lineEnd = FindEOL(startBuf, endBuf);
    if (!lineEnd)
    {
      rv = RebufferLeftovers(startBuf, (endBuf - startBuf) + 1);
      break;
    }

    rv = DeliverQueuedLine(startBuf, (lineEnd - startBuf) + 1);
    if (NS_FAILED(rv))
      break;

    startBuf = lineEnd+1;
  }

  PR_Free(newbuf);
  PR_Free(aBuf);
  return rv;
}

NS_IMETHODIMP
nsMsgSendLater::OnStartRunningUrl(nsIURI *url)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMsgSendLater::OnStopRunningUrl(nsIURI *url, nsresult aExitCode)
{
  if (NS_SUCCEEDED(aExitCode))
    InternalSendMessages(mUserInitiated, mIdentity);
  return NS_OK;
}

NS_IMETHODIMP
nsMsgSendLater::OnStartRequest(nsIRequest *request, nsISupports *ctxt)
{
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////////
// This is the listener class for the send operation. We have to create this class 
// to listen for message send completion and eventually notify the caller
////////////////////////////////////////////////////////////////////////////////////
NS_IMPL_ISUPPORTS2(SendOperationListener, nsIMsgSendListener,
                   nsIMsgCopyServiceListener)

SendOperationListener::SendOperationListener(nsMsgSendLater *aSendLater)
: mSendLater(aSendLater)
{
}

SendOperationListener::~SendOperationListener(void) 
{
}

NS_IMETHODIMP
SendOperationListener::OnGetDraftFolderURI(const char *aFolderURI)
{
  return NS_OK;
}
  
NS_IMETHODIMP
SendOperationListener::OnStartSending(const char *aMsgID, uint32_t aMsgSize)
{
#ifdef NS_DEBUG
  printf("SendOperationListener::OnStartSending()\n");
#endif
  return NS_OK;
}
  
NS_IMETHODIMP
SendOperationListener::OnProgress(const char *aMsgID, uint32_t aProgress, uint32_t aProgressMax)
{
#ifdef NS_DEBUG
  printf("SendOperationListener::OnProgress()\n");
#endif
  return NS_OK;
}

NS_IMETHODIMP
SendOperationListener::OnStatus(const char *aMsgID, const PRUnichar *aMsg)
{
#ifdef NS_DEBUG
  printf("SendOperationListener::OnStatus()\n");
#endif

  return NS_OK;
}

NS_IMETHODIMP
SendOperationListener::OnSendNotPerformed(const char *aMsgID, nsresult aStatus)
{
  return NS_OK;
}
  
NS_IMETHODIMP
SendOperationListener::OnStopSending(const char *aMsgID, nsresult aStatus, const PRUnichar *aMsg, 
                                     nsIFile *returnFile)
{
  if (mSendLater && !mSendLater->OnSendStepFinished(aStatus))
      NS_RELEASE(mSendLater);

  return NS_OK;
}

// nsIMsgCopyServiceListener

NS_IMETHODIMP
SendOperationListener::OnStartCopy(void)
{
  return NS_OK;
}

NS_IMETHODIMP
SendOperationListener::OnProgress(uint32_t aProgress, uint32_t aProgressMax)
{
  return NS_OK;
}

NS_IMETHODIMP
SendOperationListener::SetMessageKey(uint32_t aKey)
{
  NS_NOTREACHED("SendOperationListener::SetMessageKey()");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
SendOperationListener::GetMessageId(nsACString& messageId)
{
  NS_NOTREACHED("SendOperationListener::GetMessageId()\n");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
SendOperationListener::OnStopCopy(nsresult aStatus)
{
  if (mSendLater)
  {
    mSendLater->OnCopyStepFinished(aStatus);
    NS_RELEASE(mSendLater);
  }

  return NS_OK;
}

nsresult
nsMsgSendLater::CompleteMailFileSend()
{
  // get the identity from the key
  // if no key, or we fail to find the identity
  // use the default identity on the default account
  nsCOMPtr<nsIMsgIdentity> identity;
  nsresult rv = GetIdentityFromKey(mIdentityKey, getter_AddRefs(identity));
  NS_ENSURE_SUCCESS(rv,rv);

  // If for some reason the tmp file didn't get created, we've failed here
  bool created;
  mTempFile->Exists(&created);
  if (!created)
    return NS_ERROR_FAILURE;

  // Get the recipients...
  nsCString recips;
  nsCString ccList;
  if (NS_FAILED(mMessage->GetRecipients(getter_Copies(recips))))
    return NS_ERROR_UNEXPECTED;
  else
    mMessage->GetCcList(getter_Copies(ccList));

  nsCOMPtr<nsIMsgCompFields> compFields = do_CreateInstance(NS_MSGCOMPFIELDS_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr<nsIMsgSend> pMsgSend = do_CreateInstance(NS_MSGSEND_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr<nsIMimeConverter> mimeConverter = do_GetService(NS_MIME_CONVERTER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // Since we have already parsed all of the headers, we are simply going to
  // set the composition fields and move on.
  //
  nsCString author;
  mMessage->GetAuthor(getter_Copies(author));

  nsMsgCompFields * fields = (nsMsgCompFields *)compFields.get();

  nsCString decodedString;
  // decoded string is null if the input is not MIME encoded
  mimeConverter->DecodeMimeHeaderToCharPtr(author.get(), nullptr, false,
                                           true,
                                           getter_Copies(decodedString));

  fields->SetFrom(decodedString.IsEmpty() ? author.get() : decodedString.get());

  if (m_to)
  {
    mimeConverter->DecodeMimeHeaderToCharPtr(m_to, nullptr, false, true,
                                             getter_Copies(decodedString));
    fields->SetTo(decodedString.IsEmpty() ? m_to : decodedString.get());
  }

  if (m_bcc)
  {
    mimeConverter->DecodeMimeHeaderToCharPtr(m_bcc, nullptr, false, true,
                                             getter_Copies(decodedString));
    fields->SetBcc(decodedString.IsEmpty() ? m_bcc : decodedString.get());
  }

  if (m_fcc)
  {
    mimeConverter->DecodeMimeHeaderToCharPtr(m_fcc, nullptr, false, true,
                                             getter_Copies(decodedString));
    fields->SetFcc(decodedString.IsEmpty() ? m_fcc : decodedString.get());
  }

  if (m_newsgroups)
    fields->SetNewsgroups(m_newsgroups);

#if 0
  // needs cleanup.  SetNewspostUrl()?
  if (m_newshost)
    fields->SetNewshost(m_newshost);
#endif

  // Create the listener for the send operation...
  SendOperationListener *sendListener = new SendOperationListener(this);
  if (!sendListener)
    return NS_ERROR_OUT_OF_MEMORY;
  
  NS_ADDREF(sendListener);

  NS_ADDREF(this);  //TODO: We should remove this!!!
  rv = pMsgSend->SendMessageFile(identity,
                                 mAccountKey,
                                 compFields, // nsIMsgCompFields *fields,
                                 mTempFile, // nsIFile *sendFile,
                                 true, // bool deleteSendFileOnCompletion,
                                 false, // bool digest_p,
                                 nsIMsgSend::nsMsgSendUnsent, // nsMsgDeliverMode mode,
                                 nullptr, // nsIMsgDBHdr *msgToReplace, 
                                 sendListener,
                                 mFeedback,
                                 nullptr); 
  NS_RELEASE(sendListener);
  return rv;
}

nsresult
nsMsgSendLater::StartNextMailFileSend(nsresult prevStatus)
{
  bool hasMoreElements = false;
  if ((!mEnumerator) ||
      NS_FAILED(mEnumerator->HasMoreElements(&hasMoreElements)) ||
      !hasMoreElements)
  {
    // Notify that this message has finished being sent.
    NotifyListenersOnProgress(mTotalSendCount, mMessagesToSend.Count(), 100, 100);

    // EndSendMessages resets everything for us
    EndSendMessages(prevStatus, nullptr, mTotalSendCount, mTotalSentSuccessfully);

    // XXX Should we be releasing references so that we don't hold onto items
    // unnecessarily.
    return NS_OK;
  }

  // If we've already sent a message, and are sending more, send out a progress
  // update with 100% for both send and copy as we must have finished by now.
  if (mTotalSendCount)
    NotifyListenersOnProgress(mTotalSendCount, mMessagesToSend.Count(), 100, 100);

  nsCOMPtr<nsISupports> currentItem;
  nsresult rv = mEnumerator->GetNext(getter_AddRefs(currentItem));
  NS_ENSURE_SUCCESS(rv, rv);

  mMessage = do_QueryInterface(currentItem); 
  if (!mMessage)
    return NS_ERROR_NOT_AVAILABLE;

  if (!mMessageFolder)
    return NS_ERROR_UNEXPECTED;

  nsCString messageURI;
  mMessageFolder->GetUriForMsg(mMessage, messageURI);

  rv = nsMsgCreateTempFile("nsqmail.tmp", getter_AddRefs(mTempFile)); 
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIMsgMessageService> messageService;
  rv = GetMessageServiceFromURI(messageURI, getter_AddRefs(messageService));
  if (NS_FAILED(rv) && !messageService)
    return NS_ERROR_FACTORY_NOT_LOADED;

  ++mTotalSendCount;

  nsCString identityKey;
  rv = mMessage->GetStringProperty(HEADER_X_MOZILLA_IDENTITY_KEY,
                                   getter_Copies(identityKey));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIMsgIdentity> identity;
  rv = GetIdentityFromKey(identityKey.get(), getter_AddRefs(identity));
  NS_ENSURE_SUCCESS(rv, rv);

  // Notify that we're just about to start sending this message
  NotifyListenersOnMessageStartSending(mTotalSendCount, mMessagesToSend.Count(),
                                       identity);

  // Setup what we need to parse the data stream correctly
  m_inhead = true;
  m_headersFP = 0;
  m_headersPosition = 0;
  m_bytesRead = 0;
  m_position = 0;
  m_flagsPosition = 0;
  m_headersSize = 0;
  PR_FREEIF(mLeftoverBuffer);

  // Now, get our stream listener interface and plug it into the DisplayMessage
  // operation
  AddRef();

  rv = messageService->DisplayMessage(messageURI.get(),
                                      static_cast<nsIStreamListener*>(this),
                                      nullptr, nullptr, nullptr, nullptr);

  Release();

  return rv;
}

NS_IMETHODIMP 
nsMsgSendLater::GetUnsentMessagesFolder(nsIMsgIdentity *aIdentity, nsIMsgFolder **folder)
{
  nsCString uri;
  GetFolderURIFromUserPrefs(nsIMsgSend::nsMsgQueueForLater, aIdentity, uri);
  return LocateMessageFolder(aIdentity, nsIMsgSend::nsMsgQueueForLater,
                             uri.get(), folder);
}

NS_IMETHODIMP
nsMsgSendLater::HasUnsentMessages(nsIMsgIdentity *aIdentity, bool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  nsresult rv;

  nsCOMPtr<nsIMsgAccountManager> accountManager =
    do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISupportsArray> accounts;
  accountManager->GetAccounts(getter_AddRefs(accounts));
  NS_ENSURE_SUCCESS(rv, rv);

  uint32_t cnt = 0;
  rv = accounts->Count(&cnt);
  if (cnt == 0) {
    *aResult = false;
    return NS_OK; // no account set up -> no unsent messages
  }

  // XXX This code should be set up for multiple unsent folders, however we
  // don't support that at the moment, so for now just assume one folder.
  if (!mMessageFolder)
  {
    rv = GetUnsentMessagesFolder(nullptr,
                                 getter_AddRefs(mMessageFolder));
    NS_ENSURE_SUCCESS(rv, rv);
  }
  rv = ReparseDBIfNeeded(nullptr);
  NS_ENSURE_SUCCESS(rv, rv);

  int32_t totalMessages;
  rv = mMessageFolder->GetTotalMessages(false, &totalMessages);
  NS_ENSURE_SUCCESS(rv, rv);

  *aResult = totalMessages > 0;
  return NS_OK;
}

//
// To really finalize this capability, we need to have the ability to get
// the message from the mail store in a stream for processing. The flow 
// would be something like this:
//
//      foreach (message in Outbox folder)
//         get stream of Nth message
//         if (not done with headers)
//            Tack on to current buffer of headers
//         when done with headers
//            BuildHeaders()
//            Write Headers to Temp File
//         after done with headers
//            write rest of message body to temp file
//
//          when done with the message
//            do send operation
//
//          when send is complete
//            Copy from Outbox to FCC folder
//            Delete from Outbox folder
//
//
NS_IMETHODIMP 
nsMsgSendLater::SendUnsentMessages(nsIMsgIdentity *aIdentity)
{
  return InternalSendMessages(true, aIdentity);
}

// Returns NS_OK if the db is OK, an error otherwise, e.g., we had to reparse.
nsresult nsMsgSendLater::ReparseDBIfNeeded(nsIUrlListener *aListener)
{
  // This will kick off a reparse, if needed. So the next time we check if
  // there are unsent messages, the db will be up to date.
  nsCOMPtr<nsIMsgDatabase> unsentDB;
  nsresult rv;
  nsCOMPtr<nsIMsgLocalMailFolder> locFolder(do_QueryInterface(mMessageFolder, &rv));
  NS_ENSURE_SUCCESS(rv, rv);
  return locFolder->GetDatabaseWithReparse(aListener, nullptr,
                                           getter_AddRefs(unsentDB));
}

nsresult
nsMsgSendLater::InternalSendMessages(bool aUserInitiated,
                                     nsIMsgIdentity *aIdentity)
{
  if (WeAreOffline())
    return NS_MSG_ERROR_OFFLINE;

  // Protect against being called whilst we're already sending.
  if (mSendingMessages)
  {
    NS_ERROR("nsMsgSendLater is already sending messages\n");
    return NS_ERROR_FAILURE;
  }

  nsresult rv;

  // XXX This code should be set up for multiple unsent folders, however we
  // don't support that at the moment, so for now just assume one folder.
  if (!mMessageFolder)
  {
    rv = GetUnsentMessagesFolder(nullptr,
                                 getter_AddRefs(mMessageFolder));
    NS_ENSURE_SUCCESS(rv, rv);
  }
  nsCOMPtr<nsIMsgDatabase> unsentDB;
  // Remember these in case we need to reparse the db.
  mUserInitiated = aUserInitiated;
  mIdentity = aIdentity;
  rv = ReparseDBIfNeeded(this);
  NS_ENSURE_SUCCESS(rv, rv);
  mIdentity = nullptr; // don't hold onto the identity since we're a service.

  nsCOMPtr<nsISimpleEnumerator> enumerator;
  rv = mMessageFolder->GetMessages(getter_AddRefs(enumerator));
  NS_ENSURE_SUCCESS(rv, rv);

  // copy all the elements in the enumerator into our isupports array....

  nsCOMPtr<nsISupports> currentItem;
  nsCOMPtr<nsIMsgDBHdr> messageHeader;
  bool hasMoreElements = false;
  while (NS_SUCCEEDED(enumerator->HasMoreElements(&hasMoreElements)) && hasMoreElements)
  {
    rv = enumerator->GetNext(getter_AddRefs(currentItem));
    if (NS_SUCCEEDED(rv))
    {
      messageHeader = do_QueryInterface(currentItem, &rv);
      if (NS_SUCCEEDED(rv))
      {
        if (aUserInitiated)
          // If the user initiated the send, add all messages
          mMessagesToSend.AppendObject(messageHeader);
        else
        {
          // Else just send those that are NOT marked as Queued.
          uint32_t flags;
          rv = messageHeader->GetFlags(&flags);
          if (NS_SUCCEEDED(rv) && !(flags & nsMsgMessageFlags::Queued))
            mMessagesToSend.AppendObject(messageHeader);
        }  
      }
    }
  }

  // Now get an enumerator for our array.
  rv = NS_NewArrayEnumerator(getter_AddRefs(mEnumerator), mMessagesToSend);
  NS_ENSURE_SUCCESS(rv, rv);

  // We're now sending messages so its time to signal that and reset our counts.
  mSendingMessages = true;
  mTotalSentSuccessfully = 0;
  mTotalSendCount = 0;

  // Notify the listeners that we are starting a send.
  NotifyListenersOnStartSending(mMessagesToSend.Count());

  return StartNextMailFileSend(NS_OK);
}

nsresult nsMsgSendLater::SetOrigMsgDisposition()
{
  if (!mMessage)
    return NS_ERROR_NULL_POINTER;

  // We're finished sending a queued message. We need to look at mMessage 
  // and see if we need to set replied/forwarded
  // flags for the original message that this message might be a reply to
  // or forward of.
  nsCString originalMsgURIs;
  nsCString queuedDisposition;
  mMessage->GetStringProperty(ORIG_URI_PROPERTY, getter_Copies(originalMsgURIs));
  mMessage->GetStringProperty(QUEUED_DISPOSITION_PROPERTY, getter_Copies(queuedDisposition));
  if (!queuedDisposition.IsEmpty())
  {
    nsTArray<nsCString> uriArray;
    ParseString(originalMsgURIs, ',', uriArray);
    for (uint32_t i = 0; i < uriArray.Length(); i++)
    {
      nsCOMPtr <nsIMsgDBHdr> msgHdr;
      nsresult rv = GetMsgDBHdrFromURI(uriArray[i].get(), getter_AddRefs(msgHdr));
      NS_ENSURE_SUCCESS(rv,rv);
      if (msgHdr)
      {
        // get the folder for the message resource
        nsCOMPtr<nsIMsgFolder> msgFolder;
        msgHdr->GetFolder(getter_AddRefs(msgFolder));
        if (msgFolder)
        {
          nsMsgDispositionState dispositionSetting = nsIMsgFolder::nsMsgDispositionState_Replied;
          if (queuedDisposition.Equals("forwarded"))
            dispositionSetting = nsIMsgFolder::nsMsgDispositionState_Forwarded;
          
          msgFolder->AddMessageDispositionState(msgHdr, dispositionSetting);
        }
      }
    }
  }
  return NS_OK;
}

nsresult
nsMsgSendLater::DeleteCurrentMessage()
{
  if (!mMessage)
  {
    NS_ERROR("nsMsgSendLater: Attempt to delete an already deleted message");
    return NS_OK;
  }

  // Get the composition fields interface
  nsCOMPtr<nsIMutableArray> msgArray(do_CreateInstance(NS_ARRAY_CONTRACTID));
  if (!msgArray)
    return NS_ERROR_FACTORY_NOT_LOADED;

  if (!mMessageFolder)
    return NS_ERROR_UNEXPECTED;

  msgArray->InsertElementAt(mMessage, 0, false);

  nsresult res = mMessageFolder->DeleteMessages(msgArray, nullptr, true, false, nullptr, false /*allowUndo*/);
  if (NS_FAILED(res))
    return NS_ERROR_FAILURE;

  // Null out the message so we don't try and delete it again.
  mMessage = nullptr;

  return NS_OK;
}

//
// This function parses the headers, and also deletes from the header block
// any headers which should not be delivered in mail, regardless of whether
// they were present in the queue file.  Such headers include: BCC, FCC,
// Sender, X-Mozilla-Status, X-Mozilla-News-Host, and Content-Length.
// (Content-Length is for the disk file only, and must not be allowed to
// escape onto the network, since it depends on the local linebreak
// representation.  Arguably, we could allow Lines to escape, but it's not
// required by NNTP.)
//
#define UNHEX(C) \
  ((C >= '0' && C <= '9') ? C - '0' : \
  ((C >= 'A' && C <= 'F') ? C - 'A' + 10 : \
        ((C >= 'a' && C <= 'f') ? C - 'a' + 10 : 0)))
nsresult
nsMsgSendLater::BuildHeaders()
{
  char *buf = m_headers;
  char *buf_end = buf + m_headersFP;

  PR_FREEIF(m_to);
  PR_FREEIF(m_bcc);
  PR_FREEIF(m_newsgroups);
  PR_FREEIF(m_newshost);
  PR_FREEIF(m_fcc);
  PR_FREEIF(mIdentityKey);
  PR_FREEIF(mAccountKey);
  m_flags = 0;

  while (buf < buf_end)
  {
    bool prune_p = false;
    bool    do_flags_p = false;
    char *colon = PL_strchr(buf, ':');
    char *end;
    char *value = 0;
    char **header = 0;
    char *header_start = buf;

    if (! colon)
      break;

    end = colon;
    while (end > buf && (*end == ' ' || *end == '\t'))
      end--;

    switch (buf [0])
    {
    case 'B': case 'b':
      if (!PL_strncasecmp ("BCC", buf, end - buf))
      {
        header = &m_bcc;
        prune_p = true;
      }
      break;
    case 'C': case 'c':
      if (!PL_strncasecmp ("CC", buf, end - buf))
      header = &m_to;
      else if (!PL_strncasecmp (HEADER_CONTENT_LENGTH, buf, end - buf))
      prune_p = true;
      break;
    case 'F': case 'f':
      if (!PL_strncasecmp ("FCC", buf, end - buf))
      {
        header = &m_fcc;
        prune_p = true;
      }
      break;
    case 'L': case 'l':
      if (!PL_strncasecmp ("Lines", buf, end - buf))
      prune_p = true;
      break;
    case 'N': case 'n':
      if (!PL_strncasecmp ("Newsgroups", buf, end - buf))
        header = &m_newsgroups;
      break;
    case 'S': case 's':
      if (!PL_strncasecmp ("Sender", buf, end - buf))
      prune_p = true;
      break;
    case 'T': case 't':
      if (!PL_strncasecmp ("To", buf, end - buf))
      header = &m_to;
      break;
    case 'X': case 'x':
      {
        int32_t headLen = PL_strlen(HEADER_X_MOZILLA_STATUS2);
        if (headLen == end - buf &&
          !PL_strncasecmp(HEADER_X_MOZILLA_STATUS2, buf, end - buf))
          prune_p = true;
        else if (PL_strlen(HEADER_X_MOZILLA_STATUS) == end - buf &&
          !PL_strncasecmp(HEADER_X_MOZILLA_STATUS, buf, end - buf))
          prune_p = do_flags_p = true;
        else if (!PL_strncasecmp(HEADER_X_MOZILLA_DRAFT_INFO, buf, end - buf))
          prune_p = true;
        else if (!PL_strncasecmp(HEADER_X_MOZILLA_KEYWORDS, buf, end - buf))
          prune_p = true;
        else if (!PL_strncasecmp(HEADER_X_MOZILLA_NEWSHOST, buf, end - buf))
        {
          prune_p = true;
          header = &m_newshost;
        }
        else if (!PL_strncasecmp(HEADER_X_MOZILLA_IDENTITY_KEY, buf, end - buf))
        {
          prune_p = true;
          header = &mIdentityKey;
        }
        else if (!PL_strncasecmp(HEADER_X_MOZILLA_ACCOUNT_KEY, buf, end - buf))
        {
          prune_p = true;
          header = &mAccountKey;
        }
        break;
      }
    }

    buf = colon + 1;
    while (*buf == ' ' || *buf == '\t')
    buf++;

    value = buf;

SEARCH_NEWLINE:
    while (*buf != 0 && *buf != '\r' && *buf != '\n')
      buf++;

    if (buf+1 >= buf_end)
      ;
    // If "\r\n " or "\r\n\t" is next, that doesn't terminate the header.
    else if (buf+2 < buf_end &&
         (buf[0] == '\r'  && buf[1] == '\n') &&
         (buf[2] == ' ' || buf[2] == '\t'))
    {
      buf += 3;
      goto SEARCH_NEWLINE;
    }
    // If "\r " or "\r\t" or "\n " or "\n\t" is next, that doesn't terminate
    // the header either. 
    else if ((buf[0] == '\r'  || buf[0] == '\n') &&
         (buf[1] == ' ' || buf[1] == '\t'))
    {
      buf += 2;
      goto SEARCH_NEWLINE;
    }

    if (header)
    {
      int L = buf - value;
      if (*header)
      {
        char *newh = (char*) PR_Realloc ((*header),
                         PL_strlen(*header) + L + 10);
        if (!newh) return NS_ERROR_OUT_OF_MEMORY;
        *header = newh;
        newh = (*header) + PL_strlen (*header);
        *newh++ = ',';
        *newh++ = ' ';
        memcpy(newh, value, L);
        newh [L] = 0;
      }
      else
      {
        *header = (char *) PR_Malloc(L+1);
        if (!*header) return NS_ERROR_OUT_OF_MEMORY;
        memcpy((*header), value, L);
        (*header)[L] = 0;
      }
    }
    else if (do_flags_p)
    {
      int i;
      char *s = value;
      PR_ASSERT(*s != ' ' && *s != '\t');
      m_flags = 0;
      for (i=0 ; i<4 ; i++) {
      m_flags = (m_flags << 4) | UNHEX(*s);
      s++;
      }
    }

    if (*buf == '\r' || *buf == '\n')
    {
      if (*buf == '\r' && buf[1] == '\n')
      buf++;
      buf++;
    }

    if (prune_p)
    {
      char *to = header_start;
      char *from = buf;
      while (from < buf_end)
      *to++ = *from++;
      buf = header_start;
      buf_end = to;
      m_headersFP = buf_end - m_headers;
    }
  }

  m_headers[m_headersFP++] = '\r';
  m_headers[m_headersFP++] = '\n';

  // Now we have parsed out all of the headers we need and we 
  // can proceed.
  return NS_OK;
}

nsresult
DoGrowBuffer(int32_t desired_size, int32_t element_size, int32_t quantum,
            char **buffer, int32_t *size)
{
  if (*size <= desired_size)
  {
    char *new_buf;
    int32_t increment = desired_size - *size;
    if (increment < quantum) // always grow by a minimum of N bytes 
      increment = quantum;
    
    new_buf = (*buffer
                ? (char *) PR_Realloc (*buffer, (*size + increment)
                * (element_size / sizeof(char)))
                : (char *) PR_Malloc ((*size + increment)
                * (element_size / sizeof(char))));
    if (! new_buf)
      return NS_ERROR_OUT_OF_MEMORY;
    *buffer = new_buf;
    *size += increment;
  }
  return NS_OK;
}

#define do_grow_headers(desired_size) \
  (((desired_size) >= m_headersSize) ? \
   DoGrowBuffer ((desired_size), sizeof(char), 1024, \
           &m_headers, &m_headersSize) \
   : NS_OK)

nsresult
nsMsgSendLater::DeliverQueuedLine(char *line, int32_t length)
{
  int32_t flength = length;
  
  m_bytesRead += length;
  
// convert existing newline to CRLF 
// Don't need this because the calling routine is taking care of it.
//  if (length > 0 && (line[length-1] == '\r' || 
//     (line[length-1] == '\n' && (length < 2 || line[length-2] != '\r'))))
//  {
//    line[length-1] = '\r';
//    line[length++] = '\n';
//  }
//
  //
  // We are going to check if we are looking at a "From - " line. If so, 
  // then just eat it and return NS_OK
  //
  if (!PL_strncasecmp(line, "From - ", 7))
    return NS_OK;

  if (m_inhead)
  {
    if (m_headersPosition == 0)
    {
      // This line is the first line in a header block.
      // Remember its position.
      m_headersPosition = m_position;
      
      // Also, since we're now processing the headers, clear out the
      // slots which we will parse data into, so that the values that
      // were used the last time around do not persist.
      
      // We must do that here, and not in the previous clause of this
      // `else' (the "I've just seen a `From ' line clause") because
      // that clause happens before delivery of the previous message is
      // complete, whereas this clause happens after the previous msg
      // has been delivered.  If we did this up there, then only the
      // last message in the folder would ever be able to be both
      // mailed and posted (or fcc'ed.)
      PR_FREEIF(m_to);
      PR_FREEIF(m_bcc);
      PR_FREEIF(m_newsgroups);
      PR_FREEIF(m_newshost);
      PR_FREEIF(m_fcc);
      PR_FREEIF(mIdentityKey);
    }
    
    if (line[0] == '\r' || line[0] == '\n' || line[0] == 0)
    {
      // End of headers.  Now parse them; open the temp file;
      // and write the appropriate subset of the headers out. 
      m_inhead = false;

      nsresult rv = MsgNewBufferedFileOutputStream(getter_AddRefs(mOutFile), mTempFile, -1, 00600);
      if (NS_FAILED(rv))
        return NS_MSG_ERROR_WRITING_FILE;

      nsresult status = BuildHeaders();
      if (NS_FAILED(status))
        return status;

      uint32_t n;
      rv = mOutFile->Write(m_headers, m_headersFP, &n);
      if (NS_FAILED(rv) || n != (uint32_t)m_headersFP)
        return NS_MSG_ERROR_WRITING_FILE;
    }
    else
    {
      // Otherwise, this line belongs to a header.  So append it to the
      // header data.
      
      if (!PL_strncasecmp (line, HEADER_X_MOZILLA_STATUS, PL_strlen(HEADER_X_MOZILLA_STATUS)))
        // Notice the position of the flags.
        m_flagsPosition = m_position;
      else if (m_headersFP == 0)
        m_flagsPosition = 0;
      
      nsresult status = do_grow_headers (length + m_headersFP + 10);
      if (NS_FAILED(status)) 
        return status;
      
      memcpy(m_headers + m_headersFP, line, length);
      m_headersFP += length;
    }
  }
  else
  {
    // This is a body line.  Write it to the file.
    PR_ASSERT(mOutFile);
    if (mOutFile)
    {
      uint32_t wrote;
      nsresult rv = mOutFile->Write(line, length, &wrote);
      if (NS_FAILED(rv) || wrote < (uint32_t) length) 
        return NS_MSG_ERROR_WRITING_FILE;
    }
  }
  
  m_position += flength;
  return NS_OK;
}

NS_IMETHODIMP
nsMsgSendLater::AddListener(nsIMsgSendLaterListener *aListener)
{
  NS_ENSURE_ARG_POINTER(aListener);
  mListenerArray.AppendElement(aListener);
  return NS_OK;
}

NS_IMETHODIMP
nsMsgSendLater::RemoveListener(nsIMsgSendLaterListener *aListener)
{
  NS_ENSURE_ARG_POINTER(aListener);
  return mListenerArray.RemoveElement(aListener) ? NS_OK : NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP
nsMsgSendLater::GetSendingMessages(bool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = mSendingMessages;
  return NS_OK;
}

#define NOTIFY_LISTENERS(propertyfunc_, params_) \
  PR_BEGIN_MACRO                                 \
  nsTObserverArray<nsCOMPtr<nsIMsgSendLaterListener> >::ForwardIterator iter(mListenerArray); \
  nsCOMPtr<nsIMsgSendLaterListener> listener;    \
  while (iter.HasMore()) {                       \
    listener = iter.GetNext();                   \
    listener->propertyfunc_ params_;             \
  }                                              \
  PR_END_MACRO

void
nsMsgSendLater::NotifyListenersOnStartSending(uint32_t aTotalMessageCount)
{
  NOTIFY_LISTENERS(OnStartSending, (aTotalMessageCount));
}

void
nsMsgSendLater::NotifyListenersOnMessageStartSending(uint32_t aCurrentMessage,
                                                     uint32_t aTotalMessage,
                                                     nsIMsgIdentity *aIdentity)
{
  NOTIFY_LISTENERS(OnMessageStartSending, (aCurrentMessage, aTotalMessage,
                                           mMessage, aIdentity));
}

void
nsMsgSendLater::NotifyListenersOnProgress(uint32_t aCurrentMessage,
                                          uint32_t aTotalMessage,
                                          uint32_t aSendPercent,
                                          uint32_t aCopyPercent)
{
  NOTIFY_LISTENERS(OnMessageSendProgress, (aCurrentMessage, aTotalMessage,
                                           aSendPercent, aCopyPercent));
}

void
nsMsgSendLater::NotifyListenersOnMessageSendError(uint32_t aCurrentMessage,
                                                  nsresult aStatus,
                                                  const PRUnichar *aMsg)
{
  NOTIFY_LISTENERS(OnMessageSendError, (aCurrentMessage, mMessage,
                                        aStatus, aMsg));
}

/**
 * This function is called to end sending of messages, it resets the send later
 * system and notifies the relevant parties that we have finished.
 */
void
nsMsgSendLater::EndSendMessages(nsresult aStatus, const PRUnichar *aMsg,
                                uint32_t aTotalTried, uint32_t aSuccessful)
{
  // Catch-all, we may have had an issue sending, so we may not be calling
  // StartNextMailFileSend to fully finish the sending. Therefore set
  // mSendingMessages to false here so that we don't think we're still trying
  // to send messages
  mSendingMessages = false;

  // Clear out our array of messages.
  mMessagesToSend.Clear();

  // We don't need to keep hold of the database now we've finished sending.
  (void)mMessageFolder->SetMsgDatabase(nullptr);

  // or the enumerator, temp file or output stream
  mEnumerator = nullptr;
  mTempFile = nullptr;
  mOutFile = nullptr;  

  NOTIFY_LISTENERS(OnStopSending, (aStatus, aMsg, aTotalTried, aSuccessful));

  // If we've got a shutdown listener, notify it that we've finished.
  if (mShutdownListener)
  {
    mShutdownListener->OnStopRunningUrl(nullptr, NS_OK);
    mShutdownListener = nullptr;
  }
}

/**
 * Called when the send part of sending a message is finished. This will set up
 * for the next step or "end" depending on the status.
 *
 * @param aStatus  The success or fail result of the send step.
 * @return         True if the copy process will continue, false otherwise.
 */
bool
nsMsgSendLater::OnSendStepFinished(nsresult aStatus)
{
  if (NS_SUCCEEDED(aStatus))
  {
    SetOrigMsgDisposition();
    DeleteCurrentMessage();

    // Send finished, so that is now 100%, copy to proceed...
    NotifyListenersOnProgress(mTotalSendCount, mMessagesToSend.Count(), 100, 0);

    ++mTotalSentSuccessfully;
    return true;
  }
  else
  {
    // XXX we don't currently get a message string from the send service.
    NotifyListenersOnMessageSendError(mTotalSendCount, aStatus, nullptr);
    nsresult rv = StartNextMailFileSend(aStatus);
    // if this is the last message we're sending, we should report
    // the status failure.
    if (NS_FAILED(rv))
      EndSendMessages(rv, nullptr, mTotalSendCount, mTotalSentSuccessfully);
  }
  return false;
}

/**
 * Called when the copy part of sending a message is finished. This will send
 * the next message or handle failure as appropriate.
 *
 * @param aStatus  The success or fail result of the copy step.
 */
void
nsMsgSendLater::OnCopyStepFinished(nsresult aStatus)
{
  // Regardless of the success of the copy we will still keep trying
  // to send the rest...
  nsresult rv = StartNextMailFileSend(aStatus);
  if (NS_FAILED(rv))
    EndSendMessages(rv, nullptr, mTotalSendCount, mTotalSentSuccessfully);
}

// XXX todo
// maybe this should just live in the account manager?
nsresult
nsMsgSendLater::GetIdentityFromKey(const char *aKey, nsIMsgIdentity  **aIdentity)
{
  NS_ENSURE_ARG_POINTER(aIdentity);

  nsresult rv;
  nsCOMPtr<nsIMsgAccountManager> accountManager = 
    do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);
 
  if (aKey)
  {
    nsCOMPtr<nsISupportsArray> identities;
    if (NS_SUCCEEDED(accountManager->GetAllIdentities(getter_AddRefs(identities))))
    {
      nsCOMPtr<nsIMsgIdentity> lookupIdentity;
      uint32_t          count = 0;

      identities->Count(&count);
      for (uint32_t i = 0; i < count; i++)
      {
        rv = identities->QueryElementAt(i, NS_GET_IID(nsIMsgIdentity),
                                  getter_AddRefs(lookupIdentity));
        if (NS_FAILED(rv))
          continue;

        nsCString key;
        lookupIdentity->GetKey(key);
        if (key.Equals(aKey))
        {
          NS_IF_ADDREF(*aIdentity = lookupIdentity);
          return NS_OK;
        }
      }
    }
  }

  // if no aKey, or we failed to find the identity from the key
  // use the identity from the default account.
  nsCOMPtr<nsIMsgAccount> defaultAccount;
  rv = accountManager->GetDefaultAccount(getter_AddRefs(defaultAccount));
  NS_ENSURE_SUCCESS(rv,rv);
  
  rv = defaultAccount->GetDefaultIdentity(aIdentity);
  NS_ENSURE_SUCCESS(rv,rv);
  return rv;
}

NS_IMETHODIMP
nsMsgSendLater::OnItemAdded(nsIMsgFolder *aParentItem, nsISupports *aItem)
{
  // No need to trigger if timer is already set
  if (mTimerSet)
    return NS_OK;

  // XXX only trigger for non-queued headers

  // Items from this function return NS_OK because the callee won't care about
  // the result anyway.
  nsresult rv;
  if (!mTimer)
  {
    mTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
    NS_ENSURE_SUCCESS(rv, NS_OK);
  }

  rv = mTimer->Init(static_cast<nsIObserver*>(this), kInitialMessageSendTime,
                    nsITimer::TYPE_ONE_SHOT);
  NS_ENSURE_SUCCESS(rv, NS_OK);

  mTimerSet = true;

  return NS_OK;
}

NS_IMETHODIMP
nsMsgSendLater::OnItemRemoved(nsIMsgFolder *aParentItem, nsISupports *aItem)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMsgSendLater::OnItemPropertyChanged(nsIMsgFolder *aItem, nsIAtom *aProperty,
                                      const char* aOldValue,
                                      const char* aNewValue)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMsgSendLater::OnItemIntPropertyChanged(nsIMsgFolder *aItem, nsIAtom *aProperty,
                                         int32_t aOldValue, int32_t aNewValue)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMsgSendLater::OnItemBoolPropertyChanged(nsIMsgFolder *aItem, nsIAtom *aProperty,
                                          bool aOldValue, bool aNewValue)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMsgSendLater::OnItemUnicharPropertyChanged(nsIMsgFolder *aItem,
                                             nsIAtom *aProperty,
                                             const PRUnichar* aOldValue,
                                             const PRUnichar* aNewValue)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMsgSendLater::OnItemPropertyFlagChanged(nsIMsgDBHdr *aItem, nsIAtom *aProperty,
                                          uint32_t aOldValue, uint32_t aNewValue)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMsgSendLater::OnItemEvent(nsIMsgFolder* aItem, nsIAtom *aEvent)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMsgSendLater::GetNeedsToRunTask(bool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = mSendingMessages;
  return NS_OK;
}

NS_IMETHODIMP
nsMsgSendLater::DoShutdownTask(nsIUrlListener *aListener, nsIMsgWindow *aWindow,
                               bool *aResult)
{
  if (mTimer)
    mTimer->Cancel();
  // If we're already sending messages, nothing to do, but save the shutdown
  // listener until we've finished.
  if (mSendingMessages)
  {
    mShutdownListener = aListener;
    return NS_OK;
  }
  // Else we have pending messages, we need to throw up a dialog to find out
  // if to send them or not.

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMsgSendLater::GetCurrentTaskName(nsAString &aResult)
{
  // XXX Bug 440794 will localize this, left as non-localized whilst we decide
  // on the actual strings and try out the UI.
  aResult = NS_LITERAL_STRING("Sending Messages");
  return NS_OK;
}
