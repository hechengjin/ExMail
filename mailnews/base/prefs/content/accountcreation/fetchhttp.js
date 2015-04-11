/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This is a small wrapper around XMLHttpRequest, which solves various
 * inadequacies of the API, e.g. error handling. It is entirely generic and
 * can be used for purposes outside of even mail.
 *
 * It does not provide download progress, but assumes that the
 * fetched resource is so small (<1 10 KB) that the roundtrip and
 * response generation is far more significant than the
 * download time of the response. In other words, it's fine for RPC,
 * but not for bigger file downloads.
 */

/**
 * Set up a fetch.
 *
 * @param url {String}   URL of the server function.
 *    ATTENTION: The caller needs to make sure that the URL is secure to call.
 * @param urlArgs {Object, associative array} Parameters to add
 *   to the end of the URL as query string. E.g.
 *   { foo: "bla", bar: "blub blub" } will add "?foo=bla&bar=blub%20blub"
 *   to the URL
 *   (unless the URL already has a "?", then it adds "&foo...").
 *   The values will be urlComponentEncoded, so pass them unencoded.
 * @param post {Boolean}   HTTP GET or POST
 *   Only influences the HTTP request method,
 *   i.e. first line of the HTTP request, not the body or parameters.
 *   Use POST when you modify server state,
 *   GET when you only request information.
 *
 * @param successCallback {Function(result {String})}
 *   Called when the server call worked (no errors).
 *   |result| will contain the body of the HTTP reponse, as string.
 * @param errorCallback {Function(ex)}
 *   Called in case of error. ex contains the error
 *   with a user-displayable but not localized |.message| and maybe a
 *   |.code|, which can be either
 *  - an nsresult error code,
 *  - an HTTP result error code (0...1000) or
 *  - negative: 0...-100 :
 *     -2 = can't resolve server in DNS etc.
 *     -4 = response body (e.g. XML) malformed
 */
/* not yet supported:
 * @param headers {Object, associative array} Like urlArgs,
 *   just that the params will be added as HTTP headers.
 *   { foo: "blub blub" } will add "Foo: Blub blub"
 *   The values will be urlComponentEncoded, apart from space,
 *   so pass them unencoded.
 * @param headerArgs {Object, associative array} Like urlArgs,
 *   just that the params will be added as HTTP headers.
 *   { foo: "blub blub" } will add "X-Moz-Arg-Foo: Blub blub"
 *   The values will be urlComponentEncoded, apart from space,
 *   so pass them unencoded.
 * @param bodyArgs {Object, associative array} Like urlArgs,
 *   just that the params will be sent x-url-encoded in the body,
 *   like a HTML form post.
 *   The values will be urlComponentEncoded, so pass them unencoded.
 *   This cannot be used together with |uploadBody|.
 * @param uploadbody {Object}   Arbitrary object, which to use as
 *   body of the HTTP request. Will also set the mimetype accordingly.
 *   Only supported object types, currently only E4X is supported
 *   (sending XML).
 *   Usually, you have nothing to upload, so just pass |null|.
 */
function FetchHTTP(url, urlArgs, post, successCallback, errorCallback)
{
  assert(typeof(successCallback) == "function", "BUG: successCallback");
  assert(typeof(errorCallback) == "function", "BUG: errorCallback");
  this._url = sanitize.string(url);
  if (!urlArgs)
    urlArgs = {};

  this._urlArgs = urlArgs;
  this._post = sanitize.boolean(post);
  this._successCallback = successCallback;
  this._errorCallback = errorCallback;
}
FetchHTTP.prototype =
{
  _url : null, // URL as passed to ctor, without arguments
  _urlArgs : null,
  _post : null,
  _successCallback : null,
  _errorCallback : null,
  _request : null, // the XMLHttpRequest object
  result : null,

  start : function()
  {
    var url = this._url;
    for (var name in this._urlArgs)
    {
      url += (url.indexOf("?") == -1 ? "?" : "&") +
              name + "=" + encodeURIComponent(this._urlArgs[name]);
    }
    this._request = new XMLHttpRequest();
    let request = this._request;
    request.open(this._post ? "POST" : "GET", url);
    request.channel.loadGroup = null;
    // needs bug 407190 patch v4 (or higher) - uncomment if that lands.
    // try {
    //    var channel = request.channel.QueryInterface(Ci.nsIHttpChannel2);
    //    channel.connectTimeout = 5;
    //    channel.requestTimeout = 5;
    //    } catch (e) { dump(e + "\n"); }

    var me = this;
    request.onload = function() { me._response(true); }
    request.onerror = function() { me._response(false); }
    request.send(null);
  },
  _response : function(success, exStored)
  {
    try
    {
    var errorCode = null;
    var errorStr = null;

    if (success && this._request.status >= 200 &&
        this._request.status < 300) // HTTP level success
    {
      try
      {
        // response
        var mimetype = this._request.getResponseHeader("Content-Type");
        if (!mimetype)
          mimetype = "";
        mimetype = mimetype.split(";")[0];
        if (mimetype == "text/xml" ||
            mimetype == "application/xml" ||
            mimetype == "text/rdf")
        {
          // Bug 270553 prevents usage of .responseXML
          var text = this._request.responseText;
           // Bug 336551 trips over <?xml ... >
          text = text.replace(/<\?xml[^>]*\?>/, "");
          this.result = new XML(text);
        }
        else
        {
          //ddump("mimetype: " + mimetype + " only supported as text");
          this.result = this._request.responseText;
        }
        //ddump("result:\n" + this.result);
      }
      catch (e)
      {
        success = false;
        errorStr = getStringBundle(
                   "chrome://messenger/locale/accountCreationUtil.properties")
                   .GetStringFromName("bad_response_content.error");
        errorCode = -4;
      }
    }
    else
    {
      success = false;
      try
      {
        errorCode = this._request.status;
        errorStr = this._request.statusText;
      } catch (e) {
        // If we can't resolve the hostname in DNS etc., .statusText throws
        errorCode = -2;
        errorStr = getStringBundle(
                   "chrome://messenger/locale/accountCreationUtil.properties")
                   .GetStringFromName("cannot_contact_server.error");
        ddump(errorStr);
      }
    }

    // Callbacks
    if (success)
    {
      try {
        this._successCallback(this.result);
      } catch (e) {
        logException(e);
        this._error(e);
      }
    }
    else if (exStored)
      this._error(exStored);
    else
      this._error(new ServerException(errorStr, errorCode, this._url));

    if (this._finishedCallback)
    {
      try {
        this._finishedCallback(this);
      } catch (e) {
        logException(e);
        this._error(e);
      }
    }

    } catch (e) {
      // error in our fetchhttp._response() code
      logException(e);
      this._error(e);
    }
  },
  _error : function(e)
  {
    try {
      this._errorCallback(e);
    } catch (e) {
      // error in errorCallback, too!
      logException(e);
      alertPrompt("Error in errorCallback for fetchhttp", e);
    }
  },
  /**
   * Call this between start() and finishedCallback fired.
   */
  cancel : function(ex)
  {
    assert(!this.result, "Call already returned");

    this._request.abort();

    // Need to manually call error handler
    // <https://bugzilla.mozilla.org/show_bug.cgi?id=218236#c11>
    this._response(false, ex ? ex : new UserCancelledException());
  },
  /**
   * Allows caller or lib to be notified when the call is done.
   * This is useful to enable and disable a Cancel button in the UI,
   * which allows to cancel the network request.
   */
  setFinishedCallback : function(finishedCallback)
  {
    this._finishedCallback = finishedCallback;
  }
}
extend(FetchHTTP, Abortable);


function CancelledException(msg)
{
  Exception.call(this, msg);
}
CancelledException.prototype =
{
}
extend(CancelledException, Exception);

function UserCancelledException(msg)
{
  // The user knows they cancelled so I don't see a need
  // for a message to that effect.
  if (!msg)
    msg = "User cancelled";
  CancelledException.call(this, msg);
}
UserCancelledException.prototype =
{
}
extend(UserCancelledException, CancelledException);

function ServerException(msg, code, uri)
{
  Exception.call(this, msg);
  this.code = code;
  this.uri = uri;
}
ServerException.prototype =
{
}
extend(ServerException, Exception);
