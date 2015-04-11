/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/**
 * Authentication tests for NNTP (based on RFC4643) - checks for servers whose
 * details have changed (e.g. realhostname is different from hostname).
 *
 * Note: Logins for newsgroup servers for 1.8 were stored with either the
 * default port or the SSL default port. Nothing else!
 */

// The basic daemon to use for testing nntpd.js implementations
var daemon = setupNNTPDaemon();

// Define these up here for checking with the transaction
var type = null;
var test = null;

function run_test() {
  // These preferences set up a local news server that has had its hostname
  // and username changed from the original settings. We can't do this by
  // function calls for this test as they would cause the password to be
  // forgotten when changing the hostname/username and this breaks the test.
  Services.prefs.setCharPref("mail.account.account1.server", "server1");
  Services.prefs.setCharPref("mail.account.account2.server", "server2");
  Services.prefs.setCharPref("mail.account.account2.identities", "id1");
  Services.prefs.setCharPref("mail.accountmanager.accounts",
                             "account1,account2");
  Services.prefs.setCharPref("mail.accountmanager.localfoldersserver",
                             "server1");
  Services.prefs.setCharPref("mail.accountmanager.defaultaccount",
                             "account2");
  Services.prefs.setCharPref("mail.identity.id1.fullName", "testnntp");
  Services.prefs.setCharPref("mail.identity.id1.useremail",
                             "testnntp@localhost");
  Services.prefs.setBoolPref("mail.identity.id1.valid", true);
  Services.prefs.setCharPref("mail.server.server1.directory-rel",
                             "[ProfD]Mail/Local Folders");
  Services.prefs.setCharPref("mail.server.server1.hostname",
                             "Local Folders");
  Services.prefs.setCharPref("mail.server.server1.name", "Local Folders");
  Services.prefs.setCharPref("mail.server.server1.type", "none");
  Services.prefs.setCharPref("mail.server.server1.userName", "nobody");
  Services.prefs.setCharPref("mail.server.server2.directory-rel",
                             "[ProfD]Mail/invalid");
  Services.prefs.setCharPref("mail.server.server2.hostname", "invalid");
  Services.prefs.setCharPref("mail.server.server2.name",
                             "testnntp on localhost");
  Services.prefs.setIntPref("mail.server.server2.port", NNTP_PORT);
  Services.prefs.setCharPref("mail.server.server2.realhostname",
                             "localhost");
  Services.prefs.setCharPref("mail.server.server2.type", "nntp");

  type = "RFC 4643";

  // Passwords File (generated from Mozilla 1.8 branch).
  var signons = do_get_file("../../../data/signons-mailnews1.8-alt.txt");

  // Copy the file to the profile directory
  signons.copyTo(gProfileDir, "signons.txt");

  var server = makeServer(NNTP_RFC4643_extension, daemon);
  server.start(NNTP_PORT);

  try {
    // Note, the uri is for hostname "invalid" which is the original uri. See 
    // setupProtocolTest parameters.
    var prefix = "news://invalid:"+NNTP_PORT+"/";

    // Test - group subscribe listing
    test = "news:*";

    // Get the existing incoming server
    var acctMgr = Cc["@mozilla.org/messenger/account-manager;1"]
      .getService(Ci.nsIMsgAccountManager);

    acctMgr.LoadAccounts();

    // Create the incoming server with "original" details.
    var incomingServer = acctMgr.getIncomingServer("server2");

    subscribeServer(incomingServer);

    // Now set up and run the tests
    setupProtocolTest(NNTP_PORT, prefix+"*", incomingServer);
    server.performTest();
    var transaction = server.playTransaction();
    do_check_transaction(transaction, ["MODE READER", "LIST",
                                       "AUTHINFO user testnews",
                                       "AUTHINFO pass newstest", "LIST"]);
    incomingServer.QueryInterface(Components.interfaces.nsISubscribableServer)
                  .subscribeCleanup();

  } catch (e) {
    dump("NNTP Protocol test "+test+" failed for type RFC 977:\n");
    try {
      var trans = server.playTransaction();
     if (trans)
        dump("Commands called: "+trans.them+"\n");
    } catch (exp) {}
    do_throw(e);
  }
  server.stop();

  var thread = gThreadManager.currentThread;
  while (thread.hasPendingEvents())
    thread.processNextEvent(true);
}
