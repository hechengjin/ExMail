// This file tests undoing of an imap delete to the trash.
// There are three main cases:
// 1. Normal undo
// 2. Undo after the source folder has been compacted.
// 2.1 Same, but the server doesn't support COPYUID (GMail case)
//
// Original Author: David Bienvenu <bienvenu@nventure.com>


load("../../../resources/logHelper.js");
load("../../../resources/mailTestUtils.js");
load("../../../resources/asyncTestUtils.js");
load("../../../resources/IMAPpump.js");

var gRootFolder;
var gLastKey;
var gMessages = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);
var gCopyService = Cc["@mozilla.org/messenger/messagecopyservice;1"]
                .getService(Ci.nsIMsgCopyService);
var gMsgWindow;

Components.utils.import("resource:///modules/iteratorUtils.jsm");

const gMsgFile1 = do_get_file("../../../data/bugmail10");
const gMsgFile2 = do_get_file("../../../data/bugmail11");
const gMsgFile3 = do_get_file("../../../data/draft1");
const gMsgFile4 = do_get_file("../../../data/bugmail7");
const gMsgFile5 = do_get_file("../../../data/bugmail6");

// Copied straight from the example files
const gMsgId1 = "200806061706.m56H6RWT004933@mrapp54.mozilla.org";
const gMsgId2 = "200804111417.m3BEHTk4030129@mrapp51.mozilla.org";
const gMsgId3 = "4849BF7B.2030800@example.com";
const gMsgId4 = "bugmail7.m47LtAEf007542@mrapp51.mozilla.org";
const gMsgId5 = "bugmail6.m47LtAEf007542@mrapp51.mozilla.org";



// Adds some messages directly to a mailbox (eg new mail)
function addMessagesToServer(messages, mailbox, localFolder)
{
  let ioService = Cc["@mozilla.org/network/io-service;1"]
                    .getService(Ci.nsIIOService);

  // For every message we have, we need to convert it to a file:/// URI
  messages.forEach(function (message)
  {
    message.spec = ioService.newFileURI(message.file)
                     .QueryInterface(Ci.nsIFileURL).spec;
  });

  // Create the imapMessages and store them on the mailbox
  messages.forEach(function (message)
  {
    mailbox.addMessage(new imapMessage(message.spec, mailbox.uidnext++, []));
  });
}

function alertListener() {}

alertListener.prototype = {
  reset: function () {
  },

  onAlert: function (aMessage, aMsgWindow) {
    dump("got alert " + aMessage + "\n");
    do_throw ('TEST FAILED ' + aMessage);
  }
};

var tests = [
  setup,
  function updateFolder() {
    gIMAPInbox.updateFolderWithListener(null, asyncUrlListener);
    yield false;
  },
  function deleteMessage() {
    let msgToDelete = gIMAPInbox.msgDatabase.getMsgHdrForMessageID(gMsgId1);
    gMessages.appendElement(msgToDelete, false);
    gIMAPInbox.deleteMessages(gMessages, gMsgWindow, false, true, asyncCopyListener, true);
    yield false;
  },
  function expunge() {
    gIMAPInbox.expunge(asyncUrlListener, gMsgWindow);
    yield false;

    // Ensure that the message has been surely deleted.
    do_check_eq(gIMAPInbox.msgDatabase.dBFolderInfo.numMessages, 3);
  },
  function undoDelete() {
    gMsgWindow.transactionManager.undoTransaction();
    // after undo, we select the trash and then the inbox, so that we sync
    // up with the server, and clear out the effects of having done the
    // delete offline.
    let trash = gRootFolder.getChildNamed("Trash");
    trash.QueryInterface(Ci.nsIMsgImapMailFolder)
         .updateFolderWithListener(null, asyncUrlListener);
    yield false;
  },
  function goBackToInbox() {
    gIMAPInbox.updateFolderWithListener(gMsgWindow, asyncUrlListener);
    yield false;
  },
  function verifyFolders() {
    let msgRestored = gIMAPInbox.msgDatabase.getMsgHdrForMessageID(gMsgId1);
    do_check_neq(msgRestored, null);
    do_check_eq(gIMAPInbox.msgDatabase.dBFolderInfo.numMessages, 4);
  },
  teardown
];

function setup() {
  setupIMAPPump();

  var mailSession = Cc["@mozilla.org/messenger/services/session;1"]
    .getService(Ci.nsIMsgMailSession);

  var listener1 = new alertListener();

  mailSession.addUserFeedbackListener(listener1);

  Services.prefs.setBoolPref("mail.server.server1.autosync_offline_stores", false);
  Services.prefs.setBoolPref("mail.server.server1.offline_download", false);

  gMsgWindow = Cc["@mozilla.org/messenger/msgwindow;1"]
                  .createInstance(Components.interfaces.nsIMsgWindow);

  gRootFolder = gIMAPIncomingServer.rootFolder;
  // these hacks are required because we've created the inbox before
  // running initial folder discovery, and adding the folder bails
  // out before we set it as verified online, so we bail out, and
  // then remove the INBOX folder since it's not verified.
  gIMAPInbox.hierarchyDelimiter = '/';
  gIMAPInbox.verifiedAsOnlineFolder = true;


  // Add a couple of messages to the INBOX
  // this is synchronous, afaik
  addMessagesToServer([{file: gMsgFile1, messageId: gMsgId1},
                       {file: gMsgFile4, messageId: gMsgId4},
                       {file: gMsgFile5, messageId: gMsgId5},
                       {file: gMsgFile2, messageId: gMsgId2}],
                      gIMAPMailbox, gIMAPInbox);
}

asyncUrlListener.callback = function(aUrl, aExitCode) {
  do_check_eq(aExitCode, 0);
};

function teardown() {
  // Cleanup, null out everything, close all cached connections and stop the
  // server
  gMessages.clear();
  gMsgWindow.closeWindow();
  gMsgWindow = null;
  gRootFolder = null;
  teardownIMAPPump();
}

function run_test() {
  async_run_tests(tests);
}
