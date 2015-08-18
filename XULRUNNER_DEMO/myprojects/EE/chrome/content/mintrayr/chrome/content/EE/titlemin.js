/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/ */
//Components.utils.import("chrome://EE/content/mintrayr/modules/preferences.jsm");

var gMinTrayR = {};
addEventListener(
  'load',
  function() {
    removeEventListener("load", arguments.callee, true);
    try{
      Components.utils.import("chrome://EE/content/mintrayr/modules/mintrayr.jsm", gMinTrayR);
    }
    
    catch(e)
    {
      alert(e)
    }
    gMinTrayR = new gMinTrayR.MinTrayR(
      document.getElementById("MinTrayR_context"),
      "mozapps.watchee",
      function() {}
      );
  },
  true
);
