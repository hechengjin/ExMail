/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsEudoraAddress.h"

#include "nsIAddrDatabase.h"
#include "mdb.h"
#include "nsAbBaseCID.h"
#include "nsIAbCard.h"
#include "nsIServiceManager.h"
#include "nsEudoraImport.h"
#include "nsEudoraCompose.h"
#include "nsMsgUtils.h"
#include "nsTextFormatter.h"
#include "nsEudoraStringBundle.h"
#include "nsIStringBundle.h"
#include "nsNetUtil.h"
#include "nsILineInputStream.h"
#include "EudoraDebugLog.h"

#define  kWhitespace  " \t\b\r\n"

#define ADD_FIELD_TO_DB_ROW(pdb, func, dbRow, val, uniStr)    \
  if (!val.IsEmpty())                                         \
  {                                                           \
        NS_CopyNativeToUnicode(val, uniStr);                  \
        pdb->func(dbRow, NS_ConvertUTF16toUTF8(uniStr).get());\
  }


// If we get a line longer than 16K it's just toooooo bad!
#define kEudoraAddressBufferSz  (16 * 1024)


#ifdef IMPORT_DEBUG
void DumpAliasArray(nsVoidArray& a);
#endif

class CAliasData {
public:
  CAliasData() {}
  ~CAliasData() {}

  bool Process(const char *pLine, int32_t len);

public:
    nsCString   m_fullEntry;
    nsCString   m_nickName;
    nsCString   m_realName;
    nsCString   m_email;
};

class CAliasEntry {
public:
  CAliasEntry(nsCString& name) { m_name = name;}
  ~CAliasEntry() { EmptyList();}

  void EmptyList(void) {
    CAliasData *pData;
    for (int32_t i = 0; i < m_list.Count(); i++) {
      pData = (CAliasData *)m_list.ElementAt(i);
      delete pData;
    }
    m_list.Clear();
  }

public:
  nsCString  m_name;
  nsVoidArray  m_list;
  nsCString  m_notes;
};

nsEudoraAddress::nsEudoraAddress()
{
}

nsEudoraAddress::~nsEudoraAddress()
{
  EmptyAliases();
}


nsresult nsEudoraAddress::ImportAddresses(uint32_t *pBytes, bool *pAbort,
                                          const PRUnichar *pName, nsIFile *pSrc,
                                          nsIAddrDatabase *pDb, nsString& errors)
{
  // Open the source file for reading, read each line and process it!

  EmptyAliases();
  nsCOMPtr<nsIInputStream> inputStream;
  nsresult rv = NS_NewLocalFileInputStream(getter_AddRefs(inputStream), pSrc);
  if (NS_FAILED(rv)) {
    IMPORT_LOG0("*** Error opening address file for reading\n");
    return rv;
  }

  uint64_t bytesLeft = 0;

  rv = inputStream->Available(&bytesLeft);
  if (NS_FAILED(rv)) {
    IMPORT_LOG0("*** Error checking address file for eof\n");
    inputStream->Close();
    return rv;
  }

  nsCOMPtr<nsILineInputStream> lineStream(do_QueryInterface(inputStream, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  bool more = true;

  while ((!(*pAbort) && more && NS_SUCCEEDED(rv)))
  {
    nsCString line;
    rv = lineStream->ReadLine(line, &more);
    if (NS_SUCCEEDED(rv))
    {
      int32_t  len = line.Length();
      ProcessLine(line.get(), len, errors);
      if (pBytes)
        *pBytes += (len / 2);
    }
  }
  rv = inputStream->Close();

  if (more)
  {
    IMPORT_LOG0("*** Error reading the address book, didn't reach the end\n");
    return NS_ERROR_FAILURE;
  }
  // Run through the alias array and make address book entries...
#ifdef IMPORT_DEBUG
  DumpAliasArray(m_alias);
#endif

  BuildABCards(pBytes, pDb);

  return pDb->Commit(nsAddrDBCommitType::kLargeCommit);
  }


int32_t nsEudoraAddress::CountWhiteSpace(const char *pLine, int32_t len)
{
  int32_t    cnt = 0;
  while (len && ((*pLine == ' ') || (*pLine == '\t'))) {
    len--;
    pLine++;
    cnt++;
  }

  return cnt;
}

void nsEudoraAddress::EmptyAliases(void)
{
  CAliasEntry *pData;
  for (int32_t i = 0; i < m_alias.Count(); i++) {
    pData = (CAliasEntry *)m_alias.ElementAt(i);
    delete pData;
  }
  m_alias.Clear();
}

void nsEudoraAddress::ProcessLine(const char *pLine, int32_t len, nsString& errors)
{
  if (len < 6)
    return;

  int32_t  cnt;
  CAliasEntry  *pEntry;

  if (!strncmp(pLine, "alias", 5)) {
    pLine += 5;
    len -= 5;
    cnt = CountWhiteSpace(pLine, len);
    if (cnt) {
      pLine += cnt;
      len -= cnt;
      if ((pEntry = ProcessAlias(pLine, len, errors)) != nullptr)
        m_alias.AppendElement(pEntry);
    }
  }
  else if (!strncmp(pLine, "note", 4)) {
    pLine += 4;
    len -= 4;
    cnt = CountWhiteSpace(pLine, len);
    if (cnt) {
      pLine += cnt;
      len -= cnt;
      ProcessNote(pLine, len, errors);
    }
  }

  // as far as I know everything must be on one line
  // if not, then I need to add a state variable.
}

int32_t nsEudoraAddress::GetAliasName(const char *pLine, int32_t len, nsCString& name)
{
  name.Truncate();
  if (!len)
    return 0;
  const char *pStart = pLine;
  char  end[2] = {' ', '\t'};
  if (*pLine == '"') {
    pLine++;
    pStart++;
    len--;
    end[0] = '"';
    end[1] = 0;
  }

  int32_t cnt = 0;
  while (len) {
    if ((*pLine == end[0]) || (*pLine == end[1]))
      break;
    len--;
    pLine++;
    cnt++;
  }

  if (cnt)
    name.Append(pStart, cnt);

  if (end[0] == '"') {
    cnt++;
    if (len && (*pLine == '"')) {
      cnt++;
      pLine++;
      len--;
    }
  }

  cnt += CountWhiteSpace(pLine, len);

  return cnt;
}


CAliasEntry *nsEudoraAddress::ProcessAlias(const char *pLine, int32_t len, nsString& errors)
{
  nsCString  name;
  int32_t    cnt = GetAliasName(pLine, len, name);
  pLine += cnt;
  len -= cnt;

  // we have 3 known forms of addresses in Eudora
  // 1) real name <email@address>
  // 2) email@address
  // 3) <email@address>
  // 4) real name email@address
  // 5) <email@address> (Real name)

  CAliasEntry *pEntry = new CAliasEntry(name);
  if (!cnt || !len)
    return pEntry;

  // Theoretically, an alias is just an RFC822 email adress, but it may contain
  // an alias to another alias as the email!  I general, it appears close
  // but unfortunately not exact so we can't use the nsIMsgHeaderParser to do
  // the work for us!

  // Very big bummer!

  const char *pStart;
  int32_t    tLen;
  nsCString  alias;

  while (len) {
    pStart = pLine;
    cnt = 0;
    while (len && (*pLine != ',')) {
      if (*pLine == '"') {
        tLen = CountQuote(pLine, len);
        pLine += tLen;
        len -= tLen;
        cnt += tLen;
      }
      else if (*pLine == '(') {
        tLen = CountComment(pLine, len);
        pLine += tLen;
        len -= tLen;
        cnt += tLen;
      }
      else if (*pLine == '<') {
        tLen = CountAngle(pLine, len);
        pLine += tLen;
        len -= tLen;
        cnt += tLen;
      }
      else {
        cnt++;
        pLine++;
        len--;
      }
    }
    if (cnt) {
      CAliasData *pData = new CAliasData();
      if (pData->Process(pStart, cnt))
        pEntry->m_list.AppendElement(pData);
      else
        delete pData;
    }

    if (len && (*pLine == ',')) {
      pLine++;
      len--;
    }
  }

  // Always return the entry even if there's no other attribute associated with the contact.
  return pEntry;
}


void nsEudoraAddress::ProcessNote(const char *pLine, int32_t len, nsString& errors)
{
  nsCString  name;
  int32_t    cnt = GetAliasName(pLine, len, name);
  pLine += cnt;
  len -= cnt;
  if (!cnt || !len)
    return;

  // Find the alias for this note and store the note data there!
  CAliasEntry *pEntry = nullptr;
  int32_t  idx = FindAlias(name);
  if (idx == -1)
    return;

  pEntry = (CAliasEntry *) m_alias.ElementAt(idx);
  pEntry->m_notes.Append(pLine, len);
  pEntry->m_notes.Trim(kWhitespace);
}



int32_t nsEudoraAddress::CountQuote(const char *pLine, int32_t len)
{
  if (!len)
    return 0;

  int32_t cnt = 1;
  pLine++;
  len--;

  while (len && (*pLine != '"')) {
    cnt++;
    len--;
    pLine++;
  }

  if (len)
    cnt++;
  return cnt;
}


int32_t nsEudoraAddress::CountAngle(const char *pLine, int32_t len)
{
  if (!len)
    return 0;

  int32_t cnt = 1;
  pLine++;
  len--;

  while (len && (*pLine != '>')) {
    cnt++;
    len--;
    pLine++;
  }

  if (len)
    cnt++;
  return cnt;
}

int32_t nsEudoraAddress::CountComment(const char *pLine, int32_t len)
{
  if (!len)
    return 0;

  int32_t  cCnt;
  int32_t cnt = 1;
  pLine++;
  len--;

  while (len && (*pLine != ')')) {
    if (*pLine == '(') {
      cCnt = CountComment(pLine, len);
      cnt += cCnt;
      pLine += cCnt;
      len -= cCnt;
    }
    else {
      cnt++;
      len--;
      pLine++;
    }
  }

  if (len)
    cnt++;
  return cnt;
}

/*
  nsCString  m_nickName;
  nsCString  m_realName;
  nsCString  m_email;
*/

bool CAliasData::Process(const char *pLine, int32_t len)
{
  // Extract any comments first!
  nsCString  str;

  const char *pStart = pLine;
  int32_t    tCnt = 0;
  int32_t    cnt = 0;
  int32_t    max = len;
  bool      endCollect = false;

    // Keep track of the full entry without any processing for potential alias
    // nickname resolution. Previously alias resolution was done with m_email,
    // but unfortunately that doesn't work for nicknames with spaces.
    // For example for the nickname "Joe Smith", "Smith" was being interpreted
    // as the potential email address and placed in m_email, but routines like
    // ResolveAlias were failing because "Smith" is not the full nickname.
    // Now we just stash the full entry for nickname resolution before processing
    // the line as a potential entry in its own right.
    m_fullEntry.Append(pLine, len);

  while (max) {
    if (*pLine == '"') {
      if (tCnt && !endCollect) {
        str.Trim(kWhitespace);
        if (!str.IsEmpty())
          str.Append(" ", 1);
        str.Append(pStart, tCnt);
      }
      cnt = nsEudoraAddress::CountQuote(pLine, max);
      if ((cnt > 2) && m_realName.IsEmpty()) {
        m_realName.Append(pLine + 1, cnt - 2);
      }
      pLine += cnt;
      max -= cnt;
      pStart = pLine;
      tCnt = 0;
    }
    else if (*pLine == '<') {
      if (tCnt && !endCollect) {
        str.Trim(kWhitespace);
        if (!str.IsEmpty())
          str.Append(" ", 1);
        str.Append(pStart, tCnt);
      }
      cnt = nsEudoraAddress::CountAngle(pLine, max);
      if ((cnt > 2) && m_email.IsEmpty()) {
        m_email.Append(pLine + 1, cnt - 2);
      }
      pLine += cnt;
      max -= cnt;
      pStart = pLine;
      tCnt = 0;
      endCollect = true;
    }
    else if (*pLine == '(') {
      if (tCnt && !endCollect) {
        str.Trim(kWhitespace);
        if (!str.IsEmpty())
          str.Append(" ", 1);
        str.Append(pStart, tCnt);
      }
      cnt = nsEudoraAddress::CountComment(pLine, max);
      if (cnt > 2) {
        if (!m_realName.IsEmpty() && m_nickName.IsEmpty())
          m_nickName = m_realName;
        m_realName.Truncate();
        m_realName.Append(pLine + 1, cnt - 2);
      }
      pLine += cnt;
      max -= cnt;
      pStart = pLine;
      tCnt = 0;
    }
    else {
      tCnt++;
      pLine++;
      max--;
    }
  }

  if (tCnt) {
    str.Trim(kWhitespace);
    if (!str.IsEmpty())
      str.Append(" ", 1);
    str.Append(pStart, tCnt);
  }

  str.Trim(kWhitespace);

  if (!m_realName.IsEmpty() && !m_email.IsEmpty())
    return true;

  // now we should have a string with any remaining non-delimitted text
  // we assume that the last token is the email
  // anything before that is realName
  if (!m_email.IsEmpty()) {
    m_realName = str;
    return true;
  }

  tCnt = str.RFindChar(' ');
  if (tCnt == -1) {
    if (!str.IsEmpty()) {
      m_email = str;
      return true;
    }
    return false;
  }

  m_email = Substring(str, tCnt + 1);
  m_realName = StringHead(str, tCnt);
  m_realName.Trim(kWhitespace);
  m_email.Trim(kWhitespace);

  return !m_email.IsEmpty();
}

#ifdef IMPORT_DEBUG
void DumpAliasArray(nsVoidArray& a)
{
  CAliasEntry *pEntry;
  CAliasData *pData;

  int32_t cnt = a.Count();
  IMPORT_LOG1("Alias list size: %ld\n", cnt);
  for (int32_t i = 0; i < cnt; i++) {
    pEntry = (CAliasEntry *)a.ElementAt(i);
    IMPORT_LOG1("\tAlias: %s\n", pEntry->m_name.get());
    if (pEntry->m_list.Count() > 1) {
      IMPORT_LOG1("\tList count #%ld\n", pEntry->m_list.Count());
      for (int32_t j = 0; j < pEntry->m_list.Count(); j++) {
        pData = (CAliasData *) pEntry->m_list.ElementAt(j);
        IMPORT_LOG0("\t\t--------\n");
        IMPORT_LOG1("\t\temail: %s\n", pData->m_email.get());
        IMPORT_LOG1("\t\trealName: %s\n", pData->m_realName.get());
        IMPORT_LOG1("\t\tnickName: %s\n", pData->m_nickName.get());
      }
    }
    else if (pEntry->m_list.Count()) {
      pData = (CAliasData *) pEntry->m_list.ElementAt(0);
      IMPORT_LOG1("\t\temail: %s\n", pData->m_email.get());
      IMPORT_LOG1("\t\trealName: %s\n", pData->m_realName.get());
      IMPORT_LOG1("\t\tnickName: %s\n", pData->m_nickName.get());
    }
  }
}
#endif

CAliasEntry *nsEudoraAddress::ResolveAlias(nsCString& name)
{
  int32_t  max = m_alias.Count();
  CAliasEntry *pEntry;
  for (int32_t i = 0; i < max; i++) {
    pEntry = (CAliasEntry *) m_alias.ElementAt(i);
    if (name.Equals(pEntry->m_name, nsCaseInsensitiveCStringComparator()))
      return pEntry;
  }

  return nullptr;
}

void nsEudoraAddress::ResolveEntries(nsCString& name, nsVoidArray& list,
                                     nsVoidArray& result, bool addResolvedEntries,
                                     bool wasResolved, int32_t& numResolved)
{
    /* a safe-guard against recursive entries */
    if (result.Count() > m_alias.Count())
        return;

    int32_t         max = list.Count();
    int32_t         i;
    CAliasData *    pData;
    CAliasEntry *   pEntry;
    for (i = 0; i < max; i++) {
        pData = (CAliasData *)list.ElementAt(i);
        // resolve the email to an existing alias!
        if (!name.Equals(pData->m_email, nsCaseInsensitiveCStringComparator()) &&
             ((pEntry = ResolveAlias(pData->m_fullEntry)) != nullptr)) {
            // This new entry has all of the entries for this puppie.
            // Resolve all of it's entries!
            numResolved++;  // Track the number of entries resolved

            // We pass in true for the 5th parameter so that we know that we're
            // calling ourselves recursively.
            ResolveEntries(pEntry->m_name,
                           pEntry->m_list,
                           result,
                           addResolvedEntries,
                           true,
                           numResolved);
        }
        else if (addResolvedEntries || !wasResolved) {
            // This is either an ordinary entry (i.e. just contains the info) or we were told
            // to add resolved alias entries.
            result.AppendElement(pData);
        }
    }
}

int32_t nsEudoraAddress::FindAlias(nsCString& name)
{
  CAliasEntry *  pEntry;
  int32_t      max = m_alias.Count();
  int32_t      i;

  for (i = 0; i < max; i++) {
    pEntry = (CAliasEntry *) m_alias.ElementAt(i);
    if (pEntry->m_name == name)
      return i;
  }

  return -1;
}

void nsEudoraAddress::BuildABCards(uint32_t *pBytes, nsIAddrDatabase *pDb)
{
  CAliasEntry *  pEntry;
  int32_t      max = m_alias.Count();
  int32_t      i;
  nsVoidArray    emailList;
  nsVoidArray membersArray;// Remember group members.
  nsVoidArray groupsArray; // Remember groups.

  // First off, run through the list and build person cards - groups/lists have to be done later
  for (i = 0; i < max; i++) {
    int32_t   numResolved = 0;
    pEntry = (CAliasEntry *) m_alias.ElementAt(i);

    // false for 4th parameter tells ResolveEntries not to add resolved entries (avoids
    // duplicates as mailing lists are being resolved to other cards - the other cards that
    // are found have already been added and don't need to be added again).
    //
    // false for 5th parameter tells ResolveEntries that we're calling it - it's not being
    // called recursively by itself.
    ResolveEntries(pEntry->m_name, pEntry->m_list, emailList, false, false, numResolved);

    // Treat it as a group if there's more than one email address or if we
    // needed to resolve one or more aliases. We treat single aliases to
    // other aliases as a mailing list because there's no better equivalent.
    if ((emailList.Count() > 1) || (numResolved > 0))
    {
      // Remember group members uniquely and add them to db later.
      RememberGroupMembers(membersArray, emailList);
      // Remember groups and add them to db later.
      groupsArray.AppendElement(pEntry);
    }
    else
      AddSingleCard(pEntry, emailList, pDb);

    emailList.Clear();

    if (pBytes) {
      // This isn't exact but it will get us close enough
      *pBytes += (pEntry->m_name.Length() + pEntry->m_notes.Length() + 10);
    }
  }

  // Make sure group members exists before adding groups.
  nsresult rv = AddGroupMembersAsCards(membersArray, pDb);
  if (NS_FAILED(rv))
    return;

  // Now add the lists/groups (now that all cards have been added).
  max = groupsArray.Count();
  for (i = 0; i < max; i++)
  {
    int32_t   numResolved = 0;
    pEntry = (CAliasEntry *) groupsArray.ElementAt(i);

    // false for 4th parameter tells ResolveEntries to add resolved entries so that we
    // can create the mailing list with references to all entries correctly.
    //
    // false for 5th parameter tells ResolveEntries that we're calling it - it's not being
    // called recursively by itself.
    ResolveEntries(pEntry->m_name, pEntry->m_list, emailList, true, false, numResolved);
    AddSingleList(pEntry, emailList, pDb);
    emailList.Clear();
  }
}

void nsEudoraAddress::ExtractNoteField(nsCString& note, nsCString& value, const char *pFieldName)
{
  value.Truncate();
  nsCString field("<");
  field.Append(pFieldName);
  field.Append(':');

/*
    this is a bit of a cheat, but there's no reason it won't work
    fine for us, even better than Eudora in some cases!
*/

  int32_t idx = note.Find(field);
  if (idx != -1) {
    idx += field.Length();
    int32_t endIdx = note.FindChar('>', idx);
    if (endIdx == -1)
      endIdx = note.Length() - 1;
    value = Substring(note, idx, endIdx - idx);
    idx -= field.Length();
    note.Cut(idx, endIdx + 1);
  }
}

void nsEudoraAddress::FormatExtraDataInNoteField(int32_t labelStringID, nsCString& extraData, nsString& noteUTF16)
{
  nsAutoString    label;
  nsEudoraStringBundle::GetStringByID(labelStringID, label);

  noteUTF16.Append(label);
  noteUTF16.AppendLiteral("\n");
  noteUTF16.Append(NS_ConvertASCIItoUTF16(extraData));
  noteUTF16.AppendLiteral("\n\n");
}

void nsEudoraAddress::SanitizeValue(nsCString& val)
{
  MsgReplaceSubstring(val, "\n", ", ");
  MsgReplaceChar(val, '\r', ',');
}

void nsEudoraAddress::SplitString(nsCString& val1, nsCString& val2)
{
  nsCString  temp;

  // Find the last line if there is more than one!
  int32_t idx = val1.RFind("\x0D\x0A");
  int32_t  cnt = 2;
  if (idx == -1) {
    cnt = 1;
    idx = val1.RFindChar(13);
  }
  if (idx == -1)
    idx= val1.RFindChar(10);
  if (idx != -1) {
    val2 = Substring(val1, idx + cnt);
    val1.SetLength(idx);
    SanitizeValue(val1);
  }
}

void nsEudoraAddress::AddSingleCard(CAliasEntry *pEntry, nsVoidArray &emailList, nsIAddrDatabase *pDb)
{
  // We always have a nickname and everything else is optional.
  // Map both home and work related fields to our address card. Eudora
  // fields that can't be mapped will be left in the 'note' field!
  nsIMdbRow* newRow = nullptr;
  pDb->GetNewRow(&newRow);
  if (!newRow)
    return;

  nsCString                   displayName, name, firstName, lastName;
  nsCString                   fax, secondaryFax, phone, mobile, secondaryMobile, webLink;
  nsCString                   address, address2, city, state, zip, country;
  nsCString                   phoneWK, webLinkWK, title, company;
  nsCString                   addressWK, address2WK, cityWK, stateWK, zipWK, countryWK;
  nsCString                   primaryLocation, otherPhone, otherWeb;
  nsCString                   additionalEmail, stillMoreEmail;
  nsCString                   note(pEntry->m_notes);
  nsString                    noteUTF16;
  bool                        isSecondaryMobileWorkNumber = true;
  bool                        isSecondaryFaxWorkNumber = true;

  if (!note.IsEmpty())
  {
    ExtractNoteField(note, fax, "fax");
    ExtractNoteField(note, secondaryFax, "fax2");
    ExtractNoteField(note, phone, "phone");
    ExtractNoteField(note, mobile, "mobile");
    ExtractNoteField(note, secondaryMobile, "mobile2");
    ExtractNoteField(note, address, "address");
    ExtractNoteField(note, city, "city");
    ExtractNoteField(note, state, "state");
    ExtractNoteField(note, zip, "zip");
    ExtractNoteField(note, country, "country");
    ExtractNoteField(note, name, "name");
    ExtractNoteField(note, firstName, "first");
    ExtractNoteField(note, lastName, "last");
    ExtractNoteField(note, webLink, "web");

    ExtractNoteField(note, addressWK, "address2");
    ExtractNoteField(note, cityWK, "city2");
    ExtractNoteField(note, stateWK, "state2");
    ExtractNoteField(note, zipWK, "zip2");
    ExtractNoteField(note, countryWK, "country2");
    ExtractNoteField(note, phoneWK, "phone2");
    ExtractNoteField(note, title, "title");
    ExtractNoteField(note, company, "company");
    ExtractNoteField(note, webLinkWK, "web2");

    ExtractNoteField(note, primaryLocation, "primary");
    ExtractNoteField(note, additionalEmail, "otheremail");
    ExtractNoteField(note, otherPhone, "otherphone");
    ExtractNoteField(note, otherWeb, "otherweb");

    // Is there any "extra" data that we may want to format nicely and place
    // in the notes field?
    if (!additionalEmail.IsEmpty() || !otherPhone.IsEmpty() || !otherWeb.IsEmpty())
    {
      nsCString     otherNotes(note);

      if (!additionalEmail.IsEmpty())
      {
        // Reconstitute line breaks for additional email
        MsgReplaceSubstring(additionalEmail, "\x03", "\n");

        // Try to figure out if there are multiple email addresses in additionalEmail
        int32_t     idx = MsgFindCharInSet(additionalEmail, "\t\r\n,; ");

        if (idx != -1)
        {
          // We found a character that indicates that there's more than one email address here.
          // Separate out the addresses after the first one.
          stillMoreEmail = Substring(additionalEmail, idx + 1);
          stillMoreEmail.Trim(kWhitespace);

          // Separate out the first address.
          additionalEmail.SetLength(idx);
        }

        // If there were more than one additional email addresses store all the extra
        // ones in the notes field, labeled nicely.
        if (!stillMoreEmail.IsEmpty())
          FormatExtraDataInNoteField(EUDORAIMPORT_ADDRESS_LABEL_OTHEREMAIL, stillMoreEmail, noteUTF16);
      }

      if (!otherPhone.IsEmpty())
      {
        // Reconstitute line breaks for other phone numbers
        MsgReplaceSubstring(otherPhone, "\x03", "\n");

        // Store other phone numbers in the notes field, labeled nicely
        FormatExtraDataInNoteField(EUDORAIMPORT_ADDRESS_LABEL_OTHERPHONE, otherPhone, noteUTF16);
      }

      if (!otherWeb.IsEmpty())
      {
        // Reconstitute line breaks for other web sites
        MsgReplaceSubstring(otherWeb, "\x03", "\n");

        // Store other web sites in the notes field, labeled nicely
        FormatExtraDataInNoteField(EUDORAIMPORT_ADDRESS_LABEL_OTHERWEB, otherWeb, noteUTF16);
      }

      noteUTF16.Append(NS_ConvertASCIItoUTF16(note));
    }
  }

  CAliasData *pData = emailList.Count() ? (CAliasData *)emailList.ElementAt(0) : nullptr;

  if (pData && !pData->m_realName.IsEmpty())
    displayName = pData->m_realName;
  else if (!name.IsEmpty())
    displayName = name;
  else
    displayName = pEntry->m_name;

  MsgReplaceSubstring(address, "\x03", "\n");
  SplitString(address, address2);
  MsgReplaceSubstring(note, "\x03", "\n");
  MsgReplaceSubstring(fax, "\x03", " ");
  MsgReplaceSubstring(secondaryFax, "\x03", " ");
  MsgReplaceSubstring(phone, "\x03", " ");
  MsgReplaceSubstring(name, "\x03", " ");
  MsgReplaceSubstring(city, "\x03", " ");
  MsgReplaceSubstring(state, "\x03", " ");
  MsgReplaceSubstring(zip, "\x03", " ");
  MsgReplaceSubstring(country, "\x03", " ");

  MsgReplaceSubstring(addressWK, "\x03", "\n");
  SplitString(addressWK, address2WK);
  MsgReplaceSubstring(phoneWK, "\x03", " ");
  MsgReplaceSubstring(cityWK, "\x03", " ");
  MsgReplaceSubstring(stateWK, "\x03", " ");
  MsgReplaceSubstring(zipWK, "\x03", " ");
  MsgReplaceSubstring(countryWK, "\x03", " ");
  MsgReplaceSubstring(title, "\x03", " ");
  MsgReplaceSubstring(company, "\x03", " ");

  if (newRow)
  {
    nsAutoString uniStr;

    // Home related fields.
    ADD_FIELD_TO_DB_ROW(pDb, AddDisplayName, newRow, displayName, uniStr);
    ADD_FIELD_TO_DB_ROW(pDb, AddNickName, newRow, pEntry->m_name, uniStr);
    if (pData)
      ADD_FIELD_TO_DB_ROW(pDb, AddPrimaryEmail, newRow, pData->m_email, uniStr);
    ADD_FIELD_TO_DB_ROW(pDb, AddFirstName, newRow, firstName, uniStr);
    ADD_FIELD_TO_DB_ROW(pDb, AddLastName, newRow, lastName, uniStr);
    ADD_FIELD_TO_DB_ROW(pDb, AddWebPage2, newRow, webLink, uniStr);
    ADD_FIELD_TO_DB_ROW(pDb, AddHomePhone, newRow, phone, uniStr);
    ADD_FIELD_TO_DB_ROW(pDb, AddHomeAddress, newRow, address, uniStr);
    ADD_FIELD_TO_DB_ROW(pDb, AddHomeAddress2, newRow, address2, uniStr);
    ADD_FIELD_TO_DB_ROW(pDb, AddHomeCity, newRow, city, uniStr);
    ADD_FIELD_TO_DB_ROW(pDb, AddHomeZipCode, newRow, zip, uniStr);
    ADD_FIELD_TO_DB_ROW(pDb, AddHomeState, newRow, state, uniStr);
    ADD_FIELD_TO_DB_ROW(pDb, AddHomeCountry, newRow, country, uniStr);

    // Work related fields.
    ADD_FIELD_TO_DB_ROW(pDb, AddJobTitle, newRow, title, uniStr);
    ADD_FIELD_TO_DB_ROW(pDb, AddCompany, newRow, company, uniStr);
    ADD_FIELD_TO_DB_ROW(pDb, AddWebPage1, newRow, webLinkWK, uniStr);
    ADD_FIELD_TO_DB_ROW(pDb, AddWorkPhone, newRow, phoneWK, uniStr);
    ADD_FIELD_TO_DB_ROW(pDb, AddWorkAddress, newRow, addressWK, uniStr);
    ADD_FIELD_TO_DB_ROW(pDb, AddWorkAddress2, newRow, address2WK, uniStr);
    ADD_FIELD_TO_DB_ROW(pDb, AddWorkCity, newRow, cityWK, uniStr);
    ADD_FIELD_TO_DB_ROW(pDb, AddWorkZipCode, newRow, zipWK, uniStr);
    ADD_FIELD_TO_DB_ROW(pDb, AddWorkState, newRow, stateWK, uniStr);
    ADD_FIELD_TO_DB_ROW(pDb, AddWorkCountry, newRow, countryWK, uniStr);

    if ((primaryLocation.IsEmpty() || primaryLocation.LowerCaseEqualsLiteral("home")) &&
         !mobile.IsEmpty())
    {
      // Primary location field is either specified to be "home" or is not
      // specified and there is a home mobile number, so use that as the mobile number.
      ADD_FIELD_TO_DB_ROW(pDb, AddCellularNumber, newRow, mobile, uniStr);

      isSecondaryMobileWorkNumber = true;
    }
    else
    {
      // Primary location field is either specified to be "work" or there is no
      // home mobile number, so use work mobile number.
      ADD_FIELD_TO_DB_ROW(pDb, AddCellularNumber, newRow, secondaryMobile, uniStr);

      // Home mobile number (if any) is the secondary mobile number
      secondaryMobile = mobile;
      isSecondaryMobileWorkNumber = false;
    }

    if ((primaryLocation.IsEmpty() || primaryLocation.LowerCaseEqualsLiteral("home")) &&
         !fax.IsEmpty())
    {
      // Primary location field is either specified to be "home" or is not
      // specified and there is a home fax number, so use that as the fax number.
      ADD_FIELD_TO_DB_ROW(pDb, AddFaxNumber, newRow, fax, uniStr);

      isSecondaryFaxWorkNumber = true;
    }
    else
    {
      // Primary location field is either specified to be "work" or there is no
      // home fax number, so use work fax number.
      ADD_FIELD_TO_DB_ROW(pDb, AddFaxNumber, newRow, secondaryFax, uniStr);

      // Home fax number (if any) is the secondary fax number
      secondaryFax = fax;
      isSecondaryFaxWorkNumber = false;
    }

    ADD_FIELD_TO_DB_ROW(pDb, Add2ndEmail, newRow, additionalEmail, uniStr);

    // Extra info fields
    int32_t         stringID;
    nsString        pFormat;
    nsString        pCustomData;

    // Add second mobile number, if any, to the Custom 1 field
    if (!secondaryMobile.IsEmpty())
    {
      stringID = isSecondaryMobileWorkNumber ?
                 EUDORAIMPORT_ADDRESS_LABEL_WORKMOBILE : EUDORAIMPORT_ADDRESS_LABEL_HOMEMOBILE;
      pFormat.Adopt(nsEudoraStringBundle::GetStringByID(stringID));
      pCustomData.Adopt(nsTextFormatter::smprintf(pFormat.get(), NS_ConvertASCIItoUTF16(secondaryMobile).get()));
      pDb->AddCustom1(newRow, NS_ConvertUTF16toUTF8(pCustomData).get());
    }

    // Add second fax number, if any, to the Custom 2 field
    if (!secondaryFax.IsEmpty())
    {
      stringID = isSecondaryFaxWorkNumber ?
                 EUDORAIMPORT_ADDRESS_LABEL_WORKFAX : EUDORAIMPORT_ADDRESS_LABEL_HOMEFAX;
      pFormat.Adopt(nsEudoraStringBundle::GetStringByID(stringID));
      pCustomData.Adopt(nsTextFormatter::smprintf(pFormat.get(), NS_ConvertASCIItoUTF16(secondaryFax).get()));
      pDb->AddCustom2(newRow, NS_ConvertUTF16toUTF8(pCustomData).get());
    }

    // Lastly, note field.
    pDb->AddNotes(newRow, NS_ConvertUTF16toUTF8(noteUTF16).get());

    pDb->AddCardRowToDB(newRow);

    IMPORT_LOG1("Added card to db: %s\n", displayName.get());
  }
}

//
// Since there is no way to check if a card for a given email address already exists,
// elements in 'membersArray' are make unique. So for each email address in 'emailList'
// we check it in 'membersArray' and if it's not there then we add it to 'membersArray'.
//
void nsEudoraAddress::RememberGroupMembers(nsVoidArray &membersArray, nsVoidArray &emailList)
{
  int32_t cnt = emailList.Count();
  CAliasData *pData;

  for (int32_t i = 0; i < cnt; i++)
  {
    pData = (CAliasData *)emailList.ElementAt(i);
    if (!pData)
      continue;

    int32_t memberCnt = membersArray.Count();
    int32_t j = 0;
    for (j = 0; j < memberCnt; j++)
    {
      if (pData == membersArray.ElementAt(j))
        break;
    }
    if (j >= memberCnt)
      membersArray.AppendElement(pData); // add to member list
  }
}

nsresult nsEudoraAddress::AddGroupMembersAsCards(nsVoidArray &membersArray, nsIAddrDatabase *pDb)
{
  int32_t max = membersArray.Count();
  CAliasData *pData;
  nsresult rv = NS_OK;
  nsCOMPtr <nsIMdbRow> newRow;
  nsAutoString uniStr;
  nsCAutoString  displayName;

  for (int32_t i = 0; i < max; i++)
  {
    pData = (CAliasData *)membersArray.ElementAt(i);

    if (!pData || (pData->m_email.IsEmpty()))
      continue;

    rv = pDb->GetNewRow(getter_AddRefs(newRow));
    if (NS_FAILED(rv) || !newRow)
      return rv;

    if (!pData->m_realName.IsEmpty())
      displayName = pData->m_realName;
    else if (!pData->m_nickName.IsEmpty())
      displayName = pData->m_nickName;
    else
      displayName.Truncate();

    ADD_FIELD_TO_DB_ROW(pDb, AddDisplayName, newRow, displayName, uniStr);
    ADD_FIELD_TO_DB_ROW(pDb, AddPrimaryEmail, newRow, pData->m_email, uniStr);
    rv = pDb->AddCardRowToDB(newRow);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return rv;
}

nsresult nsEudoraAddress::AddSingleList(CAliasEntry *pEntry, nsVoidArray &emailList, nsIAddrDatabase *pDb)
{
  // Create a list.
  nsCOMPtr <nsIMdbRow> newRow;
  nsresult rv = pDb->GetNewListRow(getter_AddRefs(newRow));
  if (NS_FAILED(rv) || !newRow)
      return rv;

  // Extract name from notes, if any
  nsCString     name;

  if (!pEntry->m_notes.IsEmpty())
  {
    nsCString     note(pEntry->m_notes);
    ExtractNoteField(note, name, "name");
  }

  // If we got a name from the notes, use that for the name otherwise use the
  // name in pEntry (which is the Eudora nickname).
  rv = pDb->AddListName(newRow, name.IsEmpty() ? pEntry->m_name.get() : name.get());
  NS_ENSURE_SUCCESS(rv, rv);

  // Add the name in pEntry as the list nickname, because it was the Eudora nickname
  rv = pDb->AddListNickName(newRow, pEntry->m_name.get());
  NS_ENSURE_SUCCESS(rv, rv);

  // Now add the members.
  int32_t max = emailList.Count();
  for (int32_t i = 0; i < max; i++)
  {
    CAliasData *pData = (CAliasData *)emailList.ElementAt(i);
    nsCAutoString ldifValue("mail");
    ldifValue.Append(pData->m_email);
    rv = pDb->AddLdifListMember(newRow, ldifValue.get());
  }

  rv = pDb->AddCardRowToDB(newRow);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = pDb->AddListDirNode(newRow);
  return rv;
}

