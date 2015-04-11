/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test that summarization happens at the right time, that it clears itself at
 *  the right time, that it waits for selection stability when recently
 *  summarized, and that summarization does not break under tabbing.
 *
 * Because most of the legwork is done automatically by
 *  test-folder-display-helpers, the more basic tests may look like general
 *  selection / tabbing tests, but are intended to specifically exercise the
 *  summarization logic and edge cases.  (Although general selection tests and
 *  tab tests may do the same thing too...)
 *
 * Things we don't test but should:
 * - The difference between thread summary and multi-message summary.
 */

var MODULE_NAME = 'test-summarization';

var RELATIVE_ROOT = '../shared-modules';
var MODULE_REQUIRES = ['folder-display-helpers', 'address-book-helpers'];

var folder;
var thread1, thread2, msg1, msg2;

var setupModule = function(module) {
  let fdh = collector.getModule('folder-display-helpers');
  fdh.installInto(module);
  let abh = collector.getModule('address-book-helpers');
  abh.installInto(module);

  folder = create_folder("SummarizationA");
  thread1 = create_thread(10);
  msg1 = create_thread(1);
  thread2 = create_thread(10);
  msg2 = create_thread(1);
  add_sets_to_folders([folder], [thread1, msg1, thread2, msg2]);
};

function test_basic_summarization() {
  be_in_folder(folder);

  // - make sure we get a summary
  select_click_row(0);
  select_shift_click_row(5);
  // this will verify a multi-message display is happening
  assert_selected_and_displayed([0, 5]);
}

function test_summarization_goes_away() {
  select_none();
  assert_nothing_selected();
}

/**
 * Verify that we update summarization when switching amongst tabs.
 */
function test_folder_tabs_update_correctly() {
  // tab with summary
  let tabA = be_in_folder(folder);
  select_click_row(0);
  select_control_click_row(2);
  assert_selected_and_displayed(0, 2);

  // tab with nothing
  let tabB = open_folder_in_new_tab(folder);
  wait_for_blank_content_pane();
  assert_nothing_selected();

  // correct changes, none <=> summary
  switch_tab(tabA);
  assert_selected_and_displayed(0, 2);
  switch_tab(tabB);
  assert_nothing_selected();

  // correct changes, one <=> summary
  select_click_row(0);
  assert_selected_and_displayed(0);
  switch_tab(tabA);
  assert_selected_and_displayed(0, 2);
  switch_tab(tabB);
  assert_selected_and_displayed(0);

  // correct changes, summary <=> summary
  select_shift_click_row(3);
  assert_selected_and_displayed([0, 3]);
  switch_tab(tabA);
  assert_selected_and_displayed(0, 2);
  switch_tab(tabB);
  assert_selected_and_displayed([0, 3]);

  // closing tab returns state correctly...
  close_tab(tabB);
  assert_selected_and_displayed(0, 2);
}

function test_message_tabs_update_correctly() {
  let tabFolder = be_in_folder(folder);
  let message = select_click_row(0);
  assert_selected_and_displayed(0);

  let tabMessage = open_selected_message_in_new_tab();
  assert_selected_and_displayed(message);

  switch_tab(tabFolder);
  select_shift_click_row(2);
  assert_selected_and_displayed([0, 2]);

  switch_tab(tabMessage);
  assert_selected_and_displayed(message);

  switch_tab(tabFolder);
  assert_selected_and_displayed([0, 2]);

  close_tab(tabMessage);
}

/**
 * Test the stabilization logic by making the stabilization interval absurd and
 *  then manually clearing things up.
 */
function test_selection_stabilization_logic() {
  // make sure all summarization has run to completion.
  mc.sleep(0);
  // make it inconceivable that the timeout happens.
  mc.window.MessageDisplayWidget.prototype
    .SUMMARIZATION_SELECTION_STABILITY_INTERVAL_MS = 10000;
  // does not summarize anything, does not affect timer
  select_click_row(0);
  // does summarize things.  timer will be tick tick ticking!
  select_shift_click_row(1);
  // verify that things were summarized...
  assert_selected_and_displayed([0, 1]);
  // save the set of messages so we can verify the summary sticks to this.
  let messages = mc.folderDisplay.selectedMessages;

  // make sure the

  // this will not summarize!
  select_shift_click_row(2, mc, true);
  // verify that our summary is still just 0 and 1.
  assert_messages_summarized(mc, messages);

  // - put it back, the way it was
  // oh put it back the way it was
  // ...
  // That's right folks, a 'Lil Abner reference.
  // ...
  // Culture!
  // ...
  // I'm already embarassed I wrote that.
  mc.window.MessageDisplayWidget.prototype
    .SUMMARIZATION_SELECTION_STABILITY_INTERVAL_MS = 0;
  // (we did that because the stability logic is going to schedule another guard
  //  timer when we manually trigger it, and we want that to clear immediately.)

  // - pretend the timer fired.
  // we need to de-schedule the timer, but do not need to clear the variable
  //  because it will just get overwritten anyways
  mc.window.clearTimeout(mc.messageDisplay._summaryStabilityTimeout);
  mc.messageDisplay._showSummary(true);

  // - the summary should now be up-to-date
  assert_selected_and_displayed([0, 2]);
}


function test_summarization_thread_detection() {
  select_none();
  assert_nothing_selected();
  make_display_threaded();
  select_click_row(0);
  select_shift_click_row(9);
  let messages = mc.folderDisplay.selectedMessages;
  toggle_thread_row(0);
  assert_messages_summarized(mc, messages);
  // count the number of messages represented
  assert_summary_contains_N_divs('wrappedsender', 10);
  select_shift_click_row(1);
  // this should have shifted to the multi-message view
  assert_summary_contains_N_divs('wrappedsender', 0);
  assert_summary_contains_N_divs('wrappedsubject', 2);
  select_none();
  assert_nothing_selected();
  select_click_row(1); // select a single message
  select_shift_click_row(2); // add a thread
  assert_summary_contains_N_divs('wrappedsender', 0);
  assert_summary_contains_N_divs('wrappedsubject', 2);
}

/**
 * If you are looking at a message that becomes part of a thread because of the
 *  arrival of a new message, expand the thread so you do not have the message
 *  turn into a summary beneath your feet.
 *
 * There are really two cases here:
 * - The thread gets moved because its sorted position changes.
 * - The thread does not move.
 */
function test_new_thread_that_was_not_summarized_expands() {
  be_in_folder(folder);
  make_display_threaded();

  // - create the base messages
  let [willMoveMsg, willNotMoveMsg] = make_new_sets_in_folders(
    [folder], [{count: 1}, {count: 1}]);

  // - do the non-move case
  // XXX actually, this still gets treated as a move. I don't know why...
  // select it
  select_click_row(willNotMoveMsg);
  assert_selected_and_displayed(willNotMoveMsg);

  // give it a friend...
  let [extraNonMoveMsg] = make_new_sets_in_folders(
    [folder], [{count: 1, inReplyTo: willNotMoveMsg}]);
  assert_expanded(willNotMoveMsg);
  assert_selected_and_displayed(willNotMoveMsg);

  // - do the move case
  select_click_row(willMoveMsg);
  assert_selected_and_displayed(willMoveMsg);

  // give it a friend...
  let [extraMoveMsg] = make_new_sets_in_folders(
    [folder], [{count: 1, inReplyTo: willMoveMsg}]);
  assert_expanded(willMoveMsg);
  assert_selected_and_displayed(willMoveMsg);
}

/**
 * Selecting an existing (and collapsed) thread, then add a message and make
 *  sure the summary updates.
 */
function test_summary_updates_when_new_message_added_to_collapsed_thread() {
  be_in_folder(folder);
  make_display_threaded();
  collapse_all_threads();

  // - select the thread root, thereby summarizing it
  let thread1Root = select_click_row(thread1); // this just uses the root msg
  assert_collapsed(thread1Root);
  // just the thread root should be selected
  assert_selected(thread1Root);
  // but the whole thread should be summarized
  assert_messages_summarized(mc, thread1);

  // - add a new message, make sure it's in the summary now.
  let [thread1Extra] = make_new_sets_in_folders(
                         [folder], [{count: 1, inReplyTo: thread1}]);
  let thread1All = thread1.union(thread1Extra);
  assert_selected(thread1Root);
  assert_messages_summarized(mc, thread1All);
}

function test_summary_when_multiple_identities() {
  // First half of the test, makes sure messageDisplay.js understands there's
  // only one thread
  let folder1 = create_folder("Search1");
  be_in_folder(folder1);
  let thread1 = create_thread(1);
  add_sets_to_folders([folder1], [thread1]);

  let folder2 = create_folder("Search2");
  be_in_folder(folder2);
  make_new_sets_in_folders([folder2], [{count: 1, inReplyTo: thread1}])

  let folderVirtual = create_virtual_folder([folder1, folder2], {}, true, "SearchBoth");

  // Do the needed tricks
  be_in_folder(folder1);
  select_click_row(0);
  plan_to_wait_for_folder_events("DeleteOrMoveMsgCompleted",
                                 "DeleteOrMoveMsgFailed");
  mc.window.MsgMoveMessage(folder2);
  wait_for_folder_events();

  be_in_folder(folder2);
  select_click_row(1);
  plan_to_wait_for_folder_events("DeleteOrMoveMsgCompleted",
                                 "DeleteOrMoveMsgFailed");
  mc.window.MsgMoveMessage(folder1);
  wait_for_folder_events();

  be_in_folder(folderVirtual);
  make_display_threaded();
  collapse_all_threads();

  // Assertions
  select_click_row(0);
  assert_messages_summarized(mc, mc.folderDisplay.selectedMessages);
  // Thread summary uses class wrappedsender, while multimessage summary uses
  // class author.
  assert_summary_contains_N_divs('author', 0);
  assert_summary_contains_N_divs('wrappedsender', 2);

  // Second half of the test, makes sure MultiMessageSummary groups messages
  // according to their view thread id
  let thread1 = create_thread(1);
  add_sets_to_folders([folder1], [thread1]);
  be_in_folder(folderVirtual);
  select_shift_click_row(1);

  assert_summary_contains_N_divs('author', 2);
}

function extract_first_address(thread)
{
  let headerParser = Cc["@mozilla.org/messenger/headerparser;1"]
                       .getService(Ci.nsIMsgHeaderParser);
  let addresses = {};
  let fullNames = {};
  let names = {};
  let numAddresses = headerParser.parseHeadersWithArray(
    thread1.getMsgHdr(0).mime2DecodedAuthor,
    addresses, names, fullNames);

  return {email: addresses.value[0], name: names.value[0]};
}

function check_address_name(name) {
  let htmlframe = mc.e('multimessage');
  let matches = htmlframe.contentDocument.getElementsByClassName('sender');
  if (matches[0].textContent != name)
    throw new Error("Expected to find sender named '" + name + "', found '" +
                    matches[0].textContent + "'");
}

function test_display_name_no_abook()
{
  be_in_folder(folder);

  let address = extract_first_address(thread1);
  ensure_no_card_exists(address.email);

  collapse_all_threads();
  select_click_row(thread1);

  check_address_name(address.name);
}

function test_display_name_abook()
{
  be_in_folder(folder);

  let address = extract_first_address(thread1);
  ensure_card_exists(address.email, "My Friend", true);

  collapse_all_threads();
  select_click_row(thread1);

  check_address_name("My Friend");
}

function test_display_name_abook_no_pdn()
{
  be_in_folder(folder);

  let address = extract_first_address(thread1);
  ensure_card_exists(address.email, "My Friend", false);

  collapse_all_threads();
  select_click_row(thread1);

  check_address_name(address.name);
}
