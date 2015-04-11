/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSpamSettings.h"
#include "nsISupportsObsolete.h"
#include "nsIFile.h"
#include "plstr.h"
#include "prmem.h"
#include "nsIMsgHdr.h"
#include "nsNetUtil.h"
#include "nsIMsgFolder.h"
#include "nsMsgUtils.h"
#include "nsMsgFolderFlags.h"
#include "nsImapCore.h"
#include "nsIImapIncomingServer.h"
#include "nsIRDFService.h"
#include "nsIRDFResource.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIStringBundle.h"
#include "nsDateTimeFormatCID.h"
#include "mozilla/Services.h"

#include "nsMailDirServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsISimpleEnumerator.h"
#include "nsIDirectoryEnumerator.h"
#include "nsIMsgHeaderParser.h"
#include "nsAbBaseCID.h"
#include "nsIAbManager.h"
#include "nsIMsgAccountManager.h"
#include "nsMsgBaseCID.h"

nsSpamSettings::nsSpamSettings()
{
  mLevel = 0;
  mMoveOnSpam = false;
  mMoveTargetMode = nsISpamSettings::MOVE_TARGET_MODE_ACCOUNT;
  mPurge = false;
  mPurgeInterval = 14; // 14 days

  mServerFilterTrustFlags = 0;

  mUseWhiteList = false;
  mUseServerFilter = false;

  nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(mLogFile));
  if (NS_SUCCEEDED(rv))
    mLogFile->Append(NS_LITERAL_STRING("junklog.html"));
}

nsSpamSettings::~nsSpamSettings()
{
}

NS_IMPL_ISUPPORTS2(nsSpamSettings, nsISpamSettings, nsIUrlListener)

NS_IMETHODIMP
nsSpamSettings::GetLevel(int32_t *aLevel)
{
  NS_ENSURE_ARG_POINTER(aLevel);
  *aLevel = mLevel;
  return NS_OK;
}

NS_IMETHODIMP nsSpamSettings::SetLevel(int32_t aLevel)
{
  NS_ASSERTION((aLevel >= 0 && aLevel <= 100), "bad level");
  mLevel = aLevel;
  return NS_OK;
}

NS_IMETHODIMP
nsSpamSettings::GetMoveTargetMode(int32_t *aMoveTargetMode)
{
  NS_ENSURE_ARG_POINTER(aMoveTargetMode);
  *aMoveTargetMode = mMoveTargetMode;
  return NS_OK;
}

NS_IMETHODIMP nsSpamSettings::SetMoveTargetMode(int32_t aMoveTargetMode)
{
  NS_ASSERTION((aMoveTargetMode == nsISpamSettings::MOVE_TARGET_MODE_FOLDER || aMoveTargetMode == nsISpamSettings::MOVE_TARGET_MODE_ACCOUNT), "bad move target mode");
  mMoveTargetMode = aMoveTargetMode;
  return NS_OK;
}

NS_IMETHODIMP nsSpamSettings::GetManualMark(bool *aManualMark)
{
  NS_ENSURE_ARG_POINTER(aManualMark);
  nsresult rv;
  nsCOMPtr<nsIPrefBranch> prefBranch(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);
  return prefBranch->GetBoolPref("mail.spam.manualMark", aManualMark);
}

NS_IMETHODIMP nsSpamSettings::GetManualMarkMode(int32_t *aManualMarkMode)
{
  NS_ENSURE_ARG_POINTER(aManualMarkMode);
  nsresult rv;
  nsCOMPtr<nsIPrefBranch> prefBranch(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);
  return prefBranch->GetIntPref("mail.spam.manualMarkMode", aManualMarkMode);
}

NS_IMETHODIMP nsSpamSettings::GetLoggingEnabled(bool *aLoggingEnabled)
{
  NS_ENSURE_ARG_POINTER(aLoggingEnabled);
  nsresult rv;
  nsCOMPtr<nsIPrefBranch> prefBranch(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);
  return prefBranch->GetBoolPref("mail.spam.logging.enabled", aLoggingEnabled);
}

NS_IMETHODIMP nsSpamSettings::GetMarkAsReadOnSpam(bool *aMarkAsReadOnSpam)
{
  NS_ENSURE_ARG_POINTER(aMarkAsReadOnSpam);
  nsresult rv;
  nsCOMPtr<nsIPrefBranch> prefBranch(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);
  return prefBranch->GetBoolPref("mail.spam.markAsReadOnSpam", aMarkAsReadOnSpam);
}

NS_IMPL_GETSET(nsSpamSettings, MoveOnSpam, bool, mMoveOnSpam)
NS_IMPL_GETSET(nsSpamSettings, Purge, bool, mPurge)
NS_IMPL_GETSET(nsSpamSettings, UseWhiteList, bool, mUseWhiteList)
NS_IMPL_GETSET(nsSpamSettings, UseServerFilter, bool, mUseServerFilter)

NS_IMETHODIMP nsSpamSettings::GetWhiteListAbURI(char * *aWhiteListAbURI)
{
  NS_ENSURE_ARG_POINTER(aWhiteListAbURI);
  *aWhiteListAbURI = ToNewCString(mWhiteListAbURI);
  return NS_OK;
}
NS_IMETHODIMP nsSpamSettings::SetWhiteListAbURI(const char * aWhiteListAbURI)
{
  mWhiteListAbURI = aWhiteListAbURI;
  return NS_OK;
}

NS_IMETHODIMP nsSpamSettings::GetActionTargetAccount(char * *aActionTargetAccount)
{
  NS_ENSURE_ARG_POINTER(aActionTargetAccount);
  *aActionTargetAccount = ToNewCString(mActionTargetAccount);
  return NS_OK;
}

NS_IMETHODIMP nsSpamSettings::SetActionTargetAccount(const char * aActionTargetAccount)
{
  mActionTargetAccount = aActionTargetAccount;
  return NS_OK;
}

NS_IMETHODIMP nsSpamSettings::GetActionTargetFolder(char * *aActionTargetFolder)
{
  NS_ENSURE_ARG_POINTER(aActionTargetFolder);
  *aActionTargetFolder = ToNewCString(mActionTargetFolder);
  return NS_OK;
}

NS_IMETHODIMP nsSpamSettings::SetActionTargetFolder(const char * aActionTargetFolder)
{
  mActionTargetFolder = aActionTargetFolder;
  return NS_OK;
}

NS_IMETHODIMP nsSpamSettings::GetPurgeInterval(int32_t *aPurgeInterval)
{
  NS_ENSURE_ARG_POINTER(aPurgeInterval);
  *aPurgeInterval = mPurgeInterval;
  return NS_OK;
}

NS_IMETHODIMP nsSpamSettings::SetPurgeInterval(int32_t aPurgeInterval)
{
  NS_ASSERTION(aPurgeInterval >= 0, "bad purge interval");
  mPurgeInterval = aPurgeInterval;
  return NS_OK;
}

NS_IMETHODIMP
nsSpamSettings::SetLogStream(nsIOutputStream *aLogStream)
{
  // if there is a log stream already, close it
  if (mLogStream) {
    // will flush
    nsresult rv = mLogStream->Close();
    NS_ENSURE_SUCCESS(rv,rv);
  }

  mLogStream = aLogStream;
  return NS_OK;
}

#define LOG_HEADER "<head><meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\"></head>"
#define LOG_HEADER_LEN (strlen(LOG_HEADER))

NS_IMETHODIMP
nsSpamSettings::GetLogStream(nsIOutputStream **aLogStream)
{
  NS_ENSURE_ARG_POINTER(aLogStream);

  nsresult rv;

  if (!mLogStream) {
    // append to the end of the log file
    rv = MsgNewBufferedFileOutputStream(getter_AddRefs(mLogStream),
                                        mLogFile,
                                        PR_CREATE_FILE | PR_WRONLY | PR_APPEND,
                                        0600);
    NS_ENSURE_SUCCESS(rv, rv);

    int64_t fileSize;
    rv = mLogFile->GetFileSize(&fileSize);
    NS_ENSURE_SUCCESS(rv, rv);

    // write the header at the start
    if (fileSize == 0)
    {
      uint32_t writeCount;

      rv = mLogStream->Write(LOG_HEADER, LOG_HEADER_LEN, &writeCount);
      NS_ENSURE_SUCCESS(rv, rv);
      NS_ASSERTION(writeCount == LOG_HEADER_LEN, "failed to write out log header");
    }
  }

  NS_ADDREF(*aLogStream = mLogStream);
  return NS_OK;
}

NS_IMETHODIMP nsSpamSettings::Initialize(nsIMsgIncomingServer *aServer)
{
  NS_ENSURE_ARG_POINTER(aServer);
  nsresult rv;
  int32_t spamLevel;
  rv = aServer->GetIntValue("spamLevel", &spamLevel);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = SetLevel(spamLevel);
  NS_ENSURE_SUCCESS(rv, rv);

  bool moveOnSpam;
  rv = aServer->GetBoolValue("moveOnSpam", &moveOnSpam);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = SetMoveOnSpam(moveOnSpam);
  NS_ENSURE_SUCCESS(rv, rv);

  int32_t moveTargetMode;
  rv = aServer->GetIntValue("moveTargetMode", &moveTargetMode);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = SetMoveTargetMode(moveTargetMode);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCString spamActionTargetAccount;
  rv = aServer->GetCharValue("spamActionTargetAccount", spamActionTargetAccount);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = SetActionTargetAccount(spamActionTargetAccount.get());
  NS_ENSURE_SUCCESS(rv, rv);

  nsCString spamActionTargetFolder;
  rv = aServer->GetCharValue("spamActionTargetFolder", spamActionTargetFolder);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = SetActionTargetFolder(spamActionTargetFolder.get());
  NS_ENSURE_SUCCESS(rv, rv);

  bool useWhiteList;
  rv = aServer->GetBoolValue("useWhiteList", &useWhiteList);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = SetUseWhiteList(useWhiteList);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCString whiteListAbURI;
  rv = aServer->GetCharValue("whiteListAbURI", whiteListAbURI);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = SetWhiteListAbURI(whiteListAbURI.get());
  NS_ENSURE_SUCCESS(rv, rv);

  bool purgeSpam;
  rv = aServer->GetBoolValue("purgeSpam", &purgeSpam);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = SetPurge(purgeSpam);
  NS_ENSURE_SUCCESS(rv, rv);

  int32_t purgeSpamInterval;
  rv = aServer->GetIntValue("purgeSpamInterval", &purgeSpamInterval);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = SetPurgeInterval(purgeSpamInterval);
  NS_ENSURE_SUCCESS(rv, rv);

  bool useServerFilter;
  rv = aServer->GetBoolValue("useServerFilter", &useServerFilter);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = SetUseServerFilter(useServerFilter);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCString serverFilterName;
  rv = aServer->GetCharValue("serverFilterName", serverFilterName);
  if (NS_SUCCEEDED(rv))
    SetServerFilterName(serverFilterName);
  int32_t serverFilterTrustFlags = 0;
  rv = aServer->GetIntValue("serverFilterTrustFlags", &serverFilterTrustFlags);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = SetServerFilterTrustFlags(serverFilterTrustFlags);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIPrefBranch> prefBranch(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
  if (prefBranch)
    prefBranch->GetCharPref("mail.trusteddomains",
                            getter_Copies(mTrustedMailDomains));

  mWhiteListDirArray.Clear();
  if (!mWhiteListAbURI.IsEmpty())
  {
    nsCOMPtr<nsIAbManager> abManager(do_GetService(NS_ABMANAGER_CONTRACTID, &rv));
    NS_ENSURE_SUCCESS(rv, rv);

    nsTArray<nsCString> whiteListArray;
    ParseString(mWhiteListAbURI, ' ', whiteListArray);

    for (uint32_t index = 0; index < whiteListArray.Length(); index++)
    {
      nsCOMPtr<nsIAbDirectory> directory;
      rv = abManager->GetDirectory(whiteListArray[index],
                                   getter_AddRefs(directory));
      NS_ENSURE_SUCCESS(rv, rv);

      if (directory)
        mWhiteListDirArray.AppendObject(directory);
    }
  }

  // the next two preferences affect whether we try to whitelist our own
  // address or domain. Spammers send emails with spoofed from address matching
  // either the email address of the recipient, or the recipient's domain,
  // hoping to get whitelisted.
  //
  // The terms to describe this get wrapped up in chains of negatives. A full
  // definition of the boolean inhibitWhiteListingIdentityUser is "Suppress address
  // book whitelisting if the sender matches an identity's email address"

  rv = aServer->GetBoolValue("inhibitWhiteListingIdentityUser",
                             &mInhibitWhiteListingIdentityUser);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aServer->GetBoolValue("inhibitWhiteListingIdentityDomain",
                             &mInhibitWhiteListingIdentityDomain);
  NS_ENSURE_SUCCESS(rv, rv);

  // collect lists of identity users if needed
  if (mInhibitWhiteListingIdentityDomain || mInhibitWhiteListingIdentityUser)
  {
    nsCOMPtr<nsIMsgAccountManager>
      accountManager(do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIMsgAccount> account;
    rv = accountManager->FindAccountForServer(aServer, getter_AddRefs(account));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCAutoString accountKey;
    if (account) 
      account->GetKey(accountKey);

    // Loop through all accounts, adding emails from this account, as well as
    // from any accounts that defer to this account.
    mEmails.Clear();
    nsCOMPtr<nsISupportsArray> accounts;
    rv = accountManager->GetAccounts(getter_AddRefs(accounts));
    NS_ENSURE_SUCCESS(rv, rv);
    uint32_t accountCount = 0;
    if (account && accounts) // no sense scanning accounts if we've nothing to match
      accounts->Count(&accountCount);

    for (uint32_t i = 0; i < accountCount; i++)
    {
      nsCOMPtr<nsIMsgAccount> loopAccount(do_QueryElementAt(accounts, i));
      if (!loopAccount)
        continue;
      nsCAutoString loopAccountKey;
      loopAccount->GetKey(loopAccountKey);
      nsCOMPtr<nsIMsgIncomingServer> loopServer;
      loopAccount->GetIncomingServer(getter_AddRefs(loopServer));
      nsCAutoString deferredToAccountKey;
      if (loopServer)
        loopServer->GetCharValue("deferred_to_account", deferredToAccountKey);

      // Add the emails for any account that defers to this one, or for the
      // account itself.
      if (accountKey.Equals(deferredToAccountKey) || accountKey.Equals(loopAccountKey))
      {
        nsCOMPtr<nsISupportsArray> identities;
        loopAccount->GetIdentities(getter_AddRefs(identities));
        if (!identities)
          continue;
        uint32_t identityCount = 0;
        identities->Count(&identityCount);
        for (uint32_t j = 0; j < identityCount; ++j)
        {
          nsCOMPtr<nsIMsgIdentity> identity(do_QueryElementAt(identities, j));
          if (!identity)
            continue;
          nsCAutoString email;
          identity->GetEmail(email);
          if (!email.IsEmpty())
            mEmails.AppendElement(email);
        }
      }
    }
  }

  return UpdateJunkFolderState();
}

nsresult nsSpamSettings::UpdateJunkFolderState()
{
  nsresult rv;

  // if the spam folder uri changed on us, we need to unset the junk flag
  // on the old spam folder
  nsCString newJunkFolderURI;
  rv = GetSpamFolderURI(getter_Copies(newJunkFolderURI));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!mCurrentJunkFolderURI.IsEmpty() && !mCurrentJunkFolderURI.Equals(newJunkFolderURI))
  {
    nsCOMPtr<nsIMsgFolder> oldJunkFolder;
    rv = GetExistingFolder(mCurrentJunkFolderURI, getter_AddRefs(oldJunkFolder));
    if (NS_SUCCEEDED(rv) && oldJunkFolder)
    {
      // remove the nsMsgFolderFlags::Junk on the old junk folder
      // XXX TODO
      // JUNK MAIL RELATED
      // (in ClearFlag?) we need to make sure that this folder
      // is not the junk folder for another account
      // the same goes for set flag.  have fun with all that.
      oldJunkFolder->ClearFlag(nsMsgFolderFlags::Junk);
    }
  }

  mCurrentJunkFolderURI = newJunkFolderURI;

  // only try to create the junk folder if we are moving junk
  // and we have a non-empty uri
  if (mMoveOnSpam && !mCurrentJunkFolderURI.IsEmpty()) {
    // as the url listener, the spam settings will set the nsMsgFolderFlags::Junk folder flag
    // on the junk mail folder, after it is created
    rv = GetOrCreateFolder(mCurrentJunkFolderURI, this);
  }

  return rv;
}

NS_IMETHODIMP nsSpamSettings::Clone(nsISpamSettings *aSpamSettings)
{
  NS_ENSURE_ARG_POINTER(aSpamSettings);

  nsresult rv = aSpamSettings->GetUseWhiteList(&mUseWhiteList);
  NS_ENSURE_SUCCESS(rv,rv);

  (void)aSpamSettings->GetMoveOnSpam(&mMoveOnSpam);
  (void)aSpamSettings->GetPurge(&mPurge);
  (void)aSpamSettings->GetUseServerFilter(&mUseServerFilter);

  rv = aSpamSettings->GetPurgeInterval(&mPurgeInterval);
  NS_ENSURE_SUCCESS(rv,rv);

  rv = aSpamSettings->GetLevel(&mLevel);
  NS_ENSURE_SUCCESS(rv,rv);

  rv = aSpamSettings->GetMoveTargetMode(&mMoveTargetMode);
  NS_ENSURE_SUCCESS(rv,rv);

  nsCString actionTargetAccount;
  rv = aSpamSettings->GetActionTargetAccount(getter_Copies(actionTargetAccount));
  NS_ENSURE_SUCCESS(rv,rv);
  mActionTargetAccount = actionTargetAccount;

  nsCString actionTargetFolder;
  rv = aSpamSettings->GetActionTargetFolder(getter_Copies(actionTargetFolder));
  NS_ENSURE_SUCCESS(rv,rv);
  mActionTargetFolder = actionTargetFolder;

  nsCString whiteListAbURI;
  rv = aSpamSettings->GetWhiteListAbURI(getter_Copies(whiteListAbURI));
  NS_ENSURE_SUCCESS(rv,rv);
  mWhiteListAbURI = whiteListAbURI;

  aSpamSettings->GetServerFilterName(mServerFilterName);
  aSpamSettings->GetServerFilterTrustFlags(&mServerFilterTrustFlags);

  return rv;
}

NS_IMETHODIMP nsSpamSettings::GetSpamFolderURI(char **aSpamFolderURI)
{
  NS_ENSURE_ARG_POINTER(aSpamFolderURI);

  if (mMoveTargetMode == nsISpamSettings::MOVE_TARGET_MODE_FOLDER)
    return GetActionTargetFolder(aSpamFolderURI);

  // if the mode is nsISpamSettings::MOVE_TARGET_MODE_ACCOUNT
  // the spam folder URI = account uri + "/Junk"
  nsCString folderURI;
  nsresult rv = GetActionTargetAccount(getter_Copies(folderURI));
  NS_ENSURE_SUCCESS(rv,rv);

  // we might be trying to get the old spam folder uri
  // in order to clear the flag
  // if we didn't have one, bail out.
  if (folderURI.IsEmpty())
    return NS_OK;

  nsCOMPtr<nsIRDFService> rdf(do_GetService("@mozilla.org/rdf/rdf-service;1", &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIRDFResource> folderResource;
  rv = rdf->GetResource(folderURI, getter_AddRefs(folderResource));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr <nsIMsgFolder> folder = do_QueryInterface(folderResource);
  if (!folder)
    return NS_ERROR_UNEXPECTED;

  nsCOMPtr <nsIMsgIncomingServer> server;
  rv = folder->GetServer(getter_AddRefs(server));
  NS_ENSURE_SUCCESS(rv,rv);

  // see nsMsgFolder::SetPrettyName() for where the pretty name is set.

  // Check for an existing junk folder - this will do a case-insensitive 
  // search by URI - if we find a junk folder, use its URI.
  nsCOMPtr<nsIMsgFolder> junkFolder;
  folderURI.Append("/Junk");
  if (NS_SUCCEEDED(server->GetMsgFolderFromURI(nullptr, folderURI,
                                               getter_AddRefs(junkFolder))) &&
      junkFolder)
    junkFolder->GetURI(folderURI);

  // XXX todo
  // better not to make base depend in imap
  // but doing it here, like in nsMsgCopy.cpp
  // one day, we'll fix this (and nsMsgCopy.cpp) to use GetMsgFolderFromURI()
  nsCOMPtr<nsIImapIncomingServer> imapServer = do_QueryInterface(server);
  if (imapServer) {
    // Make sure an specific IMAP folder has correct personal namespace
    // see bug #197043
    nsCString folderUriWithNamespace;
    (void)imapServer->GetUriWithNamespacePrefixIfNecessary(kPersonalNamespace, folderURI,
                                                           folderUriWithNamespace);
    if (!folderUriWithNamespace.IsEmpty())
      folderURI = folderUriWithNamespace;
  }

  *aSpamFolderURI = ToNewCString(folderURI);
  if (!*aSpamFolderURI)
    return NS_ERROR_OUT_OF_MEMORY;
  else
    return rv;
}

NS_IMETHODIMP nsSpamSettings::GetServerFilterName(nsACString &aFilterName)
{
  aFilterName = mServerFilterName;
  return NS_OK;
}

NS_IMETHODIMP nsSpamSettings::SetServerFilterName(const nsACString &aFilterName)
{
  mServerFilterName = aFilterName;
  mServerFilterFile = nullptr; // clear out our stored location value
  return NS_OK;
}

NS_IMETHODIMP nsSpamSettings::GetServerFilterFile(nsIFile ** aFile)
{
  NS_ENSURE_ARG_POINTER(aFile);
  if (!mServerFilterFile)
  {
    nsresult rv;
    nsCAutoString serverFilterFileName;
    GetServerFilterName(serverFilterFileName);
    serverFilterFileName.Append(".sfd");

    nsCOMPtr<nsIProperties> dirSvc = do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    // Walk through the list of isp directories
    nsCOMPtr<nsISimpleEnumerator> ispDirectories;
    rv = dirSvc->Get(ISP_DIRECTORY_LIST, NS_GET_IID(nsISimpleEnumerator), getter_AddRefs(ispDirectories));
    NS_ENSURE_SUCCESS(rv, rv);

    bool hasMore;
    nsCOMPtr<nsIFile> file;
    while (NS_SUCCEEDED(ispDirectories->HasMoreElements(&hasMore)) && hasMore)
    {
      nsCOMPtr<nsISupports> elem;
      ispDirectories->GetNext(getter_AddRefs(elem));
      file = do_QueryInterface(elem);

      if (file)
      {
        // append our desired leaf name then test to see if the file exists. If it does, we've found
        // mServerFilterFile.
        file->AppendNative(serverFilterFileName);
        bool exists;
        if (NS_SUCCEEDED(file->Exists(&exists)) && exists)
        {
          file.swap(mServerFilterFile);
          break;
        }
      } // if file
    } // until we find the location of mServerFilterName
  } // if we haven't already stored mServerFilterFile

  NS_IF_ADDREF(*aFile = mServerFilterFile);
  return NS_OK;
}


NS_IMPL_GETSET(nsSpamSettings, ServerFilterTrustFlags, int32_t, mServerFilterTrustFlags)

#define LOG_ENTRY_START_TAG "<p>\n"
#define LOG_ENTRY_START_TAG_LEN (strlen(LOG_ENTRY_START_TAG))
#define LOG_ENTRY_END_TAG "</p>\n"
#define LOG_ENTRY_END_TAG_LEN (strlen(LOG_ENTRY_END_TAG))

NS_IMETHODIMP nsSpamSettings::LogJunkHit(nsIMsgDBHdr *aMsgHdr, bool aMoveMessage)
{
  bool loggingEnabled;
  nsresult rv = GetLoggingEnabled(&loggingEnabled);
  NS_ENSURE_SUCCESS(rv,rv);

  if (!loggingEnabled)
    return NS_OK;

  PRTime date;

  nsString authorValue;
  nsString subjectValue;
  nsString dateValue;

  (void)aMsgHdr->GetDate(&date);
  PRExplodedTime exploded;
  PR_ExplodeTime(date, PR_LocalTimeParameters, &exploded);

  if (!mDateFormatter)
  {
    mDateFormatter = do_CreateInstance(NS_DATETIMEFORMAT_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!mDateFormatter)
    {
      return NS_ERROR_FAILURE;
    }
  }
  mDateFormatter->FormatPRExplodedTime(nullptr, kDateFormatShort,
                                      kTimeFormatSeconds, &exploded,
                                      dateValue);

  (void)aMsgHdr->GetMime2DecodedAuthor(authorValue);
  (void)aMsgHdr->GetMime2DecodedSubject(subjectValue);

  nsCString buffer;
  // this is big enough to hold a log entry.
  // do this so we avoid growing and copying as we append to the log.
#ifdef MOZILLA_INTERNAL_API
  buffer.SetCapacity(512);
#endif

  nsCOMPtr<nsIStringBundleService> bundleService =
    mozilla::services::GetStringBundleService();
  NS_ENSURE_TRUE(bundleService, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsIStringBundle> bundle;
  rv = bundleService->CreateBundle("chrome://messenger/locale/filter.properties",
    getter_AddRefs(bundle));
  NS_ENSURE_SUCCESS(rv, rv);

  const PRUnichar *junkLogDetectFormatStrings[3] = { authorValue.get(), subjectValue.get(), dateValue.get() };
  nsString junkLogDetectStr;
  rv = bundle->FormatStringFromName(
    NS_LITERAL_STRING("junkLogDetectStr").get(),
    junkLogDetectFormatStrings, 3,
    getter_Copies(junkLogDetectStr));
  NS_ENSURE_SUCCESS(rv, rv);

  buffer += NS_ConvertUTF16toUTF8(junkLogDetectStr);
  buffer +=  "\n";

  if (aMoveMessage) {
    nsCString msgId;
    aMsgHdr->GetMessageId(getter_Copies(msgId));

    nsCString junkFolderURI;
    rv = GetSpamFolderURI(getter_Copies(junkFolderURI));
    NS_ENSURE_SUCCESS(rv, rv);

    NS_ConvertASCIItoUTF16 msgIdValue(msgId);
    NS_ConvertASCIItoUTF16 junkFolderURIValue(junkFolderURI);

    const PRUnichar *logMoveFormatStrings[2] = { msgIdValue.get(), junkFolderURIValue.get() };
    nsString logMoveStr;
    rv = bundle->FormatStringFromName(
      NS_LITERAL_STRING("logMoveStr").get(),
      logMoveFormatStrings, 2,
      getter_Copies(logMoveStr));
    NS_ENSURE_SUCCESS(rv, rv);

    buffer += NS_ConvertUTF16toUTF8(logMoveStr);
    buffer += "\n";
  }

  return LogJunkString(buffer.get());
}

NS_IMETHODIMP nsSpamSettings::LogJunkString(const char *string)
{
  bool loggingEnabled;
  nsresult rv = GetLoggingEnabled(&loggingEnabled);
  NS_ENSURE_SUCCESS(rv,rv);

  if (!loggingEnabled)
    return NS_OK;

  nsCOMPtr <nsIOutputStream> logStream;
  rv = GetLogStream(getter_AddRefs(logStream));
  NS_ENSURE_SUCCESS(rv,rv);

  uint32_t writeCount;

  rv = logStream->Write(LOG_ENTRY_START_TAG, LOG_ENTRY_START_TAG_LEN, &writeCount);
  NS_ENSURE_SUCCESS(rv,rv);
  NS_ASSERTION(writeCount == LOG_ENTRY_START_TAG_LEN, "failed to write out start log tag");

  // html escape the log for security reasons.
  // we don't want some to send us a message with a subject with
  // html tags, especially <script>
  char *escapedBuffer = MsgEscapeHTML(string);
  if (!escapedBuffer)
    return NS_ERROR_OUT_OF_MEMORY;

  uint32_t escapedBufferLen = strlen(escapedBuffer);
  rv = logStream->Write(escapedBuffer, escapedBufferLen, &writeCount);
  PR_Free(escapedBuffer);
  NS_ENSURE_SUCCESS(rv,rv);
  NS_ASSERTION(writeCount == escapedBufferLen, "failed to write out log hit");

  rv = logStream->Write(LOG_ENTRY_END_TAG, LOG_ENTRY_END_TAG_LEN, &writeCount);
  NS_ENSURE_SUCCESS(rv,rv);
  NS_ASSERTION(writeCount == LOG_ENTRY_END_TAG_LEN, "failed to write out end log tag");
  return NS_OK;
}

NS_IMETHODIMP nsSpamSettings::OnStartRunningUrl(nsIURI* aURL)
{
  // do nothing
  // all the action happens in OnStopRunningUrl()
  return NS_OK;
}

NS_IMETHODIMP nsSpamSettings::OnStopRunningUrl(nsIURI* aURL, nsresult exitCode)
{
  nsCString junkFolderURI;
  nsresult rv = GetSpamFolderURI(getter_Copies(junkFolderURI));
  NS_ENSURE_SUCCESS(rv,rv);

  if (junkFolderURI.IsEmpty())
    return NS_ERROR_UNEXPECTED;

  // when we get here, the folder should exist.
  nsCOMPtr <nsIMsgFolder> junkFolder;
  rv = GetExistingFolder(junkFolderURI, getter_AddRefs(junkFolder));
  NS_ENSURE_SUCCESS(rv,rv);
  if (!junkFolder)
    return NS_ERROR_UNEXPECTED;

  rv = junkFolder->SetFlag(nsMsgFolderFlags::Junk);
  NS_ENSURE_SUCCESS(rv,rv);
  return rv;
}

NS_IMETHODIMP nsSpamSettings::CheckWhiteList(nsIMsgDBHdr *aMsgHdr, bool *aResult)
{
  NS_ENSURE_ARG_POINTER(aMsgHdr);
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = false;  // default in case of error or no whitelisting

  if (!mUseWhiteList || (!mWhiteListDirArray.Count() &&
                          mTrustedMailDomains.IsEmpty()))
    return NS_OK;

  // do per-message processing

  nsCString author;
  aMsgHdr->GetAuthor(getter_Copies(author));
  nsresult rv;
  nsCOMPtr<nsIMsgHeaderParser> headerParser =
    do_GetService(NS_MAILNEWS_MIME_HEADER_PARSER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString authorEmailAddress;
  rv = headerParser->ExtractHeaderAddressMailboxes(author, authorEmailAddress);
  NS_ENSURE_SUCCESS(rv, rv);

  if (authorEmailAddress.IsEmpty())
    return NS_OK;

  // should we skip whitelisting for the identity email?
  if (mInhibitWhiteListingIdentityUser)
  {
    for (uint32_t i = 0; i < mEmails.Length(); ++i)
    {
      if (mEmails[i].Equals(authorEmailAddress, nsCaseInsensitiveCStringComparator()))
        return NS_OK;
    }
  }

  if (!mTrustedMailDomains.IsEmpty() || mInhibitWhiteListingIdentityDomain)
  {
    nsCAutoString domain;
    int32_t atPos = authorEmailAddress.FindChar('@');
    if (atPos >= 0)
      domain = Substring(authorEmailAddress, atPos + 1);
    if (!domain.IsEmpty())
    {
      if (!mTrustedMailDomains.IsEmpty() &&
          MsgHostDomainIsTrusted(domain, mTrustedMailDomains))
      {
        *aResult = true;
        return NS_OK;
      }

      if (mInhibitWhiteListingIdentityDomain)
      {
        for (uint32_t i = 0; i < mEmails.Length(); ++i)
        {
          nsCAutoString identityDomain;
          int32_t atPos = mEmails[i].FindChar('@');
          if (atPos >= 0)
          {
            identityDomain = Substring(mEmails[i], atPos + 1);
            if (identityDomain.Equals(domain, nsCaseInsensitiveCStringComparator()))
              return NS_OK; // don't whitelist
          }
        }
      }
    }
  }

  if (mWhiteListDirArray.Count())
  {
    nsCOMPtr<nsIAbCard> cardForAddress;
    for (int32_t index = 0;
         index < mWhiteListDirArray.Count() && !cardForAddress;
         index++)
    {
      mWhiteListDirArray[index]->CardForEmailAddress(authorEmailAddress,
                                                     getter_AddRefs(cardForAddress));
    }
    if (cardForAddress)
    {
      *aResult = true;
      return NS_OK;
    }
  }
  return NS_OK; // default return is false
}
