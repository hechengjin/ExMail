/*
 * Test bug 460636 - nsMsgSaveAsListener sometimes inserts extra LF characters
 */

load("../../../resources/logHelper.js");
load("../../../resources/mailTestUtils.js");
load("../../../resources/asyncTestUtils.js");
load("../../../resources/IMAPpump.js");

var gSavedMsgFile;

const gIMAPService = Cc["@mozilla.org/messenger/messageservice;1?type=imap"]
                       .getService(Ci.nsIMsgMessageService);

const gFileName = "bug460636";
const gMsgFile = do_get_file("../../../data/" + gFileName);
                     
var tests = [
  setup,
  checkSavedMessage,
  teardown
];

function setup() {
  setupIMAPPump();

  /*
   * Ok, prelude done. Read the original message from disk
   * (through a file URI), and add it to the Inbox.
   */
  var msgfileuri = Cc["@mozilla.org/network/io-service;1"]
                     .getService(Ci.nsIIOService)
                     .newFileURI(gMsgFile).QueryInterface(Ci.nsIFileURL);

  gIMAPMailbox.addMessage(new imapMessage(msgfileuri.spec, gIMAPMailbox.uidnext++, []));
  gIMAPInbox.updateFolderWithListener(null, asyncUrlListener);
  yield false;

  /*
   * Save the message to a local file. IMapMD corresponds to
   * <profile_dir>/mailtest/ImapMail (where fakeserver puts the IMAP mailbox
   * files). If we pass the test, we'll remove the file afterwards
   * (cf. UrlListener), otherwise it's kept in IMapMD.
   */
  gSavedMsgFile = Cc["@mozilla.org/file/directory_service;1"]
                  .getService(Ci.nsIProperties)
                  .get("IMapMD", Ci.nsILocalFile);
  gSavedMsgFile.append(gFileName + ".eml");

  /*
   * From nsIMsgMessageService.idl:
   * void SaveMessageToDisk(in string aMessageURI, in nsIFile aFile,
   *                        in boolean aGenerateDummyEnvelope,
   *                        in nsIUrlListener aUrlListener, out nsIURI aURL,
   *                        in boolean canonicalLineEnding,
   *                        in nsIMsgWindow aMsgWindow);
   * Enforcing canonicalLineEnding (i.e., CRLF) makes sure that the
   * test also runs successfully on platforms not using CRLF by default.
   */
  gIMAPService.SaveMessageToDisk("imap-message://user@localhost/INBOX#"
                                 + (gIMAPMailbox.uidnext-1), gSavedMsgFile,
                                 false, asyncUrlListener, {}, true, null);
  yield false;
}

function checkSavedMessage() {
  do_check_eq(loadFileToString(gMsgFile), loadFileToString(gSavedMsgFile));
}

function teardown() {
  try {
    gSavedMsgFile.remove(false);
  }
  catch (ex) {
    dump(ex);
    do_throw(ex);
  }
  teardownIMAPPump();
}

function run_test() {
  async_run_tests(tests);
}
