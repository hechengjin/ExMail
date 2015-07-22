function doLockClient()
{
    var observerService = Components.classes["@mozilla.org/observer-service;1"]
                          .getService(Components.interfaces.nsIObserverService);
    observerService.notifyObservers(null, "doLockClient", null); 
}