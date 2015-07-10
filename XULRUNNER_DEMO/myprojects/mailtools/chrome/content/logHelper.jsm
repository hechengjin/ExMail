 "use strict";

 const EXPORTED_SYMBOLS = [ "logHelper" ];


var logHelper = {

    writelog: function(data)
   {
	 var observerService = Components.classes["@mozilla.org/observer-service;1"]
                .getService(Components.interfaces.nsIObserverService);
          observerService.notifyObservers(null, "addlog", data);

   },



};


