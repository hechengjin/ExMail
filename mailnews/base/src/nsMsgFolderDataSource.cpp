/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "msgCore.h"    // precompiled header...
#include "prlog.h"
#include "prmem.h"
#include "nsMsgFolderDataSource.h"
#include "nsMsgFolderFlags.h"

#include "nsMsgUtils.h"
#include "nsMsgRDFUtils.h"

#include "rdf.h"
#include "nsIRDFService.h"
#include "nsRDFCID.h"
#include "nsIRDFNode.h"
#include "nsEnumeratorUtils.h"

#include "nsStringGlue.h"
#include "nsCOMPtr.h"

#include "nsIMsgMailSession.h"
#include "nsIMsgCopyService.h"
#include "nsMsgBaseCID.h"
#include "nsIInputStream.h"
#include "nsIMsgHdr.h"
#include "nsTraceRefcnt.h"
#include "nsIMsgFolder.h" // TO include biffState enum. Change to bool later...
#include "nsIMutableArray.h"
#include "nsIPop3IncomingServer.h"
#include "nsINntpIncomingServer.h"
#include "nsTextFormatter.h"
#include "nsIStringBundle.h"
#include "nsIPrompt.h"
#include "nsIMsgAccountManager.h"
#include "nsArrayEnumerator.h"
#include "nsArrayUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsMemory.h"
#include "nsMsgUtils.h"
#include "mozilla/Services.h"

#define MESSENGER_STRING_URL       "chrome://messenger/locale/messenger.properties"

nsIRDFResource* nsMsgFolderDataSource::kNC_Child = nullptr;
nsIRDFResource* nsMsgFolderDataSource::kNC_Folder= nullptr;
nsIRDFResource* nsMsgFolderDataSource::kNC_Name= nullptr;
nsIRDFResource* nsMsgFolderDataSource::kNC_Open = nullptr;
nsIRDFResource* nsMsgFolderDataSource::kNC_FolderTreeName= nullptr;
nsIRDFResource* nsMsgFolderDataSource::kNC_FolderTreeSimpleName= nullptr;
nsIRDFResource* nsMsgFolderDataSource::kNC_NameSort= nullptr;
nsIRDFResource* nsMsgFolderDataSource::kNC_FolderTreeNameSort= nullptr;
nsIRDFResource* nsMsgFolderDataSource::kNC_SpecialFolder= nullptr;
nsIRDFResource* nsMsgFolderDataSource::kNC_ServerType = nullptr;
nsIRDFResource* nsMsgFolderDataSource::kNC_IsDeferred = nullptr;
nsIRDFResource* nsMsgFolderDataSource::kNC_CanCreateFoldersOnServer = nullptr;
nsIRDFResource* nsMsgFolderDataSource::kNC_CanFileMessagesOnServer = nullptr;
nsIRDFResource* nsMsgFolderDataSource::kNC_IsServer = nullptr;
nsIRDFResource* nsMsgFolderDataSource::kNC_IsSecure = nullptr;
nsIRDFResource* nsMsgFolderDataSource::kNC_CanSubscribe = nullptr;
nsIRDFResource* nsMsgFolderDataSource::kNC_SupportsOffline = nullptr;
nsIRDFResource* nsMsgFolderDataSource::kNC_CanFileMessages = nullptr;
nsIRDFResource* nsMsgFolderDataSource::kNC_CanCreateSubfolders = nullptr;
nsIRDFResource* nsMsgFolderDataSource::kNC_CanRename = nullptr;
nsIRDFResource* nsMsgFolderDataSource::kNC_CanCompact = nullptr;
nsIRDFResource* nsMsgFolderDataSource::kNC_TotalMessages= nullptr;
nsIRDFResource* nsMsgFolderDataSource::kNC_TotalUnreadMessages= nullptr;
nsIRDFResource* nsMsgFolderDataSource::kNC_FolderSize = nullptr;
nsIRDFResource* nsMsgFolderDataSource::kNC_Charset = nullptr;
nsIRDFResource* nsMsgFolderDataSource::kNC_BiffState = nullptr;
nsIRDFResource* nsMsgFolderDataSource::kNC_HasUnreadMessages = nullptr;
nsIRDFResource* nsMsgFolderDataSource::kNC_NewMessages = nullptr;
nsIRDFResource* nsMsgFolderDataSource::kNC_SubfoldersHaveUnreadMessages = nullptr;
nsIRDFResource* nsMsgFolderDataSource::kNC_NoSelect = nullptr;
nsIRDFResource* nsMsgFolderDataSource::kNC_VirtualFolder = nullptr;
nsIRDFResource* nsMsgFolderDataSource::kNC_InVFEditSearchScope = nullptr;
nsIRDFResource* nsMsgFolderDataSource::kNC_ImapShared = nullptr;
nsIRDFResource* nsMsgFolderDataSource::kNC_Synchronize = nullptr;
nsIRDFResource* nsMsgFolderDataSource::kNC_SyncDisabled = nullptr;
nsIRDFResource* nsMsgFolderDataSource::kNC_CanSearchMessages = nullptr;
nsIRDFResource* nsMsgFolderDataSource::kNC_UnreadFolders = nullptr;
nsIRDFResource* nsMsgFolderDataSource::kNC_FavoriteFolders = nullptr;
nsIRDFResource* nsMsgFolderDataSource::kNC_RecentFolders = nullptr;

// commands
nsIRDFResource* nsMsgFolderDataSource::kNC_Delete= nullptr;
nsIRDFResource* nsMsgFolderDataSource::kNC_ReallyDelete= nullptr;
nsIRDFResource* nsMsgFolderDataSource::kNC_NewFolder= nullptr;
nsIRDFResource* nsMsgFolderDataSource::kNC_GetNewMessages= nullptr;
nsIRDFResource* nsMsgFolderDataSource::kNC_Copy= nullptr;
nsIRDFResource* nsMsgFolderDataSource::kNC_Move= nullptr;
nsIRDFResource* nsMsgFolderDataSource::kNC_CopyFolder= nullptr;
nsIRDFResource* nsMsgFolderDataSource::kNC_MoveFolder= nullptr;
nsIRDFResource* nsMsgFolderDataSource::kNC_MarkAllMessagesRead= nullptr;
nsIRDFResource* nsMsgFolderDataSource::kNC_Compact= nullptr;
nsIRDFResource* nsMsgFolderDataSource::kNC_CompactAll= nullptr;
nsIRDFResource* nsMsgFolderDataSource::kNC_Rename= nullptr;
nsIRDFResource* nsMsgFolderDataSource::kNC_EmptyTrash= nullptr;

nsrefcnt nsMsgFolderDataSource::gFolderResourceRefCnt = 0;

nsIAtom * nsMsgFolderDataSource::kBiffStateAtom = nullptr;
nsIAtom * nsMsgFolderDataSource::kSortOrderAtom = nullptr;
nsIAtom * nsMsgFolderDataSource::kNewMessagesAtom = nullptr;
nsIAtom * nsMsgFolderDataSource::kTotalMessagesAtom = nullptr;
nsIAtom * nsMsgFolderDataSource::kTotalUnreadMessagesAtom = nullptr;
nsIAtom * nsMsgFolderDataSource::kFolderSizeAtom = nullptr;
nsIAtom * nsMsgFolderDataSource::kNameAtom = nullptr;
nsIAtom * nsMsgFolderDataSource::kSynchronizeAtom = nullptr;
nsIAtom * nsMsgFolderDataSource::kOpenAtom = nullptr;
nsIAtom * nsMsgFolderDataSource::kIsDeferredAtom = nullptr;
nsIAtom * nsMsgFolderDataSource::kIsSecureAtom = nullptr;
nsIAtom * nsMsgFolderDataSource::kCanFileMessagesAtom = nullptr;
nsIAtom * nsMsgFolderDataSource::kInVFEditSearchScopeAtom = nullptr;

static const uint32_t kDisplayBlankCount = 0xFFFFFFFE;
static const uint32_t kDisplayQuestionCount = 0xFFFFFFFF;

nsMsgFolderDataSource::nsMsgFolderDataSource()
{
  // one-time initialization here
  nsIRDFService* rdf = getRDFService();

  if (gFolderResourceRefCnt++ == 0) {
    nsCOMPtr<nsIStringBundle> sMessengerStringBundle;

    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_CHILD),   &kNC_Child);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_FOLDER),  &kNC_Folder);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_NAME),    &kNC_Name);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_OPEN),    &kNC_Open);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_FOLDERTREENAME),    &kNC_FolderTreeName);
    rdf->GetResource(NS_LITERAL_CSTRING("mailnewsunreadfolders:/"),    &kNC_UnreadFolders);
    rdf->GetResource(NS_LITERAL_CSTRING("mailnewsfavefolders:/"),    &kNC_FavoriteFolders);
    rdf->GetResource(NS_LITERAL_CSTRING("mailnewsrecentfolders:/"),    &kNC_RecentFolders);

    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_FOLDERTREESIMPLENAME),    &kNC_FolderTreeSimpleName);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_NAME_SORT),    &kNC_NameSort);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_FOLDERTREENAME_SORT),    &kNC_FolderTreeNameSort);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_SPECIALFOLDER), &kNC_SpecialFolder);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_SERVERTYPE), &kNC_ServerType);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_ISDEFERRED),&kNC_IsDeferred);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_CANCREATEFOLDERSONSERVER), &kNC_CanCreateFoldersOnServer);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_CANFILEMESSAGESONSERVER), &kNC_CanFileMessagesOnServer);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_ISSERVER), &kNC_IsServer);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_ISSECURE), &kNC_IsSecure);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_CANSUBSCRIBE), &kNC_CanSubscribe);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_SUPPORTSOFFLINE), &kNC_SupportsOffline);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_CANFILEMESSAGES), &kNC_CanFileMessages);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_CANCREATESUBFOLDERS), &kNC_CanCreateSubfolders);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_CANRENAME), &kNC_CanRename);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_CANCOMPACT), &kNC_CanCompact);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_TOTALMESSAGES), &kNC_TotalMessages);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_TOTALUNREADMESSAGES), &kNC_TotalUnreadMessages);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_FOLDERSIZE), &kNC_FolderSize);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_CHARSET), &kNC_Charset);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_BIFFSTATE), &kNC_BiffState);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_HASUNREADMESSAGES), &kNC_HasUnreadMessages);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_NEWMESSAGES), &kNC_NewMessages);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_SUBFOLDERSHAVEUNREADMESSAGES), &kNC_SubfoldersHaveUnreadMessages);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_NOSELECT), &kNC_NoSelect);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_VIRTUALFOLDER), &kNC_VirtualFolder);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_INVFEDITSEARCHSCOPE), &kNC_InVFEditSearchScope);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_IMAPSHARED), &kNC_ImapShared);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_SYNCHRONIZE), &kNC_Synchronize);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_SYNCDISABLED), &kNC_SyncDisabled);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_CANSEARCHMESSAGES), &kNC_CanSearchMessages);

    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_DELETE), &kNC_Delete);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_REALLY_DELETE), &kNC_ReallyDelete);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_NEWFOLDER), &kNC_NewFolder);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_GETNEWMESSAGES), &kNC_GetNewMessages);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_COPY), &kNC_Copy);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_MOVE), &kNC_Move);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_COPYFOLDER), &kNC_CopyFolder);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_MOVEFOLDER), &kNC_MoveFolder);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_MARKALLMESSAGESREAD),
                             &kNC_MarkAllMessagesRead);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_COMPACT), &kNC_Compact);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_COMPACTALL), &kNC_CompactAll);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_RENAME), &kNC_Rename);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_EMPTYTRASH), &kNC_EmptyTrash);

    kTotalMessagesAtom           = MsgNewAtom("TotalMessages");
    kTotalUnreadMessagesAtom     = MsgNewAtom("TotalUnreadMessages");
    kFolderSizeAtom              = MsgNewAtom("FolderSize");
    kBiffStateAtom               = MsgNewAtom("BiffState");
    kSortOrderAtom               = MsgNewAtom("SortOrder");
    kNewMessagesAtom             = MsgNewAtom("NewMessages");
    kNameAtom                    = MsgNewAtom("Name");
    kSynchronizeAtom             = MsgNewAtom("Synchronize");
    kOpenAtom                    = MsgNewAtom("open");
    kIsDeferredAtom              = MsgNewAtom("isDeferred");
    kIsSecureAtom                = MsgNewAtom("isSecure");
    kCanFileMessagesAtom         = MsgNewAtom("canFileMessages");
    kInVFEditSearchScopeAtom     = MsgNewAtom("inVFEditSearchScope");
  }

  CreateLiterals(rdf);
  CreateArcsOutEnumerator();
}

nsMsgFolderDataSource::~nsMsgFolderDataSource (void)
{
  if (--gFolderResourceRefCnt == 0)
  {
    nsrefcnt refcnt;
    NS_RELEASE2(kNC_Child, refcnt);
    NS_RELEASE2(kNC_Folder, refcnt);
    NS_RELEASE2(kNC_Name, refcnt);
    NS_RELEASE2(kNC_Open, refcnt);
    NS_RELEASE2(kNC_FolderTreeName, refcnt);
    NS_RELEASE2(kNC_FolderTreeSimpleName, refcnt);
    NS_RELEASE2(kNC_NameSort, refcnt);
    NS_RELEASE2(kNC_FolderTreeNameSort, refcnt);
    NS_RELEASE2(kNC_SpecialFolder, refcnt);
    NS_RELEASE2(kNC_ServerType, refcnt);
    NS_RELEASE2(kNC_IsDeferred, refcnt);
    NS_RELEASE2(kNC_CanCreateFoldersOnServer, refcnt);
    NS_RELEASE2(kNC_CanFileMessagesOnServer, refcnt);
    NS_RELEASE2(kNC_IsServer, refcnt);
    NS_RELEASE2(kNC_IsSecure, refcnt);
    NS_RELEASE2(kNC_CanSubscribe, refcnt);
    NS_RELEASE2(kNC_SupportsOffline, refcnt);
    NS_RELEASE2(kNC_CanFileMessages, refcnt);
    NS_RELEASE2(kNC_CanCreateSubfolders, refcnt);
    NS_RELEASE2(kNC_CanRename, refcnt);
    NS_RELEASE2(kNC_CanCompact, refcnt);
    NS_RELEASE2(kNC_TotalMessages, refcnt);
    NS_RELEASE2(kNC_TotalUnreadMessages, refcnt);
    NS_RELEASE2(kNC_FolderSize, refcnt);
    NS_RELEASE2(kNC_Charset, refcnt);
    NS_RELEASE2(kNC_BiffState, refcnt);
    NS_RELEASE2(kNC_HasUnreadMessages, refcnt);
    NS_RELEASE2(kNC_NewMessages, refcnt);
    NS_RELEASE2(kNC_SubfoldersHaveUnreadMessages, refcnt);
    NS_RELEASE2(kNC_NoSelect, refcnt);
    NS_RELEASE2(kNC_VirtualFolder, refcnt);
    NS_RELEASE2(kNC_InVFEditSearchScope, refcnt);
    NS_RELEASE2(kNC_ImapShared, refcnt);
    NS_RELEASE2(kNC_Synchronize, refcnt);
    NS_RELEASE2(kNC_SyncDisabled, refcnt);
    NS_RELEASE2(kNC_CanSearchMessages, refcnt);

    NS_RELEASE2(kNC_Delete, refcnt);
    NS_RELEASE2(kNC_ReallyDelete, refcnt);
    NS_RELEASE2(kNC_NewFolder, refcnt);
    NS_RELEASE2(kNC_GetNewMessages, refcnt);
    NS_RELEASE2(kNC_Copy, refcnt);
    NS_RELEASE2(kNC_Move, refcnt);
    NS_RELEASE2(kNC_CopyFolder, refcnt);
    NS_RELEASE2(kNC_MoveFolder, refcnt);
    NS_RELEASE2(kNC_MarkAllMessagesRead, refcnt);
    NS_RELEASE2(kNC_Compact, refcnt);
    NS_RELEASE2(kNC_CompactAll, refcnt);
    NS_RELEASE2(kNC_Rename, refcnt);
    NS_RELEASE2(kNC_EmptyTrash, refcnt);
    NS_RELEASE2(kNC_UnreadFolders, refcnt);
    NS_RELEASE2(kNC_FavoriteFolders, refcnt);
    NS_RELEASE2(kNC_RecentFolders, refcnt);

    NS_RELEASE(kTotalMessagesAtom);
    NS_RELEASE(kTotalUnreadMessagesAtom);
    NS_RELEASE(kFolderSizeAtom);
    NS_RELEASE(kBiffStateAtom);
    NS_RELEASE(kSortOrderAtom);
    NS_RELEASE(kNewMessagesAtom);
    NS_RELEASE(kNameAtom);
    NS_RELEASE(kSynchronizeAtom);
    NS_RELEASE(kOpenAtom);
    NS_RELEASE(kIsDeferredAtom);
    NS_RELEASE(kIsSecureAtom);
    NS_RELEASE(kCanFileMessagesAtom);
    NS_RELEASE(kInVFEditSearchScopeAtom);
  }
}

nsresult nsMsgFolderDataSource::CreateLiterals(nsIRDFService *rdf)
{
  createNode(NS_LITERAL_STRING("true").get(),
    getter_AddRefs(kTrueLiteral), rdf);
  createNode(NS_LITERAL_STRING("false").get(),
    getter_AddRefs(kFalseLiteral), rdf);

  return NS_OK;
}

nsresult nsMsgFolderDataSource::Init()
{
  nsresult rv;

  rv = nsMsgRDFDataSource::Init();
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIMsgMailSession> mailSession =
    do_GetService(NS_MSGMAILSESSION_CONTRACTID, &rv);

  if(NS_SUCCEEDED(rv))
    mailSession->AddFolderListener(this,
      nsIFolderListener::added |
      nsIFolderListener::removed |
      nsIFolderListener::intPropertyChanged |
      nsIFolderListener::boolPropertyChanged |
      nsIFolderListener::unicharPropertyChanged);

  return NS_OK;
}

void nsMsgFolderDataSource::Cleanup()
{
  nsresult rv;
  if (!m_shuttingDown)
  {
    nsCOMPtr<nsIMsgMailSession> mailSession =
      do_GetService(NS_MSGMAILSESSION_CONTRACTID, &rv);

    if(NS_SUCCEEDED(rv))
      mailSession->RemoveFolderListener(this);
  }

  nsMsgRDFDataSource::Cleanup();
}

nsresult nsMsgFolderDataSource::CreateArcsOutEnumerator()
{
  nsresult rv;

  rv = getFolderArcLabelsOut(kFolderArcsOutArray);
  if(NS_FAILED(rv)) return rv;

  return rv;
}

NS_IMPL_ADDREF_INHERITED(nsMsgFolderDataSource, nsMsgRDFDataSource)
NS_IMPL_RELEASE_INHERITED(nsMsgFolderDataSource, nsMsgRDFDataSource)

NS_IMPL_QUERY_INTERFACE_INHERITED1(nsMsgFolderDataSource, nsMsgRDFDataSource, nsIFolderListener)

 // nsIRDFDataSource methods
NS_IMETHODIMP nsMsgFolderDataSource::GetURI(char* *uri)
{
  if ((*uri = strdup("rdf:mailnewsfolders")) == nullptr)
    return NS_ERROR_OUT_OF_MEMORY;
  else
    return NS_OK;
}

NS_IMETHODIMP nsMsgFolderDataSource::GetSource(nsIRDFResource* property,
                                               nsIRDFNode* target,
                                               bool tv,
                                               nsIRDFResource** source /* out */)
{
  NS_ASSERTION(false, "not implemented");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgFolderDataSource::GetTarget(nsIRDFResource* source,
                                               nsIRDFResource* property,
                                               bool tv,
                                               nsIRDFNode** target)
{
  nsresult rv = NS_RDF_NO_VALUE;

  // we only have positive assertions in the mail data source.
  if (! tv)
    return NS_RDF_NO_VALUE;

  nsCOMPtr<nsIMsgFolder> folder(do_QueryInterface(source));
  if (folder)
    rv = createFolderNode(folder, property, target);
  else
    return NS_RDF_NO_VALUE;
  return rv;
}


NS_IMETHODIMP nsMsgFolderDataSource::GetSources(nsIRDFResource* property,
                                                nsIRDFNode* target,
                                                bool tv,
                                                nsISimpleEnumerator** sources)
{
  return NS_RDF_NO_VALUE;
}

NS_IMETHODIMP nsMsgFolderDataSource::GetTargets(nsIRDFResource* source,
                                                nsIRDFResource* property,
                                                bool tv,
                                                nsISimpleEnumerator** targets)
{
  nsresult rv = NS_RDF_NO_VALUE;
  if(!targets)
    return NS_ERROR_NULL_POINTER;

  *targets = nullptr;

  nsCOMPtr<nsIMsgFolder> folder(do_QueryInterface(source, &rv));
  if (NS_SUCCEEDED(rv))
  {
    if ((kNC_Child == property))
    {
      rv = folder->GetSubFolders(targets);
    }
    else if ((kNC_Name == property) ||
      (kNC_Open == property) ||
      (kNC_FolderTreeName == property) ||
      (kNC_FolderTreeSimpleName == property) ||
      (kNC_SpecialFolder == property) ||
      (kNC_IsServer == property) ||
      (kNC_IsSecure == property) ||
      (kNC_CanSubscribe == property) ||
      (kNC_SupportsOffline == property) ||
      (kNC_CanFileMessages == property) ||
      (kNC_CanCreateSubfolders == property) ||
      (kNC_CanRename == property) ||
      (kNC_CanCompact == property) ||
      (kNC_ServerType == property) ||
      (kNC_IsDeferred == property) ||
      (kNC_CanCreateFoldersOnServer == property) ||
      (kNC_CanFileMessagesOnServer == property) ||
      (kNC_NoSelect == property) ||
      (kNC_VirtualFolder == property) ||
      (kNC_InVFEditSearchScope == property) ||
      (kNC_ImapShared == property) ||
      (kNC_Synchronize == property) ||
      (kNC_SyncDisabled == property) ||
      (kNC_CanSearchMessages == property))
    {
      return NS_NewSingletonEnumerator(targets, property);
    }
  }
  if(!*targets)
  {
    //create empty cursor
    rv = NS_NewEmptyEnumerator(targets);
  }

  return rv;
}

NS_IMETHODIMP nsMsgFolderDataSource::Assert(nsIRDFResource* source,
                      nsIRDFResource* property,
                      nsIRDFNode* target,
                      bool tv)
{
  nsresult rv;
  nsCOMPtr<nsIMsgFolder> folder(do_QueryInterface(source, &rv));
  //We don't handle tv = false at the moment.
  if(NS_SUCCEEDED(rv) && tv)
    return DoFolderAssert(folder, property, target);
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMsgFolderDataSource::Unassert(nsIRDFResource* source,
                        nsIRDFResource* property,
                        nsIRDFNode* target)
{
  nsresult rv;
  nsCOMPtr<nsIMsgFolder> folder(do_QueryInterface(source, &rv));
  NS_ENSURE_SUCCESS(rv,rv);
  return DoFolderUnassert(folder, property, target);
}


NS_IMETHODIMP nsMsgFolderDataSource::HasAssertion(nsIRDFResource* source,
                            nsIRDFResource* property,
                            nsIRDFNode* target,
                            bool tv,
                            bool* hasAssertion)
{
  nsresult rv;
  nsCOMPtr<nsIMsgFolder> folder(do_QueryInterface(source, &rv));
  if(NS_SUCCEEDED(rv))
    return DoFolderHasAssertion(folder, property, target, tv, hasAssertion);
  else
    *hasAssertion = false;
  return NS_OK;
}


NS_IMETHODIMP
nsMsgFolderDataSource::HasArcOut(nsIRDFResource *aSource, nsIRDFResource *aArc, bool *result)
{
  nsresult rv;
  nsCOMPtr<nsIMsgFolder> folder(do_QueryInterface(aSource, &rv));
  if (NS_SUCCEEDED(rv))
  {
    *result = (aArc == kNC_Name ||
      aArc == kNC_Open ||
      aArc == kNC_FolderTreeName ||
      aArc == kNC_FolderTreeSimpleName ||
      aArc == kNC_SpecialFolder ||
      aArc == kNC_ServerType ||
      aArc == kNC_IsDeferred ||
      aArc == kNC_CanCreateFoldersOnServer ||
      aArc == kNC_CanFileMessagesOnServer ||
      aArc == kNC_IsServer ||
      aArc == kNC_IsSecure ||
      aArc == kNC_CanSubscribe ||
      aArc == kNC_SupportsOffline ||
      aArc == kNC_CanFileMessages ||
      aArc == kNC_CanCreateSubfolders ||
      aArc == kNC_CanRename ||
      aArc == kNC_CanCompact ||
      aArc == kNC_TotalMessages ||
      aArc == kNC_TotalUnreadMessages ||
      aArc == kNC_FolderSize ||
      aArc == kNC_Charset ||
      aArc == kNC_BiffState ||
      aArc == kNC_Child ||
      aArc == kNC_NoSelect ||
      aArc == kNC_VirtualFolder ||
      aArc == kNC_InVFEditSearchScope ||
      aArc == kNC_ImapShared ||
      aArc == kNC_Synchronize ||
      aArc == kNC_SyncDisabled ||
      aArc == kNC_CanSearchMessages);
  }
  else
  {
    *result = false;
  }
  return NS_OK;
}

NS_IMETHODIMP nsMsgFolderDataSource::ArcLabelsIn(nsIRDFNode* node,
                                                 nsISimpleEnumerator** labels)
{
  return nsMsgRDFDataSource::ArcLabelsIn(node, labels);
}

NS_IMETHODIMP nsMsgFolderDataSource::ArcLabelsOut(nsIRDFResource* source,
                                                  nsISimpleEnumerator** labels)
{
  nsresult rv = NS_RDF_NO_VALUE;

  nsCOMPtr<nsIMsgFolder> folder(do_QueryInterface(source, &rv));
  if (NS_SUCCEEDED(rv))
  {
    rv = NS_NewArrayEnumerator(labels, kFolderArcsOutArray);
  }
  else
  {
    rv = NS_NewEmptyEnumerator(labels);
  }

  return rv;
}

nsresult
nsMsgFolderDataSource::getFolderArcLabelsOut(nsCOMArray<nsIRDFResource> &aArcs)
{
  aArcs.AppendObject(kNC_Name);
  aArcs.AppendObject(kNC_Open);
  aArcs.AppendObject(kNC_FolderTreeName);
  aArcs.AppendObject(kNC_FolderTreeSimpleName);
  aArcs.AppendObject(kNC_SpecialFolder);
  aArcs.AppendObject(kNC_ServerType);
  aArcs.AppendObject(kNC_IsDeferred);
  aArcs.AppendObject(kNC_CanCreateFoldersOnServer);
  aArcs.AppendObject(kNC_CanFileMessagesOnServer);
  aArcs.AppendObject(kNC_IsServer);
  aArcs.AppendObject(kNC_IsSecure);
  aArcs.AppendObject(kNC_CanSubscribe);
  aArcs.AppendObject(kNC_SupportsOffline);
  aArcs.AppendObject(kNC_CanFileMessages);
  aArcs.AppendObject(kNC_CanCreateSubfolders);
  aArcs.AppendObject(kNC_CanRename);
  aArcs.AppendObject(kNC_CanCompact);
  aArcs.AppendObject(kNC_TotalMessages);
  aArcs.AppendObject(kNC_TotalUnreadMessages);
  aArcs.AppendObject(kNC_FolderSize);
  aArcs.AppendObject(kNC_Charset);
  aArcs.AppendObject(kNC_BiffState);
  aArcs.AppendObject(kNC_Child);
  aArcs.AppendObject(kNC_NoSelect);
  aArcs.AppendObject(kNC_VirtualFolder);
  aArcs.AppendObject(kNC_InVFEditSearchScope);
  aArcs.AppendObject(kNC_ImapShared);
  aArcs.AppendObject(kNC_Synchronize);
  aArcs.AppendObject(kNC_SyncDisabled);
  aArcs.AppendObject(kNC_CanSearchMessages);

  return NS_OK;
}

NS_IMETHODIMP
nsMsgFolderDataSource::GetAllResources(nsISimpleEnumerator** aCursor)
{
  NS_NOTYETIMPLEMENTED("sorry!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMsgFolderDataSource::GetAllCmds(nsIRDFResource* source,
                                      nsISimpleEnumerator/*<nsIRDFResource>*/** commands)
{
  NS_NOTYETIMPLEMENTED("no one actually uses me");
  nsresult rv;

  nsCOMPtr<nsIMsgFolder> folder(do_QueryInterface(source, &rv));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIMutableArray> cmds =
    do_CreateInstance(NS_ARRAY_CONTRACTID);
  NS_ENSURE_STATE(cmds);

  cmds->AppendElement(kNC_Delete, false);
  cmds->AppendElement(kNC_ReallyDelete, false);
  cmds->AppendElement(kNC_NewFolder, false);
  cmds->AppendElement(kNC_GetNewMessages, false);
  cmds->AppendElement(kNC_Copy, false);
  cmds->AppendElement(kNC_Move, false);
  cmds->AppendElement(kNC_CopyFolder, false);
  cmds->AppendElement(kNC_MoveFolder, false);
  cmds->AppendElement(kNC_MarkAllMessagesRead, false);
  cmds->AppendElement(kNC_Compact, false);
  cmds->AppendElement(kNC_CompactAll, false);
  cmds->AppendElement(kNC_Rename, false);
  cmds->AppendElement(kNC_EmptyTrash, false);

  return cmds->Enumerate(commands);
}

NS_IMETHODIMP
nsMsgFolderDataSource::IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                                        nsIRDFResource*   aCommand,
                                        nsISupportsArray/*<nsIRDFResource>*/* aArguments,
                                        bool* aResult)
{
  nsresult rv;
  nsCOMPtr<nsIMsgFolder> folder;

  uint32_t cnt;
  rv = aSources->Count(&cnt);
  if (NS_FAILED(rv)) return rv;
  for (uint32_t i = 0; i < cnt; i++)
  {
    folder = do_QueryElementAt(aSources, i, &rv);
    if (NS_SUCCEEDED(rv))
    {
      // we don't care about the arguments -- folder commands are always enabled
      if (!((aCommand == kNC_Delete) ||
            (aCommand == kNC_ReallyDelete) ||
            (aCommand == kNC_NewFolder) ||
            (aCommand == kNC_Copy) ||
            (aCommand == kNC_Move) ||
            (aCommand == kNC_CopyFolder) ||
            (aCommand == kNC_MoveFolder) ||
            (aCommand == kNC_GetNewMessages) ||
            (aCommand == kNC_MarkAllMessagesRead) ||
            (aCommand == kNC_Compact) ||
            (aCommand == kNC_CompactAll) ||
            (aCommand == kNC_Rename) ||
            (aCommand == kNC_EmptyTrash)))
      {
        *aResult = false;
        return NS_OK;
      }
    }
  }
  *aResult = true;
  return NS_OK; // succeeded for all sources
}

NS_IMETHODIMP
nsMsgFolderDataSource::DoCommand(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                                 nsIRDFResource*   aCommand,
                                 nsISupportsArray/*<nsIRDFResource>*/* aArguments)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsISupports> supports;
  nsCOMPtr<nsIMsgWindow> window;

  // callers can pass in the msgWindow as the last element of the arguments
  // array. If they do, we'll use that as the msg window for progress, etc.
  if (aArguments)
  {
    uint32_t numArgs;
    aArguments->Count(&numArgs);
    if (numArgs > 1)
      window = do_QueryElementAt(aArguments, numArgs - 1);
  }
  if (!window)
    window = mWindow;

  // XXX need to handle batching of command applied to all sources

  uint32_t cnt = 0;
  uint32_t i = 0;

  rv = aSources->Count(&cnt);
  if (NS_FAILED(rv)) return rv;

  for ( ; i < cnt; i++)
  {
    nsCOMPtr<nsIMsgFolder> folder = do_QueryElementAt(aSources, i, &rv);
    if (NS_SUCCEEDED(rv))
    {
      if ((aCommand == kNC_Delete))
      {
        rv = DoDeleteFromFolder(folder, aArguments, window, false);
      }
      if ((aCommand == kNC_ReallyDelete))
      {
        rv = DoDeleteFromFolder(folder, aArguments, window, true);
      }
      else if((aCommand == kNC_NewFolder))
      {
        rv = DoNewFolder(folder, aArguments, window);
      }
      else if((aCommand == kNC_GetNewMessages))
      {
        nsCOMPtr<nsIMsgIncomingServer> server = do_QueryElementAt(aArguments, i, &rv);
        NS_ENSURE_SUCCESS(rv, rv);
        rv = server->GetNewMessages(folder, window, nullptr);
      }
      else if((aCommand == kNC_Copy))
      {
        rv = DoCopyToFolder(folder, aArguments, window, false);
      }
      else if((aCommand == kNC_Move))
      {
        rv = DoCopyToFolder(folder, aArguments, window, true);
      }
      else if((aCommand == kNC_CopyFolder))
      {
        rv = DoFolderCopyToFolder(folder, aArguments, window, false);
      }
      else if((aCommand == kNC_MoveFolder))
      {
        rv = DoFolderCopyToFolder(folder, aArguments, window, true);
      }
      else if((aCommand == kNC_MarkAllMessagesRead))
      {
        rv = folder->MarkAllMessagesRead(window);
      }
      else if ((aCommand == kNC_Compact))
      {
        rv = folder->Compact(nullptr, window);
      }
      else if ((aCommand == kNC_CompactAll))
      {
        // this will also compact offline stores for IMAP
        rv = folder->CompactAll(nullptr, window, true);
      }
      else if ((aCommand == kNC_EmptyTrash))
      {
          rv = folder->EmptyTrash(window, nullptr);
      }
      else if ((aCommand == kNC_Rename))
      {
        nsCOMPtr<nsIRDFLiteral> literal = do_QueryElementAt(aArguments, 0, &rv);
        if(NS_SUCCEEDED(rv))
        {
          nsString name;
          literal->GetValue(getter_Copies(name));
          rv = folder->Rename(name, window);
        }
      }
    }
    else
    {
      rv = NS_ERROR_NOT_IMPLEMENTED;
    }
  }
  //for the moment return NS_OK, because failure stops entire DoCommand process.
  return rv;
  //return NS_OK;
}

NS_IMETHODIMP nsMsgFolderDataSource::OnItemAdded(nsIMsgFolder *parentItem, nsISupports *item)
{
  return OnItemAddedOrRemoved(parentItem, item, true);
}

NS_IMETHODIMP nsMsgFolderDataSource::OnItemRemoved(nsIMsgFolder *parentItem, nsISupports *item)
{
  return OnItemAddedOrRemoved(parentItem, item, false);
}

nsresult nsMsgFolderDataSource::OnItemAddedOrRemoved(nsIMsgFolder *parentItem, nsISupports *item, bool added)
{
  nsCOMPtr<nsIRDFNode> itemNode(do_QueryInterface(item));
  if (itemNode)
  {
    nsCOMPtr<nsIRDFResource> parentResource(do_QueryInterface(parentItem));
    if (parentResource) // RDF is not happy about a null parent resource.
      NotifyObservers(parentResource, kNC_Child, itemNode, nullptr, added, false);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsMsgFolderDataSource::OnItemPropertyChanged(nsIMsgFolder *resource,
                                             nsIAtom *property,
                                             const char *oldValue,
                                             const char *newValue)

{
  return NS_OK;
}

NS_IMETHODIMP
nsMsgFolderDataSource::OnItemIntPropertyChanged(nsIMsgFolder *folder,
                                                nsIAtom *property,
                                                int32_t oldValue,
                                                int32_t newValue)
{
  nsCOMPtr<nsIRDFResource> resource(do_QueryInterface(folder));
  if (kTotalMessagesAtom == property)
    OnTotalMessagePropertyChanged(resource, oldValue, newValue);
  else if (kTotalUnreadMessagesAtom == property)
    OnUnreadMessagePropertyChanged(resource, oldValue, newValue);
  else if (kFolderSizeAtom == property)
    OnFolderSizePropertyChanged(resource, oldValue, newValue);
  else if (kSortOrderAtom == property)
    OnFolderSortOrderPropertyChanged(resource, oldValue, newValue);
  else if (kBiffStateAtom == property) {
    // be careful about skipping if oldValue == newValue
    // see the comment in nsMsgFolder::SetBiffState() about filters

    nsCOMPtr<nsIRDFNode> biffNode;
    nsresult rv = createBiffStateNodeFromFlag(newValue, getter_AddRefs(biffNode));
    NS_ENSURE_SUCCESS(rv,rv);

    NotifyPropertyChanged(resource, kNC_BiffState, biffNode);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsMsgFolderDataSource::OnItemUnicharPropertyChanged(nsIMsgFolder *folder,
                                                    nsIAtom *property,
                                                    const PRUnichar *oldValue,
                                                    const PRUnichar *newValue)
{
  nsCOMPtr<nsIRDFResource> resource(do_QueryInterface(folder));
  if (kNameAtom == property)
  {
    int32_t numUnread;
    folder->GetNumUnread(false, &numUnread);
    NotifyFolderTreeNameChanged(folder, resource, numUnread);
    NotifyFolderTreeSimpleNameChanged(folder, resource);
    NotifyFolderNameChanged(folder, resource);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsMsgFolderDataSource::OnItemBoolPropertyChanged(nsIMsgFolder *folder,
                                                 nsIAtom *property,
                                                 bool oldValue,
                                                 bool newValue)
{
  nsCOMPtr<nsIRDFResource> resource(do_QueryInterface(folder));
  if (newValue != oldValue) {
    nsIRDFNode* literalNode = newValue?kTrueLiteral:kFalseLiteral;
    nsIRDFNode* oldLiteralNode = oldValue?kTrueLiteral:kFalseLiteral;
    if (kNewMessagesAtom == property)
      NotifyPropertyChanged(resource, kNC_NewMessages, literalNode);
    else if (kSynchronizeAtom == property)
      NotifyPropertyChanged(resource, kNC_Synchronize, literalNode);
    else if (kOpenAtom == property)
      NotifyPropertyChanged(resource, kNC_Open, literalNode);
    else if (kIsDeferredAtom == property)
      NotifyPropertyChanged(resource, kNC_IsDeferred, literalNode, oldLiteralNode);
    else if (kIsSecureAtom == property)
      NotifyPropertyChanged(resource, kNC_IsSecure, literalNode, oldLiteralNode);
    else if (kCanFileMessagesAtom == property)
      NotifyPropertyChanged(resource, kNC_CanFileMessages, literalNode, oldLiteralNode);
    else if (kInVFEditSearchScopeAtom == property)
      NotifyPropertyChanged(resource, kNC_InVFEditSearchScope, literalNode);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsMsgFolderDataSource::OnItemPropertyFlagChanged(nsIMsgDBHdr *item,
                                                 nsIAtom *property,
                                                 uint32_t oldFlag,
                                                 uint32_t newFlag)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMsgFolderDataSource::OnItemEvent(nsIMsgFolder *aFolder, nsIAtom *aEvent)
{
  return NS_OK;
}


nsresult nsMsgFolderDataSource::createFolderNode(nsIMsgFolder* folder,
                                                 nsIRDFResource* property,
                                                 nsIRDFNode** target)
{
  nsresult rv = NS_RDF_NO_VALUE;

  if (kNC_NameSort == property)
    rv = createFolderNameNode(folder, target, true);
  else if(kNC_FolderTreeNameSort == property)
    rv = createFolderNameNode(folder, target, true);
  else if (kNC_Name == property)
    rv = createFolderNameNode(folder, target, false);
  else if(kNC_Open == property)
    rv = createFolderOpenNode(folder, target);
  else if (kNC_FolderTreeName == property)
    rv = createFolderTreeNameNode(folder, target);
  else if (kNC_FolderTreeSimpleName == property)
    rv = createFolderTreeSimpleNameNode(folder, target);
  else if ((kNC_SpecialFolder == property))
    rv = createFolderSpecialNode(folder,target);
  else if ((kNC_ServerType == property))
    rv = createFolderServerTypeNode(folder, target);
  else if ((kNC_IsDeferred == property))
    rv = createServerIsDeferredNode(folder, target);
  else if ((kNC_CanCreateFoldersOnServer == property))
    rv = createFolderCanCreateFoldersOnServerNode(folder, target);
  else if ((kNC_CanFileMessagesOnServer == property))
    rv = createFolderCanFileMessagesOnServerNode(folder, target);
  else if ((kNC_IsServer == property))
    rv = createFolderIsServerNode(folder, target);
  else if ((kNC_IsSecure == property))
    rv = createFolderIsSecureNode(folder, target);
  else if ((kNC_CanSubscribe == property))
    rv = createFolderCanSubscribeNode(folder, target);
  else if ((kNC_SupportsOffline == property))
    rv = createFolderSupportsOfflineNode(folder, target);
  else if ((kNC_CanFileMessages == property))
    rv = createFolderCanFileMessagesNode(folder, target);
  else if ((kNC_CanCreateSubfolders == property))
    rv = createFolderCanCreateSubfoldersNode(folder, target);
  else if ((kNC_CanRename == property))
    rv = createFolderCanRenameNode(folder, target);
  else if ((kNC_CanCompact == property))
    rv = createFolderCanCompactNode(folder, target);
  else if ((kNC_TotalMessages == property))
    rv = createTotalMessagesNode(folder, target);
  else if ((kNC_TotalUnreadMessages == property))
    rv = createUnreadMessagesNode(folder, target);
  else if ((kNC_FolderSize == property))
    rv = createFolderSizeNode(folder, target);
  else if ((kNC_Charset == property))
    rv = createCharsetNode(folder, target);
  else if ((kNC_BiffState == property))
    rv = createBiffStateNodeFromFolder(folder, target);
  else if ((kNC_HasUnreadMessages == property))
    rv = createHasUnreadMessagesNode(folder, false, target);
  else if ((kNC_NewMessages == property))
    rv = createNewMessagesNode(folder, target);
  else if ((kNC_SubfoldersHaveUnreadMessages == property))
    rv = createHasUnreadMessagesNode(folder, true, target);
  else if ((kNC_Child == property))
    rv = createFolderChildNode(folder, target);
  else if ((kNC_NoSelect == property))
    rv = createFolderNoSelectNode(folder, target);
  else if ((kNC_VirtualFolder == property))
    rv = createFolderVirtualNode(folder, target);
  else if (kNC_InVFEditSearchScope == property)
    rv = createInVFEditSearchScopeNode(folder, target);
  else if ((kNC_ImapShared == property))
    rv = createFolderImapSharedNode(folder, target);
  else if ((kNC_Synchronize == property))
    rv = createFolderSynchronizeNode(folder, target);
  else if ((kNC_SyncDisabled == property))
    rv = createFolderSyncDisabledNode(folder, target);
  else if ((kNC_CanSearchMessages == property))
    rv = createCanSearchMessages(folder, target);
  return NS_FAILED(rv) ? NS_RDF_NO_VALUE : rv;
}

nsresult
nsMsgFolderDataSource::createFolderNameNode(nsIMsgFolder *folder,
                                            nsIRDFNode **target, bool sort)
{
  nsresult rv;
  if (sort)
  {
    uint8_t *sortKey=nullptr;
    uint32_t sortKeyLength;
    rv = folder->GetSortKey(&sortKeyLength, &sortKey);
    NS_ENSURE_SUCCESS(rv, rv);
    createBlobNode(sortKey, sortKeyLength, target, getRDFService());
    PR_Free(sortKey);
  }
  else
  {
    nsString name;
    rv = folder->GetName(name);
    if (NS_FAILED(rv))
      return rv;
    createNode(name.get(), target, getRDFService());
  }

  return NS_OK;
}

nsresult nsMsgFolderDataSource::GetFolderDisplayName(nsIMsgFolder *folder, nsString& folderName)
{
  return folder->GetAbbreviatedName(folderName);
}

nsresult
nsMsgFolderDataSource::createFolderTreeNameNode(nsIMsgFolder *folder,
                                                nsIRDFNode **target)
{
  nsString name;
  nsresult rv = GetFolderDisplayName(folder, name);
  if (NS_FAILED(rv)) return rv;
  nsAutoString nameString(name);
  int32_t unreadMessages;

  rv = folder->GetNumUnread(false, &unreadMessages);
  if(NS_SUCCEEDED(rv))
    CreateUnreadMessagesNameString(unreadMessages, nameString);

  createNode(nameString.get(), target, getRDFService());
  return NS_OK;
}

nsresult nsMsgFolderDataSource::createFolderTreeSimpleNameNode(nsIMsgFolder * folder, nsIRDFNode **target)
{
  nsString name;
  nsresult rv = GetFolderDisplayName(folder, name);
  if (NS_FAILED(rv)) return rv;

  createNode(name.get(), target, getRDFService());
  return NS_OK;
}

nsresult nsMsgFolderDataSource::CreateUnreadMessagesNameString(int32_t unreadMessages, nsAutoString &nameString)
{
  //Only do this if unread messages are positive
  if(unreadMessages > 0)
  {
    nameString.Append(NS_LITERAL_STRING(" (").get());
    nameString.AppendInt(unreadMessages);
    nameString.Append(NS_LITERAL_STRING(")").get());
  }
  return NS_OK;
}

nsresult
nsMsgFolderDataSource::createFolderSpecialNode(nsIMsgFolder *folder,
                                               nsIRDFNode **target)
{
  uint32_t flags;
  nsresult rv = folder->GetFlags(&flags);
  if(NS_FAILED(rv))
    return rv;

  nsAutoString specialFolderString;
  if (flags & nsMsgFolderFlags::Inbox)
    specialFolderString.AssignLiteral("Inbox");
  else if (flags & nsMsgFolderFlags::Trash)
    specialFolderString.AssignLiteral("Trash");
  else if (flags & nsMsgFolderFlags::Queue)
    specialFolderString.AssignLiteral("Outbox");
  else if (flags & nsMsgFolderFlags::SentMail)
    specialFolderString.AssignLiteral("Sent");
  else if (flags & nsMsgFolderFlags::Drafts)
    specialFolderString.AssignLiteral("Drafts");
  else if (flags & nsMsgFolderFlags::Templates)
    specialFolderString.AssignLiteral("Templates");
  else if (flags & nsMsgFolderFlags::Junk)
    specialFolderString.AssignLiteral("Junk");
  else if (flags & nsMsgFolderFlags::Virtual)
    specialFolderString.AssignLiteral("Virtual");
  else if (flags & nsMsgFolderFlags::Archive)
    specialFolderString.AssignLiteral("Archives");
  else {
    // XXX why do this at all? or just ""
    specialFolderString.AssignLiteral("none");
  }

  createNode(specialFolderString.get(), target, getRDFService());
  return NS_OK;
}

nsresult
nsMsgFolderDataSource::createFolderServerTypeNode(nsIMsgFolder* folder,
                                                  nsIRDFNode **target)
{
  nsresult rv;
  nsCOMPtr<nsIMsgIncomingServer> server;
  rv = folder->GetServer(getter_AddRefs(server));
  if (NS_FAILED(rv) || !server) return NS_ERROR_FAILURE;

  nsCString serverType;
  rv = server->GetType(serverType);
  if (NS_FAILED(rv)) return rv;

  createNode(NS_ConvertASCIItoUTF16(serverType).get(), target, getRDFService());
  return NS_OK;
}

nsresult
nsMsgFolderDataSource::createServerIsDeferredNode(nsIMsgFolder* folder,
                                                  nsIRDFNode **target)
{
  bool isDeferred = false;
  nsCOMPtr <nsIMsgIncomingServer> incomingServer;
  folder->GetServer(getter_AddRefs(incomingServer));
  if (incomingServer)
  {
    nsCOMPtr <nsIPop3IncomingServer> pop3Server = do_QueryInterface(incomingServer);
    if (pop3Server)
    {
      nsCString deferredToServer;
      pop3Server->GetDeferredToAccount(deferredToServer);
      isDeferred = !deferredToServer.IsEmpty();
    }
  }
  *target = (isDeferred) ? kTrueLiteral : kFalseLiteral;
  NS_IF_ADDREF(*target);
  return NS_OK;
}

nsresult
nsMsgFolderDataSource::createFolderCanCreateFoldersOnServerNode(nsIMsgFolder* folder,
                                                                 nsIRDFNode **target)
{
  nsresult rv;

  nsCOMPtr<nsIMsgIncomingServer> server;
  rv = folder->GetServer(getter_AddRefs(server));
  if (NS_FAILED(rv) || !server) return NS_ERROR_FAILURE;

  bool canCreateFoldersOnServer;
  rv = server->GetCanCreateFoldersOnServer(&canCreateFoldersOnServer);
  if (NS_FAILED(rv)) return rv;

  if (canCreateFoldersOnServer)
    *target = kTrueLiteral;
  else
    *target = kFalseLiteral;
  NS_IF_ADDREF(*target);

  return NS_OK;
}

nsresult
nsMsgFolderDataSource::createFolderCanFileMessagesOnServerNode(nsIMsgFolder* folder,
                                                                 nsIRDFNode **target)
{
  nsresult rv;

  nsCOMPtr<nsIMsgIncomingServer> server;
  rv = folder->GetServer(getter_AddRefs(server));
  if (NS_FAILED(rv) || !server) return NS_ERROR_FAILURE;

  bool canFileMessagesOnServer;
  rv = server->GetCanFileMessagesOnServer(&canFileMessagesOnServer);
  if (NS_FAILED(rv)) return rv;

  *target = (canFileMessagesOnServer) ? kTrueLiteral : kFalseLiteral;
  NS_IF_ADDREF(*target);

  return NS_OK;
}


nsresult
nsMsgFolderDataSource::createFolderIsServerNode(nsIMsgFolder* folder,
                                                  nsIRDFNode **target)
{
  nsresult rv;
  bool isServer;
  rv = folder->GetIsServer(&isServer);
  if (NS_FAILED(rv)) return rv;

  *target = nullptr;

  if (isServer)
    *target = kTrueLiteral;
  else
    *target = kFalseLiteral;
  NS_IF_ADDREF(*target);
  return NS_OK;
}

nsresult
nsMsgFolderDataSource::createFolderNoSelectNode(nsIMsgFolder* folder,
                                                  nsIRDFNode **target)
{
  nsresult rv;
  bool noSelect;
  rv = folder->GetNoSelect(&noSelect);
  if (NS_FAILED(rv)) return rv;

  *target = (noSelect) ? kTrueLiteral : kFalseLiteral;
  NS_IF_ADDREF(*target);
  return NS_OK;
}

nsresult
nsMsgFolderDataSource::createInVFEditSearchScopeNode(nsIMsgFolder* folder,
                                                  nsIRDFNode **target)
{
  bool inVFEditSearchScope = false;
  folder->GetInVFEditSearchScope(&inVFEditSearchScope);

  *target = inVFEditSearchScope ? kTrueLiteral : kFalseLiteral;
  NS_IF_ADDREF(*target);
  return NS_OK;
}

nsresult
nsMsgFolderDataSource::createFolderVirtualNode(nsIMsgFolder* folder,
                                                  nsIRDFNode **target)
{
  uint32_t folderFlags;
  folder->GetFlags(&folderFlags);

  *target = (folderFlags & nsMsgFolderFlags::Virtual) ? kTrueLiteral : kFalseLiteral;
  NS_IF_ADDREF(*target);
  return NS_OK;
}


nsresult
nsMsgFolderDataSource::createFolderImapSharedNode(nsIMsgFolder* folder,
                                                  nsIRDFNode **target)
{
  nsresult rv;
  bool imapShared;
  rv = folder->GetImapShared(&imapShared);
  if (NS_FAILED(rv)) return rv;

  *target = (imapShared) ? kTrueLiteral : kFalseLiteral;
  NS_IF_ADDREF(*target);
  return NS_OK;
}


nsresult
nsMsgFolderDataSource::createFolderSynchronizeNode(nsIMsgFolder* folder,
                                                  nsIRDFNode **target)
{
  nsresult rv;
  bool sync;
  rv = folder->GetFlag(nsMsgFolderFlags::Offline, &sync);
  if (NS_FAILED(rv)) return rv;

  *target = nullptr;

  *target = (sync) ? kTrueLiteral : kFalseLiteral;
  NS_IF_ADDREF(*target);
  return NS_OK;
}

nsresult
nsMsgFolderDataSource::createFolderSyncDisabledNode(nsIMsgFolder* folder,
                                                  nsIRDFNode **target)
{

  nsresult rv;
  bool isServer;
  nsCOMPtr<nsIMsgIncomingServer> server;

  rv = folder->GetIsServer(&isServer);
  if (NS_FAILED(rv)) return rv;

  rv = folder->GetServer(getter_AddRefs(server));
  if (NS_FAILED(rv) || !server) return NS_ERROR_FAILURE;

  nsCString serverType;
  rv = server->GetType(serverType);
  if (NS_FAILED(rv)) return rv;

  *target = isServer || MsgLowerCaseEqualsLiteral(serverType, "none") || MsgLowerCaseEqualsLiteral(serverType, "pop3") ?
            kTrueLiteral : kFalseLiteral;
  NS_IF_ADDREF(*target);
  return NS_OK;
}

nsresult
nsMsgFolderDataSource::createCanSearchMessages(nsIMsgFolder* folder,
                                                                 nsIRDFNode **target)
{
  nsresult rv;

  nsCOMPtr<nsIMsgIncomingServer> server;
  rv = folder->GetServer(getter_AddRefs(server));
  if (NS_FAILED(rv) || !server) return NS_ERROR_FAILURE;

  bool canSearchMessages;
  rv = server->GetCanSearchMessages(&canSearchMessages);
  if (NS_FAILED(rv)) return rv;

  *target = (canSearchMessages) ? kTrueLiteral : kFalseLiteral;
  NS_IF_ADDREF(*target);

  return NS_OK;
}

nsresult
nsMsgFolderDataSource::createFolderOpenNode(nsIMsgFolder *folder, nsIRDFNode **target)
{
  NS_ENSURE_ARG_POINTER(target);

  // call GetSubFolders() to ensure mFlags is set correctly
  // from the folder cache on startup
  nsCOMPtr<nsISimpleEnumerator> subFolders;
  nsresult rv = folder->GetSubFolders(getter_AddRefs(subFolders));
  if (NS_FAILED(rv))
    return NS_RDF_NO_VALUE;

  bool closed;
  rv = folder->GetFlag(nsMsgFolderFlags::Elided, &closed);
  if (NS_FAILED(rv))
    return rv;

  *target = (closed) ? kFalseLiteral : kTrueLiteral;

  NS_IF_ADDREF(*target);
  return NS_OK;
}

nsresult
nsMsgFolderDataSource::createFolderIsSecureNode(nsIMsgFolder* folder,
                                                  nsIRDFNode **target)
{
  nsresult rv;
  bool isSecure = false;

  nsCOMPtr<nsIMsgIncomingServer> server;
  rv = folder->GetServer(getter_AddRefs(server));

  if (NS_SUCCEEDED(rv) && server) {
    rv = server->GetIsSecure(&isSecure);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  *target = (isSecure) ? kTrueLiteral : kFalseLiteral;
  NS_IF_ADDREF(*target);
  return NS_OK;
}


nsresult
nsMsgFolderDataSource::createFolderCanSubscribeNode(nsIMsgFolder* folder,
                                                  nsIRDFNode **target)
{
  nsresult rv;
  bool canSubscribe;
  rv = folder->GetCanSubscribe(&canSubscribe);
  if (NS_FAILED(rv)) return rv;

  *target = (canSubscribe) ? kTrueLiteral : kFalseLiteral;
  NS_IF_ADDREF(*target);
  return NS_OK;
}

nsresult
nsMsgFolderDataSource::createFolderSupportsOfflineNode(nsIMsgFolder* folder,
                                                  nsIRDFNode **target)
{
  nsresult rv;
  bool supportsOffline;
  rv = folder->GetSupportsOffline(&supportsOffline);
  NS_ENSURE_SUCCESS(rv,rv);

  *target = (supportsOffline) ? kTrueLiteral : kFalseLiteral;
  NS_IF_ADDREF(*target);
  return NS_OK;
}

nsresult
nsMsgFolderDataSource::createFolderCanFileMessagesNode(nsIMsgFolder* folder,
                                                  nsIRDFNode **target)
{
  nsresult rv;
  bool canFileMessages;
  rv = folder->GetCanFileMessages(&canFileMessages);
  if (NS_FAILED(rv)) return rv;

  *target = (canFileMessages) ? kTrueLiteral : kFalseLiteral;
  NS_IF_ADDREF(*target);
  return NS_OK;
}

nsresult
nsMsgFolderDataSource::createFolderCanCreateSubfoldersNode(nsIMsgFolder* folder,
                                                  nsIRDFNode **target)
{
  nsresult rv;
  bool canCreateSubfolders;
  rv = folder->GetCanCreateSubfolders(&canCreateSubfolders);
  if (NS_FAILED(rv)) return rv;

  *target = (canCreateSubfolders) ? kTrueLiteral : kFalseLiteral;
  NS_IF_ADDREF(*target);
  return NS_OK;
}

nsresult
nsMsgFolderDataSource::createFolderCanRenameNode(nsIMsgFolder* folder,
                                                  nsIRDFNode **target)
{
  bool canRename;
  nsresult rv = folder->GetCanRename(&canRename);
  if (NS_FAILED(rv)) return rv;

  *target = (canRename) ? kTrueLiteral : kFalseLiteral;
  NS_IF_ADDREF(*target);
  return NS_OK;
}

nsresult
nsMsgFolderDataSource::createFolderCanCompactNode(nsIMsgFolder* folder,
                                                  nsIRDFNode **target)
{
  bool canCompact;
  nsresult rv = folder->GetCanCompact(&canCompact);
  if (NS_FAILED(rv)) return rv;

  *target = (canCompact) ? kTrueLiteral : kFalseLiteral;
  NS_IF_ADDREF(*target);
  return NS_OK;
}


nsresult
nsMsgFolderDataSource::createTotalMessagesNode(nsIMsgFolder *folder,
                                               nsIRDFNode **target)
{

  bool isServer;
  nsresult rv = folder->GetIsServer(&isServer);
  if (NS_FAILED(rv)) return rv;

  int32_t totalMessages;
  if(isServer)
    totalMessages = kDisplayBlankCount;
  else
  {
    rv = folder->GetTotalMessages(false, &totalMessages);
    if(NS_FAILED(rv)) return rv;
  }
  GetNumMessagesNode(totalMessages, target);

  return rv;
}

nsresult
nsMsgFolderDataSource::createFolderSizeNode(nsIMsgFolder *folder, nsIRDFNode **target)
{
  bool isServer;
  nsresult rv = folder->GetIsServer(&isServer);
  NS_ENSURE_SUCCESS(rv, rv);

  int32_t folderSize;
  if(isServer)
    folderSize = kDisplayBlankCount;
  else
  {
    // XXX todo, we are asserting here for news
    // for offline news, we'd know the size on disk, right?
    rv = folder->GetSizeOnDisk((uint32_t *) &folderSize);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  GetFolderSizeNode(folderSize, target);

  return rv;
}

nsresult
nsMsgFolderDataSource::createCharsetNode(nsIMsgFolder *folder, nsIRDFNode **target)
{
  nsCString charset;
  nsresult rv = folder->GetCharset(charset);
  if (NS_SUCCEEDED(rv))
    createNode(NS_ConvertASCIItoUTF16(charset).get(), target, getRDFService());
  else
    createNode(EmptyString().get(), target, getRDFService());
  return NS_OK;
}

nsresult
nsMsgFolderDataSource::createBiffStateNodeFromFolder(nsIMsgFolder *folder, nsIRDFNode **target)
{
  uint32_t biffState;
  nsresult rv = folder->GetBiffState(&biffState);
  if(NS_FAILED(rv)) return rv;

  rv = createBiffStateNodeFromFlag(biffState, target);
  NS_ENSURE_SUCCESS(rv,rv);

  return NS_OK;
}

nsresult
nsMsgFolderDataSource::createBiffStateNodeFromFlag(uint32_t flag, nsIRDFNode **target)
{
  const PRUnichar *biffStateStr;

  switch (flag) {
    case nsIMsgFolder::nsMsgBiffState_NewMail:
      biffStateStr = NS_LITERAL_STRING("NewMail").get();
      break;
    case nsIMsgFolder::nsMsgBiffState_NoMail:
      biffStateStr = NS_LITERAL_STRING("NoMail").get();
      break;
    default:
      biffStateStr = NS_LITERAL_STRING("UnknownMail").get();
      break;
  }

  createNode(biffStateStr, target, getRDFService());
  return NS_OK;
}

nsresult
nsMsgFolderDataSource::createUnreadMessagesNode(nsIMsgFolder *folder,
                        nsIRDFNode **target)
{
  bool isServer;
  nsresult rv = folder->GetIsServer(&isServer);
  if (NS_FAILED(rv)) return rv;

  int32_t totalUnreadMessages;
  if(isServer)
    totalUnreadMessages = kDisplayBlankCount;
  else
  {
    rv = folder->GetNumUnread(false, &totalUnreadMessages);
    if(NS_FAILED(rv)) return rv;
  }
  GetNumMessagesNode(totalUnreadMessages, target);

  return NS_OK;
}

nsresult
nsMsgFolderDataSource::createHasUnreadMessagesNode(nsIMsgFolder *folder, bool aIncludeSubfolders, nsIRDFNode **target)
{
  bool isServer;
  nsresult rv = folder->GetIsServer(&isServer);
  if (NS_FAILED(rv)) return rv;

  *target = kFalseLiteral;

  int32_t totalUnreadMessages;
  if(!isServer)
  {
    rv = folder->GetNumUnread(aIncludeSubfolders, &totalUnreadMessages);
    if(NS_FAILED(rv)) return rv;
    // if we're including sub-folders, we're trying to find out if child folders
    // have unread. If so, we subtract the unread msgs in the current folder.
    if (aIncludeSubfolders)
    {
      int32_t numUnreadInFolder;
      rv = folder->GetNumUnread(false, &numUnreadInFolder);
      NS_ENSURE_SUCCESS(rv, rv);
      // don't subtract if numUnread is negative (which means we don't know the unread count)
      if (numUnreadInFolder > 0)
        totalUnreadMessages -= numUnreadInFolder;
    }
    *target = (totalUnreadMessages > 0) ? kTrueLiteral : kFalseLiteral;
  }

  NS_IF_ADDREF(*target);
  return NS_OK;
}

nsresult
nsMsgFolderDataSource::OnUnreadMessagePropertyChanged(nsIRDFResource *folderResource, int32_t oldValue, int32_t newValue)
{
  nsCOMPtr<nsIMsgFolder> folder = do_QueryInterface(folderResource);
  if(folder)
  {
    //First send a regular unread message changed notification
    nsCOMPtr<nsIRDFNode> newNode;

    GetNumMessagesNode(newValue, getter_AddRefs(newNode));
    NotifyPropertyChanged(folderResource, kNC_TotalUnreadMessages, newNode);

    //Now see if hasUnreadMessages has changed
    if(oldValue <=0 && newValue >0)
    {
      NotifyPropertyChanged(folderResource, kNC_HasUnreadMessages, kTrueLiteral);
      NotifyAncestors(folder, kNC_SubfoldersHaveUnreadMessages, kTrueLiteral);
    }
    else if(oldValue > 0 && newValue <= 0)
    {
      NotifyPropertyChanged(folderResource, kNC_HasUnreadMessages, kFalseLiteral);
      // this isn't quite right - parents could still have other children with
      // unread messages. NotifyAncestors will have to figure that out...
      NotifyAncestors(folder, kNC_SubfoldersHaveUnreadMessages, kFalseLiteral);
    }

    //We will have to change the folderTreeName if the unread column is hidden
    NotifyFolderTreeNameChanged(folder, folderResource, newValue);
  }
  return NS_OK;
}

nsresult
nsMsgFolderDataSource::NotifyFolderNameChanged(nsIMsgFolder* aFolder, nsIRDFResource *folderResource)
{
  nsString name;
  nsresult rv = aFolder->GetName(name);

  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIRDFNode> newNameNode;
    createNode(name.get(), getter_AddRefs(newNameNode), getRDFService());
    NotifyPropertyChanged(folderResource, kNC_Name, newNameNode);
  }
  return NS_OK;
}

nsresult
nsMsgFolderDataSource::NotifyFolderTreeSimpleNameChanged(nsIMsgFolder* aFolder, nsIRDFResource *folderResource)
{
  nsString abbreviatedName;
  nsresult rv = GetFolderDisplayName(aFolder, abbreviatedName);
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIRDFNode> newNameNode;
    createNode(abbreviatedName.get(), getter_AddRefs(newNameNode), getRDFService());
    NotifyPropertyChanged(folderResource, kNC_FolderTreeSimpleName, newNameNode);
  }

  return NS_OK;
}

nsresult
nsMsgFolderDataSource::NotifyFolderTreeNameChanged(nsIMsgFolder* aFolder,
                                                   nsIRDFResource* aFolderResource,
                                                   int32_t aUnreadMessages)
{
  nsString name;
  nsresult rv = GetFolderDisplayName(aFolder, name);
  if (NS_SUCCEEDED(rv)) {
    nsAutoString newNameString(name);
    CreateUnreadMessagesNameString(aUnreadMessages, newNameString);
    nsCOMPtr<nsIRDFNode> newNameNode;
    createNode(newNameString.get(), getter_AddRefs(newNameNode), getRDFService());
    NotifyPropertyChanged(aFolderResource, kNC_FolderTreeName, newNameNode);
  }
  return NS_OK;
}

nsresult
nsMsgFolderDataSource::NotifyAncestors(nsIMsgFolder *aFolder, nsIRDFResource *aPropertyResource, nsIRDFNode *aNode)
{
  bool isServer = false;
  nsresult rv = aFolder->GetIsServer(&isServer);
  NS_ENSURE_SUCCESS(rv,rv);

  if (isServer)
    // done, stop
    return NS_OK;

  nsCOMPtr <nsIMsgFolder> parentMsgFolder;
  rv = aFolder->GetParent(getter_AddRefs(parentMsgFolder));
  NS_ENSURE_SUCCESS(rv,rv);
  if (!parentMsgFolder)
    return NS_OK;

  rv = parentMsgFolder->GetIsServer(&isServer);
  NS_ENSURE_SUCCESS(rv,rv);

  // don't need to notify servers either.
  if (isServer)
    // done, stop
    return NS_OK;

  nsCOMPtr<nsIRDFResource> parentFolderResource = do_QueryInterface(parentMsgFolder,&rv);
  NS_ENSURE_SUCCESS(rv,rv);

  // if we're setting the subFoldersHaveUnreadMessages property to false, check
  // if the folder really doesn't have subfolders with unread messages.
  if (aPropertyResource == kNC_SubfoldersHaveUnreadMessages && aNode == kFalseLiteral)
  {
    nsCOMPtr <nsIRDFNode> unreadMsgsNode;
    createHasUnreadMessagesNode(parentMsgFolder, true, getter_AddRefs(unreadMsgsNode));
    aNode = unreadMsgsNode;
  }
  NotifyPropertyChanged(parentFolderResource, aPropertyResource, aNode);

  return NotifyAncestors(parentMsgFolder, aPropertyResource, aNode);
}

// New Messages

nsresult
nsMsgFolderDataSource::createNewMessagesNode(nsIMsgFolder *folder, nsIRDFNode **target)
{

  nsresult rv;

  bool isServer;
  rv = folder->GetIsServer(&isServer);
  if (NS_FAILED(rv)) return rv;

  *target = kFalseLiteral;

  //int32_t totalNewMessages;
  bool isNewMessages;
  if(!isServer)
  {
    rv = folder->GetHasNewMessages(&isNewMessages);
    if(NS_FAILED(rv)) return rv;
    *target = (isNewMessages) ? kTrueLiteral : kFalseLiteral;
  }
  NS_IF_ADDREF(*target);
  return NS_OK;
}

/**
nsresult
nsMsgFolderDataSource::OnUnreadMessagePropertyChanged(nsIMsgFolder *folder, int32_t oldValue, int32_t newValue)
{
  nsCOMPtr<nsIRDFResource> folderResource = do_QueryInterface(folder);
  if(folderResource)
  {
    //First send a regular unread message changed notification
    nsCOMPtr<nsIRDFNode> newNode;

    GetNumMessagesNode(newValue, getter_AddRefs(newNode));
    NotifyPropertyChanged(folderResource, kNC_TotalUnreadMessages, newNode);

    //Now see if hasUnreadMessages has changed
    nsCOMPtr<nsIRDFNode> oldHasUnreadMessages;
    nsCOMPtr<nsIRDFNode> newHasUnreadMessages;
    if(oldValue <=0 && newValue >0)
    {
      oldHasUnreadMessages = kFalseLiteral;
      newHasUnreadMessages = kTrueLiteral;
      NotifyPropertyChanged(folderResource, kNC_HasUnreadMessages, newHasUnreadMessages);
    }
    else if(oldValue > 0 && newValue <= 0)
    {
      newHasUnreadMessages = kFalseLiteral;
      NotifyPropertyChanged(folderResource, kNC_HasUnreadMessages, newHasUnreadMessages);
    }
  }
  return NS_OK;
}

**/
nsresult
nsMsgFolderDataSource::OnFolderSortOrderPropertyChanged(nsIRDFResource *folderResource, int32_t oldValue, int32_t newValue)
{
  nsCOMPtr<nsIMsgFolder> folder(do_QueryInterface(folderResource));
  if (folder)
  {
    nsCOMPtr<nsIRDFNode> newNode;
    createFolderNameNode(folder, getter_AddRefs(newNode), true);
    NotifyPropertyChanged(folderResource, kNC_FolderTreeNameSort, newNode);
  }
  return NS_OK;
}

nsresult
nsMsgFolderDataSource::OnFolderSizePropertyChanged(nsIRDFResource *folderResource, int32_t oldValue, int32_t newValue)
{
  nsCOMPtr<nsIRDFNode> newNode;
  GetFolderSizeNode(newValue, getter_AddRefs(newNode));
  NotifyPropertyChanged(folderResource, kNC_FolderSize, newNode);
  return NS_OK;
}

nsresult
nsMsgFolderDataSource::OnTotalMessagePropertyChanged(nsIRDFResource *folderResource, int32_t oldValue, int32_t newValue)
{
  nsCOMPtr<nsIRDFNode> newNode;
  GetNumMessagesNode(newValue, getter_AddRefs(newNode));
  NotifyPropertyChanged(folderResource, kNC_TotalMessages, newNode);
  return NS_OK;
}

nsresult
nsMsgFolderDataSource::GetNumMessagesNode(int32_t aNumMessages, nsIRDFNode **node)
{
  uint32_t numMessages = aNumMessages;
  if(numMessages == kDisplayQuestionCount)
    createNode(NS_LITERAL_STRING("???").get(), node, getRDFService());
  else if (numMessages == kDisplayBlankCount || numMessages == 0)
    createNode(EmptyString().get(), node, getRDFService());
  else
    createIntNode(numMessages, node, getRDFService());
  return NS_OK;
}

nsresult
nsMsgFolderDataSource::GetFolderSizeNode(int32_t aFolderSize, nsIRDFNode **aNode)
{
  nsresult rv;
  uint32_t folderSize = aFolderSize;
  if (folderSize == kDisplayBlankCount || folderSize == 0)
    createNode(EmptyString().get(), aNode, getRDFService());
  else if(folderSize == kDisplayQuestionCount)
    createNode(NS_LITERAL_STRING("???").get(), aNode, getRDFService());
  else
  {
    nsAutoString sizeString;
    rv = FormatFileSize(folderSize, true, sizeString);
    NS_ENSURE_SUCCESS(rv, rv);

    createNode(sizeString.get(), aNode, getRDFService());
  }
  return NS_OK;
}

nsresult
nsMsgFolderDataSource::createFolderChildNode(nsIMsgFolder *folder,
                                             nsIRDFNode **target)
{
  nsCOMPtr<nsISimpleEnumerator> subFolders;
  nsresult rv = folder->GetSubFolders(getter_AddRefs(subFolders));
  if (NS_FAILED(rv))
    return NS_RDF_NO_VALUE;

  bool hasMore;
  rv = subFolders->HasMoreElements(&hasMore);
  if (NS_FAILED(rv) || !hasMore)
    return NS_RDF_NO_VALUE;

  nsCOMPtr<nsISupports> firstFolder;
  rv = subFolders->GetNext(getter_AddRefs(firstFolder));
  if (NS_FAILED(rv))
    return NS_RDF_NO_VALUE;

  return CallQueryInterface(firstFolder, target);
}


nsresult nsMsgFolderDataSource::DoCopyToFolder(nsIMsgFolder *dstFolder, nsISupportsArray *arguments,
                         nsIMsgWindow *msgWindow, bool isMove)
{
  nsresult rv;
  uint32_t itemCount;
  rv = arguments->Count(&itemCount);
  NS_ENSURE_SUCCESS(rv, rv);

  //need source folder and at least one item to copy
  if(itemCount < 2)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIMsgFolder> srcFolder(do_QueryElementAt(arguments, 0));
  if(!srcFolder)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIMutableArray> messageArray(do_CreateInstance(NS_ARRAY_CONTRACTID));

  // Remove first element
  for(uint32_t i = 1; i < itemCount; i++)
  {
    nsCOMPtr<nsIMsgDBHdr> message(do_QueryElementAt(arguments, i));
    if (message)
    {
      messageArray->AppendElement(message, false);
    }
  }

  //Call copyservice with dstFolder, srcFolder, messages, isMove, and txnManager
  nsCOMPtr<nsIMsgCopyService> copyService =
    do_GetService(NS_MSGCOPYSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  return copyService->CopyMessages(srcFolder, messageArray, dstFolder, isMove,
    nullptr, msgWindow, true/* allowUndo */);
}

nsresult nsMsgFolderDataSource::DoFolderCopyToFolder(nsIMsgFolder *dstFolder, nsISupportsArray *arguments,
                                                     nsIMsgWindow *msgWindow, bool isMoveFolder)
{
  nsresult rv;
  uint32_t itemCount;
  rv = arguments->Count(&itemCount);
  NS_ENSURE_SUCCESS(rv, rv);

  //need at least one item to copy
  if(itemCount < 1)
    return NS_ERROR_FAILURE;

  if (!isMoveFolder)   // copy folder not on the same server
  {
    // Create an nsIMutableArray from the nsISupportsArray
    nsCOMPtr<nsIMutableArray> folderArray(do_CreateInstance(NS_ARRAY_CONTRACTID));
    for (uint32_t i = 0; i < itemCount; i++)
      folderArray->AppendElement(arguments->ElementAt(i), false);

    //Call copyservice with dstFolder, srcFolder, folders and isMoveFolder
    nsCOMPtr<nsIMsgCopyService> copyService = do_GetService(NS_MSGCOPYSERVICE_CONTRACTID, &rv);
    if(NS_SUCCEEDED(rv))
    {
      rv = copyService->CopyFolders(folderArray, dstFolder, isMoveFolder,
        nullptr, msgWindow);

    }
  }
  else    //within the same server therefore no need for copy service
  {

    nsCOMPtr<nsIMsgFolder> msgFolder;
    for (uint32_t i=0;i< itemCount; i++)
    {
      msgFolder = do_QueryElementAt(arguments, i, &rv);
      if (NS_SUCCEEDED(rv))
      {
        rv = dstFolder->CopyFolder(msgFolder, isMoveFolder , msgWindow, nullptr);
        NS_ASSERTION((NS_SUCCEEDED(rv)),"Copy folder failed.");
      }
    }
  }

  return rv;
  //return NS_OK;
}

nsresult nsMsgFolderDataSource::DoDeleteFromFolder(nsIMsgFolder *folder, nsISupportsArray *arguments,
                                                   nsIMsgWindow *msgWindow, bool reallyDelete)
{
  nsresult rv = NS_OK;
  uint32_t itemCount;
  rv = arguments->Count(&itemCount);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIMutableArray> messageArray(do_CreateInstance(NS_ARRAY_CONTRACTID));
  nsCOMPtr<nsIMutableArray> folderArray(do_CreateInstance(NS_ARRAY_CONTRACTID));

  //Split up deleted items into different type arrays to be passed to the folder
  //for deletion.
  for(uint32_t item = 0; item < itemCount; item++)
  {
    nsCOMPtr<nsISupports> supports(do_QueryElementAt(arguments, item));
    nsCOMPtr<nsIMsgDBHdr> deletedMessage(do_QueryInterface(supports));
    nsCOMPtr<nsIMsgFolder> deletedFolder(do_QueryInterface(supports));
    if (deletedMessage)
    {
      messageArray->AppendElement(supports, false);
    }
    else if(deletedFolder)
    {
      folderArray->AppendElement(supports, false);
    }
  }
  uint32_t cnt;
  rv = messageArray->GetLength(&cnt);
  if (NS_FAILED(rv)) return rv;
  if (cnt > 0)
    rv = folder->DeleteMessages(messageArray, msgWindow, reallyDelete, false, nullptr, true /*allowUndo*/);

  rv = folderArray->GetLength(&cnt);
  if (NS_FAILED(rv)) return rv;
  if (cnt > 0)
  {
    nsCOMPtr<nsIMsgFolder> folderToDelete = do_QueryElementAt(folderArray, 0);
    uint32_t folderFlags = 0;
    if (folderToDelete)
    {
      folderToDelete->GetFlags(&folderFlags);
      if (folderFlags & nsMsgFolderFlags::Virtual)
      {
        NS_ENSURE_ARG_POINTER(msgWindow);
        nsCOMPtr<nsIStringBundleService> sBundleService =
          mozilla::services::GetStringBundleService();
        NS_ENSURE_TRUE(sBundleService, NS_ERROR_UNEXPECTED);
        nsCOMPtr<nsIStringBundle> sMessengerStringBundle;
        nsString confirmMsg;
        rv = sBundleService->CreateBundle(MESSENGER_STRING_URL, getter_AddRefs(sMessengerStringBundle));
        NS_ENSURE_SUCCESS(rv, rv);
        sMessengerStringBundle->GetStringFromName(NS_LITERAL_STRING("confirmSavedSearchDeleteMessage").get(), getter_Copies(confirmMsg));

        nsCOMPtr<nsIPrompt> dialog;
        rv = msgWindow->GetPromptDialog(getter_AddRefs(dialog));
        if (NS_SUCCEEDED(rv))
        {
          bool dialogResult;
          rv = dialog->Confirm(nullptr, confirmMsg.get(), &dialogResult);
          if (!dialogResult)
            return NS_OK;
        }
      }
    }
    rv = folder->DeleteSubFolders(folderArray, msgWindow);
  }
  return rv;
}

nsresult nsMsgFolderDataSource::DoNewFolder(nsIMsgFolder *folder, nsISupportsArray *arguments, nsIMsgWindow *window)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIRDFLiteral> literal = do_QueryElementAt(arguments, 0, &rv);
  if(NS_SUCCEEDED(rv))
  {
    nsString name;
    literal->GetValue(getter_Copies(name));
    rv = folder->CreateSubfolder(name, window);
  }
  return rv;
}

nsresult nsMsgFolderDataSource::DoFolderAssert(nsIMsgFolder *folder, nsIRDFResource *property, nsIRDFNode *target)
{
  nsresult rv = NS_ERROR_FAILURE;

  if((kNC_Charset == property))
  {
    nsCOMPtr<nsIRDFLiteral> literal(do_QueryInterface(target));
    if(literal)
    {
      const PRUnichar* value;
      rv = literal->GetValueConst(&value);
      if(NS_SUCCEEDED(rv))
        rv = folder->SetCharset(NS_LossyConvertUTF16toASCII(value));
    }
    else
      rv = NS_ERROR_FAILURE;
  }
  else if (kNC_Open == property && target == kTrueLiteral)
      rv = folder->ClearFlag(nsMsgFolderFlags::Elided);

  return rv;
}

nsresult nsMsgFolderDataSource::DoFolderUnassert(nsIMsgFolder *folder, nsIRDFResource *property, nsIRDFNode *target)
{
  nsresult rv = NS_ERROR_FAILURE;

  if((kNC_Open == property) && target == kTrueLiteral)
      rv = folder->SetFlag(nsMsgFolderFlags::Elided);

  return rv;
}

nsresult nsMsgFolderDataSource::DoFolderHasAssertion(nsIMsgFolder *folder,
                                                     nsIRDFResource *property,
                                                     nsIRDFNode *target,
                                                     bool tv,
                                                     bool *hasAssertion)
{
  nsresult rv = NS_OK;
  if(!hasAssertion)
    return NS_ERROR_NULL_POINTER;

  //We're not keeping track of negative assertions on folders.
  if(!tv)
  {
    *hasAssertion = false;
    return NS_OK;
  }

  if((kNC_Child == property))
  {
    nsCOMPtr<nsIMsgFolder> childFolder(do_QueryInterface(target, &rv));
    if(NS_SUCCEEDED(rv))
    {
      nsCOMPtr<nsIMsgFolder> childsParent;
      rv = childFolder->GetParent(getter_AddRefs(childsParent));
      *hasAssertion = (NS_SUCCEEDED(rv) && childsParent && folder
        && (childsParent.get() == folder));
    }
  }
  else if ((kNC_Name == property) ||
    (kNC_Open == property) ||
    (kNC_FolderTreeName == property) ||
    (kNC_FolderTreeSimpleName == property) ||
    (kNC_SpecialFolder == property) ||
    (kNC_ServerType == property) ||
    (kNC_IsDeferred == property) ||
    (kNC_CanCreateFoldersOnServer == property) ||
    (kNC_CanFileMessagesOnServer == property) ||
    (kNC_IsServer == property) ||
    (kNC_IsSecure == property) ||
    (kNC_CanSubscribe == property) ||
    (kNC_SupportsOffline == property) ||
    (kNC_CanFileMessages == property) ||
    (kNC_CanCreateSubfolders == property) ||
    (kNC_CanRename == property) ||
    (kNC_CanCompact == property) ||
    (kNC_TotalMessages == property) ||
    (kNC_TotalUnreadMessages == property) ||
    (kNC_FolderSize == property) ||
    (kNC_Charset == property) ||
    (kNC_BiffState == property) ||
    (kNC_HasUnreadMessages == property) ||
    (kNC_NoSelect == property)  ||
    (kNC_Synchronize == property) ||
    (kNC_SyncDisabled == property) ||
    (kNC_VirtualFolder == property) ||
    (kNC_CanSearchMessages == property))
  {
    nsCOMPtr<nsIRDFResource> folderResource(do_QueryInterface(folder, &rv));

    if(NS_FAILED(rv))
      return rv;

    rv = GetTargetHasAssertion(this, folderResource, property, tv, target, hasAssertion);
  }
  else
    *hasAssertion = false;
  return rv;
}

nsMsgFlatFolderDataSource::nsMsgFlatFolderDataSource()
{
  m_builtFolders = false;
}

nsMsgFlatFolderDataSource::~nsMsgFlatFolderDataSource()
{
}

nsresult nsMsgFlatFolderDataSource::Init()
{
  nsIRDFService* rdf = getRDFService();
  NS_ENSURE_TRUE(rdf, NS_ERROR_FAILURE);
  nsCOMPtr<nsIRDFResource> source;
  nsCAutoString dsUri(m_dsName);
  dsUri.Append(":/");
  rdf->GetResource(dsUri, getter_AddRefs(m_rootResource));

  return nsMsgFolderDataSource::Init();
}

void nsMsgFlatFolderDataSource::Cleanup()
{
  m_folders.Clear();
  m_builtFolders = false;
  nsMsgFolderDataSource::Cleanup();
}

NS_IMETHODIMP nsMsgFlatFolderDataSource::GetTarget(nsIRDFResource* source,
                       nsIRDFResource* property,
                       bool tv,
                       nsIRDFNode** target)
{
  return (property == kNC_Child)
    ? NS_RDF_NO_VALUE
    : nsMsgFolderDataSource::GetTarget(source, property, tv, target);
}


NS_IMETHODIMP nsMsgFlatFolderDataSource::GetTargets(nsIRDFResource* source,
                                                nsIRDFResource* property,
                                                bool tv,
                                                nsISimpleEnumerator** targets)
{
  if (kNC_Child != property)
    return nsMsgFolderDataSource::GetTargets(source, property, tv, targets);

  if(!targets)
    return NS_ERROR_NULL_POINTER;

  if (ResourceIsOurRoot(source))
  {
    EnsureFolders();
    return NS_NewArrayEnumerator(targets, m_folders);
  }
  return NS_NewSingletonEnumerator(targets, property);
}

void nsMsgFlatFolderDataSource::EnsureFolders()
{
  if (!m_builtFolders)
  {
    m_builtFolders = true; // in case something goes wrong

    // need an enumerator that gives all folders with unread
    nsresult rv;
    nsCOMPtr <nsIMsgAccountManager> accountManager = do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
    if (NS_FAILED(rv))
      return;

    nsCOMPtr<nsISupportsArray> allServers;
    rv = accountManager->GetAllServers(getter_AddRefs(allServers));
    nsCOMPtr <nsISupportsArray> allFolders = do_CreateInstance(NS_SUPPORTSARRAY_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv) && allServers)
    {
      uint32_t count = 0;
      allServers->Count(&count);
      uint32_t i;
      for (i = 0; i < count; i++)
      {
        nsCOMPtr<nsIMsgIncomingServer> server = do_QueryElementAt(allServers, i);
        if (server)
        {
          nsCOMPtr <nsIMsgFolder> rootFolder;
          server->GetRootFolder(getter_AddRefs(rootFolder));
          if (rootFolder)
          {
            nsCOMPtr<nsISimpleEnumerator> subFolders;
            rv = rootFolder->GetSubFolders(getter_AddRefs(subFolders));

            uint32_t lastEntry;
            allFolders->Count(&lastEntry);
            rv = rootFolder->ListDescendents(allFolders);
            uint32_t newLastEntry;
            allFolders->Count(&newLastEntry);
            for (uint32_t newEntryIndex = lastEntry; newEntryIndex < newLastEntry; newEntryIndex++)
            {
              nsCOMPtr <nsIMsgFolder> curFolder = do_QueryElementAt(allFolders, newEntryIndex);
              if (WantsThisFolder(curFolder))
              {
                m_folders.AppendObject(curFolder);
              }
            }
          }
        }
      }
    }
  }
}


NS_IMETHODIMP nsMsgFlatFolderDataSource::GetURI(char* *aUri)
{
  nsCAutoString uri("rdf:");
  uri.Append(m_dsName);
  return (*aUri = ToNewCString(uri))
    ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP nsMsgFlatFolderDataSource::HasAssertion(nsIRDFResource* source,
                            nsIRDFResource* property,
                            nsIRDFNode* target,
                            bool tv,
                            bool* hasAssertion)
{
  nsresult rv;

  nsCOMPtr<nsIMsgFolder> folder(do_QueryInterface(source, &rv));
  // we need to check if the folder belongs in this datasource.
  if (NS_SUCCEEDED(rv) && property != kNC_Open && property != kNC_Child)
  {
    if (WantsThisFolder(folder) && (kNC_Child != property))
      return DoFolderHasAssertion(folder, property, target, tv, hasAssertion);
  }
  else if (property == kNC_Child && ResourceIsOurRoot(source)) // check if source is us
  {
    folder = do_QueryInterface(target);
    if (folder)
    {
      nsCOMPtr<nsIMsgFolder> parentMsgFolder;
      folder->GetParent(getter_AddRefs(parentMsgFolder));
      // a folder without a parent must be getting deleted as part of
      // the rename operation and is thus a folder we are
      // no longer interested in
      if (parentMsgFolder && WantsThisFolder(folder))
      {
        *hasAssertion = true;
        return NS_OK;
      }
    }
  }
  *hasAssertion = false;
  return NS_OK;
}

nsresult nsMsgFlatFolderDataSource::OnItemAddedOrRemoved(nsIMsgFolder *parentItem, nsISupports *item, bool added)
{
  // When a folder is added or removed, parentItem is the parent folder and item is the folder being
  // added or removed. In a flat data source, there is no arc in the graph between the parent folder
  // and the folder being added or removed. Our flat data source root (i.e. mailnewsunreadfolders:/) has
  // an arc with the child property to every folder in the data source.  We must change parentItem
  // to be our data source root before calling nsMsgFolderDataSource::OnItemAddedOrRemoved. This ensures
  // that datasource listeners such as the template builder properly handle add and remove
  // notifications on the flat datasource.
  nsCOMPtr<nsIRDFNode> itemNode(do_QueryInterface(item));
  if (itemNode)
    NotifyObservers(m_rootResource, kNC_Child, itemNode, nullptr, added, false);
  return NS_OK;
}

bool nsMsgFlatFolderDataSource::ResourceIsOurRoot(nsIRDFResource *resource)
{
  return m_rootResource.get() == resource;
}

bool nsMsgFlatFolderDataSource::WantsThisFolder(nsIMsgFolder *folder)
{
  EnsureFolders();
  return m_folders.IndexOf(folder) != kNotFound;
}

nsresult nsMsgFlatFolderDataSource::GetFolderDisplayName(nsIMsgFolder *folder, nsString& folderName)
{
  folder->GetName(folderName);
  uint32_t foldersCount = m_folders.Count();
  nsString otherFolderName;
  for (uint32_t index = 0; index < foldersCount; index++)
  {
    if (folder == m_folders[index]) // ignore ourselves.
      continue;
    m_folders[index]->GetName(otherFolderName);
    if (otherFolderName.Equals(folderName))
    {
      nsCOMPtr <nsIMsgIncomingServer> server;
      folder->GetServer(getter_AddRefs(server));
      if (server)
      {
        nsString serverName;
        server->GetPrettyName(serverName);
        folderName.AppendLiteral(" - ");
        folderName.Append(serverName);
        return NS_OK;
      }
    }
  }
  // check if folder name is unique - if not, append account name
  return folder->GetAbbreviatedName(folderName);
}


bool nsMsgUnreadFoldersDataSource::WantsThisFolder(nsIMsgFolder *folder)
{
  int32_t numUnread;
  folder->GetNumUnread(false, &numUnread);
  return numUnread > 0;
}

nsresult nsMsgUnreadFoldersDataSource::NotifyPropertyChanged(nsIRDFResource *resource,
                    nsIRDFResource *property, nsIRDFNode *newNode,
                    nsIRDFNode *oldNode)
{
  // check if it's the has unread property that's changed; if so, see if we need
  // to add this folder to the view.
  // Then, call base class.
  if (kNC_HasUnreadMessages == property)
  {
    nsCOMPtr<nsIMsgFolder> folder(do_QueryInterface(resource));
    if (folder)
    {
      int32_t numUnread;
      folder->GetNumUnread(false, &numUnread);
      if (numUnread > 0)
      {
        if (m_folders.IndexOf(folder) == kNotFound)
          m_folders.AppendObject(folder);
        NotifyObservers(kNC_UnreadFolders, kNC_Child, resource, nullptr, true, false);
      }
    }
  }
  return nsMsgFolderDataSource::NotifyPropertyChanged(resource, property,
                                                newNode, oldNode);
}

bool nsMsgFavoriteFoldersDataSource::WantsThisFolder(nsIMsgFolder *folder)
{
  uint32_t folderFlags;
  folder->GetFlags(&folderFlags);
  return folderFlags & nsMsgFolderFlags::Favorite;
}


void nsMsgRecentFoldersDataSource::Cleanup()
{
  m_cutOffDate = 0;
  nsMsgFlatFolderDataSource::Cleanup();
}


void nsMsgRecentFoldersDataSource::EnsureFolders()
{
  if (!m_builtFolders)
  {
    m_builtFolders = true;

    nsresult rv;
    nsCOMPtr <nsIMsgAccountManager> accountManager = do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
    if (NS_FAILED(rv))
      return;

    nsCOMPtr<nsISupportsArray> allServers;
    rv = accountManager->GetAllServers(getter_AddRefs(allServers));
    nsCOMPtr <nsISupportsArray> allFolders = do_CreateInstance(NS_SUPPORTSARRAY_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv) && allServers)
    {
      uint32_t count = 0;
      allServers->Count(&count);
      uint32_t i;
      for (i = 0; i < count; i++)
      {
        nsCOMPtr<nsIMsgIncomingServer> server = do_QueryElementAt(allServers, i);
        if (server)
        {
          nsCOMPtr <nsIMsgFolder> rootFolder;
          server->GetRootFolder(getter_AddRefs(rootFolder));
          if (rootFolder)
          {
            nsCOMPtr<nsISimpleEnumerator> subFolders;
            rv = rootFolder->GetSubFolders(getter_AddRefs(subFolders));

            uint32_t lastEntry;
            allFolders->Count(&lastEntry);
            rv = rootFolder->ListDescendents(allFolders);
            uint32_t newLastEntry;
            allFolders->Count(&newLastEntry);
            for (uint32_t newEntryIndex = lastEntry; newEntryIndex < newLastEntry; newEntryIndex++)
            {
              nsCOMPtr <nsIMsgFolder> curFolder = do_QueryElementAt(allFolders, newEntryIndex);
              nsCString dateStr;
              nsresult err;
              curFolder->GetStringProperty(MRU_TIME_PROPERTY, dateStr);
              uint32_t curFolderDate = (uint32_t) dateStr.ToInteger(&err);
              if (err)
                curFolderDate = 0;
              if (curFolderDate > m_cutOffDate)
              {
                // if m_folders is "full", replace oldest folder with this folder,
                // and adjust m_cutOffDate so that it's the mrutime
                // of the "new" oldest folder.
                uint32_t curFaveFoldersCount = m_folders.Count();
                if (curFaveFoldersCount > m_maxNumFolders)
                {
                  uint32_t indexOfOldestFolder = 0;
                  uint32_t oldestFaveDate = 0;
                  uint32_t newOldestFaveDate = 0;
                  for (uint32_t index = 0; index < curFaveFoldersCount; )
                  {
                    nsCString curFaveFolderDateStr;
                    m_folders[index]->GetStringProperty(MRU_TIME_PROPERTY, curFaveFolderDateStr);
                    uint32_t curFaveFolderDate = (uint32_t) curFaveFolderDateStr.ToInteger(&err);
                    if (!oldestFaveDate || curFaveFolderDate < oldestFaveDate)
                    {
                      indexOfOldestFolder = index;
                      newOldestFaveDate = oldestFaveDate;
                      oldestFaveDate = curFaveFolderDate;
                    }
                    if (!newOldestFaveDate || (index != indexOfOldestFolder
                                                && curFaveFolderDate < newOldestFaveDate))
                      newOldestFaveDate = curFaveFolderDate;
                    index++;
                  }
                  if (curFolderDate > oldestFaveDate && m_folders.IndexOf(curFolder) == kNotFound)
                    m_folders.ReplaceObjectAt(curFolder, indexOfOldestFolder);

                  NS_ASSERTION(newOldestFaveDate >= m_cutOffDate, "cutoff date should be getting bigger");
                  m_cutOffDate = newOldestFaveDate;
                }
                else if (m_folders.IndexOf(curFolder) == kNotFound)
                  m_folders.AppendObject(curFolder);
              }
#ifdef DEBUG_David_Bienvenu
              else
              {
                for (uint32_t index = 0; index < m_folders.Count(); index++)
                {
                  nsCString curFaveFolderDateStr;
                  m_folders[index]->GetStringProperty(MRU_TIME_PROPERTY, curFaveFolderDateStr);
                  uint32_t curFaveFolderDate = (uint32_t) curFaveFolderDateStr.ToInteger(&err);
                  NS_ASSERTION(curFaveFolderDate > curFolderDate, "folder newer then faves but not added");
                }
              }
#endif
            }
          }
        }
      }
    }
  }
}

NS_IMETHODIMP nsMsgRecentFoldersDataSource::OnItemAdded(nsIMsgFolder *parentItem, nsISupports *item)
{
  // if we've already built the recent folder array, we should add this item to the array
  // since just added items are by definition new.
  // I think this means newly discovered imap folders (ones w/o msf files) will
  // get added, but maybe that's OK.
  if (m_builtFolders)
  {
    nsCOMPtr<nsIMsgFolder> folder(do_QueryInterface(item));
    if (folder && m_folders.IndexOf(folder) == kNotFound)
    {
      m_folders.AppendObject(folder);
      nsCOMPtr<nsIRDFResource> resource = do_QueryInterface(item);
      NotifyObservers(kNC_RecentFolders, kNC_Child, resource, nullptr, true, false);
    }
  }
  return nsMsgFlatFolderDataSource::OnItemAdded(parentItem, item);
}

nsresult nsMsgRecentFoldersDataSource::NotifyPropertyChanged(nsIRDFResource *resource,
                    nsIRDFResource *property, nsIRDFNode *newNode,
                    nsIRDFNode *oldNode)
{
  // check if it's the has new property that's changed; if so, see if we need
  // to add this folder to the view.
  // Then, call base class.
  if (kNC_NewMessages == property)
  {
    nsCOMPtr<nsIMsgFolder> folder(do_QueryInterface(resource));
    if (folder)
    {
      bool hasNewMessages;
      folder->GetHasNewMessages(&hasNewMessages);
      if (hasNewMessages > 0)
      {
        if (m_folders.IndexOf(folder) == kNotFound)
        {
          m_folders.AppendObject(folder);
          NotifyObservers(kNC_RecentFolders, kNC_Child, resource, nullptr, true, false);
        }
      }
    }
  }
  return nsMsgFolderDataSource::NotifyPropertyChanged(resource, property,
                                                newNode, oldNode);
}
