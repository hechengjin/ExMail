/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMsgMailViewList.h"
#include "nsISupportsArray.h"
#include "nsIFileChannel.h"
#include "nsIMsgFilterService.h"
#include "nsIMsgMailSession.h"
#include "nsMsgBaseCID.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsIFile.h"
#include "nsComponentManagerUtils.h"
#include "mozilla/Services.h"

#define kDefaultViewPeopleIKnow "People I Know"
#define kDefaultViewRecent "Recent Mail"
#define kDefaultViewFiveDays "Last 5 Days"
#define kDefaultViewNotJunk "Not Junk"
#define kDefaultViewHasAttachments "Has Attachments"
 
nsMsgMailView::nsMsgMailView()
{
    mViewSearchTerms = do_CreateInstance(NS_SUPPORTSARRAY_CONTRACTID);
}

NS_IMPL_ADDREF(nsMsgMailView)
NS_IMPL_RELEASE(nsMsgMailView)
NS_IMPL_QUERY_INTERFACE1(nsMsgMailView, nsIMsgMailView)

nsMsgMailView::~nsMsgMailView()
{
    if (mViewSearchTerms)
        mViewSearchTerms->Clear();
}

NS_IMETHODIMP nsMsgMailView::GetMailViewName(PRUnichar ** aMailViewName)
{
    NS_ENSURE_ARG_POINTER(aMailViewName);

    *aMailViewName = ToNewUnicode(mName);
    return NS_OK;
}

NS_IMETHODIMP nsMsgMailView::SetMailViewName(const PRUnichar * aMailViewName)
{
    mName = aMailViewName;
    return NS_OK;
}

NS_IMETHODIMP nsMsgMailView::GetPrettyName(PRUnichar ** aMailViewName)
{
    NS_ENSURE_ARG_POINTER(aMailViewName);

    nsresult rv = NS_OK;
    if (!mBundle)
    {
        nsCOMPtr<nsIStringBundleService> bundleService =
          mozilla::services::GetStringBundleService();
        NS_ENSURE_TRUE(bundleService, NS_ERROR_UNEXPECTED);
        bundleService->CreateBundle("chrome://messenger/locale/mailviews.properties",
                                    getter_AddRefs(mBundle));
    }

    NS_ENSURE_TRUE(mBundle, NS_ERROR_FAILURE);

    // see if mName has an associated pretty name inside our string bundle and if so, use that as the pretty name
    // otherwise just return mName
    if (mName.EqualsLiteral(kDefaultViewPeopleIKnow))
        rv = mBundle->GetStringFromName(NS_LITERAL_STRING("mailViewPeopleIKnow").get(), aMailViewName);
    else if (mName.EqualsLiteral(kDefaultViewRecent))
        rv = mBundle->GetStringFromName(NS_LITERAL_STRING("mailViewRecentMail").get(), aMailViewName);
    else if (mName.EqualsLiteral(kDefaultViewFiveDays))
        rv = mBundle->GetStringFromName(NS_LITERAL_STRING("mailViewLastFiveDays").get(), aMailViewName);
    else if (mName.EqualsLiteral(kDefaultViewNotJunk))
        rv = mBundle->GetStringFromName(NS_LITERAL_STRING("mailViewNotJunk").get(), aMailViewName);
    else if (mName.EqualsLiteral(kDefaultViewHasAttachments))
        rv = mBundle->GetStringFromName(NS_LITERAL_STRING("mailViewHasAttachments").get(), aMailViewName);
    else
        *aMailViewName = ToNewUnicode(mName);

    return rv;
}

NS_IMETHODIMP nsMsgMailView::GetSearchTerms(nsISupportsArray ** aSearchTerms)
{
    NS_ENSURE_ARG_POINTER(aSearchTerms);
    NS_IF_ADDREF(*aSearchTerms = mViewSearchTerms);
    return NS_OK;
}

NS_IMETHODIMP nsMsgMailView::SetSearchTerms(nsISupportsArray * aSearchTerms)
{
    mViewSearchTerms = aSearchTerms;
    return NS_OK;
}

NS_IMETHODIMP nsMsgMailView::AppendTerm(nsIMsgSearchTerm * aTerm)
{
    NS_ENSURE_TRUE(aTerm, NS_ERROR_NULL_POINTER);
    
    return mViewSearchTerms->AppendElement(static_cast<nsISupports*>(aTerm));
}

NS_IMETHODIMP nsMsgMailView::CreateTerm(nsIMsgSearchTerm **aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    nsCOMPtr<nsIMsgSearchTerm> searchTerm = do_CreateInstance("@mozilla.org/messenger/searchTerm;1");
    NS_IF_ADDREF(*aResult = searchTerm);
    return NS_OK;
}

/////////////////////////////////////////////////////////////////////////////
// nsMsgMailViewList implementation
/////////////////////////////////////////////////////////////////////////////
nsMsgMailViewList::nsMsgMailViewList()
{
    LoadMailViews();
}

NS_IMPL_ADDREF(nsMsgMailViewList)
NS_IMPL_RELEASE(nsMsgMailViewList)
NS_IMPL_QUERY_INTERFACE1(nsMsgMailViewList, nsIMsgMailViewList)

nsMsgMailViewList::~nsMsgMailViewList()
{

}

NS_IMETHODIMP nsMsgMailViewList::GetMailViewCount(uint32_t * aCount)
{
    NS_ENSURE_ARG_POINTER(aCount);

    if (m_mailViews)
       m_mailViews->Count(aCount);
    else
        *aCount = 0;
    return NS_OK;
}

NS_IMETHODIMP nsMsgMailViewList::GetMailViewAt(uint32_t aMailViewIndex, nsIMsgMailView ** aMailView)
{
    NS_ENSURE_ARG_POINTER(aMailView);
    NS_ENSURE_TRUE(m_mailViews, NS_ERROR_FAILURE);

    uint32_t mailViewCount;
    m_mailViews->Count(&mailViewCount);
    NS_ENSURE_TRUE(mailViewCount >= aMailViewIndex, NS_ERROR_FAILURE);

    return m_mailViews->QueryElementAt(aMailViewIndex, NS_GET_IID(nsIMsgMailView),
                                      (void **)aMailView);
}

NS_IMETHODIMP nsMsgMailViewList::AddMailView(nsIMsgMailView * aMailView)
{
    NS_ENSURE_ARG_POINTER(aMailView);
    NS_ENSURE_TRUE(m_mailViews, NS_ERROR_FAILURE);

    m_mailViews->AppendElement(static_cast<nsISupports*>(aMailView));
    return NS_OK;
}

NS_IMETHODIMP nsMsgMailViewList::RemoveMailView(nsIMsgMailView * aMailView)
{
    NS_ENSURE_ARG_POINTER(aMailView);
    NS_ENSURE_TRUE(m_mailViews, NS_ERROR_FAILURE);

    m_mailViews->RemoveElement(static_cast<nsISupports*>(aMailView));
    return NS_OK;
}

NS_IMETHODIMP nsMsgMailViewList::CreateMailView(nsIMsgMailView ** aMailView)
{
    NS_ENSURE_ARG_POINTER(aMailView);

    nsMsgMailView * mailView = new nsMsgMailView;
    NS_ENSURE_TRUE(mailView, NS_ERROR_OUT_OF_MEMORY);

    NS_IF_ADDREF(*aMailView = mailView);
    return NS_OK;
}

NS_IMETHODIMP nsMsgMailViewList::Save()
{
    // brute force...remove all the old filters in our filter list, then we'll re-add our current
    // list
    nsCOMPtr<nsIMsgFilter> msgFilter;
    uint32_t numFilters = 0;
    if (mFilterList)
      mFilterList->GetFilterCount(&numFilters);
    while (numFilters)
    {
        mFilterList->RemoveFilterAt(numFilters - 1);
        numFilters--;
    }

    // now convert our mail view list into a filter list and save it
    ConvertMailViewListToFilterList();

    // now save the filters to our file
    return mFilterList ? mFilterList->SaveToDefaultFile() : NS_ERROR_FAILURE;
}

nsresult nsMsgMailViewList::ConvertMailViewListToFilterList()
{
  uint32_t mailViewCount = 0;

  if (m_mailViews)
    m_mailViews->Count(&mailViewCount);
  nsCOMPtr<nsIMsgMailView> mailView;
  nsCOMPtr<nsIMsgFilter> newMailFilter;
  nsString mailViewName;
  for (uint32_t index = 0; index < mailViewCount; index++)
  {
      GetMailViewAt(index, getter_AddRefs(mailView));
      if (!mailView)
          continue;
      mailView->GetMailViewName(getter_Copies(mailViewName));
      mFilterList->CreateFilter(mailViewName, getter_AddRefs(newMailFilter));
      if (!newMailFilter)
          continue;

      nsCOMPtr<nsISupportsArray> searchTerms;
      mailView->GetSearchTerms(getter_AddRefs(searchTerms));
      newMailFilter->SetSearchTerms(searchTerms);
      mFilterList->InsertFilterAt(index, newMailFilter);
  }

  return NS_OK;
}

nsresult nsMsgMailViewList::LoadMailViews()
{
    nsCOMPtr<nsIFile> file;
    nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(file));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = file->AppendNative(nsDependentCString("mailViews.dat"));

    // if the file doesn't exist, we should try to get it from the defaults directory and copy it over
    bool exists = false;
    file->Exists(&exists);
    if (!exists)
    {
        nsCOMPtr<nsIMsgMailSession> mailSession = do_GetService(NS_MSGMAILSESSION_CONTRACTID, &rv);
        NS_ENSURE_SUCCESS(rv, rv);
        nsCOMPtr<nsIFile> defaultMessagesFile;
        nsCOMPtr<nsIFile> profileDir;
        rv = mailSession->GetDataFilesDir("messenger", getter_AddRefs(defaultMessagesFile));
        rv = defaultMessagesFile->AppendNative(nsDependentCString("mailViews.dat"));

         // get the profile directory
        rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(profileDir));
        
        // now copy the file over to the profile directory
        defaultMessagesFile->CopyToNative(profileDir, EmptyCString());
    }
    // this is kind of a hack but I think it will be an effective hack. The filter service already knows how to 
    // take a nsIFile and parse the contents into filters which are very similar to mail views. Intead of
    // re-writing all of that dirty parsing code, let's just re-use it then convert the results into a data strcuture
    // we wish to give to our consumers. 
      
    nsCOMPtr<nsIMsgFilterService> filterService = do_GetService(NS_MSGFILTERSERVICE_CONTRACTID, &rv);
    nsCOMPtr<nsIMsgFilterList> mfilterList;
      
    rv = filterService->OpenFilterList(file, NULL, NULL, getter_AddRefs(mFilterList));
    NS_ENSURE_SUCCESS(rv, rv);

    // now convert the filter list into our mail view objects, stripping out just the info we need
    ConvertFilterListToMailView(mFilterList, getter_AddRefs(m_mailViews));
    return rv;
}

nsresult nsMsgMailViewList::ConvertFilterListToMailView(nsIMsgFilterList * aFilterList, nsISupportsArray ** aMailViewList)
{
    nsresult rv = NS_OK;
    NS_ENSURE_ARG_POINTER(aFilterList);
    NS_ENSURE_ARG_POINTER(aMailViewList);

    nsCOMPtr<nsISupportsArray> mailViewList = do_CreateInstance(
        NS_SUPPORTSARRAY_CONTRACTID);

    // iterate over each filter in the list
    nsCOMPtr<nsIMsgFilter> msgFilter;
    uint32_t numFilters;
    aFilterList->GetFilterCount(&numFilters);
    for (uint32_t index = 0; index < numFilters; index++)
    {
        aFilterList->GetFilterAt(index, getter_AddRefs(msgFilter));
        if (!msgFilter)
            continue;
        // create a new nsIMsgMailView for this item
        nsCOMPtr<nsIMsgMailView> newMailView; 
        rv = CreateMailView(getter_AddRefs(newMailView));
        NS_ENSURE_SUCCESS(rv, rv);
        nsString filterName;
        msgFilter->GetFilterName(filterName);
        newMailView->SetMailViewName(filterName.get());

        nsCOMPtr<nsISupportsArray> filterSearchTerms;
        msgFilter->GetSearchTerms(getter_AddRefs(filterSearchTerms));
        newMailView->SetSearchTerms(filterSearchTerms);

        // now append this new mail view to our global list view
        mailViewList->AppendElement(static_cast<nsISupports*>(newMailView));
    }

    NS_IF_ADDREF(*aMailViewList = mailViewList);

    return rv;
}
