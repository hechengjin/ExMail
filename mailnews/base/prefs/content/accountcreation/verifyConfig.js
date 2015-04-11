/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This checks a given config, by trying a real connection and login,
 * with username and password.
 *
 * TODO
 * - give specific errors, bug 555448
 * - return a working |Abortable| to allow cancel
 *
 * @param accountConfig {AccountConfig} The guessed account config.
 *    username, password, realname, emailaddress etc. are not filled out,
 *    but placeholders to be filled out via replaceVariables().
 * @param alter {boolean}
 *    Try other usernames and login schemes, until login works.
 *    Warning: Modifies |accountConfig|.
 *
 * This function is async.
 * @param successCallback function(accountConfig)
 *   Called when we could guess the config.
 *   For accountConfig, see below.
 * @param errorCallback function(ex)
 *   Called when we could guess not the config, either
 *   because we have not found anything or
 *   because there was an error (e.g. no network connection).
 *   The ex.message will contain a user-presentable message.
 */
function verifyConfig(config, alter, msgWindow, successCallback, errorCallback)
{
  ddump(debugObject(config, "config", 3));
  assert(config instanceof AccountConfig,
         "BUG: Arg 'config' needs to be an AccountConfig object");
  assert(typeof(alter) == "boolean");
  assert(typeof(successCallback) == "function");
  assert(typeof(errorCallback) == "function");

  var accountManager = Cc["@mozilla.org/messenger/account-manager;1"]
                       .getService(Ci.nsIMsgAccountManager);

  if (accountManager.findRealServer(config.incoming.username,
                                    config.incoming.hostname,
                                    sanitize.enum(config.incoming.type,
                                                  ["pop3", "imap", "nntp"]),
                                    config.incoming.port)) {
    errorCallback("Incoming server exists");
    return;
  }

  // incoming server
  var inServer =
    accountManager.createIncomingServer(config.incoming.username,
                                        config.incoming.hostname,
                                        sanitize.enum(config.incoming.type,
                                                    ["pop3", "imap", "nntp"]));
  inServer.port = config.incoming.port;
  inServer.password = config.incoming.password;
  if (config.incoming.socketType == 1) // plain
    inServer.socketType = Ci.nsMsgSocketType.plain;
  else if (config.incoming.socketType == 2) // SSL
    inServer.socketType = Ci.nsMsgSocketType.SSL;
  else if (config.incoming.socketType == 3) // STARTTLS
    inServer.socketType = Ci.nsMsgSocketType.alwaysSTARTTLS;
  inServer.authMethod = config.incoming.auth;

  try {
    if (inServer.password)
      verifyLogon(config, inServer, alter, msgWindow,
                  successCallback, errorCallback);
    else {
      // Avoid pref pollution, clear out server prefs.
      accountManager.removeIncomingServer(inServer, true);
      successCallback(config);
    }
  } catch (e) {
    ddump("ERROR: verify logon shouldn't have failed");
    errorCallback(e);
    throw(e);
  }
};

function verifyLogon(config, inServer, alter, msgWindow, successCallback,
                     errorCallback)
{
  // hack - save away the old callbacks.
  let saveCallbacks = msgWindow.notificationCallbacks;
  // set our own callbacks - this works because verifyLogon will
  // synchronously create the transport and use the notification callbacks.
  let listener = new urlListener(config, inServer, alter, msgWindow,
                                 successCallback, errorCallback);
  // our listener listens both for the url and cert errors.
  msgWindow.notificationCallbacks = listener;
  // try to work around bug where backend is clearing password.
  try {
    inServer.password = config.incoming.password;
    let uri = inServer.verifyLogon(listener, msgWindow);
    // clear msgWindow so url won't prompt for passwords.
    uri.QueryInterface(Ci.nsIMsgMailNewsUrl).msgWindow = null;
  }
  finally {
    // restore them
    msgWindow.notificationCallbacks = saveCallbacks;
  }
}

/**
 * The url listener also implements nsIBadCertListener2.  Its job is to prevent
 * "bad cert" security dialogs from being shown to the user.  Currently it puts
 * up the cert override dialog, though we'd like to give the user more detailed
 * information in the future.
 */

function urlListener(config, server, alter, msgWindow, successCallback,
                     errorCallback)
{
  this.mConfig = config;
  this.mServer = server;
  this.mAlter = alter;
  this.mSuccessCallback = successCallback;
  this.mErrorCallback = errorCallback;
  this.mMsgWindow = msgWindow;
  this.mCertError = false;
  this._log = Log4Moz.getConfiguredLogger("mail.wizard");
}
urlListener.prototype =
{
  OnStartRunningUrl: function(aUrl)
  {
    this._log.info("Starting to test username");
    this._log.info("  username=" + (this.mConfig.incoming.username !=
                          this.mConfig.identity.emailAddress) +
                          ", have savedUsername=" +
                          (this.mConfig.usernameSaved ? "true" : "false"));
    this._log.info("  authMethod=" + this.mServer.authMethod);
  },

  OnStopRunningUrl: function(aUrl, aExitCode)
  {
    this._log.info("Finished verifyConfig resulted in " + aExitCode);
    if (Components.isSuccessCode(aExitCode))
    {
      this._cleanup();
      this.mSuccessCallback(this.mConfig);
    }
    // Logon failed, and we aren't supposed to try other variations.
    else if (!this.mAlter)
    {
      this._cleanup();
      var errorMsg = getStringBundle(
          "chrome://messenger/locale/accountCreationModel.properties")
          .GetStringFromName("cannot_login.error");
      this.mErrorCallback(new Exception(errorMsg));
    }
    // Try other variations, unless there's a cert error, in which
    // case we'll see what the user chooses.
    else if (!this.mCertError)
    {
      this.tryNextLogon()
    }
  },

  tryNextLogon: function()
  {
    this._log.info("tryNextLogon()");
    this._log.info("  username=" + (this.mConfig.incoming.username !=
                          this.mConfig.identity.emailAddress) +
                          ", have savedUsername=" +
                          (this.mConfig.usernameSaved ? "true" : "false"));
    this._log.info("  authMethod=" + this.mServer.authMethod);
    // check if we tried full email address as username
    if (this.mConfig.incoming.username != this.mConfig.identity.emailAddress)
    {
      this._log.info("  Changing username to email address.");
      this.mConfig.usernameSaved = this.mConfig.incoming.username;
      this.mConfig.incoming.username = this.mConfig.identity.emailAddress;
      this.mConfig.outgoing.username = this.mConfig.identity.emailAddress;
      this.mServer.username = this.mConfig.incoming.username;
      this.mServer.password = this.mConfig.incoming.password;
      verifyLogon(this.mConfig, this.mServer, this.mAlter, this.mMsgWindow,
                  this.mSuccessCallback, this.mErrorCallback);
      return;
    }

    if (this.mConfig.usernameSaved)
    {
      this._log.info("  Re-setting username.");
      // If we tried the full email address as the username, then let's go
      // back to trying just the username before trying the other cases.
      this.mConfig.incoming.username = this.mConfig.usernameSaved;
      this.mConfig.outgoing.username = this.mConfig.usernameSaved;
      this.mConfig.usernameSaved = null;
      this.mServer.username = this.mConfig.incoming.username;
      this.mServer.password = this.mConfig.incoming.password;
    }

    // sec auth seems to have failed, and we've tried both
    // varieties of user name, sadly.
    // So fall back to non-secure auth, and
    // again try the user name and email address as username
    assert(this.mConfig.incoming.auth == this.mServer.authMethod);
    this._log.info("  Using SSL: " +
        (this.mServer.socketType == Ci.nsMsgSocketType.SSL ||
         this.mServer.socketType == Ci.nsMsgSocketType.alwaysSTARTTLS));
    if (this.mConfig.incoming.authAlternatives &&
        this.mConfig.incoming.authAlternatives.length)
        // We may be dropping back to insecure auth methods here,
        // which is not good. But then again, we already warned the user,
        // if it is a config without SSL.
    {
      this._log.info("  auth alternatives = " +
          this.mConfig.incoming.authAlternatives.join(","));
      this._log.info("  Decreasing auth.");
      this._log.info("  Have password: " +
                     (this.mServer.password ? "true" : "false"));
      let brokenAuth = this.mConfig.incoming.auth;
      // take the next best method (compare chooseBestAuthMethod() in guess)
      this.mConfig.incoming.auth =
          this.mConfig.incoming.authAlternatives.shift();
      this.mServer.authMethod = this.mConfig.incoming.auth;
      // Assume that SMTP server has same methods working as incoming.
      // Broken assumption, but we currently have no SMTP verification.
      // TODO implement real SMTP verification
      if (this.mConfig.outgoing.auth == brokenAuth &&
          this.mConfig.outgoing.authAlternatives.indexOf(
            this.mConfig.incoming.auth) != -1)
        this.mConfig.outgoing.auth = this.mConfig.incoming.auth;
      this._log.info("  outgoing auth: " + this.mConfig.outgoing.auth);
      verifyLogon(this.mConfig, this.mServer, this.mAlter, this.mMsgWindow,
                  this.mSuccessCallback, this.mErrorCallback);
      return;
    }

    // Tried all variations we can. Give up.
    this._log.info("Giving up.");
    this._cleanup();
    let errorMsg = getStringBundle(
        "chrome://messenger/locale/accountCreationModel.properties")
        .GetStringFromName("cannot_login.error");
    this.mErrorCallback(new Exception(errorMsg));
    return;
  },

  _cleanup : function()
  {
    try {
      // Avoid pref pollution, clear out server prefs.
      if (this.mServer) {
        Cc["@mozilla.org/messenger/account-manager;1"]
          .getService(Ci.nsIMsgAccountManager)
          .removeIncomingServer(this.mServer, true);
        this.mServer = null;
      }
    } catch (e) { this._log.error(e); }
  },

  // Suppress any certificate errors
  notifyCertProblem: function(socketInfo, status, targetSite) {
    if (!status)
      return true;

    this.mCertError = true;
    this._log.error("cert error");
    var me = this;
    setTimeout(function () {
      try {
        me.informUserOfCertError(socketInfo, targetSite);
      } catch (e)  { logException(e); }
    }, 0);
    return true;
  },

  informUserOfCertError : function(socketInfo, targetSite) {
    var params = {
      exceptionAdded : false,
      prefetchCert : true,
      location : targetSite,
    };
    window.openDialog("chrome://pippki/content/exceptionDialog.xul",
                      "","chrome,centerscreen,modal", params);
    this._log.info("cert exception dialog closed");
    this._log.info("cert exceptionAdded = " + params.exceptionAdded);
    if (!params.exceptionAdded) {
      this._cleanup();
      let errorMsg = getStringBundle(
          "chrome://messenger/locale/accountCreationModel.properties")
          .GetStringFromName("cannot_login.error");
      this.mErrorCallback(new Exception(errorMsg));
    }
    else {
      // Retry the logon now that we've added the cert exception.
      verifyLogon(this.mConfig, this.mServer, this.mAlter, this.mMsgWindow,
                  this.mSuccessCallback, this.mErrorCallback);
    }
  },

  // nsIInterfaceRequestor
  getInterface: function(iid) {
    return this.QueryInterface(iid);
  },

  // nsISupports
  QueryInterface: function(iid) {
    if (!iid.equals(Components.interfaces.nsIBadCertListener2) &&
        !iid.equals(Components.interfaces.nsIInterfaceRequestor) &&
        !iid.equals(Components.interfaces.nsIUrlListener) &&
        !iid.equals(Components.interfaces.nsISupports))
      throw Components.results.NS_ERROR_NO_INTERFACE;

    return this;
  }
}
