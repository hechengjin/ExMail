/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * Test to ensure that BCC gets added to message headers on IMAP download
 *
 * adapted from test_downloadOffline.js
 *
 * original author Kent James <kent@caspia.com>
 */

load("../../../resources/logHelper.js");
load("../../../resources/mailTestUtils.js");
load("../../../resources/asyncTestUtils.js");
load("../../../resources/IMAPpump.js");

const gFileName = "draft1";
const gMsgFile = do_get_file("../../../data/" + gFileName);

var tests = [
  setup,
  downloadAllForOffline,
  checkBccs,
  teardown
];

function setup() {
  setupIMAPPump();

  /*
   * Ok, prelude done. Read the original message from disk
   * (through a file URI), and add it to the Inbox.
   */
  let msgfileuri = Cc["@mozilla.org/network/io-service;1"]
                     .getService(Ci.nsIIOService)
                     .newFileURI(gMsgFile).QueryInterface(Ci.nsIFileURL);

  gIMAPMailbox.addMessage(new imapMessage(msgfileuri.spec,
                                          gIMAPMailbox.uidnext++, []));

  // ...and download for offline use.
  gIMAPInbox.downloadAllForOffline(asyncUrlListener, null);
  yield false;
}

function downloadAllForOffline() {
  gIMAPInbox.downloadAllForOffline(asyncUrlListener, null);
  yield false;
}

function checkBccs() {
  // locate the new message by enumerating through the database
  let enumerator = gIMAPInbox.msgDatabase.EnumerateMessages();
  while(enumerator.hasMoreElements()) {
    let hdr = enumerator.getNext().QueryInterface(Ci.nsIMsgDBHdr);
    do_check_true(hdr.bccList.indexOf("Another Person") >= 0);
    do_check_true(hdr.bccList.indexOf("<u1@example.com>") >= 0);
    do_check_false(hdr.bccList.indexOf("IDoNotExist") >=0);
  }
}

function teardown() {
  teardownIMAPPump();
}

function run_test() {
  async_run_tests(tests);
}

