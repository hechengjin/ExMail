/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * Test suite for auto-detecting attachment file charset.
 */

load("../../../resources/logHelper.js");
load("../../../resources/asyncTestUtils.js");

Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource:///modules/mailServices.js");

function checkAttachmentCharset(expectedCharset) {
  let msgData = loadMessageToString(gDraftFolder, firstMsgHdr(gDraftFolder));
  let attachmentData = getAttachmentFromContent(msgData);

  do_check_eq(expectedCharset, getContentCharset(attachmentData));
}

function getContentCharset(aContent) {
  let found = aContent.match(/^Content-Type: text\/plain; charset=(.*?);/);
  do_check_neq(found, null);
  do_check_eq(found.length, 2);

  return found[1];
}

function testUTF8() {
  yield async_run({ func: createMessage, args: [do_get_file("data/test-UTF-8.txt")] });
  checkAttachmentCharset("UTF-8");
}

function testUTF16() {
  yield async_run({ func: createMessage, args: [do_get_file("data/test-UTF-16.txt")] });
  checkAttachmentCharset("UTF-16");
}

function testShiftJIS() {
  yield async_run({ func: createMessage, args: [do_get_file("data/test-SHIFT_JIS.txt")] });
  checkAttachmentCharset("Shift_JIS");
}

var tests = [
  testUTF8,
  testUTF16,
  testShiftJIS,
  do_test_finished
]

function run_test() {
  // Ensure we have at least one mail account
  loadLocalMailAccount();
  Services.prefs.setIntPref("mail.strictly_mime.parm_folding", 0);

  do_test_pending();

  async_run_tests(tests);
}
