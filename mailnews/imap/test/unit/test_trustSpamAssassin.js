/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 
/*
 * This file tests recognizing a message as junk due to
 *  SpamAssassin headers, and marking that as good
 *  without having the message return to the junk folder,
 *  as discussed in bug 540385.
 *
 * adapted from test_filterNeedsBody.js
 */

// async support 
load("../../../resources/logHelper.js");
load("../../../resources/mailTestUtils.js");
load("../../../resources/asyncTestUtils.js");

// IMAP pump
load("../../../resources/IMAPpump.js");

// Globals
const gMessage = "SpamAssassinYes"; // message file used as the test message

setupIMAPPump();

// Definition of tests

var tests = [
  createJunkFolder,
  loadImapMessage,
  testMessageInJunk,
  markMessageAsGood,
  updateFoldersAndCheck,
  endTest,
]

let gJunkFolder;
function createJunkFolder()
{
  gIMAPIncomingServer.rootFolder.createSubfolder("Junk", null);
  dl('wait for folderAdded');
  yield false;
  gJunkFolder = gIMAPIncomingServer.rootFolder.getChildNamed("Junk");
  do_check_true(gJunkFolder instanceof Ci.nsIMsgImapMailFolder);
  gJunkFolder.updateFolderWithListener(null, asyncUrlListener);
  dl('wait for OnStopRunningURL');
  yield false;
}

/*
 * Load and update a message in the imap fake server, should move
 *  SpamAssassin-marked junk message to junk folder
 */
function loadImapMessage()
{
  gIMAPMailbox.addMessage(new imapMessage(specForFileName(gMessage),
                          gIMAPMailbox.uidnext++, []));
  /*
   * The message matched the SpamAssassin header, so it moved
   *  to the junk folder
   */
  gIMAPInbox.updateFolder(null);
  dl('wait for msgsMoveCopyCompleted');
  yield false;
  gJunkFolder.updateFolderWithListener(null, asyncUrlListener);
  dl('wait for OnStopRunningURL');
  yield false;
}

function testMessageInJunk()
{
  do_check_eq(0, gIMAPInbox.getTotalMessages(false));
  do_check_eq(1, gJunkFolder.getTotalMessages(false));
  yield true;
}

function markMessageAsGood()
{
  /*
   * This is done in the application in nsMsgDBView, which is difficult
   *  to test in xpcshell tests. We aren't really trying to test that here
   *  though, since the point of this test is working with the server-based
   *  filters. So I will simply simulate the operations that would typically
   *  be done by a manual marking of the messages.
   */
  let msgHdr = firstMsgHdr(gJunkFolder);
  msgHdr.setStringProperty("junkscoreorigin", "user");
  msgHdr.setStringProperty("junkpercent", "0"); // good percent
  msgHdr.setStringProperty("junkscore", "0");   // good score

  /*
   * Now move this message to the inbox. In bug 540385, the message just
   *  gets moved back to the junk folder again. We'll test that we
   *  are now preventing that.
   */
  let messages = Cc["@mozilla.org/array;1"]
                   .createInstance(Ci.nsIMutableArray);
  messages.appendElement(msgHdr, false);
  let copyService = Cc["@mozilla.org/messenger/messagecopyservice;1"]
                       .getService(Ci.nsIMsgCopyService);
  /*
  void CopyMessages(in nsIMsgFolder srcFolder,
                    in nsIArray messages,
                    in nsIMsgFolder dstFolder,
                    in boolean isMove,
                    in nsIMsgCopyServiceListener listener,
                    in nsIMsgWindow msgWindow,
                    in boolean allowUndo);
  */

  copyService.CopyMessages(gJunkFolder, messages, gIMAPInbox, true,
                            null, null, false);
  dl('wait for msgsMoveCopyCompleted');
  yield false;
}

function updateFoldersAndCheck()
{
  gIMAPInbox.updateFolderWithListener(null, asyncUrlListener);
  dl('wait for OnStopRunningURL');
  yield false;
  gJunkFolder.updateFolderWithListener(null, asyncUrlListener);
  dl('wait for OnStopRunningURL');
  yield false;
  // bug 540385 causes this test to fail
  do_check_eq(1, gIMAPInbox.getTotalMessages(false));
  do_check_eq(0, gJunkFolder.getTotalMessages(false));
  yield true;
}

function endTest()
{
  teardownIMAPPump();
}

function run_test()
{
  let server = gIMAPIncomingServer;
  let spamSettings = server.spamSettings;
  server.setBoolValue("useServerFilter", true);
  server.setCharValue("serverFilterName", "SpamAssassin");
  server.setIntValue("serverFilterTrustFlags", Ci.nsISpamSettings.TRUST_POSITIVES);
  server.setBoolValue("moveOnSpam", true);
  server.setIntValue("moveTargetMode", Ci.nsISpamSettings.MOVE_TARGET_MODE_ACCOUNT);
  server.setCharValue("spamActionTargetAccount", server.serverURI);

  spamSettings.initialize(server);

  // Add folder listeners that will capture async events
  const nsIMFNService = Ci.nsIMsgFolderNotificationService;
  let MFNService = Cc["@mozilla.org/messenger/msgnotificationservice;1"]
                      .getService(nsIMFNService);

  let flags =
        nsIMFNService.msgsMoveCopyCompleted |
        nsIMFNService.folderAdded | 
        nsIMFNService.msgAdded;
  MFNService.addListener(mfnListener, flags);

  //start first test
  async_run_tests(tests);
}

mfnListener =
{
  msgsMoveCopyCompleted: function (aMove, aSrcMsgs, aDestFolder, aDestMsgs)
  {
    dl('msgsMoveCopyCompleted to folder ' + aDestFolder.name);
    async_driver();
  },

  folderAdded: function (aFolder)
  {
    dl('folderAdded <' + aFolder.name + '>');
    // we are only using async add on the Junk folder
    if (aFolder.name == "Junk")
      async_driver();
  },

  msgAdded: function msgAdded(aMsg)
  {
    dl('msgAdded with subject <' + aMsg.subject + '>')
  }
};

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

// quick shorthand for output of a line of text.
function dl(text) {
  dump(text + '\n');
}
