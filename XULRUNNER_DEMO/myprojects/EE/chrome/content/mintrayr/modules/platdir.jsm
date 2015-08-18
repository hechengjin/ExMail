/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/ */

const EXPORTED_SYMBOLS = ["platdir"];
const platdir = {
	getSpecialDir: function(customDirName, sysPathStringId)
	{
	  var installDir = Components.classes['@mozilla.org/file/directory_service;1']
				   .getService(Components.interfaces.nsIProperties)
				   .get(sysPathStringId, Components.interfaces.nsIFile);
	  if(customDirName) installDir.append(customDirName);
	  return (installDir && installDir.exists()) ? installDir : null;
	}
};