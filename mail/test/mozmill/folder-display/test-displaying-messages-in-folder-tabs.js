/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Test displaying messages in folder tabs. This is supposed to test a variety
 * of situations, including:
 * - dropping view filters to select the message
 * - displaying the message in an existing folder tab
 */

var MODULE_NAME = "test-displaying-messages-in-folder-tabs";

var RELATIVE_ROOT = "../shared-modules";
var MODULE_REQUIRES = ["folder-display-helpers", "window-helpers",
                       "search-window-helpers"];

var folderA;
var folderB;
var folderC;

var folderTab;

let msgHdrsInFolderA = [], msgHdrsInFolderB = [];

// The number of messages in folderA/folderB.
const NUM_MESSAGES_IN_FOLDER = 10;

// A generator for indexes to load test.
function _generateIndexes() {
  let index = 0;
  while (true) {
    yield index;
    index = (index + 1) % NUM_MESSAGES_IN_FOLDER;
  }
}
var indexes = _generateIndexes();

function setupModule(module) {
  let fdh = collector.getModule("folder-display-helpers");
  fdh.installInto(module);
  let wh = collector.getModule("window-helpers");
  wh.installInto(module);
  let sh = collector.getModule("search-window-helpers");
  sh.installInto(module);

  folderA = create_folder("DisplayMessageFolderTabA");
  folderB = create_folder("DisplayMessageFolderTabB");
  folderC = create_folder("DisplayMessageFolderTabC");
  make_new_sets_in_folder(folderA, [{count: NUM_MESSAGES_IN_FOLDER}]);
  let [, taggedMsg] = make_new_sets_in_folder(folderB,
                                              [{count: NUM_MESSAGES_IN_FOLDER - 1},
                                               {count: 1}]);
  taggedMsg.addTag("$label3");
  make_new_sets_in_folder(folderC, [{count: 50}]);
}

/**
 * Helper to test that a new 3-pane window is opened, a message is displayed,
 * and that the message is visible in the thread pane, even if there are no
 * 3-pane windows open.
 *
 * This test is done right at the beginning so that the globals set up right
 * after that are valid for all the other tests.
 */
function test_display_message_with_no_3pane_windows_open() {
  be_in_folder(folderC);
  // Scroll to the top
  mc.folderDisplay.ensureRowIsVisible(0);
  let msgHdr = mc.dbView.getMsgHdrAt(45);

  // Switch to a different folder, just to exercise that code
  be_in_folder(folderB);

  // Make sure we have a different window open, so that we don't start shutting
  // down just because the last window was closed.
  let swc = open_search_window();

  plan_for_window_close(mc);
  mc.window.close();
  wait_for_window_close();

  display_message_in_folder_tab(msgHdr, true);

  // We don't need the search window any more
  plan_for_window_close(swc);
  swc.window.close();
  wait_for_window_close();

  // Check that the right message is displayed
  assert_number_of_tabs_open(1);
  assert_folder_selected_and_displayed(folderC);
  assert_selected_and_displayed(msgHdr);
  // Check that row 45 is visible
  assert_row_visible(45);
}


/**
 * Initialize the globals we'll need for all our tests.
 */
function test_setup_displaying_messages_in_folder_tabs_globals() {
  // We don't obey mail view persistence unless the view picker is there.
  // XXX We used to do this in setupModule, but window.close() seems to cause
  // bad problems with the toolbar item's persistence. (This seems to be an
  // issue only in tests.)
  add_to_toolbar(mc.e("mail-bar3"), "mailviews-container");

  folderTab = mc.tabmail.currentTabInfo;

  // Stash messages into arrays for convenience. We do it this way so that the
  // order of messages in the arrays is the same as in the views.
  be_in_folder(folderA);
  for (let i = 0; i < NUM_MESSAGES_IN_FOLDER; i++)
    msgHdrsInFolderA.push(mc.dbView.getMsgHdrAt(i));
  be_in_folder(folderB);
  for (let i = 0; i < NUM_MESSAGES_IN_FOLDER; i++)
    msgHdrsInFolderB.push(mc.dbView.getMsgHdrAt(i));

  // Mark all messages read -- we need this for some tests
  folderA.markAllMessagesRead(null);
  folderB.markAllMessagesRead(null);
}

/**
 * Verifies that no new tab was opened, and that the right folder and message
 * were selected in the same tab.
 */
function _verify_display_in_existing_tab(aPreCount, aFolder, aMsgHdr) {
  // Verify that a new tab has not been opened
  assert_number_of_tabs_open(aPreCount);
  // Verify that the current tab is the folder tab
  assert_selected_tab(folderTab);
  // Verify that the correct folder is selected
  assert_folder_selected_and_displayed(aFolder);
  // Verify that the correct message is displayed
  assert_selected_and_displayed(aMsgHdr);
}

/**
 * Test displaying a message with the same folder already open.
 */
function test_display_message_in_same_folder() {
  be_in_folder(folderA);
  let preCount = mc.tabmail.tabContainer.childNodes.length;

  let msgHdr = msgHdrsInFolderA[indexes.next()];

  display_message_in_folder_tab(msgHdr);
  // Verify
  _verify_display_in_existing_tab(preCount, folderA, msgHdr);
}

/**
 * Test displaying a message with a different folder open.
 */
function test_display_message_in_different_folder() {
  be_in_folder(folderB);
  let preCount = mc.tabmail.tabContainer.childNodes.length;

  let msgHdr = msgHdrsInFolderA[indexes.next()];

  display_message_in_folder_tab(msgHdr);
  // Verify
  _verify_display_in_existing_tab(preCount, folderA, msgHdr);
}

/**
 * Test displaying a message with a message tab active.
 */
function test_display_message_in_same_folder_with_message_tab_active() {
  be_in_folder(folderA);

  let indexToOpen = indexes.next();
  select_click_row(indexToOpen);
  let messageTab = open_selected_message_in_new_tab(false);

  let preCount = mc.tabmail.tabContainer.childNodes.length;
  let msgHdr = msgHdrsInFolderA[indexes.next()];
  display_message_in_folder_tab(msgHdr);
  // Verify
  _verify_display_in_existing_tab(preCount, folderA, msgHdr);

  // Clean up, close the message tab
  close_tab(messageTab);
}

/**
 * Test displaying a message with a different folder open and a message tab
 * active.
 */
function test_display_message_in_different_folder_with_message_tab_active() {
  be_in_folder(folderA);

  let indexToOpen = indexes.next();
  select_click_row(indexToOpen);
  let messageTab = open_selected_message_in_new_tab(false);

  let preCount = mc.tabmail.tabContainer.childNodes.length;
  let msgHdr = msgHdrsInFolderB[indexes.next()];
  display_message_in_folder_tab(msgHdr);
  // Verify
  _verify_display_in_existing_tab(preCount, folderB, msgHdr);

  // Clean up, close the message tab
  close_tab(messageTab);
}

/**
 * Test displaying a message with the same folder open but filtered.
 */
function test_display_message_in_same_folder_filtered() {
  be_in_folder(folderA);
  set_mail_view(MailViewConstants.kViewItemTags, "$label1");
  // Make sure all the messages have actually disappeared
  assert_messages_not_in_view(msgHdrsInFolderA);

  let preCount = mc.tabmail.tabContainer.childNodes.length;
  let msgHdr = msgHdrsInFolderA[indexes.next()];
  display_message_in_folder_tab(msgHdr);
  // Verify
  _verify_display_in_existing_tab(preCount, folderA, msgHdr);

  // Reset the mail view
  set_mail_view(MailViewConstants.kViewItemAll, null);
}

/**
 * Test displaying a message with the folder filtered, and another folder open.
 */
function test_display_message_in_different_folder_filtered() {
  be_in_folder(folderB);
  set_mail_view(MailViewConstants.kViewItemTags, "$label1");
  // Make sure all the messages have actually disappeared
  assert_messages_not_in_view(msgHdrsInFolderB);

  // Do the same for folder A
  be_in_folder(folderA);
  set_mail_view(MailViewConstants.kViewItemTags, "$label2");
  assert_messages_not_in_view(msgHdrsInFolderA);

  let preCount = mc.tabmail.tabContainer.childNodes.length;
  let msgHdr = msgHdrsInFolderB[indexes.next()];
  display_message_in_folder_tab(msgHdr);
  // Verify
  _verify_display_in_existing_tab(preCount, folderB, msgHdr);
  // Reset folder B's state
  be_in_folder(folderB);
  set_mail_view(MailViewConstants.kViewItemAll, null);

  // Now get back to folder A, and check that its state hasn't changed
  be_in_folder(folderA);
  assert_mail_view(MailViewConstants.kViewItemTags, "$label2");
  // Reset folder A's state
  set_mail_view(MailViewConstants.kViewItemAll, null);
}

/**
 * Test displaying a message with the folder filtered and a message tab active.
 */
function test_display_message_in_same_folder_filtered_with_message_tab_active() {
  be_in_folder(folderB);

  let indexToOpen = indexes.next();
  select_click_row(indexToOpen);
  let messageTab = open_selected_message_in_new_tab(false);

  switch_tab(folderTab);
  set_mail_view(MailViewConstants.kViewItemTags, "$label1");
  // Make sure all the messages have actually disappeared
  assert_messages_not_in_view(msgHdrsInFolderB);
  switch_tab(messageTab);

  let preCount = mc.tabmail.tabContainer.childNodes.length;
  let msgHdr = msgHdrsInFolderB[indexes.next()];
  display_message_in_folder_tab(msgHdr);
  // Verify
  _verify_display_in_existing_tab(preCount, folderB, msgHdr);

  // Clean up, close the message tab and reset the mail view
  close_tab(messageTab);
  set_mail_view(MailViewConstants.kViewItemAll, null);
}

/**
 * Test displaying a message with the folder filtered, a different folder open,
 * and a message tab active.
 */
function
  test_display_message_in_different_folder_filtered_with_message_tab_active() {
  be_in_folder(folderA);
  set_mail_view(MailViewConstants.kViewItemTags, "$label1");
  // Make sure all the messages have actually disappeared
  assert_messages_not_in_view(msgHdrsInFolderA);

  be_in_folder(folderB);
  set_mail_view(MailViewConstants.kViewItemTags, "$label3");
  // There should be one message in here
  select_click_row(0);
  let messageTab = open_selected_message_in_new_tab(false);

  let preCount = mc.tabmail.tabContainer.childNodes.length;
  let msgHdr = msgHdrsInFolderA[indexes.next()];
  display_message_in_folder_tab(msgHdr);
  // Verify
  _verify_display_in_existing_tab(preCount, folderA, msgHdr);
  // Close the message tab
  close_tab(messageTab);
  // Reset folder A's state
  be_in_folder(folderA);
  set_mail_view(MailViewConstants.kViewItemAll, null);

  // Now get back to folder B, and check that its state hasn't chaned
  be_in_folder(folderB);
  assert_mail_view(MailViewConstants.kViewItemTags, "$label3");
  // Reset folder B's state
  set_mail_view(MailViewConstants.kViewItemAll, null);
}

/**
 * Test displaying a message with the folder set to show unread messages only.
 */
function test_display_message_in_same_folder_unread() {
  be_in_folder(folderB);
  set_show_unread_only(true);
  // Make sure all the messages have actually disappeared
  assert_messages_not_in_view(msgHdrsInFolderB);

  let preCount = mc.tabmail.tabContainer.childNodes.length;
  let msgHdr = msgHdrsInFolderB[indexes.next()];
  display_message_in_folder_tab(msgHdr);
  // Verify
  _verify_display_in_existing_tab(preCount, folderB, msgHdr);
  // Reset the state
  be_in_folder(folderB);
  set_show_unread_only(false);
}

/**
 * Test displaying a message with the folder set to show unread messages only,
 * and a different folder open.
 */
function test_display_message_in_different_folder_unread() {
  be_in_folder(folderA);
  set_show_unread_only(true);
  // Make sure all the messages have actually disappeared
  assert_messages_not_in_view(msgHdrsInFolderA);

  // Do the same for folder B
  be_in_folder(folderB);
  set_show_unread_only(true);
  assert_messages_not_in_view(msgHdrsInFolderB);

  let preCount = mc.tabmail.tabContainer.childNodes.length;
  let msgHdr = msgHdrsInFolderA[indexes.next()];
  display_message_in_folder_tab(msgHdr);
  // Verify
  _verify_display_in_existing_tab(preCount, folderA, msgHdr);
  // Reset folder A's state
  be_in_folder(folderA);
  set_show_unread_only(false);

  // Now get back to folder B, and check that its state hasn't changed
  be_in_folder(folderB);
  assert_showing_unread_only();
  // Reset folder B's state
  set_show_unread_only(false);
}

/**
 * Test that we correctly scroll to the index of the message in a folder.
 */
function test_display_message_scrolls_to_message() {
  be_in_folder(folderC);
  // Scroll to the top
  mc.folderDisplay.ensureRowIsVisible(0);

  let preCount = mc.tabmail.tabContainer.childNodes.length;
  let msgHdr = mc.dbView.getMsgHdrAt(45);
  display_message_in_folder_tab(msgHdr);

  // Check that row 45 is visible
  assert_row_visible(45);
  // Verify the rest
  _verify_display_in_existing_tab(preCount, folderC, msgHdr);
}

/**
 * Clean up and remove the view picker.
 */
function test_cleanup() {
  remove_from_toolbar(mc.e("mail-bar3"), "mailviews-container");
};
