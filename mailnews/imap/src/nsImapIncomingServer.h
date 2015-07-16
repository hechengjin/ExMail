/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsImapIncomingServer_h
#define __nsImapIncomingServer_h

#include "msgCore.h"
#include "nsIImapIncomingServer.h"
#include "nsMsgIncomingServer.h"
#include "nsIImapServerSink.h"
#include "nsIStringBundle.h"
#include "nsISubscribableServer.h"
#include "nsIUrlListener.h"
#include "nsIMsgImapMailFolder.h"
#include "nsCOMArray.h"
#include "mozilla/Mutex.h"

class nsIRDFService;

/* get some implementation from nsMsgIncomingServer */
class nsImapIncomingServer : public nsMsgIncomingServer,
                             public nsIImapIncomingServer,
                             public nsIImapServerSink,
                             public nsISubscribableServer,
                             public nsIUrlListener
{
public:
    NS_DECL_ISUPPORTS_INHERITED

    nsImapIncomingServer();
    virtual ~nsImapIncomingServer();

    // overriding nsMsgIncomingServer methods
  NS_IMETHOD SetKey(const nsACString& aKey);  // override nsMsgIncomingServer's implementation...
  NS_IMETHOD GetLocalStoreType(nsACString& type);

  NS_DECL_NSIIMAPINCOMINGSERVER
  NS_DECL_NSIIMAPSERVERSINK
  NS_DECL_NSISUBSCRIBABLESERVER
  NS_DECL_NSIURLLISTENER

  NS_IMETHOD PerformBiff(nsIMsgWindow *aMsgWindow);
  NS_IMETHOD PerformExpand(nsIMsgWindow *aMsgWindow);
  NS_IMETHOD CloseCachedConnections();
  NS_IMETHOD GetConstructedPrettyName(nsAString& retval);
  NS_IMETHOD GetCanBeDefaultServer(bool *canBeDefaultServer);
  NS_IMETHOD GetCanCompactFoldersOnServer(bool *canCompactFoldersOnServer);
  NS_IMETHOD GetCanUndoDeleteOnServer(bool *canUndoDeleteOnServer);
  NS_IMETHOD GetCanSearchMessages(bool *canSearchMessages);
  NS_IMETHOD GetCanEmptyTrashOnExit(bool *canEmptyTrashOnExit);
  NS_IMETHOD GetOfflineSupportLevel(int32_t *aSupportLevel);
  NS_IMETHOD GeneratePrettyNameForMigration(nsAString& aPrettyName);
  NS_IMETHOD GetSupportsDiskSpace(bool *aSupportsDiskSpace);
  NS_IMETHOD GetCanCreateFoldersOnServer(bool *aCanCreateFoldersOnServer);
  NS_IMETHOD GetCanFileMessagesOnServer(bool *aCanFileMessagesOnServer);
  NS_IMETHOD GetFilterScope(nsMsgSearchScopeValue *filterScope);
  NS_IMETHOD GetSearchScope(nsMsgSearchScopeValue *searchScope);
  NS_IMETHOD GetServerRequiresPasswordForBiff(bool *aServerRequiresPasswordForBiff);
  NS_IMETHOD OnUserOrHostNameChanged(const nsACString& oldName,
                                     const nsACString& newName,
                                     bool hostnameChanged);
  NS_IMETHOD GetNumIdleConnections(int32_t *aNumIdleConnections);
  NS_IMETHOD ForgetSessionPassword();
  NS_IMETHOD GetMsgFolderFromURI(nsIMsgFolder *aFolderResource, const nsACString& aURI, nsIMsgFolder **aFolder);
  NS_IMETHOD SetSocketType(int32_t aSocketType);
  NS_IMETHOD VerifyLogon(nsIUrlListener *aUrlListener, nsIMsgWindow *aMsgWindow,
                         nsIURI **aURL);

protected:
  nsresult GetFolder(const nsACString& name, nsIMsgFolder** pFolder);
  virtual nsresult CreateRootFolderFromUri(const nsCString &serverUri,
                                           nsIMsgFolder **rootFolder);
  nsresult ResetFoldersToUnverified(nsIMsgFolder *parentFolder);
  void GetUnverifiedSubFolders(nsIMsgFolder *parentFolder,
                               nsCOMArray<nsIMsgImapMailFolder> &aFoldersArray);
  void GetUnverifiedFolders(nsCOMArray<nsIMsgImapMailFolder> &aFolderArray);
  nsresult DeleteNonVerifiedFolders(nsIMsgFolder *parentFolder);
  bool NoDescendentsAreVerified(nsIMsgFolder *parentFolder);
  bool AllDescendentsAreNoSelect(nsIMsgFolder *parentFolder);

  nsresult GetStringBundle();
  nsString GetImapStringByName(const nsString &aName);
  static nsresult AlertUser(const nsAString& aString, nsIMsgMailNewsUrl *aUrl);

private:
  void OnlyOneTrashFolder(nsCOMPtr<nsIMsgFolder> aRootMsgFolder);
  bool GetSentMailFolderName(char *folderName);
  bool GetDraftFolderName(char *folderName);
  bool GetJunkFolderName(nsCOMPtr<nsIMsgFolder> aRootMsgFolder, char *folderName);
  void OnlyOneSpecialFolder(nsCOMPtr<nsIMsgFolder> aRootMsgFolder, uint32_t folderFlag);
  
  nsCString DropPath(nsCString URI);
  
  nsresult SubscribeToFolder(const PRUnichar *aName, bool subscribe);
  nsresult GetImapConnection(nsIImapUrl* aImapUrl,
                             nsIImapProtocol** aImapConnection);
  nsresult CreateProtocolInstance(nsIImapProtocol ** aImapConnection);
  nsresult CreateHostSpecificPrefName(const char *prefPrefix, nsCAutoString &prefName);

  nsresult DoomUrlIfChannelHasError(nsIImapUrl *aImapUrl, bool *urlDoomed);
  bool ConnectionTimeOut(nsIImapProtocol* aImapConnection);
  nsresult GetFormattedStringFromID(const nsAString& aValue, int32_t aID, nsAString& aResult);
  nsresult GetPrefForServerAttribute(const char *prefSuffix, bool *prefValue);
  bool CheckSpecialFolder(nsIRDFService *rdf, nsCString &folderUri,
                            uint32_t folderFlag, nsCString &existingUri);

  nsCOMArray<nsIImapProtocol> m_connectionCache;
  nsCOMArray<nsIImapUrl> m_urlQueue;
  nsCOMPtr<nsIStringBundle>	m_stringBundle;
  nsCOMArray<nsIMsgFolder> m_subscribeFolders; // used to keep folder resources around while subscribe UI is up.
  nsCOMArray<nsIMsgImapMailFolder> m_foldersToStat; // folders to check for new mail with Status
  nsVoidArray       m_urlConsumers;
  eIMAPCapabilityFlags          m_capability;
  nsCString         m_manageMailAccountUrl;
  bool              m_userAuthenticated;
  bool              mDoingSubscribeDialog;
  bool              mDoingLsub;
  bool              m_shuttingDown;

  mozilla::Mutex mLock;
  // subscribe dialog stuff
  nsresult AddFolderToSubscribeDialog(const char *parentUri, const char *uri,const char *folderName);
  nsCOMPtr <nsISubscribableServer> mInner;
  nsresult EnsureInner();
  nsresult ClearInner();

  // Utility function for checking folder existence
  nsresult GetExistingMsgFolder(const nsACString& aURI,
                                nsACString& folderUriWithNamespace,
                                bool& namespacePrefixAdded,
                                bool caseInsensitive,
                                nsIMsgFolder **aFolder);
};

#endif
