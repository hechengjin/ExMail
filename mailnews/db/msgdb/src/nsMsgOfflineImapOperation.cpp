/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZ_LOGGING
// This has to be before the pre-compiled header
#define FORCE_PR_LOG /* Allow logging in the release build */
#endif

#include "msgCore.h"
#include "nsMsgOfflineImapOperation.h"
#include "nsMsgUtils.h"

PRLogModuleInfo *IMAPOffline;

/* Implementation file */
NS_IMPL_ISUPPORTS1(nsMsgOfflineImapOperation, nsIMsgOfflineImapOperation)

// property names for offine imap operation fields.
#define PROP_OPERATION "op"
#define PROP_OPERATION_FLAGS "opFlags"
#define PROP_NEW_FLAGS "newFlags"
#define PROP_MESSAGE_KEY "msgKey"
#define PROP_SRC_MESSAGE_KEY "srcMsgKey"
#define PROP_SRC_FOLDER_URI "srcFolderURI"
#define PROP_MOVE_DEST_FOLDER_URI "moveDest"
#define PROP_NUM_COPY_DESTS "numCopyDests"
#define PROP_COPY_DESTS "copyDests" // how to delimit these? Or should we do the "dest1","dest2" etc trick? But then we'd need to shuffle
                                    // them around since we delete off the front first.
#define PROP_KEYWORD_ADD "addedKeywords"
#define PROP_KEYWORD_REMOVE "removedKeywords"
#define PROP_MSG_SIZE "msgSize"
#define PROP_PLAYINGBACK "inPlayback"

nsMsgOfflineImapOperation::nsMsgOfflineImapOperation(nsMsgDatabase *db, nsIMdbRow *row)
{
  NS_ASSERTION(db, "can't have null db");
  NS_ASSERTION(row, "can't have null row");
  m_operation = 0;
  m_operationFlags = 0;
  m_messageKey = nsMsgKey_None;
  m_sourceMessageKey = nsMsgKey_None;
  m_mdb = db;
  NS_ADDREF(m_mdb);
  m_mdbRow = row;
  m_newFlags = 0;
  m_mdb->GetUint32Property(m_mdbRow, PROP_OPERATION, (uint32_t *) &m_operation, 0);
  m_mdb->GetUint32Property(m_mdbRow, PROP_MESSAGE_KEY, &m_messageKey, 0);
  m_mdb->GetUint32Property(m_mdbRow, PROP_OPERATION_FLAGS, &m_operationFlags, 0);
  m_mdb->GetUint32Property(m_mdbRow, PROP_NEW_FLAGS, (uint32_t *) &m_newFlags, 0);
}

nsMsgOfflineImapOperation::~nsMsgOfflineImapOperation()
{
  // clear the row first, in case we're holding the last reference
  // to the db.
  m_mdbRow = nullptr;
  NS_IF_RELEASE(m_mdb);
}

/* attribute nsOfflineImapOperationType operation; */
NS_IMETHODIMP nsMsgOfflineImapOperation::GetOperation(nsOfflineImapOperationType *aOperation)
{
  NS_ENSURE_ARG(aOperation);
  *aOperation = m_operation;
  return NS_OK;
}

NS_IMETHODIMP nsMsgOfflineImapOperation::SetOperation(nsOfflineImapOperationType aOperation)
{
  if (PR_LOG_TEST(IMAPOffline, PR_LOG_ALWAYS))
    PR_LOG(IMAPOffline, PR_LOG_ALWAYS, ("msg id %x setOperation was %x add %x", m_messageKey, m_operation, aOperation));

  m_operation |= aOperation;
  return m_mdb->SetUint32Property(m_mdbRow, PROP_OPERATION, m_operation);
}

/* void clearOperation (in nsOfflineImapOperationType operation); */
NS_IMETHODIMP nsMsgOfflineImapOperation::ClearOperation(nsOfflineImapOperationType aOperation)
{
  if (PR_LOG_TEST(IMAPOffline, PR_LOG_ALWAYS))
    PR_LOG(IMAPOffline, PR_LOG_ALWAYS, ("msg id %x clearOperation was %x clear %x", m_messageKey, m_operation, aOperation));
  m_operation &= ~aOperation;
  switch (aOperation)
  {
  case kMsgMoved:
  case kAppendTemplate:
  case kAppendDraft:
    m_moveDestination.Truncate();
    break;
  case kMsgCopy:
    m_copyDestinations.RemoveElementAt(0);
    break;
  }
  return m_mdb->SetUint32Property(m_mdbRow, PROP_OPERATION, m_operation);
}

/* attribute nsMsgKey messageKey; */
NS_IMETHODIMP nsMsgOfflineImapOperation::GetMessageKey(nsMsgKey *aMessageKey)
{
  NS_ENSURE_ARG(aMessageKey);
  *aMessageKey = m_messageKey;
  return NS_OK;
}

NS_IMETHODIMP nsMsgOfflineImapOperation::SetMessageKey(nsMsgKey aMessageKey)
{
  m_messageKey = aMessageKey;
  return m_mdb->SetUint32Property(m_mdbRow, PROP_MESSAGE_KEY, m_messageKey);
}

/* attribute nsMsgKey srcMessageKey; */
NS_IMETHODIMP nsMsgOfflineImapOperation::GetSrcMessageKey(nsMsgKey *aMessageKey)
{
  NS_ENSURE_ARG(aMessageKey);
  return m_mdb->GetUint32Property(m_mdbRow, PROP_SRC_MESSAGE_KEY, aMessageKey, nsMsgKey_None);
}

NS_IMETHODIMP nsMsgOfflineImapOperation::SetSrcMessageKey(nsMsgKey aMessageKey)
{
  m_messageKey = aMessageKey;
  return m_mdb->SetUint32Property(m_mdbRow, PROP_SRC_MESSAGE_KEY, m_messageKey);
}

/* attribute imapMessageFlagsType flagOperation; */
NS_IMETHODIMP nsMsgOfflineImapOperation::GetFlagOperation(imapMessageFlagsType *aFlagOperation)
{
  NS_ENSURE_ARG(aFlagOperation);
  *aFlagOperation = m_operationFlags;
  return NS_OK;
}

NS_IMETHODIMP nsMsgOfflineImapOperation::SetFlagOperation(imapMessageFlagsType aFlagOperation)
{
  if (PR_LOG_TEST(IMAPOffline, PR_LOG_ALWAYS))
    PR_LOG(IMAPOffline, PR_LOG_ALWAYS, ("msg id %x setFlagOperation was %x add %x", m_messageKey, m_operationFlags, aFlagOperation));
  SetOperation(kFlagsChanged);
  nsresult rv = SetNewFlags(aFlagOperation);
  NS_ENSURE_SUCCESS(rv, rv);
  m_operationFlags |= aFlagOperation;
  return m_mdb->SetUint32Property(m_mdbRow, PROP_OPERATION_FLAGS, m_operationFlags);
}

/* attribute imapMessageFlagsType flagOperation; */
NS_IMETHODIMP nsMsgOfflineImapOperation::GetNewFlags(imapMessageFlagsType *aNewFlags)
{
  NS_ENSURE_ARG(aNewFlags);
  uint32_t flags;
  nsresult rv = m_mdb->GetUint32Property(m_mdbRow, PROP_NEW_FLAGS, &flags, 0);
  *aNewFlags = m_newFlags = (imapMessageFlagsType) flags;
  return rv;
}

NS_IMETHODIMP nsMsgOfflineImapOperation::SetNewFlags(imapMessageFlagsType aNewFlags)
{
  if (PR_LOG_TEST(IMAPOffline, PR_LOG_ALWAYS) && m_newFlags != aNewFlags)
    PR_LOG(IMAPOffline, PR_LOG_ALWAYS, ("msg id %x SetNewFlags was %x to %x", m_messageKey, m_newFlags, aNewFlags));
  m_newFlags = aNewFlags;
  return m_mdb->SetUint32Property(m_mdbRow, PROP_NEW_FLAGS, m_newFlags);
}


/* attribute string destinationFolderURI; */
NS_IMETHODIMP nsMsgOfflineImapOperation::GetDestinationFolderURI(char * *aDestinationFolderURI)
{
  NS_ENSURE_ARG(aDestinationFolderURI);
  (void) m_mdb->GetProperty(m_mdbRow, PROP_MOVE_DEST_FOLDER_URI, getter_Copies(m_moveDestination));
  *aDestinationFolderURI = ToNewCString(m_moveDestination);
  return (*aDestinationFolderURI) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP nsMsgOfflineImapOperation::SetDestinationFolderURI(const char * aDestinationFolderURI)
{
  if (PR_LOG_TEST(IMAPOffline, PR_LOG_ALWAYS))
    PR_LOG(IMAPOffline, PR_LOG_ALWAYS, ("msg id %x SetDestinationFolderURI to %s", m_messageKey, aDestinationFolderURI));
  m_moveDestination = aDestinationFolderURI ? aDestinationFolderURI : 0;
  return m_mdb->SetProperty(m_mdbRow, PROP_MOVE_DEST_FOLDER_URI, aDestinationFolderURI);
}

/* attribute string sourceFolderURI; */
NS_IMETHODIMP nsMsgOfflineImapOperation::GetSourceFolderURI(char * *aSourceFolderURI)
{
  NS_ENSURE_ARG(aSourceFolderURI);
  nsresult rv = m_mdb->GetProperty(m_mdbRow, PROP_SRC_FOLDER_URI, getter_Copies(m_sourceFolder));
  *aSourceFolderURI = ToNewCString(m_sourceFolder);
  return rv;
}

NS_IMETHODIMP nsMsgOfflineImapOperation::SetSourceFolderURI(const char * aSourceFolderURI)
{
  m_sourceFolder = aSourceFolderURI ? aSourceFolderURI : 0;
  SetOperation(kMoveResult);

  return m_mdb->SetProperty(m_mdbRow, PROP_SRC_FOLDER_URI, aSourceFolderURI);
}

/* attribute string keyword; */
NS_IMETHODIMP nsMsgOfflineImapOperation::GetKeywordsToAdd(char * *aKeywords)
{
  NS_ENSURE_ARG(aKeywords);
  nsresult rv = m_mdb->GetProperty(m_mdbRow, PROP_KEYWORD_ADD, getter_Copies(m_keywordsToAdd));
  *aKeywords = ToNewCString(m_keywordsToAdd);
  return rv;
}

NS_IMETHODIMP nsMsgOfflineImapOperation::AddKeywordToAdd(const char * aKeyword)
{
  SetOperation(kAddKeywords);
  return AddKeyword(aKeyword, m_keywordsToAdd, PROP_KEYWORD_ADD, m_keywordsToRemove, PROP_KEYWORD_REMOVE);
}

NS_IMETHODIMP nsMsgOfflineImapOperation::GetKeywordsToRemove(char * *aKeywords)
{
  NS_ENSURE_ARG(aKeywords);
  nsresult rv = m_mdb->GetProperty(m_mdbRow, PROP_KEYWORD_REMOVE, getter_Copies(m_keywordsToRemove));
  *aKeywords = ToNewCString(m_keywordsToRemove);
  return rv;
}

nsresult nsMsgOfflineImapOperation::AddKeyword(const char *aKeyword, nsCString &addList, const char *addProp,
                                               nsCString &removeList, const char *removeProp)
{
  int32_t startOffset, keywordLength;
  if (!MsgFindKeyword(nsDependentCString(aKeyword), addList, &startOffset, &keywordLength))
  {
    if (!addList.IsEmpty())
      addList.Append(' ');
    addList.Append(aKeyword);
  }
  // if the keyword we're removing was in the list of keywords to add,
  // cut it from that list.
  if (MsgFindKeyword(nsDependentCString(aKeyword), removeList, &startOffset, &keywordLength))
  {
    removeList.Cut(startOffset, keywordLength);
    m_mdb->SetProperty(m_mdbRow, removeProp, removeList.get());
  }
  return m_mdb->SetProperty(m_mdbRow, addProp, addList.get());
}

NS_IMETHODIMP nsMsgOfflineImapOperation::AddKeywordToRemove(const char * aKeyword)
{
  SetOperation(kRemoveKeywords);
  return AddKeyword(aKeyword, m_keywordsToRemove, PROP_KEYWORD_REMOVE, m_keywordsToAdd, PROP_KEYWORD_ADD);
}


NS_IMETHODIMP nsMsgOfflineImapOperation::AddMessageCopyOperation(const char *destinationBox)
{
  SetOperation(kMsgCopy);
  nsCAutoString newDest(destinationBox);
  nsresult rv = GetCopiesFromDB();
  NS_ENSURE_SUCCESS(rv, rv);
  m_copyDestinations.AppendElement(newDest);
  return SetCopiesToDB();
}

// we write out the folders as one string, separated by 0x1.
#define FOLDER_SEP_CHAR '\001'

nsresult nsMsgOfflineImapOperation::GetCopiesFromDB()
{
  nsCString copyDests;
  m_copyDestinations.Clear();
  nsresult rv = m_mdb->GetProperty(m_mdbRow, PROP_COPY_DESTS, getter_Copies(copyDests));
  // use 0x1 as the delimiter between folder names since it's not a legal character
  if (NS_SUCCEEDED(rv) && !copyDests.IsEmpty())
  {
    int32_t curCopyDestStart = 0;
    int32_t nextCopyDestPos = 0;

    while (nextCopyDestPos != -1)
    {
      nsCString curDest;
      nextCopyDestPos = copyDests.FindChar(FOLDER_SEP_CHAR, curCopyDestStart);
      if (nextCopyDestPos > 0)
        curDest = Substring(copyDests, curCopyDestStart, nextCopyDestPos - curCopyDestStart);
      else
        curDest = Substring(copyDests, curCopyDestStart, copyDests.Length() - curCopyDestStart);
      curCopyDestStart = nextCopyDestPos + 1;
      m_copyDestinations.AppendElement(curDest);
    }
  }
  return rv;
}

nsresult nsMsgOfflineImapOperation::SetCopiesToDB()
{
  nsCAutoString copyDests;

  // use 0x1 as the delimiter between folders
  for (uint32_t i = 0; i < m_copyDestinations.Length(); i++)
  {
    if (i > 0)
      copyDests.Append(FOLDER_SEP_CHAR);
    copyDests.Append(m_copyDestinations.ElementAt(i));
  }
  return m_mdb->SetProperty(m_mdbRow, PROP_COPY_DESTS, copyDests.get());
}

/* attribute long numberOfCopies; */
NS_IMETHODIMP nsMsgOfflineImapOperation::GetNumberOfCopies(int32_t *aNumberOfCopies)
{
  NS_ENSURE_ARG(aNumberOfCopies);
  nsresult rv = GetCopiesFromDB();
  NS_ENSURE_SUCCESS(rv, rv);
  *aNumberOfCopies = m_copyDestinations.Length();
  return NS_OK;
}

/* string getCopyDestination (in long copyIndex); */
NS_IMETHODIMP nsMsgOfflineImapOperation::GetCopyDestination(int32_t copyIndex, char **retval)
{
  NS_ENSURE_ARG(retval);
  nsresult rv = GetCopiesFromDB();
  NS_ENSURE_SUCCESS(rv, rv);
  if (copyIndex >= (int32_t)m_copyDestinations.Length())
    return NS_ERROR_ILLEGAL_VALUE;
  *retval = ToNewCString(m_copyDestinations.ElementAt(copyIndex));
  return (*retval) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

/* attribute unsigned log msgSize; */
NS_IMETHODIMP nsMsgOfflineImapOperation::GetMsgSize(uint32_t *aMsgSize)
{
  NS_ENSURE_ARG(aMsgSize);
  return m_mdb->GetUint32Property(m_mdbRow, PROP_MSG_SIZE, aMsgSize, 0);
}

NS_IMETHODIMP nsMsgOfflineImapOperation::SetMsgSize(uint32_t aMsgSize)
{
  return m_mdb->SetUint32Property(m_mdbRow, PROP_MSG_SIZE, aMsgSize);
}

NS_IMETHODIMP nsMsgOfflineImapOperation::SetPlayingBack(bool aPlayingBack)
{
  return m_mdb->SetBooleanProperty(m_mdbRow, PROP_PLAYINGBACK, aPlayingBack);
}

NS_IMETHODIMP nsMsgOfflineImapOperation::GetPlayingBack(bool *aPlayingBack)
{
  NS_ENSURE_ARG(aPlayingBack);
  return m_mdb->GetBooleanProperty(m_mdbRow, PROP_PLAYINGBACK, aPlayingBack);
}


void nsMsgOfflineImapOperation::Log(PRLogModuleInfo *logFile)
{
  if (!IMAPOffline)
    IMAPOffline = PR_NewLogModule("IMAPOFFLINE");
  if (!PR_LOG_TEST(IMAPOffline, PR_LOG_ALWAYS))
    return;
  //  const long kMoveResult              = 0x8;
  //  const long kAppendDraft           = 0x10;
  //  const long kAddedHeader           = 0x20;
  //  const long kDeletedMsg              = 0x40;
  //  const long kMsgMarkedDeleted = 0x80;
  //  const long kAppendTemplate      = 0x100;
  //  const long kDeleteAllMsgs          = 0x200;
  if (m_operation & nsIMsgOfflineImapOperation::kFlagsChanged)
    PR_LOG(IMAPOffline, PR_LOG_ALWAYS, ("msg id %x changeFlag:%x", m_messageKey, m_newFlags));
  if (m_operation & nsIMsgOfflineImapOperation::kMsgMoved)
  {
    nsCString moveDestFolder;
    GetDestinationFolderURI(getter_Copies(moveDestFolder));
    PR_LOG(IMAPOffline, PR_LOG_ALWAYS, ("msg id %x moveTo:%s", m_messageKey, moveDestFolder.get()));
  }
  if (m_operation & nsIMsgOfflineImapOperation::kMsgCopy)
  {
    nsCString copyDests;
    m_mdb->GetProperty(m_mdbRow, PROP_COPY_DESTS, getter_Copies(copyDests));
    PR_LOG(IMAPOffline, PR_LOG_ALWAYS, ("msg id %x moveTo:%s", m_messageKey, copyDests.get()));
  }
  if (m_operation & nsIMsgOfflineImapOperation::kAppendDraft)
    PR_LOG(IMAPOffline, PR_LOG_ALWAYS, ("msg id %x append draft", m_messageKey));
  if (m_operation & nsIMsgOfflineImapOperation::kAddKeywords)
    PR_LOG(IMAPOffline, PR_LOG_ALWAYS, ("msg id %x add keyword:%s", m_messageKey, m_keywordsToAdd.get()));
  if (m_operation & nsIMsgOfflineImapOperation::kRemoveKeywords)
    PR_LOG(IMAPOffline, PR_LOG_ALWAYS, ("msg id %x remove keyword:%s", m_messageKey, m_keywordsToRemove.get()));
}
