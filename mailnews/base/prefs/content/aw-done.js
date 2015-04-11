/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var gPrefsBundle;

function donePageInit() {
    var pageData = parent.GetPageData();
    var currentAccountData = gCurrentAccountData;

    if ("testingIspServices" in this) {
      if (testingIspServices()) {
        if ("setOtherISPServices" in this) {
          setOtherISPServices();
        }

        if (currentAccountData && currentAccountData.useOverlayPanels && currentAccountData.createNewAccount) {
          var backButton = document.documentElement.getButton("back");
          backButton.setAttribute("disabled", true);

          var cancelButton = document.documentElement.getButton("cancel");
          cancelButton.setAttribute("disabled", true);

          setPageData(pageData, "identity", "email", gEmailAddress);
          setPageData(pageData, "identity", "fullName", gUserFullName);
          setPageData(pageData, "login", "username", gScreenName);
        }
      }
    }
   
    gPrefsBundle = document.getElementById("bundle_prefs");
    var showMailServerDetails = true; 

    if (currentAccountData) {
        // find out if we need to hide server details
        showMailServerDetails = currentAccountData.showServerDetailsOnWizardSummary; 
        // Change the username field description to email field label in aw-identity
        setUserNameDescField(currentAccountData.emailIDFieldTitle);
    }

    // Find out if we need to hide details for incoming servers
    var hideIncoming = (gCurrentAccountData && gCurrentAccountData.wizardHideIncoming);

    var email = "";
    if (pageData.identity && pageData.identity.email) {
        // fixup the email
        email = pageData.identity.email.value;
        if (email.split('@').length < 2 && 
                     currentAccountData && 
                     currentAccountData.domain)
            email += "@" + currentAccountData.domain;
    }
    setDivTextFromForm("identity.email", email);
    
    var userName="";
    if (pageData.login && pageData.login.username)
        userName = pageData.login.username.value;
    if (!userName && email)
        userName = getUsernameFromEmail(email, currentAccountData && 
                                        currentAccountData.incomingServerUserNameRequiresDomain);
              
    // Hide the "username" field if we don't want to show information
    // on the incoming server.
    setDivTextFromForm("server.username", hideIncoming ? null : userName);

    var smtpUserName="";
    if (pageData.login && pageData.login.smtpusername)
        smtpUserName = pageData.login.smtpusername.value;
    if (!smtpUserName && email)
        smtpUserName = getUsernameFromEmail(email, currentAccountData && 
                                            currentAccountData.smtpUserNameRequiresDomain);
    setDivTextFromForm("smtpServer.username", smtpUserName);

    if (pageData.accname && pageData.accname.prettyName) {
      // If currentAccountData has a pretty name (e.g. news link or from
      // isp data) only set the user name with the pretty name if the
      // pretty name originally came from ispdata
      if ((currentAccountData && 
           currentAccountData.incomingServer.prettyName) &&
           (pageData.ispdata && 
            pageData.ispdata.supplied &&
            pageData.ispdata.supplied.value))
      {
        // Get the polished account name - if the user has modified the
        // account name then use that, otherwise use the userName.
        pageData.accname.prettyName.value =
          gPrefsBundle.getFormattedString("accountName",
                         [currentAccountData.incomingServer.prettyName,
                          (pageData.accname.userset && pageData.accname.userset.value) ? pageData.accname.prettyName.value : userName]);
      }
      // else just use the default supplied name

      setDivTextFromForm("account.name", pageData.accname.prettyName.value);
    } else {
      setDivTextFromForm("account.name", "");
    }

    // Show mail servers (incoming&outgoing) details
    // based on current account data. ISP can set 
    // rdf value of literal showServerDetailsOnWizardSummary
    // to false to hide server details
    if (showMailServerDetails && !serverIsNntp(pageData)) {
        var incomingServerName="";
        if (pageData.server && pageData.server.hostname) {
            incomingServerName = pageData.server.hostname.value;
            if (!incomingServerName && 
                 currentAccountData && 
                 currentAccountData.incomingServer.hostname)
                incomingServerName = currentAccountData.incomingServer.hostName;
        }
        // Hide the incoming server name field if the user specified
        // wizardHideIncoming in the ISP defaults file
        setDivTextFromForm("server.name", hideIncoming ? null : incomingServerName);
        setDivTextFromForm("server.port", pageData.server.port.value);
        var incomingServerType="";
        if (pageData.server && pageData.server.servertype) {
            incomingServerType = pageData.server.servertype.value;
            if (!incomingServerType && 
                 currentAccountData && 
                 currentAccountData.incomingServer.type)
                incomingServerType = currentAccountData.incomingServer.type;
        }
        setDivTextFromForm("server.type", incomingServerType.toUpperCase());

        var smtpServerName="";
        if (pageData.server && pageData.server.smtphostname) {
          var smtpServer = parent.smtpService.defaultServer;
          smtpServerName = pageData.server.smtphostname.value;
          if (!smtpServerName && smtpServer && smtpServer.hostname)
              smtpServerName = smtpServer.hostname;
        }
        setDivTextFromForm("smtpServer.name", smtpServerName);
    }
    else {
        setDivTextFromForm("server.name", null);
        setDivTextFromForm("server.type", null);
        setDivTextFromForm("smtpServer.name", null);
    }

    if (serverIsNntp(pageData)) {
        var newsServerName="";
        if (pageData.newsserver && pageData.newsserver.hostname)
            newsServerName = pageData.newsserver.hostname.value;
        if (newsServerName) {
            // No need to show username for news account
            setDivTextFromForm("server.username", null);
        }
        setDivTextFromForm("newsServer.name", newsServerName);
    }
    else {
        setDivTextFromForm("newsServer.name", null);
    }

    var isPop = false;
    if (pageData.server && pageData.server.servertype) {
      isPop = (pageData.server.servertype.value == "pop3");
    }

    hideShowDownloadMsgsUI(isPop);
}

function hideShowDownloadMsgsUI(isPop)
{
  // only show the "download messages now" UI
  // if this is a pop account, we are online, and this was opened
  // from the 3 pane
  var downloadMsgs = document.getElementById("downloadMsgs");
  if (isPop) {
    var ioService = Components.classes["@mozilla.org/network/io-service;1"].getService(Components.interfaces.nsIIOService);
    if (!ioService.offline) {
      if (window.opener.location.href == "chrome://messenger/content/messenger.xul") {
        downloadMsgs.hidden = false;
        return;
      }
    }
  }
 
  // else hide it
  downloadMsgs.hidden = true;
}

function setDivTextFromForm(divid, value) {

    // collapse the row if the div has no value
    var div = document.getElementById(divid);
    if (!value) {
        div.setAttribute("collapsed","true");
        return;
    }
    else {
        div.removeAttribute("collapsed");
    }

    // otherwise fill in the .text element
    div = document.getElementById(divid+".text");
    if (!div) return;

    // set the value
    div.setAttribute("value", value);
}

function setUserNameDescField(name)
{
   if (name) {
       var userNameField = document.getElementById("server.username.label");
       userNameField.setAttribute("value", name);
   }
}
