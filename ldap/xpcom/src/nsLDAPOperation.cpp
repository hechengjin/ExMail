/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsLDAPInternal.h"
#include "nsLDAPOperation.h"
#include "nsLDAPBERValue.h"
#include "nsILDAPMessage.h"
#include "nsILDAPModification.h"
#include "nsIComponentManager.h"
#include "nspr.h"
#include "nsISimpleEnumerator.h"
#include "nsLDAPControl.h"
#include "nsILDAPErrors.h"
#include "nsIClassInfoImpl.h"
#include "nsIAuthModule.h"
#include "nsArrayUtils.h"
#include "nsMemory.h"

// Helper function
static nsresult TranslateLDAPErrorToNSError(const int ldapError)
{
  switch (ldapError) {
  case LDAP_SUCCESS:
    return NS_OK;

  case LDAP_ENCODING_ERROR:
    return NS_ERROR_LDAP_ENCODING_ERROR;

  case LDAP_CONNECT_ERROR:
    return NS_ERROR_LDAP_CONNECT_ERROR;

  case LDAP_SERVER_DOWN:
    return NS_ERROR_LDAP_SERVER_DOWN;

  case LDAP_NO_MEMORY:
    return NS_ERROR_OUT_OF_MEMORY;

  case LDAP_NOT_SUPPORTED:
    return NS_ERROR_LDAP_NOT_SUPPORTED;

  case LDAP_PARAM_ERROR:
    return NS_ERROR_INVALID_ARG;

  case LDAP_FILTER_ERROR:
    return NS_ERROR_LDAP_FILTER_ERROR;

  default:
    PR_LOG(gLDAPLogModule, PR_LOG_ERROR,
           ("TranslateLDAPErrorToNSError: "
            "Do not know how to translate LDAP error: 0x%x", ldapError));
    return NS_ERROR_UNEXPECTED;
  }
}


// constructor
nsLDAPOperation::nsLDAPOperation()
{
}

// destructor
nsLDAPOperation::~nsLDAPOperation()
{
}

NS_IMPL_CLASSINFO(nsLDAPOperation, NULL, nsIClassInfo::THREADSAFE,
                  NS_LDAPOPERATION_CID)

NS_IMPL_THREADSAFE_ADDREF(nsLDAPOperation)
NS_IMPL_THREADSAFE_RELEASE(nsLDAPOperation)
NS_INTERFACE_MAP_BEGIN(nsLDAPOperation)
  NS_INTERFACE_MAP_ENTRY(nsILDAPOperation)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsILDAPOperation)
  NS_IMPL_QUERY_CLASSINFO(nsLDAPOperation)
NS_INTERFACE_MAP_END_THREADSAFE
NS_IMPL_CI_INTERFACE_GETTER1(nsLDAPOperation, nsILDAPOperation)

/**
 * Initializes this operation.  Must be called prior to use.
 *
 * @param aConnection connection this operation should use
 * @param aMessageListener where are the results are called back to.
 */
NS_IMETHODIMP
nsLDAPOperation::Init(nsILDAPConnection *aConnection,
                      nsILDAPMessageListener *aMessageListener,
                      nsISupports *aClosure)
{
    if (!aConnection) {
        return NS_ERROR_ILLEGAL_VALUE;
    }

    // so we know that the operation is not yet running (and therefore don't
    // try and call ldap_abandon_ext() on it) or remove it from the queue.
    //
    mMsgID = 0;

    // set the member vars
    //
    mConnection = static_cast<nsLDAPConnection*>(aConnection);
    mMessageListener = aMessageListener;
    mClosure = aClosure;

    // cache the connection handle
    //
    mConnectionHandle =
        mConnection->mConnectionHandle;

    return NS_OK;
}

NS_IMETHODIMP
nsLDAPOperation::GetClosure(nsISupports **_retval)
{
    if (!_retval) {
        return NS_ERROR_ILLEGAL_VALUE;
    }
    NS_IF_ADDREF(*_retval = mClosure);
    return NS_OK;
}

NS_IMETHODIMP
nsLDAPOperation::SetClosure(nsISupports *aClosure)
{
    mClosure = aClosure;
    return NS_OK;
}

NS_IMETHODIMP
nsLDAPOperation::GetConnection(nsILDAPConnection* *aConnection)
{
    if (!aConnection) {
        return NS_ERROR_ILLEGAL_VALUE;
    }

    *aConnection = mConnection;
    NS_IF_ADDREF(*aConnection);

    return NS_OK;
}

void
nsLDAPOperation::Clear()
{
  mMessageListener = nullptr;
  mClosure = nullptr;
  mConnection = nullptr;
}

NS_IMETHODIMP
nsLDAPOperation::GetMessageListener(nsILDAPMessageListener **aMessageListener)
{
    if (!aMessageListener) {
        return NS_ERROR_ILLEGAL_VALUE;
    }

    *aMessageListener = mMessageListener;
    NS_IF_ADDREF(*aMessageListener);

    return NS_OK;
}

NS_IMETHODIMP
nsLDAPOperation::SaslBind(const nsACString &service,
                          const nsACString &mechanism,
                          nsIAuthModule *authModule)
{
  nsresult rv;
  nsCAutoString bindName;
  struct berval creds;
  unsigned int credlen;

  mAuthModule = authModule;
  mMechanism.Assign(mechanism);

  rv = mConnection->GetBindName(bindName);
  NS_ENSURE_SUCCESS(rv, rv);

  creds.bv_val = NULL;
  mAuthModule->Init(PromiseFlatCString(service).get(),
                    nsIAuthModule::REQ_DEFAULT, nullptr,
                    NS_ConvertUTF8toUTF16(bindName).get(), nullptr);

  rv = mAuthModule->GetNextToken(nullptr, 0, (void **)&creds.bv_val,
                                 &credlen);
  if (NS_FAILED(rv) || !creds.bv_val)
    return rv;

  creds.bv_len = credlen;
  const int lderrno = ldap_sasl_bind(mConnectionHandle, bindName.get(),
                                     mMechanism.get(), &creds, NULL, NULL,
                                     &mMsgID);
  nsMemory::Free(creds.bv_val);

  if (lderrno != LDAP_SUCCESS)
    return TranslateLDAPErrorToNSError(lderrno);

  // make sure the connection knows where to call back once the messages
  // for this operation start coming in
  rv = mConnection->AddPendingOperation(mMsgID, this);

  if (NS_FAILED(rv))
    (void)ldap_abandon_ext(mConnectionHandle, mMsgID, 0, 0);

  return rv;
}

NS_IMETHODIMP
nsLDAPOperation::SaslStep(const char *token, uint32_t tokenLen)
{
  nsresult rv;
  nsCAutoString bindName;
  struct berval clientCreds;
  struct berval serverCreds;
  unsigned int credlen;

  rv = mConnection->RemovePendingOperation(mMsgID);
  NS_ENSURE_SUCCESS(rv, rv);

  serverCreds.bv_val = (char *) token;
  serverCreds.bv_len = tokenLen;

  rv = mConnection->GetBindName(bindName);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mAuthModule->GetNextToken(serverCreds.bv_val, serverCreds.bv_len,
                                 (void **) &clientCreds.bv_val, &credlen);
  NS_ENSURE_SUCCESS(rv, rv);

  clientCreds.bv_len = credlen;

  const int lderrno = ldap_sasl_bind(mConnectionHandle, bindName.get(),
                                     mMechanism.get(), &clientCreds, NULL,
                                     NULL, &mMsgID);

  nsMemory::Free(clientCreds.bv_val);

  if (lderrno != LDAP_SUCCESS)
    return TranslateLDAPErrorToNSError(lderrno);

  // make sure the connection knows where to call back once the messages
  // for this operation start coming in
  rv = mConnection->AddPendingOperation(mMsgID, this);
  if (NS_FAILED(rv))
    (void)ldap_abandon_ext(mConnectionHandle, mMsgID, 0, 0);

  return rv;
}


// wrapper for ldap_simple_bind()
//
NS_IMETHODIMP
nsLDAPOperation::SimpleBind(const nsACString& passwd)
{
    nsRefPtr<nsLDAPConnection> connection = mConnection;
    // There is a possibilty that mConnection can be cleared by another
    // thread. Grabbing a local reference to mConnection may avoid this.
    // See https://bugzilla.mozilla.org/show_bug.cgi?id=557928#c1
    nsresult rv;
    nsCAutoString bindName;
    int32_t originalMsgID = mMsgID;
    // Ugly hack alert:
    // the first time we get called with a passwd, remember it.
    // Then, if we get called again w/o a password, use the
    // saved one. Getting called again means we're trying to
    // fall back to VERSION2.
    // Since LDAP operations are thrown away when done, it won't stay
    // around in memory.
    if (!passwd.IsEmpty())
      mSavePassword = passwd;

    NS_PRECONDITION(mMessageListener != 0, "MessageListener not set");

    rv = connection->GetBindName(bindName);
    if (NS_FAILED(rv))
        return rv;

    PR_LOG(gLDAPLogModule, PR_LOG_DEBUG,
           ("nsLDAPOperation::SimpleBind(): called; bindName = '%s'; ",
            bindName.get()));

    // If this is a second try at binding, remove the operation from pending ops
    // because msg id has changed...
    if (originalMsgID)
      connection->RemovePendingOperation(originalMsgID);

    mMsgID = ldap_simple_bind(mConnectionHandle, bindName.get(),
                              PromiseFlatCString(mSavePassword).get());

    if (mMsgID == -1) {
      // XXX Should NS_ERROR_LDAP_SERVER_DOWN cause a rebind here?
      return TranslateLDAPErrorToNSError(ldap_get_lderrno(mConnectionHandle,
                                                          0, 0));
    }

    // make sure the connection knows where to call back once the messages
    // for this operation start coming in
    rv = connection->AddPendingOperation(mMsgID, this);
    switch (rv) {
    case NS_OK:
        break;

        // note that the return value of ldap_abandon_ext() is ignored, as
        // there's nothing useful to do with it

    case NS_ERROR_OUT_OF_MEMORY:
        (void)ldap_abandon_ext(mConnectionHandle, mMsgID, 0, 0);
        return NS_ERROR_OUT_OF_MEMORY;
        break;

    case NS_ERROR_UNEXPECTED:
    case NS_ERROR_ILLEGAL_VALUE:
    default:
        (void)ldap_abandon_ext(mConnectionHandle, mMsgID, 0, 0);
        return NS_ERROR_UNEXPECTED;
    }

    return NS_OK;
}

/**
 * Given an nsIArray of nsILDAPControls, return the appropriate
 * zero-terminated array of LDAPControls ready to pass in to the C SDK.
 */
static nsresult
convertControlArray(nsIArray *aXpcomArray, LDAPControl ***aArray)
{
    // get the size of the original array
    uint32_t length;
    nsresult rv  = aXpcomArray->GetLength(&length);
    NS_ENSURE_SUCCESS(rv, rv);

    // don't allocate an array if someone passed us in an empty one
    if (!length) {
        *aArray = 0;
        return NS_OK;
    }

    // allocate a local array of the form understood by the C-SDK;
    // +1 is to account for the final null terminator.  PR_Calloc is
    // is used so that ldap_controls_free will work anywhere during the
    // iteration
    LDAPControl **controls =
        static_cast<LDAPControl **>
                   (PR_Calloc(length+1, sizeof(LDAPControl)));

    // prepare to enumerate the array
    nsCOMPtr<nsISimpleEnumerator> enumerator;
    rv = aXpcomArray->Enumerate(getter_AddRefs(enumerator));
    NS_ENSURE_SUCCESS(rv, rv);

    bool moreElements;
    rv = enumerator->HasMoreElements(&moreElements);
    NS_ENSURE_SUCCESS(rv, rv);

    uint32_t i = 0;
    while (moreElements) {

        // get the next array element
        nsCOMPtr<nsISupports> isupports;
        rv = enumerator->GetNext(getter_AddRefs(isupports));
        if (NS_FAILED(rv)) {
            ldap_controls_free(controls);
            return rv;
        }
        nsCOMPtr<nsILDAPControl> control = do_QueryInterface(isupports, &rv);
        if (NS_FAILED(rv)) {
            ldap_controls_free(controls);
            return NS_ERROR_INVALID_ARG; // bogus element in the array
        }
        nsLDAPControl *ctl = static_cast<nsLDAPControl *>
                                        (static_cast<nsILDAPControl *>
                                                    (control.get()));

        // convert it to an LDAPControl structure placed in the new array
        rv = ctl->ToLDAPControl(&controls[i]);
        if (NS_FAILED(rv)) {
            ldap_controls_free(controls);
            return rv;
        }

        // on to the next element
        rv = enumerator->HasMoreElements(&moreElements);
        if (NS_FAILED(rv)) {
            ldap_controls_free(controls);
            return NS_ERROR_UNEXPECTED;
        }
        ++i;
    }

    *aArray = controls;
    return NS_OK;
}

NS_IMETHODIMP
nsLDAPOperation::SearchExt(const nsACString& aBaseDn, int32_t aScope,
                           const nsACString& aFilter,
                           const nsACString &aAttributes,
                           PRIntervalTime aTimeOut, int32_t aSizeLimit)
{
    if (!mMessageListener) {
        NS_ERROR("nsLDAPOperation::SearchExt(): mMessageListener not set");
        return NS_ERROR_NOT_INITIALIZED;
    }

    // XXX add control logging
    PR_LOG(gLDAPLogModule, PR_LOG_DEBUG,
           ("nsLDAPOperation::SearchExt(): called with aBaseDn = '%s'; "
            "aFilter = '%s'; aAttributes = %s; aSizeLimit = %d",
            PromiseFlatCString(aBaseDn).get(),
            PromiseFlatCString(aFilter).get(),
            PromiseFlatCString(aAttributes).get(), aSizeLimit));

    LDAPControl **serverctls = 0;
    nsresult rv;
    if (mServerControls) {
        rv = convertControlArray(mServerControls, &serverctls);
        if (NS_FAILED(rv)) {
            PR_LOG(gLDAPLogModule, PR_LOG_ERROR,
                   ("nsLDAPOperation::SearchExt(): error converting server "
                    "control array: %x", rv));
            return rv;
        }
    }

    LDAPControl **clientctls = 0;
    if (mClientControls) {
        rv = convertControlArray(mClientControls, &clientctls);
        if (NS_FAILED(rv)) {
            PR_LOG(gLDAPLogModule, PR_LOG_ERROR,
                   ("nsLDAPOperation::SearchExt(): error converting client "
                    "control array: %x", rv));
            ldap_controls_free(serverctls);
            return rv;
        }
    }

    // Convert our comma separated string to one that the C-SDK will like, i.e.
    // convert to a char array and add a last NULL element.
    nsTArray<nsCString> attrArray;
    ParseString(aAttributes, ',', attrArray);
    char **attrs = nullptr;
    uint32_t origLength = attrArray.Length();
    if (origLength)
    {
      attrs = static_cast<char **> (NS_Alloc((origLength + 1) * sizeof(char *)));
      if (!attrs)
        return NS_ERROR_OUT_OF_MEMORY;

      for (uint32_t i = 0; i < origLength; ++i)
        attrs[i] = ToNewCString(attrArray[i]);

      attrs[origLength] = 0;
    }

    // XXX deal with timeout here
    int retVal = ldap_search_ext(mConnectionHandle,
                                 PromiseFlatCString(aBaseDn).get(),
                                 aScope, PromiseFlatCString(aFilter).get(),
                                 attrs, 0, serverctls, clientctls, 0,
                                 aSizeLimit, &mMsgID);

    // clean up
    ldap_controls_free(serverctls);
    ldap_controls_free(clientctls);
    // The last entry is null, so no need to free that.
    NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(origLength, attrs);

    rv = TranslateLDAPErrorToNSError(retVal);
    NS_ENSURE_SUCCESS(rv, rv);

    // make sure the connection knows where to call back once the messages
    // for this operation start coming in
    //
    rv = mConnection->AddPendingOperation(mMsgID, this);
    if (NS_FAILED(rv)) {
        switch (rv) {
        case NS_ERROR_OUT_OF_MEMORY:
            (void)ldap_abandon_ext(mConnectionHandle, mMsgID, 0, 0);
            return NS_ERROR_OUT_OF_MEMORY;

        default:
            (void)ldap_abandon_ext(mConnectionHandle, mMsgID, 0, 0);
            NS_ERROR("nsLDAPOperation::SearchExt(): unexpected error in "
                     "mConnection->AddPendingOperation");
            return NS_ERROR_UNEXPECTED;
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsLDAPOperation::GetMessageID(int32_t *aMsgID)
{
    if (!aMsgID) {
        return NS_ERROR_ILLEGAL_VALUE;
    }

    *aMsgID = mMsgID;

    return NS_OK;
}

// as far as I can tell from reading the LDAP C SDK code, abandoning something
// that has already been abandoned does not return an error
//
NS_IMETHODIMP
nsLDAPOperation::AbandonExt()
{
    nsresult rv;
    nsresult retStatus = NS_OK;

    if ( mMessageListener == 0 || mMsgID == 0 ) {
        NS_ERROR("nsLDAPOperation::AbandonExt(): mMessageListener or "
                 "mMsgId not initialized");
        return NS_ERROR_NOT_INITIALIZED;
    }

    // XXX handle controls here
    if (mServerControls || mClientControls) {
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    rv = TranslateLDAPErrorToNSError(ldap_abandon_ext(mConnectionHandle,
                                                      mMsgID, 0, 0));
    NS_ENSURE_SUCCESS(rv, rv);

    // try to remove it from the pendingOperations queue, if it's there.
    // even if something goes wrong here, the abandon() has already succeeded
    // succeeded (and there's nothing else the caller can reasonably do),
    // so we only pay attention to this in debug builds.
    //
    // check mConnection in case we're getting bit by
    // http://bugzilla.mozilla.org/show_bug.cgi?id=239729, wherein we
    // theorize that ::Clearing the operation is nulling out the mConnection
    // from another thread.
    if (mConnection)
    {
      rv = mConnection->RemovePendingOperation(mMsgID);

      if (NS_FAILED(rv)) {
          // XXXdmose should we keep AbandonExt from happening on multiple
          // threads at the same time?  that's when this condition is most
          // likely to occur.  i _think_ the LDAP C SDK is ok with this; need
          // to verify.
          //
          NS_WARNING("nsLDAPOperation::AbandonExt: "
                     "mConnection->RemovePendingOperation(this) failed.");
      }
    }

    return retStatus;
}

NS_IMETHODIMP
nsLDAPOperation::GetClientControls(nsIMutableArray **aControls)
{
    NS_IF_ADDREF(*aControls = mClientControls);
    return NS_OK;
}

NS_IMETHODIMP
nsLDAPOperation::SetClientControls(nsIMutableArray *aControls)
{
    mClientControls = aControls;
    return NS_OK;
}

NS_IMETHODIMP nsLDAPOperation::GetServerControls(nsIMutableArray **aControls)
{
    NS_IF_ADDREF(*aControls = mServerControls);
    return NS_OK;
}

NS_IMETHODIMP nsLDAPOperation::SetServerControls(nsIMutableArray *aControls)
{
    mServerControls = aControls;
    return NS_OK;
}

// wrappers for ldap_add_ext
//
nsresult
nsLDAPOperation::AddExt(const char *base,
                        nsIArray *mods,
                        LDAPControl **serverctrls,
                        LDAPControl **clientctrls)
{
  if (mMessageListener == 0) {
    NS_ERROR("nsLDAPOperation::AddExt(): mMessageListener not set");
    return NS_ERROR_NOT_INITIALIZED;
  }

  LDAPMod **attrs = 0;
  int retVal = LDAP_SUCCESS;
  uint32_t modCount = 0;
  nsresult rv = mods->GetLength(&modCount);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mods && modCount) {
    attrs = static_cast<LDAPMod **>(nsMemory::Alloc((modCount + 1) *
                                                       sizeof(LDAPMod *)));
    if (!attrs) {
      NS_ERROR("nsLDAPOperation::AddExt: out of memory ");
      return NS_ERROR_OUT_OF_MEMORY;
    }

    nsCAutoString type;
    uint32_t index;
    for (index = 0; index < modCount && NS_SUCCEEDED(rv); ++index) {
      attrs[index] = new LDAPMod();

      if (!attrs[index])
        return NS_ERROR_OUT_OF_MEMORY;

      nsCOMPtr<nsILDAPModification> modif(do_QueryElementAt(mods, index, &rv));
      if (NS_FAILED(rv))
        break;

#ifdef NS_DEBUG
      int32_t operation;
      NS_ASSERTION(NS_SUCCEEDED(modif->GetOperation(&operation)) &&
                   ((operation & ~LDAP_MOD_BVALUES) == LDAP_MOD_ADD),
                   "AddExt can only add.");
#endif

      attrs[index]->mod_op = LDAP_MOD_ADD | LDAP_MOD_BVALUES;

      nsresult rv = modif->GetType(type);
      if (NS_FAILED(rv))
        break;

      attrs[index]->mod_type = ToNewCString(type);

      rv = CopyValues(modif, &attrs[index]->mod_bvalues);
      if (NS_FAILED(rv))
        break;
    }

    if (NS_SUCCEEDED(rv)) {
      attrs[modCount] = 0;

      retVal = ldap_add_ext(mConnectionHandle, base, attrs,
                            serverctrls, clientctrls, &mMsgID);
    }
    else
      // reset the modCount so we correctly free the array.
      modCount = index;
  }

  for (uint32_t counter = 0; counter < modCount; ++counter)
    delete attrs[counter];

  nsMemory::Free(attrs);

  return NS_FAILED(rv) ? rv : TranslateLDAPErrorToNSError(retVal);
}

/**
 * wrapper for ldap_add_ext(): kicks off an async add request.
 *
 * @param aBaseDn           Base DN to search
 * @param aModCount         Number of modifications
 * @param aMods             Array of modifications
 *
 * XXX doesn't currently handle LDAPControl params
 *
 * void addExt (in AUTF8String aBaseDn, in unsigned long aModCount,
 *              [array, size_is (aModCount)] in nsILDAPModification aMods);
 */
NS_IMETHODIMP
nsLDAPOperation::AddExt(const nsACString& aBaseDn,
                        nsIArray *aMods)
{
  PR_LOG(gLDAPLogModule, PR_LOG_DEBUG,
         ("nsLDAPOperation::AddExt(): called with aBaseDn = '%s'",
          PromiseFlatCString(aBaseDn).get()));

  nsresult rv = AddExt(PromiseFlatCString(aBaseDn).get(), aMods, 0, 0);
  if (NS_FAILED(rv))
    return rv;

  // make sure the connection knows where to call back once the messages
  // for this operation start coming in
  rv = mConnection->AddPendingOperation(mMsgID, this);

  if (NS_FAILED(rv)) {
    (void)ldap_abandon_ext(mConnectionHandle, mMsgID, 0, 0);
    PR_LOG(gLDAPLogModule, PR_LOG_DEBUG,
           ("nsLDAPOperation::AddExt(): abandoned due to rv %x",
            rv));
  }
  return rv;
}

// wrappers for ldap_delete_ext
//
nsresult
nsLDAPOperation::DeleteExt(const char *base,
                           LDAPControl **serverctrls,
                           LDAPControl **clientctrls)
{
  if (mMessageListener == 0) {
    NS_ERROR("nsLDAPOperation::DeleteExt(): mMessageListener not set");
    return NS_ERROR_NOT_INITIALIZED;
  }

  return TranslateLDAPErrorToNSError(ldap_delete_ext(mConnectionHandle, base,
                                                     serverctrls, clientctrls,
                                                     &mMsgID));
}

/**
 * wrapper for ldap_delete_ext(): kicks off an async delete request.
 *
 * @param aBaseDn               Base DN to delete
 *
 * XXX doesn't currently handle LDAPControl params
 *
 * void deleteExt(in AUTF8String aBaseDn);
 */
NS_IMETHODIMP
nsLDAPOperation::DeleteExt(const nsACString& aBaseDn)
{
  PR_LOG(gLDAPLogModule, PR_LOG_DEBUG,
         ("nsLDAPOperation::DeleteExt(): called with aBaseDn = '%s'",
          PromiseFlatCString(aBaseDn).get()));

  nsresult rv = DeleteExt(PromiseFlatCString(aBaseDn).get(), 0, 0);
  if (NS_FAILED(rv))
    return rv;

  // make sure the connection knows where to call back once the messages
  // for this operation start coming in
  rv = mConnection->AddPendingOperation(mMsgID, this);

  if (NS_FAILED(rv)) {
    (void)ldap_abandon_ext(mConnectionHandle, mMsgID, 0, 0);
    PR_LOG(gLDAPLogModule, PR_LOG_DEBUG,
           ("nsLDAPOperation::AddExt(): abandoned due to rv %x",
            rv));
  }
  return rv;
}

// wrappers for ldap_modify_ext
//
nsresult
nsLDAPOperation::ModifyExt(const char *base,
                           nsIArray *mods,
                           LDAPControl **serverctrls,
                           LDAPControl **clientctrls)
{
  if (mMessageListener == 0) {
    NS_ERROR("nsLDAPOperation::ModifyExt(): mMessageListener not set");
    return NS_ERROR_NOT_INITIALIZED;
  }

  LDAPMod **attrs = 0;
  int retVal = 0;
  uint32_t modCount = 0;
  nsresult rv = mods->GetLength(&modCount);
  NS_ENSURE_SUCCESS(rv, rv);
  if (modCount && mods) {
    attrs = static_cast<LDAPMod **>(nsMemory::Alloc((modCount + 1) *
                                                       sizeof(LDAPMod *)));
    if (!attrs) {
      NS_ERROR("nsLDAPOperation::ModifyExt: out of memory ");
      return NS_ERROR_OUT_OF_MEMORY;
    }

    nsCAutoString type;
    uint32_t index;
    for (index = 0; index < modCount && NS_SUCCEEDED(rv); ++index) {
      attrs[index] = new LDAPMod();
      if (!attrs[index])
        return NS_ERROR_OUT_OF_MEMORY;

      nsCOMPtr<nsILDAPModification> modif(do_QueryElementAt(mods, index, &rv));
      if (NS_FAILED(rv))
        break;

      int32_t operation;
      nsresult rv = modif->GetOperation(&operation);
      if (NS_FAILED(rv))
        break;

      attrs[index]->mod_op = operation | LDAP_MOD_BVALUES;

      rv = modif->GetType(type);
      if (NS_FAILED(rv))
        break;

      attrs[index]->mod_type = ToNewCString(type);

      rv = CopyValues(modif, &attrs[index]->mod_bvalues);
      if (NS_FAILED(rv))
        break;
    }

    if (NS_SUCCEEDED(rv)) {
      attrs[modCount] = 0;

      retVal = ldap_modify_ext(mConnectionHandle, base, attrs,
                               serverctrls, clientctrls, &mMsgID);
    }
    else
      // reset the modCount so we correctly free the array.
      modCount = index;

  }

  for (uint32_t counter = 0; counter < modCount; ++counter)
    delete attrs[counter];

  nsMemory::Free(attrs);

  return NS_FAILED(rv) ? rv : TranslateLDAPErrorToNSError(retVal);
}

/**
 * wrapper for ldap_modify_ext(): kicks off an async modify request.
 *
 * @param aBaseDn           Base DN to modify
 * @param aModCount         Number of modifications
 * @param aMods             Array of modifications
 *
 * XXX doesn't currently handle LDAPControl params
 *
 * void modifyExt (in AUTF8String aBaseDn, in unsigned long aModCount,
 *                 [array, size_is (aModCount)] in nsILDAPModification aMods);
 */
NS_IMETHODIMP
nsLDAPOperation::ModifyExt(const nsACString& aBaseDn,
                           nsIArray *aMods)
{
  PR_LOG(gLDAPLogModule, PR_LOG_DEBUG,
         ("nsLDAPOperation::ModifyExt(): called with aBaseDn = '%s'",
          PromiseFlatCString(aBaseDn).get()));

  nsresult rv = ModifyExt(PromiseFlatCString(aBaseDn).get(),
                          aMods, 0, 0);
  if (NS_FAILED(rv))
    return rv;

  // make sure the connection knows where to call back once the messages
  // for this operation start coming in
  rv = mConnection->AddPendingOperation(mMsgID, this);

  if (NS_FAILED(rv)) {
    (void)ldap_abandon_ext(mConnectionHandle, mMsgID, 0, 0);
    PR_LOG(gLDAPLogModule, PR_LOG_DEBUG,
           ("nsLDAPOperation::AddExt(): abandoned due to rv %x",
            rv));
  }
  return rv;
}

// wrappers for ldap_rename
//
nsresult
nsLDAPOperation::Rename(const char *base,
                        const char *newRDn,
                        const char *newParent,
                        bool deleteOldRDn,
                        LDAPControl **serverctrls,
                        LDAPControl **clientctrls)
{
  if (mMessageListener == 0) {
    NS_ERROR("nsLDAPOperation::Rename(): mMessageListener not set");
    return NS_ERROR_NOT_INITIALIZED;
  }

  return TranslateLDAPErrorToNSError(ldap_rename(mConnectionHandle, base,
                                                 newRDn, newParent,
                                                 deleteOldRDn, serverctrls,
                                                 clientctrls, &mMsgID));
}

/**
 * wrapper for ldap_rename(): kicks off an async rename request.
 *
 * @param aBaseDn               Base DN to rename
 * @param aNewRDn               New relative DN
 * @param aNewParent            DN of the new parent under which to move the
 *
 * XXX doesn't currently handle LDAPControl params
 *
 * void rename(in AUTF8String aBaseDn, in AUTF8String aNewRDn,
 *             in AUTF8String aNewParent, in boolean aDeleteOldRDn);
 */
NS_IMETHODIMP
nsLDAPOperation::Rename(const nsACString& aBaseDn,
                        const nsACString& aNewRDn,
                        const nsACString& aNewParent,
                        bool aDeleteOldRDn)
{
  PR_LOG(gLDAPLogModule, PR_LOG_DEBUG,
         ("nsLDAPOperation::Rename(): called with aBaseDn = '%s'",
          PromiseFlatCString(aBaseDn).get()));

  nsresult rv = Rename(PromiseFlatCString(aBaseDn).get(),
                       PromiseFlatCString(aNewRDn).get(),
                       PromiseFlatCString(aNewParent).get(),
                       aDeleteOldRDn, 0, 0);
  if (NS_FAILED(rv))
    return rv;

  // make sure the connection knows where to call back once the messages
  // for this operation start coming in
  rv = mConnection->AddPendingOperation(mMsgID, this);

  if (NS_FAILED(rv)) {
    (void)ldap_abandon_ext(mConnectionHandle, mMsgID, 0, 0);
    PR_LOG(gLDAPLogModule, PR_LOG_DEBUG,
           ("nsLDAPOperation::AddExt(): abandoned due to rv %x",
            rv));
  }
  return rv;
}

// wrappers for ldap_search_ext
//

/* static */
nsresult
nsLDAPOperation::CopyValues(nsILDAPModification* aMod, berval*** aBValues)
{
  nsCOMPtr<nsIArray> values;
  nsresult rv = aMod->GetValues(getter_AddRefs(values));
  NS_ENSURE_SUCCESS(rv, rv);

  uint32_t valuesCount;
  rv = values->GetLength(&valuesCount);
  NS_ENSURE_SUCCESS(rv, rv);

  *aBValues = static_cast<berval **>
                         (nsMemory::Alloc((valuesCount + 1) *
                                             sizeof(berval *)));
  if (!*aBValues)
    return NS_ERROR_OUT_OF_MEMORY;

  uint32_t valueIndex;
  for (valueIndex = 0; valueIndex < valuesCount; ++valueIndex) {
    nsCOMPtr<nsILDAPBERValue> value(do_QueryElementAt(values, valueIndex, &rv));

    berval* bval = new berval;
    if (NS_FAILED(rv) || !bval) {
      for (uint32_t counter = 0;
           counter < valueIndex && counter < valuesCount;
           ++counter)
        delete (*aBValues)[valueIndex];

      nsMemory::Free(*aBValues);
      delete bval;
      return NS_ERROR_OUT_OF_MEMORY;
    }
    value->Get((uint32_t*)&bval->bv_len,
               (uint8_t**)&bval->bv_val);
    (*aBValues)[valueIndex] = bval;
  }

  (*aBValues)[valuesCount] = 0;
  return NS_OK;
}
