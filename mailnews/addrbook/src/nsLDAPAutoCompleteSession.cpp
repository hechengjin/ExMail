/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsLDAPAutoCompleteSession.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsILDAPURL.h"
#include "nsILDAPService.h"
#include "nspr.h"
#include "nsIStringBundle.h"
#include "nsCRTGlue.h"
#include "nsIObserverService.h"
#include "nsNetUtil.h"
#include "nsICategoryManager.h"
#include "nsCategoryManagerUtils.h"
#include "nsILDAPService.h"
#include "nsILDAPMessage.h"
#include "nsILDAPErrors.h"
#include "nsXPCOMCIDInternal.h"

#ifdef PR_LOGGING
static PRLogModuleInfo *sLDAPAutoCompleteLogModule = 0;
#endif


// Because this object gets called via proxies, we need to use a THREADSAFE
// ISUPPORTS; if bug 101252 gets fixed, we can go back to the non-threadsafe
// version.
//
NS_IMPL_THREADSAFE_ISUPPORTS3(nsLDAPAutoCompleteSession, 
                              nsIAutoCompleteSession, nsILDAPMessageListener,
                              nsILDAPAutoCompleteSession)

nsLDAPAutoCompleteSession::nsLDAPAutoCompleteSession() :
    mState(UNBOUND), 
    mFilterTemplate("(|(cn=%v1*%v2-*)(mail=%v1*%v2-*)(sn=%v1*%v2-*))"),
    mMaxHits(100), mMinStringLength(2), mCjkMinStringLength(0), 
    mVersion(nsILDAPConnection::VERSION3)
{
}

nsLDAPAutoCompleteSession::~nsLDAPAutoCompleteSession()
{
    NS_IF_RELEASE(mConnection);
}

#define IS_CJK_CHAR_FOR_LDAP(u)  (0x2e80 <= (u) && (u) <= 0xd7ff)

/* void onStartLookup (in wstring searchString, in nsIAutoCompleteResults previousSearchResult, in nsIAutoCompleteListener listener); */
NS_IMETHODIMP 
nsLDAPAutoCompleteSession::OnStartLookup(const PRUnichar *searchString, 
                                         nsIAutoCompleteResults *previousSearchResult,
                                         nsIAutoCompleteListener *listener)
{
  NS_ENSURE_ARG_POINTER(listener);
  if (!mFormatter)
  {
    NS_WARNING("mFormatter should not be null for nsLDAPAutoCompleteSession::OnStartLookup");
    return NS_ERROR_NOT_INITIALIZED;
  }

    nsresult rv; // Hold return values from XPCOM calls

#ifdef PR_LOGGING
    // Initialize logging, if it hasn't been already.
    if (!sLDAPAutoCompleteLogModule) {
        sLDAPAutoCompleteLogModule = PR_NewLogModule("ldapautocomplete");

        NS_ABORT_IF_FALSE(sLDAPAutoCompleteLogModule, 
                          "failed to initialize ldapautocomplete log module");
    }
#endif

    PR_LOG(sLDAPAutoCompleteLogModule, PR_LOG_DEBUG, 
           ("nsLDAPAutoCompleteSession::OnStartLookup entered\n"));

    mListener = listener;

    // Ignore the empty string, strings with @ in them, and strings
    // that are too short.
    if (searchString[0] == 0 ||
        nsDependentString(searchString).FindChar(PRUnichar('@'), 0) != -1 || 
        nsDependentString(searchString).FindChar(PRUnichar(','), 0) != -1 || 
        ( !IS_CJK_CHAR_FOR_LDAP(searchString[0]) ?
          mMinStringLength && NS_strlen(searchString) < mMinStringLength :
          mCjkMinStringLength && NS_strlen(searchString) < 
          mCjkMinStringLength ) ) {

        FinishAutoCompleteLookup(nsIAutoCompleteStatus::ignored, NS_OK, mState);
        return NS_OK;
    } else {
        mSearchString = searchString;        // save it for later use
    }

    // Make sure this was called appropriately.
    if (mState == SEARCHING || mState == BINDING) {
        NS_ERROR("nsLDAPAutoCompleteSession::OnStartLookup(): called while "
                 "search already in progress; no lookup started.");
        FinishAutoCompleteLookup(nsIAutoCompleteStatus::failureItems,
                                 NS_ERROR_FAILURE, mState);
        return NS_ERROR_FAILURE;
    }

    // See if this is a narrow search that we could potentially do locally.
    if (previousSearchResult) {

        // Get the string representing previous search results.
        nsString prevSearchStr;

        rv = previousSearchResult->GetSearchString(
            getter_Copies(prevSearchStr));
        if ( NS_FAILED(rv) ) {
            NS_ERROR("nsLDAPAutoCompleteSession::OnStartLookup(): couldn't "
                     "get search string from previousSearchResult");
            FinishAutoCompleteLookup(nsIAutoCompleteStatus::failureItems, 
                                     NS_ERROR_FAILURE, mState);
            return NS_ERROR_FAILURE;
        }

        // Does the string actually contain anything?
        if ( prevSearchStr.get() && prevSearchStr.get()[0]) {

            PR_LOG(sLDAPAutoCompleteLogModule, PR_LOG_DEBUG, 
                   ("nsLDAPAutoCompleteSession::OnStartLookup(): starting "
                    "narrowing search\n"));

            // XXXdmose for performance, we should really do a local,
            // synchronous search against the existing dataset instead of
            // just kicking off a new LDAP search here.  When implementing
            // this, need to be sure that only previous results which did not
            // hit the size limit and were successfully completed are used.
            //
            mState = SEARCHING;
            return DoTask();
        }
    }

    // Init connection if necessary
    //
    switch (mState) {
    case UNBOUND:

        // Initialize the connection.  
        //
        rv = InitConnection();
        if (NS_FAILED(rv)) {

            // InitConnection() will have already called
            // FinishAutoCompleteLookup for us as necessary
            //
            return rv;
        }

        return NS_OK;

    case BOUND:

        PR_LOG(sLDAPAutoCompleteLogModule, PR_LOG_DEBUG, 
               ("nsLDAPAutoComplete::OnStartLookup(): subsequent search "
                "starting"));

        // Kick off an LDAP search
        mState = SEARCHING;
        return DoTask();

    case INITIALIZING:
        // We don't need to do anything here (for now at least), because
        // we can't really abandon the initialization. If we allowed the
        // initialization to be aborted, we could potentially lock the
        // UI thread again, since the DNS service might be stalled.
        //
        return NS_OK;

    case BINDING:
    case SEARCHING:
        // We should never get here
        NS_ERROR("nsLDAPAutoCompleteSession::OnStartLookup(): unexpected "
                 "value of mStatus");
        return NS_ERROR_UNEXPECTED;
    }
    
    return NS_ERROR_UNEXPECTED;     /*NOTREACHED*/
}

/* void onStopLookup (); */
NS_IMETHODIMP 
nsLDAPAutoCompleteSession::OnStopLookup()
{
#ifdef PR_LOGGING
    // Initialize logging, if it hasn't been already.
    if (!sLDAPAutoCompleteLogModule) {
        sLDAPAutoCompleteLogModule = PR_NewLogModule("ldapautocomplete");

        NS_ABORT_IF_FALSE(sLDAPAutoCompleteLogModule, 
                          "failed to initialize ldapautocomplete log module");
    }
#endif

    PR_LOG(sLDAPAutoCompleteLogModule, PR_LOG_DEBUG, 
           ("nsLDAPAutoCompleteSession::OnStopLookup entered\n"));

    switch (mState) {
    case UNBOUND:
        // Nothing to stop
        return NS_OK;

    case BOUND:
        // Nothing to stop
        return NS_OK;

    case INITIALIZING:
        // We can't or shouldn't abort the initialization, because then the
        // DNS service can hang again...
        //
        return NS_OK;

    case BINDING:
    case SEARCHING:
        // Abandon the operation, if there is one
        if (mOperation) {
            nsresult rv = mOperation->AbandonExt();

            if (NS_FAILED(rv)) {
                // Since there's nothing interesting that can or should be
                // done if this abandon failed, warn about it and move on
                NS_WARNING("nsLDAPAutoCompleteSession::OnStopLookup(): "
                           "error calling mOperation->AbandonExt()");
            }

            // Force nsCOMPtr to release mOperation
            mOperation = 0;
        }

        // Set the status properly, set to UNBOUND of we were binding, or
        // to BOUND if we were searching.
        mState = (mState == BINDING ? UNBOUND : BOUND);
        if (mState == UNBOUND)
          NS_IF_RELEASE(mConnection);
    }

    mResultsArray = 0;
    mResults = 0;
    mListener = 0;

    return NS_OK;
}

/* void onAutoComplete (in wstring searchString, in nsIAutoCompleteResults previousSearchResult, in nsIAutoCompleteListener listener); */
NS_IMETHODIMP 
nsLDAPAutoCompleteSession::OnAutoComplete(const PRUnichar *searchString, 
                                 nsIAutoCompleteResults *previousSearchResult, 
                                          nsIAutoCompleteListener *listener)
{
    // OnStopLookup should have already been called, so there's nothing to
    // free here.  Additionally, as of this writing, no one is hanging around
    // waiting for mListener->OnAutoComplete() to be called either, and if
    // they were, it's unclear what we'd return, since we're not guaranteed
    // to be in any particular state.  My suspicion is that this method
    // (nsIAutoCompleteSession::OnAutoComplete) should probably be removed
    // from the interface.

    return NS_OK;
}

/**
 * Messages received are passed back via this function.
 *
 * @arg aMessage  The message that was returned, NULL if none was.
 *
 * void OnLDAPMessage (in nsILDAPMessage aMessage)
 */
NS_IMETHODIMP 
nsLDAPAutoCompleteSession::OnLDAPMessage(nsILDAPMessage *aMessage)
{
    int32_t messageType;

    // Just in case.
    // XXXdmose the semantics of NULL are currently undefined, but are likely
    // to be defined once we insert timeout handling code into the XPCOM SDK
    // At that time we should figure out if this still the right thing to do.
    if (!aMessage) {
        return NS_OK;
    }

    // Figure out what sort of message was returned.
    nsresult rv = aMessage->GetType(&messageType);
    if (NS_FAILED(rv)) {
        NS_ERROR("nsLDAPAutoCompleteSession::OnLDAPMessage(): unexpected "
                 "error in aMessage->GetType()");
       
        // Don't call FinishAutoCompleteLookup(), as this could conceivably 
        // be an anomaly, and perhaps the next message will be ok. If this
        // really was a problem, this search should eventually get
        // reaped by a timeout (once that code gets implemented).
        return NS_ERROR_UNEXPECTED;
    }

    // If this message is not associated with the current operation,
    // discard it, since it is probably from a previous (aborted)
    // operation.
    bool isCurrent;
    rv = IsMessageCurrent(aMessage, &isCurrent);
    if (NS_FAILED(rv)) {
        // IsMessageCurrent will have logged any necessary errors
        return rv;
    }
    if ( ! isCurrent ) {
        PR_LOG(sLDAPAutoCompleteLogModule, PR_LOG_DEBUG,
               ("nsLDAPAutoCompleteSession::OnLDAPMessage(): received message "
                "from operation other than current one; discarded"));
        return NS_OK;
    }

    // XXXdmose - we may want a small state machine either here or
    // or in the nsLDAPConnection object, to make sure that things are
    // happening in the right order and timing out appropriately. This will
    // certainly depend on how timeouts are implemented, and how binding
    // gets is dealt with by nsILDAPService. Also need to deal with the case
    // where a bind callback happens after onStopLookup was called.
    switch (messageType) {

    case nsILDAPMessage::RES_BIND:

        // A bind has completed
        if (mState != BINDING) {
            PR_LOG(sLDAPAutoCompleteLogModule, PR_LOG_DEBUG, 
                   ("nsLDAPAutoCompleteSession::OnLDAPMessage(): LDAP bind "
                    "entry returned after OnStopLookup() called; ignoring"));

            // XXXdmose when nsLDAPService integration happens, need to make
            // sure that the possibility of having an already bound
            // connection, due to a previously unaborted bind, doesn't cause
            // any problems.

            return NS_OK;
        }

        rv = OnLDAPMessageBind(aMessage);
        if (NS_FAILED(rv)) {
          mState = UNBOUND;
          FinishAutoCompleteLookup(nsIAutoCompleteStatus::failureItems, 
                                   rv, UNBOUND);
        }
        else
          mState = SEARCHING;

        return rv;

    case nsILDAPMessage::RES_SEARCH_ENTRY:
        
        // Ignore this if OnStopLookup was already called.
        if (mState != SEARCHING) {
            PR_LOG(sLDAPAutoCompleteLogModule, PR_LOG_DEBUG, 
                   ("nsLDAPAutoCompleteSession::OnLDAPMessage(): LDAP search "
                    "entry returned after OnStopLoookup() called; ignoring"));
            return NS_OK;
        }

        // A search entry has been returned.
        return OnLDAPSearchEntry(aMessage);

    case nsILDAPMessage::RES_SEARCH_RESULT:

        // Ignore this if OnStopLookup was already called.
        if (mState != SEARCHING) {
            PR_LOG(sLDAPAutoCompleteLogModule, PR_LOG_DEBUG,
                   ("nsLDAPAutoCompleteSession::OnLDAPMessage(): LDAP search "
                    "result returned after OnStopLookup called; ignoring"));
            return NS_OK;
        }

        // The search is finished; we're all done.
        return OnLDAPSearchResult(aMessage);

    default:
        
        // Given the LDAP operations nsLDAPAutoCompleteSession uses, we should
        // never get here. If we do get here in a release build, it's
        // probably a bug, but maybe it's the LDAP server doing something
        // weird. Might as well try and continue anyway. The session should
        // eventually get reaped by the timeout code, if necessary.
        //
        NS_ERROR("nsLDAPAutoCompleteSession::OnLDAPMessage(): unexpected "
                 "LDAP message received");
        return NS_OK;
    }
}

void
nsLDAPAutoCompleteSession::InitFailed(bool aCancelled)
{
  FinishAutoCompleteLookup(nsIAutoCompleteStatus::failureItems, NS_OK,
                           UNBOUND);
}

// void onLDAPInit (in nsresult aStatus);
NS_IMETHODIMP
nsLDAPAutoCompleteSession::OnLDAPInit(nsILDAPConnection *aConn, nsresult aStatus)
{
  nsresult rv = nsAbLDAPListenerBase::OnLDAPInit(aConn, aStatus);

  if (NS_SUCCEEDED(rv))
    mState = BINDING;

  return rv;
}

nsresult
nsLDAPAutoCompleteSession::OnLDAPSearchEntry(nsILDAPMessage *aMessage)
{
    PR_LOG(sLDAPAutoCompleteLogModule, PR_LOG_DEBUG, 
           ("nsLDAPAutoCompleteSession::OnLDAPSearchEntry entered\n"));

    // Make sure this is only getting called after DoTask has
    // initialized the result set.
    NS_ASSERTION(mResultsArray,
                 "nsLDAPAutoCompleteSession::OnLDAPSearchEntry(): "
                 "mResultsArrayItems is uninitialized");

    // Errors in this method return an error (which ultimately gets
    // ignored, since this is being called through an async proxy).
    // But the important thing is that we're bailing out here rather
    // than trying to generate a bogus nsIAutoCompleteItem. Also note
    // that FinishAutoCompleteLookup is _NOT_ being called here, because
    // this error may just have to do with this particular item.

    // Generate an autocomplete item from this message by calling the
    // formatter.
    nsCOMPtr<nsIAutoCompleteItem> item;
    nsresult rv = mFormatter->Format(aMessage, getter_AddRefs(item));
    if (NS_FAILED(rv)) {
        PR_LOG(sLDAPAutoCompleteLogModule, PR_LOG_DEBUG,
               ("nsLDAPAutoCompleteSession::OnLDAPSearchEntry(): "
                "mFormatter->Format() failed"));
        return NS_ERROR_FAILURE;
    }

  nsString itemValue;
  item->GetValue(itemValue);

  uint32_t nbrOfItems;
  rv = mResultsArray->Count(&nbrOfItems);
  if (NS_FAILED(rv)) {
    NS_ERROR("nsLDAPAutoCompleteSession::OnLDAPSearchEntry(): "
             "mResultsArray->Count() failed");
    return NS_ERROR_FAILURE;
  }

  int32_t insertPosition = 0;

  nsCOMPtr<nsIAutoCompleteItem> currentItem;
  for (; insertPosition < nbrOfItems; insertPosition++) {
    currentItem = do_QueryElementAt(mResultsArray, insertPosition, &rv);

    if (NS_FAILED(rv))
      continue;

    nsString currentItemValue;
    currentItem->GetValue(currentItemValue);
    if (itemValue < currentItemValue) 
      break;
  }

  mResultsArray->InsertElementAt(item, insertPosition);

  // Remember that something has been returned.
  mEntriesReturned++;

  return NS_OK;
}

nsresult
nsLDAPAutoCompleteSession::OnLDAPSearchResult(nsILDAPMessage *aMessage)
{
    nsresult rv;        // Temp for return vals

    PR_LOG(sLDAPAutoCompleteLogModule, PR_LOG_DEBUG, 
           ("nsLDAPAutoCompleteSession::OnLDAPSearchResult entered\n"));

    // Figure out if we succeeded or failed, and set the status
    // and default index appropriately.
    AutoCompleteStatus status;
    int32_t lderrno;

    if (mEntriesReturned) {

        status = nsIAutoCompleteStatus::matchFound;

        // There's at least one match, so the default index should
        // point to the first thing here.  This ensures that if the local
        // addressbook autocomplete session only found foo@local.domain,
        // this will be given preference.
        rv = mResults->SetDefaultItemIndex(0);
        if (NS_FAILED(rv)) {
            NS_ERROR("nsLDAPAutoCompleteSession::OnLDAPSearchResult(): "
                     "mResults->SetDefaultItemIndex(0) failed");
            FinishAutoCompleteLookup(nsIAutoCompleteStatus::failureItems, rv, 
                                     BOUND);
        }
    } else {
        // Note that we only look at the error code if there are no results for
        // this session; if we got results and then an error happened, this
        // is ignored, in part because it seems likely to be confusing to the
        // user, and in part because it is likely to be scrolled out of view
        // anyway.
        aMessage->GetErrorCode(&lderrno);
        if (lderrno != nsILDAPErrors::SUCCESS) {

            PR_LOG(sLDAPAutoCompleteLogModule, PR_LOG_DEBUG, 
                   ("nsLDAPAutoCompleteSession::OnLDAPSearchResult(): "
                    "lderrno=%d\n", lderrno));
            FinishAutoCompleteLookup(nsIAutoCompleteStatus::failureItems, 
                                     NS_ERROR_GENERATE_FAILURE(
                                         NS_ERROR_MODULE_LDAP, lderrno), 
                                     BOUND);
            return NS_OK;
        }

        // We could potentially keep track of non-fatal errors to the
        // search, and if there has been more than 1, and there are no entries,
        // we could return |failed| instead of |noMatch|. It's unclear to me
        // that this actually buys us anything though.
        status = nsIAutoCompleteStatus::noMatch;
    }

    // Call the mListener's OnAutoComplete and clean up
    //
    // XXXdmose should we really leave the connection BOUND here?
    FinishAutoCompleteLookup(status, NS_OK, BOUND);

    return NS_OK;
}

nsresult
nsLDAPAutoCompleteSession::DoTask()
{
    nsresult rv; // temp for xpcom return values

    PR_LOG(sLDAPAutoCompleteLogModule, PR_LOG_DEBUG, 
           ("nsLDAPAutoCompleteSession::DoTask entered\n"));

    // Create and initialize an LDAP operation (to be used for the search).
    mOperation = 
        do_CreateInstance("@mozilla.org/network/ldap-operation;1", &rv);

    if (NS_FAILED(rv)) {
        NS_ERROR("nsLDAPAutoCompleteSession::DoTask(): couldn't "
                 "create @mozilla.org/network/ldap-operation;1");

        FinishAutoCompleteLookup(nsIAutoCompleteStatus::failureItems, rv, 
                                 BOUND);
        return NS_ERROR_FAILURE;
    }

    // Initialize the LDAP operation object.
    rv = mOperation->Init(mConnection, this, nullptr);
    if (NS_FAILED(rv)) {
        NS_ERROR("nsLDAPAutoCompleteSession::DoTask(): couldn't "
                 "initialize LDAP operation");
        FinishAutoCompleteLookup(nsIAutoCompleteStatus::failureItems, rv, 
                                 BOUND);
        return NS_ERROR_UNEXPECTED;
    }

    // Set the server and client controls on the operation.
    if (mSearchServerControls) {
        rv = mOperation->SetServerControls(mSearchServerControls);
        if (NS_FAILED(rv)) {
            NS_ERROR("nsLDAPAutoCompleteSession::DoTask(): couldn't "
                     "initialize LDAP search operation server controls");
            FinishAutoCompleteLookup(nsIAutoCompleteStatus::failureItems, rv, 
                                     BOUND);
            return NS_ERROR_UNEXPECTED;
        }
    }
    if (mSearchClientControls) {
        rv = mOperation->SetClientControls(mSearchClientControls);
        if (NS_FAILED(rv)) {
            NS_ERROR("nsLDAPAutoCompleteSession::DoTask(): couldn't "
                     "initialize LDAP search operation client controls");
            FinishAutoCompleteLookup(nsIAutoCompleteStatus::failureItems, rv, 
                                     BOUND);
            return NS_ERROR_UNEXPECTED;
        }
    }

    // Get the search filter associated with the directory server url;
    // it will be ANDed with the rest of the search filter that we're using.
    nsCAutoString urlFilter;
    rv = mDirectoryUrl->GetFilter(urlFilter);
    if ( NS_FAILED(rv) ){
        FinishAutoCompleteLookup(nsIAutoCompleteStatus::failureItems, rv,
                                 BOUND);
        return NS_ERROR_UNEXPECTED;
    }

    // Get the LDAP service, since createFilter is called through it.
    nsCOMPtr<nsILDAPService> ldapSvc = do_GetService(
        "@mozilla.org/network/ldap-service;1", &rv);
    if (NS_FAILED(rv)) {
        NS_ERROR("nsLDAPAutoCompleteSession::DoTask(): couldn't "
                 "get @mozilla.org/network/ldap-service;1");
        FinishAutoCompleteLookup(nsIAutoCompleteStatus::failureItems, rv,
                                 BOUND);
        return NS_ERROR_FAILURE;
    }

    // If urlFilter is unset (or set to the default "objectclass=*"), there's
    // no need to AND in an empty search term, so leave prefix and suffix empty.
    nsCAutoString prefix, suffix;
    if (urlFilter.Length() && !urlFilter.EqualsLiteral("(objectclass=*)")) {

        // If urlFilter isn't parenthesized, we need to add in parens so that
        // the filter works as a term to &
        if (urlFilter[0] != '(') {
            prefix.AssignLiteral("(&(");
            prefix.Append(urlFilter);
            prefix.AppendLiteral(")");
        } else {
            prefix.AssignLiteral("(&");
            prefix.Append(urlFilter);
        }
        
        suffix = ')';
    }

    // Generate an LDAP search filter from mFilterTemplate. If it's unset,
    // use the default.
#define MAX_AUTOCOMPLETE_FILTER_SIZE 1024
    nsCAutoString searchFilter;
    rv = ldapSvc->CreateFilter(MAX_AUTOCOMPLETE_FILTER_SIZE,
                               mFilterTemplate,
                               prefix, suffix, EmptyCString(), 
                               NS_ConvertUTF16toUTF8(mSearchString),
                               searchFilter);
    if (NS_FAILED(rv)) {
        switch(rv) {

        case NS_ERROR_OUT_OF_MEMORY:
            FinishAutoCompleteLookup(nsIAutoCompleteStatus::failureItems, rv,
                                     BOUND);
            return rv;

        case NS_ERROR_NOT_AVAILABLE:
            PR_LOG(sLDAPAutoCompleteLogModule, PR_LOG_DEBUG, 
                   ("nsLDAPAutoCompleteSession::DoTask(): "
                    "createFilter generated filter longer than max filter "
                    "size of %d", MAX_AUTOCOMPLETE_FILTER_SIZE));
            FinishAutoCompleteLookup(nsIAutoCompleteStatus::failureItems, rv,
                                     BOUND);
            return rv;

        case NS_ERROR_INVALID_ARG:
        case NS_ERROR_UNEXPECTED:
        default:

            // All this stuff indicates code bugs.
            NS_ERROR("nsLDAPAutoCompleteSession::DoTask(): "
                     "createFilter returned unexpected value");
            FinishAutoCompleteLookup(nsIAutoCompleteStatus::failureItems, rv, 
                                     BOUND);
            return NS_ERROR_UNEXPECTED;
        }

    }

    // If the results array for this search hasn't already been created, do
    // so now. Note that we don't return ::failureItems here, because if
    // there's no array, there's nowhere to put the items.
    rv = CreateResultsArray();
    if (NS_FAILED(rv)) {
        FinishAutoCompleteLookup(nsIAutoCompleteStatus::failed, rv, BOUND);
    }

    // Nothing returned yet!
    mEntriesReturned = 0;
    
    // Get the base dn to search
    nsCAutoString dn;
    rv = mDirectoryUrl->GetDn(dn);
    if ( NS_FAILED(rv) ){
        FinishAutoCompleteLookup(nsIAutoCompleteStatus::failureItems, rv,
                                 BOUND);
        return NS_ERROR_UNEXPECTED;
    }

    // And the scope
    int32_t scope;
    rv = mDirectoryUrl->GetScope(&scope);
    if ( NS_FAILED(rv) ){
        mState = BOUND;
        FinishAutoCompleteLookup(nsIAutoCompleteStatus::failureItems, rv, 
                                 BOUND);
        return NS_ERROR_UNEXPECTED;
    }

    // Take the relevant controls on this object and set them
    // on the operation
    rv = mOperation->SetServerControls(mSearchServerControls.get());
    if ( NS_FAILED(rv) ){
        mState = BOUND;
        FinishAutoCompleteLookup(nsIAutoCompleteStatus::failureItems, rv, 
                                 BOUND);
        return NS_ERROR_UNEXPECTED;
    }

    rv = mOperation->SetClientControls(mSearchClientControls.get());
    if ( NS_FAILED(rv) ){
        mState = BOUND;
        FinishAutoCompleteLookup(nsIAutoCompleteStatus::failureItems, rv, 
                                 BOUND);
        return NS_ERROR_UNEXPECTED;
    }

    // Time to kick off the search.
    //
    // XXXdmose what about timeouts? 
    //
    rv = mOperation->SearchExt(dn, scope, searchFilter, mSearchAttrs, 0,
                               mMaxHits);
    if (NS_FAILED(rv)) {
        switch(rv) {

        case NS_ERROR_LDAP_ENCODING_ERROR:
            PR_LOG(sLDAPAutoCompleteLogModule, PR_LOG_DEBUG, 
                   ("nsLDAPAutoCompleteSession::DoTask(): SearchExt "
                    "returned NS_ERROR_LDAP_ENCODING_ERROR"));
            FinishAutoCompleteLookup(nsIAutoCompleteStatus::failureItems, rv, 
                                     BOUND);
            return NS_OK;

        case NS_ERROR_LDAP_FILTER_ERROR:
            PR_LOG(sLDAPAutoCompleteLogModule, PR_LOG_DEBUG, 
                   ("nsLDAPAutoCompleteSession::DoTask(): SearchExt "
                    "returned NS_ERROR_LDAP_FILTER_ERROR"));
            FinishAutoCompleteLookup(nsIAutoCompleteStatus::failureItems, rv, 
                                     BOUND);
            return NS_OK;

        case NS_ERROR_LDAP_SERVER_DOWN:
            // XXXdmose discuss with leif how to handle this in general in the
            // LDAP XPCOM SDK.

            PR_LOG(sLDAPAutoCompleteLogModule, PR_LOG_DEBUG, 
                   ("nsLDAPAutoCompleteSession::DoTask(): SearchExt "
                    "returned NS_ERROR_LDAP_SERVER_DOWN"));
            FinishAutoCompleteLookup(nsIAutoCompleteStatus::failureItems, rv,
                                     UNBOUND);
            return NS_OK;

        case NS_ERROR_OUT_OF_MEMORY:
            FinishAutoCompleteLookup(nsIAutoCompleteStatus::failureItems, rv, 
                                     BOUND);
            return NS_ERROR_OUT_OF_MEMORY;

        case NS_ERROR_LDAP_NOT_SUPPORTED:
        case NS_ERROR_NOT_INITIALIZED:        
        case NS_ERROR_INVALID_ARG:
        default:

            // All this stuff indicates code bugs.
            NS_ERROR("nsLDAPAutoCompleteSession::DoTask(): SearchExt "
                     "returned unexpected value");
            FinishAutoCompleteLookup(nsIAutoCompleteStatus::failureItems, rv, 
                                     BOUND);
            return NS_ERROR_UNEXPECTED;
        }
    }

    return NS_OK;
}

// XXXdmose - stopgap routine until nsLDAPService is working
//
nsresult
nsLDAPAutoCompleteSession::InitConnection()
{
    nsresult rv;        // temp for xpcom return values
    NS_ASSERTION(!mConnection, "in InitConnection w/ existing connection");

    // Create an LDAP connection
    nsCOMPtr<nsILDAPConnection> connection =
      do_CreateInstance("@mozilla.org/network/ldap-connection;1", &rv);
    if (NS_FAILED(rv)) {
        NS_ERROR("nsLDAPAutoCompleteSession::InitConnection(): could "
                 "not create @mozilla.org/network/ldap-connection;1");
        FinishAutoCompleteLookup(nsIAutoCompleteStatus::failureItems, rv, 
                                 UNBOUND);
        return NS_ERROR_FAILURE;
    }

    NS_ADDREF(mConnection = connection);

    // Have we been properly initialized?
    if (!mDirectoryUrl) {
        NS_ERROR("nsLDAPAutoCompleteSession::InitConnection(): mDirectoryUrl "
                 "is NULL");
        FinishAutoCompleteLookup(nsIAutoCompleteStatus::failureItems, rv, 
                                 UNBOUND);
        return NS_ERROR_NOT_INITIALIZED;
    }

    // Initialize the connection. This will cause an asynchronous DNS
    // lookup to occur, and we'll finish the binding of the connection
    // in the OnLDAPInit() listener function.
    rv = mConnection->Init(mDirectoryUrl, mLogin, this, nullptr, mVersion);
    if (NS_FAILED(rv)) {
        switch (rv) {

        case NS_ERROR_OUT_OF_MEMORY:
        case NS_ERROR_NOT_AVAILABLE:
        case NS_ERROR_FAILURE:
            PR_LOG(sLDAPAutoCompleteLogModule, PR_LOG_DEBUG, 
                   ("nsLDAPAutoCompleteSession::InitConnection(): mSimpleBind "
                    "failed, rv = 0x%lx", rv));
            FinishAutoCompleteLookup(nsIAutoCompleteStatus::failureItems, rv, 
                                     UNBOUND);
            return rv;

        case NS_ERROR_ILLEGAL_VALUE:
        default:
            FinishAutoCompleteLookup(nsIAutoCompleteStatus::failureItems, rv, 
                                     UNBOUND);
            return NS_ERROR_UNEXPECTED;
        }
    }

    // Set our state
    mState = INITIALIZING;

    return NS_OK;
}

nsresult
nsLDAPAutoCompleteSession::CreateResultsArray(void)
{
    nsresult rv;

    // Create a result set
    mResults = do_CreateInstance(NS_AUTOCOMPLETERESULTS_CONTRACTID, &rv);

    if (NS_FAILED(rv)) {
        NS_ERROR("nsLDAPAutoCompleteSession::DoTask() couldn't"
                 " create " NS_AUTOCOMPLETERESULTS_CONTRACTID);
        return NS_ERROR_FAILURE;
    }

    // This seems to be necessary for things to work, though I'm not sure
    // why that's true.
    rv = mResults->SetSearchString(mSearchString.get());
    if (NS_FAILED(rv)) {
        NS_ERROR("nsLDAPAutoCompleteSession::OnLDAPSearchResult(): couldn't "
                 "set search string in results object");
        return NS_ERROR_FAILURE;
    }

    // Get a pointer to the array in question now, so that we don't have to
    // keep re-fetching it every time an entry callback happens.
    rv = mResults->GetItems(getter_AddRefs(mResultsArray));
    if (NS_FAILED(rv)) {
        NS_ERROR("nsLDAPAutoCompleteSession::DoTask() couldn't "
                 "get results array.");
        return NS_ERROR_FAILURE;
    }

    return NS_OK;
}

void
nsLDAPAutoCompleteSession::FinishAutoCompleteLookup(
    AutoCompleteStatus aACStatus, const nsresult aResult,
    enum SessionState aEndState)
{
    nsCOMPtr<nsIAutoCompleteItem> errorItem; // pointer to item we may create
    nsresult rv; // temp for return value 

    // If there's a listener, inform the listener that the search is over.
    rv = NS_OK;
    if (mListener) {
        
        switch (aACStatus) {

        case nsIAutoCompleteStatus::matchFound:
            rv = mListener->OnAutoComplete(mResults, aACStatus);
            break;

        case nsIAutoCompleteStatus::failureItems:
            // If the results array hasn't already been created, make one
            // to return the error message.  If there's an error, fallback
            // to ::failed
            if (!mResults) {
                rv = CreateResultsArray();
                if (NS_FAILED(rv)) {
                    NS_ERROR("nsLDAPAutoCompleteSession::"
                             "FinishAutoCompleteLookup():"
                             " CreateResultsArray() failed");
                    rv = mListener->OnAutoComplete(0, 
                                                nsIAutoCompleteStatus::failed);
                    break;
                }
            }

            // Create the error item.
            rv = mFormatter->FormatException(mState, aResult,
                                             getter_AddRefs(errorItem));
            if (NS_SUCCEEDED(rv)) {

                // Try and append the error item; falling back to ::failed
                // if there's a problem.
                //
                rv = mResultsArray->AppendElement(errorItem);
                if (NS_FAILED(rv)) {
                    NS_ERROR("nsLDAPAutoCompleteSession::"
                             "FinishAutoCompleteLookup():"
                             " mItems->AppendElement() failed");
                    rv = mListener->OnAutoComplete(0, 
                                                nsIAutoCompleteStatus::failed);
                    break;
                } 

                // We don't want the autocomplete widget trying to
                // automagically use the error item for anything. If
                // something goes wrong here, continue on anyway.
                //
                (void)mResults->SetDefaultItemIndex(-1);

                rv = mListener->OnAutoComplete(mResults, 
                                          nsIAutoCompleteStatus::failureItems);
                break;
            } 

            // Fallback to ::failed
            NS_ERROR("nsLDAPAutoCompleteSession::FinishAutoCompleteLookup(): "
                     "error calling FormatException()");

            rv = mListener->OnAutoComplete(0, nsIAutoCompleteStatus::failed);
            break;
        
        case nsIAutoCompleteStatus::failed:
        default:
            rv = mListener->OnAutoComplete(0, aACStatus);
            break;
        }

    } else {
        // If there's no listener, something's wrong.
        NS_ERROR("nsLDAPAutoCompleteSession::FinishAutoCompleteLookup(): "
                 "called with mListener unset!");
    }

    if (NS_FAILED(rv)) {

        // There's nothing we can actually do here other than warn.
        NS_WARNING("nsLDAPAutoCompleteSession::FinishAutoCompleteLookup(): "
                   "error calling mListener->OnAutoComplete()");
    }

    // Set the state appropriately.
    mState = aEndState;

    // We're done with various things; cause nsCOMPtr to release them.
    mResultsArray = 0;
    mResults = 0;
    mListener = 0;
    mOperation = 0;

    // If we are unbound, drop the connection (if any).
    if (mState == UNBOUND) {
        NS_IF_RELEASE(mConnection);
    }
}

// methods for nsILDAPAutoCompleteSession

// attribute AUTF8String searchFilter;
NS_IMETHODIMP 
nsLDAPAutoCompleteSession::GetFilterTemplate(nsACString & aFilterTemplate)
{
    aFilterTemplate.Assign(mFilterTemplate);

    return NS_OK;
}
NS_IMETHODIMP 
nsLDAPAutoCompleteSession::SetFilterTemplate(const nsACString & aFilterTemplate)
{
    mFilterTemplate.Assign(aFilterTemplate);

    return NS_OK;
}


// attribute long maxHits;
NS_IMETHODIMP 
nsLDAPAutoCompleteSession::GetMaxHits(int32_t *aMaxHits)
{
    if (!aMaxHits) {
        return NS_ERROR_NULL_POINTER;
    }

    *aMaxHits = mMaxHits;
    return NS_OK;
}
NS_IMETHODIMP 
nsLDAPAutoCompleteSession::SetMaxHits(int32_t aMaxHits)
{
    if ( aMaxHits < -1 || aMaxHits > 65535) {
        return NS_ERROR_ILLEGAL_VALUE;
    }

    mMaxHits = aMaxHits;
    return NS_OK;
}

// attribute nsILDAPURL serverURL; 
NS_IMETHODIMP 
nsLDAPAutoCompleteSession::GetServerURL(nsILDAPURL * *aServerURL)
{
    if (! aServerURL ) {
        return NS_ERROR_NULL_POINTER;
    }
    
    NS_IF_ADDREF(*aServerURL = mDirectoryUrl);

    return NS_OK;
}
NS_IMETHODIMP 
nsLDAPAutoCompleteSession::SetServerURL(nsILDAPURL * aServerURL)
{
    if (! aServerURL ) {
        return NS_ERROR_NULL_POINTER;
    }

    mDirectoryUrl = aServerURL;

    // The following line will cause the next call to OnStartLookup to
    // call InitConnection again. This will reinitialize all the relevant
    // member variables and kick off an LDAP bind. By virtue of the magic of
    // nsCOMPtrs, doing this will cause all the nsISupports-based stuff to
    // be Released, which will eventually result in the old connection being
    // destroyed, and the destructor for that calls ldap_unbind()
    mState = UNBOUND;

    return NS_OK;
}

// attribute unsigned long minStringLength
NS_IMETHODIMP
nsLDAPAutoCompleteSession::GetMinStringLength(uint32_t *aMinStringLength)
{
    if (!aMinStringLength) {
        return NS_ERROR_NULL_POINTER;
    }

    *aMinStringLength = mMinStringLength;
    return NS_OK;
}
NS_IMETHODIMP
nsLDAPAutoCompleteSession::SetMinStringLength(uint32_t aMinStringLength)
{
    mMinStringLength = aMinStringLength;

    return NS_OK;
}

// attribute unsigned long cjkMinStringLength
NS_IMETHODIMP
nsLDAPAutoCompleteSession::GetCjkMinStringLength(uint32_t *aCjkMinStringLength)
{
    if (!aCjkMinStringLength) {
        return NS_ERROR_NULL_POINTER;
    }

    *aCjkMinStringLength = mCjkMinStringLength;
    return NS_OK;
}
NS_IMETHODIMP
nsLDAPAutoCompleteSession::SetCjkMinStringLength(uint32_t aCjkMinStringLength)
{
    mCjkMinStringLength = aCjkMinStringLength;

    return NS_OK;
}

// Check to see if the message returned is related to our current operation
// if there is no current operation, it's not. :-)
nsresult 
nsLDAPAutoCompleteSession::IsMessageCurrent(nsILDAPMessage *aMessage, 
                                            bool *aIsCurrent)
{
    // If there's no operation, this message must be stale (ie non-current).
    if ( !mOperation ) {
        *aIsCurrent = false;
        return NS_OK;
    }

    // Get the message id from the current operation.
    int32_t currentId;
    nsresult rv = mOperation->GetMessageID(&currentId);
    if (NS_FAILED(rv)) {
        PR_LOG(sLDAPAutoCompleteLogModule, PR_LOG_DEBUG, 
               ("nsLDAPAutoCompleteSession::IsMessageCurrent(): unexpected "
                "error 0x%lx calling mOperation->GetMessageId()", rv));
        return NS_ERROR_UNEXPECTED;
    }

    // Get the message operation from the message.
    nsCOMPtr<nsILDAPOperation> msgOp;
    rv = aMessage->GetOperation(getter_AddRefs(msgOp));
    if (NS_FAILED(rv)) {
        PR_LOG(sLDAPAutoCompleteLogModule, PR_LOG_DEBUG, 
               ("nsLDAPAutoCompleteSession::IsMessageCurrent(): unexpected "
                "error 0x%lx calling aMessage->GetOperation()", rv));
        return NS_ERROR_UNEXPECTED;
    }

    // Get the message operation id from the message operation.
    int32_t msgOpId;
    rv = msgOp->GetMessageID(&msgOpId);
    if (NS_FAILED(rv)) {
        PR_LOG(sLDAPAutoCompleteLogModule, PR_LOG_DEBUG, 
               ("nsLDAPAutoCompleteSession::IsMessageCurrent(): unexpected "
                "error 0x%lx calling msgOp->GetMessageId()", rv));
        return NS_ERROR_UNEXPECTED;
    }
    
    *aIsCurrent = (msgOpId == currentId);

    return NS_OK;
}

// attribute nsILDAPAutoCompFormatter formatter
NS_IMETHODIMP
nsLDAPAutoCompleteSession::GetFormatter(nsILDAPAutoCompFormatter* *aFormatter)
{
    if (! aFormatter ) {
        return NS_ERROR_NULL_POINTER;
    }
    
    NS_IF_ADDREF(*aFormatter = mFormatter);

    return NS_OK;
}
NS_IMETHODIMP 
nsLDAPAutoCompleteSession::SetFormatter(nsILDAPAutoCompFormatter* aFormatter)
{
    if (! aFormatter ) {
        return NS_ERROR_NULL_POINTER;
    }

    NS_ASSERTION(mState == UNBOUND || mState == BOUND, 
                 "nsLDAPAutoCompleteSession::SetFormatter was called when "
                 "mState was set to something other than BOUND or UNBOUND");

    mFormatter = aFormatter;

    // Get and cache the attributes that will be used to do lookups.
    nsresult rv = mFormatter->GetAttributes(mSearchAttrs);
    if (NS_FAILED(rv)) {
        NS_ERROR("nsLDAPAutoCompleteSession::SetFormatter(): "
                 " mFormatter->GetAttributes failed");
        return NS_ERROR_FAILURE;
    }
    
    return NS_OK;
}

NS_IMETHODIMP
nsLDAPAutoCompleteSession::SetLogin(const nsACString & aLogin)
{
    mLogin = aLogin;
    return NS_OK;
}
NS_IMETHODIMP
nsLDAPAutoCompleteSession::GetLogin(nsACString & aLogin) 
{
    aLogin = mLogin;
    return NS_OK;
}

// attribute ACString saslMechanism

NS_IMETHODIMP
nsLDAPAutoCompleteSession::SetSaslMechanism(const nsACString & aSaslMechanism)
{
    mSaslMechanism.Assign(aSaslMechanism);
    return NS_OK;
}
NS_IMETHODIMP
nsLDAPAutoCompleteSession::GetSaslMechanism(nsACString & aSaslMechanism)
{
    aSaslMechanism.Assign(mSaslMechanism);
    return NS_OK;
}

// attribute unsigned long version;
NS_IMETHODIMP 
nsLDAPAutoCompleteSession::GetVersion(uint32_t *aVersion)
{
    if (!aVersion) {
        return NS_ERROR_NULL_POINTER;
    }

    *aVersion = mVersion;
    return NS_OK;
}
NS_IMETHODIMP 
nsLDAPAutoCompleteSession::SetVersion(uint32_t aVersion)
{
    if ( mVersion != nsILDAPConnection::VERSION2 && 
         mVersion != nsILDAPConnection::VERSION3) {
        return NS_ERROR_ILLEGAL_VALUE;
    }

    mVersion = aVersion;
    return NS_OK;
}

NS_IMETHODIMP
nsLDAPAutoCompleteSession::GetSearchServerControls(nsIMutableArray **aControls)
{
    NS_IF_ADDREF(*aControls = mSearchServerControls);
    return NS_OK;
}

NS_IMETHODIMP
nsLDAPAutoCompleteSession::SetSearchServerControls(nsIMutableArray *aControls)
{
    mSearchServerControls = aControls;
    return NS_OK;
}

NS_IMETHODIMP
nsLDAPAutoCompleteSession::GetSearchClientControls(nsIMutableArray **aControls)
{
    NS_IF_ADDREF(*aControls = mSearchClientControls);
    return NS_OK;
}

NS_IMETHODIMP 
nsLDAPAutoCompleteSession::SetSearchClientControls(nsIMutableArray *aControls)
{
    mSearchClientControls = aControls;
    return NS_OK;
}
