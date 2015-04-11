/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * Test to ensure that changes made while offline are played back when we
 * go back online.
 */

const copyService = Cc["@mozilla.org/messenger/messagecopyservice;1"]
                      .getService(Ci.nsIMsgCopyService);

const nsIIOService = Cc["@mozilla.org/network/io-service;1"]
                     .getService(Ci.nsIIOService);

load("../../../resources/logHelper.js");
load("../../../resources/mailTestUtils.js");
load("../../../resources/asyncTestUtils.js");
load("../../../resources/messageGenerator.js");
load("../../../resources/IMAPpump.js");

var gSecondFolder, gThirdFolder;
var gSynthMessage1, gSynthMessage2;
// the message id of bugmail10
const gMsgId1 = "200806061706.m56H6RWT004933@mrapp54.mozilla.org";
var gOfflineManager;

var tests = [
  setup,
  function prepareToGoOffline() {
    let rootFolder = gIMAPIncomingServer.rootFolder;
    gSecondFolder = rootFolder.getChildNamed("secondFolder")
                      .QueryInterface(Ci.nsIMsgImapMailFolder);
    gThirdFolder =  rootFolder.getChildNamed("thirdFolder")
                      .QueryInterface(Ci.nsIMsgImapMailFolder);
    gIMAPIncomingServer.closeCachedConnections();
    var thread = gThreadManager.currentThread;
    while (thread.hasPendingEvents())
      thread.processNextEvent(true);

    dump("wait 2 seconds, then go offline\n");
    do_timeout(2000, async_driver);
    yield false;
  },
  function doOfflineOps() {
    gIMAPServer.stop();
    nsIIOService.offline = true;

    // Flag the two messages, and then copy them to different folders. Since
    // we're offline, these operations are synchronous.
    let msgHdr1 = gIMAPInbox.msgDatabase.getMsgHdrForMessageID(gSynthMessage1.messageId);
    let msgHdr2 = gIMAPInbox.msgDatabase.getMsgHdrForMessageID(gSynthMessage2.messageId);
    let headers1 = Cc["@mozilla.org/array;1"]
                     .createInstance(Ci.nsIMutableArray);
    let headers2 = Cc["@mozilla.org/array;1"]
                     .createInstance(Ci.nsIMutableArray);
    headers1.appendElement(msgHdr1, false);
    headers2.appendElement(msgHdr2, false);
    msgHdr1.folder.markMessagesFlagged(headers1, true);
    msgHdr2.folder.markMessagesFlagged(headers2, true);
    copyService.CopyMessages(gIMAPInbox, headers1, gSecondFolder, true, null,
                             null, true);
    copyService.CopyMessages(gIMAPInbox, headers2, gThirdFolder, true, null,
                             null, true);
    var file = do_get_file("../../../data/bugmail10");
    copyService.CopyFileMessage(file, gIMAPInbox, null, false, 0,
                                "", asyncCopyListener, null);
    yield false;
  },
  function goOnline() {
    gOfflineManager = Cc["@mozilla.org/messenger/offline-manager;1"]
                           .getService(Ci.nsIMsgOfflineManager);
    gIMAPDaemon.closing = false;
    nsIIOService.offline = false;

    gIMAPServer.start(IMAP_PORT);
    gOfflineManager.goOnline(false, true, null);
    do_timeout(2000, async_driver);
    yield false;
  },
  function updateSecondFolder() {
    if (gOfflineManager.inProgress)
      do_timeout(2000, updateSecondFolder);
    gSecondFolder.updateFolderWithListener(null, asyncUrlListener);
    yield false;
  },
  function updateThirdFolder() {
    gThirdFolder.updateFolderWithListener(null, asyncUrlListener);
    yield false;
  },
  function updateInbox() {
    gIMAPInbox.updateFolderWithListener(null, asyncUrlListener);
    yield false;
  },
  function checkDone() {
    let msgHdr1 = gSecondFolder.msgDatabase.getMsgHdrForMessageID(gSynthMessage1.messageId);
    let msgHdr2 = gThirdFolder.msgDatabase.getMsgHdrForMessageID(gSynthMessage2.messageId);
    let msgHdr3 = gIMAPInbox.msgDatabase.getMsgHdrForMessageID(gMsgId1);
    do_check_neq(msgHdr1, null);
    do_check_neq(msgHdr2, null);
    do_check_neq(msgHdr3, null);
  },
  teardown
];

function setup() {
  setupIMAPPump();

  /*
   * Set up an IMAP server.
   */
  gIMAPDaemon.createMailbox("secondFolder", {subscribed : true});
  gIMAPDaemon.createMailbox("thirdFolder", {subscribed : true});
  Services.prefs.setBoolPref("mail.server.default.autosync_offline_stores", false);
  // Don't prompt about offline download when going offline
  Services.prefs.setIntPref("offline.download.download_messages", 2);

  // make a couple messges
  let messages = [];
  let gMessageGenerator = new MessageGenerator();
  messages = messages.concat(gMessageGenerator.makeMessage());
  messages = messages.concat(gMessageGenerator.makeMessage());
  gSynthMessage1 = messages[0];
  gSynthMessage2 = messages[1];

  let msgURI =
    nsIIOService.newURI("data:text/plain;base64," +
                     btoa(messages[0].toMessageString()),
                     null, null);
  let imapInbox =  gIMAPDaemon.getMailbox("INBOX")
  let message = new imapMessage(msgURI.spec, imapInbox.uidnext++, ["\\Seen"]);
  imapInbox.addMessage(message);
  msgURI =
    nsIIOService.newURI("data:text/plain;base64," +
                     btoa(messages[1].toMessageString()),
                     null, null);
  message = new imapMessage(msgURI.spec, imapInbox.uidnext++, ["\\Seen"]);
  imapInbox.addMessage(message);

  // update folder to download header.
  gIMAPInbox.updateFolderWithListener(null, asyncUrlListener);
  yield false;
}

function teardown() {
  teardownIMAPPump();
}

function run_test() {
  async_run_tests(tests);
}
