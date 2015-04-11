// This file tests the folder copying with IMAP. In particular, we're
// going to test copying local folders to imap servers, but other tests
// could be added.

load("../../../resources/logHelper.js");
load("../../../resources/mailTestUtils.js");
load("../../../resources/asyncTestUtils.js");
load("../../../resources/messageGenerator.js");
load("../../../resources/IMAPpump.js");

var gEmptyLocal1, gEmptyLocal2, gEmptyLocal3, gNotEmptyLocal4;
var gCopyService = Cc["@mozilla.org/messenger/messagecopyservice;1"]
                .getService(Ci.nsIMsgCopyService);

Components.utils.import("resource:///modules/folderUtils.jsm");
Components.utils.import("resource:///modules/iteratorUtils.jsm");

var tests = [
  setup,
  function copyFolder1() {
    dump("gEmpty1 " + gEmptyLocal1.URI + "\n");
    let folders = new Array;
    folders.push(gEmptyLocal1.QueryInterface(Ci.nsIMsgFolder));
    let array = toXPCOMArray(folders, Ci.nsIMutableArray);
    gCopyService.CopyFolders(array, gIMAPInbox, false, CopyListener, null);
    yield false;
  },
  function copyFolder2() {
    dump("gEmpty2 " + gEmptyLocal2.URI + "\n");
    let folders = new Array;
    folders.push(gEmptyLocal2);
    let array = toXPCOMArray(folders, Ci.nsIMutableArray);
    gCopyService.CopyFolders(array, gIMAPInbox, false, CopyListener, null);
    yield false;
  },
  function copyFolder3() {
    dump("gEmpty3 " + gEmptyLocal3.URI + "\n");
    let folders = new Array;
    folders.push(gEmptyLocal3);
    let array = toXPCOMArray(folders, Ci.nsIMutableArray);
    gCopyService.CopyFolders(array, gIMAPInbox, false, CopyListener, null);
    yield false;
  },
  function verifyFolders() {
    let folder1 = gIMAPInbox.getChildNamed("empty 1");
    dump("found folder1\n");
    let folder2 = gIMAPInbox.getChildNamed("empty 2");
    dump("found folder2\n");
    let folder3 = gIMAPInbox.getChildNamed("empty 3");
    dump("found folder3\n");
    do_check_neq(folder1, null);
    do_check_neq(folder2, null);
    do_check_neq(folder3, null);
  },
  function moveImapFolder1() {
    let folders = new Array;
    let folder1 = gIMAPInbox.getChildNamed("empty 1");
    let folder2 = gIMAPInbox.getChildNamed("empty 2");
    folders.push(folder2.QueryInterface(Ci.nsIMsgFolder));
    let array = toXPCOMArray(folders, Ci.nsIMutableArray);
    gCopyService.CopyFolders(array, folder1, true, CopyListener, null);
    yield false;
  },
  function moveImapFolder2() {
    let folders = new Array;
    let folder1 = gIMAPInbox.getChildNamed("empty 1");
    let folder3 = gIMAPInbox.getChildNamed("empty 3");
    folders.push(folder3.QueryInterface(Ci.nsIMsgFolder));
    let array = toXPCOMArray(folders, Ci.nsIMutableArray);
    gCopyService.CopyFolders(array, folder1, true, CopyListener, null);
    yield false;
  },
  function verifyImapFolders() {
    let folder1 = gIMAPInbox.getChildNamed("empty 1");
    dump("found folder1\n");
    let folder2 = folder1.getChildNamed("empty 2");
    dump("found folder2\n");
    let folder3 = folder1.getChildNamed("empty 3");
    dump("found folder3\n");
    do_check_neq(folder1, null);
    do_check_neq(folder2, null);
    do_check_neq(folder3, null);
  },
  function testImapFolderCopyFailure() {
    let folders = new Array;
    folders.push(gNotEmptyLocal4.QueryInterface(Ci.nsIMsgFolder));
    let array = toXPCOMArray(folders, Ci.nsIMutableArray);
    gIMAPDaemon.commandToFail = "APPEND";
    // we expect NS_MSG_ERROR_IMAP_COMMAND_FAILED;
    CopyListener._expectedStatus = 0x80550021;
    gCopyService.CopyFolders(array, gIMAPInbox, false, CopyListener, null);

    // In failure case OnStopCopy is sent twice, the first one comes from
    // nsMsgCopyService, the second one comes from nsImapFolderCopyState.
    yield false;

    yield false;
  },
  teardown
];

function setup() {
  setupIMAPPump();

  gEmptyLocal1 = gLocalIncomingServer.rootFolder.createLocalSubfolder("empty 1");
  gEmptyLocal2 = gLocalIncomingServer.rootFolder.createLocalSubfolder("empty 2");
  gEmptyLocal3 = gLocalIncomingServer.rootFolder.createLocalSubfolder("empty 3");
  gNotEmptyLocal4 = gLocalIncomingServer.rootFolder.createLocalSubfolder("not empty 4");

  let messageGenerator = new MessageGenerator();
  let message = messageGenerator.makeMessage();
  gNotEmptyLocal4.QueryInterface(Ci.nsIMsgLocalMailFolder);
  gNotEmptyLocal4.addMessage(message.toMboxString());

  // these hacks are required because we've created the inbox before
  // running initial folder discovery, and adding the folder bails
  // out before we set it as verified online, so we bail out, and
  // then remove the INBOX folder since it's not verified.
  gIMAPInbox.hierarchyDelimiter = '/';
  gIMAPInbox.verifiedAsOnlineFolder = true;
}

// nsIMsgCopyServiceListener implementation - runs next test when copy
// is completed.
var CopyListener = {
  _expectedStatus : 0,
  OnStartCopy: function() {},
  OnProgress: function(aProgress, aProgressMax) {},
  SetMessageKey: function(aKey) {},
  SetMessageId: function(aMessageId) {},
  OnStopCopy: function(aStatus) {
    // Check: message successfully copied.
    do_check_eq(aStatus, this._expectedStatus);
    async_driver();
  }
};

function teardown() {
  teardownIMAPPump();
}

function run_test() {
  async_run_tests(tests);
}

