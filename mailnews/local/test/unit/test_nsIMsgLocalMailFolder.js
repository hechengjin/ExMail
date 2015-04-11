/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * Test suite for local folder functions.
 */

load("../../../resources/messageGenerator.js");

function run_test() {
  var acctMgr = Components.classes["@mozilla.org/messenger/account-manager;1"]
                          .getService(Components.interfaces.nsIMsgAccountManager);

  // Create a local mail account (we need this first)
  acctMgr.createLocalMailAccount();

  // Get the account
  var account = acctMgr.accounts.GetElementAt(0)
                       .QueryInterface(Components.interfaces.nsIMsgAccount);

  // Get the root folder
  var root = account.incomingServer.rootFolder.QueryInterface(Ci.nsIMsgLocalMailFolder);

  // Give it a poke so that the directories all exist
  root.subFolders;

  // Test - num/hasSubFolders

  // Get the current number of folders
  var numSubFolders = root.numSubFolders;

  var folder = root.createLocalSubfolder("folder1").QueryInterface(Ci.nsIMsgLocalMailFolder);

  do_check_true(root.hasSubFolders);
  do_check_eq(root.numSubFolders, numSubFolders + 1);

  do_check_false(folder.hasSubFolders);
  do_check_eq(folder.numSubFolders, 0);

  var folder2 = folder.createLocalSubfolder("folder2");

  do_check_true(folder.hasSubFolders);
  do_check_eq(folder.numSubFolders, 1);

  // Test - getChildNamed

  do_check_eq(root.getChildNamed("folder1"), folder);

  // Check for non match, this should throw
  var thrown = false;
  try {
    root.getChildNamed("folder2");
  }
  catch (e) {
    thrown = true;
  }

  do_check_true(thrown);

  // folder2 is a child of folder however.
  var folder2 = folder.getChildNamed("folder2");

  // Test - isAncestorOf

  do_check_true(folder.isAncestorOf(folder2));
  do_check_true(root.isAncestorOf(folder2));
  do_check_false(folder.isAncestorOf(root));

  // Test - FoldersWithFlag

  const nsMsgFolderFlags = Components.interfaces.nsMsgFolderFlags;

  folder.setFlag(nsMsgFolderFlags.CheckNew);
  do_check_true(folder.getFlag(nsMsgFolderFlags.CheckNew));
  do_check_false(folder.getFlag(nsMsgFolderFlags.Offline));

  folder.setFlag(nsMsgFolderFlags.Offline);
  do_check_true(folder.getFlag(nsMsgFolderFlags.CheckNew));
  do_check_true(folder.getFlag(nsMsgFolderFlags.Offline));

  folder.toggleFlag(nsMsgFolderFlags.CheckNew);
  do_check_false(folder.getFlag(nsMsgFolderFlags.CheckNew));
  do_check_true(folder.getFlag(nsMsgFolderFlags.Offline));

  folder.clearFlag(nsMsgFolderFlags.Offline);
  do_check_false(folder.getFlag(nsMsgFolderFlags.CheckNew));
  do_check_false(folder.getFlag(nsMsgFolderFlags.Offline));

  folder.setFlag(nsMsgFolderFlags.Favorite);
  folder2.setFlag(nsMsgFolderFlags.Favorite);
  folder.setFlag(nsMsgFolderFlags.CheckNew);
  folder2.setFlag(nsMsgFolderFlags.Offline);

  do_check_eq(root.getFolderWithFlags(nsMsgFolderFlags.CheckNew),
              folder);

  // Test - Move folders around

  var folder3 = root.createLocalSubfolder("folder3");
  var folder3Local = folder3.QueryInterface(Components.interfaces.nsIMsgLocalMailFolder);
  var folder1Local = folder.QueryInterface(Components.interfaces.nsIMsgLocalMailFolder);

  // put a single message in folder1.
  let messageGenerator = new MessageGenerator();
  let message = messageGenerator.makeMessage();
  let hdr = folder1Local.addMessage(message.toMboxString());
  do_check_eq(message.messageId, hdr.messageId);

  folder3Local.copyFolderLocal(folder, true, null, null);

  // Test - Get the new folders, make sure the old ones don't exist

  var folder1Moved = folder3.getChildNamed("folder1");
  var folder2Moved = folder1Moved.getChildNamed("folder2");

  thrown = false;
  try {
    root.getChildNamed("folder1");
  }
  catch (e) {
    thrown = true;
  }

  do_check_true(thrown);

  if (folder.filePath.exists())
    dump("shouldn't exist - folder file path " + folder.URI + "\n");
  do_check_false(folder.filePath.exists());
  if (folder2.filePath.exists())
    dump("shouldn't exist - folder2 file path " + folder2.URI + "\n");
  do_check_false(folder2.filePath.exists());

  // make sure getting the db doesn't throw an exception
  let db = folder1Moved.msgDatabase;
  do_check_true(db.summaryValid);

  // Move folders back, get them
  var rootLocal = root.QueryInterface(Components.interfaces.nsIMsgLocalMailFolder);
  rootLocal.copyFolderLocal(folder1Moved, true, null, null);
  folder = root.getChildNamed("folder1");
  folder2 = folder.getChildNamed("folder2");

  // Test - Rename (test that .msf file is renamed as well)
  folder.rename("folder1-newname", null);
  // make sure getting the db doesn't throw an exception, and is valid
  folder = rootLocal.getChildNamed("folder1-newname");
  let db = folder.msgDatabase;
  do_check_true(db.summaryValid);

  folder.rename("folder1", null);
  folder = rootLocal.getChildNamed("folder1");

  // Test - propagateDelete (this tests recursiveDelete as well)

  var path1 = folder.filePath;
  var path2 = folder2.filePath;
  var path3 = folder3.filePath;

  do_check_true(path1.exists());
  do_check_true(path2.exists());
  do_check_true(path3.exists());

  // First try deleting folder3 -- folder1 and folder2 paths should still exist
  root.propagateDelete(folder3, true, null);

  do_check_true(path1.exists());
  do_check_true(path2.exists());
  do_check_false(path3.exists());

  root.propagateDelete(folder, true, null);

  do_check_false(path1.exists());
  do_check_false(path2.exists());
}
