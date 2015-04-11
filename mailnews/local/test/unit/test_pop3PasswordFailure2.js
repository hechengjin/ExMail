/**
 * This test checks to see if the pop3 password failure is handled correctly.
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
load("../../../resources/mailTestUtils.js");
load("../../../resources/asyncTestUtils.js");
load("../../../resources/alertTestUtils.js");

var test = null;
var server;
var daemon;
var incomingServer;
var attempt = 0;
var loginMgr = Cc["@mozilla.org/login-manager;1"].getService(Ci.nsILoginManager);
var count = {};
var logins;

const kUserName = "testpop3";
const kInvalidPassword = "pop3test";
const kValidPassword = "testpop3";

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

function promptPasswordPS(aParent, aDialogTitle, aText, aPassword, aCheckMsg,
                          aCheckState) {
  if (attempt == 4) {
    aPassword.value = kValidPassword;
    aCheckState.value = true;
    return true;
  }
  return false;
}

function getPopMail() {
  MailServices.pop3.GetNewMail(gDummyMsgWindow, urlListener, gLocalInboxFolder,
                               incomingServer);
}

var urlListener =
{
  OnStartRunningUrl: function (url) {
  },
  OnStopRunningUrl: function (url, result) {
    try {
      // On the last attempt, we should have successfully got one mail.
      do_check_eq(gLocalInboxFolder.getTotalMessages(false),
                  attempt == 4 ? 1 : 0);

      // If we've just cancelled, expect failure rather than success
      // because the server dropped the connection.
      dump("in onStopRunning, result = " + result + "\n");
      do_check_eq(result, attempt == 2 ? Cr.NS_ERROR_FAILURE : 0);
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

function actually_run_test() {
  server.start(POP3_PORT);
  daemon.setMessages(["message1.eml"]);
  async_run_tests(tests);
}

function getMail1() {
  dump("\nGet Mail 1\n");

  // Now get mail
  getPopMail();
  yield false;
  dump("\nGot Mail 1\n");

  do_check_eq(attempt, 2);

  // Check that we haven't forgetton the login even though we've retried and
  // canceled.
  logins = loginMgr.findLogins(count, "mailbox://localhost", null,
                                   "mailbox://localhost");

  do_check_eq(count.value, 1);
  do_check_eq(logins[0].username, kUserName);
  do_check_eq(logins[0].password, kInvalidPassword);

  server.resetTest();
  yield true;
}

function getMail2() {
  dump("\nGet Mail 2\n");

  // Now get the mail
  getPopMail();
  yield false;
  dump("\nGot Mail 2\n");
}

function endTest() {
  // Now check the new one has been saved.
  logins = loginMgr.findLogins(count, "mailbox://localhost", null,
                               "mailbox://localhost");

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

  // Set up the Server
  daemon = new pop3Daemon();
  function createHandler(d) {
    var handler = new POP3_RFC1939_handler(d);
    handler.dropOnAuthFailure = true;

    // Set the server expected username & password to what we have in signons.txt
    handler.kUsername = kUserName;
    handler.kPassword = kValidPassword;
    return handler;
  }
  server = new nsMailServer(createHandler, daemon);

  // Set up the basic accounts and folders.
  // We would use createPop3ServerAndLocalFolders() however we want to have
  // a different username and NO password for this test (as we expect to load
  // it from signons.txt).
  loadLocalMailAccount();

  incomingServer = MailServices.accounts
                    .createIncomingServer(kUserName, "localhost", "pop3");
  incomingServer.port = POP3_PORT;

  // Check that we haven't got any messages in the folder, if we have its a test
  // setup issue.
  do_check_eq(gLocalInboxFolder.getTotalMessages(false), 0);

  actually_run_test();
}
