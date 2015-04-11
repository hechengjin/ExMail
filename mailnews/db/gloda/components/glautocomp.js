/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource:///modules/errUtils.js");

var Gloda = null;
var GlodaUtils = null;
var MultiSuffixTree = null;
var TagNoun = null;
var FreeTagNoun = null;

function ResultRowFullText(aItem, words, typeForStyle) {
  this.item = aItem;
  this.words = words;
  this.typeForStyle = "gloda-fulltext-" + typeForStyle;
}
ResultRowFullText.prototype = {
  multi: false,
  fullText: true
};

function ResultRowSingle(aItem, aCriteriaType, aCriteria, aExplicitNounID) {
  this.nounID = aExplicitNounID || aItem.NOUN_ID;
  this.nounDef = Gloda._nounIDToDef[this.nounID];
  this.criteriaType = aCriteriaType;
  this.criteria = aCriteria;
  this.item = aItem;
  this.typeForStyle = "gloda-single-" + this.nounDef.name;
}
ResultRowSingle.prototype = {
  multi: false,
  fullText: false
};

function ResultRowMulti(aNounID, aCriteriaType, aCriteria, aQuery) {
  this.nounID = aNounID;
  this.nounDef = Gloda._nounIDToDef[aNounID];
  this.criteriaType = aCriteriaType;
  this.criteria = aCriteria;
  this.collection = aQuery.getCollection(this);
  this.collection.becomeExplicit();
  this.renderer = null;
}
ResultRowMulti.prototype = {
  multi: true,
  typeForStyle: "gloda-multi",
  fullText: false,
  onItemsAdded: function(aItems) {
    if (this.renderer) {
      for each (let [iItem, item] in Iterator(aItems)) {
        this.renderer.renderItem(item);
      }
    }
  },
  onItemsModified: function(aItems) {
  },
  onItemsRemoved: function(aItems) {
  },
  onQueryCompleted: function() {
  }
};

function nsAutoCompleteGlodaResult(aListener, aCompleter, aString) {
  this.listener = aListener;
  this.completer = aCompleter;
  this.searchString = aString;
  this._results = [];
  this._pendingCount = 0;
  this._problem = false;
  // Track whether we have reported anything to the complete controller so
  //  that we know not to send notifications to it during calls to addRows
  //  prior to that point.
  this._initiallyReported = false;

  this.wrappedJSObject = this;
}
nsAutoCompleteGlodaResult.prototype = {
  getObjectAt: function(aIndex) {
    return this._results[aIndex];
  },
  markPending: function ACGR_markPending(aCompleter) {
    this._pendingCount++;
  },
  markCompleted: function ACGR_markCompleted(aCompleter) {
    if (--this._pendingCount == 0) {
      this.listener.onSearchResult(this.completer, this);
    }
  },
  announceYourself: function ACGR_announceYourself() {
    this._initiallyReported = true;
    this.listener.onSearchResult(this.completer, this);
  },
  addRows: function ACGR_addRows(aRows) {
    if (!aRows.length)
      return;
    this._results.push.apply(this._results, aRows);
    if (this._initiallyReported)
      this.listener.onSearchResult(this.completer, this);
  },
  // ==== nsIAutoCompleteResult
  searchString: null,
  get searchResult() {
    if (this._problem)
      return Ci.nsIAutoCompleteResult.RESULT_FAILURE;
    if (this._results.length)
      return (!this._pendingCount) ? Ci.nsIAutoCompleteResult.RESULT_SUCCESS
                          : Ci.nsIAutoCompleteResult.RESULT_SUCCESS_ONGOING;
    else
      return (!this._pendingCount) ? Ci.nsIAutoCompleteResult.RESULT_NOMATCH
                          : Ci.nsIAutoCompleteResult.RESULT_NOMATCH_ONGOING;
  },
  defaultIndex: -1,
  errorDescription: null,
  get matchCount() {
    return (this._results === null) ? 0 : this._results.length;
  },
  // this is the lower text, (shows the url in firefox)
  // we try and show the contact's name here.
  getValueAt: function(aIndex) {
    let thing = this._results[aIndex];
    return thing.name || thing.value || thing.subject;
  },
  getLabelAt: function(aIndex) {
    return this.getValueAt(aIndex);
  },
  // rich uses this to be the "title".  it is the upper text
  // we try and show the identity here.
  getCommentAt: function(aIndex) {
    let thing = this._results[aIndex];
    if (thing.value) // identity
      return thing.contact.name;
    else
      return thing.name || thing.subject;
  },
  // rich uses this to be the "type"
  getStyleAt: function(aIndex) {
    let row = this._results[aIndex];
    return row.typeForStyle;
  },
  // rich uses this to be the icon
  getImageAt: function(aIndex) {
    let thing = this._results[aIndex];
    if (!thing.value)
      return null;

    return ""; // we don't want to use gravatars as is.

    let md5hash = GlodaUtils.md5HashString(thing.value);
    let gravURL = "http://www.gravatar.com/avatar/" + md5hash +
                                "?d=identicon&s=32&r=g";
    return gravURL;
  },
  removeValueAt: function() {},

  _stop: function() {
  }
};

const MAX_POPULAR_CONTACTS = 200;

/**
 * Complete contacts/identities based on name/email.  Instant phase is based on
 *  a suffix-tree built of popular contacts/identities.  Delayed phase relies
 *  on a LIKE search of all known contacts.
 */
function ContactIdentityCompleter() {
  // get all the contacts
  let contactQuery = Gloda.newQuery(Gloda.NOUN_CONTACT);
  contactQuery.orderBy("-popularity").limit(MAX_POPULAR_CONTACTS);
  this.contactCollection = contactQuery.getCollection(this, null);
  this.contactCollection.becomeExplicit();
}
ContactIdentityCompleter.prototype = {
  _popularitySorter: function(a, b){ return b.popularity - a.popularity; },
  complete: function ContactIdentityCompleter_complete(aResult, aString) {
    if (aString.length < 3) {
      // In CJK, first name or last name is sometime used as 1 character only.
      // So we allow autocompleted search even if 1 character.
      //
      // [U+3041 - U+9FFF ... Full-width Katakana, Hiragana
      //                      and CJK Ideograph
      // [U+AC00 - U+D7FF ... Hangul
      // [U+F900 - U+FFDC ... CJK compatibility ideograph
      if (!aString.match(/[\u3041-\u9fff\uac00-\ud7ff\uf900-\uffdc]/))
        return false;
    }

    let matches;
    if (this.suffixTree) {
      matches = this.suffixTree.findMatches(aString.toLowerCase());
    }
    else
      matches = [];

    // let's filter out duplicates due to identity/contact double-hits by
    //  establishing a map based on the contact id for these guys.
    // let's also favor identities as we do it, because that gets us the
    //  most accurate gravat, potentially
    let contactToThing = {};
    for (let iMatch = 0; iMatch < matches.length; iMatch++) {
      let thing = matches[iMatch];
      if (thing.NOUN_ID == Gloda.NOUN_CONTACT && !(thing.id in contactToThing))
        contactToThing[thing.id] = thing;
      else if (thing.NOUN_ID == Gloda.NOUN_IDENTITY)
        contactToThing[thing.contactID] = thing;
    }
    // and since we can now map from contacts down to identities, map contacts
    //  to the first identity for them that we find...
    matches = [val.NOUN_ID == Gloda.NOUN_IDENTITY ? val : val.identities[0]
               for each ([iVal, val] in Iterator(contactToThing))];

    let rows = [new ResultRowSingle(match, "text", aResult.searchString)
                for each ([iMatch, match] in Iterator(matches))];
    aResult.addRows(rows);

    // - match against database contacts / identities
    let pending = {contactToThing: contactToThing, pendingCount: 2};

    let contactQuery = Gloda.newQuery(Gloda.NOUN_CONTACT);
    contactQuery.nameLike(contactQuery.WILDCARD, aString,
	contactQuery.WILDCARD);
    pending.contactColl = contactQuery.getCollection(this, aResult);
    pending.contactColl.becomeExplicit();

    let identityQuery = Gloda.newQuery(Gloda.NOUN_IDENTITY);
    identityQuery.kind("email").valueLike(identityQuery.WILDCARD, aString,
        identityQuery.WILDCARD);
    pending.identityColl = identityQuery.getCollection(this, aResult);
    pending.identityColl.becomeExplicit();

    aResult._contactCompleterPending = pending;

    return true;
  },
  onItemsAdded: function(aItems, aCollection) {
  },
  onItemsModified: function(aItems, aCollection) {
  },
  onItemsRemoved: function(aItems, aCollection) {
  },
  onQueryCompleted: function(aCollection) {
    // handle the initial setup case...
    if (aCollection.data == null) {
      // cheat and explicitly add our own contact...
      if (Gloda.myContact &&
          !(Gloda.myContact.id in this.contactCollection._idMap))
        this.contactCollection._onItemsAdded([Gloda.myContact]);

      // the set of identities owned by the contacts is automatically loaded as part
      //  of the contact loading...
      // (but only if we actually have any contacts)
      this.identityCollection =
        this.contactCollection.subCollections[Gloda.NOUN_IDENTITY];

      let contactNames = [(c.name.replace(" ", "").toLowerCase() || "x") for each
                          ([, c] in Iterator(this.contactCollection.items))];
      // if we had no contacts, we will have no identity collection!
      let identityMails;
      if (this.identityCollection)
        identityMails = [i.value.toLowerCase() for each
                         ([, i] in Iterator(this.identityCollection.items))];

      // The suffix tree takes two parallel lists; the first contains strings
      //  while the second contains objects that correspond to those strings.
      // In the degenerate case where identityCollection does not exist, it will
      //  be undefined.  Calling concat with an argument of undefined simply
      //  duplicates the list we called concat on, and is thus harmless.  Our
      //  use of && on identityCollection allows its undefined value to be
      //  passed through to concat.  identityMails will likewise be undefined.
      this.suffixTree = new MultiSuffixTree(contactNames.concat(identityMails),
        this.contactCollection.items.concat(this.identityCollection &&
          this.identityCollection.items));

      return;
    }

    // handle the completion case
    let result = aCollection.data;
    let pending = result._contactCompleterPending;

    if (--pending.pendingCount == 0) {
      let possibleDudes = [];

      let contactToThing = pending.contactToThing;

      let items;

      // check identities first because they are better than contacts in terms
      //  of display
      items = pending.identityColl.items;
      for (let iIdentity = 0; iIdentity < items.length; iIdentity++){
        let identity = items[iIdentity];
        if (!(identity.contactID in contactToThing)) {
          contactToThing[identity.contactID] = identity;
          possibleDudes.push(identity);
          // augment the identity with its contact's popularity
          identity.popularity = identity.contact.popularity;
        }
      }
      items = pending.contactColl.items;
      for (let iContact = 0; iContact < items.length; iContact++) {
        let contact = items[iContact];
        if (!(contact.id in contactToThing)) {
          contactToThing[contact.id] = contact;
          possibleDudes.push(contact.identities[0]);
        }
      }

      // sort in order of descending popularity
      possibleDudes.sort(this._popularitySorter);
      let rows = [new ResultRowSingle(dude, "text", result.searchString)
                  for each ([iDude, dude] in Iterator(possibleDudes))];
      result.addRows(rows);
      result.markCompleted(this);

      // the collections no longer care about the result, make it clear.
      delete pending.identityColl.data;
      delete pending.contactColl.data;
      // the result object no longer needs us or our data
      delete result._contactCompleterPending;
    }
  }
};

/**
 * Complete tags that are used on contacts.
 */
function ContactTagCompleter() {
  FreeTagNoun.populateKnownFreeTags();
  this._buildSuffixTree();
  FreeTagNoun.addListener(this);
}
ContactTagCompleter.prototype = {
  _buildSuffixTree: function() {
    let tagNames = [], tags = [];
    for (let [tagName, tag] in Iterator(FreeTagNoun.knownFreeTags)) {
      tagNames.push(tagName.toLowerCase());
      tags.push(tag);
    }
    this._suffixTree = new MultiSuffixTree(tagNames, tags);
    this._suffixTreeDirty = false;
  },
  onFreeTagAdded: function(aTag) {
    this._suffixTreeDirty = true;
  },
  complete: function ContactTagCompleter_complete(aResult, aString) {
    // now is not the best time to do this; have onFreeTagAdded use a timer.
    if (this._suffixTreeDirty)
      this._buildSuffixTree();

    if (aString.length < 2)
      return false; // no async mechanism that will add new rows

    let tags = this._suffixTree.findMatches(aString.toLowerCase());
    let rows = [];
    for each (let [iTag, tag] in Iterator(tags)) {
      let query = Gloda.newQuery(Gloda.NOUN_CONTACT);
      query.freeTags(tag);
      let resRow = new ResultRowMulti(Gloda.NOUN_CONTACT, "tag", tag.name,
                                      query);
      rows.push(resRow);
    }
    aResult.addRows(rows);

    return false; // no async mechanism that will add new rows
  }
};

/**
 * Complete tags that are used on messages
 */
function MessageTagCompleter() {
  this._buildSuffixTree();
}
MessageTagCompleter.prototype = {
  _buildSuffixTree: function MessageTagCompleter__buildSufficeTree() {
    let tagNames = [], tags = [];
    let tagArray = TagNoun.getAllTags();
    for (let iTag = 0; iTag < tagArray.length; iTag++) {
      let tag = tagArray[iTag];
      tagNames.push(tag.tag.toLowerCase());
      tags.push(tag);
    }
    this._suffixTree = new MultiSuffixTree(tagNames, tags);
    this._suffixTreeDirty = false;
  },
  complete: function MessageTagCompleter_complete(aResult, aString) {
    if (aString.length < 2)
      return false;

    let tags = this._suffixTree.findMatches(aString.toLowerCase());
    let rows = [];
    for each (let [, tag] in Iterator(tags)) {
      let resRow = new ResultRowSingle(tag, "tag", tag.tag, TagNoun.id);
      rows.push(resRow);
    }
    aResult.addRows(rows);

    return false; // no async mechanism that will add new rows
  }
};

/**
 * Complete with helpful hints about full-text search
 */
function FullTextCompleter() {
}
FullTextCompleter.prototype = {
  complete: function FullTextCompleter_complete(aResult, aSearchString) {
    if (aSearchString.length < 4)
      return false;
    // We use code very similar to that in msg_search.js, except that we
    // need to detect when we found phrases, as well as strip commas.
    aSearchString = aSearchString.trim();
    let terms = [];
    let phraseFound = false;
    while (aSearchString) {
      let term = "";
      if (aSearchString[0] == '"') {
        let endIndex = aSearchString.indexOf(aSearchString[0], 1);
        // eat the quote if it has no friend
        if (endIndex == -1) {
          aSearchString = aSearchString.substring(1);
          continue;
        }
        phraseFound = true;
        term = aSearchString.substring(1, endIndex).trim();
        if (term)
          terms.push(term);
        aSearchString = aSearchString.substring(endIndex + 1);
        continue;
      }

      let spaceIndex = aSearchString.indexOf(" ");
      if (spaceIndex == -1) {
        terms.push(aSearchString.replace(/,/g, ""));
        break;
      }

      term = aSearchString.substring(0, spaceIndex).replace(/,/g, "");
      if (term)
        terms.push(term);
      aSearchString = aSearchString.substring(spaceIndex+1);
    }

    if (terms.length == 1 && !phraseFound)
      aResult.addRows([new ResultRowFullText(aSearchString, terms, "single")]);
    else
      aResult.addRows([new ResultRowFullText(aSearchString, terms, "all")]);

    return false; // no async mechanism that will add new rows
  }
};

var LOG;

function nsAutoCompleteGloda() {
  this.wrappedJSObject = this;
  try {
    // set up our awesome globals!
    if (Gloda === null) {
      let loadNS = {};
      Cu.import("resource:///modules/gloda/public.js", loadNS);
      Gloda = loadNS.Gloda;

      Cu.import("resource:///modules/gloda/utils.js", loadNS);
      GlodaUtils = loadNS.GlodaUtils;
      Cu.import("resource:///modules/gloda/suffixtree.js", loadNS);
      MultiSuffixTree = loadNS.MultiSuffixTree;
      Cu.import("resource:///modules/gloda/noun_tag.js", loadNS);
      TagNoun = loadNS.TagNoun;
      Cu.import("resource:///modules/gloda/noun_freetag.js", loadNS);
      FreeTagNoun = loadNS.FreeTagNoun;

      Cu.import("resource:///modules/gloda/log4moz.js", loadNS);
      LOG = loadNS["Log4Moz"].repository.getLogger("gloda.autocomp");
    }

    this.completers = [];
    this.curResult = null;

    this.completers.push(new FullTextCompleter());
    this.completers.push(new ContactIdentityCompleter());
    this.completers.push(new ContactTagCompleter());
    this.completers.push(new MessageTagCompleter());
  } catch (e) {
    logException(e);
  }
}

nsAutoCompleteGloda.prototype = {
  classID: Components.ID("{3bbe4d77-3f70-4252-9500-bc00c26f476d}"),
  QueryInterface: XPCOMUtils.generateQI([
      Components.interfaces.nsIAutoCompleteSearch]),

  startSearch: function(aString, aParam, aResult, aListener) {
    try {
      let result = new nsAutoCompleteGlodaResult(aListener, this, aString);
      // save this for hacky access to the search.  I somewhat suspect we simply
      //  should not be using the formal autocomplete mechanism at all.
      this.curResult = result;

      if (aParam == "global") {
        for each (let [iCompleter, completer] in Iterator(this.completers)) {
          // they will return true if they have something pending.
          if (completer.complete(result, aString))
            result.markPending(completer);
        }
      //} else {
      //   It'd be nice to do autocomplete in the quicksearch modes based
      //   on the specific values for that mode in the current view.
      //   But we don't do that yet.
      }

      result.announceYourself();
    } catch (e) {
      logException(e);
    }
  },

  stopSearch: function() {
  }
};

var components = [nsAutoCompleteGloda];
const NSGetFactory = XPCOMUtils.generateNSGetFactory(components);
