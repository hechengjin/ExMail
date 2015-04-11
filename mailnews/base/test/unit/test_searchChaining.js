/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test of chaining of search scopes in a search session. In particular,
//  Bug 541969 made us not search an imap folder if the search scope before it
// there was an empty local folder.

// main test

load("../../../resources/messageGenerator.js");

var gIMAPInbox;
var gIMAPDaemon, gServer, gIMAPIncomingServer;

function run_test()
{
  // Pull in the IMAP fake server code
  load("../../../imap/test/unit/head_server.js");

  loadLocalMailAccount();

  /*
   * Set up an IMAP server.
   */
  let IMAPDaemon = new imapDaemon();
  gServer = makeServer(IMAPDaemon, "");
  IMAPDaemon.createMailbox("secondFolder", {subscribed : true});
  gIMAPIncomingServer = createLocalIMAPServer();
  gIMAPIncomingServer.maximumConnectionsNumber = 1;

  // We need an identity so that updateFolder doesn't fail
  let acctMgr = Cc["@mozilla.org/messenger/account-manager;1"]
  .getService(Ci.nsIMsgAccountManager);
  let localAccount = acctMgr.createAccount();
  let identity = acctMgr.createIdentity();
  localAccount.addIdentity(identity);
  localAccount.defaultIdentity = identity;
  localAccount.incomingServer = gLocalIncomingServer;
  acctMgr.defaultAccount = localAccount;
  
  // Let's also have another account, using the same identity
  let imapAccount = acctMgr.createAccount();
  imapAccount.addIdentity(identity);
  imapAccount.defaultIdentity = identity;
  imapAccount.incomingServer = gIMAPIncomingServer;
  
  // pref tuning: one connection only, turn off notifications
  Services.prefs.setBoolPref("mail.biff.play_sound", false);
  Services.prefs.setBoolPref("mail.biff.show_alert", false);
  Services.prefs.setBoolPref("mail.biff.show_tray_icon", false);
  Services.prefs.setBoolPref("mail.biff.animate_dock_icon", false);

  // add a single message to the imap inbox.
  let messages = [];
  let gMessageGenerator = new MessageGenerator();
  messages = messages.concat(gMessageGenerator.makeMessage());
  gSynthMessage = messages[0];

  let ioService = Cc["@mozilla.org/network/io-service;1"]
                  .getService(Ci.nsIIOService);
  let msgURI =
    ioService.newURI("data:text/plain;base64," +
                     btoa(gSynthMessage.toMessageString()),
                     null, null);
  let imapInbox =  IMAPDaemon.getMailbox("INBOX")
  gMessage = new imapMessage(msgURI.spec, imapInbox.uidnext++, []);
  imapInbox.addMessage(gMessage);

  // Get the IMAP inbox...
  let rootFolder = gIMAPIncomingServer.rootFolder;
  gIMAPInbox = rootFolder.getFolderWithFlags(Ci.nsMsgFolderFlags.Inbox)
                         .QueryInterface(Ci.nsIMsgImapMailFolder);

  // update folder to download header.
  gIMAPInbox.updateFolderWithListener(null, UrlListener);
  do_test_pending();
}

var UrlListener = 
{
  OnStartRunningUrl: function(url) { },
  OnStopRunningUrl: function(url, rc)
  {
    // Check for ok status.
    do_check_eq(rc, 0);
    searchTest();
  }
};

function searchTest()
{
  // Get the IMAP inbox...
  emptyLocal1 = gLocalIncomingServer.rootFolder.createLocalSubfolder("empty 1");

  let searchSession = Cc["@mozilla.org/messenger/searchSession;1"]
                        .createInstance(Ci.nsIMsgSearchSession);

  let searchTerm = searchSession.createTerm();
  searchTerm.matchAll = true;
  searchSession.appendTerm(searchTerm);
  searchSession.addScopeTerm(Ci.nsMsgSearchScope.offlineMail, emptyLocal1);
  searchSession.addScopeTerm(Ci.nsMsgSearchScope.onlineMail, gIMAPInbox);
  searchSession.registerListener(searchListener);
  searchSession.search(null);
}

var numTotalMessages;

// nsIMsgSearchNotify implementation
var searchListener =
{ 
  onNewSearch: function() 
  {
    numTotalMessages = 0;
  },
  onSearchHit: function(dbHdr, folder)
  {
    numTotalMessages++;
  },
  onSearchDone: function(status)
  { 
    do_check_eq(numTotalMessages, 1);
    do_timeout(1000, endTest);
    return true;
  }
};

function endTest()
{
  // Cleanup, null out everything, close all cached connections and stop the
  // server
  gIMAPIncomingServer.closeCachedConnections();
  gServer.stop();

  var thread = gThreadManager.currentThread;
  while (thread.hasPendingEvents())
    thread.processNextEvent(true);

  do_test_finished();
}
