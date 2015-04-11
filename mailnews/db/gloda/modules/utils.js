/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const EXPORTED_SYMBOLS = ['GlodaUtils'];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

/**
 * @namespace A holding place for logic that is not gloda-specific and should
 *  reside elsewhere.
 */
var GlodaUtils = {

  /**
   * This Regexp is super-complicated and used at least in two different parts of
   * the code, so let's expose it from one single location.
   */
  PART_RE: new RegExp("^[^?]+\\?(?:/;section=\\d+\\?)?(?:[^&]+&)*part=([^&]+)(?:&[^&]+)*$"),

  _mimeConverter: null,
  deMime: function gloda_utils_deMime(aString) {
    if (this._mimeConverter == null) {
      this._mimeConverter = Cc["@mozilla.org/messenger/mimeconverter;1"].
                            getService(Ci.nsIMimeConverter);
    }

    return this._mimeConverter.decodeMimeHeader(aString, null, false, true);
  },

  _headerParser: null,

  /**
   * Parses an RFC 2822 list of e-mail addresses and returns an object with
   *  4 attributes, as described below.  We will use the example of the user
   *  passing an argument of '"Bob Smith" <bob@company.com>'.
   *
   * This method (by way of nsIMsgHeaderParser) takes care of decoding mime
   *  headers, but is not aware of folder-level character set overrides.
   *
   * count: the number of addresses parsed. (ex: 1)
   * addresses: a list of e-mail addresses (ex: ["bob@company.com"])
   * names: a list of names (ex: ["Bob Smith"])
   * fullAddresses: aka the list of name and e-mail together (ex: ['"Bob Smith"
   *  <bob@company.com>']).
   *
   * This method is a convenience wrapper around nsIMsgHeaderParser.
   */
  parseMailAddresses: function gloda_utils_parseMailAddresses(aMailAddresses) {
    if (this._headerParser == null) {
      this._headerParser = Cc["@mozilla.org/messenger/headerparser;1"].
                           getService(Ci.nsIMsgHeaderParser);
    }
    let addresses = {}, names = {}, fullAddresses = {};
    this._headerParser.parseHeadersWithArray(aMailAddresses, addresses,
                                             names, fullAddresses);
    return {names: names.value, addresses: addresses.value,
            fullAddresses: fullAddresses.value,
            count: names.value.length};
  },

  /**
   * MD5 hash a string and return the hex-string result. Impl from nsICryptoHash
   *  docs.
   */
  md5HashString: function gloda_utils_md5hash(aString) {
    let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"].
                    createInstance(Ci.nsIScriptableUnicodeConverter);
    let trash = {};
    converter.charset = "UTF-8";
    let data = converter.convertToByteArray(aString, trash);

    let hasher = Cc['@mozilla.org/security/hash;1'].
                 createInstance(Ci.nsICryptoHash);
    hasher.init(Ci.nsICryptoHash.MD5);
    hasher.update(data, data.length);
    let hash = hasher.finish(false);

     // return the two-digit hexadecimal code for a byte
    function toHexString(charCode) {
      return ("0" + charCode.toString(16)).slice(-2);
    }

    // convert the binary hash data to a hex string.
    return [toHexString(hash.charCodeAt(i)) for (i in hash)].join("");
  },

  getCardForEmail: function gloda_utils_getCardForEmail(aAddress) {
    // search through all of our local address books looking for a match.
    let enumerator = Components.classes["@mozilla.org/abmanager;1"]
                               .getService(Ci.nsIAbManager)
                               .directories;
    let cardForEmailAddress;
    let addrbook;
    while (!cardForEmailAddress && enumerator.hasMoreElements())
    {
      addrbook = enumerator.getNext().QueryInterface(Ci.nsIAbDirectory);
      try
      {
        cardForEmailAddress = addrbook.cardForEmailAddress(aAddress);
        if (cardForEmailAddress)
          return cardForEmailAddress;
      } catch (ex) {}
    }

    return null;
  },

  _FORCE_GC_AFTER_NUM_HEADERS: 4096,
  _headersSeen: 0,
  /**
   * As |forceGarbageCollection| says, once XPConnect sees a header, it likes
   *  to hold onto that reference.  This method is used to track the number of
   *  headers we have seen and force a GC when we have to.
   *
   * Ideally the indexer's idle-biased GC mechanism would take care of all the
   *  GC; we are just a failsafe to make sure that our memory usage is bounded
   *  based on the number of headers we have seen rather than just time.
   *  Since holding onto headers can keep databases around too, this also
   *  helps avoid keeping file handles open, etc.
   *
   * |forceGarbageCollection| will zero our tracking variable when a GC happens
   *  so we are informed by the indexer's GC triggering.
   *
   * And of course, we don't want to trigger collections willy nilly because
   *  they have a cost even if there is no garbage.
   *
   * @param aNumHeadersSeen The number of headers code has seen.  A granularity
   *     of hundreds of messages should be fine.
   */
  considerHeaderBasedGC: function(aNumHeadersSeen) {
    this._headersSeen += aNumHeadersSeen;
    if (this._headersSeen >= this._FORCE_GC_AFTER_NUM_HEADERS)
      this.forceGarbageCollection();
  },

  /**
   * Force a garbage-collection sweep.  Gloda has to force garbage collection
   *  periodically because XPConnect's XPCJSRuntime::DeferredRelease mechanism
   *  can end up holding onto a ridiculously high number of XPConnect objects in
   *  between normal garbage collections.  This has mainly posed a problem
   *  because nsAutolock is a jerk in DEBUG builds in 1.9.1, but in theory this
   *  also helps us even out our memory usage.
   * We also are starting to do this more to try and keep the garbage collection
   *  durations acceptable.  We intentionally avoid triggering the cycle
   *  collector in those cases, as we do presume a non-trivial fixed cost for
   *  cycle collection.  (And really all we want is XPConnect to not be a jerk.)
   * This method exists mainly to centralize our GC activities and because if
   *  we do start involving the cycle collector, that is a non-trivial block of
   *  code to copy-and-paste all over the place (at least in a module).
   *
   * @param aCycleCollecting Do we need the cycle collector to run?  Currently
   *     unused / unimplemented, but we would use
   *     nsIDOMWindowUtils.garbageCollect() to do so.
   */
  forceGarbageCollection:
      function gloda_utils_garbageCollection(aCycleCollecting) {
    Cu.forceGC();
    this._headersSeen = 0;
  }
};
