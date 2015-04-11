/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// this file implements the nsAddrDatabase interface using the MDB Interface.

#include "nsAddrDatabase.h"
#include "nsStringGlue.h"
#include "nsAutoPtr.h"
#include "nsUnicharUtils.h"
#include "nsAbBaseCID.h"
#include "nsIAbMDBDirectory.h"
#include "nsServiceManagerUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsMsgUtils.h"
#include "nsMorkCID.h"
#include "nsIMdbFactoryFactory.h"
#include "prprf.h"
#include "nsIMutableArray.h"
#include "nsArrayUtils.h"
#include "nsIPromptService.h"
#include "nsIStringBundle.h"
#include "nsIFile.h"
#include "nsEmbedCID.h"
#include "nsIProperty.h"
#include "nsIVariant.h"
#include "nsCOMArray.h"
#include "nsArrayEnumerator.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIAbManager.h"
#include "mozilla/Services.h"

#define ID_PAB_TABLE            1
#define ID_DELETEDCARDS_TABLE           2

// There's two books by default, although Mac may have one more, so set this
// to three. Its not going to affect much, but will save us a few reallocations
// when the cache is allocated.
const uint32_t kInitialAddrDBCacheSize = 3;

const int32_t kAddressBookDBVersion = 1;

static const char kPabTableKind[] = "ns:addrbk:db:table:kind:pab";
static const char kDeletedCardsTableKind[] = "ns:addrbk:db:table:kind:deleted"; // this table is used to keep the deleted cards

static const char kCardRowScope[] = "ns:addrbk:db:row:scope:card:all";
static const char kListRowScope[] = "ns:addrbk:db:row:scope:list:all";
static const char kDataRowScope[] = "ns:addrbk:db:row:scope:data:all";

#define DATAROW_ROWID 1

#define COLUMN_STR_MAX 16

#define PURGE_CUTOFF_COUNT 50

static const char kRecordKeyColumn[] = "RecordKey";
static const char kLastRecordKeyColumn[] = "LastRecordKey";
static const char kRowIDProperty[] = "DbRowID";

static const char kMailListTotalLists[] = "ListTotalLists";    // total number of mail list in a mailing list
static const char kLowerListNameColumn[] = "LowercaseListName";

struct mdbOid gAddressBookTableOID;

static const char kMailListAddressFormat[] = "Address%d";

nsAddrDatabase::nsAddrDatabase()
    : m_mdbEnv(nullptr), m_mdbStore(nullptr),
      m_mdbPabTable(nullptr),
      m_mdbDeletedCardsTable(nullptr),
      m_mdbTokensInitialized(false),
      m_PabTableKind(0),
      m_MailListTableKind(0),
      m_DeletedCardsTableKind(0),
      m_CardRowScopeToken(0),
      m_FirstNameColumnToken(0),
      m_LastNameColumnToken(0),
      m_PhoneticFirstNameColumnToken(0),
      m_PhoneticLastNameColumnToken(0),
      m_DisplayNameColumnToken(0),
      m_NickNameColumnToken(0),
      m_PriEmailColumnToken(0),
      m_2ndEmailColumnToken(0),
      m_WorkPhoneColumnToken(0),
      m_HomePhoneColumnToken(0),
      m_FaxColumnToken(0),
      m_PagerColumnToken(0),
      m_CellularColumnToken(0),
      m_WorkPhoneTypeColumnToken(0),
      m_HomePhoneTypeColumnToken(0),
      m_FaxTypeColumnToken(0),
      m_PagerTypeColumnToken(0),
      m_CellularTypeColumnToken(0),
      m_HomeAddressColumnToken(0),
      m_HomeAddress2ColumnToken(0),
      m_HomeCityColumnToken(0),
      m_HomeStateColumnToken(0),
      m_HomeZipCodeColumnToken(0),
      m_HomeCountryColumnToken(0),
      m_WorkAddressColumnToken(0),
      m_WorkAddress2ColumnToken(0),
      m_WorkCityColumnToken(0),
      m_WorkStateColumnToken(0),
      m_WorkZipCodeColumnToken(0),
      m_WorkCountryColumnToken(0),
      m_CompanyColumnToken(0),
      m_AimScreenNameColumnToken(0),
      m_AnniversaryYearColumnToken(0),
      m_AnniversaryMonthColumnToken(0),
      m_AnniversaryDayColumnToken(0),
      m_SpouseNameColumnToken(0),
      m_FamilyNameColumnToken(0),
      m_DefaultAddressColumnToken(0),
      m_CategoryColumnToken(0),
      m_WebPage1ColumnToken(0),
      m_WebPage2ColumnToken(0),
      m_BirthYearColumnToken(0),
      m_BirthMonthColumnToken(0),
      m_BirthDayColumnToken(0),
      m_Custom1ColumnToken(0),
      m_Custom2ColumnToken(0),
      m_Custom3ColumnToken(0),
      m_Custom4ColumnToken(0),
      m_NotesColumnToken(0),
      m_LastModDateColumnToken(0),
      m_MailFormatColumnToken(0),
      m_PopularityIndexColumnToken(0),
      m_AllowRemoteContentColumnToken(0),
      m_AddressCharSetColumnToken(0),
      m_LastRecordKey(0),
      m_dbDirectory(nullptr)
{
}

nsAddrDatabase::~nsAddrDatabase()
{
    Close(false);    // better have already been closed.

    // better not be any listeners, because we're going away.
    NS_ASSERTION(m_ChangeListeners.Length() == 0, "shouldn't have any listeners");

    RemoveFromCache(this);
}

NS_IMPL_THREADSAFE_ADDREF(nsAddrDatabase)

NS_IMETHODIMP_(nsrefcnt) nsAddrDatabase::Release(void)
{
  // XXX FIX THIS
  NS_PRECONDITION(0 != mRefCnt, "dup release");
  nsrefcnt count = NS_AtomicDecrementRefcnt(mRefCnt);
  NS_LOG_RELEASE(this, count,"nsAddrDatabase");
  if (count == 0)    // OK, the cache is no longer holding onto this, so we really want to delete it,
  {                // after removing it from the cache.
    mRefCnt = 1; /* stabilize */
    RemoveFromCache(this);
    // clean up after ourself!
    if (m_mdbPabTable)
      m_mdbPabTable->Release();
    if (m_mdbDeletedCardsTable)
      m_mdbDeletedCardsTable->Release();
    NS_IF_RELEASE(m_mdbStore);
    NS_IF_RELEASE(m_mdbEnv);
    delete this;
    return 0;
  }
  return count;
}

NS_IMETHODIMP nsAddrDatabase::QueryInterface(REFNSIID aIID, void** aResult)
{
    if (aResult == NULL)
        return NS_ERROR_NULL_POINTER;

    if (aIID.Equals(NS_GET_IID(nsIAddrDatabase)) ||
        aIID.Equals(NS_GET_IID(nsIAddrDBAnnouncer)) ||
        aIID.Equals(NS_GET_IID(nsISupports))) {
        *aResult = static_cast<nsIAddrDatabase*>(this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}

NS_IMETHODIMP nsAddrDatabase::AddListener(nsIAddrDBListener *listener)
{
  NS_ENSURE_ARG_POINTER(listener);
  return m_ChangeListeners.AppendElement(listener) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsAddrDatabase::RemoveListener(nsIAddrDBListener *listener)
{
  NS_ENSURE_ARG_POINTER(listener);
  return m_ChangeListeners.RemoveElement(listener) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsAddrDatabase::NotifyCardAttribChange(uint32_t abCode)
{
  NS_OBSERVER_ARRAY_NOTIFY_OBSERVERS(m_ChangeListeners, nsIAddrDBListener,
                                     OnCardAttribChange, (abCode));
  return NS_OK;
}

NS_IMETHODIMP nsAddrDatabase::NotifyCardEntryChange(uint32_t aAbCode, nsIAbCard *aCard, nsIAbDirectory *aParent)
{
  int32_t currentDisplayNameVersion = 0;

  //Update "mail.displayname.version" prefernce
  nsCOMPtr<nsIPrefBranch> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID));

  prefs->GetIntPref("mail.displayname.version",&currentDisplayNameVersion);

  prefs->SetIntPref("mail.displayname.version",++currentDisplayNameVersion);

  NS_OBSERVER_ARRAY_NOTIFY_OBSERVERS(m_ChangeListeners, nsIAddrDBListener,
                                     OnCardEntryChange, (aAbCode, aCard, aParent));
  return NS_OK;
}

nsresult nsAddrDatabase::NotifyListEntryChange(uint32_t abCode, nsIAbDirectory *dir)
{
  NS_OBSERVER_ARRAY_NOTIFY_OBSERVERS(m_ChangeListeners, nsIAddrDBListener,
                                     OnListEntryChange, (abCode, dir));
  return NS_OK;
}


NS_IMETHODIMP nsAddrDatabase::NotifyAnnouncerGoingAway(void)
{
  NS_OBSERVER_ARRAY_NOTIFY_OBSERVERS(m_ChangeListeners, nsIAddrDBListener,
                                     OnAnnouncerGoingAway, ());
  return NS_OK;
}


// Apparently its not good for nsTArray to be allocated as static. Don't know
// why it isn't but its not, so don't think about making it a static variable.
// Maybe bz knows.
nsTArray<nsAddrDatabase*>* nsAddrDatabase::m_dbCache = nullptr;

nsTArray<nsAddrDatabase*>*
nsAddrDatabase::GetDBCache()
{
  if (!m_dbCache)
    m_dbCache = new nsAutoTArray<nsAddrDatabase*, kInitialAddrDBCacheSize>;

  return m_dbCache;
}

void
nsAddrDatabase::CleanupCache()
{
  if (m_dbCache)
  {
    for (int32_t i = m_dbCache->Length() - 1; i >= 0; --i)
    {
      nsAddrDatabase* pAddrDB = m_dbCache->ElementAt(i);
      if (pAddrDB)
        pAddrDB->ForceClosed();
    }
    //        NS_ASSERTION(m_dbCache.Length() == 0, "some msg dbs left open");    // better not be any open db's.
    delete m_dbCache;
    m_dbCache = nullptr;
  }
}

//----------------------------------------------------------------------
// FindInCache - this addrefs the db it finds.
//----------------------------------------------------------------------
nsAddrDatabase* nsAddrDatabase::FindInCache(nsIFile *dbName)
{
  nsTArray<nsAddrDatabase*>* dbCache = GetDBCache();
  uint32_t length = dbCache->Length();
  for (uint32_t i = 0; i < length; ++i)
  {
    nsAddrDatabase* pAddrDB = dbCache->ElementAt(i);
    if (pAddrDB->MatchDbName(dbName))
    {
      NS_ADDREF(pAddrDB);
      return pAddrDB;
    }
  }
  return nullptr;
}

bool nsAddrDatabase::MatchDbName(nsIFile* dbName)    // returns true if they match
{
    bool dbMatches = false;

    nsresult rv = m_dbName->Equals(dbName, &dbMatches);
    if (NS_FAILED(rv))
      return false;

    return dbMatches;
}

//----------------------------------------------------------------------
// RemoveFromCache
//----------------------------------------------------------------------
void nsAddrDatabase::RemoveFromCache(nsAddrDatabase* pAddrDB)
{
  if (m_dbCache)
    m_dbCache->RemoveElement(pAddrDB);
}

void nsAddrDatabase::GetMDBFactory(nsIMdbFactory ** aMdbFactory)
{
  if (!mMdbFactory)
  {
    nsresult rv;
    nsCOMPtr <nsIMdbFactoryService> mdbFactoryService = do_GetService(NS_MORK_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv) && mdbFactoryService)
      rv = mdbFactoryService->GetMdbFactory(getter_AddRefs(mMdbFactory));
  }
  NS_IF_ADDREF(*aMdbFactory = mMdbFactory);
}

/* caller need to delete *aDbPath */
NS_IMETHODIMP nsAddrDatabase::GetDbPath(nsIFile* *aDbPath)
{
    if (!aDbPath)
        return NS_ERROR_NULL_POINTER;

    return m_dbName->Clone(aDbPath);
}

NS_IMETHODIMP nsAddrDatabase::SetDbPath(nsIFile* aDbPath)
{
    return aDbPath->Clone(getter_AddRefs(m_dbName));
}

NS_IMETHODIMP nsAddrDatabase::Open
(nsIFile *aMabFile, bool aCreate, bool upgrading /* unused */, nsIAddrDatabase** pAddrDB)
{
  *pAddrDB = nullptr;

  nsAddrDatabase *pAddressBookDB = FindInCache(aMabFile);

  if (pAddressBookDB) {
    *pAddrDB = pAddressBookDB;
    return NS_OK;
  }

  nsresult rv = OpenInternal(aMabFile, aCreate, pAddrDB);
  if (NS_SUCCEEDED(rv))
    return NS_OK;

  if (rv == NS_ERROR_FILE_ACCESS_DENIED)
  {
    static bool gAlreadyAlerted;
     // only do this once per session to avoid annoying the user
    if (!gAlreadyAlerted)
    {
      gAlreadyAlerted = true;
      nsAutoString mabFileName;
      rv = aMabFile->GetLeafName(mabFileName);
      NS_ENSURE_SUCCESS(rv, rv);
      AlertAboutLockedMabFile(mabFileName.get());

      // We just overwrote rv, so return the proper value here.
      return NS_ERROR_FILE_ACCESS_DENIED;
    }
  }
  // try one more time
  // but first rename corrupt mab file
  // and prompt the user
  else if (aCreate)
  {
    nsCOMPtr<nsIFile> dummyBackupMabFile;
    nsCOMPtr<nsIFile> actualBackupMabFile;

    // First create a clone of the corrupt mab file that we'll
    // use to generate the name for the backup file that we are
    // going to move it to.
    rv = aMabFile->Clone(getter_AddRefs(dummyBackupMabFile));
    NS_ENSURE_SUCCESS(rv, rv);

    // Now create a second clone that we'll use to do the move
    // (this allows us to leave the original name intact)
    rv = aMabFile->Clone(getter_AddRefs(actualBackupMabFile));
    NS_ENSURE_SUCCESS(rv, rv);

    // Now we try and generate a new name for the corrupt mab
    // file using the dummy backup mab file

    // First append .bak - we have to do this the long way as
    // AppendNative is to the path, not the LeafName.
    nsCAutoString dummyBackupMabFileName;
    rv = dummyBackupMabFile->GetNativeLeafName(dummyBackupMabFileName);
    NS_ENSURE_SUCCESS(rv, rv);

    dummyBackupMabFileName.Append(NS_LITERAL_CSTRING(".bak"));

    rv = dummyBackupMabFile->SetNativeLeafName(dummyBackupMabFileName);
    NS_ENSURE_SUCCESS(rv, rv);

    // Now see if we can create it unique
    rv = dummyBackupMabFile->CreateUnique(nsIFile::NORMAL_FILE_TYPE, 0600);
    NS_ENSURE_SUCCESS(rv, rv);

    // Now get the new name
    nsCAutoString backupMabFileName;
    rv = dummyBackupMabFile->GetNativeLeafName(backupMabFileName);
    NS_ENSURE_SUCCESS(rv, rv);

    // And the parent directory
    nsCOMPtr<nsIFile> parentDir;
    rv = dummyBackupMabFile->GetParent(getter_AddRefs(parentDir));
    NS_ENSURE_SUCCESS(rv, rv);

    // Now move the corrupt file to its backup location
    rv = actualBackupMabFile->MoveToNative(parentDir, backupMabFileName);
    NS_ASSERTION(NS_SUCCEEDED(rv), "failed to rename corrupt mab file");

    if (NS_SUCCEEDED(rv)) {
      // now we can try to recreate the original mab file
      rv = OpenInternal(aMabFile, aCreate, pAddrDB);
      NS_ASSERTION(NS_SUCCEEDED(rv), "failed to create .mab file, after rename");

      if (NS_SUCCEEDED(rv)) {
        nsAutoString originalMabFileName;
        rv = aMabFile->GetLeafName(originalMabFileName);
        NS_ENSURE_SUCCESS(rv, rv);

        // if this fails, we don't care
        (void)AlertAboutCorruptMabFile(originalMabFileName.get(),
          NS_ConvertASCIItoUTF16(backupMabFileName).get());
      }
    }
  }
  return rv;
}

nsresult nsAddrDatabase::DisplayAlert(const PRUnichar *titleName, const PRUnichar *alertStringName, const PRUnichar **formatStrings, int32_t numFormatStrings)
{
  nsresult rv;
  nsCOMPtr<nsIStringBundleService> bundleService =
    mozilla::services::GetStringBundleService();
  NS_ENSURE_TRUE(bundleService, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsIStringBundle> bundle;
  rv = bundleService->CreateBundle("chrome://messenger/locale/addressbook/addressBook.properties", getter_AddRefs(bundle));
  NS_ENSURE_SUCCESS(rv, rv);

  nsString alertMessage;
  rv = bundle->FormatStringFromName(alertStringName, formatStrings, numFormatStrings,
    getter_Copies(alertMessage));
  NS_ENSURE_SUCCESS(rv, rv);

  nsString alertTitle;
  rv = bundle->GetStringFromName(titleName, getter_Copies(alertTitle));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIPromptService> prompter =
      do_GetService(NS_PROMPTSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  return prompter->Alert(nullptr /* we don't know the parent window */, alertTitle.get(), alertMessage.get());
}

nsresult nsAddrDatabase::AlertAboutCorruptMabFile(const PRUnichar *aOldFileName, const PRUnichar *aNewFileName)
{
  const PRUnichar *formatStrings[] = { aOldFileName, aOldFileName, aNewFileName };
  return DisplayAlert(NS_LITERAL_STRING("corruptMabFileTitle").get(),
    NS_LITERAL_STRING("corruptMabFileAlert").get(), formatStrings, 3);
}

nsresult nsAddrDatabase::AlertAboutLockedMabFile(const PRUnichar *aFileName)
{
  const PRUnichar *formatStrings[] = { aFileName };
  return DisplayAlert(NS_LITERAL_STRING("lockedMabFileTitle").get(),
    NS_LITERAL_STRING("lockedMabFileAlert").get(), formatStrings, 1);
}

nsresult
nsAddrDatabase::OpenInternal(nsIFile *aMabFile, bool aCreate, nsIAddrDatabase** pAddrDB)
{
  nsAddrDatabase *pAddressBookDB = new nsAddrDatabase();
  if (!pAddressBookDB) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(pAddressBookDB);

  nsresult rv = pAddressBookDB->OpenMDB(aMabFile, aCreate);
  if (NS_SUCCEEDED(rv))
  {
    pAddressBookDB->SetDbPath(aMabFile);
    GetDBCache()->AppendElement(pAddressBookDB);
    *pAddrDB = pAddressBookDB;
  }
  else
  {
    *pAddrDB = nullptr;
    pAddressBookDB->ForceClosed();
    NS_IF_RELEASE(pAddressBookDB);
    pAddressBookDB = nullptr;
  }
  return rv;
}

// Open the MDB database synchronously. If successful, this routine
// will set up the m_mdbStore and m_mdbEnv of the database object
// so other database calls can work.
NS_IMETHODIMP nsAddrDatabase::OpenMDB(nsIFile *dbName, bool create)
{
  nsresult ret;
  nsCOMPtr<nsIMdbFactory> mdbFactory;
  GetMDBFactory(getter_AddRefs(mdbFactory));
  NS_ENSURE_TRUE(mdbFactory, NS_ERROR_FAILURE);

  ret = mdbFactory->MakeEnv(NULL, &m_mdbEnv);
  if (NS_SUCCEEDED(ret))
  {
    nsIMdbThumb *thumb = nullptr;
    nsCAutoString filePath;

    ret = dbName->GetNativePath(filePath);
    NS_ENSURE_SUCCESS(ret, ret);

    nsIMdbHeap* dbHeap = 0;
    mdb_bool dbFrozen = mdbBool_kFalse; // not readonly, we want modifiable

    if (m_mdbEnv)
      m_mdbEnv->SetAutoClear(true);

    bool dbNameExists = false;
    ret = dbName->Exists(&dbNameExists);
    NS_ENSURE_SUCCESS(ret, ret);

    if (!dbNameExists)
      ret = NS_ERROR_FILE_NOT_FOUND;
    else
    {
      mdbOpenPolicy inOpenPolicy;
      mdb_bool    canOpen;
      mdbYarn        outFormatVersion;
      nsIMdbFile* oldFile = 0;
      int64_t fileSize;
      ret = dbName->GetFileSize(&fileSize);
      NS_ENSURE_SUCCESS(ret, ret);

      ret = mdbFactory->OpenOldFile(m_mdbEnv, dbHeap, filePath.get(),
                                    dbFrozen, &oldFile);
      if ( oldFile )
      {
        if ( ret == NS_OK )
        {
          ret = mdbFactory->CanOpenFilePort(m_mdbEnv, oldFile, // the file to investigate
                                            &canOpen, &outFormatVersion);
          if (ret == 0 && canOpen)
          {
            inOpenPolicy.mOpenPolicy_ScopePlan.mScopeStringSet_Count = 0;
            inOpenPolicy.mOpenPolicy_MinMemory = 0;
            inOpenPolicy.mOpenPolicy_MaxLazy = 0;

            ret = mdbFactory->OpenFileStore(m_mdbEnv, dbHeap,
                                            oldFile, &inOpenPolicy, &thumb);
          }
          else if (fileSize != 0)
            ret = NS_ERROR_FILE_ACCESS_DENIED;
        }
        NS_RELEASE(oldFile); // always release our file ref, store has own
      }
      if (NS_FAILED(ret))
        ret = NS_ERROR_FILE_ACCESS_DENIED;
    }

    if (NS_SUCCEEDED(ret) && thumb)
    {
      mdb_count outTotal;    // total somethings to do in operation
      mdb_count outCurrent;  // subportion of total completed so far
      mdb_bool outDone = false;      // is operation finished?
      mdb_bool outBroken;     // is operation irreparably dead and broken?
      do
      {
        ret = thumb->DoMore(m_mdbEnv, &outTotal, &outCurrent, &outDone, &outBroken);
        if (ret != 0)
        {
          outDone = true;
          break;
        }
      }
      while (NS_SUCCEEDED(ret) && !outBroken && !outDone);
      if (NS_SUCCEEDED(ret) && outDone)
      {
        ret = mdbFactory->ThumbToOpenStore(m_mdbEnv, thumb, &m_mdbStore);
        if (ret == NS_OK && m_mdbStore)
        {
          ret = InitExistingDB();
          create = false;
        }
      }
    }
    else if (create && ret != NS_ERROR_FILE_ACCESS_DENIED)
    {
      nsIMdbFile* newFile = 0;
      ret = mdbFactory->CreateNewFile(m_mdbEnv, dbHeap, filePath.get(), &newFile);
      if ( newFile )
      {
        if (ret == NS_OK)
        {
          mdbOpenPolicy inOpenPolicy;

          inOpenPolicy.mOpenPolicy_ScopePlan.mScopeStringSet_Count = 0;
          inOpenPolicy.mOpenPolicy_MinMemory = 0;
          inOpenPolicy.mOpenPolicy_MaxLazy = 0;

          ret = mdbFactory->CreateNewFileStore(m_mdbEnv, dbHeap,
                                               newFile, &inOpenPolicy,
                                               &m_mdbStore);
          if (ret == NS_OK)
            ret = InitNewDB();
        }
        NS_RELEASE(newFile); // always release our file ref, store has own
      }
    }
    NS_IF_RELEASE(thumb);
  }
  //Convert the DB error to a valid nsresult error.
  if (ret == 1)
    ret = NS_ERROR_FAILURE;
  return ret;
}

NS_IMETHODIMP nsAddrDatabase::CloseMDB(bool commit)
{
    if (commit)
        Commit(nsAddrDBCommitType::kSessionCommit);
//???    RemoveFromCache(this);  // if we've closed it, better not leave it in the cache.
    return NS_OK;
}

// force the database to close - this'll flush out anybody holding onto
// a database without having a listener!
// This is evil in the com world, but there are times we need to delete the file.
NS_IMETHODIMP nsAddrDatabase::ForceClosed()
{
    nsresult    err = NS_OK;
    nsCOMPtr<nsIAddrDatabase> aDb(do_QueryInterface(this, &err));

    // make sure someone has a reference so object won't get deleted out from under us.
    AddRef();
    NotifyAnnouncerGoingAway();
    // OK, remove from cache first and close the store.
    RemoveFromCache(this);

    err = CloseMDB(false);    // since we're about to delete it, no need to commit.
    NS_IF_RELEASE(m_mdbStore);
    Release();
    return err;
}

NS_IMETHODIMP nsAddrDatabase::Commit(uint32_t commitType)
{
  nsresult err = NS_OK;
  nsIMdbThumb *commitThumb = nullptr;

  if (commitType == nsAddrDBCommitType::kLargeCommit ||
      commitType == nsAddrDBCommitType::kSessionCommit)
  {
    mdb_percent outActualWaste = 0;
    mdb_bool outShould;
    if (m_mdbStore && m_mdbEnv)
    {
      // check how much space would be saved by doing a compress commit.
      // If it's more than 30%, go for it.
      // N.B. - I'm not sure this calls works in Mork for all cases.
      err = m_mdbStore->ShouldCompress(m_mdbEnv, 30, &outActualWaste, &outShould);
      if (NS_SUCCEEDED(err) && outShould)
      {
        commitType = nsAddrDBCommitType::kCompressCommit;
      }
    }
  }

  if (m_mdbStore && m_mdbEnv)
  {
    switch (commitType)
    {
      case nsAddrDBCommitType::kLargeCommit:
        err = m_mdbStore->LargeCommit(m_mdbEnv, &commitThumb);
        break;
      case nsAddrDBCommitType::kSessionCommit:
        // comment out until persistence works.
        err = m_mdbStore->SessionCommit(m_mdbEnv, &commitThumb);
        break;
      case nsAddrDBCommitType::kCompressCommit:
        err = m_mdbStore->CompressCommit(m_mdbEnv, &commitThumb);
        break;
      }
  }
  if (commitThumb && m_mdbEnv)
  {
    mdb_count outTotal = 0;    // total somethings to do in operation
    mdb_count outCurrent = 0;  // subportion of total completed so far
    mdb_bool outDone = false;      // is operation finished?
    mdb_bool outBroken = false;     // is operation irreparably dead and broken?
    while (!outDone && !outBroken && err == NS_OK)
    {
      err = commitThumb->DoMore(m_mdbEnv, &outTotal, &outCurrent, &outDone, &outBroken);
    }
    NS_RELEASE(commitThumb);
  }
  // ### do something with error, but clear it now because mork errors out on commits.
  if (m_mdbEnv)
    m_mdbEnv->ClearErrors();
  return err;
}

NS_IMETHODIMP nsAddrDatabase::Close(bool forceCommit /* = TRUE */)
{
    return CloseMDB(forceCommit);
}

// set up empty tablesetc.
nsresult nsAddrDatabase::InitNewDB()
{
  nsresult err = InitMDBInfo();
  if (NS_SUCCEEDED(err))
  {
    err = InitPabTable();
    err = InitLastRecorKey();
    Commit(nsAddrDBCommitType::kLargeCommit);
  }
  return err;
}

nsresult nsAddrDatabase::AddRowToDeletedCardsTable(nsIAbCard *card, nsIMdbRow **pCardRow)
{
  if (!m_mdbEnv)
    return NS_ERROR_NULL_POINTER;

  nsresult rv = NS_OK;
  if (!m_mdbDeletedCardsTable)
    rv = InitDeletedCardsTable(true);

  if (NS_SUCCEEDED(rv)) {
    // lets first purge old records if there are more than PURGE_CUTOFF_COUNT records
    PurgeDeletedCardTable();
    nsCOMPtr<nsIMdbRow> cardRow;
    rv = GetNewRow(getter_AddRefs(cardRow));
    if (NS_SUCCEEDED(rv) && cardRow) {
      mdb_err merror = m_mdbDeletedCardsTable->AddRow(m_mdbEnv, cardRow);
      if (merror != NS_OK) return NS_ERROR_FAILURE;
      nsString unicodeStr;
      card->GetFirstName(unicodeStr);
      AddFirstName(cardRow, NS_ConvertUTF16toUTF8(unicodeStr).get());

      card->GetLastName(unicodeStr);
      AddLastName(cardRow, NS_ConvertUTF16toUTF8(unicodeStr).get());

      card->GetDisplayName(unicodeStr);
      AddDisplayName(cardRow, NS_ConvertUTF16toUTF8(unicodeStr).get());

      card->GetPrimaryEmail(unicodeStr);
      if (!unicodeStr.IsEmpty())
        AddUnicodeToColumn(cardRow, m_PriEmailColumnToken, m_LowerPriEmailColumnToken, unicodeStr.get());

      uint32_t nowInSeconds;
      PRTime now = PR_Now();
      PRTime2Seconds(now, &nowInSeconds);
      AddIntColumn(cardRow, m_LastModDateColumnToken, nowInSeconds);

      nsString value;
      GetCardValue(card, CARD_ATTRIB_PALMID, getter_Copies(value));
      if (!value.IsEmpty())
      {
        nsCOMPtr<nsIAbCard> addedCard;
        rv = CreateCardFromDeletedCardsTable(cardRow, 0, getter_AddRefs(addedCard));
        if (NS_SUCCEEDED(rv))
          SetCardValue(addedCard, CARD_ATTRIB_PALMID, value.get(), false);
      }
      NS_IF_ADDREF(*pCardRow = cardRow);
    }
    Commit(nsAddrDBCommitType::kLargeCommit);
  }
  return rv;
}

nsresult nsAddrDatabase::DeleteRowFromDeletedCardsTable(nsIMdbRow *pCardRow)
{
  if (!m_mdbEnv)
    return NS_ERROR_NULL_POINTER;

  mdb_err merror = NS_OK;
  if (m_mdbDeletedCardsTable) {
    pCardRow->CutAllColumns(m_mdbEnv);
    merror = m_mdbDeletedCardsTable->CutRow(m_mdbEnv, pCardRow);
  }
  return merror;
}


nsresult nsAddrDatabase::InitDeletedCardsTable(bool aCreate)
{
  nsresult mdberr = NS_OK;
  if (!m_mdbDeletedCardsTable)
  {
    struct mdbOid deletedCardsTableOID;
    deletedCardsTableOID.mOid_Scope = m_CardRowScopeToken;
    deletedCardsTableOID.mOid_Id = ID_DELETEDCARDS_TABLE;
    if (m_mdbStore && m_mdbEnv)
    {
      m_mdbStore->GetTable(m_mdbEnv, &deletedCardsTableOID, &m_mdbDeletedCardsTable);
      // if deletedCardsTable does not exist and bCreate is set, create a new one
      if (!m_mdbDeletedCardsTable && aCreate)
      {
        mdberr = (nsresult) m_mdbStore->NewTableWithOid(m_mdbEnv, &deletedCardsTableOID,
                                                        m_DeletedCardsTableKind,
                                                        true, (const mdbOid*)nullptr,
                                                        &m_mdbDeletedCardsTable);
      }
    }
  }
  return mdberr;
}

nsresult nsAddrDatabase::InitPabTable()
{
  return m_mdbStore && m_mdbEnv ? m_mdbStore->NewTableWithOid(m_mdbEnv,
                                                  &gAddressBookTableOID,
                                                  m_PabTableKind,
                                                  false,
                                                  (const mdbOid*)nullptr,
                                                  &m_mdbPabTable)
      : NS_ERROR_NULL_POINTER;
}

//save the last record number, store in m_DataRowScopeToken, row 1
nsresult nsAddrDatabase::InitLastRecorKey()
{
  if (!m_mdbPabTable || !m_mdbStore || !m_mdbEnv)
    return NS_ERROR_NULL_POINTER;

  nsIMdbRow *pDataRow = nullptr;
  mdbOid dataRowOid;
  dataRowOid.mOid_Scope = m_DataRowScopeToken;
  dataRowOid.mOid_Id = DATAROW_ROWID;
  nsresult err = m_mdbStore->NewRowWithOid(m_mdbEnv, &dataRowOid, &pDataRow);

  if (NS_SUCCEEDED(err) && pDataRow)
  {
    m_LastRecordKey = 0;
    err = AddIntColumn(pDataRow, m_LastRecordKeyColumnToken, 0);
    err = m_mdbPabTable->AddRow(m_mdbEnv, pDataRow);
    NS_RELEASE(pDataRow);
  }
  return err;
}

nsresult nsAddrDatabase::GetDataRow(nsIMdbRow **pDataRow)
{
  if (!m_mdbStore || !m_mdbEnv)
    return NS_ERROR_NULL_POINTER;

  nsIMdbRow *pRow = nullptr;
  mdbOid dataRowOid;
  dataRowOid.mOid_Scope = m_DataRowScopeToken;
  dataRowOid.mOid_Id = DATAROW_ROWID;
  m_mdbStore->GetRow(m_mdbEnv, &dataRowOid, &pRow);
  *pDataRow = pRow;

  return pRow ? NS_OK : NS_ERROR_FAILURE;
}

nsresult nsAddrDatabase::GetLastRecordKey()
{
    if (!m_mdbPabTable)
        return NS_ERROR_NULL_POINTER;

    nsCOMPtr <nsIMdbRow> pDataRow;
    nsresult err = GetDataRow(getter_AddRefs(pDataRow));

    if (NS_SUCCEEDED(err) && pDataRow)
    {
        m_LastRecordKey = 0;
        err = GetIntColumn(pDataRow, m_LastRecordKeyColumnToken, &m_LastRecordKey, 0);
        if (NS_FAILED(err))
            err = NS_ERROR_NOT_AVAILABLE;
        return NS_OK;
    }

    return NS_ERROR_NOT_AVAILABLE;
}

nsresult nsAddrDatabase::UpdateLastRecordKey()
{
  if (!m_mdbPabTable || !m_mdbEnv)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr <nsIMdbRow> pDataRow;
  nsresult err = GetDataRow(getter_AddRefs(pDataRow));

  if (NS_SUCCEEDED(err) && pDataRow)
  {
    err = AddIntColumn(pDataRow, m_LastRecordKeyColumnToken, m_LastRecordKey);
    err = m_mdbPabTable->AddRow(m_mdbEnv, pDataRow);
    return NS_OK;
  }
  else if (!pDataRow)
    err = InitLastRecorKey();
  else
    return NS_ERROR_NOT_AVAILABLE;
  return err;
}

nsresult nsAddrDatabase::InitExistingDB()
{
  nsresult err = InitMDBInfo();
  if (err == NS_OK)
  {
    if (!m_mdbStore || !m_mdbEnv)
      return NS_ERROR_NULL_POINTER;

    err = m_mdbStore->GetTable(m_mdbEnv, &gAddressBookTableOID, &m_mdbPabTable);
    if (NS_SUCCEEDED(err) && m_mdbPabTable)
    {
      err = GetLastRecordKey();
      if (err == NS_ERROR_NOT_AVAILABLE)
        CheckAndUpdateRecordKey();
      UpdateLowercaseEmailListName();
    }
  }
  return err;
}

nsresult nsAddrDatabase::CheckAndUpdateRecordKey()
{
  if (!m_mdbEnv)
    return NS_ERROR_NULL_POINTER;

  nsresult err = NS_OK;
  nsIMdbTableRowCursor* rowCursor = nullptr;
  nsIMdbRow* findRow = nullptr;
  mdb_pos    rowPos = 0;

  mdb_err merror = m_mdbPabTable->GetTableRowCursor(m_mdbEnv, -1, &rowCursor);

  if (!(merror == NS_OK && rowCursor))
    return NS_ERROR_FAILURE;

  nsCOMPtr <nsIMdbRow> pDataRow;
  err = GetDataRow(getter_AddRefs(pDataRow));
  if (NS_FAILED(err))
    InitLastRecorKey();

  do
  {  //add key to each card and mailing list row
    merror = rowCursor->NextRow(m_mdbEnv, &findRow, &rowPos);
    if (merror == NS_OK && findRow)
    {
      mdbOid rowOid;

      if (findRow->GetOid(m_mdbEnv, &rowOid) == NS_OK)
      {
        if (!IsDataRowScopeToken(rowOid.mOid_Scope))
        {
          m_LastRecordKey++;
          err = AddIntColumn(findRow, m_RecordKeyColumnToken, m_LastRecordKey);
        }
      }
    }
  } while (findRow);

  UpdateLastRecordKey();
  Commit(nsAddrDBCommitType::kLargeCommit);
  return NS_OK;
}

nsresult nsAddrDatabase::UpdateLowercaseEmailListName()
{
  if (!m_mdbEnv)
    return NS_ERROR_NULL_POINTER;

  nsresult err = NS_OK;
  nsIMdbTableRowCursor* rowCursor = nullptr;
  nsIMdbRow* findRow = nullptr;
  mdb_pos    rowPos = 0;
  bool commitRequired = false;

  mdb_err merror = m_mdbPabTable->GetTableRowCursor(m_mdbEnv, -1, &rowCursor);

  if (!(merror == NS_OK && rowCursor))
    return NS_ERROR_FAILURE;

  do
  {   //add lowercase primary email to each card and mailing list row
    merror = rowCursor->NextRow(m_mdbEnv, &findRow, &rowPos);
    if (merror == NS_OK && findRow)
    {
      mdbOid rowOid;

      if (findRow->GetOid(m_mdbEnv, &rowOid) == NS_OK)
      {
        nsAutoString tempString;
        if (IsCardRowScopeToken(rowOid.mOid_Scope))
        {
          err = GetStringColumn(findRow, m_LowerPriEmailColumnToken, tempString);
          if (NS_SUCCEEDED(err))
            break;

          err = ConvertAndAddLowercaseColumn(findRow, m_PriEmailColumnToken,
            m_LowerPriEmailColumnToken);
          commitRequired = true;
        }
        else if (IsListRowScopeToken(rowOid.mOid_Scope))
        {
          err = GetStringColumn(findRow, m_LowerListNameColumnToken, tempString);
          if (NS_SUCCEEDED(err))
            break;

          err = ConvertAndAddLowercaseColumn(findRow, m_ListNameColumnToken,
            m_LowerListNameColumnToken);
          commitRequired = true;
        }
      }
      findRow->Release();
    }
  } while (findRow);

  if (findRow)
    findRow->Release();
  rowCursor->Release();
  if (commitRequired)
    Commit(nsAddrDBCommitType::kLargeCommit);
  return NS_OK;
}

/*
We store UTF8 strings in the database.  We need to convert the UTF8
string into unicode string, then convert to lower case.  Before storing
back into the database,  we need to convert the lowercase unicode string
into UTF8 string.
*/
nsresult nsAddrDatabase::ConvertAndAddLowercaseColumn
(nsIMdbRow * row, mdb_token fromCol, mdb_token toCol)
{
    nsAutoString colString;

    nsresult rv = GetStringColumn(row, fromCol, colString);
    if (!colString.IsEmpty())
    {
        rv = AddLowercaseColumn(row, toCol, NS_ConvertUTF16toUTF8(colString).get());
    }
    return rv;
}

// Change the unicode string to lowercase, then convert to UTF8 string to store in db
nsresult nsAddrDatabase::AddUnicodeToColumn(nsIMdbRow * row, mdb_token aColToken, mdb_token aLowerCaseColToken, const PRUnichar* aUnicodeStr)
{
  nsresult rv = AddCharStringColumn(row, aColToken, NS_ConvertUTF16toUTF8(aUnicodeStr).get());
  NS_ENSURE_SUCCESS(rv,rv);

  rv = AddLowercaseColumn(row, aLowerCaseColToken, NS_ConvertUTF16toUTF8(aUnicodeStr).get());
  NS_ENSURE_SUCCESS(rv,rv);
  return rv;
}

// initialize the various tokens and tables in our db's env
nsresult nsAddrDatabase::InitMDBInfo()
{
  nsresult err = NS_OK;

  if (!m_mdbTokensInitialized && m_mdbStore && m_mdbEnv)
  {
    m_mdbTokensInitialized = true;
    err = m_mdbStore->StringToToken(m_mdbEnv, kCardRowScope, &m_CardRowScopeToken);
    err = m_mdbStore->StringToToken(m_mdbEnv, kListRowScope, &m_ListRowScopeToken);
    err = m_mdbStore->StringToToken(m_mdbEnv, kDataRowScope, &m_DataRowScopeToken);
    gAddressBookTableOID.mOid_Scope = m_CardRowScopeToken;
    gAddressBookTableOID.mOid_Id = ID_PAB_TABLE;
    if (NS_SUCCEEDED(err))
    {
      m_mdbStore->StringToToken(m_mdbEnv,  kFirstNameProperty, &m_FirstNameColumnToken);
      m_mdbStore->StringToToken(m_mdbEnv,  kLastNameProperty, &m_LastNameColumnToken);
      m_mdbStore->StringToToken(m_mdbEnv,  kPhoneticFirstNameProperty, &m_PhoneticFirstNameColumnToken);
      m_mdbStore->StringToToken(m_mdbEnv,  kPhoneticLastNameProperty, &m_PhoneticLastNameColumnToken);
      m_mdbStore->StringToToken(m_mdbEnv,  kDisplayNameProperty, &m_DisplayNameColumnToken);
      m_mdbStore->StringToToken(m_mdbEnv,  kNicknameProperty, &m_NickNameColumnToken);
      m_mdbStore->StringToToken(m_mdbEnv,  kPriEmailProperty, &m_PriEmailColumnToken);
      m_mdbStore->StringToToken(m_mdbEnv,  kLowerPriEmailColumn, &m_LowerPriEmailColumnToken);
      m_mdbStore->StringToToken(m_mdbEnv,  k2ndEmailProperty, &m_2ndEmailColumnToken);
      m_mdbStore->StringToToken(m_mdbEnv,  kPreferMailFormatProperty, &m_MailFormatColumnToken);
      m_mdbStore->StringToToken(m_mdbEnv,  kPopularityIndexProperty, &m_PopularityIndexColumnToken);
      m_mdbStore->StringToToken(m_mdbEnv,  kAllowRemoteContentProperty, &m_AllowRemoteContentColumnToken);
      m_mdbStore->StringToToken(m_mdbEnv,  kWorkPhoneProperty, &m_WorkPhoneColumnToken);
      m_mdbStore->StringToToken(m_mdbEnv,  kHomePhoneProperty, &m_HomePhoneColumnToken);
      m_mdbStore->StringToToken(m_mdbEnv,  kFaxProperty, &m_FaxColumnToken);
      m_mdbStore->StringToToken(m_mdbEnv,  kPagerProperty, &m_PagerColumnToken);
      m_mdbStore->StringToToken(m_mdbEnv,  kCellularProperty, &m_CellularColumnToken);
      m_mdbStore->StringToToken(m_mdbEnv,  kWorkPhoneTypeProperty, &m_WorkPhoneTypeColumnToken);
      m_mdbStore->StringToToken(m_mdbEnv,  kHomePhoneTypeProperty, &m_HomePhoneTypeColumnToken);
      m_mdbStore->StringToToken(m_mdbEnv,  kFaxTypeProperty, &m_FaxTypeColumnToken);
      m_mdbStore->StringToToken(m_mdbEnv,  kPagerTypeProperty, &m_PagerTypeColumnToken);
      m_mdbStore->StringToToken(m_mdbEnv,  kCellularTypeProperty, &m_CellularTypeColumnToken);
      m_mdbStore->StringToToken(m_mdbEnv,  kHomeAddressProperty, &m_HomeAddressColumnToken);
      m_mdbStore->StringToToken(m_mdbEnv,  kHomeAddress2Property, &m_HomeAddress2ColumnToken);
      m_mdbStore->StringToToken(m_mdbEnv,  kHomeCityProperty, &m_HomeCityColumnToken);
      m_mdbStore->StringToToken(m_mdbEnv,  kHomeStateProperty, &m_HomeStateColumnToken);
      m_mdbStore->StringToToken(m_mdbEnv,  kHomeZipCodeProperty, &m_HomeZipCodeColumnToken);
      m_mdbStore->StringToToken(m_mdbEnv,  kHomeCountryProperty, &m_HomeCountryColumnToken);
      m_mdbStore->StringToToken(m_mdbEnv,  kWorkAddressProperty, &m_WorkAddressColumnToken);
      m_mdbStore->StringToToken(m_mdbEnv,  kWorkAddress2Property, &m_WorkAddress2ColumnToken);
      m_mdbStore->StringToToken(m_mdbEnv,  kWorkCityProperty, &m_WorkCityColumnToken);
      m_mdbStore->StringToToken(m_mdbEnv,  kWorkStateProperty, &m_WorkStateColumnToken);
      m_mdbStore->StringToToken(m_mdbEnv,  kWorkZipCodeProperty, &m_WorkZipCodeColumnToken);
      m_mdbStore->StringToToken(m_mdbEnv,  kWorkCountryProperty, &m_WorkCountryColumnToken);
      m_mdbStore->StringToToken(m_mdbEnv,  kJobTitleProperty, &m_JobTitleColumnToken);
      m_mdbStore->StringToToken(m_mdbEnv,  kDepartmentProperty, &m_DepartmentColumnToken);
      m_mdbStore->StringToToken(m_mdbEnv,  kCompanyProperty, &m_CompanyColumnToken);
      m_mdbStore->StringToToken(m_mdbEnv,  kScreenNameProperty, &m_AimScreenNameColumnToken);
      m_mdbStore->StringToToken(m_mdbEnv,  kAnniversaryYearProperty, &m_AnniversaryYearColumnToken);
      m_mdbStore->StringToToken(m_mdbEnv,  kAnniversaryMonthProperty, &m_AnniversaryMonthColumnToken);
      m_mdbStore->StringToToken(m_mdbEnv,  kAnniversaryDayProperty, &m_AnniversaryDayColumnToken);
      m_mdbStore->StringToToken(m_mdbEnv,  kSpouseNameProperty, &m_SpouseNameColumnToken);
      m_mdbStore->StringToToken(m_mdbEnv,  kFamilyNameProperty, &m_FamilyNameColumnToken);
      m_mdbStore->StringToToken(m_mdbEnv,  kWorkWebPageProperty, &m_WebPage1ColumnToken);
      m_mdbStore->StringToToken(m_mdbEnv,  kHomeWebPageProperty, &m_WebPage2ColumnToken);
      m_mdbStore->StringToToken(m_mdbEnv,  kBirthYearProperty, &m_BirthYearColumnToken);
      m_mdbStore->StringToToken(m_mdbEnv,  kBirthMonthProperty, &m_BirthMonthColumnToken);
      m_mdbStore->StringToToken(m_mdbEnv,  kBirthDayProperty, &m_BirthDayColumnToken);
      m_mdbStore->StringToToken(m_mdbEnv,  kCustom1Property, &m_Custom1ColumnToken);
      m_mdbStore->StringToToken(m_mdbEnv,  kCustom2Property, &m_Custom2ColumnToken);
      m_mdbStore->StringToToken(m_mdbEnv,  kCustom3Property, &m_Custom3ColumnToken);
      m_mdbStore->StringToToken(m_mdbEnv,  kCustom4Property, &m_Custom4ColumnToken);
      m_mdbStore->StringToToken(m_mdbEnv,  kNotesProperty, &m_NotesColumnToken);
      m_mdbStore->StringToToken(m_mdbEnv,  kLastModifiedDateProperty, &m_LastModDateColumnToken);
      m_mdbStore->StringToToken(m_mdbEnv,  kRecordKeyColumn, &m_RecordKeyColumnToken);
      m_mdbStore->StringToToken(m_mdbEnv,  kAddressCharSetColumn, &m_AddressCharSetColumnToken);
      m_mdbStore->StringToToken(m_mdbEnv,  kLastRecordKeyColumn, &m_LastRecordKeyColumnToken);

      err = m_mdbStore->StringToToken(m_mdbEnv, kPabTableKind, &m_PabTableKind);

      m_mdbStore->StringToToken(m_mdbEnv,  kMailListName, &m_ListNameColumnToken);
      m_mdbStore->StringToToken(m_mdbEnv,  kMailListNickName, &m_ListNickNameColumnToken);
      m_mdbStore->StringToToken(m_mdbEnv,  kMailListDescription, &m_ListDescriptionColumnToken);
      m_mdbStore->StringToToken(m_mdbEnv,  kMailListTotalAddresses, &m_ListTotalColumnToken);
      m_mdbStore->StringToToken(m_mdbEnv,  kLowerListNameColumn, &m_LowerListNameColumnToken);
      m_mdbStore->StringToToken(m_mdbEnv,  kDeletedCardsTableKind, &m_DeletedCardsTableKind);
    }
  }
  return err;
}

////////////////////////////////////////////////////////////////////////////////

nsresult nsAddrDatabase::AddRecordKeyColumnToRow(nsIMdbRow *pRow)
{
  if (pRow && m_mdbEnv)
  {
    m_LastRecordKey++;
    nsresult err = AddIntColumn(pRow, m_RecordKeyColumnToken, m_LastRecordKey);
    NS_ENSURE_SUCCESS(err, err);

    err = m_mdbPabTable->AddRow(m_mdbEnv, pRow);
    UpdateLastRecordKey();
    return err;
  }
  return NS_ERROR_NULL_POINTER;
}

nsresult nsAddrDatabase::AddAttributeColumnsToRow(nsIAbCard *card, nsIMdbRow *cardRow)
{
  nsresult rv = NS_OK;

  if ((!card && !cardRow) || !m_mdbEnv)
    return NS_ERROR_NULL_POINTER;

  mdbOid rowOid;
  cardRow->GetOid(m_mdbEnv, &rowOid);

  card->SetPropertyAsUint32(kRowIDProperty, rowOid.mOid_Id);

  // add the row to the singleton table.
  if (card && cardRow)
  {
    nsCOMPtr<nsISimpleEnumerator> properties;
    rv = card->GetProperties(getter_AddRefs(properties));
    NS_ENSURE_SUCCESS(rv, rv);

    bool hasMore;
    while (NS_SUCCEEDED(properties->HasMoreElements(&hasMore)) && hasMore)
    {
      nsCOMPtr<nsISupports> next;
      rv = properties->GetNext(getter_AddRefs(next));
      NS_ENSURE_SUCCESS(rv, rv);

      nsCOMPtr<nsIProperty> prop = do_QueryInterface(next);
      nsAutoString name;
      prop->GetName(name);

      nsCOMPtr<nsIVariant> variant;
      prop->GetValue(getter_AddRefs(variant));
      
      // We can't get as a char * because that messes up UTF8 stuff
      nsCAutoString value;
      variant->GetAsAUTF8String(value);

      mdb_token token;
      rv = m_mdbStore->StringToToken(m_mdbEnv, NS_ConvertUTF16toUTF8(name).get(), &token);
      NS_ENSURE_SUCCESS(rv, rv);
 
      rv = AddCharStringColumn(cardRow, token, value.get());
      NS_ENSURE_SUCCESS(rv, rv);
    }

    // Primary email is special: it is stored lowercase as well as in its
    // original format.
    nsAutoString primaryEmail;
    card->GetPrimaryEmail(primaryEmail);
    AddPrimaryEmail(cardRow, NS_ConvertUTF16toUTF8(primaryEmail).get());
  }

  return NS_OK;
}

NS_IMETHODIMP nsAddrDatabase::CreateNewCardAndAddToDB(nsIAbCard *aNewCard, bool aNotify /* = FALSE */, nsIAbDirectory *aParent)
{
  nsCOMPtr <nsIMdbRow> cardRow;

  if (!aNewCard || !m_mdbPabTable || !m_mdbEnv || !m_mdbStore)
    return NS_ERROR_NULL_POINTER;

  // Per the UUID requirements, we want to try to reuse the local id if at all
  // possible. nsACString::ToInteger probably won't fail if the local id looks
  // like "23bozo" (returning 23 instead), but it's okay since we aren't going
  // to overwrite anything with 23 if it already exists and the id for the row
  // doesn't matter otherwise.
  nsresult rv;

  nsCAutoString id;
  aNewCard->GetLocalId(id);

  mdbOid rowId;
  rowId.mOid_Scope = m_CardRowScopeToken;
  rowId.mOid_Id = id.ToInteger(&rv);
  if (NS_SUCCEEDED(rv))
  {
    // Mork is being very naughty here. If the table does not have the oid, we
    // should be able to reuse it. To be on the safe side, however, we're going
    // to reference the store's reference count.
    mdb_count rowCount = 1;
    m_mdbStore->GetRowRefCount(m_mdbEnv, &rowId, &rowCount);
    if (rowCount == 0)
    {
      // So apparently, the row can have a count of 0 yet still exist (probably
      // meaning we haven't flushed it out of memory). In this case, we need to
      // get the row and cut its cells.
      rv = m_mdbStore->GetRow(m_mdbEnv, &rowId, getter_AddRefs(cardRow));
      if (NS_SUCCEEDED(rv) && cardRow)
        cardRow->CutAllColumns(m_mdbEnv);
      else
        rv = m_mdbStore->NewRowWithOid(m_mdbEnv, &rowId, getter_AddRefs(cardRow));
    }
  }

  // If we don't have a cardRow yet, just get one with any ol' id.
  if (!cardRow)
    rv = GetNewRow(getter_AddRefs(cardRow));

  if (NS_SUCCEEDED(rv) && cardRow)
  {
    AddAttributeColumnsToRow(aNewCard, cardRow);
    AddRecordKeyColumnToRow(cardRow);

    // we need to do this for dnd
    uint32_t key = 0;
    rv = GetIntColumn(cardRow, m_RecordKeyColumnToken, &key, 0);
    if (NS_SUCCEEDED(rv))
      aNewCard->SetPropertyAsUint32(kRecordKeyColumn, key);
    
    aNewCard->GetPropertyAsAUTF8String(kRowIDProperty, id);
    aNewCard->SetLocalId(id);

    nsCOMPtr<nsIAbDirectory> abDir = do_QueryReferent(m_dbDirectory);
    if (abDir)
      abDir->GetUuid(id);

    aNewCard->SetDirectoryId(id);

    mdb_err merror = m_mdbPabTable->AddRow(m_mdbEnv, cardRow);
    if (merror != NS_OK) return NS_ERROR_FAILURE;

  }
  else
    return rv;

  //  do notification
  if (aNotify)
  {
    NotifyCardEntryChange(AB_NotifyInserted, aNewCard, aParent);
  }
  return rv;
}

NS_IMETHODIMP nsAddrDatabase::CreateNewListCardAndAddToDB(nsIAbDirectory *aList, uint32_t listRowID, nsIAbCard *newCard, bool notify /* = FALSE */)
{
  if (!newCard || !m_mdbPabTable || !m_mdbStore || !m_mdbEnv)
        return NS_ERROR_NULL_POINTER;

  nsIMdbRow* pListRow = nullptr;
  mdbOid listRowOid;
  listRowOid.mOid_Scope = m_ListRowScopeToken;
  listRowOid.mOid_Id = listRowID;
  nsresult rv = m_mdbStore->GetRow(m_mdbEnv, &listRowOid, &pListRow);
  NS_ENSURE_SUCCESS(rv,rv);

  if (!pListRow)
    return NS_OK;

  nsCOMPtr<nsIMutableArray> addressList;
  rv = aList->GetAddressLists(getter_AddRefs(addressList));
  NS_ENSURE_SUCCESS(rv,rv);

  uint32_t count;
  addressList->GetLength(&count);

  nsAutoString newEmail;
  rv = newCard->GetPrimaryEmail(newEmail);
  NS_ENSURE_SUCCESS(rv,rv);

  uint32_t i;
  for (i = 0; i < count; i++) {
    nsCOMPtr<nsIAbCard> currentCard = do_QueryElementAt(addressList, i, &rv);
    NS_ENSURE_SUCCESS(rv,rv);

    bool equals;
    rv = newCard->Equals(currentCard, &equals);
    NS_ENSURE_SUCCESS(rv,rv);

    if (equals) {
      // card is already in list, bail out.
      // this can happen when dropping a card on a mailing list from the directory that contains the mailing list
      return NS_OK;
    }

    nsAutoString currentEmail;
    rv = currentCard->GetPrimaryEmail(currentEmail);
    NS_ENSURE_SUCCESS(rv,rv);

    if (newEmail.Equals(currentEmail)) {
      // card is already in list, bail out
      // this can happen when dropping a card on a mailing list from another directory (not the one that contains the mailing list
      // or if you have multiple cards on a directory, with the same primary email address.
      return NS_OK;
    }
  }

  // start from 1
  uint32_t totalAddress = GetListAddressTotal(pListRow) + 1;
  SetListAddressTotal(pListRow, totalAddress);
  nsCOMPtr<nsIAbCard> pNewCard;
  rv = AddListCardColumnsToRow(newCard, pListRow, totalAddress, getter_AddRefs(pNewCard), true /* aInMailingList */, aList, nullptr);
  NS_ENSURE_SUCCESS(rv,rv);

  addressList->AppendElement(newCard, false);

  if (notify)
    NotifyCardEntryChange(AB_NotifyInserted, newCard, aList);

    return rv;
}

NS_IMETHODIMP nsAddrDatabase::AddListCardColumnsToRow
(nsIAbCard *aPCard, nsIMdbRow *aPListRow, uint32_t aPos, nsIAbCard** aPNewCard, bool aInMailingList, nsIAbDirectory *aParent, nsIAbDirectory *aRoot)
{
  if (!aPCard || !aPListRow || !m_mdbStore || !m_mdbEnv)
    return NS_ERROR_NULL_POINTER;

  nsresult    err = NS_OK;
  nsString email;
  aPCard->GetPrimaryEmail(email);
  if (!email.IsEmpty())
  {
    nsIMdbRow    *pCardRow = nullptr;
    // Please DO NOT change the 3rd param of GetRowFromAttribute() call to
    // true (ie, case insensitive) without reading bugs #128535 and #121478.
    err = GetRowFromAttribute(kPriEmailProperty, NS_ConvertUTF16toUTF8(email),
                              false /* retain case */, &pCardRow, nullptr);
    bool cardWasAdded = false;
    if (NS_FAILED(err) || !pCardRow)
    {
      //New Email, then add a new row with this email
      err  = GetNewRow(&pCardRow);

      if (NS_SUCCEEDED(err) && pCardRow)
      {
        AddPrimaryEmail(pCardRow, NS_ConvertUTF16toUTF8(email).get());
        err = m_mdbPabTable->AddRow(m_mdbEnv, pCardRow);
        // Create a key for this row as well.
        if (NS_SUCCEEDED(err))
          AddRecordKeyColumnToRow(pCardRow);
      }

      cardWasAdded = true;
    }

    NS_ENSURE_TRUE(pCardRow, NS_ERROR_NULL_POINTER);

    nsString name;
    aPCard->GetDisplayName(name);
    if (!name.IsEmpty()) {
      AddDisplayName(pCardRow, NS_ConvertUTF16toUTF8(name).get());
      err = m_mdbPabTable->AddRow(m_mdbEnv, pCardRow);
    }

    nsCOMPtr<nsIAbCard> newCard;
    CreateABCard(pCardRow, 0, getter_AddRefs(newCard));
    NS_IF_ADDREF(*aPNewCard = newCard);

    if (cardWasAdded) {
      NotifyCardEntryChange(AB_NotifyInserted, newCard, aParent);
      if (aRoot)
        NotifyCardEntryChange(AB_NotifyInserted, newCard, aRoot);
    }
    else if (!aInMailingList) {
      nsresult rv;
      nsCOMPtr<nsIAddrDBListener> parentListener(do_QueryInterface(aParent, &rv));

      // Ensure the parent is in the listener list (and hence wants to be notified)
      if (NS_SUCCEEDED(rv) && m_ChangeListeners.Contains(parentListener))
        parentListener->OnCardEntryChange(AB_NotifyInserted, aPCard, aParent);
    }
    else {
      NotifyCardEntryChange(AB_NotifyPropertyChanged, aPCard, aParent);
    }

    //add a column with address row id to the list row
    mdb_token listAddressColumnToken;

    char columnStr[COLUMN_STR_MAX];
    PR_snprintf(columnStr, COLUMN_STR_MAX, kMailListAddressFormat, aPos);
    m_mdbStore->StringToToken(m_mdbEnv,  columnStr, &listAddressColumnToken);

    mdbOid outOid;

    if (pCardRow->GetOid(m_mdbEnv, &outOid) == NS_OK)
    {
      //save address row ID to the list row
      err = AddIntColumn(aPListRow, listAddressColumnToken, outOid.mOid_Id);
    }
    NS_RELEASE(pCardRow);

  }

  return NS_OK;
}

nsresult nsAddrDatabase::AddListAttributeColumnsToRow(nsIAbDirectory *list, nsIMdbRow *listRow, nsIAbDirectory *aParent)
{
    nsresult    err = NS_OK;

    if ((!list && !listRow) || !m_mdbEnv)
        return NS_ERROR_NULL_POINTER;

    mdbOid rowOid, tableOid;
    m_mdbPabTable->GetOid(m_mdbEnv, &tableOid);
    listRow->GetOid(m_mdbEnv, &rowOid);

    nsCOMPtr<nsIAbMDBDirectory> dblist(do_QueryInterface(list,&err));
    if (NS_SUCCEEDED(err))
        dblist->SetDbRowID(rowOid.mOid_Id);

    // add the row to the singleton table.
    if (NS_SUCCEEDED(err) && listRow)
    {
        nsString unicodeStr;

        list->GetDirName(unicodeStr);
        if (!unicodeStr.IsEmpty())
            AddUnicodeToColumn(listRow, m_ListNameColumnToken, m_LowerListNameColumnToken, unicodeStr.get());

        list->GetListNickName(unicodeStr);
        AddListNickName(listRow, NS_ConvertUTF16toUTF8(unicodeStr).get());

        list->GetDescription(unicodeStr);
        AddListDescription(listRow, NS_ConvertUTF16toUTF8(unicodeStr).get());

        // XXX todo, this code has problems if you manually enter duplicate emails.
        nsCOMPtr<nsIMutableArray> pAddressLists;
        list->GetAddressLists(getter_AddRefs(pAddressLists));

        uint32_t count;
        pAddressLists->GetLength(&count);

        nsAutoString email;
        uint32_t i, total;
        total = 0;
        for (i = 0; i < count; i++)
        {
            nsCOMPtr<nsIAbCard> pCard(do_QueryElementAt(pAddressLists, i, &err));

            if (NS_FAILED(err))
                continue;

            pCard->GetPrimaryEmail(email);
            if (!email.IsEmpty())
                total++;
        }
        SetListAddressTotal(listRow, total);

        uint32_t pos;
        for (i = 0; i < count; i++)
        {
            nsCOMPtr<nsIAbCard> pCard(do_QueryElementAt(pAddressLists, i, &err));

            if (NS_FAILED(err))
                continue;

            bool listHasCard = false;
            err = list->HasCard(pCard, &listHasCard);

            // start from 1
            pos = i + 1;
            pCard->GetPrimaryEmail(email);
            if (!email.IsEmpty())
            {
                nsCOMPtr<nsIAbCard> pNewCard;
                err = AddListCardColumnsToRow(pCard, listRow, pos, getter_AddRefs(pNewCard), listHasCard, list, aParent);
                if (pNewCard)
                    pAddressLists->ReplaceElementAt(pNewCard, i, false);
            }
        }
    }
    return NS_OK;
}

uint32_t nsAddrDatabase::GetListAddressTotal(nsIMdbRow* listRow)
{
    uint32_t count = 0;
    GetIntColumn(listRow, m_ListTotalColumnToken, &count, 0);
    return count;
}

NS_IMETHODIMP nsAddrDatabase::SetListAddressTotal(nsIMdbRow* aListRow, uint32_t aTotal)
{
    return AddIntColumn(aListRow, m_ListTotalColumnToken, aTotal);
}

NS_IMETHODIMP nsAddrDatabase::FindRowByCard(nsIAbCard * aCard,nsIMdbRow **aRow)
{
    nsString primaryEmail;
    aCard->GetPrimaryEmail(primaryEmail);
    return GetRowForCharColumn(primaryEmail.get(), m_PriEmailColumnToken,
                               true, true, aRow, nullptr);
}

nsresult nsAddrDatabase::GetAddressRowByPos(nsIMdbRow* listRow, uint16_t pos, nsIMdbRow** cardRow)
{
  if (!m_mdbStore || !listRow || !cardRow || !m_mdbEnv)
    return NS_ERROR_NULL_POINTER;

  mdb_token listAddressColumnToken;

  char columnStr[COLUMN_STR_MAX];
  PR_snprintf(columnStr, COLUMN_STR_MAX, kMailListAddressFormat, pos);
  m_mdbStore->StringToToken(m_mdbEnv, columnStr, &listAddressColumnToken);

  nsAutoString tempString;
  mdb_id rowID;
  nsresult err = GetIntColumn(listRow, listAddressColumnToken, (uint32_t*)&rowID, 0);
  NS_ENSURE_SUCCESS(err, err);

  return GetCardRowByRowID(rowID, cardRow);
}

NS_IMETHODIMP nsAddrDatabase::CreateMailListAndAddToDB(nsIAbDirectory *aNewList, bool aNotify /* = FALSE */, nsIAbDirectory *aParent)
{
    if (!aNewList || !m_mdbPabTable || !m_mdbEnv)
        return NS_ERROR_NULL_POINTER;

    nsIMdbRow *listRow;
    nsresult err = GetNewListRow(&listRow);

    if (NS_SUCCEEDED(err) && listRow)
    {
        AddListAttributeColumnsToRow(aNewList, listRow, aParent);
        AddRecordKeyColumnToRow(listRow);
        mdb_err merror = m_mdbPabTable->AddRow(m_mdbEnv, listRow);
        if (merror != NS_OK)
          return NS_ERROR_FAILURE;

        nsCOMPtr<nsIAbCard> listCard;
        CreateABListCard(listRow, getter_AddRefs(listCard));
        NotifyCardEntryChange(AB_NotifyInserted, listCard, aParent);

        NS_RELEASE(listRow);
        return NS_OK;
    }

    return NS_ERROR_FAILURE;
}

void nsAddrDatabase::DeleteCardFromAllMailLists(mdb_id cardRowID)
{
  if (!m_mdbEnv)
    return;

    nsCOMPtr <nsIMdbTableRowCursor> rowCursor;
    m_mdbPabTable->GetTableRowCursor(m_mdbEnv, -1, getter_AddRefs(rowCursor));

    if (rowCursor)
    {
        nsCOMPtr <nsIMdbRow> pListRow;
        mdb_pos rowPos;
        do
        {
            mdb_err err = rowCursor->NextRow(m_mdbEnv, getter_AddRefs(pListRow), &rowPos);

            if (err == NS_OK && pListRow)
            {
                mdbOid rowOid;

                if (pListRow->GetOid(m_mdbEnv, &rowOid) == NS_OK)
                {
                    if (IsListRowScopeToken(rowOid.mOid_Scope))
                        DeleteCardFromListRow(pListRow, cardRowID);
                }
            }
        } while (pListRow);
    }
}

NS_IMETHODIMP nsAddrDatabase::DeleteCard(nsIAbCard *aCard, bool aNotify, nsIAbDirectory *aParent)
{
  if (!aCard || !m_mdbPabTable || !m_mdbStore || !m_mdbEnv)
    return NS_ERROR_NULL_POINTER;

  nsresult err = NS_OK;
  bool bIsMailList = false;
  aCard->GetIsMailList(&bIsMailList);

  // get the right row
  nsIMdbRow* pCardRow = nullptr;
  mdbOid rowOid;

  rowOid.mOid_Scope = bIsMailList ? m_ListRowScopeToken : m_CardRowScopeToken;

  err = aCard->GetPropertyAsUint32(kRowIDProperty, &rowOid.mOid_Id);
  NS_ENSURE_SUCCESS(err, err);

  err = m_mdbStore->GetRow(m_mdbEnv, &rowOid, &pCardRow);
  NS_ENSURE_SUCCESS(err,err);
  if (!pCardRow)
    return NS_OK;

  // Reset the directory id
  aCard->SetDirectoryId(EmptyCString());

  // Add the deleted card to the deletedcards table
  nsCOMPtr <nsIMdbRow> cardRow;
  AddRowToDeletedCardsTable(aCard, getter_AddRefs(cardRow));
  err = DeleteRow(m_mdbPabTable, pCardRow);

  //delete the person card from all mailing list
  if (!bIsMailList)
    DeleteCardFromAllMailLists(rowOid.mOid_Id);

  if (NS_SUCCEEDED(err)) {
    if (aNotify)
      NotifyCardEntryChange(AB_NotifyDeleted, aCard, aParent);
  }
  else
    DeleteRowFromDeletedCardsTable(cardRow);

  NS_RELEASE(pCardRow);
  return NS_OK;
}

nsresult nsAddrDatabase::DeleteCardFromListRow(nsIMdbRow* pListRow, mdb_id cardRowID)
{
  NS_ENSURE_ARG_POINTER(pListRow);
  if (!m_mdbStore || !m_mdbEnv)
    return NS_ERROR_NULL_POINTER;

  nsresult err = NS_OK;

  uint32_t totalAddress = GetListAddressTotal(pListRow);

  uint32_t pos;
  for (pos = 1; pos <= totalAddress; pos++)
  {
    mdb_token listAddressColumnToken;
    mdb_id rowID;

    char columnStr[COLUMN_STR_MAX];
    PR_snprintf(columnStr, COLUMN_STR_MAX, kMailListAddressFormat, pos);
    m_mdbStore->StringToToken(m_mdbEnv, columnStr, &listAddressColumnToken);

    err = GetIntColumn(pListRow, listAddressColumnToken, (uint32_t*)&rowID, 0);

    if (cardRowID == rowID)
    {
      if (pos == totalAddress)
        err = pListRow->CutColumn(m_mdbEnv, listAddressColumnToken);
      else
      {
        //replace the deleted one with the last one and delete the last one
        mdb_id lastRowID;
        mdb_token lastAddressColumnToken;
        PR_snprintf(columnStr, COLUMN_STR_MAX, kMailListAddressFormat, totalAddress);
        m_mdbStore->StringToToken(m_mdbEnv, columnStr, &lastAddressColumnToken);

        err = GetIntColumn(pListRow, lastAddressColumnToken, (uint32_t*)&lastRowID, 0);
        NS_ENSURE_SUCCESS(err, err);

        err = AddIntColumn(pListRow, listAddressColumnToken, lastRowID);
        NS_ENSURE_SUCCESS(err, err);

        err = pListRow->CutColumn(m_mdbEnv, lastAddressColumnToken);
        NS_ENSURE_SUCCESS(err, err);
      }

      // Reset total count after the card has been deleted.
      SetListAddressTotal(pListRow, totalAddress-1);
      break;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsAddrDatabase::DeleteCardFromMailList(nsIAbDirectory *mailList, nsIAbCard *card, bool aNotify)
{
  if (!card || !m_mdbPabTable || !m_mdbStore || !m_mdbEnv)
    return NS_ERROR_NULL_POINTER;

  nsresult err = NS_OK;

  // get the right row
  nsIMdbRow* pListRow = nullptr;
  mdbOid listRowOid;
  listRowOid.mOid_Scope = m_ListRowScopeToken;

  nsCOMPtr<nsIAbMDBDirectory> dbmailList(do_QueryInterface(mailList,&err));
  NS_ENSURE_SUCCESS(err, err);

  dbmailList->GetDbRowID((uint32_t*)&listRowOid.mOid_Id);

  err = m_mdbStore->GetRow(m_mdbEnv, &listRowOid, &pListRow);
  NS_ENSURE_SUCCESS(err,err);
  if (!pListRow)
    return NS_OK;

  uint32_t cardRowID;

  err = card->GetPropertyAsUint32(kRowIDProperty, &cardRowID);
  if (NS_FAILED(err))
    return NS_ERROR_NULL_POINTER;

  err = DeleteCardFromListRow(pListRow, cardRowID);
  if (NS_SUCCEEDED(err) && aNotify) {
    NotifyCardEntryChange(AB_NotifyDeleted, card, mailList);
  }
  NS_RELEASE(pListRow);
  return NS_OK;
}

NS_IMETHODIMP nsAddrDatabase::SetCardValue(nsIAbCard *card, const char *name, const PRUnichar *value, bool notify)
{
  NS_ENSURE_ARG_POINTER(card);
  NS_ENSURE_ARG_POINTER(name);
  NS_ENSURE_ARG_POINTER(value);
  if (!m_mdbStore || !m_mdbEnv)
    return NS_ERROR_NULL_POINTER;

  nsresult rv = NS_OK;

  nsCOMPtr <nsIMdbRow> cardRow;
  mdbOid rowOid;
  rowOid.mOid_Scope = m_CardRowScopeToken;

  // it might be that the caller always has a nsAbMDBCard
  rv = card->GetPropertyAsUint32(kRowIDProperty, &rowOid.mOid_Id);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = m_mdbStore->GetRow(m_mdbEnv, &rowOid, getter_AddRefs(cardRow));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!cardRow)
    return NS_OK;

  mdb_token token;
  rv = m_mdbStore->StringToToken(m_mdbEnv, name, &token);
  NS_ENSURE_SUCCESS(rv, rv);

  return AddCharStringColumn(cardRow, token, NS_ConvertUTF16toUTF8(value).get());
}

NS_IMETHODIMP nsAddrDatabase::GetCardValue(nsIAbCard *card, const char *name, PRUnichar **value)
{
  if (!m_mdbStore || !card || !name || !value || !m_mdbEnv)
    return NS_ERROR_NULL_POINTER;

  nsresult rv = NS_OK;

  nsCOMPtr <nsIMdbRow> cardRow;
  mdbOid rowOid;
  rowOid.mOid_Scope = m_CardRowScopeToken;

  // it might be that the caller always has a nsAbMDBCard
  rv = card->GetPropertyAsUint32(kRowIDProperty, &rowOid.mOid_Id);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = m_mdbStore->GetRow(m_mdbEnv, &rowOid, getter_AddRefs(cardRow));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!cardRow) {
    *value = nullptr;
    // this can happen when adding cards when editing a mailing list
    return NS_OK;
  }

  mdb_token token;
  m_mdbStore->StringToToken(m_mdbEnv, name, &token);

  // XXX fix me
  // avoid extra copying and allocations (did dmb already do this on the trunk?)
  nsAutoString tempString;
  rv = GetStringColumn(cardRow, token, tempString);
  if (NS_FAILED(rv)) {
    // not all cards are going this column
    *value = nullptr;
    return NS_OK;
  }

  *value = NS_strdup(tempString.get());
  if (!*value)
    return NS_ERROR_OUT_OF_MEMORY;
  return NS_OK;
}

NS_IMETHODIMP nsAddrDatabase::GetDeletedCardList(nsIArray **aResult)
{
  if (!m_mdbEnv || !aResult)
    return NS_ERROR_NULL_POINTER;

  *aResult = nullptr;

  nsresult rv;
  nsCOMPtr<nsIMutableArray> resultCardArray =
    do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // make sure the member is set properly
  InitDeletedCardsTable(false);
  if (m_mdbDeletedCardsTable)
  {
    nsCOMPtr<nsIMdbTableRowCursor>      rowCursor;
    mdb_pos                             rowPos;
    bool                                done = false;
    nsCOMPtr<nsIMdbRow>                 currentRow;

    m_mdbDeletedCardsTable->GetTableRowCursor(m_mdbEnv, -1, getter_AddRefs(rowCursor));
    if (!rowCursor)
        return NS_ERROR_FAILURE;
    while (!done)
    {
      rv = rowCursor->NextRow(m_mdbEnv, getter_AddRefs(currentRow), &rowPos);
      if (currentRow && NS_SUCCEEDED(rv))
      {
        mdbOid rowOid;
        if (currentRow->GetOid(m_mdbEnv, &rowOid) == NS_OK)
        {
          nsCOMPtr<nsIAbCard>    card;
          rv = CreateCardFromDeletedCardsTable(currentRow, 0, getter_AddRefs(card));
          if (NS_SUCCEEDED(rv)) {
            resultCardArray->AppendElement(card, false);
          }
        }
      }
      else
          done = true;
    }
  }

  NS_IF_ADDREF(*aResult = resultCardArray);

  return NS_OK;
}

NS_IMETHODIMP nsAddrDatabase::GetDeletedCardCount(uint32_t *aCount)
{
    // initialize count first
    *aCount = 0;
    InitDeletedCardsTable(false);
    if (m_mdbDeletedCardsTable)
      return m_mdbDeletedCardsTable->GetCount(m_mdbEnv, aCount);
    return NS_OK;
}

NS_IMETHODIMP nsAddrDatabase::PurgeDeletedCardTable()
{
  if (!m_mdbEnv)
    return NS_ERROR_NULL_POINTER;

    if (m_mdbDeletedCardsTable) {
        mdb_count cardCount=0;
        // if not too many cards let it be
        m_mdbDeletedCardsTable->GetCount(m_mdbEnv, &cardCount);
        if(cardCount < PURGE_CUTOFF_COUNT)
            return NS_OK;
        uint32_t purgeTimeInSec;
        PRTime2Seconds(PR_Now(), &purgeTimeInSec);
        purgeTimeInSec -= (182*24*60*60);  // six months in seconds
        nsCOMPtr<nsIMdbTableRowCursor> rowCursor;
        nsresult rv = m_mdbDeletedCardsTable->GetTableRowCursor(m_mdbEnv, -1, getter_AddRefs(rowCursor));
        while(NS_SUCCEEDED(rv)) {
            nsCOMPtr<nsIMdbRow> currentRow;
            mdb_pos rowPos;
            rv = rowCursor->NextRow(m_mdbEnv, getter_AddRefs(currentRow), &rowPos);
            if(currentRow) {
                uint32_t deletedTimeStamp = 0;
                GetIntColumn(currentRow, m_LastModDateColumnToken, &deletedTimeStamp, 0);
                // if record was deleted more than six months earlier, purge it
                if(deletedTimeStamp && (deletedTimeStamp < purgeTimeInSec)) {
                    if(NS_SUCCEEDED(currentRow->CutAllColumns(m_mdbEnv)))
                        m_mdbDeletedCardsTable->CutRow(m_mdbEnv, currentRow);
                }
                else
                    // since the ordering in Mork is maintained and thus
                    // the cards added later appear on the top when retrieved
                    break;
            }
            else
                break; // no more row
        }
    }

    return NS_OK;
}

NS_IMETHODIMP nsAddrDatabase::EditCard(nsIAbCard *aCard, bool aNotify, nsIAbDirectory *aParent)
{
  // XXX make sure this isn't getting called when we're just editing one or two well known fields
  if (!aCard || !m_mdbPabTable || !m_mdbStore || !m_mdbEnv)
    return NS_ERROR_NULL_POINTER;

  nsresult err = NS_OK;

  nsCOMPtr <nsIMdbRow> cardRow;
  mdbOid rowOid;
  rowOid.mOid_Scope = m_CardRowScopeToken;

  uint32_t nowInSeconds;
  PRTime now = PR_Now();
  PRTime2Seconds(now, &nowInSeconds);
  aCard->SetPropertyAsUint32(kLastModifiedDateProperty, nowInSeconds);
  err = aCard->GetPropertyAsUint32(kRowIDProperty, &rowOid.mOid_Id);
  NS_ENSURE_SUCCESS(err, err);

  err = m_mdbStore->GetRow(m_mdbEnv, &rowOid, getter_AddRefs(cardRow));
  NS_ENSURE_SUCCESS(err, err);

  if (!cardRow)
    return NS_OK;

  err = AddAttributeColumnsToRow(aCard, cardRow);
  NS_ENSURE_SUCCESS(err, err);

  if (aNotify)
    NotifyCardEntryChange(AB_NotifyPropertyChanged, aCard, aParent);

  return NS_OK;
}

NS_IMETHODIMP nsAddrDatabase::ContainsCard(nsIAbCard *card, bool *hasCard)
{
    if (!card || !m_mdbPabTable || !m_mdbEnv)
        return NS_ERROR_NULL_POINTER;

    nsresult err = NS_OK;
    mdb_bool hasOid;
    mdbOid rowOid;
    bool bIsMailList;

    card->GetIsMailList(&bIsMailList);

    if (bIsMailList)
        rowOid.mOid_Scope = m_ListRowScopeToken;
    else
        rowOid.mOid_Scope = m_CardRowScopeToken;

    err = card->GetPropertyAsUint32(kRowIDProperty, &rowOid.mOid_Id);
    NS_ENSURE_SUCCESS(err, err);

    err = m_mdbPabTable->HasOid(m_mdbEnv, &rowOid, &hasOid);
    if (NS_SUCCEEDED(err))
    {
        *hasCard = hasOid;
    }

    return err;
}

NS_IMETHODIMP nsAddrDatabase::DeleteMailList(nsIAbDirectory *aMailList,
                                             nsIAbDirectory *aParent)
{
  if (!aMailList || !m_mdbPabTable || !m_mdbStore || !m_mdbEnv)
    return NS_ERROR_NULL_POINTER;

  nsresult err = NS_OK;

  // get the row
  nsCOMPtr<nsIMdbRow> pListRow;
  mdbOid rowOid;
  rowOid.mOid_Scope = m_ListRowScopeToken;

  nsCOMPtr<nsIAbMDBDirectory> dbmailList(do_QueryInterface(aMailList, &err));
  NS_ENSURE_SUCCESS(err, err);
  dbmailList->GetDbRowID((uint32_t*)&rowOid.mOid_Id);

  err = m_mdbStore->GetRow(m_mdbEnv, &rowOid, getter_AddRefs(pListRow));
  NS_ENSURE_SUCCESS(err,err);

  if (!pListRow)
    return NS_OK;

  nsCOMPtr<nsIAbCard> card;
  err = CreateABListCard(pListRow, getter_AddRefs(card));
  NS_ENSURE_SUCCESS(err, err);

  err = DeleteRow(m_mdbPabTable, pListRow);

  if (NS_SUCCEEDED(err) && aParent)
    NotifyCardEntryChange(AB_NotifyDeleted, card, aParent);

  return err;
}

NS_IMETHODIMP nsAddrDatabase::EditMailList(nsIAbDirectory *mailList, nsIAbCard *listCard, bool notify)
{
  if (!mailList || !m_mdbPabTable || !m_mdbStore || !m_mdbEnv)
    return NS_ERROR_NULL_POINTER;

  nsresult err = NS_OK;

  nsIMdbRow* pListRow = nullptr;
  mdbOid rowOid;
  rowOid.mOid_Scope = m_ListRowScopeToken;

  nsCOMPtr<nsIAbMDBDirectory> dbmailList(do_QueryInterface(mailList, &err));
  NS_ENSURE_SUCCESS(err, err);
  dbmailList->GetDbRowID((uint32_t*)&rowOid.mOid_Id);

  err = m_mdbStore->GetRow(m_mdbEnv, &rowOid, &pListRow);
  NS_ENSURE_SUCCESS(err, err);

  if (!pListRow)
    return NS_OK;

  err = AddListAttributeColumnsToRow(mailList, pListRow, mailList);
  NS_ENSURE_SUCCESS(err, err);

  if (notify)
  {
    NotifyListEntryChange(AB_NotifyPropertyChanged, mailList);

    if (listCard)
    {
      NotifyCardEntryChange(AB_NotifyPropertyChanged, listCard, mailList);
    }
  }

  NS_RELEASE(pListRow);
  return NS_OK;
}

NS_IMETHODIMP nsAddrDatabase::ContainsMailList(nsIAbDirectory *mailList, bool *hasList)
{
    if (!mailList || !m_mdbPabTable || !m_mdbEnv)
        return NS_ERROR_NULL_POINTER;

    mdb_err err = NS_OK;
    mdb_bool hasOid;
    mdbOid rowOid;

    rowOid.mOid_Scope = m_ListRowScopeToken;

    nsCOMPtr<nsIAbMDBDirectory> dbmailList(do_QueryInterface(mailList,&err));
    NS_ENSURE_SUCCESS(err, err);
    dbmailList->GetDbRowID((uint32_t*)&rowOid.mOid_Id);

    err = m_mdbPabTable->HasOid(m_mdbEnv, &rowOid, &hasOid);
    if (err == NS_OK)
        *hasList = hasOid;

    return (err == NS_OK) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsAddrDatabase::GetNewRow(nsIMdbRow * *newRow)
{
  if (!m_mdbStore || !newRow || !m_mdbEnv)
    return NS_ERROR_NULL_POINTER;

  return m_mdbStore->NewRow(m_mdbEnv, m_CardRowScopeToken, newRow);
}

NS_IMETHODIMP nsAddrDatabase::GetNewListRow(nsIMdbRow * *newRow)
{
  if (!m_mdbStore || !newRow || !m_mdbEnv)
    return NS_ERROR_NULL_POINTER;

  return m_mdbStore->NewRow(m_mdbEnv, m_ListRowScopeToken, newRow);
}

NS_IMETHODIMP nsAddrDatabase::AddCardRowToDB(nsIMdbRow *newRow)
{
  if (m_mdbPabTable && m_mdbEnv)
  {
    if (m_mdbPabTable->AddRow(m_mdbEnv, newRow) == NS_OK)
    {
      AddRecordKeyColumnToRow(newRow);
      return NS_OK;
    }
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsAddrDatabase::AddLdifListMember(nsIMdbRow* listRow, const char* value)
{
  if (!m_mdbStore || !listRow || !value || !m_mdbEnv)
    return NS_ERROR_NULL_POINTER;

  uint32_t total = GetListAddressTotal(listRow);
  //add member
  nsCAutoString valueString(value);
  nsCAutoString email;
  int32_t emailPos = valueString.Find("mail=");
  emailPos += strlen("mail=");
  email = Substring(valueString, emailPos);
  nsCOMPtr <nsIMdbRow> cardRow;
  // Please DO NOT change the 3rd param of GetRowFromAttribute() call to
  // true (ie, case insensitive) without reading bugs #128535 and #121478.
  nsresult rv = GetRowFromAttribute(kPriEmailProperty, email, false /* retain case */,
                                    getter_AddRefs(cardRow), nullptr);
  if (NS_SUCCEEDED(rv) && cardRow)
  {
    mdbOid outOid;
    mdb_id rowID = 0;
    if (cardRow->GetOid(m_mdbEnv, &outOid) == NS_OK)
      rowID = outOid.mOid_Id;

    // start from 1
    total += 1;
    mdb_token listAddressColumnToken;
    char columnStr[COLUMN_STR_MAX];
    PR_snprintf(columnStr, COLUMN_STR_MAX, kMailListAddressFormat, total);
    m_mdbStore->StringToToken(m_mdbEnv, columnStr, &listAddressColumnToken);

    rv = AddIntColumn(listRow, listAddressColumnToken, rowID);
    NS_ENSURE_SUCCESS(rv, rv);

    SetListAddressTotal(listRow, total);
  }
  return NS_OK;
}


void nsAddrDatabase::GetCharStringYarn(char* str, struct mdbYarn* strYarn)
{
    strYarn->mYarn_Grow = nullptr;
    strYarn->mYarn_Buf = str;
    strYarn->mYarn_Size = PL_strlen((const char *) strYarn->mYarn_Buf) + 1;
    strYarn->mYarn_Fill = strYarn->mYarn_Size - 1;
    strYarn->mYarn_Form = 0;
}

void nsAddrDatabase::GetStringYarn(const nsAString & aStr, struct mdbYarn* strYarn)
{
    strYarn->mYarn_Buf = ToNewUTF8String(aStr);
    strYarn->mYarn_Size = PL_strlen((const char *) strYarn->mYarn_Buf) + 1;
    strYarn->mYarn_Fill = strYarn->mYarn_Size - 1;
    strYarn->mYarn_Form = 0;
}

void nsAddrDatabase::GetIntYarn(uint32_t nValue, struct mdbYarn* intYarn)
{
    intYarn->mYarn_Fill = intYarn->mYarn_Size;
    intYarn->mYarn_Form = 0;
    intYarn->mYarn_Grow = nullptr;

    PR_snprintf((char*)intYarn->mYarn_Buf, intYarn->mYarn_Size, "%lx", nValue);
    intYarn->mYarn_Fill = PL_strlen((const char *) intYarn->mYarn_Buf);
}

nsresult nsAddrDatabase::AddCharStringColumn(nsIMdbRow* cardRow, mdb_column inColumn, const char* str)
{
  if (!m_mdbEnv)
    return NS_ERROR_NULL_POINTER;

  struct mdbYarn yarn;

  GetCharStringYarn((char *) str, &yarn);
  mdb_err err = cardRow->AddColumn(m_mdbEnv,  inColumn, &yarn);

  return (err == NS_OK) ? NS_OK : NS_ERROR_FAILURE;
}

nsresult nsAddrDatabase::AddStringColumn(nsIMdbRow* aCardRow, mdb_column aInColumn, const nsAString & aStr)
{
  if (!m_mdbEnv)
    return NS_ERROR_NULL_POINTER;

  struct mdbYarn yarn;

  GetStringYarn(aStr, &yarn);
  mdb_err err = aCardRow->AddColumn(m_mdbEnv, aInColumn, &yarn);

  return (err == NS_OK) ? NS_OK : NS_ERROR_FAILURE;
}

nsresult nsAddrDatabase::AddIntColumn(nsIMdbRow* cardRow, mdb_column inColumn, uint32_t nValue)
{
  if (!m_mdbEnv)
    return NS_ERROR_NULL_POINTER;

  struct mdbYarn yarn;
  char    yarnBuf[100];

  yarn.mYarn_Buf = (void *) yarnBuf;
  yarn.mYarn_Size = sizeof(yarnBuf);
  GetIntYarn(nValue, &yarn);
  mdb_err err = cardRow->AddColumn(m_mdbEnv,  inColumn, &yarn);

  return (err == NS_OK) ? NS_OK : NS_ERROR_FAILURE;
}

nsresult nsAddrDatabase::AddBoolColumn(nsIMdbRow* cardRow, mdb_column inColumn, bool bValue)
{
  if (!m_mdbEnv)
    return NS_ERROR_NULL_POINTER;

  struct mdbYarn yarn;
  char    yarnBuf[100];

  yarn.mYarn_Buf = (void *) yarnBuf;
  yarn.mYarn_Size = sizeof(yarnBuf);

  GetIntYarn(bValue ? 1 : 0, &yarn);

  mdb_err err = cardRow->AddColumn(m_mdbEnv, inColumn, &yarn);

  return (err == NS_OK) ? NS_OK : NS_ERROR_FAILURE;
}

nsresult nsAddrDatabase::GetStringColumn(nsIMdbRow *cardRow, mdb_token outToken, nsString& str)
{
  nsresult    err = NS_ERROR_NULL_POINTER;
  nsIMdbCell    *cardCell;

  if (cardRow && m_mdbEnv)
  {
    err = cardRow->GetCell(m_mdbEnv, outToken, &cardCell);
    if (err == NS_OK && cardCell)
    {
      struct mdbYarn yarn;
      cardCell->AliasYarn(m_mdbEnv, &yarn);
      NS_ConvertUTF8toUTF16 uniStr((const char*) yarn.mYarn_Buf, yarn.mYarn_Fill);
      if (!uniStr.IsEmpty())
        str.Assign(uniStr);
      else
        err = NS_ERROR_FAILURE;
      cardCell->Release(); // always release ref
    }
    else
      err = NS_ERROR_FAILURE;
  }
  return err;
}

void nsAddrDatabase::YarnToUInt32(struct mdbYarn *yarn, uint32_t *pResult)
{
    uint32_t i, result, numChars;
    char *p = (char *) yarn->mYarn_Buf;
    if (yarn->mYarn_Fill > 8)
        numChars = 8;
    else
        numChars = yarn->mYarn_Fill;
    for (i=0, result = 0; i < numChars; i++, p++)
    {
        char C = *p;

        int8_t unhex = ((C >= '0' && C <= '9') ? C - '0' :
            ((C >= 'A' && C <= 'F') ? C - 'A' + 10 :
             ((C >= 'a' && C <= 'f') ? C - 'a' + 10 : -1)));
        if (unhex < 0)
            break;
        result = (result << 4) | unhex;
    }

    *pResult = result;
}

nsresult nsAddrDatabase::GetIntColumn
(nsIMdbRow *cardRow, mdb_token outToken, uint32_t* pValue, uint32_t defaultValue)
{
    nsresult    err = NS_ERROR_NULL_POINTER;
    nsIMdbCell    *cardCell;

    if (pValue)
        *pValue = defaultValue;
    if (cardRow && m_mdbEnv)
    {
        err = cardRow->GetCell(m_mdbEnv, outToken, &cardCell);
        if (err == NS_OK && cardCell)
        {
            struct mdbYarn yarn;
            cardCell->AliasYarn(m_mdbEnv, &yarn);
            YarnToUInt32(&yarn, pValue);
            cardCell->Release();
        }
        else
            err = NS_ERROR_FAILURE;
    }
    return err;
}

nsresult nsAddrDatabase::GetBoolColumn(nsIMdbRow *cardRow, mdb_token outToken, bool* pValue)
{
  NS_ENSURE_ARG_POINTER(pValue);

    nsresult    err = NS_ERROR_NULL_POINTER;
    nsIMdbCell    *cardCell;
    uint32_t nValue = 0;

    if (cardRow && m_mdbEnv)
    {
        err = cardRow->GetCell(m_mdbEnv, outToken, &cardCell);
        if (err == NS_OK && cardCell)
        {
            struct mdbYarn yarn;
            cardCell->AliasYarn(m_mdbEnv, &yarn);
            YarnToUInt32(&yarn, &nValue);
            cardCell->Release();
        }
        else
            err = NS_ERROR_FAILURE;
    }

    *pValue = nValue ? true : false;
    return err;
}

/*  value is UTF8 string */
NS_IMETHODIMP nsAddrDatabase::AddPrimaryEmail(nsIMdbRow *aRow, const char *aValue)
{
  NS_ENSURE_ARG_POINTER(aValue);

  nsresult rv = AddCharStringColumn(aRow, m_PriEmailColumnToken, aValue);
  NS_ENSURE_SUCCESS(rv,rv);

  rv = AddLowercaseColumn(aRow, m_LowerPriEmailColumnToken, aValue);
  NS_ENSURE_SUCCESS(rv,rv);
  return rv;
}

/*  value is UTF8 string */
NS_IMETHODIMP nsAddrDatabase::AddListName(nsIMdbRow *aRow, const char *aValue)
{
  NS_ENSURE_ARG_POINTER(aValue);

  nsresult rv = AddCharStringColumn(aRow, m_ListNameColumnToken, aValue);
  NS_ENSURE_SUCCESS(rv,rv);

  rv = AddLowercaseColumn(aRow, m_LowerListNameColumnToken, aValue);
  NS_ENSURE_SUCCESS(rv,rv);
  return rv;
}

/*
columnValue is UTF8 string, need to convert back to lowercase unicode then
back to UTF8 string
*/
nsresult nsAddrDatabase::AddLowercaseColumn
(nsIMdbRow * row, mdb_token columnToken, const char* columnValue)
{
  nsresult rv = NS_OK;
  if (columnValue)
  {
    NS_ConvertUTF8toUTF16 newUnicodeString(columnValue);
    ToLowerCase(newUnicodeString);
    rv = AddCharStringColumn(row, columnToken, NS_ConvertUTF16toUTF8(newUnicodeString).get());
  }
  return rv;
}

NS_IMETHODIMP nsAddrDatabase::InitCardFromRow(nsIAbCard *newCard, nsIMdbRow* cardRow)
{
  nsresult rv = NS_OK;
  if (!newCard || !cardRow || !m_mdbEnv)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIMdbRowCellCursor> cursor;
  nsCOMPtr<nsIMdbCell> cell;

  rv = cardRow->GetRowCellCursor(m_mdbEnv, -1, getter_AddRefs(cursor));
  NS_ENSURE_SUCCESS(rv, rv);

  mdb_column columnNumber;
  char columnName[100];
  struct mdbYarn colYarn = {columnName, 0, sizeof(columnName), 0, 0, nullptr};
  struct mdbYarn cellYarn;

  do
  {
    rv = cursor->NextCell(m_mdbEnv, getter_AddRefs(cell), &columnNumber, nullptr);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!cell)
      break;

    // Get the value of the cell
    cell->AliasYarn(m_mdbEnv, &cellYarn);
    NS_ConvertUTF8toUTF16 value(static_cast<const char*>(cellYarn.mYarn_Buf),
        cellYarn.mYarn_Fill);

    if (!value.IsEmpty())
    {
      // Get the column of the cell
      // Mork makes this so hard...
      rv = m_mdbStore->TokenToString(m_mdbEnv, columnNumber, &colYarn);
      NS_ENSURE_SUCCESS(rv, rv);

      char *name = PL_strndup(static_cast<char *>(colYarn.mYarn_Buf),
          colYarn.mYarn_Fill);
      newCard->SetPropertyAsAString(name, value);
      PL_strfree(name);
    }
  } while (true);
 
  uint32_t key = 0;
  rv = GetIntColumn(cardRow, m_RecordKeyColumnToken, &key, 0);
  if (NS_SUCCEEDED(rv))
    newCard->SetPropertyAsUint32(kRecordKeyColumn, key);

  return NS_OK;
}

nsresult nsAddrDatabase::GetListCardFromDB(nsIAbCard *listCard, nsIMdbRow* listRow)
{
    nsresult    err = NS_OK;
    if (!listCard || !listRow)
        return NS_ERROR_NULL_POINTER;

    nsAutoString tempString;

    err = GetStringColumn(listRow, m_ListNameColumnToken, tempString);
    if (NS_SUCCEEDED(err) && !tempString.IsEmpty())
    {
        listCard->SetDisplayName(tempString);
        listCard->SetLastName(tempString);
    }
    err = GetStringColumn(listRow, m_ListNickNameColumnToken, tempString);
    if (NS_SUCCEEDED(err) && !tempString.IsEmpty())
    {
        listCard->SetPropertyAsAString(kNicknameProperty, tempString);
    }
    err = GetStringColumn(listRow, m_ListDescriptionColumnToken, tempString);
    if (NS_SUCCEEDED(err) && !tempString.IsEmpty())
    {
        listCard->SetPropertyAsAString(kNotesProperty, tempString);
    }
    uint32_t key = 0;
    err = GetIntColumn(listRow, m_RecordKeyColumnToken, &key, 0);
    if (NS_SUCCEEDED(err))
      listCard->SetPropertyAsUint32(kRecordKeyColumn, key);
    return err;
}

nsresult nsAddrDatabase::GetListFromDB(nsIAbDirectory *newList, nsIMdbRow* listRow)
{
  nsresult    err = NS_OK;
  if (!newList || !listRow || !m_mdbStore || !m_mdbEnv)
    return NS_ERROR_NULL_POINTER;

  nsAutoString tempString;

  err = GetStringColumn(listRow, m_ListNameColumnToken, tempString);
  if (NS_SUCCEEDED(err) && !tempString.IsEmpty())
  {
    newList->SetDirName(tempString);
  }
  err = GetStringColumn(listRow, m_ListNickNameColumnToken, tempString);
  if (NS_SUCCEEDED(err) && !tempString.IsEmpty())
  {
    newList->SetListNickName(tempString);
  }
  err = GetStringColumn(listRow, m_ListDescriptionColumnToken, tempString);
  if (NS_SUCCEEDED(err) && !tempString.IsEmpty())
  {
    newList->SetDescription(tempString);
  }

  nsCOMPtr<nsIAbMDBDirectory> dbnewList(do_QueryInterface(newList, &err));
  NS_ENSURE_SUCCESS(err, err);

  uint32_t totalAddress = GetListAddressTotal(listRow);
  uint32_t pos;
  for (pos = 1; pos <= totalAddress; ++pos)
  {
    mdb_token listAddressColumnToken;
    mdb_id rowID;

    char columnStr[COLUMN_STR_MAX];
    PR_snprintf(columnStr, COLUMN_STR_MAX, kMailListAddressFormat, pos);
    m_mdbStore->StringToToken(m_mdbEnv, columnStr, &listAddressColumnToken);

    nsCOMPtr <nsIMdbRow> cardRow;
    err = GetIntColumn(listRow, listAddressColumnToken, (uint32_t*)&rowID, 0);
    NS_ENSURE_SUCCESS(err, err);
    err = GetCardRowByRowID(rowID, getter_AddRefs(cardRow));
    NS_ENSURE_SUCCESS(err, err);

    if (cardRow)
    {
      nsCOMPtr<nsIAbCard> card;
      err = CreateABCard(cardRow, 0, getter_AddRefs(card));

      if(NS_SUCCEEDED(err))
        dbnewList->AddAddressToList(card);
    }
//        NS_IF_ADDREF(card);
  }

  return err;
}

class nsAddrDBEnumerator : public nsISimpleEnumerator, public nsIAddrDBListener
{
public:
    NS_DECL_ISUPPORTS

    // nsISimpleEnumerator methods:
    NS_DECL_NSISIMPLEENUMERATOR
    NS_DECL_NSIADDRDBLISTENER
    // nsAddrDBEnumerator methods:

    nsAddrDBEnumerator(nsAddrDatabase* aDb);
    virtual ~nsAddrDBEnumerator();
    void Clear();
protected:
    nsRefPtr<nsAddrDatabase> mDb;
    nsIMdbTable *mDbTable;
    nsCOMPtr<nsIMdbTableRowCursor> mRowCursor;
    nsCOMPtr<nsIMdbRow> mCurrentRow;
    mdb_pos mRowPos;
};

nsAddrDBEnumerator::nsAddrDBEnumerator(nsAddrDatabase* aDb)
    : mDb(aDb),
      mDbTable(aDb->GetPabTable()),
      mRowPos(-1)
{
  if (aDb)
    aDb->AddListener(this);
}

nsAddrDBEnumerator::~nsAddrDBEnumerator()
{
  Clear();
}

void nsAddrDBEnumerator::Clear()
{
  mRowCursor = nullptr;
  mCurrentRow = nullptr;
  mDbTable = nullptr;
  if (mDb)
    mDb->RemoveListener(this);
}

NS_IMPL_ISUPPORTS2(nsAddrDBEnumerator, nsISimpleEnumerator, nsIAddrDBListener)

NS_IMETHODIMP
nsAddrDBEnumerator::HasMoreElements(bool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
    *aResult = false;

    if (!mDbTable || !mDb->GetEnv())
    {
        return NS_ERROR_NULL_POINTER;
    }

    nsCOMPtr<nsIMdbTableRowCursor> rowCursor;
    mDbTable->GetTableRowCursor(mDb->GetEnv(), mRowPos,
                                getter_AddRefs(rowCursor));
    NS_ENSURE_TRUE(rowCursor, NS_ERROR_FAILURE);

    mdbOid rowOid;
    rowCursor->NextRowOid(mDb->GetEnv(), &rowOid, nullptr);
    while (rowOid.mOid_Id != (mdb_id)-1)
    {
        if (mDb->IsListRowScopeToken(rowOid.mOid_Scope) ||
            mDb->IsCardRowScopeToken(rowOid.mOid_Scope))
        {
            *aResult = true;

            return NS_OK;
        }

        if (!mDb->IsDataRowScopeToken(rowOid.mOid_Scope))
        {
            return NS_ERROR_FAILURE;
        }

        rowCursor->NextRowOid(mDb->GetEnv(), &rowOid, nullptr);
    }

    return NS_OK;
}

NS_IMETHODIMP
nsAddrDBEnumerator::GetNext(nsISupports **aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

    *aResult = nullptr;

    if (!mDbTable || !mDb->GetEnv())
    {
        return NS_ERROR_NULL_POINTER;
    }

    if (!mRowCursor)
    {
         mDbTable->GetTableRowCursor(mDb->GetEnv(), -1,
                                     getter_AddRefs(mRowCursor));
         NS_ENSURE_TRUE(mRowCursor, NS_ERROR_FAILURE);
    }

    nsCOMPtr<nsIAbCard> resultCard;
    mRowCursor->NextRow(mDb->GetEnv(), getter_AddRefs(mCurrentRow), &mRowPos);
    while (mCurrentRow)
    {
        mdbOid rowOid;
        if (NS_SUCCEEDED(mCurrentRow->GetOid(mDb->GetEnv(), &rowOid)))
        {
            nsresult rv;
            if (mDb->IsListRowScopeToken(rowOid.mOid_Scope))
            {
                rv = mDb->CreateABListCard(mCurrentRow,
                                           getter_AddRefs(resultCard));
                NS_ENSURE_SUCCESS(rv, rv);
            }
            else if (mDb->IsCardRowScopeToken(rowOid.mOid_Scope))
            {
                rv = mDb->CreateABCard(mCurrentRow, 0,
                                       getter_AddRefs(resultCard));
                NS_ENSURE_SUCCESS(rv, rv);
            }
            else if (!mDb->IsDataRowScopeToken(rowOid.mOid_Scope))
            {
                return NS_ERROR_FAILURE;
            }

            if (resultCard)
            {
                return CallQueryInterface(resultCard, aResult);
            }
        }

        mRowCursor->NextRow(mDb->GetEnv(), getter_AddRefs(mCurrentRow),
                            &mRowPos);
    }

    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsAddrDBEnumerator::OnCardAttribChange(uint32_t abCode)
{
  return NS_OK;
}

/* void onCardEntryChange (in unsigned long aAbCode, in nsIAbCard aCard, in nsIAbDirectory aParent); */
NS_IMETHODIMP nsAddrDBEnumerator::OnCardEntryChange(uint32_t aAbCode, nsIAbCard *aCard, nsIAbDirectory *aParent)
{
  return NS_OK;
}

/* void onListEntryChange (in unsigned long abCode, in nsIAbDirectory list); */
NS_IMETHODIMP nsAddrDBEnumerator::OnListEntryChange(uint32_t abCode, nsIAbDirectory *list)
{
  return NS_OK;
}

/* void onAnnouncerGoingAway (); */
NS_IMETHODIMP nsAddrDBEnumerator::OnAnnouncerGoingAway()
{
  Clear();
  return NS_OK;
}

class nsListAddressEnumerator : public nsISimpleEnumerator
{
public:
    NS_DECL_ISUPPORTS

    // nsIEnumerator methods:
    NS_DECL_NSISIMPLEENUMERATOR

    // nsListAddressEnumerator methods:

    nsListAddressEnumerator(nsAddrDatabase* aDb, mdb_id aRowID);

protected:
    nsRefPtr<nsAddrDatabase> mDb;
    nsIMdbTable *mDbTable;
    nsCOMPtr<nsIMdbRow> mListRow;
    mdb_id mListRowID;
    uint32_t mAddressTotal;
    uint16_t mAddressPos;
};

nsListAddressEnumerator::nsListAddressEnumerator(nsAddrDatabase* aDb,
                                                 mdb_id aRowID)
    : mDb(aDb),
      mDbTable(aDb->GetPabTable()),
      mListRowID(aRowID),
      mAddressPos(0)
{
    mDb->GetListRowByRowID(mListRowID, getter_AddRefs(mListRow));
    mAddressTotal = aDb->GetListAddressTotal(mListRow);
}

NS_IMPL_ISUPPORTS1(nsListAddressEnumerator, nsISimpleEnumerator)

NS_IMETHODIMP
nsListAddressEnumerator::HasMoreElements(bool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  *aResult = false;

  if (!mDbTable || !mDb->GetEnv())
  {
    return NS_ERROR_NULL_POINTER;
  }

  // In some cases it is possible that GetAddressRowByPos returns success,
  // but currentRow is null. This is typically due to the fact that a card
  // has been deleted from the parent and not the list. Whilst we have fixed
  // that there are still a few dbs around there that we need to support
  // correctly. Therefore, whilst processing lists ensure that we don't return
  // false if the only thing stopping us is a blank row, just skip it and try
  // the next one.
  while (mAddressPos < mAddressTotal)
  {
    nsCOMPtr<nsIMdbRow> currentRow;
    nsresult rv = mDb->GetAddressRowByPos(mListRow, mAddressPos + 1,
                                          getter_AddRefs(currentRow));
    NS_ENSURE_SUCCESS(rv, rv);

    if (currentRow)
    {
      *aResult = true;
      break;
    }

    ++mAddressPos;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsListAddressEnumerator::GetNext(nsISupports **aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

    *aResult = nullptr;

    if (!mDbTable || !mDb->GetEnv())
    {
        return NS_ERROR_NULL_POINTER;
    }

    if (++mAddressPos <= mAddressTotal)
    {
        nsCOMPtr<nsIMdbRow> currentRow;
        nsresult rv = mDb->GetAddressRowByPos(mListRow, mAddressPos,
                                              getter_AddRefs(currentRow));
        NS_ENSURE_SUCCESS(rv, rv);

        nsCOMPtr<nsIAbCard> resultCard;
        rv = mDb->CreateABCard(currentRow, mListRowID,
                               getter_AddRefs(resultCard));
        NS_ENSURE_SUCCESS(rv, rv);

        return CallQueryInterface(resultCard, aResult);
    }

    return NS_ERROR_FAILURE;
}

////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP nsAddrDatabase::EnumerateCards(nsIAbDirectory *directory, nsISimpleEnumerator **result)
{
    nsAddrDBEnumerator* e = new nsAddrDBEnumerator(this);
    m_dbDirectory = do_GetWeakReference(directory);
    if (!e)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(e);
    *result = e;
    return NS_OK;
}

NS_IMETHODIMP nsAddrDatabase::GetMailingListsFromDB(nsIAbDirectory *parentDir)
{
    nsCOMPtr<nsIAbDirectory>    resultList;
    nsIMdbTableRowCursor*       rowCursor = nullptr;
    nsCOMPtr<nsIMdbRow> currentRow;
     mdb_pos                        rowPos;
    bool                        done = false;

  if (!m_mdbEnv)
    return NS_ERROR_NULL_POINTER;

  m_dbDirectory = do_GetWeakReference(parentDir);

    nsIMdbTable*                dbTable = GetPabTable();

    if (!dbTable)
        return NS_ERROR_FAILURE;

    dbTable->GetTableRowCursor(m_mdbEnv, -1, &rowCursor);
    if (!rowCursor)
        return NS_ERROR_FAILURE;

    while (!done)
    {
                nsresult rv = rowCursor->NextRow(m_mdbEnv, getter_AddRefs(currentRow), &rowPos);
        if (currentRow && NS_SUCCEEDED(rv))
        {
            mdbOid rowOid;

            if (currentRow->GetOid(m_mdbEnv, &rowOid) == NS_OK)
            {
                if (IsListRowScopeToken(rowOid.mOid_Scope))
                    rv = CreateABList(currentRow, getter_AddRefs(resultList));
            }
        }
        else
            done = true;
    }
        NS_IF_RELEASE(rowCursor);
    return NS_OK;
}

NS_IMETHODIMP nsAddrDatabase::EnumerateListAddresses(nsIAbDirectory *directory, nsISimpleEnumerator **result)
{
    nsresult rv = NS_OK;
    mdb_id rowID;

    nsCOMPtr<nsIAbMDBDirectory> dbdirectory(do_QueryInterface(directory,&rv));

    if(NS_SUCCEEDED(rv))
    {
    dbdirectory->GetDbRowID((uint32_t*)&rowID);

    nsListAddressEnumerator* e = new nsListAddressEnumerator(this, rowID);
    m_dbDirectory = do_GetWeakReference(directory);
    if (!e)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(e);
    *result = e;
    }
    return rv;
}

nsresult nsAddrDatabase::CreateCardFromDeletedCardsTable(nsIMdbRow* cardRow, mdb_id listRowID, nsIAbCard **result)
{
  if (!cardRow || !m_mdbEnv || !result)
    return NS_ERROR_NULL_POINTER;

    nsresult rv = NS_OK;

    mdbOid outOid;
    mdb_id rowID=0;

    if (cardRow->GetOid(m_mdbEnv, &outOid) == NS_OK)
        rowID = outOid.mOid_Id;

    if(NS_SUCCEEDED(rv))
    {
        nsCOMPtr<nsIAbCard> personCard;
        personCard = do_CreateInstance(NS_ABMDBCARD_CONTRACTID, &rv);
        NS_ENSURE_SUCCESS(rv,rv);

        InitCardFromRow(personCard, cardRow);
        personCard->SetPropertyAsUint32(kRowIDProperty, rowID);

        NS_IF_ADDREF(*result = personCard);
    }

    return rv;
}

nsresult nsAddrDatabase::CreateCard(nsIMdbRow* cardRow, mdb_id listRowID, nsIAbCard **result)
{
  if (!cardRow || !m_mdbEnv || !result)
    return NS_ERROR_NULL_POINTER;

    nsresult rv = NS_OK;

    mdbOid outOid;
    mdb_id rowID=0;

    if (cardRow->GetOid(m_mdbEnv, &outOid) == NS_OK)
        rowID = outOid.mOid_Id;

    if(NS_SUCCEEDED(rv))
    {
        nsCOMPtr<nsIAbCard> personCard;
      personCard = do_CreateInstance(NS_ABMDBCARD_CONTRACTID, &rv);
      NS_ENSURE_SUCCESS(rv,rv);

        InitCardFromRow(personCard, cardRow);
        personCard->SetPropertyAsUint32(kRowIDProperty, rowID);

        nsCAutoString id;
        id.AppendInt(rowID);
        personCard->SetLocalId(id);

        nsCOMPtr<nsIAbDirectory> abDir(do_QueryReferent(m_dbDirectory));
        if (abDir)
         abDir->GetUuid(id);

        personCard->SetDirectoryId(id);

        NS_IF_ADDREF(*result = personCard);
    }

    return rv;
}

nsresult nsAddrDatabase::CreateABCard(nsIMdbRow* cardRow, mdb_id listRowID, nsIAbCard **result)
{
    return CreateCard(cardRow, listRowID, result);
}

/* create a card for mailing list in the address book */
nsresult nsAddrDatabase::CreateABListCard(nsIMdbRow* listRow, nsIAbCard **result)
{
  if (!listRow || !m_mdbEnv || !result)
    return NS_ERROR_NULL_POINTER;

    nsresult rv = NS_OK;

    mdbOid outOid;
    mdb_id rowID=0;

    if (listRow->GetOid(m_mdbEnv, &outOid) == NS_OK)
        rowID = outOid.mOid_Id;

    char* listURI = nullptr;

    nsAutoString fileName;
    rv = m_dbName->GetLeafName(fileName);
    NS_ENSURE_SUCCESS(rv, rv);
    listURI = PR_smprintf("%s%s/MailList%ld", kMDBDirectoryRoot, NS_ConvertUTF16toUTF8(fileName).get(), rowID);

    nsCOMPtr<nsIAbCard> personCard;
    nsCOMPtr<nsIAbMDBDirectory> dbm_dbDirectory(do_QueryReferent(m_dbDirectory,
                                                                 &rv));
    if (NS_SUCCEEDED(rv) && dbm_dbDirectory)
    {
        personCard = do_CreateInstance(NS_ABMDBCARD_CONTRACTID, &rv);
        NS_ENSURE_SUCCESS(rv,rv);

        if (personCard)
        {
            GetListCardFromDB(personCard, listRow);

            personCard->SetPropertyAsUint32(kRowIDProperty, rowID);
            personCard->SetIsMailList(true);
            personCard->SetMailListURI(listURI);

            nsCAutoString id;
            id.AppendInt(rowID);
            personCard->SetLocalId(id);

            nsCOMPtr<nsIAbDirectory> abDir(do_QueryReferent(m_dbDirectory));
            if (abDir)
             abDir->GetUuid(id);
            personCard->SetDirectoryId(id);
        }

        NS_IF_ADDREF(*result = personCard);
    }
    if (listURI)
        PR_smprintf_free(listURI);

    return rv;
}

/* create a sub directory for mailing list in the address book left pane */
nsresult nsAddrDatabase::CreateABList(nsIMdbRow* listRow, nsIAbDirectory **result)
{
    nsresult rv = NS_OK;

    if (!listRow || !m_mdbEnv || !result)
        return NS_ERROR_NULL_POINTER;

    mdbOid outOid;
    mdb_id rowID=0;

    if (listRow->GetOid(m_mdbEnv, &outOid) == NS_OK)
        rowID = outOid.mOid_Id;

    char* listURI = nullptr;

    nsAutoString fileName;
    m_dbName->GetLeafName(fileName);
    NS_ENSURE_SUCCESS(rv, rv);

    listURI = PR_smprintf("%s%s/MailList%ld", kMDBDirectoryRoot, NS_ConvertUTF16toUTF8(fileName).get(), rowID);

    nsCOMPtr<nsIAbDirectory> mailList;
    nsCOMPtr<nsIAbMDBDirectory> dbm_dbDirectory(do_QueryReferent(m_dbDirectory,
                                                                 &rv));
    if (NS_SUCCEEDED(rv) && dbm_dbDirectory)
    {
        rv = dbm_dbDirectory->AddDirectory(listURI, getter_AddRefs(mailList));

        nsCOMPtr<nsIAbMDBDirectory> dbmailList (do_QueryInterface(mailList, &rv));

        if (mailList)
        {
            // if we are using turbo, and we "exit" and restart with the same profile
            // the current mailing list will still be in memory, so when we do
            // GetResource() and QI, we'll get it again.
            // in that scenario, the mailList that we pass in will already be
            // be a mailing list, with a valid row and all the entries
            // in that scenario, we can skip GetListFromDB(), which would have
            // have added all the cards to the list again.
            // see bug #134743
            mdb_id existingID;
            dbmailList->GetDbRowID(&existingID);
            if (existingID != rowID) {
              // Ensure IsMailList is set up first.
              mailList->SetIsMailList(true);
              GetListFromDB(mailList, listRow);
              dbmailList->SetDbRowID(rowID);
            }

            dbm_dbDirectory->AddMailListToDirectory(mailList);
            NS_IF_ADDREF(*result = mailList);
        }
    }

    if (listURI)
        PR_smprintf_free(listURI);

    return rv;
}

nsresult nsAddrDatabase::GetCardRowByRowID(mdb_id rowID, nsIMdbRow **dbRow)
{
  if (!m_mdbStore || !m_mdbEnv)
    return NS_ERROR_NULL_POINTER;

  mdbOid rowOid;
  rowOid.mOid_Scope = m_CardRowScopeToken;
  rowOid.mOid_Id = rowID;

  return m_mdbStore->GetRow(m_mdbEnv, &rowOid, dbRow);
}

nsresult nsAddrDatabase::GetListRowByRowID(mdb_id rowID, nsIMdbRow **dbRow)
{
  if (!m_mdbStore || !m_mdbEnv)
    return NS_ERROR_NULL_POINTER;

  mdbOid rowOid;
  rowOid.mOid_Scope = m_ListRowScopeToken;
  rowOid.mOid_Id = rowID;

  return m_mdbStore->GetRow(m_mdbEnv, &rowOid, dbRow);
}

nsresult nsAddrDatabase::GetRowFromAttribute(const char *aName,
                                             const nsACString &aUTF8Value,
                                             bool aCaseInsensitive,
                                             nsIMdbRow **aCardRow,
                                             mdb_pos *aRowPos)
{
  NS_ENSURE_ARG_POINTER(aName);
  NS_ENSURE_ARG_POINTER(aCardRow);
  if (!m_mdbStore || !m_mdbEnv)
    return NS_ERROR_NULL_POINTER;

  mdb_token token;
  m_mdbStore->StringToToken(m_mdbEnv, aName, &token);
  NS_ConvertUTF8toUTF16 newUnicodeString(aUTF8Value);

  return GetRowForCharColumn(newUnicodeString.get(), token, true,
                             aCaseInsensitive, aCardRow, aRowPos);
}

NS_IMETHODIMP nsAddrDatabase::GetCardFromAttribute(nsIAbDirectory *aDirectory,
                                                   const char *aName,
                                                   const nsACString &aUTF8Value,
                                                   bool aCaseInsensitive,
                                                   nsIAbCard **aCardResult)
{
  NS_ENSURE_ARG_POINTER(aCardResult);

  m_dbDirectory = do_GetWeakReference(aDirectory);
  nsCOMPtr<nsIMdbRow> cardRow;
  if (NS_SUCCEEDED(GetRowFromAttribute(aName, aUTF8Value, aCaseInsensitive,
                                       getter_AddRefs(cardRow), nullptr)) && cardRow)
    return CreateABCard(cardRow, 0, aCardResult);

  *aCardResult = nullptr;
  return NS_OK;
}

NS_IMETHODIMP nsAddrDatabase::GetCardsFromAttribute(nsIAbDirectory *aDirectory,
                                                    const char *aName,
                                                    const nsACString & aUTF8Value,
                                                    bool aCaseInsensitive,
                                                    nsISimpleEnumerator **cards)
{
  NS_ENSURE_ARG_POINTER(cards);

  m_dbDirectory = do_GetWeakReference(aDirectory);
  nsCOMPtr<nsIMdbRow> row;
  bool done = false;
  nsCOMArray<nsIAbCard> list;
  nsCOMPtr<nsIAbCard> card;
  mdb_pos rowPos = -1;

  do
  {
    if (NS_SUCCEEDED(GetRowFromAttribute(aName, aUTF8Value, aCaseInsensitive,
                                         getter_AddRefs(row), &rowPos)) && row)
    {
      if (NS_FAILED(CreateABCard(row, 0, getter_AddRefs(card))))
        continue;
      list.AppendObject(card);
    }
    else
      done = true;
  } while (!done);

  return NS_NewArrayEnumerator(cards, list);
}

NS_IMETHODIMP nsAddrDatabase::AddListDirNode(nsIMdbRow * listRow)
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsIAbManager> abManager(do_GetService(NS_ABMANAGER_CONTRACTID, &rv));

  if (NS_SUCCEEDED(rv))
  {
    nsAutoString parentURI;
    rv = m_dbName->GetLeafName(parentURI);
    NS_ENSURE_SUCCESS(rv, rv);

    parentURI.Replace(0, 0, NS_LITERAL_STRING(kMDBDirectoryRoot));

    nsCOMPtr<nsIAbDirectory> parentDir;
    rv = abManager->GetDirectory(NS_ConvertUTF16toUTF8(parentURI),
                                 getter_AddRefs(parentDir));
    NS_ENSURE_SUCCESS(rv, rv);

    if (parentDir)
    {
      m_dbDirectory = do_GetWeakReference(parentDir);
      nsCOMPtr<nsIAbDirectory> mailList;
      rv = CreateABList(listRow, getter_AddRefs(mailList));
      if (mailList)
      {
        nsCOMPtr<nsIAbMDBDirectory> dbparentDir(do_QueryInterface(parentDir, &rv));
        if(NS_SUCCEEDED(rv))
          dbparentDir->NotifyDirItemAdded(mailList);
      }
    }
  }
  return rv;
}

NS_IMETHODIMP nsAddrDatabase::FindMailListbyUnicodeName(const PRUnichar *listName, bool *exist)
{
  nsAutoString unicodeString(listName);
  ToLowerCase(unicodeString);

  nsCOMPtr <nsIMdbRow> listRow;
  nsresult rv = GetRowForCharColumn(unicodeString.get(),
                                    m_LowerListNameColumnToken, false,
                                    false, getter_AddRefs(listRow), nullptr);
  *exist = (NS_SUCCEEDED(rv) && listRow);
  return rv;
}

NS_IMETHODIMP nsAddrDatabase::GetCardCount(uint32_t *count)
{
    mdb_err rv;
    mdb_count c;
    rv = m_mdbPabTable->GetCount(m_mdbEnv, &c);
    if (rv == NS_OK)
        *count = c - 1;  // Don't count LastRecordKey

    return rv;
}

bool
nsAddrDatabase::HasRowButDeletedForCharColumn(const PRUnichar *unicodeStr, mdb_column findColumn, bool aIsCard, nsIMdbRow **aFindRow)
{
  if (!m_mdbStore || !aFindRow || !m_mdbEnv)
    return false;

  mdbYarn    sourceYarn;

  NS_ConvertUTF16toUTF8 UTF8String(unicodeStr);
  sourceYarn.mYarn_Buf = (void *) UTF8String.get();
  sourceYarn.mYarn_Fill = UTF8String.Length();
  sourceYarn.mYarn_Form = 0;
  sourceYarn.mYarn_Size = sourceYarn.mYarn_Fill;

  mdbOid        outRowId;
  nsresult rv;

  if (aIsCard)
  {
    rv = m_mdbStore->FindRow(m_mdbEnv, m_CardRowScopeToken,
      findColumn, &sourceYarn,  &outRowId, aFindRow);

    // no such card, so bail out early
    if (NS_FAILED(rv) || !*aFindRow)
      return false;

    // we might not have loaded the "delete cards" table yet
    // so do that (but don't create it, if we don't have one),
    // so we can see if the row is really a delete card.
    if (!m_mdbDeletedCardsTable)
      rv = InitDeletedCardsTable(false);

    // if still no deleted cards table, there are no deleted cards
    if (!m_mdbDeletedCardsTable)
      return false;

    mdb_bool hasRow = false;
    rv = m_mdbDeletedCardsTable->HasRow(m_mdbEnv, *aFindRow, &hasRow);
    return (NS_SUCCEEDED(rv) && hasRow);
  }

  rv = m_mdbStore->FindRow(m_mdbEnv, m_ListRowScopeToken,
    findColumn, &sourceYarn,  &outRowId, aFindRow);
  return (NS_SUCCEEDED(rv) && *aFindRow);
}

/* @param aRowPos Contains the row position for multiple calls. Should be
 *                instantiated to -1 on the first call. Or can be null
 *                if you are not making multiple calls.
 */
nsresult
nsAddrDatabase::GetRowForCharColumn(const PRUnichar *unicodeStr,
                                    mdb_column findColumn, bool aIsCard,
                                    bool aCaseInsensitive,
                                    nsIMdbRow **aFindRow,
                                    mdb_pos *aRowPos)
{
  NS_ENSURE_ARG_POINTER(unicodeStr);
  NS_ENSURE_ARG_POINTER(aFindRow);
  NS_ENSURE_TRUE(m_mdbEnv && m_mdbPabTable, NS_ERROR_NULL_POINTER);

  *aFindRow = nullptr;

  // see bug #198303
  // the addition of the m_mdbDeletedCardsTable table has complicated life in the addressbook
  // (it was added for palm sync).  until we fix the underlying problem, we have to jump through hoops
  // in order to know if we have a row (think card) for a given column value (think email=foo@bar.com)
  // there are 4 scenarios:
  //   1) no cards with a match
  //   2) at least one deleted card with a match, but no non-deleted cards
  //   3) at least one non-deleted card with a match, but no deleted cards
  //   4) at least one deleted card, and one non-deleted card with a match.
  //
  // if we have no cards that match (FindRow() returns nothing), we can bail early
  // but if FindRow() returns something, we have to check if it is in the deleted table
  // if not in the deleted table we can return the row (we found a non-deleted card)
  // but if so, we have to search through the table of non-deleted cards
  // for a match.  If we find one, we return it.  but if not, we report that there are no
  // non-deleted cards.  This is the expensive part.  The worse case scenario is to have
  // deleted lots of cards, and then have a lot of non-deleted cards.
  // we'd have to call FindRow(), HasRow(), and then search the list of non-deleted cards
  // each time we call GetRowForCharColumn().
  if (!aRowPos && !HasRowButDeletedForCharColumn(unicodeStr, findColumn, aIsCard, aFindRow))
  {
    // If we have a row, it's the row for the non-delete card, so return NS_OK.
    // If we don't have a row, there are two possible conditions: either the
    // card does not exist, or we are doing case-insensitive searching and the
    // value isn't lowercase.

    // Valid result, return.
    if (*aFindRow)
      return NS_OK;

    // We definitely don't have anything at this point if case-sensitive.
    if (!aCaseInsensitive)
      return NS_ERROR_FAILURE;
  }

  // check if there is a non-deleted card
  nsCOMPtr<nsIMdbTableRowCursor> rowCursor;
  mdb_pos rowPos = -1;
  bool done = false;
  nsCOMPtr<nsIMdbRow> currentRow;
  nsAutoString columnValue;

  if (aRowPos)
    rowPos = *aRowPos;

  mdb_scope targetScope = aIsCard ? m_CardRowScopeToken : m_ListRowScopeToken;

  m_mdbPabTable->GetTableRowCursor(m_mdbEnv, rowPos, getter_AddRefs(rowCursor));
  if (!rowCursor)
    return NS_ERROR_FAILURE;

  while (!done)
  {
    nsresult rv = rowCursor->NextRow(m_mdbEnv, getter_AddRefs(currentRow), &rowPos);
    if (currentRow && NS_SUCCEEDED(rv))
    {
      mdbOid rowOid;
      if ((currentRow->GetOid(m_mdbEnv, &rowOid) == NS_OK) && (rowOid.mOid_Scope == targetScope))
      {
        rv = GetStringColumn(currentRow, findColumn, columnValue);

        bool equals = aCaseInsensitive ?
          columnValue.Equals(unicodeStr, nsCaseInsensitiveStringComparator()) :
          columnValue.Equals(unicodeStr);

        if (NS_SUCCEEDED(rv) && equals)
        {
          NS_IF_ADDREF(*aFindRow = currentRow);
          if (aRowPos)
            *aRowPos = rowPos;
          return NS_OK;
        }
      }
    }
    else
      done = true;
  }
  return NS_ERROR_FAILURE;
}

nsresult nsAddrDatabase::DeleteRow(nsIMdbTable* dbTable, nsIMdbRow* dbRow)
{
  if (!m_mdbEnv)
    return NS_ERROR_NULL_POINTER;

  mdb_err err = dbRow->CutAllColumns(m_mdbEnv);
  err = dbTable->CutRow(m_mdbEnv, dbRow);

  return (err == NS_OK) ? NS_OK : NS_ERROR_FAILURE;
}
