/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAbLDAPChangeLogData.h"
#include "nsAbLDAPChangeLogQuery.h"
#include "nsILDAPMessage.h"
#include "nsIAbCard.h"
#include "nsIAddrBookSession.h"
#include "nsAbBaseCID.h"
#include "nsAbUtils.h"
#include "nsAbMDBCard.h"
#include "nsAbLDAPCard.h"
#include "nsIAuthPrompt.h"
#include "nsIStringBundle.h"
#include "nsIWindowWatcher.h"
#include "nsUnicharUtils.h"
#include "plstr.h"
#include "nsILDAPErrors.h"
#include "prmem.h"
#include "mozilla/Services.h"

// Defined here since to be used
// only locally to this file.
enum UpdateOp {
 NO_OP,
 ENTRY_ADD,
 ENTRY_DELETE,
 ENTRY_MODIFY
};

nsAbLDAPProcessChangeLogData::nsAbLDAPProcessChangeLogData()
: mUseChangeLog(false),
  mChangeLogEntriesCount(0),
  mEntriesAddedQueryCount(0)
{
   mRootDSEEntry.firstChangeNumber = 0;
   mRootDSEEntry.lastChangeNumber = 0;
}

nsAbLDAPProcessChangeLogData::~nsAbLDAPProcessChangeLogData()
{

}

NS_IMETHODIMP nsAbLDAPProcessChangeLogData::Init(nsIAbLDAPReplicationQuery * query, nsIWebProgressListener *progressListener)
{
   NS_ENSURE_ARG_POINTER(query);

   // Here we are assuming that the caller will pass a nsAbLDAPChangeLogQuery object,
   // an implementation derived from the implementation of nsIAbLDAPReplicationQuery.
   nsresult rv = NS_OK;
   mChangeLogQuery = do_QueryInterface(query, &rv);
   if(NS_FAILED(rv))
       return rv;
   
   // Call the parent's Init now.
   return nsAbLDAPProcessReplicationData::Init(query, progressListener);
}

nsresult nsAbLDAPProcessChangeLogData::OnLDAPBind(nsILDAPMessage *aMessage)
{
    NS_ENSURE_ARG_POINTER(aMessage);
	if(!mInitialized) 
        return NS_ERROR_NOT_INITIALIZED;

    int32_t errCode;

    nsresult rv = aMessage->GetErrorCode(&errCode);
    if(NS_FAILED(rv)) {
        Done(false);
        return rv;
    }

    if(errCode != nsILDAPErrors::SUCCESS) {
        Done(false);
        return NS_ERROR_FAILURE;
    }

    switch(mState) {
    case kAnonymousBinding :
        rv = GetAuthData();
        if(NS_SUCCEEDED(rv)) 
            rv = mChangeLogQuery->QueryAuthDN(mAuthUserID);
        if(NS_SUCCEEDED(rv)) 
            mState = kSearchingAuthDN;
        break;
    case kAuthenticatedBinding :
        rv = mChangeLogQuery->QueryRootDSE();
        if(NS_SUCCEEDED(rv)) 
            mState = kSearchingRootDSE;
        break;
    } //end of switch

    if(NS_FAILED(rv))
        Abort();

    return rv;
}

nsresult nsAbLDAPProcessChangeLogData::OnLDAPSearchEntry(nsILDAPMessage *aMessage)
{
    NS_ENSURE_ARG_POINTER(aMessage);
    if(!mInitialized) 
        return NS_ERROR_NOT_INITIALIZED;

    nsresult rv = NS_OK;       

    switch(mState)
    {
    case kSearchingAuthDN :
        {
            nsCAutoString authDN;
            rv = aMessage->GetDn(authDN);
            if(NS_SUCCEEDED(rv) && !authDN.IsEmpty())
                mAuthDN = authDN.get();
        }
        break;
    case kSearchingRootDSE:
        rv = ParseRootDSEEntry(aMessage);
        break;
    case kFindingChanges:
        rv = ParseChangeLogEntries(aMessage);
        break;
    // Fall through since we only add (for updates we delete and add)
    case kReplicatingChanges:
    case kReplicatingAll :
        return nsAbLDAPProcessReplicationData::OnLDAPSearchEntry(aMessage);
    }

    if(NS_FAILED(rv))
        Abort();

    return rv;
}

nsresult nsAbLDAPProcessChangeLogData::OnLDAPSearchResult(nsILDAPMessage *aMessage)
{
    NS_ENSURE_ARG_POINTER(aMessage);
    if (!mInitialized)
        return NS_ERROR_NOT_INITIALIZED;

    int32_t errorCode;
    
    nsresult rv = aMessage->GetErrorCode(&errorCode);

    if(NS_SUCCEEDED(rv))
    {
        if(errorCode == nsILDAPErrors::SUCCESS || errorCode == nsILDAPErrors::SIZELIMIT_EXCEEDED) {
            switch(mState) {
            case kSearchingAuthDN :
                rv = OnSearchAuthDNDone();
                break;
            case kSearchingRootDSE:
             {
                // Before starting the changeLog check the DB file, if its not there or bogus
                // we need to create a new one and set to all.
                nsCOMPtr<nsIAddrBookSession> abSession = do_GetService(NS_ADDRBOOKSESSION_CONTRACTID, &rv);
                if (NS_FAILED(rv)) 
                    break;
                nsCOMPtr<nsIFile> dbPath;
                rv = abSession->GetUserProfileDirectory(getter_AddRefs(dbPath));
                if (NS_FAILED(rv)) 
                    break;

                nsCAutoString fileName;
                rv = mDirectory->GetReplicationFileName(fileName);
                if (NS_FAILED(rv))
                  break;

                rv = dbPath->AppendNative(fileName);
                if (NS_FAILED(rv)) 
                    break;

                bool fileExists;
                rv = dbPath->Exists(&fileExists);
                if (NS_FAILED(rv)) 
                    break;

                int64_t fileSize;
                rv = dbPath->GetFileSize(&fileSize);
                if(NS_FAILED(rv)) 
                    break;

                if (!fileExists || !fileSize)
                    mUseChangeLog = false;

                // Open / create the AB here since it calls Done,
                // just return from here.
                if (mUseChangeLog)
                   rv = OpenABForReplicatedDir(false);
                else
                   rv = OpenABForReplicatedDir(true);
                if (NS_FAILED(rv))
                   return rv;
                
                // Now start the appropriate query
                rv = OnSearchRootDSEDone();
                break;
             }
            case kFindingChanges:
                rv = OnFindingChangesDone();
                // If success we return from here since
                // this changes state to kReplicatingChanges
                // and it falls thru into the if clause below.
                if (NS_SUCCEEDED(rv))
                    return rv;
                break;
            case kReplicatingAll :
                return nsAbLDAPProcessReplicationData::OnLDAPSearchResult(aMessage);
            } // end of switch
        }
        else
            rv = NS_ERROR_FAILURE;
        // If one of the changed entry in changelog is not found,
        // continue with replicating the next one.
        if(mState == kReplicatingChanges)
            rv = OnReplicatingChangeDone();
    } // end of outer if

    if(NS_FAILED(rv))
        Abort();

    return rv;
}

nsresult nsAbLDAPProcessChangeLogData::GetAuthData()
{
    if(!mInitialized) 
        return NS_ERROR_NOT_INITIALIZED;

    nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService(NS_WINDOWWATCHER_CONTRACTID));
    if (!wwatch)
        return NS_ERROR_FAILURE;
    
    nsCOMPtr<nsIAuthPrompt> dialog;
    nsresult rv = wwatch->GetNewAuthPrompter(0, getter_AddRefs(dialog));
    if (NS_FAILED(rv))
        return rv;
    if (!dialog) 
        return NS_ERROR_FAILURE;

    nsCOMPtr<nsILDAPURL> url;
    rv = mQuery->GetReplicationURL(getter_AddRefs(url));
    if (NS_FAILED(rv))
        return rv;

    nsCAutoString serverUri;
    rv = url->GetSpec(serverUri);
    if (NS_FAILED(rv)) 
        return rv;

    nsCOMPtr<nsIStringBundleService> bundleService =
      mozilla::services::GetStringBundleService();
    NS_ENSURE_TRUE(bundleService, NS_ERROR_UNEXPECTED);
    nsCOMPtr<nsIStringBundle> bundle;
    rv = bundleService->CreateBundle("chrome://messenger/locale/addressbook/addressBook.properties", getter_AddRefs(bundle));
    if (NS_FAILED (rv)) 
        return rv ;

    nsString title;
    rv = bundle->GetStringFromName(NS_LITERAL_STRING("AuthDlgTitle").get(), getter_Copies(title));
    if (NS_FAILED (rv)) 
        return rv ;

    nsString desc;
    rv = bundle->GetStringFromName(NS_LITERAL_STRING("AuthDlgDesc").get(), getter_Copies(desc));
    if (NS_FAILED (rv)) 
        return rv ;

    nsString username;
    nsString password;
    bool btnResult = false;
	rv = dialog->PromptUsernameAndPassword(title, desc, 
                                            NS_ConvertUTF8toUTF16(serverUri).get(), 
                                            nsIAuthPrompt::SAVE_PASSWORD_PERMANENTLY,
                                            getter_Copies(username), getter_Copies(password), 
                                            &btnResult);
    if(NS_SUCCEEDED(rv) && btnResult) {
        CopyUTF16toUTF8(username, mAuthUserID);
        CopyUTF16toUTF8(password, mAuthPswd);
    }
    else
        rv = NS_ERROR_FAILURE;

    return rv;
}

nsresult nsAbLDAPProcessChangeLogData::OnSearchAuthDNDone()
{
  if (!mInitialized)
    return NS_ERROR_NOT_INITIALIZED;

    nsCOMPtr<nsILDAPURL> url;
    nsresult rv = mQuery->GetReplicationURL(getter_AddRefs(url));
    if(NS_SUCCEEDED(rv))
        rv = mQuery->ConnectToLDAPServer(url, mAuthDN);
    if(NS_SUCCEEDED(rv)) {
        mState = kAuthenticatedBinding;
        rv = mDirectory->SetAuthDn(mAuthDN);
    }

    return rv;
}

nsresult nsAbLDAPProcessChangeLogData::ParseRootDSEEntry(nsILDAPMessage *aMessage)
{
    NS_ENSURE_ARG_POINTER(aMessage);
    if (!mInitialized)
        return NS_ERROR_NOT_INITIALIZED;

    // Populate the RootDSEChangeLogEntry
    CharPtrArrayGuard attrs;
    nsresult rv = aMessage->GetAttributes(attrs.GetSizeAddr(), attrs.GetArrayAddr());
    // No attributes
    if(NS_FAILED(rv)) 
        return rv;

    for(int32_t i=attrs.GetSize()-1; i >= 0; i--) {
        PRUnicharPtrArrayGuard vals;
        rv = aMessage->GetValues(attrs.GetArray()[i], vals.GetSizeAddr(), vals.GetArrayAddr());
        if(NS_FAILED(rv))
            continue;
        if(vals.GetSize()) {
            if (!PL_strcasecmp(attrs[i], "changelog"))
                CopyUTF16toUTF8(vals[0], mRootDSEEntry.changeLogDN);
            if (!PL_strcasecmp(attrs[i], "firstChangeNumber"))
                mRootDSEEntry.firstChangeNumber = atol(NS_LossyConvertUTF16toASCII(vals[0]).get());
            if (!PL_strcasecmp(attrs[i], "lastChangeNumber"))
                mRootDSEEntry.lastChangeNumber = atol(NS_LossyConvertUTF16toASCII(vals[0]).get());
            if (!PL_strcasecmp(attrs[i], "dataVersion"))
                CopyUTF16toUTF8(vals[0], mRootDSEEntry.dataVersion);
        }
    }

    int32_t lastChangeNumber;
    mDirectory->GetLastChangeNumber(&lastChangeNumber);

    if ((mRootDSEEntry.lastChangeNumber > 0) &&
        (lastChangeNumber < mRootDSEEntry.lastChangeNumber) &&
        (lastChangeNumber > mRootDSEEntry.firstChangeNumber))
        mUseChangeLog = true;

    if (mRootDSEEntry.lastChangeNumber &&
        (lastChangeNumber == mRootDSEEntry.lastChangeNumber)) {
        Done(true); // We are up to date no need to replicate, db not open yet so call Done
        return NS_OK;
    }

    return rv;
}

nsresult nsAbLDAPProcessChangeLogData::OnSearchRootDSEDone()
{
    if (!mInitialized)
        return NS_ERROR_NOT_INITIALIZED;

    nsresult rv = NS_OK;

    if(mUseChangeLog) {
        rv = mChangeLogQuery->QueryChangeLog(mRootDSEEntry.changeLogDN, mRootDSEEntry.lastChangeNumber);
        if (NS_FAILED(rv))
           return rv;
        mState = kFindingChanges;
        if(mListener)
            mListener->OnStateChange(nullptr, nullptr, nsIWebProgressListener::STATE_START, false);
    }
    else {
        rv = mQuery->QueryAllEntries();
        if (NS_FAILED(rv))
           return rv;
        mState = kReplicatingAll;
        if(mListener)
            mListener->OnStateChange(nullptr, nullptr, nsIWebProgressListener::STATE_START, true);
    }

    rv = mDirectory->SetLastChangeNumber(mRootDSEEntry.lastChangeNumber);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mDirectory->SetDataVersion(mRootDSEEntry.dataVersion);

    return rv;
}

nsresult nsAbLDAPProcessChangeLogData::ParseChangeLogEntries(nsILDAPMessage *aMessage)
{
    NS_ENSURE_ARG_POINTER(aMessage);
    if(!mInitialized) 
        return NS_ERROR_NOT_INITIALIZED;

    // Populate the RootDSEChangeLogEntry
    CharPtrArrayGuard attrs;
    nsresult rv = aMessage->GetAttributes(attrs.GetSizeAddr(), attrs.GetArrayAddr());
    // No attributes
    if(NS_FAILED(rv)) 
        return rv;

    nsAutoString targetDN;
    UpdateOp operation = NO_OP;
    for(int32_t i = attrs.GetSize()-1; i >= 0; i--) {
        PRUnicharPtrArrayGuard vals;
        rv = aMessage->GetValues(attrs.GetArray()[i], vals.GetSizeAddr(), vals.GetArrayAddr());
        if(NS_FAILED(rv))
            continue;
        if(vals.GetSize()) {
            if (!PL_strcasecmp(attrs[i], "targetdn"))
                targetDN = vals[0];
            if (!PL_strcasecmp(attrs[i], "changetype")) {
                if (!Compare(nsDependentString(vals[0]), NS_LITERAL_STRING("add"), nsCaseInsensitiveStringComparator()))
                    operation = ENTRY_ADD;
                if (!Compare(nsDependentString(vals[0]), NS_LITERAL_STRING("modify"), nsCaseInsensitiveStringComparator()))
                    operation = ENTRY_MODIFY;
                if (!Compare(nsDependentString(vals[0]), NS_LITERAL_STRING("delete"), nsCaseInsensitiveStringComparator()))
                    operation = ENTRY_DELETE;
            }
        }
    }

    mChangeLogEntriesCount++;
    if(!(mChangeLogEntriesCount % 10)) { // Inform the listener every 10 entries
        mListener->OnProgressChange(nullptr,nullptr,mChangeLogEntriesCount, -1, mChangeLogEntriesCount, -1);
        // In case if the LDAP Connection thread is starved and causes problem
        // uncomment this one and try.
        // PR_Sleep(PR_INTERVAL_NO_WAIT); // give others a chance
    }

#ifdef DEBUG_rdayal
    printf ("ChangeLog Replication : Updated Entry : %s for OpType : %u\n", 
                                    NS_ConvertUTF16toUTF8(targetDN).get(), operation);
#endif

    switch(operation) {
    case ENTRY_ADD:
        // Add the DN to the add list if not already in the list
        if(!(mEntriesToAdd.IndexOf(targetDN) >= 0))
            mEntriesToAdd.AppendString(targetDN);
        break;
    case ENTRY_DELETE:
        // Do not check the return here since delete may fail if
        // entry deleted in changelog does not exist in DB
        // for e.g if the user specifies a filter, so go next entry
        DeleteCard(targetDN);
        break;
    case ENTRY_MODIFY:
        // For modify, delete the entry from DB and add updated entry
        // we do this since we cannot access the changes attribs of changelog
        rv = DeleteCard(targetDN);
        if (NS_SUCCEEDED(rv)) 
            if(!(mEntriesToAdd.IndexOf(targetDN) >= 0))
                mEntriesToAdd.AppendString(targetDN);
        break;
    default:
        // Should not come here, would come here only
        // if the entry is not a changeLog entry
        NS_WARNING("nsAbLDAPProcessChangeLogData::ParseChangeLogEntries"
           "Not an changelog entry");
    }

    // Go ahead processing the next entry, a modify or delete DB operation
    // can 'correctly' fail if the entry is not present in the DB,
    // e.g. in case a filter is specified.
    return NS_OK;
}

nsresult nsAbLDAPProcessChangeLogData::OnFindingChangesDone()
{
    if(!mInitialized) 
        return NS_ERROR_NOT_INITIALIZED;

#ifdef DEBUG_rdayal
    printf ("ChangeLog Replication : Finding Changes Done \n");
#endif

    nsresult rv = NS_OK;

    // No entries to add/update (for updates too we delete and add) entries,
    // we took care of deletes in ParseChangeLogEntries, all Done!
    mEntriesAddedQueryCount = mEntriesToAdd.Count();
    if(mEntriesAddedQueryCount <= 0) {
        if(mReplicationDB && mDBOpen) {
            // Close the DB, no need to commit since we have not made
            // any changes yet to the DB.
            rv = mReplicationDB->Close(false);
            NS_ASSERTION(NS_SUCCEEDED(rv), "Replication DB Close(no commit) on Success failed");
            mDBOpen = false;
            // Once are done with the replication file, delete the backup file
            if(mBackupReplicationFile) {
                rv = mBackupReplicationFile->Remove(false);
                NS_ASSERTION(NS_SUCCEEDED(rv), "Replication BackupFile Remove on Success failed");
            }
        }
        Done(true);
        return NS_OK;
    }

    // Decrement the count first to get the correct array element
    mEntriesAddedQueryCount--;
    rv = mChangeLogQuery->QueryChangedEntries(NS_ConvertUTF16toUTF8(*(mEntriesToAdd[mEntriesAddedQueryCount])));
    if (NS_FAILED(rv))
        return rv;

    if(mListener && NS_SUCCEEDED(rv))
        mListener->OnStateChange(nullptr, nullptr, nsIWebProgressListener::STATE_START, true);

    mState = kReplicatingChanges;
    return rv;
}

nsresult nsAbLDAPProcessChangeLogData::OnReplicatingChangeDone()
{
    if(!mInitialized) 
        return NS_ERROR_NOT_INITIALIZED;

    nsresult rv = NS_OK;

    if(!mEntriesAddedQueryCount)
    {
        if(mReplicationDB && mDBOpen) {
            rv = mReplicationDB->Close(true); // Commit and close the DB
            NS_ASSERTION(NS_SUCCEEDED(rv), "Replication DB Close (commit) on Success failed");
            mDBOpen = false;
        }
        // Once we done with the replication file, delete the backup file.
        if(mBackupReplicationFile) {
            rv = mBackupReplicationFile->Remove(false);
            NS_ASSERTION(NS_SUCCEEDED(rv), "Replication BackupFile Remove on Success failed");
        }
        Done(true);  // All data is received
        return NS_OK;
    }

    // Remove the entry already added from the list and query the next one.
    if(mEntriesAddedQueryCount < mEntriesToAdd.Count() && mEntriesAddedQueryCount >= 0)
        mEntriesToAdd.RemoveStringAt(mEntriesAddedQueryCount);
    mEntriesAddedQueryCount--;
    rv = mChangeLogQuery->QueryChangedEntries(NS_ConvertUTF16toUTF8(*(mEntriesToAdd[mEntriesAddedQueryCount])));

    return rv;
}

