/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Tests imap save and detach attachments.
 */

load("../../../resources/logHelper.js");
load("../../../resources/mailTestUtils.js");
load("../../../resources/asyncTestUtils.js");
load("../../../resources/messageGenerator.js");

// javascript mime emitter functions
mimeMsg = {};
Components.utils.import("resource:///modules/gloda/mimemsg.js", mimeMsg);

// IMAP pump
load("../../../resources/IMAPpump.js");

const kAttachFileName = 'bob.txt';

setupIMAPPump();

// Dummy message window so we can say the inbox is open in a window.
var dummyMsgWindow =
{
  openFolder : gIMAPInbox,
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIMsgWindow,
                                         Ci.nsISupportsWeakReference])
  
};

var tests = [
  loadImapMessage,
  startMime,
  startDetach,
  testDetach,
  endTest
]

// load and update a message in the imap fake server
function loadImapMessage()
{
  let gMessageGenerator = new MessageGenerator();
  // create a synthetic message with attachment
  let smsg = gMessageGenerator.makeMessage({
    attachments: [
      {filename: kAttachFileName, body: 'I like cheese!'}
    ],
  });

  let ioService = Cc["@mozilla.org/network/io-service;1"]
                  .getService(Ci.nsIIOService);
  let msgURI =
    ioService.newURI("data:text/plain;base64," +
                     btoa(smsg.toMessageString()),
                     null, null);
  let imapInbox =  gIMAPDaemon.getMailbox("INBOX")
  let message = new imapMessage(msgURI.spec, imapInbox.uidnext++, []);
  gIMAPMailbox.addMessage(message);
  gIMAPInbox.updateFolderWithListener(null, asyncUrlListener);
  yield false;
  do_check_eq(1, gIMAPInbox.getTotalMessages(false));
  let msgHdr = firstMsgHdr(gIMAPInbox);
  do_check_true(msgHdr instanceof Ci.nsIMsgDBHdr);

  yield true;
}

// process the message through mime
function startMime()
{
  let msgHdr = firstMsgHdr(gIMAPInbox);

  mimeMsg.MsgHdrToMimeMessage(msgHdr, gCallbackObject, gCallbackObject.callback,
                              true /* allowDownload */);
  yield false;
}

// detach any found attachments
function startDetach()
{
  let msgHdr = firstMsgHdr(gIMAPInbox);
  let msgURI = msgHdr.folder.generateMessageURI(msgHdr.messageKey);

  let messenger = Cc["@mozilla.org/messenger;1"].createInstance(Ci.nsIMessenger);
  let attachment = gCallbackObject.attachments[0];

  messenger.detachAttachmentsWOPrompts(gProfileDir, 1,
                                       [attachment.contentType], [attachment.url],
                                       [attachment.name], [msgURI], null);
  // deletion of original message should kick async_driver.
  yield false;
}

// test that the detachment was successful
function testDetach()
{
  // This test seems to fail on Linux without the following delay.
  do_timeout_function(200, async_driver);
  yield false;
  // Check that the file attached to the message now exists in the profile 
  // directory.
  let checkFile = gProfileDir.clone();
  checkFile.append(kAttachFileName);
  do_check_true(checkFile.exists());

  // The message should now have a detached attachment. Read the message,
  //  and search for "AttachmentDetached" which is added on detachment.

  // Get the message header - detached copy has UID 2.
  let msgHdr = gIMAPInbox.GetMessageHeader(2);
  do_check_neq(msgHdr, null);
  let messageContent = getContentFromMessage(msgHdr);
  do_check_true(messageContent.indexOf("AttachmentDetached") != -1);
}

// Cleanup
function endTest()
{
  teardownIMAPPump();
}

function SaveAttachmentCallback() {
  this.attachments = null;
}

SaveAttachmentCallback.prototype = {
  callback: function saveAttachmentCallback_callback(aMsgHdr, aMimeMessage) {
    this.attachments = aMimeMessage.allAttachments;
    async_driver();
  }
}
let gCallbackObject = new SaveAttachmentCallback();

function run_test()
{
  // Add folder listeners that will capture async events
  const nsIMFNService = Ci.nsIMsgFolderNotificationService;
  let MFNService = Cc["@mozilla.org/messenger/msgnotificationservice;1"]
                      .getService(nsIMFNService);
  MFNService.addListener(mfnListener, nsIMFNService.msgsDeleted);

  // We need to register the dummyMsgWindow so that when we've finished running
  // the append url, in nsImapMailFolder::OnStopRunningUrl, we'll think the
  // Inbox is open in a folder and update it, which the detach code relies
  // on to finish the detach.
  Cc["@mozilla.org/messenger/services/session;1"]
    .getService(Ci.nsIMsgMailSession).AddMsgWindow(dummyMsgWindow);

  async_run_tests(tests);
}

/*
 * Get the full message content.
 *
 * aMsgHdr: nsIMsgDBHdr object whose text body will be read
 *          returns: string with full message contents
 */
function getContentFromMessage(aMsgHdr) {
  const MAX_MESSAGE_LENGTH = 65536;
  let msgFolder = aMsgHdr.folder;
  let msgUri = msgFolder.getUriForMsg(aMsgHdr);

  let messenger = Cc["@mozilla.org/messenger;1"]
                    .createInstance(Ci.nsIMessenger);
  let streamListener = Cc["@mozilla.org/network/sync-stream-listener;1"]
                         .createInstance(Ci.nsISyncStreamListener);
  // pass true for aLocalOnly since message should be in offline store.
  messenger.messageServiceFromURI(msgUri).streamMessage(msgUri,
                                                        streamListener,
                                                        null,
                                                        null,
                                                        false,
                                                        "",
                                                        true);
  let sis = Cc["@mozilla.org/scriptableinputstream;1"]
              .createInstance(Ci.nsIScriptableInputStream);
  sis.init(streamListener.inputStream);
  return sis.read(MAX_MESSAGE_LENGTH);
}

mfnListener =
{
  msgsDeleted: function msgsDeleted(aMsgArray)
  {
    dump("msg deleted\n");
    async_driver();
  },
};
