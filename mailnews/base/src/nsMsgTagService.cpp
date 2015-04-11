/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "msgCore.h"
#include "nsMsgTagService.h"
#include "nsMsgBaseCID.h"
#include "nsIPrefService.h"
#include "nsISupportsPrimitives.h"
#include "nsMsgI18N.h"
#include "nsIPrefLocalizedString.h"
#include "nsMsgDBView.h" // for labels migration
#include "nsQuickSort.h"
#include "nsMsgUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsMemory.h"

#define STRLEN(s) (sizeof(s) - 1)

#define TAG_PREF_VERSION        "version"
#define TAG_PREF_SUFFIX_TAG     ".tag"
#define TAG_PREF_SUFFIX_COLOR   ".color"
#define TAG_PREF_SUFFIX_ORDINAL ".ordinal"
#define TAG_CMP_LESSER          -1
#define TAG_CMP_EQUAL           0
#define TAG_CMP_GREATER         1

static bool gMigratingKeys = false;

// comparison functions for nsQuickSort
static int
CompareMsgTagKeys(const void* aTagPref1, const void* aTagPref2, void* aData)
{
  return strcmp(*static_cast<const char* const*>(aTagPref1),
                *static_cast<const char* const*>(aTagPref2));
}

static int
CompareMsgTags(const void* aTagPref1, const void* aTagPref2, void* aData)
{
  // Sort nsMsgTag objects by ascending order, using their ordinal or key.
  // The "smallest" value will be first in the sorted array,
  // thus being the most important element.
  nsMsgTag *element1 = *(nsMsgTag**) aTagPref1;
  nsMsgTag *element2 = *(nsMsgTag**) aTagPref2;

  // if we have only one element, it wins
  if (!element1 && !element2)
    return TAG_CMP_EQUAL;
  if (!element2)
    return TAG_CMP_LESSER;
  if (!element1)
    return TAG_CMP_GREATER;

  // only use the key if the ordinal is not defined or empty
  nsCAutoString value1, value2;
  element1->GetOrdinal(value1);
  if (value1.IsEmpty())
    element1->GetKey(value1);
  element2->GetOrdinal(value2);
  if (value2.IsEmpty())
    element2->GetKey(value2);

  return strcmp(value1.get(), value2.get());
}


//
//  nsMsgTag
//
NS_IMPL_ISUPPORTS1(nsMsgTag, nsIMsgTag)

nsMsgTag::nsMsgTag(const nsACString &aKey,
                   const nsAString  &aTag,
                   const nsACString &aColor,
                   const nsACString &aOrdinal)
: mTag(aTag),
  mKey(aKey),
  mColor(aColor),
  mOrdinal(aOrdinal)
{
}

nsMsgTag::~nsMsgTag()
{
}

/* readonly attribute ACString key; */
NS_IMETHODIMP nsMsgTag::GetKey(nsACString & aKey)
{
  aKey = mKey;
  return NS_OK;
}

/* readonly attribute AString tag; */
NS_IMETHODIMP nsMsgTag::GetTag(nsAString & aTag)
{
  aTag = mTag;
  return NS_OK;
}

/* readonly attribute ACString color; */
NS_IMETHODIMP nsMsgTag::GetColor(nsACString & aColor)
{
  aColor = mColor;
  return NS_OK;
}

/* readonly attribute ACString ordinal; */
NS_IMETHODIMP nsMsgTag::GetOrdinal(nsACString & aOrdinal)
{
  aOrdinal = mOrdinal;
  return NS_OK;
}


//
//  nsMsgTagService
//
NS_IMPL_ISUPPORTS1(nsMsgTagService, nsIMsgTagService)

nsMsgTagService::nsMsgTagService()
{
  m_tagPrefBranch = nullptr;
  nsCOMPtr<nsIPrefService> prefService(do_GetService(NS_PREFSERVICE_CONTRACTID));
  if (prefService)
    prefService->GetBranch("mailnews.tags.", getter_AddRefs(m_tagPrefBranch));
  // need to figure out how to migrate the tags only once.
  MigrateLabelsToTags();
  RefreshKeyCache();
}

nsMsgTagService::~nsMsgTagService()
{
  /* destructor code */
}

/* wstring getTagForKey (in string key); */
NS_IMETHODIMP nsMsgTagService::GetTagForKey(const nsACString &key, nsAString &_retval)
{
  nsCAutoString prefName(key);
  if (!gMigratingKeys)
    ToLowerCase(prefName);
  prefName.AppendLiteral(TAG_PREF_SUFFIX_TAG);
  return GetUnicharPref(prefName.get(), _retval);
}

/* void setTagForKey (in string key); */
NS_IMETHODIMP nsMsgTagService::SetTagForKey(const nsACString &key, const nsAString &tag )
{
  nsCAutoString prefName(key);
  ToLowerCase(prefName);
  prefName.AppendLiteral(TAG_PREF_SUFFIX_TAG);
  return SetUnicharPref(prefName.get(), tag);
}

/* void getKeyForTag (in wstring tag); */
NS_IMETHODIMP nsMsgTagService::GetKeyForTag(const nsAString &aTag, nsACString &aKey)
{
  uint32_t count;
  char **prefList;
  nsresult rv = m_tagPrefBranch->GetChildList("", &count, &prefList);
  NS_ENSURE_SUCCESS(rv, rv);
  // traverse the list, and look for a pref with the desired tag value.
  for (uint32_t i = count; i--;)
  {
    // We are returned the tag prefs in the form "<key>.<tag_data_type>", but
    // since we only want the tags, just check that the string ends with "tag".
    nsDependentCString prefName(prefList[i]);
    if (StringEndsWith(prefName, NS_LITERAL_CSTRING(TAG_PREF_SUFFIX_TAG)))
    {
      nsAutoString curTag;
      GetUnicharPref(prefList[i], curTag);
      if (aTag.Equals(curTag))
      {
        aKey = Substring(prefName, 0, prefName.Length() - STRLEN(TAG_PREF_SUFFIX_TAG));
        break;
      }
    }
  }
  NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(count, prefList);
  ToLowerCase(aKey);
  return NS_OK;
}

/* ACString getTopKey (in ACString keylist); */
NS_IMETHODIMP nsMsgTagService::GetTopKey(const nsACString & keyList, nsACString & _retval)
{
  _retval.Truncate();
  // find the most important key
  nsTArray<nsCString> keyArray;
  ParseString(keyList, ' ', keyArray);
  uint32_t keyCount = keyArray.Length();
  nsCString *topKey = nullptr, *key, topOrdinal, ordinal;
  for (uint32_t i = 0; i < keyCount; ++i)
  {
    key = &keyArray[i];
    if (key->IsEmpty())
      continue;

    // ignore unknown keywords
    nsAutoString tagValue;
    nsresult rv = GetTagForKey(*key, tagValue);
    if (NS_FAILED(rv) || tagValue.IsEmpty())
      continue;

    // new top key, judged by ordinal order?
    rv = GetOrdinalForKey(*key, ordinal);
    if (NS_FAILED(rv) || ordinal.IsEmpty())
      ordinal = *key;
    if ((ordinal < topOrdinal) || topOrdinal.IsEmpty())
    {
      topOrdinal = ordinal;
      topKey = key; // copy actual result key only once - later
    }
  }
  // return the most important key - if any
  if (topKey)
    _retval = *topKey;
  return NS_OK;
}

/* void addTagForKey (in string key, in wstring tag, in string color, in string ordinal); */
NS_IMETHODIMP nsMsgTagService::AddTagForKey(const nsACString &key,
                                            const nsAString  &tag,
                                            const nsACString &color,
                                            const nsACString &ordinal)
{
  nsCAutoString prefName(key);
  ToLowerCase(prefName);
  prefName.AppendLiteral(TAG_PREF_SUFFIX_TAG);
  nsresult rv = SetUnicharPref(prefName.get(), tag);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = SetColorForKey(key, color);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = RefreshKeyCache();
  NS_ENSURE_SUCCESS(rv, rv);
  return SetOrdinalForKey(key, ordinal);
}

/* void addTag (in wstring tag, in long color); */
NS_IMETHODIMP nsMsgTagService::AddTag(const nsAString  &tag,
                                      const nsACString &color,
                                      const nsACString &ordinal)
{
  // figure out key from tag. Apply transformation stripping out
  // illegal characters like <SP> and then convert to imap mod utf7.
  // Then, check if we have a tag with that key yet, and if so,
  // make it unique by appending A, AA, etc.
  // Should we use an iterator?
  nsAutoString transformedTag(tag);
  MsgReplaceChar(transformedTag, " ()/{%*<>\\\"", '_');
  nsCAutoString key;
  CopyUTF16toMUTF7(transformedTag, key);
  // We have an imap server that converts keys to upper case so we're going
  // to normalize all keys to lower case (upper case looks ugly in prefs.js)
  ToLowerCase(key);
  nsCAutoString prefName(key);
  while (true)
  {
    nsAutoString tagValue;
    nsresult rv = GetTagForKey(prefName, tagValue);
    if (NS_FAILED(rv) || tagValue.IsEmpty() || tagValue.Equals(tag))
      return AddTagForKey(prefName, tag, color, ordinal);
    prefName.Append('A');
  }
  NS_ASSERTION(false, "can't get here");
  return NS_ERROR_FAILURE;
}

/* long getColorForKey (in string key); */
NS_IMETHODIMP nsMsgTagService::GetColorForKey(const nsACString &key, nsACString  &_retval)
{
  nsCAutoString prefName(key);
  if (!gMigratingKeys)
    ToLowerCase(prefName);
  prefName.AppendLiteral(TAG_PREF_SUFFIX_COLOR);
  nsCString color;
  nsresult rv = m_tagPrefBranch->GetCharPref(prefName.get(), getter_Copies(color));
  if (NS_SUCCEEDED(rv))
    _retval = color;
  return NS_OK;
}

/* void setColorForKey (in ACString key, in ACString color); */
NS_IMETHODIMP nsMsgTagService::SetColorForKey(const nsACString & key, const nsACString & color)
{
  nsCAutoString prefName(key);
  ToLowerCase(prefName);
  prefName.AppendLiteral(TAG_PREF_SUFFIX_COLOR);
  if (color.IsEmpty())
  {
    m_tagPrefBranch->ClearUserPref(prefName.get());
    return NS_OK;
  }
  return m_tagPrefBranch->SetCharPref(prefName.get(), nsCString(color).get());
}

/* ACString getOrdinalForKey (in ACString key); */
NS_IMETHODIMP nsMsgTagService::GetOrdinalForKey(const nsACString & key, nsACString & _retval)
{
  nsCAutoString prefName(key);
  if (!gMigratingKeys)
    ToLowerCase(prefName);
  prefName.AppendLiteral(TAG_PREF_SUFFIX_ORDINAL);
  nsCString ordinal;
  nsresult rv = m_tagPrefBranch->GetCharPref(prefName.get(), getter_Copies(ordinal));
  _retval = ordinal;
  return rv;
}

/* void setOrdinalForKey (in ACString key, in ACString ordinal); */
NS_IMETHODIMP nsMsgTagService::SetOrdinalForKey(const nsACString & key, const nsACString & ordinal)
{
  nsCAutoString prefName(key);
  ToLowerCase(prefName);
  prefName.AppendLiteral(TAG_PREF_SUFFIX_ORDINAL);
  if (ordinal.IsEmpty())
  {
    m_tagPrefBranch->ClearUserPref(prefName.get());
    return NS_OK;
  }
  return m_tagPrefBranch->SetCharPref(prefName.get(), nsCString(ordinal).get());
}

/* void deleteTag (in wstring tag); */
NS_IMETHODIMP nsMsgTagService::DeleteKey(const nsACString &key)
{
  // clear the associated prefs
  nsCAutoString prefName(key);
  if (!gMigratingKeys)
    ToLowerCase(prefName);
  nsresult rv = m_tagPrefBranch->DeleteBranch(prefName.get());
  NS_ENSURE_SUCCESS(rv, rv);
  return RefreshKeyCache();
}

/* void getAllTags (out unsigned long count, [array, size_is (count), retval] out nsIMsgTag tagArray); */
NS_IMETHODIMP nsMsgTagService::GetAllTags(uint32_t *aCount, nsIMsgTag ***aTagArray)
{
  NS_ENSURE_ARG_POINTER(aCount);
  NS_ENSURE_ARG_POINTER(aTagArray);

  // preset harmless default values
  *aCount = 0;
  *aTagArray = nullptr;

  // get the actual tag definitions
  nsresult rv;
  uint32_t prefCount;
  char **prefList;
  rv = m_tagPrefBranch->GetChildList("", &prefCount, &prefList);
  NS_ENSURE_SUCCESS(rv, rv);
  // sort them by key for ease of processing
  NS_QuickSort(prefList, prefCount, sizeof(char*), CompareMsgTagKeys, nullptr);

  // build an array of nsIMsgTag elements from the orderered list
  // it's at max the same size as the preflist, but usually only about half
  nsIMsgTag** tagArray = (nsIMsgTag**) NS_Alloc(sizeof(nsIMsgTag*) * prefCount);

  if (!tagArray)
  {
    NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(prefCount, prefList);
    return NS_ERROR_OUT_OF_MEMORY;
  }
  uint32_t currentTagIndex = 0;
  nsMsgTag *newMsgTag;
  nsString tag;
  nsCString lastKey, color, ordinal;
  for (uint32_t i = prefCount; i--;)
  {
    // extract just the key from <key>.<info=tag|color|ordinal>
    char *info = strrchr(prefList[i], '.');
    if (info)
    {
      nsCAutoString key(Substring(prefList[i], info));
      if (key != lastKey)
      {
        if (!key.IsEmpty())
        {
          // .tag MUST exist (but may be empty)
          rv = GetTagForKey(key, tag);
          if (NS_SUCCEEDED(rv))
          {
            // .color MAY exist
            color.Truncate();
            GetColorForKey(key, color);
            // .ordinal MAY exist
            rv = GetOrdinalForKey(key, ordinal);
            if (NS_FAILED(rv))
              ordinal.Truncate();
            // store the tag info in our array
            newMsgTag = new nsMsgTag(key, tag, color, ordinal);
            if (!newMsgTag)
            {
              NS_FREE_XPCOM_ISUPPORTS_POINTER_ARRAY(currentTagIndex, tagArray);
              NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(prefCount, prefList);
              return NS_ERROR_OUT_OF_MEMORY;
            }
            NS_ADDREF(tagArray[currentTagIndex++] = newMsgTag);
          }
        }
        lastKey = key;
      }
    }
  }
  NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(prefCount, prefList);

  // sort the non-null entries by ordinal
  NS_QuickSort(tagArray, currentTagIndex, sizeof(nsMsgTag*), CompareMsgTags,
               nullptr);

  // All done, now return the values (the idl's size_is(count) parameter
  // ensures that the array is cut accordingly).
  *aCount = currentTagIndex;
  *aTagArray = tagArray;

  return NS_OK;
}

nsresult nsMsgTagService::SetUnicharPref(const char *prefName,
                              const nsAString &val)
{
  nsresult rv = NS_OK;
  if (!val.IsEmpty())
  {
    nsCOMPtr<nsISupportsString> supportsString =
      do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID, &rv);
    if (supportsString)
    {
      supportsString->SetData(val);
      rv = m_tagPrefBranch->SetComplexValue(prefName,
                                            NS_GET_IID(nsISupportsString),
                                            supportsString);
    }
  }
  else
  {
    m_tagPrefBranch->ClearUserPref(prefName);
  }
  return rv;
}

nsresult nsMsgTagService::GetUnicharPref(const char *prefName,
                              nsAString &prefValue)
{
  nsresult rv;
  nsCOMPtr<nsISupportsString> supportsString =
    do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID, &rv);
  if (supportsString)
  {
    rv = m_tagPrefBranch->GetComplexValue(prefName,
                                          NS_GET_IID(nsISupportsString),
                                          getter_AddRefs(supportsString));
    if (supportsString)
      rv = supportsString->GetData(prefValue);
    else
      prefValue.Truncate();
  }
  return rv;
}


nsresult nsMsgTagService::MigrateLabelsToTags()
{
  nsCString prefString;

  int32_t prefVersion = 0;
  nsresult rv = m_tagPrefBranch->GetIntPref(TAG_PREF_VERSION, &prefVersion);
  if (NS_SUCCEEDED(rv) && prefVersion > 1)
    return rv;
  else if (prefVersion == 1)
  {
    gMigratingKeys = true;
  // need to convert the keys to lower case
    nsIMsgTag **tagArray;
    uint32_t numTags;
    GetAllTags(&numTags, &tagArray);
    for (uint32_t tagIndex = 0; tagIndex < numTags; tagIndex++)
    {
      nsCAutoString key, color, ordinal;
      nsAutoString tagStr;
      nsIMsgTag *tag = tagArray[tagIndex];
      tag->GetKey(key);
      tag->GetTag(tagStr);
      tag->GetOrdinal(ordinal);
      tag->GetColor(color);
      DeleteKey(key);
      ToLowerCase(key);
      AddTagForKey(key, tagStr, color, ordinal);
    }
    NS_FREE_XPCOM_ISUPPORTS_POINTER_ARRAY(numTags, tagArray);
    gMigratingKeys = false;
  }
  else 
  {
    nsCOMPtr<nsIPrefBranch> prefRoot(do_GetService(NS_PREFSERVICE_CONTRACTID));
    nsCOMPtr<nsIPrefLocalizedString> pls;
    nsString ucsval;
    nsCAutoString labelKey("$label1");
    for(int32_t i = 0; i < PREF_LABELS_MAX; )
    {
      prefString.Assign(PREF_LABELS_DESCRIPTION);
      prefString.AppendInt(i + 1);
      rv = prefRoot->GetComplexValue(prefString.get(),
                                     NS_GET_IID(nsIPrefLocalizedString),
                                     getter_AddRefs(pls));
      NS_ENSURE_SUCCESS(rv, rv);
      pls->ToString(getter_Copies(ucsval));

      prefString.Assign(PREF_LABELS_COLOR);
      prefString.AppendInt(i + 1);
      nsCString csval;
      rv = prefRoot->GetCharPref(prefString.get(), getter_Copies(csval));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = AddTagForKey(labelKey, ucsval, csval, EmptyCString());
      NS_ENSURE_SUCCESS(rv, rv);
      labelKey.SetCharAt(++i + '1', 6);
    }
  }
  m_tagPrefBranch->SetIntPref(TAG_PREF_VERSION, 2);
  return rv;
}

NS_IMETHODIMP nsMsgTagService::IsValidKey(const nsACString &aKey, bool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = m_keys.Contains(aKey);
  return NS_OK;
}

// refresh the local tag key array m_keys from preferences
nsresult nsMsgTagService::RefreshKeyCache()
{
  nsIMsgTag **tagArray;
  uint32_t numTags;
  nsresult rv = GetAllTags(&numTags, &tagArray);
  NS_ENSURE_SUCCESS(rv, rv);
  m_keys.Clear();

  for (uint32_t tagIndex = 0; tagIndex < numTags; tagIndex++)
  {
    nsIMsgTag *tag = tagArray[tagIndex];
    if (!tag) {
      rv = NS_ERROR_FAILURE;
      break;
    }
    nsCAutoString key;
    tag->GetKey(key);
    if (!m_keys.InsertElementAt(tagIndex, key)) {
      rv = NS_ERROR_FAILURE;
      break;
    }
  }
  NS_FREE_XPCOM_ISUPPORTS_POINTER_ARRAY(numTags, tagArray);
  return rv;
}
