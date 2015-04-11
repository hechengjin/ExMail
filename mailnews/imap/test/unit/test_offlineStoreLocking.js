/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * Test to ensure that code that writes to the imap offline store deals
 * with offline store locking correctly.
 */

const nsIIOService = Cc["@mozilla.org/network/io-service;1"]
                       .getService(Ci.nsIIOService);

load("../../../resources/logHelper.js");
load("../../../resources/mailTestUtils.js");
load("../../../resources/asyncTestUtils.js");
load("../../../resources/messageGenerator.js");
load("../../../resources/alertTestUtils.js");
load("../../../resources/IMAPpump.js");

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");


// Globals
var gIMAPTrashFolder, gMsgImapInboxFolder;
var gGotAlert = false;
var gMovedMsgId;

function alert(aDialogTitle, aText) {
//  do_check_eq(aText.indexOf("Connection to server Mail for  timed out."), 0);
  gGotAlert = true;
}

function addGeneratedMessagesToServer(messages, mailbox)
{
  let ioService = Cc["@mozilla.org/network/io-service;1"]
                    .getService(Ci.nsIIOService);
  // Create the imapMessages and store them on the mailbox
  messages.forEach(function (message)
  {
    let dataUri = ioService.newURI("data:text/plain;base64," +
                                    btoa(message.toMessageString()),
                                   null, null);
    mailbox.addMessage(new imapMessage(dataUri.spec, mailbox.uidnext++, []));
  });
}

var gStreamedHdr = null;

function checkOfflineStore(prevOfflineStoreSize) {
  dump("checking offline store\n");
  let offset = new Object;
  let size = new Object;
  let enumerator = gIMAPInbox.msgDatabase.EnumerateMessages();
  if (enumerator)
  {
    while (enumerator.hasMoreElements())
    {
      let header = enumerator.getNext();
      // this will verify that the message in the offline store
      // starts with "From " - otherwise, it returns an error.
      if (header instanceof Components.interfaces.nsIMsgDBHdr &&
         (header.flags & Ci.nsMsgMessageFlags.Offline))
        gIMAPInbox.getOfflineFileStream(header.messageKey, offset, size).close();
    }
  }
  // check that the offline store shrunk by at least 100 bytes.
  // (exact calculation might be fragile).
  do_check_true(prevOfflineStoreSize > gIMAPInbox.filePath.fileSize + 100);
}

var tests = [
  setup,
  function downloadForOffline() {
    // ...and download for offline use.
    dump("Downloading for offline use\n");
    gIMAPInbox.downloadAllForOffline(asyncUrlListener, null);
    yield false;
  },
  function deleteOneMsg() {
    let enumerator = gIMAPInbox.msgDatabase.EnumerateMessages();
    let msgHdr = enumerator.getNext().QueryInterface(Ci.nsIMsgDBHdr);
    let array = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);
    array.appendElement(msgHdr, false);
    gIMAPInbox.deleteMessages(array, null, false, true, CopyListener, false);
    yield false;
  },
  function compactOneFolder() {
    let enumerator = gIMAPInbox.msgDatabase.EnumerateMessages();
    let msgHdr = enumerator.getNext().QueryInterface(Ci.nsIMsgDBHdr);
    gStreamedHdr = msgHdr;
    // mark the message as not being offline, and then we'll make sure that
    // streaming the message while we're compacting doesn't result in the
    // message being marked for offline use.
    // Luckily, compaction compacts the offline store first, so it should
    // lock the offline store.
    gIMAPInbox.msgDatabase.MarkOffline(msgHdr.messageKey, false, null);
    let msgURI = msgHdr.folder.getUriForMsg(msgHdr);
    let messenger = Cc["@mozilla.org/messenger;1"].createInstance(Ci.nsIMessenger);
    let msgServ = messenger.messageServiceFromURI(msgURI);
    // UrlListener will get called when both expunge and offline store
    // compaction are finished. dummyMsgWindow is required to make the backend
    // compact the offline store.
    gIMAPInbox.compact(asyncUrlListener, gDummyMsgWindow);
    // Stream the message w/o a stream listener in an attempt to get the url
    // started more quickly, while the compact is still going on.
    msgServ.streamMessage(msgURI, null, null, asyncUrlListener, false, "", false);
    yield false;

    // Because we're streaming the message while compaction is going on,
    // we should not have stored it for offline use.
    do_check_false(gStreamedHdr.flags & Ci.nsMsgMessageFlags.Offline);

    yield false;
  },
  function deleteAnOtherMsg() {
    let enumerator = gIMAPInbox.msgDatabase.EnumerateMessages();
    let msgHdr = enumerator.getNext();
    let array = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);
    array.appendElement(msgHdr, false);
    gIMAPInbox.deleteMessages(array, null, false, true, CopyListener, false);
    yield false;
  },
  function updateTrash() {
    gIMAPTrashFolder = gIMAPIncomingServer.rootFolder.getChildNamed("Trash")
                         .QueryInterface(Ci.nsIMsgImapMailFolder);
    // hack to force uid validity to get initialized for trash.
    gIMAPTrashFolder.updateFolderWithListener(null, asyncUrlListener);
    yield false;
  },
  function downloadTrashForOffline() {
    // ...and download for offline use.
    dump("Downloading for offline use\n");
    gIMAPTrashFolder.downloadAllForOffline(asyncUrlListener, null);
    yield false;
  },
  function testOfflineBodyCopy() {
    // In order to check that offline copy of messages doesn't try to copy
    // the body if the offline store is locked, we're going to go offline.
    // Thunderbird itself does move/copies pseudo-offline, but that's too
    // hard to test because of the half-second delay.
    gIMAPServer.stop();
    nsIIOService.offline = true;
    let trashHdr;
    let enumerator = gIMAPTrashFolder.msgDatabase.EnumerateMessages();
    let msgHdr = enumerator.getNext().QueryInterface(Ci.nsIMsgDBHdr);
    gMovedMsgId = msgHdr.messageId;
    let array = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);
    array.appendElement(msgHdr, false);
    const gCopyService = Cc["@mozilla.org/messenger/messagecopyservice;1"]
                          .getService(Ci.nsIMsgCopyService);

    gIMAPInbox.compact(asyncUrlListener, gDummyMsgWindow);
    gCopyService.CopyMessages(gIMAPTrashFolder, array, gIMAPInbox, true,
                              CopyListener, null, true);
  },
  function verifyNoOfflineMsg() {
    try {
    let movedMsg = gIMAPInbox.msgDatabase.getMsgHdrForMessageID(gMovedMsgId);
    do_check_false(movedMsg.flags & Ci.nsMsgMessageFlags.Offline);
    } catch (ex) {dump(ex);}
    yield false;
    yield false;
  },
  teardown
];

function run_test() {
  async_run_tests(tests);
}

function setup() {
  Services.prefs.setBoolPref("mail.server.default.autosync_offline_stores", false);

  setupIMAPPump();

  gMsgImapInboxFolder = gIMAPInbox.QueryInterface(Ci.nsIMsgImapMailFolder);
  // these hacks are required because we've created the inbox before
  // running initial folder discovery, and adding the folder bails
  // out before we set it as verified online, so we bail out, and
  // then remove the INBOX folder since it's not verified.
  gMsgImapInboxFolder.hierarchyDelimiter = '/';
  gMsgImapInboxFolder.verifiedAsOnlineFolder = true;

  let messageGenerator = new MessageGenerator();
  let messages = [];
  let bodyString = "";
  for (i = 0; i < 100; i++)
    bodyString += "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890\r\n";

  for (let i = 0; i < 50; i++)
    messages = messages.concat(messageGenerator.makeMessage({body: {body: bodyString, contentType: "text/plain"}}));

  addGeneratedMessagesToServer(messages, gIMAPDaemon.getMailbox("INBOX"));
}

// nsIMsgCopyServiceListener implementation - runs next test when copy
// is completed.
var CopyListener = 
{
  OnStartCopy: function() {},
  OnProgress: function(aProgress, aProgressMax) {},
  SetMessageKey: function(aKey)
  {
    let hdr = gLocalInboxFolder.GetMessageHeader(aKey);
    gMsgHdrs.push({hdr: hdr, ID: hdr.messageId});
  },
  SetMessageId: function(aMessageId) {},
  OnStopCopy: function(aStatus)
  {
    // Check: message successfully copied.
    do_check_eq(aStatus, 0);
    async_driver();
  }
};

function teardown() {
  gMsgImapInboxFolder = null;
  gIMAPTrashFolder = null;

  // gIMAPServer has already stopped, we do not need to gIMAPServer.stop().
  gIMAPInbox = null;
  try {
    gIMAPIncomingServer.closeCachedConnections();
    let serverSink = gIMAPIncomingServer.QueryInterface(Ci.nsIImapServerSink);
    serverSink.abortQueuedUrls();
  } catch (ex) {dump(ex);}
  let thread = gThreadManager.currentThread;
  while (thread.hasPendingEvents())
    thread.processNextEvent(true);
}
