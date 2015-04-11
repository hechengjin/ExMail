/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#define INITGUID
#define USES_IID_IMAPIProp
#define USES_IID_IMAPIContainer
#define USES_IID_IABContainer
#define USES_IID_IMAPITable
#define USES_IID_IDistList

#include "nsAbWinHelper.h"
#include "nsMapiAddressBook.h"
#include "nsWabAddressBook.h"

#include <mapiguid.h>

#include "prlog.h"

#ifdef PR_LOGGING
static PRLogModuleInfo* gAbWinHelperLog
    = PR_NewLogModule("nsAbWinHelperLog");
#endif

#define PRINTF(args) PR_LOG(gAbWinHelperLog, PR_LOG_DEBUG, args)

// Small utility to ensure release of all MAPI interfaces
template <class tInterface> struct nsMapiInterfaceWrapper
{
    tInterface mInterface ;

    nsMapiInterfaceWrapper(void) : mInterface(NULL) {}
    ~nsMapiInterfaceWrapper(void) {
        if (mInterface != NULL) { mInterface->Release() ; }
    }
    operator LPUNKNOWN *(void) { return reinterpret_cast<LPUNKNOWN *>(&mInterface) ; }
    tInterface operator -> (void) const { return mInterface ; }
    operator tInterface *(void) { return &mInterface ; }
} ;

static void assignEntryID(LPENTRYID& aTarget, LPENTRYID aSource, ULONG aByteCount)
{
    if (aTarget != NULL) {
        delete [] (reinterpret_cast<LPBYTE>(aTarget)) ;
        aTarget = NULL ;
    }
    if (aSource != NULL) {
        aTarget = reinterpret_cast<LPENTRYID>(new BYTE [aByteCount]) ;
        memcpy(aTarget, aSource, aByteCount) ;
    }
}

nsMapiEntry::nsMapiEntry(void)
: mByteCount(0), mEntryId(NULL)
{
    MOZ_COUNT_CTOR(nsMapiEntry) ;
}

nsMapiEntry::nsMapiEntry(ULONG aByteCount, LPENTRYID aEntryId)
: mByteCount(0), mEntryId(NULL)
{
    Assign(aByteCount, aEntryId) ;
    MOZ_COUNT_CTOR(nsMapiEntry) ;
}

nsMapiEntry::~nsMapiEntry(void)
{
    Assign(0, NULL) ;
    MOZ_COUNT_DTOR(nsMapiEntry) ;
}

void nsMapiEntry::Assign(ULONG aByteCount, LPENTRYID aEntryId)
{
    assignEntryID(mEntryId, aEntryId, aByteCount) ;
    mByteCount = aByteCount ;
}

static char UnsignedToChar(unsigned char aUnsigned)
{
    if (aUnsigned < 0xA) { return '0' + aUnsigned ; }
    return 'A' + aUnsigned - 0xA ;
}

static char kBase64Encoding [] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-." ;
static const int kARank = 0 ;
static const int kaRank = 26 ;
static const int k0Rank = 52 ;
static const unsigned char kMinusRank = 62 ;
static const unsigned char kDotRank = 63 ;

static void UnsignedToBase64(unsigned char *& aUnsigned, 
                             ULONG aNbUnsigned, nsCString& aString)
{
    if (aNbUnsigned > 0) {
        unsigned char remain0 = (*aUnsigned & 0x03) << 4 ;

        aString.Append(kBase64Encoding [(*aUnsigned >> 2) & 0x3F]) ;
        ++ aUnsigned ;
        if (aNbUnsigned > 1) {
            unsigned char remain1 = (*aUnsigned & 0x0F) << 2 ;

            aString.Append(kBase64Encoding [remain0 | ((*aUnsigned >> 4) & 0x0F)]) ;
            ++ aUnsigned ;
            if (aNbUnsigned > 2) {
                aString.Append(kBase64Encoding [remain1 | ((*aUnsigned >> 6) & 0x03)]) ;
                aString.Append(kBase64Encoding [*aUnsigned & 0x3F]) ;
                ++ aUnsigned ;
            }
            else {
                aString.Append(kBase64Encoding [remain1]) ;
            }
        }
        else {
            aString.Append(kBase64Encoding [remain0]) ;
        }
    }
}

static unsigned char CharToUnsigned(char aChar)
{
    if (aChar >= '0' && aChar <= '9') { return static_cast<unsigned char>(aChar) - '0' ; }
    return static_cast<unsigned char>(aChar) - 'A' + 0xA ;
}

// This function must return the rank in kBase64Encoding of the 
// character provided.
static unsigned char Base64To6Bits(char aBase64)
{
    if (aBase64 >= 'A' && aBase64 <= 'Z') { 
        return static_cast<unsigned char>(aBase64 - 'A' + kARank) ; 
    }
    if (aBase64 >= 'a' && aBase64 <= 'z') { 
        return static_cast<unsigned char>(aBase64 - 'a' + kaRank) ; 
    }
    if (aBase64 >= '0' && aBase64 <= '9') {
        return static_cast<unsigned char>(aBase64 - '0' + k0Rank) ;
    }
    if (aBase64 == '-') { return kMinusRank ; }
    if (aBase64 == '.') { return kDotRank ; }
    return 0 ;
}

static void Base64ToUnsigned(const char *& aBase64, uint32_t aNbBase64, 
                             unsigned char *&aUnsigned)
{
    // By design of the encoding, we must have at least two characters to use
    if (aNbBase64 > 1) {
        unsigned char first6Bits = Base64To6Bits(*aBase64 ++) ;
        unsigned char second6Bits = Base64To6Bits(*aBase64 ++) ;

        *aUnsigned = first6Bits << 2 ;
        *aUnsigned |= second6Bits >> 4 ;
        ++ aUnsigned ;
        if (aNbBase64 > 2) {
            unsigned char third6Bits = Base64To6Bits(*aBase64 ++) ;

            *aUnsigned = second6Bits << 4 ;
            *aUnsigned |= third6Bits >> 2 ;
            ++ aUnsigned ;
            if (aNbBase64 > 3) {
                unsigned char fourth6Bits = Base64To6Bits(*aBase64 ++) ;

                *aUnsigned = third6Bits << 6 ;
                *aUnsigned |= fourth6Bits ;
                ++ aUnsigned ;
            }
        }
    }
}

void nsMapiEntry::Assign(const nsCString& aString)
{
    Assign(0, NULL) ;
    ULONG byteCount = aString.Length() / 4 * 3 ;

    if ((aString.Length() & 0x03) != 0) {
        byteCount += (aString.Length() & 0x03) - 1 ;
    }
    const char *currentSource = aString.get() ;
    unsigned char *currentTarget = new unsigned char [byteCount] ;
    uint32_t i = 0 ;

    mByteCount = byteCount ;
    mEntryId = reinterpret_cast<LPENTRYID>(currentTarget) ;
    for (i = aString.Length() ; i >= 4 ; i -= 4) {
        Base64ToUnsigned(currentSource, 4, currentTarget) ;
    }
    Base64ToUnsigned(currentSource, i, currentTarget) ;
}

void nsMapiEntry::ToString(nsCString& aString) const
{
    aString.Truncate() ;
    ULONG i = 0 ;
    unsigned char *currentSource = reinterpret_cast<unsigned char *>(mEntryId) ;

    for (i = mByteCount ; i >= 3 ; i -= 3) {
        UnsignedToBase64(currentSource, 3, aString) ;
    }
    UnsignedToBase64(currentSource, i, aString) ;
}

void nsMapiEntry::Dump(void) const
{
    PRINTF(("%d\n", mByteCount)) ;
    for (ULONG i = 0 ; i < mByteCount ; ++ i) {
        PRINTF(("%02X", (reinterpret_cast<unsigned char *>(mEntryId)) [i])) ;
    }
    PRINTF(("\n")) ;
}

nsMapiEntryArray::nsMapiEntryArray(void)
: mEntries(NULL), mNbEntries(0)
{
    MOZ_COUNT_CTOR(nsMapiEntryArray) ;
}

nsMapiEntryArray::~nsMapiEntryArray(void)
{
    if (mEntries) { delete [] mEntries ; }
    MOZ_COUNT_DTOR(nsMapiEntryArray) ;
}

void nsMapiEntryArray::CleanUp(void)
{
    if (mEntries != NULL) { 
        delete [] mEntries ;
        mEntries = NULL ;
        mNbEntries = 0 ;
    }
}

using namespace mozilla;

uint32_t nsAbWinHelper::mEntryCounter = 0;
nsAutoPtr<mozilla::Mutex> nsAbWinHelper::mMutex;
uint32_t nsAbWinHelper::mUseCount = 0;
// There seems to be a deadlock/auto-destruction issue
// in MAPI when multiple threads perform init/release 
// operations at the same time. So I've put a mutex
// around both the initialize process and the destruction
// one. I just hope the rest of the calls don't need the 
// same protection (MAPI is supposed to be thread-safe).

nsAbWinHelper::nsAbWinHelper(void)
: mAddressBook(NULL), mLastError(S_OK)
{
  if (!mUseCount++)
    mMutex = new mozilla::Mutex("nsAbWinHelper.mMutex");

  MOZ_COUNT_CTOR(nsAbWinHelper);
}

nsAbWinHelper::~nsAbWinHelper(void)
{
  if (!--mUseCount)
    mMutex = nullptr;
  MOZ_COUNT_DTOR(nsAbWinHelper) ;
}

BOOL nsAbWinHelper::GetFolders(nsMapiEntryArray& aFolders)
{
    aFolders.CleanUp() ;
    nsMapiInterfaceWrapper<LPABCONT> rootFolder ;
    nsMapiInterfaceWrapper<LPMAPITABLE> folders ;
    ULONG objType = 0 ;
    ULONG rowCount = 0 ;
    SRestriction restriction ;
    SPropTagArray folderColumns ;

    mLastError = mAddressBook->OpenEntry(0, NULL, NULL, 0, &objType, 
                                         rootFolder) ;
    if (HR_FAILED(mLastError)) { 
        PRINTF(("Cannot open root %08x.\n", mLastError)) ;
        return FALSE ; 
    }
    mLastError = rootFolder->GetHierarchyTable(0, folders) ;
    if (HR_FAILED(mLastError)) {
        PRINTF(("Cannot get hierarchy %08x.\n", mLastError)) ;
        return FALSE ; 
    }
    // We only take into account modifiable containers, 
    // otherwise, we end up with all the directory services...
    restriction.rt = RES_BITMASK ;
    restriction.res.resBitMask.ulPropTag = PR_CONTAINER_FLAGS ;
    restriction.res.resBitMask.relBMR = BMR_NEZ ;
    restriction.res.resBitMask.ulMask = AB_MODIFIABLE ;
    mLastError = folders->Restrict(&restriction, 0) ;
    if (HR_FAILED(mLastError)) {
        PRINTF(("Cannot restrict table %08x.\n", mLastError)) ;
    }
    folderColumns.cValues = 1 ;
    folderColumns.aulPropTag [0] = PR_ENTRYID ;
    mLastError = folders->SetColumns(&folderColumns, 0) ;
    if (HR_FAILED(mLastError)) {
        PRINTF(("Cannot set columns %08x.\n", mLastError)) ;
        return FALSE ;
    }
    mLastError = folders->GetRowCount(0, &rowCount) ;
    if (HR_SUCCEEDED(mLastError)) {
        aFolders.mEntries = new nsMapiEntry [rowCount] ;
        aFolders.mNbEntries = 0 ;
        do {
            LPSRowSet rowSet = NULL ;

            rowCount = 0 ;
            mLastError = folders->QueryRows(1, 0, &rowSet) ;
            if (HR_SUCCEEDED(mLastError)) {
                rowCount = rowSet->cRows ;
                if (rowCount > 0) {
                    nsMapiEntry& current = aFolders.mEntries [aFolders.mNbEntries ++] ;
                    SPropValue& currentValue = rowSet->aRow->lpProps [0] ;
                    
                    current.Assign(currentValue.Value.bin.cb,
                                   reinterpret_cast<LPENTRYID>(currentValue.Value.bin.lpb)) ;
                }
                MyFreeProws(rowSet) ;
            }
            else {
                PRINTF(("Cannot query rows %08x.\n", mLastError)) ;
            }
        } while (rowCount > 0) ;
    }
    return HR_SUCCEEDED(mLastError) ;
}

BOOL nsAbWinHelper::GetCards(const nsMapiEntry& aParent, LPSRestriction aRestriction,
                             nsMapiEntryArray& aCards)
{
    aCards.CleanUp() ;
    return GetContents(aParent, aRestriction, &aCards.mEntries, aCards.mNbEntries, 0) ;
}
 
BOOL nsAbWinHelper::GetNodes(const nsMapiEntry& aParent, nsMapiEntryArray& aNodes)
{ 
    aNodes.CleanUp() ;
    return GetContents(aParent, NULL, &aNodes.mEntries, aNodes.mNbEntries, MAPI_DISTLIST) ;
}

BOOL nsAbWinHelper::GetCardsCount(const nsMapiEntry& aParent, ULONG& aNbCards) 
{
    aNbCards = 0 ;
    return GetContents(aParent, NULL, NULL, aNbCards, 0) ;
}

BOOL nsAbWinHelper::GetPropertyString(const nsMapiEntry& aObject,
                                      ULONG aPropertyTag, 
                                      nsCString& aName)
{
    aName.Truncate() ;
    LPSPropValue values = NULL ;
    ULONG valueCount = 0 ;

    if (!GetMAPIProperties(aObject, &aPropertyTag, 1, values, valueCount)) { return FALSE ; }
    if (valueCount == 1 && values != NULL) {
        if (PROP_TYPE(values->ulPropTag) == PT_STRING8)
            aName = values->Value.lpszA ;
        else if (PROP_TYPE(values->ulPropTag) == PT_UNICODE)
            aName = NS_LossyConvertUTF16toASCII(values->Value.lpszW);
    }
    FreeBuffer(values) ;
    return TRUE ;
}

BOOL nsAbWinHelper::GetPropertyUString(const nsMapiEntry& aObject, ULONG aPropertyTag,
                                       nsString& aName)
{
    aName.Truncate() ;
    LPSPropValue values = NULL ;
    ULONG valueCount = 0 ;

    if (!GetMAPIProperties(aObject, &aPropertyTag, 1, values, valueCount)) { return FALSE ; }
    if (valueCount == 1 && values != NULL) {
        if (PROP_TYPE(values->ulPropTag) == PT_UNICODE)
            aName = values->Value.lpszW ;
        else if (PROP_TYPE(values->ulPropTag) == PT_STRING8)
            aName.AssignASCII(values->Value.lpszA);
    }
    FreeBuffer(values) ;
    return TRUE ;
}

BOOL nsAbWinHelper::GetPropertiesUString(const nsMapiEntry& aObject, const ULONG *aPropertyTags,
                                         ULONG aNbProperties, nsString *aNames)
{
  LPSPropValue values = NULL;
  ULONG valueCount = 0;

  if (!GetMAPIProperties(aObject, aPropertyTags, aNbProperties, values,
                         valueCount))
    return FALSE;

  if (valueCount == aNbProperties && values != NULL)
  {
    for (ULONG i = 0 ; i < valueCount ; ++ i)
    {
      if (PROP_ID(values[i].ulPropTag) == PROP_ID(aPropertyTags[i]))
      {
        if (PROP_TYPE(values[i].ulPropTag) == PT_STRING8)
          aNames[i].AssignASCII(values[i].Value.lpszA);
        else if (PROP_TYPE(values[i].ulPropTag) == PT_UNICODE)
          aNames[i] = values[i].Value.lpszW;
      }
    }
    FreeBuffer(values);
  }
  return TRUE;
}

BOOL nsAbWinHelper::GetPropertyDate(const nsMapiEntry& aObject, ULONG aPropertyTag, 
                                    WORD& aYear, WORD& aMonth, WORD& aDay)
{
    aYear = 0 ; 
    aMonth = 0 ;
    aDay = 0 ;
    LPSPropValue values = NULL ;
    ULONG valueCount = 0 ;

    if (!GetMAPIProperties(aObject, &aPropertyTag, 1, values, valueCount)) { return FALSE ; }
    if (valueCount == 1 && values != NULL && PROP_TYPE(values->ulPropTag) == PT_SYSTIME) {
        SYSTEMTIME readableTime ;

        if (FileTimeToSystemTime(&values->Value.ft, &readableTime)) {
            aYear = readableTime.wYear ;
            aMonth = readableTime.wMonth ;
            aDay = readableTime.wDay ;
        }
    }
    FreeBuffer(values) ;
    return TRUE ;
}

BOOL nsAbWinHelper::GetPropertyLong(const nsMapiEntry& aObject, 
                                    ULONG aPropertyTag, 
                                    ULONG& aValue)
{
    aValue = 0 ;
    LPSPropValue values = NULL ;
    ULONG valueCount = 0 ;

    if (!GetMAPIProperties(aObject, &aPropertyTag, 1, values, valueCount)) { return FALSE ; }
    if (valueCount == 1 && values != NULL && PROP_TYPE(values->ulPropTag) == PT_LONG) {
        aValue = values->Value.ul ;
    }
    FreeBuffer(values) ;
    return TRUE ;
}

BOOL nsAbWinHelper::GetPropertyBin(const nsMapiEntry& aObject, ULONG aPropertyTag, 
                                   nsMapiEntry& aValue)
{
    aValue.Assign(0, NULL) ;
    LPSPropValue values = NULL ;
    ULONG valueCount = 0 ;

    if (!GetMAPIProperties(aObject, &aPropertyTag, 1, values, valueCount)) { return FALSE ; }
    if (valueCount == 1 && values != NULL && PROP_TYPE(values->ulPropTag) == PT_BINARY) {
        aValue.Assign(values->Value.bin.cb, 
                      reinterpret_cast<LPENTRYID>(values->Value.bin.lpb)) ;
    }
    FreeBuffer(values) ;
    return TRUE ;
}

// This function, supposedly indicating whether a particular entry was
// in a particular container, doesn't seem to work very well (has
// a tendency to return TRUE even if we're talking to different containers...).
BOOL nsAbWinHelper::TestOpenEntry(const nsMapiEntry& aContainer, const nsMapiEntry& aEntry)
{
    nsMapiInterfaceWrapper<LPMAPICONTAINER> container ;
    nsMapiInterfaceWrapper<LPMAPIPROP> subObject ;
    ULONG objType = 0 ;
    
    mLastError = mAddressBook->OpenEntry(aContainer.mByteCount, aContainer.mEntryId,
                                         &IID_IMAPIContainer, 0, &objType, 
                                         container) ;
    if (HR_FAILED(mLastError)) {
        PRINTF(("Cannot open container %08x.\n", mLastError)) ;
        return FALSE ;
    }
    mLastError = container->OpenEntry(aEntry.mByteCount, aEntry.mEntryId,
                                      NULL, 0, &objType, subObject) ;
    return HR_SUCCEEDED(mLastError) ;
}

BOOL nsAbWinHelper::DeleteEntry(const nsMapiEntry& aContainer, const nsMapiEntry& aEntry)
{
    nsMapiInterfaceWrapper<LPABCONT> container ;
    ULONG objType = 0 ;
    SBinary entry ;
    SBinaryArray entryArray ;

    mLastError = mAddressBook->OpenEntry(aContainer.mByteCount, aContainer.mEntryId,
                                         &IID_IABContainer, MAPI_MODIFY, &objType, 
                                         container) ;
    if (HR_FAILED(mLastError)) {
        PRINTF(("Cannot open container %08x.\n", mLastError)) ;
        return FALSE ;
    }
    entry.cb = aEntry.mByteCount ;
    entry.lpb = reinterpret_cast<LPBYTE>(aEntry.mEntryId) ;
    entryArray.cValues = 1 ;
    entryArray.lpbin = &entry ;
    mLastError = container->DeleteEntries(&entryArray, 0) ;
    if (HR_FAILED(mLastError)) {
        PRINTF(("Cannot delete entry %08x.\n", mLastError)) ;
        return FALSE ;
    }
    return TRUE ;
}

BOOL nsAbWinHelper::SetPropertyUString(const nsMapiEntry& aObject, ULONG aPropertyTag, 
                                       const PRUnichar *aValue)
{
    SPropValue value ;
    nsCAutoString alternativeValue ;

    value.ulPropTag = aPropertyTag ;
    if (PROP_TYPE(aPropertyTag) == PT_UNICODE) {
        value.Value.lpszW = const_cast<WCHAR *>(aValue) ;
    }
    else if (PROP_TYPE(aPropertyTag) == PT_STRING8) {
        alternativeValue = NS_LossyConvertUTF16toASCII(aValue);
        value.Value.lpszA = const_cast<char *>(alternativeValue.get()) ;
    }
    else {
        PRINTF(("Property %08x is not a string.\n", aPropertyTag)) ;
        return TRUE ;
    }
    return SetMAPIProperties(aObject, 1, &value) ;
}

BOOL nsAbWinHelper::SetPropertiesUString(const nsMapiEntry& aObject, const ULONG *aPropertiesTag,
                                         ULONG aNbProperties, nsString *aValues) 
{
    LPSPropValue values = new SPropValue [aNbProperties] ;
    if (!values)
        return FALSE ;

    ULONG i = 0 ;
    ULONG currentValue = 0 ;
    nsCAutoString alternativeValue ;
    BOOL retCode = TRUE ;

    for (i = 0 ; i < aNbProperties ; ++ i) {
        values [currentValue].ulPropTag = aPropertiesTag [i] ;
        if (PROP_TYPE(aPropertiesTag [i]) == PT_UNICODE) {
            values [currentValue ++].Value.lpszW = const_cast<WCHAR *>(aValues [i].get()) ;
        }
        else if (PROP_TYPE(aPropertiesTag [i]) == PT_STRING8) {
            LossyCopyUTF16toASCII(aValues [i], alternativeValue);
            char *av = strdup(alternativeValue.get()) ;
            if (!av) {
                retCode = FALSE ;
                break ;
            }
            values [currentValue ++].Value.lpszA = av ;
        }
    }
    if (retCode)
        retCode = SetMAPIProperties(aObject, currentValue, values) ;
    for (i = 0 ; i < currentValue ; ++ i) {
        if (PROP_TYPE(aPropertiesTag [i]) == PT_STRING8) {
            NS_Free(values [i].Value.lpszA) ;
        }
    }
    delete [] values ;
    return retCode ;
}

BOOL nsAbWinHelper::SetPropertyDate(const nsMapiEntry& aObject, ULONG aPropertyTag, 
                                    WORD aYear, WORD aMonth, WORD aDay)
{
    SPropValue value ;
    
    value.ulPropTag = aPropertyTag ;
    if (PROP_TYPE(aPropertyTag) == PT_SYSTIME) {
        SYSTEMTIME readableTime ;

        readableTime.wYear = aYear ;
        readableTime.wMonth = aMonth ;
        readableTime.wDay = aDay ;
        readableTime.wDayOfWeek = 0 ;
        readableTime.wHour = 0 ;
        readableTime.wMinute = 0 ;
        readableTime.wSecond = 0 ;
        readableTime.wMilliseconds = 0 ;
        if (SystemTimeToFileTime(&readableTime, &value.Value.ft)) {
            return SetMAPIProperties(aObject, 1, &value) ;
        }
        return TRUE ;
    }
    return FALSE ;
}

BOOL nsAbWinHelper::CreateEntry(const nsMapiEntry& aParent, nsMapiEntry& aNewEntry)
{
    nsMapiInterfaceWrapper<LPABCONT> container ;
    ULONG objType = 0 ;

    mLastError = mAddressBook->OpenEntry(aParent.mByteCount, aParent.mEntryId,
                                         &IID_IABContainer, MAPI_MODIFY, &objType,
                                         container) ;
    if (HR_FAILED(mLastError)) { 
        PRINTF(("Cannot open container %08x.\n", mLastError)) ;
        return FALSE ;
    }
    SPropTagArray property ;
    LPSPropValue value = NULL ;
    ULONG valueCount = 0 ;

    property.cValues = 1 ;
    property.aulPropTag [0] = PR_DEF_CREATE_MAILUSER ;
    mLastError = container->GetProps(&property, 0, &valueCount, &value) ;
    if (HR_FAILED(mLastError) || valueCount != 1) {
        PRINTF(("Cannot obtain template %08x.\n", mLastError)) ;
        return FALSE ;
    }
    nsMapiInterfaceWrapper<LPMAPIPROP> newEntry ;

    mLastError = container->CreateEntry(value->Value.bin.cb, 
                                        reinterpret_cast<LPENTRYID>(value->Value.bin.lpb),
                                        CREATE_CHECK_DUP_LOOSE,
                                        newEntry) ;
    FreeBuffer(value) ;
    if (HR_FAILED(mLastError)) {
        PRINTF(("Cannot create new entry %08x.\n", mLastError)) ;
        return FALSE ;
    }
    SPropValue displayName ;
    LPSPropProblemArray problems = NULL ;
    nsAutoString tempName ;

    displayName.ulPropTag = PR_DISPLAY_NAME_W ;
    tempName.AssignLiteral("__MailUser__") ;
    tempName.AppendInt(mEntryCounter ++) ;
    displayName.Value.lpszW = const_cast<WCHAR *>(tempName.get()) ;
    mLastError = newEntry->SetProps(1, &displayName, &problems) ;
    if (HR_FAILED(mLastError)) {
        PRINTF(("Cannot set temporary name %08x.\n", mLastError)) ;
        return FALSE ;
    }
    mLastError = newEntry->SaveChanges(KEEP_OPEN_READONLY) ;
    if (HR_FAILED(mLastError)) {
        PRINTF(("Cannot commit new entry %08x.\n", mLastError)) ;
        return FALSE ;
    }
    property.aulPropTag [0] = PR_ENTRYID ;
    mLastError = newEntry->GetProps(&property, 0, &valueCount, &value) ;
    if (HR_FAILED(mLastError) || valueCount != 1) {
        PRINTF(("Cannot get entry id %08x.\n", mLastError)) ;
        return FALSE ;
    }
    aNewEntry.Assign(value->Value.bin.cb, reinterpret_cast<LPENTRYID>(value->Value.bin.lpb)) ;
    FreeBuffer(value) ;
    return TRUE ;
}

BOOL nsAbWinHelper::CreateDistList(const nsMapiEntry& aParent, nsMapiEntry& aNewEntry)
{
    nsMapiInterfaceWrapper<LPABCONT> container ;
    ULONG objType = 0 ;

    mLastError = mAddressBook->OpenEntry(aParent.mByteCount, aParent.mEntryId,
                                         &IID_IABContainer, MAPI_MODIFY, &objType,
                                         container) ;
    if (HR_FAILED(mLastError)) {
        PRINTF(("Cannot open container %08x.\n", mLastError)) ;
        return FALSE ;
    }
    SPropTagArray property ;
    LPSPropValue value = NULL ;
    ULONG valueCount = 0 ;

    property.cValues = 1 ;
    property.aulPropTag [0] = PR_DEF_CREATE_DL ;
    mLastError = container->GetProps(&property, 0, &valueCount, &value) ;
    if (HR_FAILED(mLastError) || valueCount != 1) {
        PRINTF(("Cannot obtain template %08x.\n", mLastError)) ;
        return FALSE ;
    }
    nsMapiInterfaceWrapper<LPMAPIPROP> newEntry ;

    mLastError = container->CreateEntry(value->Value.bin.cb, 
                                        reinterpret_cast<LPENTRYID>(value->Value.bin.lpb),
                                        CREATE_CHECK_DUP_LOOSE,
                                        newEntry) ;
    FreeBuffer(value) ;
    if (HR_FAILED(mLastError)) {
        PRINTF(("Cannot create new entry %08x.\n", mLastError)) ;
        return FALSE ;
    }
    SPropValue displayName ;
    LPSPropProblemArray problems = NULL ;
    nsAutoString tempName ;

    displayName.ulPropTag = PR_DISPLAY_NAME_W ;
    tempName.AssignLiteral("__MailList__") ;
    tempName.AppendInt(mEntryCounter ++) ;
    displayName.Value.lpszW = const_cast<WCHAR *>(tempName.get()) ;
    mLastError = newEntry->SetProps(1, &displayName, &problems) ;
    if (HR_FAILED(mLastError)) {
        PRINTF(("Cannot set temporary name %08x.\n", mLastError)) ;
        return FALSE ;
    }
    mLastError = newEntry->SaveChanges(KEEP_OPEN_READONLY) ;
    if (HR_FAILED(mLastError)) {
        PRINTF(("Cannot commit new entry %08x.\n", mLastError)) ;
        return FALSE ;
    }
    property.aulPropTag [0] = PR_ENTRYID ;
    mLastError = newEntry->GetProps(&property, 0, &valueCount, &value) ;
    if (HR_FAILED(mLastError) || valueCount != 1) {
        PRINTF(("Cannot get entry id %08x.\n", mLastError)) ;
        return FALSE ;
    }
    aNewEntry.Assign(value->Value.bin.cb, 
                     reinterpret_cast<LPENTRYID>(value->Value.bin.lpb)) ;
    FreeBuffer(value) ;
    return TRUE ;
}

BOOL nsAbWinHelper::CopyEntry(const nsMapiEntry& aContainer, const nsMapiEntry& aSource,
                              nsMapiEntry& aTarget)
{
    nsMapiInterfaceWrapper<LPABCONT> container ;
    ULONG objType = 0 ;

    mLastError = mAddressBook->OpenEntry(aContainer.mByteCount, aContainer.mEntryId,
                                         &IID_IABContainer, MAPI_MODIFY, &objType,
                                         container) ;
    if (HR_FAILED(mLastError)) { 
        PRINTF(("Cannot open container %08x.\n", mLastError)) ;
        return FALSE ;
    }
    nsMapiInterfaceWrapper<LPMAPIPROP> newEntry ;

    mLastError = container->CreateEntry(aSource.mByteCount, aSource.mEntryId,
                                        CREATE_CHECK_DUP_LOOSE, newEntry) ;
    if (HR_FAILED(mLastError)) {
        PRINTF(("Cannot create new entry %08x.\n", mLastError)) ;
        return FALSE ;
    }
    mLastError = newEntry->SaveChanges(KEEP_OPEN_READONLY) ;
    if (HR_FAILED(mLastError)) {
        PRINTF(("Cannot commit new entry %08x.\n", mLastError)) ;
        return FALSE ;
    }
    SPropTagArray property ;
    LPSPropValue value = NULL ;
    ULONG valueCount = 0 ;
    
    property.cValues = 1 ;
    property.aulPropTag [0] = PR_ENTRYID ;
    mLastError = newEntry->GetProps(&property, 0, &valueCount, &value) ;
    if (HR_FAILED(mLastError) || valueCount != 1) {
        PRINTF(("Cannot get entry id %08x.\n", mLastError)) ;
        return FALSE ;
    }
    aTarget.Assign(value->Value.bin.cb, 
                   reinterpret_cast<LPENTRYID>(value->Value.bin.lpb)) ;
    FreeBuffer(value) ;
    return TRUE ;
}

BOOL nsAbWinHelper::GetDefaultContainer(nsMapiEntry& aContainer)
{
    LPENTRYID entryId = NULL ; 
    ULONG byteCount = 0 ;

    mLastError = mAddressBook->GetPAB(&byteCount, &entryId) ;
    if (HR_FAILED(mLastError)) {
        PRINTF(("Cannot get PAB %08x.\n", mLastError)) ;
        return FALSE ;
    }
    aContainer.Assign(byteCount, entryId) ;
    FreeBuffer(entryId) ;
    return TRUE ;
}

enum
{
    ContentsColumnEntryId = 0,
    ContentsColumnObjectType,
    ContentsColumnsSize
} ;

static const SizedSPropTagArray(ContentsColumnsSize, ContentsColumns) =
{
    ContentsColumnsSize,
    {
        PR_ENTRYID,
        PR_OBJECT_TYPE
    }
} ;

BOOL nsAbWinHelper::GetContents(const nsMapiEntry& aParent, LPSRestriction aRestriction,
                                nsMapiEntry **aList, ULONG& aNbElements, ULONG aMapiType)
{
    if (aList != NULL) { *aList = NULL ; }
    aNbElements = 0 ;
    nsMapiInterfaceWrapper<LPMAPICONTAINER> parent ;
    nsMapiInterfaceWrapper<LPMAPITABLE> contents ;
    ULONG objType = 0 ;
    ULONG rowCount = 0 ;

    mLastError = mAddressBook->OpenEntry(aParent.mByteCount, aParent.mEntryId, 
                                         &IID_IMAPIContainer, 0, &objType, 
                                         parent) ;
    if (HR_FAILED(mLastError)) {
        PRINTF(("Cannot open parent %08x.\n", mLastError)) ;
        return FALSE ; 
    }
    // Here, flags for WAB and MAPI could be different, so this works
    // only as long as we don't want to use any flag in GetContentsTable
    mLastError = parent->GetContentsTable(0, contents) ;
    if (HR_FAILED(mLastError)) {
        PRINTF(("Cannot get contents %08x.\n", mLastError)) ;
        return FALSE ; 
    }
    if (aRestriction != NULL) {
        mLastError = contents->Restrict(aRestriction, 0) ;
        if (HR_FAILED(mLastError)) {
            PRINTF(("Cannot set restriction %08x.\n", mLastError)) ;
            return FALSE ;
        }
    }
    mLastError = contents->SetColumns((LPSPropTagArray) &ContentsColumns, 0) ;
    if (HR_FAILED(mLastError)) {
        PRINTF(("Cannot set columns %08x.\n", mLastError)) ;
        return FALSE ;
    }
    mLastError = contents->GetRowCount(0, &rowCount) ;
    if (HR_FAILED(mLastError)) {
        PRINTF(("Cannot get result count %08x.\n", mLastError)) ;
        return FALSE ;
    }
    if (aList != NULL) { *aList = new nsMapiEntry [rowCount] ; }
    aNbElements = 0 ;
    do {
        LPSRowSet rowSet = NULL ;
        
        rowCount = 0 ;
        mLastError = contents->QueryRows(1, 0, &rowSet) ;
        if (HR_FAILED(mLastError)) {
            PRINTF(("Cannot query rows %08x.\n", mLastError)) ;
            return FALSE ;
        }
        rowCount = rowSet->cRows ;
        if (rowCount > 0 &&
            (aMapiType == 0 ||
            rowSet->aRow->lpProps[ContentsColumnObjectType].Value.ul == aMapiType)) {
            if (aList != NULL) {
                nsMapiEntry& current = (*aList) [aNbElements] ;
                SPropValue& currentValue = rowSet->aRow->lpProps[ContentsColumnEntryId] ;
                
                current.Assign(currentValue.Value.bin.cb,
                    reinterpret_cast<LPENTRYID>(currentValue.Value.bin.lpb)) ;
            }
            ++ aNbElements ;
        }
        MyFreeProws(rowSet) ;
    } while (rowCount > 0) ;
    return TRUE ;
}

BOOL nsAbWinHelper::GetMAPIProperties(const nsMapiEntry& aObject, const ULONG *aPropertyTags, 
                                      ULONG aNbProperties, LPSPropValue& aValue, 
                                      ULONG& aValueCount)
{
    nsMapiInterfaceWrapper<LPMAPIPROP> object ;
    ULONG objType = 0 ;
    LPSPropTagArray properties = NULL ;
    ULONG i = 0 ;
    
    mLastError = mAddressBook->OpenEntry(aObject.mByteCount, aObject.mEntryId,
                                         &IID_IMAPIProp, 0, &objType, 
                                         object) ;
    if (HR_FAILED(mLastError)) { 
        PRINTF(("Cannot open entry %08x.\n", mLastError)) ;
        return FALSE ; 
    }
    AllocateBuffer(CbNewSPropTagArray(aNbProperties), 
                   reinterpret_cast<void **>(&properties)) ;
    properties->cValues = aNbProperties ;
    for (i = 0 ; i < aNbProperties ; ++ i) {
        properties->aulPropTag [i] = aPropertyTags [i] ;
    }
    mLastError = object->GetProps(properties, 0, &aValueCount, &aValue) ;
    FreeBuffer(properties) ;
    if (HR_FAILED(mLastError)) {
        PRINTF(("Cannot get props %08x.\n", mLastError)) ;
    }
    return HR_SUCCEEDED(mLastError) ;
}

BOOL nsAbWinHelper::SetMAPIProperties(const nsMapiEntry& aObject, ULONG aNbProperties, 
                                      const LPSPropValue& aValues)
{
    nsMapiInterfaceWrapper<LPMAPIPROP> object ;
    ULONG objType = 0 ;
    LPSPropProblemArray problems = NULL ;

    mLastError = mAddressBook->OpenEntry(aObject.mByteCount, aObject.mEntryId,
                                         &IID_IMAPIProp, MAPI_MODIFY, &objType, 
                                         object) ;
    if (HR_FAILED(mLastError)) {
        PRINTF(("Cannot open entry %08x.\n", mLastError)) ;
        return FALSE ;
    }
    mLastError = object->SetProps(aNbProperties, aValues, &problems) ;
    if (HR_FAILED(mLastError)) {
        PRINTF(("Cannot update the object %08x.\n", mLastError)) ;
        return FALSE ;
    }
    if (problems != NULL) {
        for (ULONG i = 0 ; i < problems->cProblem ; ++ i) {
            PRINTF(("Problem %d: index %d code %08x.\n", i, 
                problems->aProblem [i].ulIndex, 
                problems->aProblem [i].scode)) ;
        }
    }
    mLastError = object->SaveChanges(0) ;
    if (HR_FAILED(mLastError)) {
        PRINTF(("Cannot commit changes %08x.\n", mLastError)) ;
    }
    return HR_SUCCEEDED(mLastError) ;
}

void nsAbWinHelper::MyFreeProws(LPSRowSet aRowset)
{
    if (aRowset == NULL) { return ; }
    ULONG i = 0 ; 

    for (i = 0 ; i < aRowset->cRows ; ++ i) {
        FreeBuffer(aRowset->aRow [i].lpProps) ;
    }
    FreeBuffer(aRowset) ;
}

nsAbWinHelperGuard::nsAbWinHelperGuard(uint32_t aType)
: mHelper(NULL) 
{
    switch(aType) {
    case nsAbWinType_Outlook: mHelper = new nsMapiAddressBook ; break ;
    case nsAbWinType_OutlookExp: mHelper = new nsWabAddressBook ; break ;
    default: break ;
    }
}

nsAbWinHelperGuard::~nsAbWinHelperGuard(void)
{
    delete mHelper ;
}

const char *kOutlookDirectoryScheme = "moz-aboutlookdirectory://" ;
const int kOutlookDirSchemeLength = 21 ;
const char *kOutlookStub = "op/" ;
const int kOutlookStubLength = 3 ;
const char *kOutlookExpStub = "oe/" ;
const int kOutlookExpStubLength = 3 ;
const char *kOutlookCardScheme = "moz-aboutlookcard://" ;
const int kOutlookCardSchemeLength = 16 ;

nsAbWinType getAbWinType(const char *aScheme, const char *aUri, nsCString& aStub, nsCString& aEntry) 
{
    aStub.Truncate() ;
    aEntry.Truncate() ;
    uint32_t schemeLength = strlen(aScheme) ;

    if (strncmp(aUri, aScheme, schemeLength) == 0) {
        if (strncmp(aUri + schemeLength, kOutlookStub, kOutlookStubLength) == 0) {
            aEntry = aUri + schemeLength + kOutlookStubLength ;
            aStub = kOutlookStub ;
            return nsAbWinType_Outlook ;
        }
        if (strncmp(aUri + schemeLength, kOutlookExpStub, kOutlookExpStubLength) == 0) {
            aEntry = aUri + schemeLength + kOutlookExpStubLength ;
            aStub = kOutlookExpStub ;
            return nsAbWinType_OutlookExp ;
        }
    }
    return nsAbWinType_Unknown ;   
}

void buildAbWinUri(const char *aScheme, uint32_t aType, nsCString& aUri)
{
    aUri.Assign(aScheme) ;
    switch(aType) {
    case nsAbWinType_Outlook: aUri.Append(kOutlookStub) ; break ; 
    case nsAbWinType_OutlookExp: aUri.Append(kOutlookExpStub) ; break ;
    default: aUri.Assign("") ;
    }
}






