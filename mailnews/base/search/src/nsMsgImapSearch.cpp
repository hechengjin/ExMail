/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "msgCore.h"
#include "nsMsgSearchAdapter.h"
#include "nsMsgSearchScopeTerm.h"
#include "nsMsgResultElement.h"
#include "nsMsgSearchTerm.h"
#include "nsIMsgHdr.h"
#include "nsMsgSearchImap.h"
#include "prmem.h"
#include "nsISupportsArray.h"
// Implementation of search for IMAP mail folders


nsMsgSearchOnlineMail::nsMsgSearchOnlineMail (nsMsgSearchScopeTerm *scope, nsISupportsArray *termList) : nsMsgSearchAdapter (scope, termList)
{
}


nsMsgSearchOnlineMail::~nsMsgSearchOnlineMail () 
{
}


nsresult nsMsgSearchOnlineMail::ValidateTerms ()
{
  nsresult err = nsMsgSearchAdapter::ValidateTerms ();
  
  if (NS_SUCCEEDED(err))
  {
      // ### mwelch Figure out the charsets to use 
      //            for the search terms and targets.
      nsAutoString srcCharset, dstCharset;
      GetSearchCharsets(srcCharset, dstCharset);

      // do IMAP specific validation
      err = Encode (m_encoding, m_searchTerms, dstCharset.get());
      NS_ASSERTION(NS_SUCCEEDED(err), "failed to encode imap search");
  }
  
  return err;
}


NS_IMETHODIMP nsMsgSearchOnlineMail::GetEncoding (char **result)
{
  *result = ToNewCString(m_encoding);
  return NS_OK;
}

NS_IMETHODIMP nsMsgSearchOnlineMail::AddResultElement (nsIMsgDBHdr *pHeaders)
{
    nsresult err = NS_OK;

    nsCOMPtr<nsIMsgSearchSession> searchSession;
    m_scope->GetSearchSession(getter_AddRefs(searchSession));
    if (searchSession)
    {
      nsCOMPtr <nsIMsgFolder> scopeFolder;
      err = m_scope->GetFolder(getter_AddRefs(scopeFolder));
      searchSession->AddSearchHit(pHeaders, scopeFolder);
    }
    //XXXX alecf do not checkin without fixing!        m_scope->m_searchSession->AddResultElement (newResult);
    return err;
}

nsresult nsMsgSearchOnlineMail::Search (bool *aDone)
{
    // we should never end up here for a purely online
    // folder.  We might for an offline IMAP folder.
    nsresult err = NS_ERROR_NOT_IMPLEMENTED;

    return err;
}

nsresult nsMsgSearchOnlineMail::Encode (nsCString& pEncoding,
                                        nsISupportsArray *searchTerms,
                                        const PRUnichar *destCharset)
{
  nsCString imapTerms;
  
  //check if searchTerms are ascii only
  bool asciiOnly = true;
  // ### what's this mean in the NWO?????
  
  if (true) // !(srcCharset & CODESET_MASK == STATEFUL || srcCharset & CODESET_MASK == WIDECHAR) )   //assume all single/multiple bytes charset has ascii as subset
  {
    uint32_t termCount;
    searchTerms->Count(&termCount);
    uint32_t i = 0;
    
    for (i = 0; i < termCount && asciiOnly; i++)
    {
      nsCOMPtr<nsIMsgSearchTerm> pTerm;
      searchTerms->QueryElementAt(i, NS_GET_IID(nsIMsgSearchTerm),
        (void **)getter_AddRefs(pTerm));
      
      nsMsgSearchAttribValue attribute;
      pTerm->GetAttrib(&attribute);
      if (IS_STRING_ATTRIBUTE(attribute))
      {
        nsString pchar;
        nsCOMPtr <nsIMsgSearchValue> searchValue;
        
        nsresult rv = pTerm->GetValue(getter_AddRefs(searchValue));
        if (NS_FAILED(rv) || !searchValue)
          continue;
        
        
        rv = searchValue->GetStr(pchar);
        if (NS_FAILED(rv) || pchar.IsEmpty())
          continue;
        asciiOnly = NS_IsAscii(pchar.get());
      }
    }
  }
  else
    asciiOnly = false;
  
  nsAutoString usAsciiCharSet(NS_LITERAL_STRING("us-ascii"));
  // Get the optional CHARSET parameter, in case we need it.
  char *csname = GetImapCharsetParam(asciiOnly ? usAsciiCharSet.get() : destCharset);
  
  // We do not need "srcCharset" since the search term in always unicode.
  // I just pass destCharset for both src and dest charset instead of removing srcCharst from the arguemnt.
  nsresult err = nsMsgSearchAdapter::EncodeImap (getter_Copies(imapTerms), searchTerms, 
    asciiOnly ?  usAsciiCharSet.get(): destCharset, 
    asciiOnly ?  usAsciiCharSet.get(): destCharset, false);
  if (NS_SUCCEEDED(err))
  {
    pEncoding.Append("SEARCH");
    if (csname)
      pEncoding.Append(csname);
    pEncoding.Append(imapTerms);
  }
  PR_FREEIF(csname);
  return err;
}


nsresult 
nsMsgSearchValidityManager::InitOfflineMailTable()
{
  NS_ASSERTION(!m_offlineMailTable, "offline mail table already initted");
  nsresult rv = NewTable (getter_AddRefs(m_offlineMailTable));
  NS_ENSURE_SUCCESS(rv,rv);

  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::Sender, nsMsgSearchOp::Contains, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::Sender, nsMsgSearchOp::Contains, 1);
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::Sender, nsMsgSearchOp::DoesntContain, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::Sender, nsMsgSearchOp::DoesntContain, 1);
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::Sender, nsMsgSearchOp::Is, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::Sender, nsMsgSearchOp::Is, 1);
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::Sender, nsMsgSearchOp::Isnt, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::Sender, nsMsgSearchOp::Isnt, 1);
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::Sender, nsMsgSearchOp::BeginsWith, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::Sender, nsMsgSearchOp::BeginsWith, 1);
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::Sender, nsMsgSearchOp::EndsWith, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::Sender, nsMsgSearchOp::EndsWith, 1);
  
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::Sender, nsMsgSearchOp::IsInAB, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::Sender, nsMsgSearchOp::IsInAB, 1);
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::Sender, nsMsgSearchOp::IsntInAB, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::Sender, nsMsgSearchOp::IsntInAB, 1);
  
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::To, nsMsgSearchOp::Contains, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::To, nsMsgSearchOp::Contains, 1);
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::To, nsMsgSearchOp::DoesntContain, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::To, nsMsgSearchOp::DoesntContain, 1);
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::To, nsMsgSearchOp::Is, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::To, nsMsgSearchOp::Is, 1);
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::To, nsMsgSearchOp::Isnt, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::To, nsMsgSearchOp::Isnt, 1);
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::To, nsMsgSearchOp::BeginsWith, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::To, nsMsgSearchOp::BeginsWith, 1);
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::To, nsMsgSearchOp::EndsWith, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::To, nsMsgSearchOp::EndsWith, 1);
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::To, nsMsgSearchOp::IsInAB, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::To, nsMsgSearchOp::IsInAB, 1);
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::To, nsMsgSearchOp::IsntInAB, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::To, nsMsgSearchOp::IsntInAB, 1);

  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::CC, nsMsgSearchOp::Contains, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::CC, nsMsgSearchOp::Contains, 1);
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::CC, nsMsgSearchOp::DoesntContain, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::CC, nsMsgSearchOp::DoesntContain, 1);
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::CC, nsMsgSearchOp::Is, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::CC, nsMsgSearchOp::Is, 1);
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::CC, nsMsgSearchOp::Isnt, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::CC, nsMsgSearchOp::Isnt, 1);
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::CC, nsMsgSearchOp::BeginsWith, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::CC, nsMsgSearchOp::BeginsWith, 1);
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::CC, nsMsgSearchOp::EndsWith, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::CC, nsMsgSearchOp::EndsWith, 1);
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::CC, nsMsgSearchOp::IsInAB, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::CC, nsMsgSearchOp::IsInAB, 1);
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::CC, nsMsgSearchOp::IsntInAB, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::CC, nsMsgSearchOp::IsntInAB, 1);

  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::Contains, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::Contains, 1);
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::DoesntContain, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::DoesntContain, 1);
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::BeginsWith, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::BeginsWith, 1);
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::EndsWith, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::EndsWith, 1);
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::IsInAB, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::IsInAB, 1);
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::IsntInAB, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::IsntInAB, 1);

  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::AllAddresses, nsMsgSearchOp::Contains, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::AllAddresses, nsMsgSearchOp::Contains, 1);
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::AllAddresses, nsMsgSearchOp::DoesntContain, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::AllAddresses, nsMsgSearchOp::DoesntContain, 1);
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::AllAddresses, nsMsgSearchOp::BeginsWith, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::AllAddresses, nsMsgSearchOp::BeginsWith, 1);
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::AllAddresses, nsMsgSearchOp::EndsWith, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::AllAddresses, nsMsgSearchOp::EndsWith, 1);
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::AllAddresses, nsMsgSearchOp::IsInAB, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::AllAddresses, nsMsgSearchOp::IsInAB, 1);
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::AllAddresses, nsMsgSearchOp::IsntInAB, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::AllAddresses, nsMsgSearchOp::IsntInAB, 1);

  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::Subject, nsMsgSearchOp::Contains, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::Subject, nsMsgSearchOp::Contains, 1);
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::Subject, nsMsgSearchOp::DoesntContain, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::Subject, nsMsgSearchOp::DoesntContain, 1);
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::Subject, nsMsgSearchOp::Is, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::Subject, nsMsgSearchOp::Is, 1);
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::Subject, nsMsgSearchOp::Isnt, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::Subject, nsMsgSearchOp::Isnt, 1);
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::Subject, nsMsgSearchOp::BeginsWith, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::Subject, nsMsgSearchOp::BeginsWith, 1);
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::Subject, nsMsgSearchOp::EndsWith, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::Subject, nsMsgSearchOp::EndsWith, 1);
  
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::Body, nsMsgSearchOp::Contains, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::Body, nsMsgSearchOp::Contains, 1);
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::Body, nsMsgSearchOp::DoesntContain, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::Body, nsMsgSearchOp::DoesntContain, 1);
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::Body, nsMsgSearchOp::Is, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::Body, nsMsgSearchOp::Is, 1);
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::Body, nsMsgSearchOp::Isnt, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::Body, nsMsgSearchOp::Isnt, 1);
  
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::Date, nsMsgSearchOp::IsBefore, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::Date, nsMsgSearchOp::IsBefore, 1);
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::Date, nsMsgSearchOp::IsAfter, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::Date, nsMsgSearchOp::IsAfter, 1);
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::Date, nsMsgSearchOp::Is, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::Date, nsMsgSearchOp::Is, 1);
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::Date, nsMsgSearchOp::Isnt, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::Date, nsMsgSearchOp::Isnt, 1);
  
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::Priority, nsMsgSearchOp::IsHigherThan, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::Priority, nsMsgSearchOp::IsHigherThan, 1);
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::Priority, nsMsgSearchOp::IsLowerThan, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::Priority, nsMsgSearchOp::IsLowerThan, 1);
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::Priority, nsMsgSearchOp::Is, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::Priority, nsMsgSearchOp::Is, 1);
  
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::MsgStatus, nsMsgSearchOp::Is, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::MsgStatus, nsMsgSearchOp::Is, 1);
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::MsgStatus, nsMsgSearchOp::Isnt, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::MsgStatus, nsMsgSearchOp::Isnt, 1);
  
  //      m_offlineMailTable->SetValidButNotShown (nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::IsHigherThan, 1);
  //      m_offlineMailTable->SetValidButNotShown (nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::IsLowerThan,  1);
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::IsGreaterThan, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::IsGreaterThan, 1);
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::IsLessThan,  1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::IsLessThan, 1);
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::Is,  1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::Is, 1);
  
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::JunkStatus, nsMsgSearchOp::Is, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::JunkStatus, nsMsgSearchOp::Is, 1);
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::JunkStatus, nsMsgSearchOp::Isnt, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::JunkStatus, nsMsgSearchOp::Isnt, 1);
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::JunkStatus, nsMsgSearchOp::IsEmpty, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::JunkStatus, nsMsgSearchOp::IsEmpty, 1);
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::JunkStatus, nsMsgSearchOp::IsntEmpty, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::JunkStatus, nsMsgSearchOp::IsntEmpty, 1);
  
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::JunkPercent, nsMsgSearchOp::IsGreaterThan, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::JunkPercent, nsMsgSearchOp::IsGreaterThan, 1);
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::JunkPercent, nsMsgSearchOp::IsLessThan, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::JunkPercent, nsMsgSearchOp::IsLessThan, 1);
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::JunkPercent, nsMsgSearchOp::Is, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::JunkPercent, nsMsgSearchOp::Is, 1);

  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::JunkScoreOrigin, nsMsgSearchOp::Is, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::JunkScoreOrigin, nsMsgSearchOp::Is, 1);
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::JunkScoreOrigin, nsMsgSearchOp::Isnt, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::JunkScoreOrigin, nsMsgSearchOp::Isnt, 1);
  
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::HasAttachmentStatus, nsMsgSearchOp::Is, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::HasAttachmentStatus, nsMsgSearchOp::Is, 1);
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::HasAttachmentStatus, nsMsgSearchOp::Isnt, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::HasAttachmentStatus, nsMsgSearchOp::Isnt, 1);
  
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::Size, nsMsgSearchOp::IsGreaterThan, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::Size, nsMsgSearchOp::IsGreaterThan, 1);
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::Size, nsMsgSearchOp::IsLessThan, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::Size, nsMsgSearchOp::IsLessThan, 1);
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::Size, nsMsgSearchOp::Is, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::Size, nsMsgSearchOp::Is, 1);

  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::Contains, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::Contains, 1);
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::DoesntContain, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::DoesntContain, 1);
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::Is, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::Is, 1);
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::Isnt, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::Isnt, 1);
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::BeginsWith, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::BeginsWith, 1);
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::EndsWith, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::EndsWith, 1);

  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::Keywords, nsMsgSearchOp::Contains, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::Keywords, nsMsgSearchOp::Contains, 1);
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::Keywords, nsMsgSearchOp::DoesntContain, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::Keywords, nsMsgSearchOp::DoesntContain, 1);
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::Keywords, nsMsgSearchOp::Is, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::Keywords, nsMsgSearchOp::Is, 1);
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::Keywords, nsMsgSearchOp::Isnt, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::Keywords, nsMsgSearchOp::Isnt, 1);
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::Keywords, nsMsgSearchOp::IsEmpty, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::Keywords, nsMsgSearchOp::IsEmpty, 1);
  m_offlineMailTable->SetAvailable (nsMsgSearchAttrib::Keywords, nsMsgSearchOp::IsntEmpty, 1);
  m_offlineMailTable->SetEnabled   (nsMsgSearchAttrib::Keywords, nsMsgSearchOp::IsntEmpty, 1);
  return rv;
}


nsresult 
nsMsgSearchValidityManager::InitOnlineMailTable()
{
  NS_ASSERTION(!m_onlineMailTable, "Online mail table already initted!");
  nsresult rv = NewTable (getter_AddRefs(m_onlineMailTable));
  NS_ENSURE_SUCCESS(rv,rv);

  m_onlineMailTable->SetAvailable (nsMsgSearchAttrib::Sender, nsMsgSearchOp::Contains, 1);
  m_onlineMailTable->SetEnabled   (nsMsgSearchAttrib::Sender, nsMsgSearchOp::Contains, 1);
  m_onlineMailTable->SetAvailable (nsMsgSearchAttrib::Sender, nsMsgSearchOp::DoesntContain, 1);
  m_onlineMailTable->SetEnabled   (nsMsgSearchAttrib::Sender, nsMsgSearchOp::DoesntContain, 1);
  
  m_onlineMailTable->SetAvailable (nsMsgSearchAttrib::To, nsMsgSearchOp::Contains, 1);
  m_onlineMailTable->SetEnabled   (nsMsgSearchAttrib::To, nsMsgSearchOp::Contains, 1);
  m_onlineMailTable->SetAvailable (nsMsgSearchAttrib::To, nsMsgSearchOp::DoesntContain, 1);
  m_onlineMailTable->SetEnabled   (nsMsgSearchAttrib::To, nsMsgSearchOp::DoesntContain, 1);
  
  m_onlineMailTable->SetAvailable (nsMsgSearchAttrib::CC, nsMsgSearchOp::Contains, 1);
  m_onlineMailTable->SetEnabled   (nsMsgSearchAttrib::CC, nsMsgSearchOp::Contains, 1);
  m_onlineMailTable->SetAvailable (nsMsgSearchAttrib::CC, nsMsgSearchOp::DoesntContain, 1);
  m_onlineMailTable->SetEnabled   (nsMsgSearchAttrib::CC, nsMsgSearchOp::DoesntContain, 1);
  
  m_onlineMailTable->SetAvailable (nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::Contains, 1);
  m_onlineMailTable->SetEnabled   (nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::Contains, 1);
  m_onlineMailTable->SetAvailable (nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::DoesntContain, 1);
  m_onlineMailTable->SetEnabled   (nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::DoesntContain, 1);
  
  m_onlineMailTable->SetAvailable (nsMsgSearchAttrib::Subject, nsMsgSearchOp::Contains, 1);
  m_onlineMailTable->SetEnabled   (nsMsgSearchAttrib::Subject, nsMsgSearchOp::Contains, 1);
  m_onlineMailTable->SetAvailable (nsMsgSearchAttrib::Subject, nsMsgSearchOp::DoesntContain, 1);
  m_onlineMailTable->SetEnabled   (nsMsgSearchAttrib::Subject, nsMsgSearchOp::DoesntContain, 1);
  
  m_onlineMailTable->SetAvailable (nsMsgSearchAttrib::Body, nsMsgSearchOp::Contains, 1);
  m_onlineMailTable->SetEnabled   (nsMsgSearchAttrib::Body, nsMsgSearchOp::Contains, 1);
  m_onlineMailTable->SetAvailable (nsMsgSearchAttrib::Body, nsMsgSearchOp::DoesntContain, 1);
  m_onlineMailTable->SetEnabled   (nsMsgSearchAttrib::Body, nsMsgSearchOp::DoesntContain, 1);
  
  m_onlineMailTable->SetAvailable (nsMsgSearchAttrib::Date, nsMsgSearchOp::IsBefore, 1);
  m_onlineMailTable->SetEnabled   (nsMsgSearchAttrib::Date, nsMsgSearchOp::IsBefore, 1);
  m_onlineMailTable->SetAvailable (nsMsgSearchAttrib::Date, nsMsgSearchOp::IsAfter, 1);
  m_onlineMailTable->SetEnabled   (nsMsgSearchAttrib::Date, nsMsgSearchOp::IsAfter, 1);
  m_onlineMailTable->SetAvailable (nsMsgSearchAttrib::Date, nsMsgSearchOp::Is, 1);
  m_onlineMailTable->SetEnabled   (nsMsgSearchAttrib::Date, nsMsgSearchOp::Is, 1);
  m_onlineMailTable->SetAvailable (nsMsgSearchAttrib::Date, nsMsgSearchOp::Isnt, 1);
  m_onlineMailTable->SetEnabled   (nsMsgSearchAttrib::Date, nsMsgSearchOp::Isnt, 1);
  
  m_onlineMailTable->SetAvailable (nsMsgSearchAttrib::MsgStatus, nsMsgSearchOp::Is, 1);
  m_onlineMailTable->SetEnabled   (nsMsgSearchAttrib::MsgStatus, nsMsgSearchOp::Is, 1);
  m_onlineMailTable->SetAvailable (nsMsgSearchAttrib::MsgStatus, nsMsgSearchOp::Isnt, 1);
  m_onlineMailTable->SetEnabled   (nsMsgSearchAttrib::MsgStatus, nsMsgSearchOp::Isnt, 1);
  
  m_onlineMailTable->SetAvailable (nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::IsGreaterThan, 1);
  m_onlineMailTable->SetEnabled   (nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::IsGreaterThan, 1);
  m_onlineMailTable->SetAvailable (nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::IsLessThan,  1);
  m_onlineMailTable->SetEnabled   (nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::IsLessThan, 1);
  m_onlineMailTable->SetEnabled   (nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::Is, 1);
  m_onlineMailTable->SetAvailable (nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::Is, 1);
  
  m_onlineMailTable->SetAvailable (nsMsgSearchAttrib::Size, nsMsgSearchOp::IsGreaterThan, 1);
  m_onlineMailTable->SetEnabled   (nsMsgSearchAttrib::Size, nsMsgSearchOp::IsGreaterThan, 1);
  m_onlineMailTable->SetAvailable (nsMsgSearchAttrib::Size, nsMsgSearchOp::IsLessThan, 1);
  m_onlineMailTable->SetEnabled   (nsMsgSearchAttrib::Size, nsMsgSearchOp::IsLessThan, 1);

  m_onlineMailTable->SetAvailable (nsMsgSearchAttrib::Keywords, nsMsgSearchOp::Contains, 1);
  m_onlineMailTable->SetEnabled   (nsMsgSearchAttrib::Keywords, nsMsgSearchOp::Contains, 1);
  m_onlineMailTable->SetAvailable (nsMsgSearchAttrib::Keywords, nsMsgSearchOp::DoesntContain, 1);
  m_onlineMailTable->SetEnabled   (nsMsgSearchAttrib::Keywords, nsMsgSearchOp::DoesntContain, 1);

  m_onlineMailTable->SetAvailable (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::Contains, 1);
  m_onlineMailTable->SetEnabled   (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::Contains, 1);
  m_onlineMailTable->SetAvailable (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::Is, 1);
  m_onlineMailTable->SetEnabled   (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::Is, 1);
  m_onlineMailTable->SetAvailable (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::BeginsWith, 1);
  m_onlineMailTable->SetEnabled   (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::BeginsWith, 1);
  m_onlineMailTable->SetAvailable (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::EndsWith, 1);
  m_onlineMailTable->SetEnabled   (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::EndsWith, 1);

  return rv;
}

nsresult 
nsMsgSearchValidityManager::InitOnlineMailFilterTable()
{
  // Oh what a tangled web...
  //
  // IMAP filtering happens on the client, fundamentally using the same
  // capabilities as POP filtering. However, since we don't yet have the 
  // IMAP message body, we can't filter on body attributes. So this table
  // is supposed to be the same as offline mail, except that the body 
  // attribute is omitted  
  NS_ASSERTION(!m_onlineMailFilterTable, "online filter table already initted");
  nsresult rv = NewTable (getter_AddRefs(m_onlineMailFilterTable));
  NS_ENSURE_SUCCESS(rv,rv);
  
  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Sender, nsMsgSearchOp::Contains, 1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Sender, nsMsgSearchOp::Contains, 1);
  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Sender, nsMsgSearchOp::DoesntContain, 1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Sender, nsMsgSearchOp::DoesntContain, 1);
  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Sender, nsMsgSearchOp::Is, 1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Sender, nsMsgSearchOp::Is, 1);
  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Sender, nsMsgSearchOp::Isnt, 1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Sender, nsMsgSearchOp::Isnt, 1);
  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Sender, nsMsgSearchOp::BeginsWith, 1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Sender, nsMsgSearchOp::BeginsWith, 1);
  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Sender, nsMsgSearchOp::EndsWith, 1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Sender, nsMsgSearchOp::EndsWith, 1);
  
  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Sender, nsMsgSearchOp::IsInAB, 1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Sender, nsMsgSearchOp::IsInAB, 1);
  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Sender, nsMsgSearchOp::IsntInAB, 1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Sender, nsMsgSearchOp::IsntInAB, 1);
  
  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::To, nsMsgSearchOp::Contains, 1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::To, nsMsgSearchOp::Contains, 1);
  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::To, nsMsgSearchOp::DoesntContain, 1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::To, nsMsgSearchOp::DoesntContain, 1);
  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::To, nsMsgSearchOp::Is, 1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::To, nsMsgSearchOp::Is, 1);
  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::To, nsMsgSearchOp::Isnt, 1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::To, nsMsgSearchOp::Isnt, 1);
  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::To, nsMsgSearchOp::BeginsWith, 1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::To, nsMsgSearchOp::BeginsWith, 1);
  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::To, nsMsgSearchOp::EndsWith, 1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::To, nsMsgSearchOp::EndsWith, 1);
  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::To, nsMsgSearchOp::IsInAB, 1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::To, nsMsgSearchOp::IsInAB, 1);
  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::To, nsMsgSearchOp::IsntInAB, 1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::To, nsMsgSearchOp::IsntInAB, 1);
  
  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::CC, nsMsgSearchOp::Contains, 1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::CC, nsMsgSearchOp::Contains, 1);
  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::CC, nsMsgSearchOp::DoesntContain, 1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::CC, nsMsgSearchOp::DoesntContain, 1);
  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::CC, nsMsgSearchOp::Is, 1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::CC, nsMsgSearchOp::Is, 1);
  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::CC, nsMsgSearchOp::Isnt, 1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::CC, nsMsgSearchOp::Isnt, 1);
  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::CC, nsMsgSearchOp::BeginsWith, 1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::CC, nsMsgSearchOp::BeginsWith, 1);
  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::CC, nsMsgSearchOp::EndsWith, 1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::CC, nsMsgSearchOp::EndsWith, 1);
  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::CC, nsMsgSearchOp::IsInAB, 1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::CC, nsMsgSearchOp::IsInAB, 1);
  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::CC, nsMsgSearchOp::IsntInAB, 1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::CC, nsMsgSearchOp::IsntInAB, 1);
  
  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::Contains, 1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::Contains, 1);
  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::DoesntContain, 1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::DoesntContain, 1);
  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::BeginsWith, 1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::BeginsWith, 1);
  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::EndsWith, 1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::EndsWith, 1);
  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::IsInAB, 1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::IsInAB, 1);
  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::IsntInAB, 1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::IsntInAB, 1);

  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::AllAddresses, nsMsgSearchOp::Contains, 1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::AllAddresses, nsMsgSearchOp::Contains, 1);
  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::AllAddresses, nsMsgSearchOp::DoesntContain, 1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::AllAddresses, nsMsgSearchOp::DoesntContain, 1);
  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::AllAddresses, nsMsgSearchOp::BeginsWith, 1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::AllAddresses, nsMsgSearchOp::BeginsWith, 1);
  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::AllAddresses, nsMsgSearchOp::EndsWith, 1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::AllAddresses, nsMsgSearchOp::EndsWith, 1);
  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::AllAddresses, nsMsgSearchOp::IsInAB, 1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::AllAddresses, nsMsgSearchOp::IsInAB, 1);
  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::AllAddresses, nsMsgSearchOp::IsntInAB, 1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::AllAddresses, nsMsgSearchOp::IsntInAB, 1);

  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Subject, nsMsgSearchOp::Contains, 1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Subject, nsMsgSearchOp::Contains, 1);
  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Subject, nsMsgSearchOp::DoesntContain, 1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Subject, nsMsgSearchOp::DoesntContain, 1);
  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Subject, nsMsgSearchOp::Is, 1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Subject, nsMsgSearchOp::Is, 1);
  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Subject, nsMsgSearchOp::Isnt, 1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Subject, nsMsgSearchOp::Isnt, 1);
  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Subject, nsMsgSearchOp::BeginsWith, 1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Subject, nsMsgSearchOp::BeginsWith, 1);
  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Subject, nsMsgSearchOp::EndsWith, 1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Subject, nsMsgSearchOp::EndsWith, 1);
  
  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Date, nsMsgSearchOp::IsBefore, 1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Date, nsMsgSearchOp::IsBefore, 1);
  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Date, nsMsgSearchOp::IsAfter, 1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Date, nsMsgSearchOp::IsAfter, 1);
  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Date, nsMsgSearchOp::Is, 1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Date, nsMsgSearchOp::Is, 1);
  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Date, nsMsgSearchOp::Isnt, 1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Date, nsMsgSearchOp::Isnt, 1);
  
  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Priority, nsMsgSearchOp::IsHigherThan, 1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Priority, nsMsgSearchOp::IsHigherThan, 1);
  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Priority, nsMsgSearchOp::IsLowerThan, 1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Priority, nsMsgSearchOp::IsLowerThan, 1);
  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Priority, nsMsgSearchOp::Is, 1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Priority, nsMsgSearchOp::Is, 1);
  
  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::MsgStatus, nsMsgSearchOp::Is, 1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::MsgStatus, nsMsgSearchOp::Is, 1);
  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::MsgStatus, nsMsgSearchOp::Isnt, 1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::MsgStatus, nsMsgSearchOp::Isnt, 1);
  
  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::IsGreaterThan, 1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::IsGreaterThan, 1);
  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::IsLessThan,  1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::IsLessThan, 1);
  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::Is, 1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::Is, 1);
  
  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Size, nsMsgSearchOp::IsGreaterThan, 1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Size, nsMsgSearchOp::IsGreaterThan, 1);
  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Size, nsMsgSearchOp::IsLessThan, 1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Size, nsMsgSearchOp::IsLessThan, 1);

  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Keywords, nsMsgSearchOp::Contains, 1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Keywords, nsMsgSearchOp::Contains, 1);
  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Keywords, nsMsgSearchOp::DoesntContain, 1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Keywords, nsMsgSearchOp::DoesntContain, 1);
  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Keywords, nsMsgSearchOp::Is, 1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Keywords, nsMsgSearchOp::Is, 1);
  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Keywords, nsMsgSearchOp::Isnt, 1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Keywords, nsMsgSearchOp::Isnt, 1);
  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Keywords, nsMsgSearchOp::IsEmpty, 1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Keywords, nsMsgSearchOp::IsEmpty, 1);
  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Keywords, nsMsgSearchOp::IsntEmpty, 1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Keywords, nsMsgSearchOp::IsntEmpty, 1);

  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::Contains, 1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::Contains, 1);
  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::DoesntContain, 1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::DoesntContain, 1);
  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::Is, 1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::Is, 1);
  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::Isnt, 1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::Isnt, 1);
  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::BeginsWith, 1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::BeginsWith, 1);
  m_onlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::EndsWith, 1);
  m_onlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::EndsWith, 1);

  return rv;
}

nsresult 
nsMsgSearchValidityManager::InitOfflineMailFilterTable()
{
  NS_ASSERTION(!m_offlineMailFilterTable, "offline mail filter table already initted");
  nsresult rv = NewTable (getter_AddRefs(m_offlineMailFilterTable));
  NS_ENSURE_SUCCESS(rv,rv);

  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Sender, nsMsgSearchOp::Contains, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Sender, nsMsgSearchOp::Contains, 1);
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Sender, nsMsgSearchOp::DoesntContain, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Sender, nsMsgSearchOp::DoesntContain, 1);
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Sender, nsMsgSearchOp::Is, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Sender, nsMsgSearchOp::Is, 1);
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Sender, nsMsgSearchOp::Isnt, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Sender, nsMsgSearchOp::Isnt, 1);
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Sender, nsMsgSearchOp::BeginsWith, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Sender, nsMsgSearchOp::BeginsWith, 1);
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Sender, nsMsgSearchOp::EndsWith, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Sender, nsMsgSearchOp::EndsWith, 1);
  
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Sender, nsMsgSearchOp::IsInAB, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Sender, nsMsgSearchOp::IsInAB, 1);
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Sender, nsMsgSearchOp::IsntInAB, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Sender, nsMsgSearchOp::IsntInAB, 1);
  
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::To, nsMsgSearchOp::Contains, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::To, nsMsgSearchOp::Contains, 1);
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::To, nsMsgSearchOp::DoesntContain, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::To, nsMsgSearchOp::DoesntContain, 1);
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::To, nsMsgSearchOp::Is, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::To, nsMsgSearchOp::Is, 1);
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::To, nsMsgSearchOp::Isnt, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::To, nsMsgSearchOp::Isnt, 1);
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::To, nsMsgSearchOp::BeginsWith, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::To, nsMsgSearchOp::BeginsWith, 1);
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::To, nsMsgSearchOp::EndsWith, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::To, nsMsgSearchOp::EndsWith, 1);
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::To, nsMsgSearchOp::IsInAB, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::To, nsMsgSearchOp::IsInAB, 1);
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::To, nsMsgSearchOp::IsntInAB, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::To, nsMsgSearchOp::IsntInAB, 1);
  
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::CC, nsMsgSearchOp::Contains, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::CC, nsMsgSearchOp::Contains, 1);
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::CC, nsMsgSearchOp::DoesntContain, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::CC, nsMsgSearchOp::DoesntContain, 1);
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::CC, nsMsgSearchOp::Is, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::CC, nsMsgSearchOp::Is, 1);
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::CC, nsMsgSearchOp::Isnt, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::CC, nsMsgSearchOp::Isnt, 1);
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::CC, nsMsgSearchOp::BeginsWith, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::CC, nsMsgSearchOp::BeginsWith, 1);
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::CC, nsMsgSearchOp::EndsWith, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::CC, nsMsgSearchOp::EndsWith, 1);
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::CC, nsMsgSearchOp::IsInAB, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::CC, nsMsgSearchOp::IsInAB, 1);
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::CC, nsMsgSearchOp::IsntInAB, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::CC, nsMsgSearchOp::IsntInAB, 1);
  
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::Contains, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::Contains, 1);
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::DoesntContain, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::DoesntContain, 1);
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::BeginsWith, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::BeginsWith, 1);
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::EndsWith, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::EndsWith, 1);
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::IsInAB, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::IsInAB, 1);
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::IsntInAB, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::IsntInAB, 1);
  
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::AllAddresses, nsMsgSearchOp::Contains, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::AllAddresses, nsMsgSearchOp::Contains, 1);
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::AllAddresses, nsMsgSearchOp::DoesntContain, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::AllAddresses, nsMsgSearchOp::DoesntContain, 1);
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::AllAddresses, nsMsgSearchOp::BeginsWith, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::AllAddresses, nsMsgSearchOp::BeginsWith, 1);
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::AllAddresses, nsMsgSearchOp::EndsWith, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::AllAddresses, nsMsgSearchOp::EndsWith, 1);
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::AllAddresses, nsMsgSearchOp::IsInAB, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::AllAddresses, nsMsgSearchOp::IsInAB, 1);
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::AllAddresses, nsMsgSearchOp::IsntInAB, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::AllAddresses, nsMsgSearchOp::IsntInAB, 1);
  
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Subject, nsMsgSearchOp::Contains, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Subject, nsMsgSearchOp::Contains, 1);
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Subject, nsMsgSearchOp::DoesntContain, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Subject, nsMsgSearchOp::DoesntContain, 1);
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Subject, nsMsgSearchOp::Is, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Subject, nsMsgSearchOp::Is, 1);
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Subject, nsMsgSearchOp::Isnt, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Subject, nsMsgSearchOp::Isnt, 1);
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Subject, nsMsgSearchOp::BeginsWith, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Subject, nsMsgSearchOp::BeginsWith, 1);
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Subject, nsMsgSearchOp::EndsWith, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Subject, nsMsgSearchOp::EndsWith, 1);
  
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Body, nsMsgSearchOp::Contains, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Body, nsMsgSearchOp::Contains, 1);
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Body, nsMsgSearchOp::DoesntContain, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Body, nsMsgSearchOp::DoesntContain, 1);
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Body, nsMsgSearchOp::Is, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Body, nsMsgSearchOp::Is, 1);
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Body, nsMsgSearchOp::Isnt, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Body, nsMsgSearchOp::Isnt, 1);
  
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Date, nsMsgSearchOp::IsBefore, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Date, nsMsgSearchOp::IsBefore, 1);
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Date, nsMsgSearchOp::IsAfter, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Date, nsMsgSearchOp::IsAfter, 1);
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Date, nsMsgSearchOp::Is, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Date, nsMsgSearchOp::Is, 1);
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Date, nsMsgSearchOp::Isnt, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Date, nsMsgSearchOp::Isnt, 1);
  
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Priority, nsMsgSearchOp::IsHigherThan, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Priority, nsMsgSearchOp::IsHigherThan, 1);
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Priority, nsMsgSearchOp::IsLowerThan, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Priority, nsMsgSearchOp::IsLowerThan, 1);
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Priority, nsMsgSearchOp::Is, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Priority, nsMsgSearchOp::Is, 1);
  
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::MsgStatus, nsMsgSearchOp::Is, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::MsgStatus, nsMsgSearchOp::Is, 1);
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::MsgStatus, nsMsgSearchOp::Isnt, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::MsgStatus, nsMsgSearchOp::Isnt, 1);
  
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::IsGreaterThan, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::IsGreaterThan, 1);
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::IsLessThan,  1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::IsLessThan, 1);
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::Is,  1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::Is, 1);

  // junk status and attachment status not available for offline mail (POP) filters
  // because we won't know those until after the message has been analyzed.
  // see bug #185937
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Size, nsMsgSearchOp::IsGreaterThan, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Size, nsMsgSearchOp::IsGreaterThan, 1);
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Size, nsMsgSearchOp::IsLessThan, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Size, nsMsgSearchOp::IsLessThan, 1);
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Size, nsMsgSearchOp::Is, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Size, nsMsgSearchOp::Is, 1);

  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Keywords, nsMsgSearchOp::Contains, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Keywords, nsMsgSearchOp::Contains, 1);
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Keywords, nsMsgSearchOp::DoesntContain, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Keywords, nsMsgSearchOp::DoesntContain, 1);
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Keywords, nsMsgSearchOp::Is, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Keywords, nsMsgSearchOp::Is, 1);
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Keywords, nsMsgSearchOp::Isnt, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Keywords, nsMsgSearchOp::Isnt, 1);
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Keywords, nsMsgSearchOp::IsEmpty, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Keywords, nsMsgSearchOp::IsEmpty, 1);
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::Keywords, nsMsgSearchOp::IsntEmpty, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::Keywords, nsMsgSearchOp::IsntEmpty, 1);

  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::Contains, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::Contains, 1);
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::DoesntContain, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::DoesntContain, 1);
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::Is, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::Is, 1);
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::Isnt, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::Isnt, 1);
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::BeginsWith, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::BeginsWith, 1);
  m_offlineMailFilterTable->SetAvailable (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::EndsWith, 1);
  m_offlineMailFilterTable->SetEnabled   (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::EndsWith, 1);

  return rv;
}

// Online Manual is used for IMAP and NEWS, where at manual
// filtering we have junk info, but cannot assure that the
// body is available.
nsresult
nsMsgSearchValidityManager::InitOnlineManualFilterTable()
{
  NS_ASSERTION(!m_onlineManualFilterTable, "online manual filter table already initted");
  nsresult rv = NewTable(getter_AddRefs(m_onlineManualFilterTable));
  NS_ENSURE_SUCCESS(rv, rv);

  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::Sender, nsMsgSearchOp::Contains, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::Sender, nsMsgSearchOp::Contains, 1);
  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::Sender, nsMsgSearchOp::DoesntContain, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::Sender, nsMsgSearchOp::DoesntContain, 1);
  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::Sender, nsMsgSearchOp::Is, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::Sender, nsMsgSearchOp::Is, 1);
  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::Sender, nsMsgSearchOp::Isnt, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::Sender, nsMsgSearchOp::Isnt, 1);
  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::Sender, nsMsgSearchOp::BeginsWith, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::Sender, nsMsgSearchOp::BeginsWith, 1);
  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::Sender, nsMsgSearchOp::EndsWith, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::Sender, nsMsgSearchOp::EndsWith, 1);

  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::Sender, nsMsgSearchOp::IsInAB, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::Sender, nsMsgSearchOp::IsInAB, 1);
  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::Sender, nsMsgSearchOp::IsntInAB, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::Sender, nsMsgSearchOp::IsntInAB, 1);

  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::To, nsMsgSearchOp::Contains, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::To, nsMsgSearchOp::Contains, 1);
  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::To, nsMsgSearchOp::DoesntContain, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::To, nsMsgSearchOp::DoesntContain, 1);
  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::To, nsMsgSearchOp::Is, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::To, nsMsgSearchOp::Is, 1);
  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::To, nsMsgSearchOp::Isnt, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::To, nsMsgSearchOp::Isnt, 1);
  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::To, nsMsgSearchOp::BeginsWith, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::To, nsMsgSearchOp::BeginsWith, 1);
  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::To, nsMsgSearchOp::EndsWith, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::To, nsMsgSearchOp::EndsWith, 1);
  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::To, nsMsgSearchOp::IsInAB, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::To, nsMsgSearchOp::IsInAB, 1);
  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::To, nsMsgSearchOp::IsntInAB, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::To, nsMsgSearchOp::IsntInAB, 1);

  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::CC, nsMsgSearchOp::Contains, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::CC, nsMsgSearchOp::Contains, 1);
  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::CC, nsMsgSearchOp::DoesntContain, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::CC, nsMsgSearchOp::DoesntContain, 1);
  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::CC, nsMsgSearchOp::Is, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::CC, nsMsgSearchOp::Is, 1);
  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::CC, nsMsgSearchOp::Isnt, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::CC, nsMsgSearchOp::Isnt, 1);
  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::CC, nsMsgSearchOp::BeginsWith, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::CC, nsMsgSearchOp::BeginsWith, 1);
  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::CC, nsMsgSearchOp::EndsWith, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::CC, nsMsgSearchOp::EndsWith, 1);
  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::CC, nsMsgSearchOp::IsInAB, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::CC, nsMsgSearchOp::IsInAB, 1);
  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::CC, nsMsgSearchOp::IsntInAB, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::CC, nsMsgSearchOp::IsntInAB, 1);

  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::Contains, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::Contains, 1);
  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::DoesntContain, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::DoesntContain, 1);
  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::BeginsWith, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::BeginsWith, 1);
  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::EndsWith, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::EndsWith, 1);
  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::IsInAB, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::IsInAB, 1);
  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::IsntInAB, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::ToOrCC, nsMsgSearchOp::IsntInAB, 1);

  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::AllAddresses, nsMsgSearchOp::Contains, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::AllAddresses, nsMsgSearchOp::Contains, 1);
  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::AllAddresses, nsMsgSearchOp::DoesntContain, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::AllAddresses, nsMsgSearchOp::DoesntContain, 1);
  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::AllAddresses, nsMsgSearchOp::BeginsWith, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::AllAddresses, nsMsgSearchOp::BeginsWith, 1);
  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::AllAddresses, nsMsgSearchOp::EndsWith, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::AllAddresses, nsMsgSearchOp::EndsWith, 1);
  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::AllAddresses, nsMsgSearchOp::IsInAB, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::AllAddresses, nsMsgSearchOp::IsInAB, 1);
  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::AllAddresses, nsMsgSearchOp::IsntInAB, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::AllAddresses, nsMsgSearchOp::IsntInAB, 1);

  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::Subject, nsMsgSearchOp::Contains, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::Subject, nsMsgSearchOp::Contains, 1);
  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::Subject, nsMsgSearchOp::DoesntContain, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::Subject, nsMsgSearchOp::DoesntContain, 1);
  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::Subject, nsMsgSearchOp::Is, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::Subject, nsMsgSearchOp::Is, 1);
  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::Subject, nsMsgSearchOp::Isnt, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::Subject, nsMsgSearchOp::Isnt, 1);
  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::Subject, nsMsgSearchOp::BeginsWith, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::Subject, nsMsgSearchOp::BeginsWith, 1);
  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::Subject, nsMsgSearchOp::EndsWith, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::Subject, nsMsgSearchOp::EndsWith, 1);

  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::Date, nsMsgSearchOp::IsBefore, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::Date, nsMsgSearchOp::IsBefore, 1);
  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::Date, nsMsgSearchOp::IsAfter, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::Date, nsMsgSearchOp::IsAfter, 1);
  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::Date, nsMsgSearchOp::Is, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::Date, nsMsgSearchOp::Is, 1);
  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::Date, nsMsgSearchOp::Isnt, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::Date, nsMsgSearchOp::Isnt, 1);

  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::Priority, nsMsgSearchOp::IsHigherThan, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::Priority, nsMsgSearchOp::IsHigherThan, 1);
  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::Priority, nsMsgSearchOp::IsLowerThan, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::Priority, nsMsgSearchOp::IsLowerThan, 1);
  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::Priority, nsMsgSearchOp::Is, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::Priority, nsMsgSearchOp::Is, 1);

  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::MsgStatus, nsMsgSearchOp::Is, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::MsgStatus, nsMsgSearchOp::Is, 1);
  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::MsgStatus, nsMsgSearchOp::Isnt, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::MsgStatus, nsMsgSearchOp::Isnt, 1);

  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::IsGreaterThan, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::IsGreaterThan, 1);
  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::IsLessThan,  1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::IsLessThan, 1);
  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::Is,  1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::Is, 1);

  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::JunkStatus, nsMsgSearchOp::Is, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::JunkStatus, nsMsgSearchOp::Is, 1);
  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::JunkStatus, nsMsgSearchOp::Isnt, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::JunkStatus, nsMsgSearchOp::Isnt, 1);
  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::JunkStatus, nsMsgSearchOp::IsEmpty, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::JunkStatus, nsMsgSearchOp::IsEmpty, 1);
  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::JunkStatus, nsMsgSearchOp::IsntEmpty, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::JunkStatus, nsMsgSearchOp::IsntEmpty, 1);

  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::JunkPercent, nsMsgSearchOp::IsGreaterThan, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::JunkPercent, nsMsgSearchOp::IsGreaterThan, 1);
  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::JunkPercent, nsMsgSearchOp::IsLessThan, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::JunkPercent, nsMsgSearchOp::IsLessThan, 1);
  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::JunkPercent, nsMsgSearchOp::Is, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::JunkPercent, nsMsgSearchOp::Is, 1);

  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::JunkScoreOrigin, nsMsgSearchOp::Is, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::JunkScoreOrigin, nsMsgSearchOp::Is, 1);
  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::JunkScoreOrigin, nsMsgSearchOp::Isnt, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::JunkScoreOrigin, nsMsgSearchOp::Isnt, 1);

  // HasAttachmentStatus does not work reliably until the user has opened a
  // message to force it through MIME. We need a solution for this (bug 105169)
  // but in the meantime, I'm doing the same thing here that we do in the
  // offline mail table, as this does not really depend at the moment on
  // whether we have downloaded the body for offline use.
  m_onlineManualFilterTable->SetAvailable (nsMsgSearchAttrib::HasAttachmentStatus, nsMsgSearchOp::Is, 1);
  m_onlineManualFilterTable->SetEnabled   (nsMsgSearchAttrib::HasAttachmentStatus, nsMsgSearchOp::Is, 1);
  m_onlineManualFilterTable->SetAvailable (nsMsgSearchAttrib::HasAttachmentStatus, nsMsgSearchOp::Isnt, 1);
  m_onlineManualFilterTable->SetEnabled   (nsMsgSearchAttrib::HasAttachmentStatus, nsMsgSearchOp::Isnt, 1);

  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::Size, nsMsgSearchOp::IsGreaterThan, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::Size, nsMsgSearchOp::IsGreaterThan, 1);
  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::Size, nsMsgSearchOp::IsLessThan, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::Size, nsMsgSearchOp::IsLessThan, 1);
  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::Size, nsMsgSearchOp::Is, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::Size, nsMsgSearchOp::Is, 1);

  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::Keywords, nsMsgSearchOp::Contains, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::Keywords, nsMsgSearchOp::Contains, 1);
  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::Keywords, nsMsgSearchOp::DoesntContain, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::Keywords, nsMsgSearchOp::DoesntContain, 1);
  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::Keywords, nsMsgSearchOp::Is, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::Keywords, nsMsgSearchOp::Is, 1);
  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::Keywords, nsMsgSearchOp::Isnt, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::Keywords, nsMsgSearchOp::Isnt, 1);
  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::Keywords, nsMsgSearchOp::IsEmpty, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::Keywords, nsMsgSearchOp::IsEmpty, 1);
  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::Keywords, nsMsgSearchOp::IsntEmpty, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::Keywords, nsMsgSearchOp::IsntEmpty, 1);

  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::Contains, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::Contains, 1);
  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::DoesntContain, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::DoesntContain, 1);
  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::Is, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::Is, 1);
  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::Isnt, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::Isnt, 1);
  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::BeginsWith, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::BeginsWith, 1);
  m_onlineManualFilterTable->SetAvailable(nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::EndsWith, 1);
  m_onlineManualFilterTable->SetEnabled(nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::EndsWith, 1);

  return rv;
}
