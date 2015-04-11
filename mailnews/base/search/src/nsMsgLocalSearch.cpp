/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Implementation of db search for POP and offline IMAP mail folders

#include "msgCore.h"
#include "nsIMsgDatabase.h"
#include "nsMsgSearchCore.h"
#include "nsMsgLocalSearch.h"
#include "nsIStreamListener.h"
#include "nsMsgSearchBoolExpression.h"
#include "nsMsgSearchTerm.h"
#include "nsMsgResultElement.h"
#include "nsIDBFolderInfo.h"
#include "nsISupportsArray.h"
#include "nsMsgBaseCID.h"
#include "nsMsgSearchValue.h"
#include "nsIMsgLocalMailFolder.h"
#include "nsIMsgWindow.h"
#include "nsIMsgHdr.h"
#include "nsIMsgFilterPlugin.h"
#include "nsMsgMessageFlags.h"
#include "nsMsgUtils.h"

extern "C"
{
    extern int MK_MSG_SEARCH_STATUS;
    extern int MK_MSG_CANT_SEARCH_IF_NO_SUMMARY;
    extern int MK_MSG_SEARCH_HITS_NOT_IN_DB;
}


//----------------------------------------------------------------------------
// Class definitions for the boolean expression structure....
//----------------------------------------------------------------------------


nsMsgSearchBoolExpression * nsMsgSearchBoolExpression::AddSearchTerm(nsMsgSearchBoolExpression * aOrigExpr, nsIMsgSearchTerm * aNewTerm, char * aEncodingStr)
// appropriately add the search term to the current expression and return a pointer to the
// new expression. The encodingStr is the IMAP/NNTP encoding string for newTerm.
{
    return aOrigExpr->leftToRightAddTerm(aNewTerm, aEncodingStr);
}

nsMsgSearchBoolExpression * nsMsgSearchBoolExpression::AddExpressionTree(nsMsgSearchBoolExpression * aOrigExpr, nsMsgSearchBoolExpression * aExpression, bool aBoolOp)
{
  if (!aOrigExpr->m_term && !aOrigExpr->m_leftChild && !aOrigExpr->m_rightChild)
  {
      // just use the original expression tree...
      // delete the original since we have a new original to use
      delete aOrigExpr;
      return aExpression;
  }

  nsMsgSearchBoolExpression * newExpr = new nsMsgSearchBoolExpression (aOrigExpr, aExpression, aBoolOp);
  return (newExpr) ? newExpr : aOrigExpr;
}

nsMsgSearchBoolExpression::nsMsgSearchBoolExpression()
{
    m_term = nullptr;
    m_boolOp = nsMsgSearchBooleanOp::BooleanAND;
    m_leftChild = nullptr;
    m_rightChild = nullptr;
}

nsMsgSearchBoolExpression::nsMsgSearchBoolExpression (nsIMsgSearchTerm * newTerm, char * encodingStr)
// we are creating an expression which contains a single search term (newTerm)
// and the search term's IMAP or NNTP encoding value for online search expressions AND
// a boolean evaluation value which is used for offline search expressions.
{
    m_term = newTerm;
    m_encodingStr = encodingStr;
    m_boolOp = nsMsgSearchBooleanOp::BooleanAND;

    // this expression does not contain sub expressions
    m_leftChild = nullptr;
    m_rightChild = nullptr;
}


nsMsgSearchBoolExpression::nsMsgSearchBoolExpression (nsMsgSearchBoolExpression * expr1, nsMsgSearchBoolExpression * expr2, nsMsgSearchBooleanOperator boolOp)
// we are creating an expression which contains two sub expressions and a boolean operator used to combine
// them.
{
    m_leftChild = expr1;
    m_rightChild = expr2;
    m_boolOp = boolOp;

    m_term = nullptr;
}

nsMsgSearchBoolExpression::~nsMsgSearchBoolExpression()
{
  // we must recursively destroy all sub expressions before we destroy ourself.....We leave search terms alone!
  delete m_leftChild;
  delete m_rightChild;
}

nsMsgSearchBoolExpression *
nsMsgSearchBoolExpression::leftToRightAddTerm(nsIMsgSearchTerm * newTerm, char * encodingStr)
{
    // we have a base case where this is the first term being added to the expression:
    if (!m_term && !m_leftChild && !m_rightChild)
    {
        m_term = newTerm;
        m_encodingStr = encodingStr;
        return this;
    }

    nsMsgSearchBoolExpression * tempExpr = new nsMsgSearchBoolExpression (newTerm,encodingStr);
    if (tempExpr)  // make sure creation succeeded
    {
      bool booleanAnd;
      newTerm->GetBooleanAnd(&booleanAnd);
      nsMsgSearchBoolExpression * newExpr = new nsMsgSearchBoolExpression (this, tempExpr, booleanAnd);
      if (newExpr)
         return newExpr;
      else
         delete tempExpr;    // clean up memory allocation in case of failure
    }
    return this;   // in case we failed to create a new expression, return self
}


// returns true or false depending on what the current expression evaluates to.
bool nsMsgSearchBoolExpression::OfflineEvaluate(nsIMsgDBHdr *msgToMatch, const char *defaultCharset,
  nsIMsgSearchScopeTerm *scope, nsIMsgDatabase *db, const char *headers,
  uint32_t headerSize, bool Filtering)
{
    bool result = true;    // always default to false positives
    bool isAnd;

    if (m_term) // do we contain just a search term?
    {
      nsMsgSearchOfflineMail::ProcessSearchTerm(msgToMatch, m_term,
        defaultCharset, scope, db, headers, headerSize, Filtering, &result);
      return result;
    }

    // otherwise we must recursively determine the value of our sub expressions

    isAnd = (m_boolOp == nsMsgSearchBooleanOp::BooleanAND);

    if (m_leftChild)
    {
        result = m_leftChild->OfflineEvaluate(msgToMatch, defaultCharset,
          scope, db, headers, headerSize, Filtering);
        if ( (result && !isAnd) || (!result && isAnd))
          return result;
    }

    // If we got this far, either there was no leftChild (which is impossible)
    // or we got (FALSE and OR) or (TRUE and AND) from the first result. That
    // means the outcome depends entirely on the rightChild.
    if (m_rightChild)
        result = m_rightChild->OfflineEvaluate(msgToMatch, defaultCharset,
          scope, db, headers, headerSize, Filtering);

    return result;
}

// ### Maybe we can get rid of these because of our use of nsString???
// constants used for online searching with IMAP/NNTP encoded search terms.
// the + 1 is to account for null terminators we add at each stage of assembling the expression...
const int sizeOfORTerm = 6+1;  // 6 bytes if we are combining two sub expressions with an OR term
const int sizeOfANDTerm = 1+1; // 1 byte if we are combining two sub expressions with an AND term

int32_t nsMsgSearchBoolExpression::CalcEncodeStrSize()
// recursively examine each sub expression and calculate a final size for the entire IMAP/NNTP encoding
{
    if (!m_term && (!m_leftChild || !m_rightChild))   // is the expression empty?
        return 0;
    if (m_term)  // are we a leaf node?
        return m_encodingStr.Length();
    if (m_boolOp == nsMsgSearchBooleanOp::BooleanOR)
        return sizeOfORTerm + m_leftChild->CalcEncodeStrSize() + m_rightChild->CalcEncodeStrSize();
    if (m_boolOp == nsMsgSearchBooleanOp::BooleanAND)
        return sizeOfANDTerm + m_leftChild->CalcEncodeStrSize() + m_rightChild->CalcEncodeStrSize();
    return 0;
}


void nsMsgSearchBoolExpression::GenerateEncodeStr(nsCString * buffer)
// recurively combine sub expressions to form a single IMAP/NNTP encoded string
{
    if ((!m_term && (!m_leftChild || !m_rightChild))) // is expression empty?
        return;

    if (m_term) // are we a leaf expression?
    {
        *buffer += m_encodingStr;
        return;
    }

    // add encode strings of each sub expression
    if (m_boolOp == nsMsgSearchBooleanOp::BooleanOR)
    {
        *buffer += " (OR";

        m_leftChild->GenerateEncodeStr(buffer);  // insert left expression into the buffer
        m_rightChild->GenerateEncodeStr(buffer);  // insert right expression into the buffer

        // HACK ALERT!!! if last returned character in the buffer is now a ' ' then we need to remove it because we don't want
        // a ' ' to preceded the closing paren in the OR encoding.
        uint32_t lastCharPos = buffer->Length() - 1;
        if (buffer->CharAt(lastCharPos) == ' ')
        {
            buffer->SetLength(lastCharPos);
        }

        *buffer += ')';
    }
    else if (m_boolOp == nsMsgSearchBooleanOp::BooleanAND)
    {
        m_leftChild->GenerateEncodeStr(buffer); // insert left expression
        m_rightChild->GenerateEncodeStr(buffer);
    }
    return;
}


//-----------------------------------------------------------------------------
//---------------- Adapter class for searching offline folders ----------------
//-----------------------------------------------------------------------------


NS_IMPL_ISUPPORTS_INHERITED1(nsMsgSearchOfflineMail, nsMsgSearchAdapter, nsIUrlListener)

nsMsgSearchOfflineMail::nsMsgSearchOfflineMail (nsIMsgSearchScopeTerm *scope, nsISupportsArray *termList) : nsMsgSearchAdapter (scope, termList)
{
}

nsMsgSearchOfflineMail::~nsMsgSearchOfflineMail ()
{
  // Database should have been closed when the scope term finished.
  CleanUpScope();
  NS_ASSERTION(!m_db, "db not closed");
}


nsresult nsMsgSearchOfflineMail::ValidateTerms ()
{
  return nsMsgSearchAdapter::ValidateTerms ();
}


nsresult nsMsgSearchOfflineMail::OpenSummaryFile ()
{
    nsCOMPtr <nsIMsgDatabase> mailDB ;

    nsresult err = NS_OK;
    // do password protection of local cache thing.
#ifdef DOING_FOLDER_CACHE_PASSWORDS
    if (m_scope->m_folder && m_scope->m_folder->UserNeedsToAuthenticateForFolder(false) && m_scope->m_folder->GetMaster()->PromptForHostPassword(m_scope->m_frame->GetContext(), m_scope->m_folder) != 0)
    {
        m_scope->m_frame->StopRunning();
        return SearchError_ScopeDone;
    }
#endif
    nsCOMPtr <nsIDBFolderInfo>  folderInfo;
    nsCOMPtr <nsIMsgFolder> scopeFolder;
    err = m_scope->GetFolder(getter_AddRefs(scopeFolder));
    if (NS_SUCCEEDED(err) && scopeFolder)
    {
      err = scopeFolder->GetDBFolderInfoAndDB(getter_AddRefs(folderInfo), getter_AddRefs(m_db));
    }
    else
      return err; // not sure why m_folder wouldn't be set.

    switch (err)
    {
        case NS_OK:
            break;
        case NS_MSG_ERROR_FOLDER_SUMMARY_MISSING:
        case NS_MSG_ERROR_FOLDER_SUMMARY_OUT_OF_DATE:
          {
            nsCOMPtr<nsIMsgLocalMailFolder> localFolder = do_QueryInterface(scopeFolder, &err);
            if (NS_SUCCEEDED(err) && localFolder)
            {
              nsCOMPtr<nsIMsgSearchSession> searchSession;
              m_scope->GetSearchSession(getter_AddRefs(searchSession));
              if (searchSession)
              {
                nsCOMPtr <nsIMsgWindow> searchWindow;

                searchSession->GetWindow(getter_AddRefs(searchWindow));
                searchSession->PauseSearch();
                localFolder->ParseFolder(searchWindow, this);
              }
            }
          }
            break;
        default:
        {
          NS_ASSERTION(false, "unexpected error opening db");
        }
    }

    return err;
}


nsresult
nsMsgSearchOfflineMail::MatchTermsForFilter(nsIMsgDBHdr *msgToMatch,
                                            nsISupportsArray *termList,
                                            const char *defaultCharset,
                                            nsIMsgSearchScopeTerm * scope,
                                            nsIMsgDatabase * db,
                                            const char * headers,
                                            uint32_t headerSize,
                                            nsMsgSearchBoolExpression ** aExpressionTree,
                                            bool *pResult)
{
    return MatchTerms(msgToMatch, termList, defaultCharset, scope, db, headers, headerSize, true, aExpressionTree, pResult);
}

// static method which matches a header against a list of search terms.
nsresult
nsMsgSearchOfflineMail::MatchTermsForSearch(nsIMsgDBHdr *msgToMatch,
                                            nsISupportsArray* termList,
                                            const char *defaultCharset,
                                            nsIMsgSearchScopeTerm *scope,
                                            nsIMsgDatabase *db,
                                            nsMsgSearchBoolExpression ** aExpressionTree,
                                            bool *pResult)
{

    return MatchTerms(msgToMatch, termList, defaultCharset, scope, db, nullptr, 0, false, aExpressionTree, pResult);
}

nsresult nsMsgSearchOfflineMail::ConstructExpressionTree(nsISupportsArray * termList,
                                            uint32_t termCount,
                                            uint32_t &aStartPosInList,
                                            nsMsgSearchBoolExpression ** aExpressionTree)
{
  nsMsgSearchBoolExpression * finalExpression = *aExpressionTree;

  if (!finalExpression)
    finalExpression = new nsMsgSearchBoolExpression();

  while (aStartPosInList < termCount)
  {
      nsCOMPtr<nsIMsgSearchTerm> pTerm;
      termList->QueryElementAt(aStartPosInList, NS_GET_IID(nsIMsgSearchTerm), (void **)getter_AddRefs(pTerm));
      NS_ASSERTION (pTerm, "couldn't get term to match");

      bool beginsGrouping;
      bool endsGrouping;
      pTerm->GetBeginsGrouping(&beginsGrouping);
      pTerm->GetEndsGrouping(&endsGrouping);

      if (beginsGrouping)
      {
          //temporarily turn off the grouping for our recursive call
          pTerm->SetBeginsGrouping(false);
          nsMsgSearchBoolExpression * innerExpression = new nsMsgSearchBoolExpression();

          // the first search term in the grouping is the one that holds the operator for how this search term
          // should be joined with the expressions to it's left.
          bool booleanAnd;
          pTerm->GetBooleanAnd(&booleanAnd);

          // now add this expression tree to our overall expression tree...
          finalExpression = nsMsgSearchBoolExpression::AddExpressionTree(finalExpression, innerExpression, booleanAnd);

          // recursively process this inner expression
          ConstructExpressionTree(termList, termCount, aStartPosInList,
            &finalExpression->m_rightChild);

          // undo our damage
          pTerm->SetBeginsGrouping(true);

      }
      else
      {
        finalExpression = nsMsgSearchBoolExpression::AddSearchTerm(finalExpression, pTerm, nullptr);    // add the term to the expression tree

        if (endsGrouping)
          break;
      }

      aStartPosInList++;
  } // while we still have terms to process in this group

  *aExpressionTree = finalExpression;

  return NS_OK;
}

nsresult nsMsgSearchOfflineMail::ProcessSearchTerm(nsIMsgDBHdr *msgToMatch,
                                            nsIMsgSearchTerm * aTerm,
                                            const char *defaultCharset,
                                            nsIMsgSearchScopeTerm * scope,
                                            nsIMsgDatabase * db,
                                            const char * headers,
                                            uint32_t headerSize,
                                            bool Filtering,
                      bool *pResult)
{
    nsresult err = NS_OK;
    nsCString  recipients;
    nsCString  ccList;
    nsCString  matchString;
    nsCString  msgCharset;
    const char *charset;
    bool charsetOverride = false; /* XXX BUG 68706 */
    uint32_t msgFlags;
    bool result;
    bool matchAll;

    NS_ENSURE_ARG_POINTER(pResult);

    aTerm->GetMatchAll(&matchAll);
    if (matchAll)
    {
      *pResult = true;
      return NS_OK;
    }
    *pResult = false;

    nsMsgSearchAttribValue attrib;
    aTerm->GetAttrib(&attrib);
    msgToMatch->GetCharset(getter_Copies(msgCharset));
    charset = msgCharset.get();
    if (!charset || !*charset)
      charset = (const char*)defaultCharset;
    msgToMatch->GetFlags(&msgFlags);

    switch (attrib)
    {
      case nsMsgSearchAttrib::Sender:
        msgToMatch->GetAuthor(getter_Copies(matchString));
        err = aTerm->MatchRfc822String (matchString.get(), charset, charsetOverride, &result);
        break;
      case nsMsgSearchAttrib::Subject:
      {
        msgToMatch->GetSubject(getter_Copies(matchString) /* , true */);
        if (msgFlags & nsMsgMessageFlags::HasRe)
        {
          // Make sure we pass along the "Re: " part of the subject if this is a reply.
          nsCString reString;
          reString.Assign("Re: ");
          reString.Append(matchString);
          err = aTerm->MatchRfc2047String(reString.get(), charset, charsetOverride, &result);
        }
        else
          err = aTerm->MatchRfc2047String (matchString.get(), charset, charsetOverride, &result);
        break;
      }
      case nsMsgSearchAttrib::ToOrCC:
      {
        bool boolKeepGoing;
        aTerm->GetMatchAllBeforeDeciding(&boolKeepGoing);
        msgToMatch->GetRecipients(getter_Copies(recipients));
        err = aTerm->MatchRfc822String (recipients.get(), charset, charsetOverride, &result);
        if (boolKeepGoing == result)
        {
          msgToMatch->GetCcList(getter_Copies(ccList));
          err = aTerm->MatchRfc822String (ccList.get(), charset, charsetOverride, &result);
        }
        break;
      }
      case nsMsgSearchAttrib::AllAddresses:
      {
        bool boolKeepGoing;
        aTerm->GetMatchAllBeforeDeciding(&boolKeepGoing);
        msgToMatch->GetRecipients(getter_Copies(recipients));
        err = aTerm->MatchRfc822String(recipients.get(), charset, charsetOverride, &result);
        if (boolKeepGoing == result)
        {
          msgToMatch->GetCcList(getter_Copies(ccList));
          err = aTerm->MatchRfc822String(ccList.get(), charset, charsetOverride, &result);
        }
        if (boolKeepGoing == result)
        {
          msgToMatch->GetAuthor(getter_Copies(matchString));
          err = aTerm->MatchRfc822String(matchString.get(), charset, charsetOverride, &result);
        }
        if (boolKeepGoing == result)
        {
          nsCString bccList;
          msgToMatch->GetBccList(getter_Copies(bccList));
          err = aTerm->MatchRfc822String(bccList.get(), charset, charsetOverride, &result);
        }
        break;
      }
      case nsMsgSearchAttrib::Body:
       {
         uint64_t messageOffset;
         uint32_t lineCount;
         msgToMatch->GetMessageOffset(&messageOffset);
         msgToMatch->GetLineCount(&lineCount);
         err = aTerm->MatchBody (scope, messageOffset, lineCount, charset, msgToMatch, db, &result);
         break;
       }
      case nsMsgSearchAttrib::Date:
      {
        PRTime date;
        msgToMatch->GetDate(&date);
        err = aTerm->MatchDate (date, &result);

        break;
      }
      case nsMsgSearchAttrib::HasAttachmentStatus:
      case nsMsgSearchAttrib::MsgStatus:
        err = aTerm->MatchStatus (msgFlags, &result);
        break;
      case nsMsgSearchAttrib::Priority:
      {
        nsMsgPriorityValue msgPriority;
        msgToMatch->GetPriority(&msgPriority);
        err = aTerm->MatchPriority (msgPriority, &result);
        break;
      }
      case nsMsgSearchAttrib::Size:
      {
        uint32_t messageSize;
        msgToMatch->GetMessageSize(&messageSize);
        err = aTerm->MatchSize (messageSize, &result);
        break;
      }
      case nsMsgSearchAttrib::To:
        msgToMatch->GetRecipients(getter_Copies(recipients));
        err = aTerm->MatchRfc822String(recipients.get(), charset, charsetOverride, &result);
        break;
      case nsMsgSearchAttrib::CC:
        msgToMatch->GetCcList(getter_Copies(ccList));
        err = aTerm->MatchRfc822String (ccList.get(), charset, charsetOverride, &result);
        break;
      case nsMsgSearchAttrib::AgeInDays:
      {
        PRTime date;
        msgToMatch->GetDate(&date);
        err = aTerm->MatchAge (date, &result);
        break;
       }
      case nsMsgSearchAttrib::Label:
      {
         nsMsgLabelValue label;
         msgToMatch->GetLabel(&label);
         err = aTerm->MatchLabel(label, &result);
         break;
      }
      case nsMsgSearchAttrib::Keywords:
      {
          nsCString keywords;
          nsMsgLabelValue label;
          msgToMatch->GetStringProperty("keywords", getter_Copies(keywords));
          msgToMatch->GetLabel(&label);
          if (label >= 1)
          {
            if (!keywords.IsEmpty())
              keywords.Append(' ');
            keywords.Append("$label");
            keywords.Append(label + '0');
          }
          err = aTerm->MatchKeyword(keywords, &result);
          break;
      }
      case nsMsgSearchAttrib::JunkStatus:
      {
         nsCString junkScoreStr;
         msgToMatch->GetStringProperty("junkscore", getter_Copies(junkScoreStr));
         err = aTerm->MatchJunkStatus(junkScoreStr.get(), &result);
         break;
      }
      case nsMsgSearchAttrib::JunkPercent:
      {
        // When the junk status is set by the plugin, use junkpercent (if available)
        // Otherwise, use the limits (0 or 100) depending on the junkscore.
        uint32_t junkPercent;
        nsresult rv;
        nsCString junkScoreOriginStr;
        nsCString junkPercentStr;
        msgToMatch->GetStringProperty("junkscoreorigin", getter_Copies(junkScoreOriginStr));
        msgToMatch->GetStringProperty("junkpercent", getter_Copies(junkPercentStr));
        if ( junkScoreOriginStr.EqualsLiteral("plugin") &&
            !junkPercentStr.IsEmpty())
        {
          junkPercent = junkPercentStr.ToInteger(&rv);
          NS_ENSURE_SUCCESS(rv, rv);
        }
        else
        {
          nsCString junkScoreStr;
          msgToMatch->GetStringProperty("junkscore", getter_Copies(junkScoreStr));
          // When junk status is not set (uncertain) we'll set the value to ham.
          if (junkScoreStr.IsEmpty())
            junkPercent = nsIJunkMailPlugin::IS_HAM_SCORE;
          else
          {
            junkPercent = junkScoreStr.ToInteger(&rv);
            NS_ENSURE_SUCCESS(rv, rv);
          }
        }
        err = aTerm->MatchJunkPercent(junkPercent, &result);
        break;
      }
      case nsMsgSearchAttrib::JunkScoreOrigin:
      {
         nsCString junkScoreOriginStr;
         msgToMatch->GetStringProperty("junkscoreorigin", getter_Copies(junkScoreOriginStr));
         err = aTerm->MatchJunkScoreOrigin(junkScoreOriginStr.get(), &result);
         break;
      }
      case nsMsgSearchAttrib::HdrProperty:
      {
        err = aTerm->MatchHdrProperty(msgToMatch, &result);
        break;
      }
      case nsMsgSearchAttrib::Uint32HdrProperty:
      {
        err = aTerm->MatchUint32HdrProperty(msgToMatch, &result);
        break;
      }
      case nsMsgSearchAttrib::Custom:
      {
        err = aTerm->MatchCustom(msgToMatch, &result);
        break;
      }
      case nsMsgSearchAttrib::FolderFlag:
      {
        err = aTerm->MatchFolderFlag(msgToMatch, &result);
        break;
      }
      default:
          // XXX todo
          // for the temporary return receipts filters, we use a custom header for Content-Type
          // but unlike the other custom headers, this one doesn't show up in the search / filter
          // UI.  we set the attrib to be nsMsgSearchAttrib::OtherHeader, where as for user
          // defined custom headers start at nsMsgSearchAttrib::OtherHeader + 1
          // Not sure if there is a better way to do this yet.  Maybe reserve the last
          // custom header for ::Content-Type?  But if we do, make sure that change
          // doesn't cause nsMsgFilter::GetTerm() to change, and start making us
          // ask IMAP servers for the Content-Type header on all messages.
          if (attrib >= nsMsgSearchAttrib::OtherHeader &&
              attrib < nsMsgSearchAttrib::kNumMsgSearchAttributes)
          {
            uint32_t lineCount;
            msgToMatch->GetLineCount(&lineCount);
            uint64_t messageOffset;
            msgToMatch->GetMessageOffset(&messageOffset);
            err = aTerm->MatchArbitraryHeader(scope, lineCount,charset,
                                              charsetOverride, msgToMatch, db,
                                              headers, headerSize, Filtering,
                                              &result);
          }
          else
            err = NS_ERROR_INVALID_ARG; // ### was SearchError_InvalidAttribute
    }

    *pResult = result;
    return NS_OK;
}

nsresult nsMsgSearchOfflineMail::MatchTerms(nsIMsgDBHdr *msgToMatch,
                                            nsISupportsArray * termList,
                                            const char *defaultCharset,
                                            nsIMsgSearchScopeTerm * scope,
                                            nsIMsgDatabase * db,
                                            const char * headers,
                                            uint32_t headerSize,
                                            bool Filtering,
                                            nsMsgSearchBoolExpression ** aExpressionTree,
                                            bool *pResult)
{
  NS_ENSURE_ARG(aExpressionTree);
  nsresult err;

  if (!*aExpressionTree)
  {
    uint32_t initialPos = 0;
    uint32_t count;
    termList->Count(&count);
    err = ConstructExpressionTree(termList, count, initialPos, aExpressionTree);
    if (NS_FAILED(err))
      return err;
  }

  // evaluate the expression tree and return the result
  *pResult = (*aExpressionTree)
    ?  (*aExpressionTree)->OfflineEvaluate(msgToMatch,
                 defaultCharset, scope, db, headers, headerSize, Filtering)
    :true;  // vacuously true...

  return NS_OK;
}


nsresult nsMsgSearchOfflineMail::Search (bool *aDone)
{
  nsresult err = NS_OK;

  NS_ENSURE_ARG(aDone);
  nsresult dbErr = NS_OK;
  nsCOMPtr<nsIMsgDBHdr> msgDBHdr;
  nsMsgSearchBoolExpression *expressionTree = nullptr;

  const uint32_t kTimeSliceInMS = 200;

  *aDone = false;
  // Try to open the DB lazily. This will set up a parser if one is required
  if (!m_db)
    err = OpenSummaryFile ();
  if (!m_db)  // must be reparsing.
    return err;

  // Reparsing is unnecessary or completed
  if (NS_SUCCEEDED(err))
  {
    if (!m_listContext)
      dbErr = m_db->EnumerateMessages (getter_AddRefs(m_listContext));
    if (NS_SUCCEEDED(dbErr) && m_listContext)
    {
      PRIntervalTime startTime = PR_IntervalNow();
      while (!*aDone)  // we'll break out of the loop after kTimeSliceInMS milliseconds
      {
        nsCOMPtr<nsISupports> currentItem;

        dbErr = m_listContext->GetNext(getter_AddRefs(currentItem));
        if(NS_SUCCEEDED(dbErr))
        {
          msgDBHdr = do_QueryInterface(currentItem, &dbErr);
        }
        if (NS_FAILED(dbErr))
          *aDone = true; //###phil dbErr is dropped on the floor. just note that we did have an error so we'll clean up later
        else
        {
          bool match = false;
          nsAutoString nullCharset, folderCharset;
          GetSearchCharsets(nullCharset, folderCharset);
          NS_ConvertUTF16toUTF8 charset(folderCharset);
          // Is this message a hit?
          err = MatchTermsForSearch (msgDBHdr, m_searchTerms, charset.get(), m_scope, m_db, &expressionTree, &match);
          // Add search hits to the results list
          if (NS_SUCCEEDED(err) && match)
          {
            AddResultElement (msgDBHdr);
          }
          PRIntervalTime elapsedTime = PR_IntervalNow() - startTime;
          // check if more than kTimeSliceInMS milliseconds have elapsed in this time slice started
          if (PR_IntervalToMilliseconds(elapsedTime) > kTimeSliceInMS)
            break;
        }
      }
    }
  }
  else
    *aDone = true; // we couldn't open up the DB. This is an unrecoverable error so mark the scope as done.

  delete expressionTree;

  // in the past an error here would cause an "infinite" search because the url would continue to run...
  // i.e. if we couldn't open the database, it returns an error code but the caller of this function says, oh,
  // we did not finish so continue...what we really want is to treat this current scope as done
  if (*aDone)
    CleanUpScope(); // Do clean up for end-of-scope processing
  return err;
}

void nsMsgSearchOfflineMail::CleanUpScope()
{
  // Let go of the DB when we're done with it so we don't kill the db cache
  if (m_db)
  {
    m_listContext = nullptr;
    m_db->Close(false);
  }
  m_db = nullptr;

  if (m_scope)
    m_scope->CloseInputStream();
}

NS_IMETHODIMP nsMsgSearchOfflineMail::AddResultElement (nsIMsgDBHdr *pHeaders)
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
    return err;
}

NS_IMETHODIMP
nsMsgSearchOfflineMail::Abort ()
{
    // Let go of the DB when we're done with it so we don't kill the db cache
    if (m_db)
        m_db->Close(true /* commit in case we downloaded new headers */);
    m_db = nullptr;
    return nsMsgSearchAdapter::Abort ();
}

/* void OnStartRunningUrl (in nsIURI url); */
NS_IMETHODIMP nsMsgSearchOfflineMail::OnStartRunningUrl(nsIURI *url)
{
    return NS_OK;
}

/* void OnStopRunningUrl (in nsIURI url, in nsresult aExitCode); */
NS_IMETHODIMP nsMsgSearchOfflineMail::OnStopRunningUrl(nsIURI *url, nsresult aExitCode)
{
  nsCOMPtr<nsIMsgSearchSession> searchSession;
  if (m_scope)
    m_scope->GetSearchSession(getter_AddRefs(searchSession));
  if (searchSession)
    searchSession->ResumeSearch();

  return NS_OK;
}

nsMsgSearchOfflineNews::nsMsgSearchOfflineNews (nsIMsgSearchScopeTerm *scope, nsISupportsArray *termList) : nsMsgSearchOfflineMail (scope, termList)
{
}


nsMsgSearchOfflineNews::~nsMsgSearchOfflineNews ()
{
}


nsresult nsMsgSearchOfflineNews::OpenSummaryFile ()
{
  nsresult err = NS_OK;
  nsCOMPtr <nsIDBFolderInfo>  folderInfo;
  nsCOMPtr <nsIMsgFolder> scopeFolder;
  err = m_scope->GetFolder(getter_AddRefs(scopeFolder));
  // code here used to check if offline store existed, which breaks offline news.
  if (NS_SUCCEEDED(err) && scopeFolder)
    err = scopeFolder->GetMsgDatabase(getter_AddRefs(m_db));
  return err;
}

nsresult nsMsgSearchOfflineNews::ValidateTerms ()
{
  return nsMsgSearchOfflineMail::ValidateTerms ();
}

// local helper functions to set subsets of the validity table

nsresult SetJunk(nsIMsgSearchValidityTable* aTable)
{
  NS_ENSURE_ARG_POINTER(aTable);

  aTable->SetAvailable(nsMsgSearchAttrib::JunkStatus, nsMsgSearchOp::Is, 1);
  aTable->SetEnabled(nsMsgSearchAttrib::JunkStatus, nsMsgSearchOp::Is, 1);
  aTable->SetAvailable(nsMsgSearchAttrib::JunkStatus, nsMsgSearchOp::Isnt, 1);
  aTable->SetEnabled(nsMsgSearchAttrib::JunkStatus, nsMsgSearchOp::Isnt, 1);
  aTable->SetAvailable(nsMsgSearchAttrib::JunkStatus, nsMsgSearchOp::IsEmpty, 1);
  aTable->SetEnabled(nsMsgSearchAttrib::JunkStatus, nsMsgSearchOp::IsEmpty, 1);
  aTable->SetAvailable(nsMsgSearchAttrib::JunkStatus, nsMsgSearchOp::IsntEmpty, 1);
  aTable->SetEnabled(nsMsgSearchAttrib::JunkStatus, nsMsgSearchOp::IsntEmpty, 1);

  aTable->SetAvailable(nsMsgSearchAttrib::JunkPercent, nsMsgSearchOp::IsGreaterThan, 1);
  aTable->SetEnabled(nsMsgSearchAttrib::JunkPercent, nsMsgSearchOp::IsGreaterThan, 1);
  aTable->SetAvailable(nsMsgSearchAttrib::JunkPercent, nsMsgSearchOp::IsLessThan, 1);
  aTable->SetEnabled(nsMsgSearchAttrib::JunkPercent, nsMsgSearchOp::IsLessThan, 1);
  aTable->SetAvailable(nsMsgSearchAttrib::JunkPercent, nsMsgSearchOp::Is, 1);
  aTable->SetEnabled(nsMsgSearchAttrib::JunkPercent, nsMsgSearchOp::Is, 1);

  aTable->SetAvailable(nsMsgSearchAttrib::JunkScoreOrigin, nsMsgSearchOp::Is, 1);
  aTable->SetEnabled(nsMsgSearchAttrib::JunkScoreOrigin, nsMsgSearchOp::Is, 1);
  aTable->SetAvailable(nsMsgSearchAttrib::JunkScoreOrigin, nsMsgSearchOp::Isnt, 1);
  aTable->SetEnabled(nsMsgSearchAttrib::JunkScoreOrigin, nsMsgSearchOp::Isnt, 1);

  return NS_OK;
}

nsresult SetBody(nsIMsgSearchValidityTable* aTable)
{
  NS_ENSURE_ARG_POINTER(aTable);
  aTable->SetAvailable (nsMsgSearchAttrib::Body, nsMsgSearchOp::Contains, 1);
  aTable->SetEnabled   (nsMsgSearchAttrib::Body, nsMsgSearchOp::Contains, 1);
  aTable->SetAvailable (nsMsgSearchAttrib::Body, nsMsgSearchOp::DoesntContain, 1);
  aTable->SetEnabled   (nsMsgSearchAttrib::Body, nsMsgSearchOp::DoesntContain, 1);
  aTable->SetAvailable (nsMsgSearchAttrib::Body, nsMsgSearchOp::Is, 1);
  aTable->SetEnabled   (nsMsgSearchAttrib::Body, nsMsgSearchOp::Is, 1);
  aTable->SetAvailable (nsMsgSearchAttrib::Body, nsMsgSearchOp::Isnt, 1);
  aTable->SetEnabled   (nsMsgSearchAttrib::Body, nsMsgSearchOp::Isnt, 1);

  return NS_OK;
}

// set the base validity table values for local news
nsresult SetLocalNews(nsIMsgSearchValidityTable* aTable)
{
  NS_ENSURE_ARG_POINTER(aTable);

  aTable->SetAvailable (nsMsgSearchAttrib::Sender, nsMsgSearchOp::Contains, 1);
  aTable->SetEnabled   (nsMsgSearchAttrib::Sender, nsMsgSearchOp::Contains, 1);
  aTable->SetAvailable (nsMsgSearchAttrib::Sender, nsMsgSearchOp::DoesntContain, 1);
  aTable->SetEnabled   (nsMsgSearchAttrib::Sender, nsMsgSearchOp::DoesntContain, 1);
  aTable->SetAvailable (nsMsgSearchAttrib::Sender, nsMsgSearchOp::Is, 1);
  aTable->SetEnabled   (nsMsgSearchAttrib::Sender, nsMsgSearchOp::Is, 1);
  aTable->SetEnabled   (nsMsgSearchAttrib::Sender, nsMsgSearchOp::Isnt, 1);
  aTable->SetAvailable (nsMsgSearchAttrib::Sender, nsMsgSearchOp::Isnt, 1);
  aTable->SetAvailable (nsMsgSearchAttrib::Sender, nsMsgSearchOp::BeginsWith, 1);
  aTable->SetEnabled   (nsMsgSearchAttrib::Sender, nsMsgSearchOp::BeginsWith, 1);
  aTable->SetAvailable (nsMsgSearchAttrib::Sender, nsMsgSearchOp::EndsWith, 1);
  aTable->SetEnabled   (nsMsgSearchAttrib::Sender, nsMsgSearchOp::EndsWith, 1);
  aTable->SetAvailable (nsMsgSearchAttrib::Sender, nsMsgSearchOp::IsInAB, 1);
  aTable->SetEnabled   (nsMsgSearchAttrib::Sender, nsMsgSearchOp::IsInAB, 1);
  aTable->SetAvailable (nsMsgSearchAttrib::Sender, nsMsgSearchOp::IsntInAB, 1);
  aTable->SetEnabled   (nsMsgSearchAttrib::Sender, nsMsgSearchOp::IsntInAB, 1);

  aTable->SetAvailable (nsMsgSearchAttrib::Subject, nsMsgSearchOp::Contains, 1);
  aTable->SetEnabled   (nsMsgSearchAttrib::Subject, nsMsgSearchOp::Contains, 1);
  aTable->SetAvailable (nsMsgSearchAttrib::Subject, nsMsgSearchOp::DoesntContain, 1);
  aTable->SetEnabled   (nsMsgSearchAttrib::Subject, nsMsgSearchOp::DoesntContain, 1);
  aTable->SetAvailable (nsMsgSearchAttrib::Subject, nsMsgSearchOp::Is, 1);
  aTable->SetEnabled   (nsMsgSearchAttrib::Subject, nsMsgSearchOp::Is, 1);
  aTable->SetAvailable (nsMsgSearchAttrib::Subject, nsMsgSearchOp::Isnt, 1);
  aTable->SetEnabled   (nsMsgSearchAttrib::Subject, nsMsgSearchOp::Isnt, 1);
  aTable->SetAvailable (nsMsgSearchAttrib::Subject, nsMsgSearchOp::BeginsWith, 1);
  aTable->SetEnabled   (nsMsgSearchAttrib::Subject, nsMsgSearchOp::BeginsWith, 1);
  aTable->SetAvailable (nsMsgSearchAttrib::Subject, nsMsgSearchOp::EndsWith, 1);
  aTable->SetEnabled   (nsMsgSearchAttrib::Subject, nsMsgSearchOp::EndsWith, 1);

  aTable->SetAvailable (nsMsgSearchAttrib::Date, nsMsgSearchOp::IsBefore, 1);
  aTable->SetEnabled   (nsMsgSearchAttrib::Date, nsMsgSearchOp::IsBefore, 1);
  aTable->SetAvailable (nsMsgSearchAttrib::Date, nsMsgSearchOp::IsAfter, 1);
  aTable->SetEnabled   (nsMsgSearchAttrib::Date, nsMsgSearchOp::IsAfter, 1);
  aTable->SetAvailable (nsMsgSearchAttrib::Date, nsMsgSearchOp::Is, 1);
  aTable->SetEnabled   (nsMsgSearchAttrib::Date, nsMsgSearchOp::Is, 1);
  aTable->SetAvailable (nsMsgSearchAttrib::Date, nsMsgSearchOp::Isnt, 1);
  aTable->SetEnabled   (nsMsgSearchAttrib::Date, nsMsgSearchOp::Isnt, 1);

  aTable->SetAvailable (nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::IsGreaterThan, 1);
  aTable->SetEnabled   (nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::IsGreaterThan, 1);
  aTable->SetAvailable (nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::IsLessThan,  1);
  aTable->SetEnabled   (nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::IsLessThan, 1);
  aTable->SetAvailable (nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::Is,  1);
  aTable->SetEnabled   (nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::Is, 1);

  aTable->SetAvailable (nsMsgSearchAttrib::MsgStatus, nsMsgSearchOp::Is, 1);
  aTable->SetEnabled   (nsMsgSearchAttrib::MsgStatus, nsMsgSearchOp::Is, 1);
  aTable->SetAvailable (nsMsgSearchAttrib::MsgStatus, nsMsgSearchOp::Isnt, 1);
  aTable->SetEnabled   (nsMsgSearchAttrib::MsgStatus, nsMsgSearchOp::Isnt, 1);

  aTable->SetAvailable (nsMsgSearchAttrib::Keywords, nsMsgSearchOp::Contains, 1);
  aTable->SetEnabled   (nsMsgSearchAttrib::Keywords, nsMsgSearchOp::Contains, 1);
  aTable->SetAvailable (nsMsgSearchAttrib::Keywords, nsMsgSearchOp::DoesntContain, 1);
  aTable->SetEnabled   (nsMsgSearchAttrib::Keywords, nsMsgSearchOp::DoesntContain, 1);
  aTable->SetAvailable (nsMsgSearchAttrib::Keywords, nsMsgSearchOp::Is, 1);
  aTable->SetEnabled   (nsMsgSearchAttrib::Keywords, nsMsgSearchOp::Is, 1);
  aTable->SetAvailable (nsMsgSearchAttrib::Keywords, nsMsgSearchOp::Isnt, 1);
  aTable->SetEnabled   (nsMsgSearchAttrib::Keywords, nsMsgSearchOp::Isnt, 1);
  aTable->SetAvailable (nsMsgSearchAttrib::Keywords, nsMsgSearchOp::IsEmpty, 1);
  aTable->SetEnabled   (nsMsgSearchAttrib::Keywords, nsMsgSearchOp::IsEmpty, 1);
  aTable->SetAvailable (nsMsgSearchAttrib::Keywords, nsMsgSearchOp::IsntEmpty, 1);
  aTable->SetEnabled   (nsMsgSearchAttrib::Keywords, nsMsgSearchOp::IsntEmpty, 1);

  aTable->SetAvailable (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::Contains, 1);
  aTable->SetEnabled   (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::Contains, 1);
  aTable->SetAvailable (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::DoesntContain, 1);
  aTable->SetEnabled   (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::DoesntContain, 1);
  aTable->SetAvailable (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::Is, 1);
  aTable->SetEnabled   (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::Is, 1);
  aTable->SetAvailable (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::Isnt, 1);
  aTable->SetEnabled   (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::Isnt, 1);
  aTable->SetAvailable (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::BeginsWith, 1);
  aTable->SetEnabled   (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::BeginsWith, 1);
  aTable->SetAvailable (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::EndsWith, 1);
  aTable->SetEnabled   (nsMsgSearchAttrib::OtherHeader, nsMsgSearchOp::EndsWith, 1);
  return NS_OK;

}

nsresult nsMsgSearchValidityManager::InitLocalNewsTable()
{
  NS_ASSERTION (nullptr == m_localNewsTable, "already have local news validity table");
  nsresult rv = NewTable(getter_AddRefs(m_localNewsTable));
  NS_ENSURE_SUCCESS(rv, rv);
  return SetLocalNews(m_localNewsTable);
}

nsresult nsMsgSearchValidityManager::InitLocalNewsBodyTable()
{
  NS_ASSERTION (nullptr == m_localNewsBodyTable, "already have local news+body validity table");
  nsresult rv = NewTable(getter_AddRefs(m_localNewsBodyTable));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = SetLocalNews(m_localNewsBodyTable);
  NS_ENSURE_SUCCESS(rv, rv);
  return SetBody(m_localNewsBodyTable);
}

nsresult nsMsgSearchValidityManager::InitLocalNewsJunkTable()
{
  NS_ASSERTION (nullptr == m_localNewsJunkTable, "already have local news+junk validity table");
  nsresult rv = NewTable(getter_AddRefs(m_localNewsJunkTable));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = SetLocalNews(m_localNewsJunkTable);
  NS_ENSURE_SUCCESS(rv, rv);
  return SetJunk(m_localNewsJunkTable);
}

nsresult nsMsgSearchValidityManager::InitLocalNewsJunkBodyTable()
{
  NS_ASSERTION (nullptr == m_localNewsJunkBodyTable, "already have local news+junk+body validity table");
  nsresult rv = NewTable(getter_AddRefs(m_localNewsJunkBodyTable));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = SetLocalNews(m_localNewsJunkBodyTable);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = SetJunk(m_localNewsJunkBodyTable);
  NS_ENSURE_SUCCESS(rv, rv);
  return SetBody(m_localNewsJunkBodyTable);
}
