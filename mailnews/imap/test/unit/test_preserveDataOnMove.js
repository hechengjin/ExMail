/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This tests that arbitrary message header properties are preserved
//  during online move of an imap message.

// async support 
load("../../../resources/logHelper.js");
load("../../../resources/mailTestUtils.js");
load("../../../resources/asyncTestUtils.js");

// IMAP pump
load("../../../resources/IMAPpump.js");

// Globals

const gMessage = "bugmail10"; // message file used as the test message

setupIMAPPump();

// Definition of tests
var tests = [
  createSubfolder,
  loadImapMessage,
  moveMessageToSubfolder,
  testPropertyOnMove,
  endTest
]

let gSubfolder;
function createSubfolder()
{
  gIMAPIncomingServer.rootFolder.createSubfolder("Subfolder", null);
  dl('wait for folderAdded notification');
  yield false; 
  gSubfolder = gIMAPIncomingServer.rootFolder.getChildNamed("Subfolder");
  do_check_true(gSubfolder instanceof Ci.nsIMsgImapMailFolder);
  gSubfolder.updateFolderWithListener(null, asyncUrlListener);
  dl('wait for OnStopRunningURL');
  yield false;
}  

// load and update a message in the imap fake server
function loadImapMessage()
{
  gIMAPMailbox.addMessage(new imapMessage(specForFileName(gMessage),
                          gIMAPMailbox.uidnext++, []));
  gIMAPInbox.updateFolder(null);
  dl('wait for msgAdded notification');
  yield false;
  do_check_eq(1, gIMAPInbox.getTotalMessages(false));
  let msgHdr = firstMsgHdr(gIMAPInbox);
  do_check_true(msgHdr instanceof Ci.nsIMsgDBHdr);

  // set an arbitrary property
  msgHdr.setStringProperty("testprop", "somevalue");
  yield true;
}

// move the message to a subfolder
function moveMessageToSubfolder()
{
  let msgHdr = firstMsgHdr(gIMAPInbox);

  // Now move this message to the subfolder.
  var messages = Cc["@mozilla.org/array;1"]
                   .createInstance(Ci.nsIMutableArray);
  messages.appendElement(msgHdr, false);
  /*
  void CopyMessages(in nsIMsgFolder srcFolder,
                    in nsIArray messages,
                    in nsIMsgFolder dstFolder,
                    in boolean isMove,
                    in nsIMsgCopyServiceListener listener,
                    in nsIMsgWindow msgWindow,
                    in boolean allowUndo);
  */

  let copyService = Cc["@mozilla.org/messenger/messagecopyservice;1"]
                      .getService(Ci.nsIMsgCopyService);
  copyService.CopyMessages(gIMAPInbox, messages, gSubfolder, true,
                           asyncCopyListener, null, false);
  dl('wait for OnStopCopy');
  yield false;
}

function testPropertyOnMove()
{
  gSubfolder.updateFolderWithListener(null, asyncUrlListener);
  dl('wait for msgAdded');
  yield false; // wait for msgAdded notification
  dl('wait for OnStopRunningURL');
  yield false; // wait for OnStopRunningUrl
  let msgHdr = firstMsgHdr(gSubfolder);
  do_check_eq(msgHdr.getStringProperty("testprop"), "somevalue");
  yield true;
}

// Cleanup
function endTest()
{
  teardownIMAPPump();
}

// listeners

mfnListener =
{
  folderAdded: function folderAdded(aFolder)
  {
    dl('folderAdded <' + aFolder.name + '>');
    // we are only using async yield on the Subfolder add
    if (aFolder.name == "Subfolder")
      async_driver();
  },

  msgAdded: function msgAdded(aMsg)
  {
    dl('msgAdded with subject <' + aMsg.subject + '>')
    async_driver();
  },

};

function run_test()
{
  Services.prefs.setBoolPref("mail.server.default.autosync_offline_stores", false);
  // Add folder listeners that will capture async events
  const nsIMFNService = Ci.nsIMsgFolderNotificationService;
  var MFNService = Cc["@mozilla.org/messenger/msgnotificationservice;1"]
                      .getService(nsIMFNService);
  let flags =
        nsIMFNService.folderAdded |
        nsIMFNService.msgAdded;
  MFNService.addListener(mfnListener, flags);
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

// shorthand output of a line of text
function dl(text) {
  dump(text + '\n');
}
