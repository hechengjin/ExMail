/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * Test to ensure that imap flag changes made from a different profile/machine
 * are stored in db.
 */

load("../../../resources/logHelper.js");
load("../../../resources/mailTestUtils.js");
load("../../../resources/asyncTestUtils.js");
load("../../../resources/messageGenerator.js");
load("../../../resources/IMAPpump.js");

var gMessage;
var gSecondFolder;
var gSynthMessage;

var tests = [
  setup,
  function switchAwayFromInbox() {
    let rootFolder = gIMAPIncomingServer.rootFolder;
    gSecondFolder =  rootFolder.getChildNamed("secondFolder")
                           .QueryInterface(Ci.nsIMsgImapMailFolder);

    // Selecting the second folder will close the cached connection
    // on the inbox because fake server only supports one connection at a time.
    //  Then, we can poke at the message on the imap server directly, which
    // simulates the user changing the message from a different machine,
    // and Thunderbird discovering the change when it does a flag sync 
    // upon reselecting the Inbox.
    gSecondFolder.updateFolderWithListener(null, asyncUrlListener);
    yield false;
  },
  function simulateForwardFlagSet() {
    gMessage.setFlag("$Forwarded");
    gIMAPInbox.updateFolderWithListener(null, asyncUrlListener);
    yield false;
  },
  function checkForwardedFlagSet() {
    let msgHdr = gIMAPInbox.msgDatabase.getMsgHdrForMessageID(gSynthMessage.messageId);
    do_check_eq(msgHdr.flags & Ci.nsMsgMessageFlags.Forwarded,
      Ci.nsMsgMessageFlags.Forwarded);
    gSecondFolder.updateFolderWithListener(null, asyncUrlListener);
    yield false;
  },
  function clearForwardedFlag() {
    gMessage.clearFlag("$Forwarded");
    gIMAPInbox.updateFolderWithListener(null, asyncUrlListener);
    yield false;
  },
  function checkForwardedFlagCleared() {
    let msgHdr = gIMAPInbox.msgDatabase.getMsgHdrForMessageID(gSynthMessage.messageId);
    do_check_eq(msgHdr.flags & Ci.nsMsgMessageFlags.Forwarded, 0);
    gSecondFolder.updateFolderWithListener(null, asyncUrlListener);
    yield false;
  },
  function setSeenFlag() {
    gMessage.setFlag("\\Seen");
    gIMAPInbox.updateFolderWithListener(null, asyncUrlListener);
    yield false;
  },
  function checkSeenFlagSet() {
    let msgHdr = gIMAPInbox.msgDatabase.getMsgHdrForMessageID(gSynthMessage.messageId);
    do_check_eq(msgHdr.flags & Ci.nsMsgMessageFlags.Read,
                Ci.nsMsgMessageFlags.Read);
    gSecondFolder.updateFolderWithListener(null, asyncUrlListener);
    yield false;
  },
  function simulateRepliedFlagSet() {
    gMessage.setFlag("\\Answered");
    gIMAPInbox.updateFolderWithListener(null, asyncUrlListener);
    yield false;
  },
  function checkRepliedFlagSet() {
    let msgHdr = gIMAPInbox.msgDatabase.getMsgHdrForMessageID(gSynthMessage.messageId);
    do_check_eq(msgHdr.flags & Ci.nsMsgMessageFlags.Replied,
      Ci.nsMsgMessageFlags.Replied);
    gSecondFolder.updateFolderWithListener(null, asyncUrlListener);
    yield false;
  },
  function simulateTagAdded() {
    gMessage.setFlag("randomtag");
    gIMAPInbox.updateFolderWithListener(null, asyncUrlListener);
    yield false;
  },
  function checkTagSet() {
    let msgHdr = gIMAPInbox.msgDatabase.getMsgHdrForMessageID(gSynthMessage.messageId);
    let keywords = msgHdr.getStringProperty("keywords");
    do_check_true(keywords.indexOf("randomtag") != -1);
    gSecondFolder.updateFolderWithListener(null, asyncUrlListener);
    yield false;
  },
  function clearTag() {
    gMessage.clearFlag("randomtag");
    gIMAPInbox.updateFolderWithListener(null, asyncUrlListener);
    yield false;
  },
  function checkTagCleared() {
    let msgHdr = gIMAPInbox.msgDatabase.getMsgHdrForMessageID(gSynthMessage.messageId);
    let keywords = msgHdr.getStringProperty("keywords");
    do_check_eq(keywords.indexOf("randomtag"), -1);
  },
  teardown
];

function setup() {
  Services.prefs.setBoolPref("mail.server.default.autosync_offline_stores", false);

  setupIMAPPump();

  gIMAPDaemon.createMailbox("secondFolder", {subscribed : true});

  // build up a diverse list of messages
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
  gMessage = new imapMessage(msgURI.spec, gIMAPMailbox.uidnext++, []);
  gIMAPMailbox.addMessage(gMessage);

  // update folder to download header.
  gIMAPInbox.updateFolderWithListener(null, asyncUrlListener);
  yield false;
}

asyncUrlListener.callback = function(aUrl, aExitCode) {
  do_check_eq(aExitCode, 0);
};

function teardown() {
  teardownIMAPPump();
}

function run_test() {
  async_run_tests(tests);
}
