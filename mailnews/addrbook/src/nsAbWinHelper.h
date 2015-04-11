/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsAbWinHelper_h___
#define nsAbWinHelper_h___

#include <windows.h>
#include <mapix.h>

#include "nsStringGlue.h"
#include "mozilla/Mutex.h"
#include "nsAutoPtr.h"

struct nsMapiEntry
{
    ULONG     mByteCount ;
    LPENTRYID mEntryId ;

    nsMapiEntry(void) ;
    ~nsMapiEntry(void) ;
    nsMapiEntry(ULONG aByteCount, LPENTRYID aEntryId) ;

    void Assign(ULONG aByteCount, LPENTRYID aEntryId) ;
    void Assign(const nsCString& aString) ;
    void ToString(nsCString& aString) const ;
    void Dump(void) const ;
} ;

struct nsMapiEntryArray 
{
    nsMapiEntry *mEntries ;
    ULONG      mNbEntries ;

    nsMapiEntryArray(void) ;
    ~nsMapiEntryArray(void) ;

    const nsMapiEntry& operator [] (int aIndex) const { return mEntries [aIndex] ; }
    void CleanUp(void) ;
} ;

class nsAbWinHelper
{
public:
    nsAbWinHelper(void) ;
    virtual ~nsAbWinHelper(void) ;

    // Get the top address books
    BOOL GetFolders(nsMapiEntryArray& aFolders) ;
    // Get a list of entries for cards/mailing lists in a folder/mailing list
    BOOL GetCards(const nsMapiEntry& aParent, LPSRestriction aRestriction, 
                  nsMapiEntryArray& aCards) ;
    // Get a list of mailing lists in a folder
    BOOL GetNodes(const nsMapiEntry& aParent, nsMapiEntryArray& aNodes) ;
    // Get the number of cards/mailing lists in a folder/mailing list
    BOOL GetCardsCount(const nsMapiEntry& aParent, ULONG& aNbCards) ;
    // Access last MAPI error
    HRESULT LastError(void) const { return mLastError ; }
    // Get the value of a MAPI property of type string
    BOOL GetPropertyString(const nsMapiEntry& aObject, ULONG aPropertyTag, nsCString& aValue) ;
    // Same as previous, but string is returned as unicode
    BOOL GetPropertyUString(const nsMapiEntry& aObject, ULONG aPropertyTag, nsString& aValue) ;
    // Get multiple string MAPI properties in one call.
    BOOL GetPropertiesUString(const nsMapiEntry& aObject, const ULONG *aPropertiesTag, 
                              ULONG aNbProperties, nsString *aValues);
    // Get the value of a MAPI property of type SYSTIME
    BOOL GetPropertyDate(const nsMapiEntry& aObject, ULONG aPropertyTag, 
                         WORD& aYear, WORD& aMonth, WORD& aDay) ;
    // Get the value of a MAPI property of type LONG
    BOOL GetPropertyLong(const nsMapiEntry& aObject, ULONG aPropertyTag, ULONG& aValue) ;
    // Get the value of a MAPI property of type BIN
    BOOL GetPropertyBin(const nsMapiEntry& aObject, ULONG aPropertyTag, nsMapiEntry& aValue) ;
    // Tests if a container contains an entry
    BOOL TestOpenEntry(const nsMapiEntry& aContainer, const nsMapiEntry& aEntry) ;
    // Delete an entry in the address book
    BOOL DeleteEntry(const nsMapiEntry& aContainer, const nsMapiEntry& aEntry) ;
    // Set the value of a MAPI property of type string in unicode
    BOOL SetPropertyUString (const nsMapiEntry& aObject, ULONG aPropertyTag, 
                             const PRUnichar *aValue) ;
    // Same as previous, but with a bunch of properties in one call
    BOOL SetPropertiesUString(const nsMapiEntry& aObject, const ULONG *aPropertiesTag,
                              ULONG aNbProperties, nsString *aValues) ;
    // Set the value of a MAPI property of type SYSTIME
    BOOL SetPropertyDate(const nsMapiEntry& aObject, ULONG aPropertyTag, 
                         WORD aYear, WORD aMonth, WORD aDay) ;
    // Create entry in the address book
    BOOL CreateEntry(const nsMapiEntry& aParent, nsMapiEntry& aNewEntry) ;
    // Create a distribution list in the address book
    BOOL CreateDistList(const nsMapiEntry& aParent, nsMapiEntry& aNewEntry) ;
    // Copy an existing entry in the address book
    BOOL CopyEntry(const nsMapiEntry& aContainer, const nsMapiEntry& aSource, nsMapiEntry& aTarget) ;
    // Get a default address book container
    BOOL GetDefaultContainer(nsMapiEntry& aContainer) ;
    // Is the helper correctly initialised?
    BOOL IsOK(void) const { return mAddressBook != NULL ; }

protected:
    HRESULT mLastError ;
    LPADRBOOK mAddressBook ;
    static uint32_t mEntryCounter ;
    static uint32_t mUseCount ;
    static nsAutoPtr<mozilla::Mutex> mMutex ;

    // Retrieve the contents of a container, with an optional restriction
    BOOL GetContents(const nsMapiEntry& aParent, LPSRestriction aRestriction, 
                     nsMapiEntry **aList, ULONG &aNbElements, ULONG aMapiType) ;
    // Retrieve the values of a set of properties on a MAPI object
    BOOL GetMAPIProperties(const nsMapiEntry& aObject, const ULONG *aPropertyTags, 
                           ULONG aNbProperties,
                           LPSPropValue& aValues, ULONG& aValueCount) ;
    // Set the values of a set of properties on a MAPI object
    BOOL SetMAPIProperties(const nsMapiEntry& aObject, ULONG aNbProperties, 
                           const LPSPropValue& aValues) ;
    // Clean-up a rowset returned by QueryRows
    void MyFreeProws(LPSRowSet aSet) ;
    // Allocation of a buffer for transmission to interfaces
    virtual void AllocateBuffer(ULONG aByteCount, LPVOID *aBuffer) = 0 ;
    // Destruction of a buffer provided by the interfaces
    virtual void FreeBuffer(LPVOID aBuffer) = 0 ;

private:
} ;

enum nsAbWinType 
{
    nsAbWinType_Unknown,
    nsAbWinType_Outlook,
    nsAbWinType_OutlookExp
} ;

class nsAbWinHelperGuard 
{
public :
    nsAbWinHelperGuard(uint32_t aType) ;
    ~nsAbWinHelperGuard(void) ;

    nsAbWinHelper *operator ->(void) { return mHelper ; }

private:
    nsAbWinHelper *mHelper ;
} ;

extern const char *kOutlookDirectoryScheme ;
extern const int  kOutlookDirSchemeLength ;
extern const char *kOutlookStub ;
extern const char *kOutlookExpStub ;
extern const char *kOutlookCardScheme ;

nsAbWinType getAbWinType(const char *aScheme, const char *aUri, 
                         nsCString& aStub, nsCString& aEntry) ;
void buildAbWinUri(const char *aScheme, uint32_t aType, nsCString& aUri) ;

#endif // nsAbWinHelper_h___



