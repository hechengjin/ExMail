/**
 * This test checks to see if the nntp password failure is handled correctly.
 * The steps are:
 *   - Have an invalid password in the password database.
 *   - Check we get a prompt asking what to do.
 *   - Check retry does what it should do.
 *   - Check cancel does what it should do.
 *   - Re-initiate connection, this time select enter new password, check that
 *     we get a new password prompt and can enter the password.
 */

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource:///modules/mailServices.js");
Components.utils.import("resource://gre/modules/Services.jsm");
load("../../../resources/logHelper.js");
load("../../../resources/asyncTestUtils.js");
load("../../../resources/alertTestUtils.js");

var test = null;
var server;
var daemon;
var incomingServer;
var folder;
var attempt = 0;
var loginMgr = Cc["@mozilla.org/login-manager;1"].getService(Ci.nsILoginManager);
var count = {};
var logins;

const kUserName = "testnews";
const kInvalidPassword = "newstest";
const kValidPassword = "notallama";

function alert(aDialogText, aText)
{
  // The first few attempts may prompt about the password problem, the last
  // attempt shouldn't.
  do_check_true(attempt < 4);

  // Log the fact we've got an alert, but we don't need to test anything here.
  dump("Alert Title: " + aDialogText + "\nAlert Text: " + aText + "\n");
}

function confirmEx(aDialogTitle, aText, aButtonFlags, aButton0Title,
                   aButton1Title, aButton2Title, aCheckMsg, aCheckState) {
  switch (++attempt) {
    // First attempt, retry.
    case 1:
      dump("\nAttempting retry\n");
      return 0;
    // Second attempt, cancel.
    case 2:
      dump("\nCancelling login attempt\n");
      return 1;
    // Third attempt, retry.
    case 3:
      dump("\nAttempting Retry\n");
      return 0;
    // Fourth attempt, enter a new password.
    case 4:
      dump("\nEnter new password\n");
      return 2;
    default:
      do_throw("unexpected attempt number " + attempt);
      return 1;
  }
}

function promptUsernameAndPasswordPS(aParent, aDialogTitle, aText, aUsername,
                                     aPassword, aCheckMsg, aCheckState) {
  if (attempt == 4) {
    aUsername.value = kUserName;
    aPassword.value = kValidPassword;
    aCheckState.value = true;
    return true;
  }
  return false;
}

function getMail() {
  folder.getNewMessages(gDummyMsgWindow, urlListener);
}

var urlListener =
{
  OnStartRunningUrl: function (url) {
  },
  OnStopRunningUrl: function (url, result) {
    try {
      // On the last attempt, we should have successfully got one mail.
      do_check_eq(folder.getTotalMessages(false),
                  attempt == 4 ? 1 : 0);

      // If we've just cancelled, expect failure rather than success
      // because the server dropped the connection.
      dump("in onStopRunning, result = " + result + "\n");
      //do_check_eq(result, attempt == 2 ? Cr.NS_ERROR_FAILURE : 0);
      async_driver();
    }
    catch (e) {
      // If we have an error, clean up nicely before we throw it.
      server.stop();

      var thread = gThreadManager.currentThread;
      while (thread.hasPendingEvents())
        thread.processNextEvent(true);

      do_throw(e);
    }
  }
};

// Definition of tests
var tests = [
  getMail1,
  getMail2,
  endTest
]

function getMail1() {
  dump("\nGet Mail 1\n");

  // Now get mail
  getMail();
  yield false;
  dump("\nGot Mail 1\n");

  do_check_eq(attempt, 2);

  // Check that we haven't forgotten the login even though we've retried and
  // canceled.
  logins = loginMgr.findLogins(count, "news://localhost", null,
                                   "news://localhost");

  do_check_eq(count.value, 1);
  do_check_eq(logins[0].username, kUserName);
  do_check_eq(logins[0].password, kInvalidPassword);

  server.resetTest();
  yield true;
}

function getMail2() {
  dump("\nGet Mail 2\n");

  // Now get the mail
  getMail();
  yield false;
  dump("\nGot Mail 2\n");
}

function endTest() {
  // Now check the new one has been saved.
  logins = loginMgr.findLogins(count, "news://localhost", null,
                               "news://localhost");

  do_check_eq(count.value, 1);
  do_check_eq(logins[0].username, kUserName);
  do_check_eq(logins[0].password, kValidPassword);
  yield true;
}

function run_test()
{
  // Disable new mail notifications
  Services.prefs.setBoolPref("mail.biff.play_sound", false);
  Services.prefs.setBoolPref("mail.biff.show_alert", false);
  Services.prefs.setBoolPref("mail.biff.show_tray_icon", false);
  Services.prefs.setBoolPref("mail.biff.animate_dock_icon", false);
  Services.prefs.setBoolPref("signon.debug", true);

  // Passwords File (generated from Mozilla 1.8 branch).
  let signons = do_get_file("../../../data/signons-mailnews1.8.txt");

  // Copy the file to the profile directory for a PAB
  signons.copyTo(gProfileDir, "signons.txt");

  registerAlertTestUtils();

  // Set up the server
  daemon = setupNNTPDaemon();
  function createHandler(d) {
    var handler = new NNTP_RFC4643_extension(d);
    handler.expectedPassword = kValidPassword;
    return handler;
  }
  server = new nsMailServer(createHandler, daemon);
  incomingServer = setupLocalServer(NNTP_PORT);
  folder = incomingServer.rootFolder.getChildNamed("test.subscribe.simple");

  // Check that we haven't got any messages in the folder, if we have its a test
  // setup issue.
  do_check_eq(folder.getTotalMessages(false), 0);

  server.start(NNTP_PORT);
  server.setDebugLevel(fsDebugAll);
  async_run_tests(tests);
}
