/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "SocialService", "resource://gre/modules/SocialService.jsm");

const EXPORTED_SYMBOLS = ["MozSocialAPI", "openChatWindow"];

var MozSocialAPI = {
  _enabled: false,
  _everEnabled: false,
  set enabled(val) {
    let enable = !!val;
    if (enable == this._enabled) {
      return;
    }
    this._enabled = enable;

    if (enable) {
      Services.obs.addObserver(injectController, "document-element-inserted", false);

      if (!this._everEnabled) {
        this._everEnabled = true;
        Services.telemetry.getHistogramById("SOCIAL_ENABLED_ON_SESSION").add(true);
      }

    } else {
      Services.obs.removeObserver(injectController, "document-element-inserted", false);
    }
  }
};

// Called on document-element-inserted, checks that the API should be injected,
// and then calls attachToWindow as appropriate
function injectController(doc, topic, data) {
  try {
    let window = doc.defaultView;
    if (!window)
      return;

    // Do not attempt to load the API into about: error pages
    if (doc.documentURIObject.scheme == "about") {
      return;
    }

    var containingBrowser = window.QueryInterface(Ci.nsIInterfaceRequestor)
                                  .getInterface(Ci.nsIWebNavigation)
                                  .QueryInterface(Ci.nsIDocShell)
                                  .chromeEventHandler;

    let origin = containingBrowser.getAttribute("origin");
    if (!origin) {
      return;
    }

    SocialService.getProvider(origin, function(provider) {
      if (provider && provider.workerURL) {
        attachToWindow(provider, window);
      }
    });
  } catch(e) {
    Cu.reportError("MozSocialAPI injectController: unable to attachToWindow for " + doc.location + ": " + e);
  }
}

// Loads mozSocial support functions associated with provider into targetWindow
function attachToWindow(provider, targetWindow) {
  let origin = provider.origin;
  if (!provider.enabled) {
    throw new Error("MozSocialAPI: cannot attach disabled provider " + origin);
  }

  let targetDocURI = targetWindow.document.documentURIObject;
  if (provider.origin != targetDocURI.prePath) {
    throw new Error("MozSocialAPI: cannot attach " + origin + " to " + targetDocURI.spec);
  }

  var port = provider.getWorkerPort(targetWindow);

  let mozSocialObj = {
    // Use a method for backwards compat with existing providers, but we
    // should deprecate this in favor of a simple .port getter.
    getWorker: {
      enumerable: true,
      configurable: true,
      writable: true,
      value: function() {
        return {
          port: port,
          __exposedProps__: {
            port: "r"
          }
        };
      }
    },
    hasBeenIdleFor: {
      enumerable: true,
      configurable: true,
      writable: true,
      value: function() {
        return false;
      }
    },
    openChatWindow: {
      enumerable: true,
      configurable: true,
      writable: true,
      value: function(toURL, callback) {
        let url = targetWindow.document.documentURIObject.resolve(toURL);
        openChatWindow(getChromeWindow(targetWindow), provider, url, callback);
      }
    },
    openPanel: {
      enumerable: true,
      configurable: true,
      writable: true,
      value: function(toURL, offset, callback) {
        let chromeWindow = getChromeWindow(targetWindow);
        if (!chromeWindow.SocialFlyout)
          return;
        let url = targetWindow.document.documentURIObject.resolve(toURL);
        let fullURL = ensureProviderOrigin(provider, url);
        if (!fullURL)
          return;
        chromeWindow.SocialFlyout.open(fullURL, offset, callback);
      }
    },
    closePanel: {
      enumerable: true,
      configurable: true,
      writable: true,
      value: function(toURL, offset, callback) {
        let chromeWindow = getChromeWindow(targetWindow);
        if (!chromeWindow.SocialFlyout || !chromeWindow.SocialFlyout.panel)
          return;
        chromeWindow.SocialFlyout.panel.hidePopup();
      }
    },
    getAttention: {
      enumerable: true,
      configurable: true,
      writable: true,
      value: function() {
        getChromeWindow(targetWindow).getAttention();
      }
    },
    isVisible: {
      enumerable: true,
      configurable: true,
      get: function() {
        return targetWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                           .getInterface(Ci.nsIWebNavigation)
                           .QueryInterface(Ci.nsIDocShell).isActive;
      }
    }
  };

  let contentObj = Cu.createObjectIn(targetWindow);
  Object.defineProperties(contentObj, mozSocialObj);
  Cu.makeObjectPropsNormal(contentObj);

  targetWindow.navigator.wrappedJSObject.__defineGetter__("mozSocial", function() {
    // We do this in a getter, so that we create these objects
    // only on demand (this is a potential concern, since
    // otherwise we might add one per iframe, and keep them
    // alive for as long as the window is alive).
    delete targetWindow.navigator.wrappedJSObject.mozSocial;
    return targetWindow.navigator.wrappedJSObject.mozSocial = contentObj;
  });

  targetWindow.addEventListener("unload", function () {
    // We want to close the port, but also want the target window to be
    // able to use the port during an unload event they setup - so we
    // set a timer which will fire after the unload events have all fired.
    schedule(function () { port.close(); });
  });
  // We allow window.close() to close the panel, so add an event handler for
  // this, then cancel the event (so the window itself doesn't die) and
  // close the panel instead.
  // However, this is typically affected by the dom.allow_scripts_to_close_windows
  // preference, but we can avoid that check by setting a flag on the window.
  let dwu = targetWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                        .getInterface(Ci.nsIDOMWindowUtils);
  dwu.allowScriptsToClose();

  targetWindow.addEventListener("DOMWindowClose", function _mozSocialDOMWindowClose(evt) {
    let elt = targetWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                .getInterface(Ci.nsIWebNavigation)
                .QueryInterface(Ci.nsIDocShell)
                .chromeEventHandler;
    while (elt) {
      if (elt.nodeName == "panel") {
        elt.hidePopup();
        break;
      } else if (elt.nodeName == "chatbox") {
        elt.close();
        break;
      }
      elt = elt.parentNode;
    }
    // preventDefault stops the default window.close() function being called,
    // which doesn't actually close anything but causes things to get into
    // a bad state (an internal 'closed' flag is set and debug builds start
    // asserting as the window is used.).
    // None of the windows we inject this API into are suitable for this
    // default close behaviour, so even if we took no action above, we avoid
    // the default close from doing anything.
    evt.preventDefault();
  }, true);
}

function schedule(callback) {
  Services.tm.mainThread.dispatch(callback, Ci.nsIThread.DISPATCH_NORMAL);
}

function getChromeWindow(contentWin) {
  return contentWin.QueryInterface(Ci.nsIInterfaceRequestor)
                   .getInterface(Ci.nsIWebNavigation)
                   .QueryInterface(Ci.nsIDocShellTreeItem)
                   .rootTreeItem
                   .QueryInterface(Ci.nsIInterfaceRequestor)
                   .getInterface(Ci.nsIDOMWindow);
}

function ensureProviderOrigin(provider, url) {
  // resolve partial URLs and check prePath matches
  let uri;
  let fullURL;
  try {
    fullURL = Services.io.newURI(provider.origin, null, null).resolve(url);
    uri = Services.io.newURI(fullURL, null, null);
  } catch (ex) {
    Cu.reportError("mozSocial: failed to resolve window URL: " + url + "; " + ex);
    return null;
  }

  if (provider.origin != uri.prePath) {
    Cu.reportError("mozSocial: unable to load new location, " +
                   provider.origin + " != " + uri.prePath);
    return null;
  }
  return fullURL;
}

function openChatWindow(chromeWindow, provider, url, callback, mode) {
  if (!chromeWindow.SocialChatBar)
    return;
  let fullURL = ensureProviderOrigin(provider, url);
  if (!fullURL)
    return;
  chromeWindow.SocialChatBar.openChat(provider, fullURL, callback, mode);
}
