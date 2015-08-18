/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/ */

const EXPORTED_SYMBOLS = ["platlog"];
const platlog = {
  dumpMsg: function(msgStr,show) {
    Components.classes['@mozilla.org/consoleservice;1'].getService(Components.interfaces.nsIConsoleService).logStringMessage(msgStr);
    if( show!= undefined && show )
    {

  		var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                    .getService(Components.interfaces.nsIPromptService);
		promptService.alert(null, 'info', msgStr);
    }
  }
};