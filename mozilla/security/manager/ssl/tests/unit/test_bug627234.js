var Cc = Components.classes;
var Ci = Components.interfaces;

Components.utils.import("resource://gre/modules/Services.jsm");

var _PBSvc = null;
function get_PBSvc() {
  if (_PBSvc)
    return _PBSvc;

  try {
    _PBSvc = Cc["@mozilla.org/privatebrowsing;1"]
               .getService(Ci.nsIPrivateBrowsingService);
    return _PBSvc;
  } catch (e) {}
  return null;
}

var gSTSService = Cc["@mozilla.org/stsservice;1"]
                  .getService(Ci.nsIStrictTransportSecurityService);

function Observer() {}
Observer.prototype = {
  observe: function(subject, topic, data) {
    do_execute_soon(gNextTest);
  }
};

var gObserver = new Observer();
var gNextTest = null;

function cleanup() {
  Services.obs.removeObserver(gObserver, "private-browsing-transition-complete");
  get_PBSvc().privateBrowsingEnabled = false;
  // (we have to remove any state added to the sts service so as to not muck
  // with other tests).
  var uri = Services.io.newURI("http://localhost", null, null);
  gSTSService.removeStsState(uri);
}

function run_test() {
  let pb = get_PBSvc();
  if (pb) {
    do_test_pending();
    do_register_cleanup(cleanup);

    gNextTest = test_part1;
    Services.obs.addObserver(gObserver, "private-browsing-transition-complete", false);
    Services.prefs.setBoolPref("browser.privatebrowsing.keep_current_session", true);
    pb.privateBrowsingEnabled = true;
  }
}

function test_part1() {
  var uri = Services.io.newURI("https://localhost/img.png", null, null);
  gSTSService.processStsHeader(uri, "max-age=1000");
  do_check_true(gSTSService.isStsHost("localhost"));
  gNextTest = test_part2;
  get_PBSvc().privateBrowsingEnabled = false;
}

function test_part2() {
  var uri = Services.io.newURI("https://localhost/img.png", null, null);
  gSTSService.processStsHeader(uri, "max-age=1000");
  do_check_true(gSTSService.isStsHost("localhost"));
  gNextTest = test_part3;
  get_PBSvc().privateBrowsingEnabled = true;
}

function test_part3() {
  var uri = Services.io.newURI("https://localhost/img.png", null, null);
  gSTSService.processStsHeader(uri, "max-age=1000");
  do_check_true(gSTSService.isStsHost("localhost"));
  do_test_finished();
}
