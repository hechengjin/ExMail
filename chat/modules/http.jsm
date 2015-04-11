/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const EXPORTED_SYMBOLS = ["doXHRequest", "percentEncode"];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource:///modules/imXPCOMUtils.jsm");

initLogModule("xhr", this);

// Strictly follow RFC 3986 when encoding URI components.
function percentEncode(aString)
  encodeURIComponent(aString).replace(/[!'()]/g, escape).replace(/\*/g, "%2A");

function doXHRequest(aUrl, aHeaders, aPOSTData, aOnLoad, aOnError, aThis) {
  let xhr = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"]
              .createInstance(Ci.nsIXMLHttpRequest);
  xhr.mozBackgroundRequest = true; // no error dialogs
  xhr.open(aPOSTData ? "POST" : "GET", aUrl);
  xhr.channel.loadFlags = Ci.nsIChannel.LOAD_ANONYMOUS | // don't send cookies
                          Ci.nsIChannel.LOAD_BYPASS_CACHE |
                          Ci.nsIChannel.INHIBIT_CACHING;
  xhr.onerror = function(aProgressEvent) {
    if (aOnError) {
      // adapted from toolkit/mozapps/extensions/nsBlocklistService.js
      let request = aProgressEvent.target;
      let status;
      try {
        // may throw (local file or timeout)
        status = request.status;
      }
      catch (e) {
        request = request.channel.QueryInterface(Ci.nsIRequest);
        status = request.status;
      }
      // When status is 0 we don't have a valid channel.
      let statusText = status ? request.statusText : "offline";
      aOnError.call(aThis, statusText, null, this);
    }
  };
  xhr.onload = function (aRequest) {
    try {
      let target = aRequest.target;
      DEBUG("Received response: " + target.responseText);
      if (target.status != 200) {
        let errorText = target.responseText;
        if (!errorText || /<(ht|\?x)ml\b/i.test(errorText))
          errorText = target.statusText;
        throw target.status + " - " + errorText;
      }
      if (aOnLoad)
        aOnLoad.call(aThis, target.responseText, this);
    } catch (e) {
      Cu.reportError(e);
      if (aOnError)
        aOnError.call(aThis, e, aRequest.target.responseText, this);
    }
  };

  if (aHeaders) {
    aHeaders.forEach(function(header) {
      xhr.setRequestHeader(header[0], header[1]);
    });
  }

  let POSTData = "";
  if (aPOSTData) {
    xhr.setRequestHeader("Content-Type",
                         "application/x-www-form-urlencoded; charset=utf-8");
    POSTData = aPOSTData.map(function(p) p[0] + "=" + percentEncode(p[1]))
                        .join("&");
  }

  LOG("sending request to " + aUrl + " (POSTData = " + POSTData + ")");
  xhr.send(POSTData);
  return xhr;
}
