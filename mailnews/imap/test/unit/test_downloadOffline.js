/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * Test to ensure that downloadAllForOffline works correctly with imap folders
 * and returns success.
 */

load("../../../resources/logHelper.js");
load("../../../resources/mailTestUtils.js");
load("../../../resources/asyncTestUtils.js");
load("../../../resources/messageGenerator.js");
load("../../../resources/IMAPpump.js");

const gFileName = "bug460636";
const gMsgFile = do_get_file("../../../data/" + gFileName);

var tests = [
  setup,
  downloadAllForOffline,
  verifyDownloaded,
  teardown
];

function setup() {
  setupIMAPPump();

  let ioService = Cc["@mozilla.org/network/io-service;1"]
                    .getService(Ci.nsIIOService);
 /*
   * Ok, prelude done. Read the original message from disk
   * (through a file URI), and add it to the Inbox.
   */
  let msgfileuri = ioService.newFileURI(gMsgFile).QueryInterface(Ci.nsIFileURL);

  gIMAPMailbox.addMessage(new imapMessage(msgfileuri.spec, gIMAPMailbox.uidnext++, []));

  let messages = [];
  let gMessageGenerator = new MessageGenerator();
  messages = messages.concat(gMessageGenerator.makeMessage());
  gSynthMessage = messages[0];
  let dataUri = ioService.newURI("data:text/plain;base64," +
                   btoa(messages[0].toMessageString()),
                   null, null);
  let imapMsg = new imapMessage(dataUri.spec, gIMAPMailbox.uidnext++, []);
  imapMsg.setSize(5000);
  gIMAPMailbox.addMessage(imapMsg);
  
  // ...and download for offline use.
  gIMAPInbox.downloadAllForOffline(asyncUrlListener, null);
  yield false;
}

function downloadAllForOffline() {
  gIMAPInbox.downloadAllForOffline(asyncUrlListener, null);
  yield false;
}

function verifyDownloaded() {
  // verify that the message headers have the offline flag set.
  let msgEnumerator = gIMAPInbox.msgDatabase.EnumerateMessages();
  let offset = {};
  let size = {};
  while (msgEnumerator.hasMoreElements()) {
    let header = msgEnumerator.getNext();
    // Verify that each message has been downloaded and looks OK.
    if (header instanceof Components.interfaces.nsIMsgDBHdr &&
        (header.flags & Ci.nsMsgMessageFlags.Offline))
      gIMAPInbox.getOfflineFileStream(header.messageKey, offset, size).close();
    else
      do_throw("Message not downloaded for offline use");
  }
}

function teardown() {
  teardownIMAPPump();
}

function run_test() {
  async_run_tests(tests);
}
