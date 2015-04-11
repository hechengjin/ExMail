/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

const ACR = Components.interfaces.nsIAutoCompleteResult;
const nsIAbAutoCompleteResult = Components.interfaces.nsIAbAutoCompleteResult;
const nsIAbDirectoryQueryResultListener =
  Components.interfaces.nsIAbDirectoryQueryResultListener;

// nsAbLDAPAutoCompleteResult
// Derived from nsIAbAutoCompleteResult, provides a LDAP specific result
// implementation.

function nsAbLDAPAutoCompleteResult(aSearchString) {
  // Can't create this in the prototype as we'd get the same array for
  // all instances
  this._searchResults = [];
  this.searchString = aSearchString;
}

nsAbLDAPAutoCompleteResult.prototype = {
  _searchResults: null,
  _commentColumn: "",

  // nsIAutoCompleteResult

  searchString: null,
  searchResult: ACR.RESULT_NOMATCH,
  defaultIndex: -1,
  errorDescription: null,

  get matchCount() {
    return this._searchResults.length;
  },

  getLabelAt: function getLabelAt(aIndex) {
    return this.getValueAt(aIndex);
  },

  getValueAt: function getValueAt(aIndex) {
    return this._searchResults[aIndex].value;
  },

  getCommentAt: function getCommentAt(aIndex) {
    return this._commentColumn;
  },

  getStyleAt: function getStyleAt(aIndex) {
    return this.searchResult == ACR.RESULT_FAILURE ? "remote-err" :
                                                     "remote-abook";
  },

  getImageAt: function getImageAt(aIndex) {
    return "";
  },

  removeValueAt: function removeValueAt(aRowIndex, aRemoveFromDB) {
  },

  // nsIAbAutoCompleteResult

  getCardAt: function getCardAt(aIndex) {
    return this._searchResults[aIndex].card;
  },

  // nsISupports

  QueryInterface: XPCOMUtils.generateQI([ACR, nsIAbAutoCompleteResult])
}

function nsAbLDAPAutoCompleteSearch() {
  Components.classes["@mozilla.org/observer-service;1"]
            .getService(Components.interfaces.nsIObserverService)
            .addObserver(this, "quit-application", false);
}

nsAbLDAPAutoCompleteSearch.prototype = {
  // For component registration
  classID: Components.ID("227e6482-fe9f-441f-9b7d-7b60375e7449"),

  // A cache of Address Books, directories and search contexts.
  // The cache is indexed by the address book URI. Each item in the cache
  // then has three items:
  // book - the address book associated with the URI.
  // query - the nsAbLDAPDirectoryQuery in use for the URI.
  // context - the search context for the URI (used for cancelling the search).
  _cachedQueries: {},

  // The URI of the currently active query.
  _activeQuery: null,

  // Cache of the identity to save getting it each time if it doesn't change
  _cachedParam: null,
  _cachedIdentity: null,

  // The current search result.
  _result: null,
  // The listener to pass back results to.
  _listener: null,

  _parser: Components.classes["@mozilla.org/messenger/headerparser;1"]
                     .getService(Components.interfaces.nsIMsgHeaderParser),

  // Private methods
  // fullString is the full search string.
  // rest is anything after the first word.
  _checkEntry: function _checkEntry(card, search) {
    return card.displayName.toLocaleLowerCase().lastIndexOf(search, 0) == 0 ||
           card.primaryEmail.toLocaleLowerCase().lastIndexOf(search, 0) == 0;
  },

  _checkDuplicate: function _checkDuplicate(card, emailAddress) {
    var lcEmailAddress = emailAddress.toLocaleLowerCase();

    return this._result._searchResults.some(function(result) {
      return result.value.toLocaleLowerCase() == lcEmailAddress;
    });
  },

  _addToResult: function _addToResult(card) {
    var emailAddress =
      this._parser.makeFullAddress(card.displayName, card.isMailList ?
        card.getProperty("Notes", "") || card.displayName : card.primaryEmail);

    // The old code used to try it manually. I think if the parser can't work
    // out the address from what we've supplied, then its busted and we're not
    // going to do any better doing it manually.
    if (!emailAddress)
      return;

    // If it is a duplicate, then just return and don't add it. The
    // _checkDuplicate function deals with it all for us.
    if (this._checkDuplicate(card, emailAddress))
      return;

    // Find out where to insert the card.
    var insertPosition = 0;

    // Next sort on full address
    while (insertPosition < this._result._searchResults.length &&
           emailAddress > this._result._searchResults[insertPosition].value)
      ++insertPosition;

    this._result._searchResults.splice(insertPosition, 0, {
      value: emailAddress,
      card: card,
    });
  },

  // nsIObserver

  observe: function observer(subject, topic, data) {
    if (topic == "quit-application") {
      // Force the individual query items to null, so that the memory
      // gets collected straight away.
      for (var item in this._cachedQueries) {
        this._cachedQueries[item].query = null;
        this._cachedQueries[item].book = null;
        this._cachedQueries[item].attributes = null;
      }
      this._cachedQueries = {};
      Components.classes["@mozilla.org/observer-service;1"]
                .getService(Components.interfaces.nsIObserverService)
                .removeObserver(this, "quit-application");
    }
  },

  // nsIAutoCompleteSearch

  startSearch: function startSearch(aSearchString, aParam,
                                    aPreviousResult, aListener) {
    this._result = new nsAbLDAPAutoCompleteResult(aSearchString);
    aSearchString = aSearchString.toLocaleLowerCase();

    // If the search string isn't value, or contains a comma, or the user
    // hasn't enabled autocomplete, then just return no matches / or the
    // result ignored.
    // The comma check is so that we don't autocomplete against the user
    // entering multiple addresses.
    if (!aSearchString || /,/.test(aSearchString)) {
      this._result.searchResult = ACR.RESULT_IGNORED;
      aListener.onSearchResult(this, this._result);
      return;
    }

    // Compare lowercase strings, because autocomplete may mangle the case
    // depending on the previous results.
    if (aPreviousResult instanceof nsIAbAutoCompleteResult &&
        aSearchString.lastIndexOf(
          aPreviousResult.searchString.toLocaleLowerCase(), 0) == 0 &&
        aPreviousResult.searchResult == ACR.RESULT_SUCCESS) {
      // We have successful previous matches, therefore iterate through the
      // list and reduce as appropriate.
      for (var i = 0; i < aPreviousResult.matchCount; ++i) {
        var card = aPreviousResult.getCardAt(i);
        if (this._checkEntry(card, aSearchString)) {
          // If it matches, just add it straight onto the array, these will
          // already be in order because the previous search returned them
          // in the correct order.
          this._result._searchResults.push({
            value: aPreviousResult.getValueAt(i),
            card: card
          });
        }
      }

      if (this._result.matchCount) {
        this._result.searchResult = ACR.RESULT_SUCCESS;
        // For LDAP results, the comment column is always the same, so just
        // use the first value from the previous results to set the value.
        this._result._commentColumn = aPreviousResult.getCommentAt(0);
      }
      aListener.onSearchResult(this, this._result);
      return;
    }

    // Here we need to do the real search, as we haven't got any previous
    // results to fall back on.

    if (aParam != this._cachedParam) {
      this._cachedIdentity =
        Components.classes['@mozilla.org/messenger/account-manager;1']
                  .getService(Components.interfaces.nsIMsgAccountManager)
                  .getIdentity(aParam);
      this._cachedParam = aParam;
    }

    // The rules here: If the current identity has a directoryServer set, then
    // use that, otherwise, try the global preference instead.
    var acDirURI = null;

    // Does the current identity override the global preference?
    if (this._cachedIdentity.overrideGlobalPref)
      acDirURI = this._cachedIdentity.directoryServer;
    else {
      // Try the global one
      var prefSvc = Components.classes["@mozilla.org/preferences-service;1"]
                              .getService(Components.interfaces.nsIPrefBranch);
      if (prefSvc.getBoolPref("ldap_2.autoComplete.useDirectory"))
        acDirURI = prefSvc.getCharPref("ldap_2.autoComplete.directoryServer");
    }

    if (!acDirURI) {
      // No directory to search, send a no match and return.
      aListener.onSearchResult(this, this._result);
      return;
    }

    var abMgr = Components.classes["@mozilla.org/abmanager;1"]
                          .getService(Components.interfaces.nsIAbManager);

    // If we don't already have a cached query for this URI, build a new one.
    if (!(acDirURI in this._cachedQueries)) {
      var query =
        Components.classes["@mozilla.org/addressbook/ldap-directory-query;1"]
                  .createInstance(Components.interfaces.nsIAbDirectoryQuery);
      var book = abMgr.getDirectory("moz-abldapdirectory://" + acDirURI)
                      .QueryInterface(Components.interfaces.nsIAbLDAPDirectory);

      // Create a minimal map just for the display name and primary email.
      var attributes =
        Components.classes["@mozilla.org/addressbook/ldap-attribute-map;1"]
                  .createInstance(Components.interfaces.nsIAbLDAPAttributeMap);
      attributes.setAttributeList("DisplayName",
        book.attributeMap.getAttributeList("DisplayName", {}), true);
      attributes.setAttributeList("PrimaryEmail",
        book.attributeMap.getAttributeList("PrimaryEmail", {}), true);

      this._cachedQueries[acDirURI] = {
        attributes: attributes,
        book: book,
        context: -1,
        query: query
      };
    }

    this.stopSearch();

    this._activeQuery = acDirURI;

    var queryObject = this._cachedQueries[acDirURI];

    this._result._commentColumn = queryObject.book.dirName;
    this._listener = aListener;

    var args =
      Components.classes["@mozilla.org/addressbook/directory/query-arguments;1"]
                .createInstance(Components.interfaces.nsIAbDirectoryQueryArguments);

    // These are hard-coded for now and the _checkEntry function should match
    // what these statements do.
    var searchStr = "(or(PrimaryEmail,bw,@V)(DisplayName,bw,@V))";
    searchStr = searchStr.replace(/@V/g, encodeURIComponent(aSearchString));

    args.expression = abMgr.convertQueryStringToExpression(searchStr);
    args.typeSpecificArg = queryObject.attributes;
    args.querySubDirectories = true;

    // Start the actual search
    queryObject.context =
      queryObject.query.doQuery(queryObject.book, args, this,
                                queryObject.book.maxHits, 0);
  },

  stopSearch: function stopSearch() {
    if (this._activeQuery) {
      this._cachedQueries[this._activeQuery].query
          .stopQuery(this._cachedQueries[this._activeQuery].context);
      this._listener = null;
      this._activeQuery = null;
    }
  },

  // nsIAbDirSearchListener

  onSearchFinished: function onSearchFinished(aResult, aErrorMsg) {
    if (!this._listener)
      return;

    if (aResult == nsIAbDirectoryQueryResultListener.queryResultComplete) {
      if (this._result.matchCount) {
        this._result.searchResult = ACR.RESULT_SUCCESS;
        this._result.defaultIndex = 0;
      }
      else
        this._result.searchResult = ACR.RESULT_NOMATCH;
    }
    else if (aResult == nsIAbDirectoryQueryResultListener.queryResultError) {
      this._result.searchResult = ACR.RESULT_FAILURE;
      this._result.defaultIndex = 0;
    }
    //    const long queryResultStopped  = 2;
    //    const long queryResultError    = 3;
    this._activeQuery = null;
    this._listener.onSearchResult(this, this._result);
    this._listener = null;
  },

  onSearchFoundCard: function onSearchFoundCard(aCard) {
    if (!this._listener)
      return;

    this._addToResult(aCard);

    /* XXX autocomplete doesn't expect you to rearrange while searching
    if (this._result.matchCount)
      this._result.searchResult = ACR.RESULT_SUCCESS_ONGOING;
    else
      this._result.searchResult = ACR.RESULT_NOMATCH_ONGOING;

    this._listener.onSearchResult(this, this._result);
    */
  },

  // nsISupports

  QueryInterface: XPCOMUtils.generateQI([Components.interfaces.nsIObserver,
                                         Components.interfaces
                                                   .nsIAutoCompleteSearch,
                                         Components.interfaces
                                                   .nsIAbDirSearchListener])
};

// Module

const NSGetFactory = XPCOMUtils.generateNSGetFactory([nsAbLDAPAutoCompleteSearch]);
