/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMsgTxn.h"
#include "nsIMsgHdr.h"
#include "nsIMsgDatabase.h"
#include "nsCOMArray.h"
#include "nsArrayEnumerator.h"
#include "nsComponentManagerUtils.h"
#include "nsIVariant.h"
#include "nsIProperty.h"
#include "nsMsgMessageFlags.h"
#include "nsIMsgFolder.h"

NS_IMPL_THREADSAFE_ADDREF(nsMsgTxn)
NS_IMPL_THREADSAFE_RELEASE(nsMsgTxn)
NS_INTERFACE_MAP_BEGIN(nsMsgTxn)
  NS_INTERFACE_MAP_ENTRY(nsIWritablePropertyBag)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIPropertyBag, nsIWritablePropertyBag)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIWritablePropertyBag)
  NS_INTERFACE_MAP_ENTRY(nsITransaction)
  NS_INTERFACE_MAP_ENTRY(nsIPropertyBag2)
  NS_INTERFACE_MAP_ENTRY(nsIWritablePropertyBag2)
NS_INTERFACE_MAP_END

nsMsgTxn::nsMsgTxn() 
{
  m_txnType = 0;
}

nsMsgTxn::~nsMsgTxn()
{
}

nsresult nsMsgTxn::Init()
{
  mPropertyHash.Init();
  return NS_OK;
}

NS_IMETHODIMP nsMsgTxn::HasKey(const nsAString& name, bool *aResult)
{
  *aResult = mPropertyHash.Get(name, nullptr);
  return NS_OK;
}

NS_IMETHODIMP nsMsgTxn::Get(const nsAString& name, nsIVariant* *_retval)
{
  mPropertyHash.Get(name, _retval);
  return NS_OK;
}

NS_IMETHODIMP nsMsgTxn::GetProperty(const nsAString& name, nsIVariant* * _retval)
{
  return mPropertyHash.Get(name, _retval) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMsgTxn::SetProperty(const nsAString& name, nsIVariant *value)
{
  NS_ENSURE_ARG_POINTER(value);
  mPropertyHash.Put(name, value);
  return NS_OK;
}

NS_IMETHODIMP nsMsgTxn::DeleteProperty(const nsAString& name)
{
  if (!mPropertyHash.Get(name, nullptr))
    return NS_ERROR_FAILURE;

  mPropertyHash.Remove(name);
  return mPropertyHash.Get(name, nullptr) ? NS_ERROR_FAILURE : NS_OK;
}

//
// nsMailSimpleProperty class and impl; used for GetEnumerator
// This is same as nsSimpleProperty but for external API use.
//

class nsMailSimpleProperty : public nsIProperty 
{
public:
  nsMailSimpleProperty(const nsAString& aName, nsIVariant* aValue)
      : mName(aName), mValue(aValue)
  {
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIPROPERTY
protected:
  nsString mName;
  nsCOMPtr<nsIVariant> mValue;
};

NS_IMPL_ISUPPORTS1(nsMailSimpleProperty, nsIProperty)

NS_IMETHODIMP nsMailSimpleProperty::GetName(nsAString& aName)
{
  aName.Assign(mName);
  return NS_OK;
}

NS_IMETHODIMP nsMailSimpleProperty::GetValue(nsIVariant* *aValue)
{
  NS_IF_ADDREF(*aValue = mValue);
  return NS_OK;
}

// end nsMailSimpleProperty

static PLDHashOperator
PropertyHashToArrayFunc (const nsAString &aKey,
                         nsIVariant* aData,
                         void *userArg)
{
  nsCOMArray<nsIProperty> *propertyArray =
      static_cast<nsCOMArray<nsIProperty> *>(userArg);
  nsMailSimpleProperty *sprop = new nsMailSimpleProperty(aKey, aData);
  propertyArray->AppendObject(sprop);
  return PL_DHASH_NEXT;
}

NS_IMETHODIMP nsMsgTxn::GetEnumerator(nsISimpleEnumerator* *_retval)
{
  nsCOMArray<nsIProperty> propertyArray;
  mPropertyHash.EnumerateRead(PropertyHashToArrayFunc, &propertyArray);
  return NS_NewArrayEnumerator(_retval, propertyArray);
}

#define IMPL_GETSETPROPERTY_AS(Name, Type) \
NS_IMETHODIMP \
nsMsgTxn::GetPropertyAs ## Name (const nsAString & prop, Type *_retval) \
{ \
    nsIVariant* v = mPropertyHash.GetWeak(prop); \
    if (!v) \
        return NS_ERROR_NOT_AVAILABLE; \
    return v->GetAs ## Name(_retval); \
} \
\
NS_IMETHODIMP \
nsMsgTxn::SetPropertyAs ## Name (const nsAString & prop, Type value) \
{ \
    nsresult rv; \
    nsCOMPtr<nsIWritableVariant> var = do_CreateInstance(NS_VARIANT_CONTRACTID, &rv); \
    NS_ENSURE_SUCCESS(rv, rv); \
    var->SetAs ## Name(value); \
    return SetProperty(prop, var); \
}

IMPL_GETSETPROPERTY_AS(Int32, int32_t)
IMPL_GETSETPROPERTY_AS(Uint32, uint32_t)
IMPL_GETSETPROPERTY_AS(Int64, int64_t)
IMPL_GETSETPROPERTY_AS(Uint64, uint64_t)
IMPL_GETSETPROPERTY_AS(Double, double)
IMPL_GETSETPROPERTY_AS(Bool, bool)

NS_IMETHODIMP nsMsgTxn::GetPropertyAsAString(const nsAString & prop, 
                                             nsAString & _retval)
{
  nsIVariant* v = mPropertyHash.GetWeak(prop);
  if (!v)
    return NS_ERROR_NOT_AVAILABLE;
  return v->GetAsAString(_retval);
}

NS_IMETHODIMP nsMsgTxn::GetPropertyAsACString(const nsAString & prop, 
                                              nsACString & _retval)
{
  nsIVariant* v = mPropertyHash.GetWeak(prop);
  if (!v)
    return NS_ERROR_NOT_AVAILABLE;
  return v->GetAsACString(_retval);
}

NS_IMETHODIMP nsMsgTxn::GetPropertyAsAUTF8String(const nsAString & prop, 
                                                 nsACString & _retval)
{
  nsIVariant* v = mPropertyHash.GetWeak(prop);
  if (!v)
    return NS_ERROR_NOT_AVAILABLE;
  return v->GetAsAUTF8String(_retval);
}

NS_IMETHODIMP nsMsgTxn::GetPropertyAsInterface(const nsAString & prop,
                                               const nsIID & aIID,
                                               void** _retval)
{
  nsIVariant* v = mPropertyHash.GetWeak(prop);
  if (!v)
    return NS_ERROR_NOT_AVAILABLE;
  nsCOMPtr<nsISupports> val;
  nsresult rv = v->GetAsISupports(getter_AddRefs(val));
  if (NS_FAILED(rv))
    return rv;
  if (!val) {
    // We have a value, but it's null
    *_retval = nullptr;
    return NS_OK;
  }
  return val->QueryInterface(aIID, _retval);
}

NS_IMETHODIMP nsMsgTxn::SetPropertyAsAString(const nsAString & prop, 
                                             const nsAString & value)
{
  nsresult rv;
  nsCOMPtr<nsIWritableVariant> var = do_CreateInstance(NS_VARIANT_CONTRACTID, &rv); 
  NS_ENSURE_SUCCESS(rv, rv); 
  var->SetAsAString(value);
  return SetProperty(prop, var);
}

NS_IMETHODIMP nsMsgTxn::SetPropertyAsACString(const nsAString & prop, 
                                              const nsACString & value)
{
  nsresult rv;
  nsCOMPtr<nsIWritableVariant> var = do_CreateInstance(NS_VARIANT_CONTRACTID, &rv); 
  NS_ENSURE_SUCCESS(rv, rv); 
  var->SetAsACString(value);
  return SetProperty(prop, var);
}

NS_IMETHODIMP nsMsgTxn::SetPropertyAsAUTF8String(const nsAString & prop, 
                                                 const nsACString & value)
{
  nsresult rv;
  nsCOMPtr<nsIWritableVariant> var = do_CreateInstance(NS_VARIANT_CONTRACTID, &rv); 
  NS_ENSURE_SUCCESS(rv, rv); 
  var->SetAsAUTF8String(value);
  return SetProperty(prop, var);
}

NS_IMETHODIMP nsMsgTxn::SetPropertyAsInterface(const nsAString & prop, 
                                               nsISupports* value)
{
  nsresult rv;
  nsCOMPtr<nsIWritableVariant> var = do_CreateInstance(NS_VARIANT_CONTRACTID, &rv); 
  NS_ENSURE_SUCCESS(rv, rv); 
  var->SetAsISupports(value);
  return SetProperty(prop, var);
}

/////////////////////// Transaction Stuff //////////////////
NS_IMETHODIMP nsMsgTxn::DoTransaction(void)
{
  return NS_OK;
}

NS_IMETHODIMP nsMsgTxn::GetIsTransient(bool *aIsTransient)
{
  if (nullptr!=aIsTransient)
    *aIsTransient = false;
  else
    return NS_ERROR_NULL_POINTER;
  return NS_OK;
}

NS_IMETHODIMP nsMsgTxn::Merge(nsITransaction *aTransaction, bool *aDidMerge)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


nsresult nsMsgTxn::GetMsgWindow(nsIMsgWindow **msgWindow)
{
    if (!msgWindow || !m_msgWindow)
        return NS_ERROR_NULL_POINTER;
    *msgWindow = m_msgWindow;
    NS_ADDREF (*msgWindow);
    return NS_OK;
}

nsresult nsMsgTxn::SetMsgWindow(nsIMsgWindow *msgWindow)
{
    m_msgWindow = msgWindow;
    return NS_OK;
}


nsresult
nsMsgTxn::SetTransactionType(uint32_t txnType)
{
  return SetPropertyAsUint32(NS_LITERAL_STRING("type"), txnType);
}

/*none of the callers pass null aFolder, 
  we always initialize aResult (before we pass in) for the case where the key is not in the db*/
nsresult 
nsMsgTxn::CheckForToggleDelete(nsIMsgFolder *aFolder, const nsMsgKey &aMsgKey, bool *aResult)
{
  NS_ENSURE_ARG(aResult);
  nsCOMPtr<nsIMsgDBHdr> message;
  nsCOMPtr<nsIMsgDatabase> db;
  nsresult rv = aFolder->GetMsgDatabase(getter_AddRefs(db));
  if (db)
  {
    bool containsKey;
    rv = db->ContainsKey(aMsgKey, &containsKey);
    if (NS_FAILED(rv) || !containsKey)   // the message has been deleted from db, so we cannot do toggle here
      return NS_OK;
    rv = db->GetMsgHdrForKey(aMsgKey, getter_AddRefs(message));
    uint32_t flags;
    if (NS_SUCCEEDED(rv) && message)
    {
      message->GetFlags(&flags);
      *aResult = (flags & nsMsgMessageFlags::IMAPDeleted) != 0;
    }
  }
  return rv;
}
