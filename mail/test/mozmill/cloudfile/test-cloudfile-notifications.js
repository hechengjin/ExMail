/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
  * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Tests the get an account workflow.
 */

let MODULE_NAME = 'test-cloudfile-notifications';

let RELATIVE_ROOT = '../shared-modules';
let MODULE_REQUIRES = ['folder-display-helpers',
                       'compose-helpers',
                       'cloudfile-helpers',
                       'attachment-helpers',
                       'prompt-helpers'];

let controller = {};
let mozmill = {};
let elib = {};
Cu.import('resource://mozmill/modules/controller.js', controller);
Cu.import('resource://mozmill/modules/mozmill.js', mozmill);
Cu.import('resource://mozmill/modules/elementslib.js', elib);
Cu.import('resource://gre/modules/Services.jsm');

let maxSize, cfh, ah, oldInsertNotificationPref;

const kOfferThreshold = "mail.compose.big_attachments.threshold_kb";
const kInsertNotificationPref = "mail.compose.big_attachments.insert_notification";

function setupModule(module) {
  let fdh = collector.getModule('folder-display-helpers');
  fdh.installInto(module);

  let ch = collector.getModule('compose-helpers');
  ch.installInto(module);

  cfh = collector.getModule('cloudfile-helpers');
  cfh.installInto(module);
  cfh.gMockCloudfileManager.register();

  ah = collector.getModule('attachment-helpers');
  ah.installInto(module);
  ah.gMockFilePickReg.register();

  collector.getModule('prompt-helpers').installInto(module);

  maxSize = Services.prefs.getIntPref(kOfferThreshold, 0) * 1024;
  oldInsertNotificationPref = Services.prefs
                                      .getBoolPref(kInsertNotificationPref);
  Services.prefs.setBoolPref(kInsertNotificationPref, true);
};

function teardownModule(module) {
  cfh.gMockCloudfileManager.unregister();
  ah.gMockFilePickReg.unregister();
  Services.prefs.setBoolPref(kInsertNotificationPref,
                             oldInsertNotificationPref);
}

/**
 * A helper function to assert that the Filelink offer notification is
 * either displayed or not displayed.
 *
 * @param aController the controller of the compose window to check.
 * @param aDisplayed true if the notification should be displayed, false
 *                   otherwise.
 */
function assert_cloudfile_notification_displayed(aController, aDisplayed) {
  assert_notification_displayed(aController, "bigAttachment", aDisplayed);
}

/**
 * A helper function to assert that the Filelink upload notification is
 * either displayed or not displayed.
 *
 * @param aController the controller of the compose window to check.
 * @param aDisplayed true if the notification should be displayed, false
 *                   otherwise.
 */
function assert_upload_notification_displayed(aController, aDisplayed) {
  assert_notification_displayed(aController, "bigAttachmentUploading",
                                aDisplayed);
}

/**
 * A helper function to assert that the Filelink privacy warning notification
 * is either displayed or not displayed.
 *
 * @param aController the controller of the compose window to check.
 * @param aDisplayed true if the notification should be displayed, false
 *                   otherwise.
 */
function assert_privacy_warning_notification_displayed(aController, aDisplayed) {
  assert_notification_displayed(aController, "bigAttachmentPrivacyWarning",
                                aDisplayed);
}

/**
 * A helper function to close the Filelink upload notification.
 */
function close_upload_notification(aController) {
  close_notification(aController, "bigAttachmentUploading");
}

/**
 * A helper function to close the Filelink privacy warning notification.
 */
function close_privacy_warning_notification(aController) {
  close_notification(aController, "bigAttachmentPrivacyWarning");
}

function test_no_notification_for_small_file() {
  let cwc = open_compose_new_mail(mc);
  add_attachments(cwc, "http://www.example.com/1", 0);
  assert_cloudfile_notification_displayed(cwc, false);

  add_attachments(cwc, "http://www.example.com/2", 1);
  assert_cloudfile_notification_displayed(cwc, false);

  add_attachments(cwc, "http://www.example.com/3", 100);
  assert_cloudfile_notification_displayed(cwc, false);

  add_attachments(cwc, "http://www.example.com/4", 500);
  assert_cloudfile_notification_displayed(cwc, false);
}

function test_notification_for_big_files() {
  let cwc = open_compose_new_mail(mc);
  add_attachments(cwc, "http://www.example.com/1", maxSize);
  assert_cloudfile_notification_displayed(cwc, true);

  add_attachments(cwc, "http://www.example.com/2", maxSize + 1000);
  assert_cloudfile_notification_displayed(cwc, true);

  add_attachments(cwc, "http://www.example.com/3", maxSize + 10000);
  assert_cloudfile_notification_displayed(cwc, true);

  add_attachments(cwc, "http://www.example.com/4", maxSize + 100000);
  assert_cloudfile_notification_displayed(cwc, true);
}

function test_graduate_to_notification() {
  let cwc = open_compose_new_mail(mc);
  add_attachments(cwc, "http://www.example.com/1", maxSize - 100);
  assert_cloudfile_notification_displayed(cwc, false);

  add_attachments(cwc, "http://www.example.com/2", maxSize - 25);
  assert_cloudfile_notification_displayed(cwc, false);

  add_attachments(cwc, "http://www.example.com/3", maxSize);
  assert_cloudfile_notification_displayed(cwc, true);
}

function test_no_notification_if_disabled() {
  let cwc = open_compose_new_mail(mc);

  Services.prefs.setBoolPref("mail.cloud_files.enabled", false);
  add_attachments(cwc, "http://www.example.com/1", maxSize);
  assert_cloudfile_notification_displayed(cwc, false);

  add_attachments(cwc, "http://www.example.com/2", maxSize + 1000);
  assert_cloudfile_notification_displayed(cwc, false);

  add_attachments(cwc, "http://www.example.com/3", maxSize + 10000);
  assert_cloudfile_notification_displayed(cwc, false);

  add_attachments(cwc, "http://www.example.com/4", maxSize + 100000);
  assert_cloudfile_notification_displayed(cwc, false);

  Services.prefs.setBoolPref("mail.cloud_files.enabled", true);
}

/**
 * Tests that if we upload a single file, we get the link insertion
 * notification bar displayed (unless preffed off).
 */
function test_link_insertion_notification_single() {
  gMockFilePicker.returnFiles = collectFiles(['./data/testFile1'], __file__);
  let provider = new MockCloudfileAccount();
  provider.init("aKey");

  let cwc = open_compose_new_mail(mc);
  cwc.window.attachToCloud(provider);

  assert_upload_notification_displayed(cwc, true);
  close_upload_notification(cwc);

  Services.prefs.setBoolPref(kInsertNotificationPref, false);
  gMockFilePicker.returnFiles = collectFiles(['./data/testFile2'], __file__);
  cwc.window.attachToCloud(provider);
  assert_upload_notification_displayed(cwc, false);
  Services.prefs.setBoolPref(kInsertNotificationPref, true);
}

/**
 * Tests that if we upload multiple files, we get the link insertion
 * notification bar displayed (unless preffed off).
 */
function test_link_insertion_notification_multiple() {
  gMockFilePicker.returnFiles = collectFiles(['./data/testFile1',
                                              './data/testFile2'], __file__);
  let provider = new MockCloudfileAccount();
  provider.init("aKey");

  let cwc = open_compose_new_mail(mc);
  cwc.window.attachToCloud(provider);

  assert_upload_notification_displayed(cwc, true);
  close_upload_notification(cwc);

  Services.prefs.setBoolPref(kInsertNotificationPref, false);
  gMockFilePicker.returnFiles = collectFiles(['./data/testFile3',
                                              './data/testFile4'], __file__);
  cwc.window.attachToCloud(provider);
  assert_upload_notification_displayed(cwc, false);
  Services.prefs.setBoolPref(kInsertNotificationPref, true);
}

/**
 * Tests that the link insertion notification bar goes away even
 * if we hit an uploading error.
 */
function test_link_insertion_goes_away_on_error() {
  gMockPromptService.register();
  gMockPromptService.returnValue = false;
  gMockFilePicker.returnFiles = collectFiles(['./data/testFile1',
                                              './data/testFile2'], __file__);
  let provider = new MockCloudfileAccount();
  provider.init("aKey");

  provider.uploadFile = function(aFile, aListener) {
    aListener.onStartRequest(null, null);
    cwc.window.setTimeout(function() {
      aListener.onStopRequest(null, null,
                              Ci.nsIMsgCloudFileProvider.uploadErr);
    }, 500);
  }
  let cwc = open_compose_new_mail(mc);
  cwc.window.attachToCloud(provider);

  assert_upload_notification_displayed(cwc, true);
  wait_for_notification_to_stop(cwc, "bigAttachmentUploading");
  gMockPromptService.unregister();
}

/**
 * Test that we do not show the Filelink offer notification if we convert
 * a Filelink back into a normal attachment.
 */
function test_no_offer_on_conversion() {
  const kFiles = ['./data/testFile1', './data/testFile2'];
  // Set the notification threshold to 0 to ensure that we get it.
  Services.prefs.setIntPref(kOfferThreshold, 0);

  // Insert some Filelinks...
  gMockFilePicker.returnFiles = collectFiles(kFiles, __file__);
  let provider = new MockCloudfileAccount();
  provider.init("someKey");

  // Override uploadFile to succeed instantaneously so that we don't have
  // to worry about waiting for the onStopRequest method being called
  // asynchronously.
  provider.uploadFile = function(aFile, aListener) {
    aListener.onStartRequest(null, null);
    aListener.onStopRequest(null, null, Components.results.NS_OK);
  };

  let cw = open_compose_new_mail();
  cw.window.attachToCloud(provider);
  assert_cloudfile_notification_displayed(cw, false);
  // Now convert the file back into a normal attachment
  select_attachments(cw, 0);
  cw.window.convertSelectedToRegularAttachment();

  assert_cloudfile_notification_displayed(cw, false);

  // Now put the old threshold back.
  Services.prefs.setIntPref(kOfferThreshold, maxSize);
}

/**
 * Test that when we kick off an upload via the offer notification, then
 * the upload notification is shown.
 */
function test_offer_then_upload_notifications() {
  const kFiles = ['./data/testFile1', './data/testFile2'];
  // Set the notification threshold to 0 to ensure that we get it.
  Services.prefs.setIntPref(kOfferThreshold, 0);

  // We're going to add attachments to the attachmentbucket, and we'll
  // use the add_attachments helper function to do it.  First, retrieve
  // some file URIs...
  let fileURIs = [Services.io.newFileURI(file).spec
                  for each (file in collectFiles(kFiles, __file__))];

  // Create our mock provider
  let provider = new MockCloudfileAccount();
  provider.init("someKey");

  // Override uploadFile to succeed instantaneously so that we don't have
  // to worry about waiting for the onStopRequest method being called
  // asynchronously.
  provider.uploadFile = function(aFile, aListener) {
    aListener.onStartRequest(null, null);
    aListener.onStopRequest(null, null, Components.results.NS_OK);
  };

  let cw = open_compose_new_mail();

  // Attach the files, saying that each is 500 bytes large - which should
  // certainly trigger the offer.
  add_attachments(cw, fileURIs, [500, 500]);
  // Assert that the offer is displayed.
  assert_cloudfile_notification_displayed(cw, true);
  // Select both attachments in the attachmentbucket, and choose to convert
  // them.
  select_attachments(cw, 0, 1);
  // Convert them.
  cw.window.convertSelectedToCloudAttachment(provider);

  // The offer should now be gone...
  assert_cloudfile_notification_displayed(cw, false);
  // And the upload notification should be displayed.
  assert_upload_notification_displayed(cw, true);

  // Now put the old threshold back.
  Services.prefs.setIntPref(kOfferThreshold, maxSize);
}

/**
 * Test that when we first upload some files, we get the privacy warning
 * message. We should only get this the first time.
 */
function test_privacy_warning_notification() {
  gMockPromptService.register();
  gMockPromptService.returnValue = false;
  gMockFilePicker.returnFiles = collectFiles(['./data/testFile1',
                                              './data/testFile2'], __file__);
  let provider = new MockCloudfileAccount();
  provider.init("aKey");

  provider.uploadFile = function(aFile, aListener) {
    aListener.onStartRequest(null, null);
    cwc.window.setTimeout(function() {
      aListener.onStopRequest(null, null,
                              Ci.nsIMsgCloudFileProvider.NS_OK);
    }, 500);
  }
  let cwc = open_compose_new_mail(mc);
  cwc.window.attachToCloud(provider);

  assert_upload_notification_displayed(cwc, true);
  wait_for_notification_to_stop(cwc, "bigAttachmentUploading");

  // Assert that the warning is displayed.
  assert_privacy_warning_notification_displayed(cwc, true);

  // Close the privacy warning notification...
  close_privacy_warning_notification(cwc);

  // And now upload some more files. We shouldn't get the warning again.
  gMockFilePicker.returnFiles = collectFiles(['./data/testFile3',
                                              './data/testFile4'], __file__);
  cwc.window.attachToCloud(provider);
  assert_privacy_warning_notification_displayed(cwc, false);
  gMockPromptService.unregister();
}

/**
 * Test that the privacy warning notification does not persist when closing
 * and re-opening a compose window.
 */
function test_privacy_warning_notification_no_persist() {
  gMockPromptService.register();
  gMockPromptService.returnValue = false;
  gMockFilePicker.returnFiles = collectFiles(['./data/testFile1',
                                              './data/testFile2'], __file__);
  let provider = new MockCloudfileAccount();
  provider.init("aKey");

  provider.uploadFile = function(aFile, aListener) {
    aListener.onStartRequest(null, null);
    cwc.window.setTimeout(function() {
      aListener.onStopRequest(null, null,
                              Ci.nsIMsgCloudFileProvider.NS_OK);
    }, 500);
  }
  let cwc = open_compose_new_mail(mc);
  cwc.window.attachToCloud(provider);

  assert_upload_notification_displayed(cwc, true);
  wait_for_notification_to_stop(cwc, "bigAttachmentUploading");

  // Assert that the warning is displayed.
  assert_privacy_warning_notification_displayed(cwc, true);

  // Close the compose window
  close_compose_window(cwc);

  // Open a new compose window
  cwc = open_compose_new_mail(mc);

  // We shouldn't be displaying the privacy warning.
  assert_privacy_warning_notification_displayed(cwc, false);
  gMockPromptService.unregister();
}

/**
 * Test that if we close the privacy warning in a composer, it will still
 * spawn in a new one.
 */
function test_privacy_warning_notification_open_after_close() {
  gMockPromptService.register();
  gMockPromptService.returnValue = false;
  gMockFilePicker.returnFiles = collectFiles(['./data/testFile1',
                                              './data/testFile2'], __file__);
  let provider = new MockCloudfileAccount();
  provider.init("aKey");

  provider.uploadFile = function(aFile, aListener) {
    aListener.onStartRequest(null, null);
    cwc.window.setTimeout(function() {
      aListener.onStopRequest(null, null,
                              Ci.nsIMsgCloudFileProvider.NS_OK);
    }, 500);
  }
  let cwc = open_compose_new_mail(mc);
  cwc.window.attachToCloud(provider);

  assert_upload_notification_displayed(cwc, true);
  wait_for_notification_to_stop(cwc, "bigAttachmentUploading");

  // Assert that the warning is displayed.
  assert_privacy_warning_notification_displayed(cwc, true);

  // Close the privacy warning notification...
  close_privacy_warning_notification(cwc);

  // Open a new compose window
  cwc = open_compose_new_mail(mc);

  gMockFilePicker.returnFiles = collectFiles(['./data/testFile3',
                                              './data/testFile4'], __file__);
  cwc.window.attachToCloud(provider);

  assert_upload_notification_displayed(cwc, true);
  wait_for_notification_to_stop(cwc, "bigAttachmentUploading");

  // Assert that the privacy warning notification is displayed again.
  assert_privacy_warning_notification_displayed(cwc, true);

  gMockPromptService.unregister();
}
