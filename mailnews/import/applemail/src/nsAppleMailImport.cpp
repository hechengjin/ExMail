/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZ_LOGGING
// sorry, this has to be before the pre-compiled header
#define FORCE_PR_LOG /* Allow logging in the release build */
#endif

#include "nsStringGlue.h"
#include "nsCOMPtr.h"
#include "nsISupportsPrimitives.h"
#include "nsIImportService.h"
#include "nsIImportMailboxDescriptor.h"
#include "nsIImportGeneric.h"
#include "nsIFile.h"
#include "nsIStringBundle.h"
#include "nsIMsgFolder.h"
#include "nsIMsgHdr.h"
#include "nsIMsgPluggableStore.h"
#include "nsNetUtil.h"
#include "nsMsgUtils.h"
#include "mozilla/Services.h"

#include "nsEmlxHelperUtils.h"
#include "nsAppleMailImport.h"

PRLogModuleInfo *APPLEMAILLOGMODULE = nullptr;

// some hard-coded strings
#define DEFAULT_MAIL_FOLDER "~/Library/Mail/"
#define POP_MBOX_SUFFIX ".mbox"
#define IMAP_MBOX_SUFFIX ".imapmbox"

// stringbundle URI
#define APPLEMAIL_MSGS_URL "chrome://messenger/locale/appleMailImportMsgs.properties"

// stringbundle IDs
#define APPLEMAILIMPORT_NAME                                   2000
#define APPLEMAILIMPORT_DESCRIPTION                            2001
#define APPLEMAILIMPORT_MAILBOX_SUCCESS                        2002
#define APPLEMAILIMPORT_MAILBOX_BADPARAM                       2003
#define APPLEMAILIMPORT_MAILBOX_CONVERTERROR                   2004

// magic constants
#define kAccountMailboxID 1234

nsAppleMailImportModule::nsAppleMailImportModule()
{
  // Init logging module.
  if (!APPLEMAILLOGMODULE)
    APPLEMAILLOGMODULE = PR_NewLogModule("APPLEMAILIMPORTLOG");

  IMPORT_LOG0("nsAppleMailImportModule Created");

  nsCOMPtr<nsIStringBundleService> bundleService =
    mozilla::services::GetStringBundleService();
  if (bundleService)
    bundleService->CreateBundle(APPLEMAIL_MSGS_URL, getter_AddRefs(mBundle));
}


nsAppleMailImportModule::~nsAppleMailImportModule()
{
  IMPORT_LOG0("nsAppleMailImportModule Deleted");
}


NS_IMPL_THREADSAFE_ISUPPORTS1(nsAppleMailImportModule, nsIImportModule)


NS_IMETHODIMP nsAppleMailImportModule::GetName(PRUnichar **aName)
{
  return mBundle ? 
    mBundle->GetStringFromID(APPLEMAILIMPORT_NAME, aName) : NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsAppleMailImportModule::GetDescription(PRUnichar **aName)
{
  return mBundle ? 
    mBundle->GetStringFromID(APPLEMAILIMPORT_DESCRIPTION, aName) : NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsAppleMailImportModule::GetSupports(char **aSupports)
{
  NS_ENSURE_ARG_POINTER(aSupports);
  *aSupports = strdup(NS_IMPORT_MAIL_STR);
  return NS_OK;
}

NS_IMETHODIMP nsAppleMailImportModule::GetSupportsUpgrade(bool *aUpgrade)
{
  NS_ENSURE_ARG_POINTER(aUpgrade);
  *aUpgrade = false;
  return NS_OK;
}

NS_IMETHODIMP nsAppleMailImportModule::GetImportInterface(const char *aImportType, nsISupports **aInterface)
{
  NS_ENSURE_ARG_POINTER(aImportType);
  NS_ENSURE_ARG_POINTER(aInterface);
  *aInterface = nullptr;
  nsresult rv = NS_ERROR_NOT_AVAILABLE;

  if (!strcmp(aImportType, "mail")) {
    nsCOMPtr<nsIImportMail> mail(do_CreateInstance(NS_APPLEMAILIMPL_CONTRACTID, &rv));
    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsIImportService> impSvc(do_GetService(NS_IMPORTSERVICE_CONTRACTID, &rv));
      if (NS_SUCCEEDED(rv)) {
        nsCOMPtr<nsIImportGeneric> generic;
        rv = impSvc->CreateNewGenericMail(getter_AddRefs(generic));
        if (NS_SUCCEEDED(rv)) {
          nsAutoString name;
          rv = mBundle->GetStringFromID(APPLEMAILIMPORT_NAME, getter_Copies(name));
          NS_ENSURE_SUCCESS(rv, rv);

          nsCOMPtr<nsISupportsString> nameString(do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID, &rv));
          NS_ENSURE_SUCCESS(rv, rv);
          nameString->SetData(name);

          generic->SetData("name", nameString);
          generic->SetData("mailInterface", mail);

          rv = CallQueryInterface(generic, aInterface);
        }
      }
    }
  }

  return rv;
}

#pragma mark -

nsAppleMailImportMail::nsAppleMailImportMail() : mProgress(0), mCurDepth(0)
{
  IMPORT_LOG0("nsAppleMailImportMail created");
}

nsresult nsAppleMailImportMail::Initialize()
{
  nsCOMPtr<nsIStringBundleService> bundleService =
    mozilla::services::GetStringBundleService();
  NS_ENSURE_TRUE(bundleService, NS_ERROR_UNEXPECTED);

  return bundleService->CreateBundle(APPLEMAIL_MSGS_URL, getter_AddRefs(mBundle));
}

nsAppleMailImportMail::~nsAppleMailImportMail()
{
  IMPORT_LOG0("nsAppleMailImportMail destroyed");
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsAppleMailImportMail, nsIImportMail)

NS_IMETHODIMP nsAppleMailImportMail::GetDefaultLocation(nsIFile **aLocation, bool *aFound, bool *aUserVerify)
{
  NS_ENSURE_ARG_POINTER(aFound);
  NS_ENSURE_ARG_POINTER(aLocation);
  NS_ENSURE_ARG_POINTER(aUserVerify);

  *aLocation = nullptr;
  *aFound = false;
  *aUserVerify = true;

  // try to find current user's top-level Mail folder
  nsCOMPtr<nsIFile> mailFolder(do_CreateInstance(NS_LOCAL_FILE_CONTRACTID));
  if (mailFolder) {
    nsresult rv = mailFolder->InitWithNativePath(NS_LITERAL_CSTRING(DEFAULT_MAIL_FOLDER));
    if (NS_SUCCEEDED(rv)) {
      *aFound = true;
      *aUserVerify = false;
      CallQueryInterface(mailFolder, aLocation);
    }
  }

  return NS_OK;
}

// this is the method that initiates all searching for mailboxes.
// it will assume that it has a directory like ~/Library/Mail/
NS_IMETHODIMP nsAppleMailImportMail::FindMailboxes(nsIFile *aMailboxFile, nsISupportsArray **aResult)
{
  NS_ENSURE_ARG_POINTER(aMailboxFile);
  NS_ENSURE_ARG_POINTER(aResult);

  IMPORT_LOG0("FindMailboxes for Apple mail invoked");

  bool exists = false;
  nsresult rv = aMailboxFile->Exists(&exists);
  if (NS_FAILED(rv) || !exists)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIImportService> importService(do_GetService(NS_IMPORTSERVICE_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISupportsArray> resultsArray;
  NS_NewISupportsArray(getter_AddRefs(resultsArray));
  if (!resultsArray)
    return NS_ERROR_OUT_OF_MEMORY;

  mCurDepth = 1;

  // 1. look for accounts with mailboxes
  FindAccountMailDirs(aMailboxFile, resultsArray, importService);
  mCurDepth--;

  if (NS_SUCCEEDED(rv)) {
    // 2. look for "global" mailboxes, that don't belong to any specific account. they are inside the
    //    root's Mailboxes/ folder
    nsCOMPtr<nsIFile> mailboxesDir(do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv));
    if (NS_SUCCEEDED(rv)) {  
        mailboxesDir->InitWithFile(aMailboxFile);
        rv = mailboxesDir->Append(NS_LITERAL_STRING("Mailboxes"));
        if (NS_SUCCEEDED(rv)) {
          IMPORT_LOG0("Looking for global Apple mailboxes");

          mCurDepth++;
          rv = FindMboxDirs(mailboxesDir, resultsArray, importService);
          mCurDepth--;
        }
    }
  }

  if (NS_SUCCEEDED(rv) && resultsArray)
    resultsArray.swap(*aResult);

  return rv;
}

// operates on the Mail/ directory root, trying to find accounts (which are folders named something like "POP-hwaara@gmail.com")
// and add their .mbox dirs
void nsAppleMailImportMail::FindAccountMailDirs(nsIFile *aRoot, nsISupportsArray *aMailboxDescs, nsIImportService *aImportService)
{
  nsCOMPtr<nsISimpleEnumerator> directoryEnumerator;
  nsresult rv = aRoot->GetDirectoryEntries(getter_AddRefs(directoryEnumerator));
  if (NS_FAILED(rv))
    return;

  bool hasMore = false;
  while (NS_SUCCEEDED(directoryEnumerator->HasMoreElements(&hasMore)) && hasMore) {

    // get the next file entry
    nsCOMPtr<nsIFile> currentEntry;
    {
      nsCOMPtr<nsISupports> rawSupports;
      directoryEnumerator->GetNext(getter_AddRefs(rawSupports));
      if (!rawSupports)
        continue;
      currentEntry = do_QueryInterface(rawSupports);
      if (!currentEntry)
        continue;
    }

    // make sure it's a directory
    bool isDirectory = false;
    currentEntry->IsDirectory(&isDirectory);

    if (isDirectory) {
      // now let's see if it's an account folder. if so, we want to traverse it for .mbox children
      nsAutoString folderName;
      currentEntry->GetLeafName(folderName);
      bool isAccountFolder = false;

      if (StringBeginsWith(folderName, NS_LITERAL_STRING("POP-"))) {
        // cut off "POP-" prefix so we get a nice folder name
        folderName.Cut(0, 4);
        isAccountFolder = true;
      }
      else if (StringBeginsWith(folderName, NS_LITERAL_STRING("IMAP-"))) {
        // cut off "IMAP-" prefix so we get a nice folder name
        folderName.Cut(0, 5);
        isAccountFolder = true;
      }

      if (isAccountFolder) {
        IMPORT_LOG1("Found account: %s\n", NS_ConvertUTF16toUTF8(folderName).get());

        // create a mailbox for this account, so we get a parent for "Inbox", "Sent Messages", etc.
        nsCOMPtr<nsIImportMailboxDescriptor> desc;
        nsresult rv = aImportService->CreateNewMailboxDescriptor(getter_AddRefs(desc));
        desc->SetSize(1);
        desc->SetDepth(mCurDepth);
        desc->SetDisplayName(folderName.get());
        desc->SetIdentifier(kAccountMailboxID);

        nsCOMPtr<nsIFile> mailboxDescFile;
        rv = desc->GetFile(getter_AddRefs(mailboxDescFile));
        if (!mailboxDescFile)
          continue;

        mailboxDescFile->InitWithFile(currentEntry);

        // add this mailbox descriptor to the list
        aMailboxDescs->AppendElement(desc);

        // now add all the children mailboxes
        mCurDepth++;
        FindMboxDirs(currentEntry, aMailboxDescs, aImportService);
        mCurDepth--;  
      }
    }
  }
}

// adds the specified file as a mailboxdescriptor to the array
nsresult nsAppleMailImportMail::AddMboxDir(nsIFile *aFolder, nsISupportsArray *aMailboxDescs, nsIImportService *aImportService)
{
  nsAutoString folderName;
  aFolder->GetLeafName(folderName);

  // cut off the suffix, if any, or prefix if this is an account folder.
  if (StringEndsWith(folderName, NS_LITERAL_STRING(POP_MBOX_SUFFIX)))
    folderName.SetLength(folderName.Length()-5);
  else if (StringEndsWith(folderName, NS_LITERAL_STRING(IMAP_MBOX_SUFFIX)))
    folderName.SetLength(folderName.Length()-9);
  else if (StringBeginsWith(folderName, NS_LITERAL_STRING("POP-")))
    folderName.Cut(4, folderName.Length());
  else if (StringBeginsWith(folderName, NS_LITERAL_STRING("IMAP-")))
    folderName.Cut(5, folderName.Length());

  nsCOMPtr<nsIImportMailboxDescriptor> desc;
  nsresult rv = aImportService->CreateNewMailboxDescriptor(getter_AddRefs(desc));
  if (NS_SUCCEEDED(rv)) {
    // find out number of messages in this .mbox
    uint32_t numMessages = 0;
    {
      // move to the .mbox's Messages folder
      nsCOMPtr<nsIFile> messagesFolder;
      aFolder->Clone(getter_AddRefs(messagesFolder));
      nsresult rv = messagesFolder->Append(NS_LITERAL_STRING("Messages"));
      NS_ENSURE_SUCCESS(rv, rv);

      // count the number of messages in this folder. it sucks that we have to iterate through the folder
      // but XPCOM doesn't give us any way to just get the file count, unfortunately. :-(
      nsCOMPtr<nsISimpleEnumerator> dirEnumerator;
      messagesFolder->GetDirectoryEntries(getter_AddRefs(dirEnumerator));
      if (dirEnumerator) {
        bool hasMore = false;
        while (NS_SUCCEEDED(dirEnumerator->HasMoreElements(&hasMore)) && hasMore) {
          nsCOMPtr<nsISupports> rawSupports;
          dirEnumerator->GetNext(getter_AddRefs(rawSupports));
          if (!rawSupports)
            continue;

          nsCOMPtr<nsIFile> file(do_QueryInterface(rawSupports));
          if (file) {
            bool isFile = false;
            file->IsFile(&isFile);
            if (isFile)
              numMessages++;
          }
        }
      }
    }

    desc->SetSize(numMessages);
    desc->SetDisplayName(folderName.get());
    desc->SetDepth(mCurDepth);

    IMPORT_LOG3("Will import %s with approx %d messages, depth is %d", NS_ConvertUTF16toUTF8(folderName).get(), numMessages, mCurDepth);

    // XXX: this is silly. there's no setter for the mailbox descriptor's file, so we need to get it, and then modify it.
    nsCOMPtr<nsIFile> mailboxDescFile;
    rv = desc->GetFile(getter_AddRefs(mailboxDescFile));
    NS_ENSURE_SUCCESS(rv, rv);

    if (mailboxDescFile)
      mailboxDescFile->InitWithFile(aFolder);

    // add this mailbox descriptor to the list
    aMailboxDescs->AppendElement(desc); 
  }

  return NS_OK;
}

// Starts looking for .mbox dirs in the specified dir. The .mbox dirs contain messages and can be considered leafs in a tree of
// nested mailboxes (subfolders).
//
// If a mailbox has sub-mailboxes, they are contained in a sibling folder with the same name without the ".mbox" part.
// example:
//   MyParentMailbox.mbox/
//   MyParentMailbox/
//     MyChildMailbox.mbox/
//     MyOtherChildMailbox.mbox/
//
nsresult nsAppleMailImportMail::FindMboxDirs(nsIFile *aFolder, nsISupportsArray *aMailboxDescs, nsIImportService *aImportService)
{
  NS_ENSURE_ARG_POINTER(aFolder);
  NS_ENSURE_ARG_POINTER(aMailboxDescs);
  NS_ENSURE_ARG_POINTER(aImportService);

  // make sure this is a directory.
  bool isDir = false;
  if (NS_FAILED(aFolder->IsDirectory(&isDir)) || !isDir)
    return NS_ERROR_FAILURE;

  // iterate through the folder contents
  nsCOMPtr<nsISimpleEnumerator> directoryEnumerator;
  nsresult rv = aFolder->GetDirectoryEntries(getter_AddRefs(directoryEnumerator));
  if (NS_FAILED(rv) || !directoryEnumerator)
    return rv;

  bool hasMore = false;
  while (NS_SUCCEEDED(directoryEnumerator->HasMoreElements(&hasMore)) && hasMore) {

    // get the next file entry
    nsCOMPtr<nsIFile> currentEntry;
    {
      nsCOMPtr<nsISupports> rawSupports;
      directoryEnumerator->GetNext(getter_AddRefs(rawSupports));
      if (!rawSupports)
        continue;
      currentEntry = do_QueryInterface(rawSupports);
      if (!currentEntry)
        continue;
    }

    // we only care about directories...
    if (NS_FAILED(currentEntry->IsDirectory(&isDir)) || !isDir)
      continue;

    // now find out if this is a .mbox dir
    nsAutoString currentFolderName;
    if (NS_SUCCEEDED(currentEntry->GetLeafName(currentFolderName)) && 
        (StringEndsWith(currentFolderName, NS_LITERAL_STRING(POP_MBOX_SUFFIX)) || 
         StringEndsWith(currentFolderName, NS_LITERAL_STRING(IMAP_MBOX_SUFFIX)))) {
      IMPORT_LOG1("Adding .mbox dir: %s", NS_ConvertUTF16toUTF8(currentFolderName).get());

      // add this .mbox
      rv = AddMboxDir(currentEntry, aMailboxDescs, aImportService);
      if (NS_FAILED(rv)) {
        IMPORT_LOG1("Couldn't add .mbox for import: %s ... continuing anyway", NS_ConvertUTF16toUTF8(currentFolderName).get());
        continue;
      }

      // see if this .mbox dir has any sub-mailboxes
      nsAutoString siblingMailboxDirPath;
      currentEntry->GetPath(siblingMailboxDirPath);

      // cut off suffix
      if (StringEndsWith(siblingMailboxDirPath, NS_LITERAL_STRING(IMAP_MBOX_SUFFIX)))
        siblingMailboxDirPath.SetLength(siblingMailboxDirPath.Length()-9);
      else if (StringEndsWith(siblingMailboxDirPath, NS_LITERAL_STRING(POP_MBOX_SUFFIX)))
        siblingMailboxDirPath.SetLength(siblingMailboxDirPath.Length()-5);

      IMPORT_LOG1("trying to locate a '%s'", NS_ConvertUTF16toUTF8(siblingMailboxDirPath).get());
      nsCOMPtr<nsIFile> siblingMailboxDir(do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv));
      if (NS_FAILED(rv))
        continue;

      rv = siblingMailboxDir->InitWithPath(siblingMailboxDirPath);
      bool reallyExists = false;
      siblingMailboxDir->Exists(&reallyExists);

      if (NS_SUCCEEDED(rv) && reallyExists) {
        IMPORT_LOG1("Found what looks like an .mbox container: %s", NS_ConvertUTF16toUTF8(currentFolderName).get());

        // traverse this folder for other .mboxes
        mCurDepth++;
        FindMboxDirs(siblingMailboxDir, aMailboxDescs, aImportService);
        mCurDepth--;
      }
    } 
  } 

  return NS_OK;
}

NS_IMETHODIMP
nsAppleMailImportMail::ImportMailbox(nsIImportMailboxDescriptor *aMailbox,
                                     nsIMsgFolder *aDstFolder,
                                     PRUnichar **aErrorLog,
                                     PRUnichar **aSuccessLog, bool *aFatalError)
{
  nsAutoString errorLog, successLog;

  // reset progress
  mProgress = 0;

  nsAutoString mailboxName;
  aMailbox->GetDisplayName(getter_Copies(mailboxName));

  nsCOMPtr<nsIFile> mboxFolder;
  nsresult rv = aMailbox->GetFile(getter_AddRefs(mboxFolder));
  if (NS_FAILED(rv) || !mboxFolder) {
    ReportStatus(APPLEMAILIMPORT_MAILBOX_CONVERTERROR, mailboxName, errorLog);
    SetLogs(successLog, errorLog, aSuccessLog, aErrorLog);
    return NS_ERROR_FAILURE;
  }

  // if we're an account mailbox, nothing do. if we're a real mbox
  // then we've got some messages to import!
  uint32_t mailboxIdentifier;
  aMailbox->GetIdentifier(&mailboxIdentifier);

  if (mailboxIdentifier != kAccountMailboxID) {
    // move to the .mbox's Messages folder
    nsCOMPtr<nsIFile> messagesFolder;
    mboxFolder->Clone(getter_AddRefs(messagesFolder));
    rv = messagesFolder->Append(NS_LITERAL_STRING("Messages"));
    if (NS_FAILED(rv)) {
      // even if there are no messages, it might still be a valid mailbox, or even
      // a parent for other mailboxes.
      //
      // just indicate that we're done, using the same number that we used to estimate
      // number of messages earlier.
      uint32_t finalSize;
      aMailbox->GetSize(&finalSize);
      mProgress = finalSize;

      // report that we successfully imported this mailbox
      ReportStatus(APPLEMAILIMPORT_MAILBOX_SUCCESS, mailboxName, successLog);
      SetLogs(successLog, errorLog, aSuccessLog, aErrorLog);
      return NS_OK;
    }

    // let's import the messages!
    nsCOMPtr<nsISimpleEnumerator> directoryEnumerator;
    rv = messagesFolder->GetDirectoryEntries(getter_AddRefs(directoryEnumerator));
    if (NS_FAILED(rv)) {
      ReportStatus(APPLEMAILIMPORT_MAILBOX_CONVERTERROR, mailboxName, errorLog);
      SetLogs(successLog, errorLog, aSuccessLog, aErrorLog);
      return NS_ERROR_FAILURE;
    }

    // prepare an outstream to the destination file
    nsCOMPtr<nsIMsgPluggableStore> msgStore;
    rv = aDstFolder->GetMsgStore(getter_AddRefs(msgStore));
    if (!msgStore || NS_FAILED(rv)) {
      ReportStatus(APPLEMAILIMPORT_MAILBOX_CONVERTERROR, mailboxName, errorLog);
      SetLogs(successLog, errorLog, aSuccessLog, aErrorLog);
      return NS_ERROR_FAILURE;
    }

    bool hasMore = false;
    nsCOMPtr<nsIOutputStream> outStream;

    while (NS_SUCCEEDED(directoryEnumerator->HasMoreElements(&hasMore)) && hasMore) {
      // get the next file entry
      nsCOMPtr<nsIFile> currentEntry;
      {
        nsCOMPtr<nsISupports> rawSupports;
        directoryEnumerator->GetNext(getter_AddRefs(rawSupports));
        if (!rawSupports)
          continue;
        currentEntry = do_QueryInterface(rawSupports);
        if (!currentEntry)
          continue;
      }

      // make sure it's an .emlx file
      bool isFile = false;
      currentEntry->IsFile(&isFile);
      if (!isFile)
        continue;

      nsAutoString leafName;
      currentEntry->GetLeafName(leafName);
      if (!StringEndsWith(leafName, NS_LITERAL_STRING(".emlx")))
        continue;

      nsCOMPtr<nsIMsgDBHdr> msgHdr;
      bool reusable;
      rv = msgStore->GetNewMsgOutputStream(aDstFolder, getter_AddRefs(msgHdr),
                                           &reusable,
                                           getter_AddRefs(outStream));
      if (NS_FAILED(rv))
        break;

      // add the data to the mbox stream
      if (NS_SUCCEEDED(nsEmlxHelperUtils::AddEmlxMessageToStream(currentEntry, outStream))) {
        mProgress++;
        msgStore->FinishNewMessage(outStream, msgHdr);
      }
      else {
        msgStore->DiscardNewMessage(outStream, msgHdr);
        break;
      }
      if (!reusable)
        outStream->Close();
    }
    if (outStream)
      outStream->Close();
  }
  // just indicate that we're done, using the same number that we used to estimate
  // number of messages earlier.
  uint32_t finalSize;
  aMailbox->GetSize(&finalSize);
  mProgress = finalSize;

  // report that we successfully imported this mailbox
  ReportStatus(APPLEMAILIMPORT_MAILBOX_SUCCESS, mailboxName, successLog);
  SetLogs(successLog, errorLog, aSuccessLog, aErrorLog);

  return NS_OK;
}

void nsAppleMailImportMail::ReportStatus(int32_t aErrorNum, nsString &aName, nsAString &aStream)
{
  // get (and format, if needed) the error string from the bundle  
  nsAutoString outString;
  const PRUnichar *fmt = { aName.get() };
  nsresult rv = mBundle->FormatStringFromID(aErrorNum, &fmt, 1, getter_Copies(outString));
  // write it out the stream
  if (NS_SUCCEEDED(rv)) {
    aStream.Append(outString);
    aStream.Append(PRUnichar('\n'));
  }
}

void nsAppleMailImportMail::SetLogs(const nsAString &aSuccess, const nsAString &aError, PRUnichar **aOutSuccess, PRUnichar **aOutError)
{
  if (aOutError && !*aOutError)
    *aOutError = ToNewUnicode(aError);
  if (aOutSuccess && !*aOutSuccess)
    *aOutSuccess = ToNewUnicode(aSuccess);
}

NS_IMETHODIMP nsAppleMailImportMail::GetImportProgress(uint32_t *aDoneSoFar)
{
  NS_ENSURE_ARG_POINTER(aDoneSoFar);
  *aDoneSoFar = mProgress;
  return NS_OK;
}

NS_IMETHODIMP nsAppleMailImportMail::TranslateFolderName(const nsAString &aFolderName, nsAString &aResult)
{
  aResult = aFolderName;
  return NS_OK;
}
