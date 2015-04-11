/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Test to ensure that imap customCommandResult function works properly
 * Bug 778246
 */

// async support
load("../../../resources/logHelper.js");
load("../../../resources/mailTestUtils.js");
load("../../../resources/asyncTestUtils.js");

// IMAP pump
load("../../../resources/IMAPpump.js");

Components.utils.import("resource://gre/modules/Services.jsm");

// Globals

// Messages to load must have CRLF line endings, that is Windows style
const gMessageFileName = "bugmail10"; // message file used as the test message
var gMessage, gExpectedLength;

var gCustomList = ['Custom1', 'Custom2', 'Custom3'];

var gMsgWindow = Cc["@mozilla.org/messenger/msgwindow;1"]
  .createInstance(Ci.nsIMsgWindow);

setupIMAPPump("CUSTOM1");

// Definition of tests
var tests = [
  loadImapMessage,
  testStoreCustomList,
  testStoreMinusCustomList,
  testStorePlusCustomList,
  endTest
]

// load and update a message in the imap fake server
function loadImapMessage()
{
  gMessage = new imapMessage(specForFileName(gMessageFileName),
    gIMAPMailbox.uidnext++, []);
  gMessage.xCustomList = [];
  gIMAPMailbox.addMessage(gMessage);
  gIMAPInbox.updateFolderWithListener(null, asyncUrlListener);
  yield false;
}

function testStoreCustomList()
{
  let msgHdr = firstMsgHdr(gIMAPInbox);
  gExpectedLength = gCustomList.length;
  let uri = gIMAPInbox.issueCommandOnMsgs("STORE", msgHdr.messageKey +
    " X-CUSTOM-LIST (" + gCustomList.join(" ") + ")", gMsgWindow);
  uri.QueryInterface(Ci.nsIMsgMailNewsUrl);
  uri.RegisterListener(storeCustomListSetListener);
  yield false;
}

// listens for response from customCommandResult request for X-CUSTOM-LIST
var storeCustomListSetListener = {
  OnStartRunningUrl: function (aUrl) {},

  OnStopRunningUrl: function (aUrl, aExitCode) {
    aUrl.QueryInterface(Ci.nsIImapUrl);
    do_check_eq(aUrl.customCommandResult,
      "(" + gMessage.xCustomList.join(" ") + ")");
    do_check_eq(gMessage.xCustomList.length, gExpectedLength);
    async_driver();
  }
};

function testStoreMinusCustomList()
{
  let msgHdr = firstMsgHdr(gIMAPInbox);
  gExpectedLength--;
  let uri = gIMAPInbox.issueCommandOnMsgs("STORE", msgHdr.messageKey +
    " -X-CUSTOM-LIST (" + gCustomList[0] + ")", gMsgWindow);
  uri.QueryInterface(Ci.nsIMsgMailNewsUrl);
  uri.RegisterListener(storeCustomListRemovedListener);
  yield false;
}

// listens for response from customCommandResult request for X-CUSTOM-LIST
var storeCustomListRemovedListener = {
  OnStartRunningUrl: function (aUrl) {},

  OnStopRunningUrl: function (aUrl, aExitCode) {
    aUrl.QueryInterface(Ci.nsIImapUrl);
    do_check_eq(aUrl.customCommandResult,
      "(" + gMessage.xCustomList.join(" ") + ")");
    do_check_eq(gMessage.xCustomList.length, gExpectedLength);
    async_driver();
  }
};

function testStorePlusCustomList()
{
  let msgHdr = firstMsgHdr(gIMAPInbox);
  gExpectedLength++;
  let uri = gIMAPInbox.issueCommandOnMsgs("STORE", msgHdr.messageKey +
    ' +X-CUSTOM-LIST ("Custom4")', gMsgWindow);
  uri.QueryInterface(Ci.nsIMsgMailNewsUrl);
  uri.RegisterListener(storeCustomListAddedListener);
  yield false;
}

// listens for response from customCommandResult request for X-CUSTOM-LIST
var storeCustomListAddedListener = {
  OnStartRunningUrl: function (aUrl) {},

  OnStopRunningUrl: function (aUrl, aExitCode) {
    aUrl.QueryInterface(Ci.nsIImapUrl);
    do_check_eq(aUrl.customCommandResult,
      "(" + gMessage.xCustomList.join(" ") + ")");
    do_check_eq(gMessage.xCustomList.length, gExpectedLength);
    async_driver();
  }
};


// Cleanup at end
function endTest()
{
  teardownIMAPPump();
}

function run_test()
{
  Services.prefs.setBoolPref("mail.server.server1.autosync_offline_stores", false);
  async_run_tests(tests);
}

/*
 * helper functions
 */

// given a test file, return the file uri spec
function specForFileName(aFileName)
{
  let file = do_get_file("../../../data/" + aFileName);
  let msgfileuri = Cc["@mozilla.org/network/io-service;1"]
                     .getService(Ci.nsIIOService)
                     .newFileURI(file)
                     .QueryInterface(Ci.nsIFileURL);
  return msgfileuri.spec;
}