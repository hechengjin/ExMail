/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This tests our handling of server timeouts during online move of
// an imap message. The move is done as an offline operation and then
// played back, to copy what the apps do.

Services.prefs.setIntPref("mailnews.tcptimeout", 2);

load("../../../resources/logHelper.js");
load("../../../resources/mailTestUtils.js");
load("../../../resources/asyncTestUtils.js");
load("../../../resources/alertTestUtils.js");
load("../../../resources/messageGenerator.js");

// IMAP pump
load("../../../resources/IMAPpump.js");

// Globals

setupIMAPPump();

var gGotAlert = false;
var gGotMsgAdded = false;

function alert(aDialogTitle, aText) {
  do_check_eq(aText.indexOf("Connection to server Mail for  timed out."), 0);
  gGotAlert = true;
  async_driver();
}

var CopyListener = {
  OnStartCopy: function() {},
  OnProgress: function(aProgress, aProgressMax) {},
  SetMessageKey: function(aMsgKey) {},
  GetMessageId: function() {},
  OnStopCopy: function(aStatus) {
    async_driver();
  }
};

// Definition of tests
var tests = [
  createTargetFolder,
  loadImapMessage,
  moveMessageToTargetFolder,
  waitForOfflinePlayback,
  updateTargetFolder,
  endTest
]

let gTargetFolder;
function createTargetFolder()
{
  gIMAPDaemon.copySleep = 5000;
  gIMAPIncomingServer.rootFolder.createSubfolder("targetFolder", null);
  yield false; 
  gTargetFolder = gIMAPIncomingServer.rootFolder.getChildNamed("targetFolder");
  do_check_true(gTargetFolder instanceof Ci.nsIMsgImapMailFolder);
  gTargetFolder.updateFolderWithListener(null, asyncUrlListener);
  yield false;
}  

// load and update a message in the imap fake server
function loadImapMessage()
{
  let messages = [];
  let gMessageGenerator = new MessageGenerator();
  messages = messages.concat(gMessageGenerator.makeMessage());

  let ioService = Cc["@mozilla.org/network/io-service;1"]
                  .getService(Ci.nsIIOService);
  let msgURI =
    ioService.newURI("data:text/plain;base64," +
                     btoa(messages[0].toMessageString()),
                     null, null);
  let imapInbox =  gIMAPDaemon.getMailbox("INBOX")
  gMessage = new imapMessage(msgURI.spec, imapInbox.uidnext++, []);
  gIMAPMailbox.addMessage(gMessage);
  gIMAPInbox.updateFolder(null);
  yield false;
  do_check_eq(1, gIMAPInbox.getTotalMessages(false));
  let msgHdr = firstMsgHdr(gIMAPInbox);
  do_check_true(msgHdr instanceof Ci.nsIMsgDBHdr);

  yield true;
}

// move the message to a diffent folder
function moveMessageToTargetFolder()
{
  let msgHdr = firstMsgHdr(gIMAPInbox);

  // Now move this message to the target folder.
  var messages = Cc["@mozilla.org/array;1"]
                   .createInstance(Ci.nsIMutableArray);
  messages.appendElement(msgHdr, false);
  let copyService = Cc["@mozilla.org/messenger/messagecopyservice;1"]
                      .getService(Ci.nsIMsgCopyService);
  // This should cause the move to be done as an offline imap operation
  // that's played back immediately.
  copyService.CopyMessages(gIMAPInbox, messages, gTargetFolder, true,
                           CopyListener, gDummyMsgWindow, true);
  yield false;
}

function waitForOfflinePlayback()
{
  // just wait for the alert about timed out connection.
  yield false;
  // then, wait for a second so we don't get our next url aborted.
  do_timeout(1000, async_driver);
  yield false;
}

function updateTargetFolder()
{
  gTargetFolder.updateFolderWithListener(null, asyncUrlListener);
  yield false;
}

// Cleanup
function endTest()
{
  do_check_true(gGotAlert);
  // Make sure neither source nor target folder have offline events.
  do_check_false(gIMAPInbox.getFlag(Ci.nsMsgFolderFlags.OfflineEvents));
  do_check_false(gTargetFolder.getFlag(Ci.nsMsgFolderFlags.OfflineEvents));

  // fake server does the copy, but then times out, so make sure the target
  // folder has only 1 message, not the multiple ones it would have if we
  // retried.
  do_check_eq(gTargetFolder.getTotalMessages(false), 1);
  teardownIMAPPump();
}

// listeners

mfnListener =
{
  folderAdded: function folderAdded(aFolder)
  {
    // we are only using async yield on the target folder add
    if (aFolder.name == "targetFolder")
      async_driver();
  },

  msgAdded: function msgAdded(aMsg)
  {
    if (!gGotMsgAdded)
      async_driver();
    gGotMsgAdded = true;
  },

};

function run_test()
{
  Services.prefs.setBoolPref("mail.server.default.autosync_offline_stores", false);
  // Add folder listeners that will capture async events
  const nsIMFNService = Ci.nsIMsgFolderNotificationService;
  let MFNService = Cc["@mozilla.org/messenger/msgnotificationservice;1"]
                      .getService(nsIMFNService);
  let flags =
        nsIMFNService.folderAdded |
        nsIMFNService.msgAdded;
  MFNService.addListener(mfnListener, flags);
  async_run_tests(tests);
}
