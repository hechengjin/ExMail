/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test loading of virtualFolders.dat, including verification of the search 
// scopes, i.e., folder uri's.

const am = Components.classes["@mozilla.org/messenger/account-manager;1"]
                     .getService(Components.interfaces.nsIMsgAccountManager);

// As currently written, this test will only work with Berkeley store.
Services.prefs.setCharPref("mail.serverDefaultStoreContractID",
                           "@mozilla.org/msgstore/berkeleystore;1");

// main test

function run_test()
{
  let vfdat = do_get_file("../../../data/test_virtualFolders.dat");

  vfdat.copyTo(gProfileDir, "virtualFolders.dat");
  loadLocalMailAccount();
  let localMailDir = gProfileDir.clone();
  localMailDir.append("Mail");
  localMailDir.append("Local Folders");
  localMailDir.append("unread-local");
  localMailDir.create(Ci.nsIFile.NORMAL_FILE_TYPE, parseInt("0644", 8));
  localMailDir.leafName = "invalidserver-local";
  localMailDir.create(Ci.nsIFile.NORMAL_FILE_TYPE, parseInt("0644", 8));

  am.loadVirtualFolders();
  let unreadLocal = gLocalIncomingServer.rootMsgFolder.getChildNamed("unread-local");
  let searchScope = unreadLocal.msgDatabase.dBFolderInfo.getCharProperty("searchFolderUri");
  do_check_eq(searchScope, "mailbox://nobody@Local%20Folders/Inbox|mailbox://nobody@Local%20Folders/Trash");
  let invalidServer = gLocalIncomingServer.rootMsgFolder.getChildNamed("invalidserver-local");
  let searchScope = invalidServer.msgDatabase.dBFolderInfo.getCharProperty("searchFolderUri");
  do_check_eq(searchScope, "mailbox://nobody@Local%20Folders/Inbox");
}
