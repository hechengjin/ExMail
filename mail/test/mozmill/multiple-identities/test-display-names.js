/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Test that we can open and close a standalone message display window from the
 *  folder pane.
 */
var MODULE_NAME = "test-display-names";

var RELATIVE_ROOT = "../shared-modules";
var MODULE_REQUIRES = ["folder-display-helpers", "address-book-helpers"];

var folder;
var decoyFolder;
var acctMgr;
var localAccount;
var secondIdentity;
var myEmail = "sender@nul.nul"; // Dictated by messagerInjector.js
var friendEmail = "carl@sagan.com";
var friendName = "Carl Sagan";
var headertoFieldMe;
var collectedAddresses;

function setupModule(module) {
  let fdh = collector.getModule("folder-display-helpers");
  fdh.installInto(module);
  let abh = collector.getModule("address-book-helpers");
  abh.installInto(module);

  acctMgr = Cc["@mozilla.org/messenger/account-manager;1"]
              .getService(Ci.nsIMsgAccountManager);
  localAccount = acctMgr.FindAccountForServer(acctMgr.localFoldersServer);

  // We need to make sure we have only one identity:
  // 1) Delete all accounts except for Local Folders
  for (let i = acctMgr.accounts.Count()-1; i >= 0; i--) {
    let account = acctMgr.accounts.QueryElementAt(i, Ci.nsIMsgAccount);
    if (account != localAccount)
      acctMgr.removeAccount(account);
  }

  // 2) Delete all identities except for one
  for (let i = localAccount.identities.Count()-1; i >= 0; i--) {
    let identity = localAccount.identities.QueryElementAt(i, Ci.nsIMsgIdentity);
    if (identity.email != myEmail)
      localAccount.removeIdentity(identity);
  }

  // 3) Create a second identity and hold onto it for later
  secondIdentity = acctMgr.createIdentity();
  secondIdentity.email = "nobody@nowhere.com";

  folder = create_folder("DisplayNamesA");
  decoyFolder = create_folder("DisplayNamesB");

  add_message_to_folder(folder, create_message({to: [["", myEmail]] }));
  add_message_to_folder(folder, create_message({from: ["", friendEmail] }));
  add_message_to_folder(folder, create_message({from: [friendName, friendEmail] }));

  let abManager = Cc["@mozilla.org/abmanager;1"].getService(Ci.nsIAbManager);
  // Ensure all the directories are initialised.
  abManager.directories;
  collectedAddresses = abManager.getDirectory("moz-abmdbdirectory://history.mab");

  let bundle = Cc["@mozilla.org/intl/stringbundle;1"]
                 .getService(Ci.nsIStringBundleService).createBundle(
                   "chrome://messenger/locale/messenger.properties");
  headertoFieldMe = bundle.GetStringFromName("headertoFieldMe");
}

function ensure_single_identity() {
  if (localAccount.identities.Count() > 1)
    localAccount.removeIdentity(secondIdentity);
  assert_true(acctMgr.allIdentities.Count() == 1,
              "Expected 1 identity, but got " + acctMgr.allIdentities.Count() +
              " identities");
}

function ensure_multiple_identities() {
  if (localAccount.identities.Count() == 1)
    localAccount.addIdentity(secondIdentity);
  assert_true(acctMgr.allIdentities.Count() > 1,
              "Expected multiple identities, but got only one identity")
}

function help_test_display_name(message, field, expectedValue) {
  // Switch to a decoy folder first to ensure that we refresh the message we're
  // looking at in order to update information changed in address book entries.
  be_in_folder(decoyFolder);
  be_in_folder(folder);
  let curMessage = select_click_row(message);

  let value = mc.window.document.getAnonymousElementByAttribute(
    mc.a("expanded"+field+"Box", {tagName: "mail-emailaddress"}),
    "class", "emaillabel").value;

  if (value != expectedValue)
    throw new Error("got '"+value+"' but expected '"+expectedValue+"'");
}



function test_single_identity() {
  ensure_no_card_exists(myEmail);
  ensure_single_identity();
  help_test_display_name(0, "to", headertoFieldMe);
}

function test_single_identity_in_abook() {
  ensure_card_exists(myEmail, "President Frankenstein", true);
  ensure_single_identity();
  help_test_display_name(0, "to", "President Frankenstein");
}

function test_single_identity_in_abook_no_pdn() {
  ensure_card_exists(myEmail, "President Frankenstein");
  ensure_single_identity();
  help_test_display_name(0, "to", headertoFieldMe);
}



function test_multiple_identities() {
  ensure_no_card_exists(myEmail);
  ensure_multiple_identities();
  help_test_display_name(0, "to", headertoFieldMe+" <"+myEmail+">");
}

function test_multiple_identities_in_abook() {
  ensure_card_exists(myEmail, "President Frankenstein", true);
  ensure_multiple_identities();
  help_test_display_name(0, "to", "President Frankenstein");
}

function test_multiple_identities_in_abook_no_pdn() {
  ensure_card_exists(myEmail, "President Frankenstein");
  ensure_multiple_identities();
  help_test_display_name(0, "to", headertoFieldMe+" <"+myEmail+">");
}



function test_no_header_name() {
  ensure_no_card_exists(friendEmail);
  ensure_single_identity();
  help_test_display_name(1, "from", friendEmail);
}

function test_no_header_name_in_abook() {
  ensure_card_exists(friendEmail, "My Buddy", true);
  ensure_single_identity();
  help_test_display_name(1, "from", "My Buddy");
}

function test_no_header_name_in_abook_no_pdn() {
  ensure_card_exists(friendEmail, "My Buddy");
  ensure_single_identity();
  help_test_display_name(1, "from", "My Buddy");
}



function test_header_name() {
  ensure_no_card_exists(friendEmail);
  ensure_single_identity();
  help_test_display_name(2, "from", friendName+" <"+friendEmail+">");
}

function test_header_name_in_abook() {
  ensure_card_exists(friendEmail, "My Buddy", true);
  ensure_single_identity();
  help_test_display_name(2, "from", "My Buddy");
}

function test_header_name_in_abook_no_pdn() {
  ensure_card_exists(friendEmail, "My Buddy");
  ensure_single_identity();
  help_test_display_name(2, "from", "Carl Sagan");
}