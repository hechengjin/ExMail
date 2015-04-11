/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsOCSPResponder.h"

#include "nsCOMPtr.h"
#include "nsIDateTimeFormat.h"
#include "nsDateTimeFormatCID.h"
#include "nsComponentManagerUtils.h"
#include "nsReadableUtils.h"

#include "certdb.h"

/* Implementation file */
NS_IMPL_ISUPPORTS1(nsOCSPResponder, nsIOCSPResponder)

nsOCSPResponder::nsOCSPResponder()
{
  /* member initializers and constructor code */
}

nsOCSPResponder::nsOCSPResponder(const PRUnichar * aCA, const PRUnichar * aURL)
{
  mCA.Assign(aCA);
  mURL.Assign(aURL);
}

nsOCSPResponder::~nsOCSPResponder()
{
  /* destructor code */
}

/* readonly attribute */
NS_IMETHODIMP nsOCSPResponder::GetResponseSigner(PRUnichar** aCA)
{
  NS_ENSURE_ARG(aCA);
  *aCA = ToNewUnicode(mCA);
  return NS_OK;
}

/* readonly attribute */
NS_IMETHODIMP nsOCSPResponder::GetServiceURL(PRUnichar** aURL)
{
  NS_ENSURE_ARG(aURL);
  *aURL = ToNewUnicode(mURL);
  return NS_OK;
}

bool nsOCSPResponder::IncludeCert(CERTCertificate *aCert)
{
  CERTCertTrust *trust;
  char *nickname;

  trust = aCert->trust;
  nickname = aCert->nickname;

  PR_ASSERT(trust != nullptr);

  // Check that trust is non-null //
  if (trust == nullptr) {
    return false;
  }

  if ( ( ( trust->sslFlags & CERTDB_INVISIBLE_CA ) ||
         (trust->emailFlags & CERTDB_INVISIBLE_CA ) ||
         (trust->objectSigningFlags & CERTDB_INVISIBLE_CA ) ) ||
       nickname == NULL) {
      return false;
  }
  if ((trust->sslFlags & CERTDB_VALID_CA) ||
      (trust->emailFlags & CERTDB_VALID_CA) ||
      (trust->objectSigningFlags & CERTDB_VALID_CA)) {
      return true;
  }
  return false;
}

// CmpByCAName
//
// Compare two responders their token name.  Returns -1, 0, 1 as
// in strcmp.  No token name (null) is treated as >.
int32_t nsOCSPResponder::CmpCAName(nsIOCSPResponder *a, nsIOCSPResponder *b)
{
  int32_t cmp1;
  nsXPIDLString aTok, bTok;
  a->GetResponseSigner(getter_Copies(aTok));
  b->GetResponseSigner(getter_Copies(bTok));
  if (aTok != nullptr && bTok != nullptr) {
    cmp1 = Compare(aTok, bTok);
  } else {
    cmp1 = (aTok == nullptr) ? 1 : -1;
  }
  return cmp1;
}

// ocsp_compare_entries
//
// Compare two responders.  Returns -1, 0, 1 as
// in strcmp.  Entries with urls come before those without urls.
int32_t nsOCSPResponder::CompareEntries(nsIOCSPResponder *a, nsIOCSPResponder *b)
{
  nsXPIDLString aURL, bURL;
  nsAutoString aURLAuto, bURLAuto;

  a->GetServiceURL(getter_Copies(aURL));
  aURLAuto.Assign(aURL);
  b->GetServiceURL(getter_Copies(bURL));
  bURLAuto.Assign(bURL);

  if (!aURLAuto.IsEmpty()) {
    if (!bURLAuto.IsEmpty()) {
      return nsOCSPResponder::CmpCAName(a, b);
    } else {
      return -1;
    }
  } else {
    if (!bURLAuto.IsEmpty()) {
      return 1;
    } else {
      return nsOCSPResponder::CmpCAName(a, b);
    }
  }
}

