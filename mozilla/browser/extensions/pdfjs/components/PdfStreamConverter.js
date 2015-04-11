/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */

'use strict';

var EXPORTED_SYMBOLS = ['PdfStreamConverter'];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;
// True only if this is the version of pdf.js that is included with firefox.
const MOZ_CENTRAL = true;
const PDFJS_EVENT_ID = 'pdf.js.message';
const PDF_CONTENT_TYPE = 'application/pdf';
const PREF_PREFIX = 'pdfjs';
const PDF_VIEWER_WEB_PAGE = 'resource://pdf.js/web/viewer.html';
const MAX_DATABASE_LENGTH = 4096;
const FIREFOX_ID = '{ec8030f7-c20a-464f-9b0e-13a3a9e97384}';
const SEAMONKEY_ID = '{92650c4d-4b8e-4d2a-b7eb-24ecf4f6b63a}';

Cu.import('resource://gre/modules/XPCOMUtils.jsm');
Cu.import('resource://gre/modules/Services.jsm');
Cu.import('resource://gre/modules/NetUtil.jsm');


let appInfo = Cc['@mozilla.org/xre/app-info;1']
                  .getService(Ci.nsIXULAppInfo);
let privateBrowsing, inPrivateBrowsing;
let Svc = {};
XPCOMUtils.defineLazyServiceGetter(Svc, 'mime',
                                   '@mozilla.org/mime;1',
                                   'nsIMIMEService');

if (appInfo.ID === FIREFOX_ID) {
  privateBrowsing = Cc['@mozilla.org/privatebrowsing;1']
                          .getService(Ci.nsIPrivateBrowsingService);
  inPrivateBrowsing = privateBrowsing.privateBrowsingEnabled;
} else if (appInfo.ID === SEAMONKEY_ID) {
  privateBrowsing = null;
  inPrivateBrowsing = false;
}

function getBoolPref(pref, def) {
  try {
    return Services.prefs.getBoolPref(pref);
  } catch (ex) {
    return def;
  }
}

function setStringPref(pref, value) {
  let str = Cc['@mozilla.org/supports-string;1']
              .createInstance(Ci.nsISupportsString);
  str.data = value;
  Services.prefs.setComplexValue(pref, Ci.nsISupportsString, str);
}

function getStringPref(pref, def) {
  try {
    return Services.prefs.getComplexValue(pref, Ci.nsISupportsString).data;
  } catch (ex) {
    return def;
  }
}

function log(aMsg) {
  if (!getBoolPref(PREF_PREFIX + '.pdfBugEnabled', false))
    return;
  let msg = 'PdfStreamConverter.js: ' + (aMsg.join ? aMsg.join('') : aMsg);
  Services.console.logStringMessage(msg);
  dump(msg + '\n');
}

function getDOMWindow(aChannel) {
  var requestor = aChannel.notificationCallbacks;
  var win = requestor.getInterface(Components.interfaces.nsIDOMWindow);
  return win;
}

function isEnabled() {
  if (MOZ_CENTRAL) {
    var disabled = getBoolPref(PREF_PREFIX + '.disabled', false);
    if (disabled)
      return false;
    // To also be considered enabled the "Preview in Firefox" option must be
    // selected in the Application preferences.
    var handlerInfo = Svc.mime
                         .getFromTypeAndExtension('application/pdf', 'pdf');
    return handlerInfo.alwaysAskBeforeHandling == false &&
           handlerInfo.preferredAction == Ci.nsIHandlerInfo.handleInternally;
  }
  // Always returns true for the extension since enabling/disabling is handled
  // by the add-on manager.
  return true;
}

function getLocalizedStrings(path) {
  var stringBundle = Cc['@mozilla.org/intl/stringbundle;1'].
      getService(Ci.nsIStringBundleService).
      createBundle('chrome://pdf.js/locale/' + path);

  var map = {};
  var enumerator = stringBundle.getSimpleEnumeration();
  while (enumerator.hasMoreElements()) {
    var string = enumerator.getNext().QueryInterface(Ci.nsIPropertyElement);
    var key = string.key, property = 'textContent';
    var i = key.lastIndexOf('.');
    if (i >= 0) {
      property = key.substring(i + 1);
      key = key.substring(0, i);
    }
    if (!(key in map))
      map[key] = {};
    map[key][property] = string.value;
  }
  return map;
}
function getLocalizedString(strings, id, property) {
  property = property || 'textContent';
  if (id in strings)
    return strings[id][property];
  return id;
}

// PDF data storage
function PdfDataListener(length) {
  this.length = length; // less than 0, if length is unknown
  this.data = new Uint8Array(length >= 0 ? length : 0x10000);
  this.loaded = 0;
}

PdfDataListener.prototype = {
  append: function PdfDataListener_append(chunk) {
    var willBeLoaded = this.loaded + chunk.length;
    if (this.length >= 0 && this.length < willBeLoaded) {
      this.length = -1; // reset the length, server is giving incorrect one
    }
    if (this.length < 0 && this.data.length < willBeLoaded) {
      // data length is unknown and new chunk will not fit in the existing
      // buffer, resizing the buffer by doubling the its last length
      var newLength = this.data.length;
      for (; newLength < willBeLoaded; newLength *= 2) {}
      var newData = new Uint8Array(newLength);
      newData.set(this.data);
      this.data = newData;
    }
    this.data.set(chunk, this.loaded);
    this.loaded = willBeLoaded;
    this.onprogress(this.loaded, this.length >= 0 ? this.length : void(0));
  },
  getData: function PdfDataListener_getData() {
    var data = this.data;
    if (this.loaded != data.length)
      data = data.subarray(0, this.loaded);
    delete this.data; // releasing temporary storage
    return data;
  },
  finish: function PdfDataListener_finish() {
    this.isDataReady = true;
    if (this.oncompleteCallback) {
      this.oncompleteCallback(this.getData());
    }
  },
  error: function PdfDataListener_error(errorCode) {
    this.errorCode = errorCode;
    if (this.oncompleteCallback) {
      this.oncompleteCallback(null, errorCode);
    }
  },
  onprogress: function() {},
  set oncomplete(value) {
    this.oncompleteCallback = value;
    if (this.isDataReady) {
      value(this.getData());
    }
    if (this.errorCode) {
      value(null, this.errorCode);
    }
  }
};

// All the priviledged actions.
function ChromeActions(domWindow, dataListener) {
  this.domWindow = domWindow;
  this.dataListener = dataListener;
}

ChromeActions.prototype = {
  download: function(data, sendResponse) {
    var originalUrl = data.originalUrl;
    // The data may not be downloaded so we need just retry getting the pdf with
    // the original url.
    var originalUri = NetUtil.newURI(data.originalUrl);
    var blobUri = data.blobUrl ? NetUtil.newURI(data.blobUrl) : originalUri;
    var extHelperAppSvc =
          Cc['@mozilla.org/uriloader/external-helper-app-service;1'].
             getService(Ci.nsIExternalHelperAppService);
    var frontWindow = Cc['@mozilla.org/embedcomp/window-watcher;1'].
                         getService(Ci.nsIWindowWatcher).activeWindow;

    NetUtil.asyncFetch(blobUri, function(aInputStream, aResult) {
      if (!Components.isSuccessCode(aResult)) {
        if (sendResponse)
          sendResponse(true);
        return;
      }
      // Create a nsIInputStreamChannel so we can set the url on the channel
      // so the filename will be correct.
      let channel = Cc['@mozilla.org/network/input-stream-channel;1'].
                       createInstance(Ci.nsIInputStreamChannel);
      channel.setURI(originalUri);
      channel.contentStream = aInputStream;
      channel.QueryInterface(Ci.nsIChannel);

      var listener = {
        extListener: null,
        onStartRequest: function(aRequest, aContext) {
          this.extListener = extHelperAppSvc.doContent('application/pdf',
                                aRequest, frontWindow, false);
          this.extListener.onStartRequest(aRequest, aContext);
        },
        onStopRequest: function(aRequest, aContext, aStatusCode) {
          if (this.extListener)
            this.extListener.onStopRequest(aRequest, aContext, aStatusCode);
          // Notify the content code we're done downloading.
          if (sendResponse)
            sendResponse(false);
        },
        onDataAvailable: function(aRequest, aContext, aInputStream, aOffset,
                                  aCount) {
          this.extListener.onDataAvailable(aRequest, aContext, aInputStream,
                                           aOffset, aCount);
        }
      };

      channel.asyncOpen(listener, null);
    });
  },
  setDatabase: function(data) {
    if (inPrivateBrowsing)
      return;
    // Protect against something sending tons of data to setDatabase.
    if (data.length > MAX_DATABASE_LENGTH)
      return;
    setStringPref(PREF_PREFIX + '.database', data);
  },
  getDatabase: function() {
    if (inPrivateBrowsing)
      return '{}';
    return getStringPref(PREF_PREFIX + '.database', '{}');
  },
  getLocale: function() {
    return getStringPref('general.useragent.locale', 'en-US');
  },
  getLoadingType: function() {
    return this.dataListener ? 'passive' : 'active';
  },
  initPassiveLoading: function() {
    if (!this.dataListener)
      return false;

    var domWindow = this.domWindow;
    this.dataListener.onprogress =
      function ChromeActions_dataListenerProgress(loaded, total) {

      domWindow.postMessage({
        pdfjsLoadAction: 'progress',
        loaded: loaded,
        total: total
      }, '*');
    };

    this.dataListener.oncomplete =
      function ChromeActions_dataListenerComplete(data, errorCode) {

      domWindow.postMessage({
        pdfjsLoadAction: 'complete',
        data: data,
        errorCode: errorCode
      }, '*');

      delete this.dataListener;
    };

    return true;
  },
  getStrings: function(data) {
    try {
      // Lazy initialization of localizedStrings
      if (!('localizedStrings' in this))
        this.localizedStrings = getLocalizedStrings('viewer.properties');

      var result = this.localizedStrings[data];
      return JSON.stringify(result || null);
    } catch (e) {
      log('Unable to retrive localized strings: ' + e);
      return 'null';
    }
  },
  pdfBugEnabled: function() {
    return getBoolPref(PREF_PREFIX + '.pdfBugEnabled', false);
  },
  searchEnabled: function() {
    return getBoolPref(PREF_PREFIX + '.searchEnabled', false);
  },
  fallback: function(url, sendResponse) {
    var self = this;
    var domWindow = this.domWindow;
    var strings = getLocalizedStrings('chrome.properties');
    var message = getLocalizedString(strings, 'unsupported_feature');

    var notificationBox = null;
    // Multiple browser windows can be opened, finding one for notification box
    var windowsEnum = Services.wm
                      .getZOrderDOMWindowEnumerator('navigator:browser', true);
    while (windowsEnum.hasMoreElements()) {
      var win = windowsEnum.getNext();
      if (win.closed)
        continue;
      var browser = win.gBrowser.getBrowserForDocument(domWindow.top.document);
      if (browser) {
        // right window/browser is found, getting the notification box
        notificationBox = win.gBrowser.getNotificationBox(browser);
        break;
      }
    }
    if (!notificationBox) {
      log('Unable to get a notification box for the fallback message');
      return;
    }

    // Flag so we don't call the response callback twice, since if the user
    // clicks open with different viewer both the button callback and
    // eventCallback will be called.
    var sentResponse = false;
    var buttons = [{
      label: getLocalizedString(strings, 'open_with_different_viewer'),
      accessKey: getLocalizedString(strings, 'open_with_different_viewer',
                                    'accessKey'),
      callback: function() {
        sentResponse = true;
        sendResponse(true);
      }
    }];
    notificationBox.appendNotification(message, 'pdfjs-fallback', null,
                                       notificationBox.PRIORITY_WARNING_LOW,
                                       buttons,
                                       function eventsCallback(eventType) {
      // Currently there is only one event "removed" but if there are any other
      // added in the future we still only care about removed at the moment.
      if (eventType !== 'removed')
        return;
      // Don't send a response again if we already responded when the button was
      // clicked.
      if (!sentResponse)
        sendResponse(false);
    });
  }
};

// Event listener to trigger chrome privedged code.
function RequestListener(actions) {
  this.actions = actions;
}
// Receive an event and synchronously or asynchronously responds.
RequestListener.prototype.receive = function(event) {
  var message = event.target;
  var doc = message.ownerDocument;
  var action = message.getUserData('action');
  var data = message.getUserData('data');
  var sync = message.getUserData('sync');
  var actions = this.actions;
  if (!(action in actions)) {
    log('Unknown action: ' + action);
    return;
  }
  if (sync) {
    var response = actions[action].call(this.actions, data);
    message.setUserData('response', response, null);
  } else {
    var response;
    if (!message.getUserData('callback')) {
      doc.documentElement.removeChild(message);
      response = null;
    } else {
      response = function sendResponse(response) {
        message.setUserData('response', response, null);

        var listener = doc.createEvent('HTMLEvents');
        listener.initEvent('pdf.js.response', true, false);
        return message.dispatchEvent(listener);
      }
    }
    actions[action].call(this.actions, data, response);
  }
};

function PdfStreamConverter() {
}

PdfStreamConverter.prototype = {

  // properties required for XPCOM registration:
  classID: Components.ID('{d0c5195d-e798-49d4-b1d3-9324328b2291}'),
  classDescription: 'pdf.js Component',
  contractID: '@mozilla.org/streamconv;1?from=application/pdf&to=*/*',

  QueryInterface: XPCOMUtils.generateQI([
      Ci.nsISupports,
      Ci.nsIStreamConverter,
      Ci.nsIStreamListener,
      Ci.nsIRequestObserver
  ]),

  /*
   * This component works as such:
   * 1. asyncConvertData stores the listener
   * 2. onStartRequest creates a new channel, streams the viewer and cancels
   *    the request so pdf.js can do the request
   * Since the request is cancelled onDataAvailable should not be called. The
   * onStopRequest does nothing. The convert function just returns the stream,
   * it's just the synchronous version of asyncConvertData.
   */

  // nsIStreamConverter::convert
  convert: function(aFromStream, aFromType, aToType, aCtxt) {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },

  // nsIStreamConverter::asyncConvertData
  asyncConvertData: function(aFromType, aToType, aListener, aCtxt) {
    if (!isEnabled())
      throw Cr.NS_ERROR_NOT_IMPLEMENTED;

    var useFetchByChrome = getBoolPref(PREF_PREFIX + '.fetchByChrome', true);
    if (!useFetchByChrome) {
      // Ignoring HTTP POST requests -- pdf.js has to repeat the request.
      var skipConversion = false;
      try {
        var request = aCtxt;
        request.QueryInterface(Ci.nsIHttpChannel);
        skipConversion = (request.requestMethod !== 'GET');
      } catch (e) {
        // Non-HTTP request... continue normally.
      }
      if (skipConversion)
        throw Cr.NS_ERROR_NOT_IMPLEMENTED;
    }

    // Store the listener passed to us
    this.listener = aListener;
  },

  // nsIStreamListener::onDataAvailable
  onDataAvailable: function(aRequest, aContext, aInputStream, aOffset, aCount) {
    if (!this.dataListener) {
      // Do nothing since all the data loading is handled by the viewer.
      return;
    }

    var binaryStream = this.binaryStream;
    binaryStream.setInputStream(aInputStream);
    this.dataListener.append(binaryStream.readByteArray(aCount));
  },

  // nsIRequestObserver::onStartRequest
  onStartRequest: function(aRequest, aContext) {

    // Setup the request so we can use it below.
    aRequest.QueryInterface(Ci.nsIChannel);
    var useFetchByChrome = getBoolPref(PREF_PREFIX + '.fetchByChrome', true);
    var dataListener;
    if (useFetchByChrome) {
      // Creating storage for PDF data
      var contentLength = aRequest.contentLength;
      dataListener = new PdfDataListener(contentLength);
      this.dataListener = dataListener;
      this.binaryStream = Cc['@mozilla.org/binaryinputstream;1']
                          .createInstance(Ci.nsIBinaryInputStream);
    } else {
      // Cancel the request so the viewer can handle it.
      aRequest.cancel(Cr.NS_BINDING_ABORTED);
    }

    // Create a new channel that is viewer loaded as a resource.
    var ioService = Services.io;
    var channel = ioService.newChannel(
                    PDF_VIEWER_WEB_PAGE, null, null);

    var listener = this.listener;
    var self = this;
    // Proxy all the request observer calls, when it gets to onStopRequest
    // we can get the dom window.
    var proxy = {
      onStartRequest: function() {
        listener.onStartRequest.apply(listener, arguments);
      },
      onDataAvailable: function() {
        listener.onDataAvailable.apply(listener, arguments);
      },
      onStopRequest: function() {
        var domWindow = getDOMWindow(channel);
        // Double check the url is still the correct one.
        if (domWindow.document.documentURIObject.equals(aRequest.URI)) {
          let actions = new ChromeActions(domWindow, dataListener);
          let requestListener = new RequestListener(actions);
          domWindow.addEventListener(PDFJS_EVENT_ID, function(event) {
            requestListener.receive(event);
          }, false, true);
        }
        listener.onStopRequest.apply(listener, arguments);
      }
    };

    // Keep the URL the same so the browser sees it as the same.
    channel.originalURI = aRequest.URI;
    channel.asyncOpen(proxy, aContext);
    if (useFetchByChrome) {
      // We can use resource principal when data is fetched by the chrome
      // e.g. useful for NoScript
      var securityManager = Cc['@mozilla.org/scriptsecuritymanager;1']
                            .getService(Ci.nsIScriptSecurityManager);
      var uri = ioService.newURI(PDF_VIEWER_WEB_PAGE, null, null);
      // FF16 and below had getCodebasePrincipal (bug 774585)
      var resourcePrincipal = 'getSimpleCodebasePrincipal' in securityManager ?
                              securityManager.getSimpleCodebasePrincipal(uri) :
                              securityManager.getCodebasePrincipal(uri);
      channel.owner = resourcePrincipal;
    }
  },

  // nsIRequestObserver::onStopRequest
  onStopRequest: function(aRequest, aContext, aStatusCode) {
    if (!this.dataListener) {
      // Do nothing
      return;
    }

    if (Components.isSuccessCode(aStatusCode))
      this.dataListener.finish();
    else
      this.dataListener.error(aStatusCode);
    delete this.dataListener;
    delete this.binaryStream;
  }
};

var NSGetFactory = XPCOMUtils.generateNSGetFactory([PdfStreamConverter]);
