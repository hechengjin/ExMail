/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/**
 * Authentication tests for SMTP.
 */

load("../../../resources/alertTestUtils.js");

var gNewPassword = null;

function confirmEx(aDialogTitle, aText, aButtonFlags, aButton0Title,
                   aButton1Title, aButton2Title, aCheckMsg, aCheckState) {
  // Just return 2 which will is pressing button 2 - enter a new password.
  return 2;
}

function promptPasswordPS(aParent, aDialogTitle, aText, aPassword,
                          aCheckMsg, aCheckState) {
  aPassword.value = gNewPassword;
  return true;
}

var server;

const kSender = "from@invalid.com";
const kTo = "to@invalid.com";
const kUsername = "test.smtp@fakeserver";
// kPassword 2 is the one defined in signons-mailnews1.8.txt, the other one
// is intentionally wrong.
const kPassword1 = "wrong";
const kPassword2 = "smtptest";

function run_test() {
  registerAlertTestUtils();

  function createHandler(d) {
    var handler = new SMTP_RFC2821_handler(d);
    // Username needs to match signons.txt
    handler.kUsername = kUsername;
    handler.kPassword = kPassword1;
    handler.kAuthRequired = true;
    handler.kAuthSchemes = [ "PLAIN", "LOGIN" ]; // make match expected transaction below
    return handler;
  }

  server = setupServerDaemon(createHandler);
  server.setDebugLevel(fsDebugAll);

  // Passwords File (generated from Mozilla 1.8 branch).
  var signons = do_get_file("data/signons-smtp.txt");

  // Copy the file to the profile directory for a PAB
  signons.copyTo(gProfileDir, "signons.txt");

  // Test file
  var testFile = do_get_file("data/message1.eml");

  // Ensure we have at least one mail account
  loadLocalMailAccount();

  var smtpServer = getBasicSmtpServer();
  var identity = getSmtpIdentity(kSender, smtpServer);

  var smtpService = Cc["@mozilla.org/messengercompose/smtp;1"]
                      .getService(Ci.nsISmtpService);

  // Handle the server in a try/catch/finally loop so that we always will stop
  // the server if something fails.
  try {
    // Start the fake SMTP server
    server.start(SMTP_PORT);

    // This time with auth
    test = "Auth sendMailMessage";

    smtpServer.authMethod = Ci.nsMsgAuthMethod.passwordCleartext;
    smtpServer.socketType = Ci.nsMsgSocketType.plain;
    smtpServer.username = kUsername;

    smtpService.sendMailMessage(testFile, kTo, identity,
                                null, null, null, null,
                                false, {}, {});

    // Set the new password for when we get a prompt
    gNewPassword = kPassword1;

    server.performTest();

    var transaction = server.playTransaction();
    do_check_transaction(transaction, ["EHLO test",
                                       "AUTH PLAIN " + AuthPLAIN.encodeLine(kUsername, kPassword2),
                                       "AUTH LOGIN",
                                       "AUTH PLAIN " + AuthPLAIN.encodeLine(kUsername, kPassword1),
                                       "MAIL FROM:<" + kSender + "> SIZE=155",
                                       "RCPT TO:<" + kTo + ">",
                                       "DATA"]);

  } catch (e) {
    do_throw(e);
  } finally {
    server.stop();
  
    var thread = gThreadManager.currentThread;
    while (thread.hasPendingEvents())
      thread.processNextEvent(true);
  }
}
