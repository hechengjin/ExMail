Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/FileUtils.jsm");
var addlogListener = {
  observe: function(subject, topic, data) {
    if (topic == "addlog") {
      gmainDialog.addlog(data);
    }
  }
}

var gmainDialog = {
  gpref: null,
  greslutsloginfos:null,
   
  onLoad: function ()
  {
    setFrame();
    this.greslutsloginfos = "";
     var observerService = Components.classes["@mozilla.org/observer-service;1"]
        .getService(Components.interfaces.nsIObserverService);
    observerService.addObserver(addlogListener, "addlog", false);

  /* ///////////////
    this.gpref = Components.classes['@mozilla.org/preferences-service;1'].
    getService(Components.interfaces.nsIPrefService);
    if (this.gpref)
         {
             //var filePath  = this.srcPath +"\\test\\prefs.js";
             var file = FileUtils.getDir("ProfD", []);
            //file.append("mailtoolspref.js");
            file.append("prefs.js");
            //file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, FileUtils.PERMS_FILE);
             //var filePath  = "prefs.js";
             // var file  = new FileUtils.File(filePath);
            if (!file.exists()) {    // 若不存在，先创建一个空文件
              alert(filePath + "文件不存在!");
             // this.reslutsinfos +=   "\n----ERROR:"+filePath + "文件不存在!";
              return;
              }      
                this.gpref.readUserPrefs(file);
                this.gpref = this.gpref.getBranch(null);

                if (this.gpref)
                 {
                    var setok = this.gpref.prefHasUserValue("version.release.flag");  
                    if( setok )
                    {
                      //alert(1)
                      this.gpref.setBoolPref("version.release.flag",false);
                      
                    }
                    else
                    {
                      //alert(2)
                      this.gpref.setBoolPref("version.release.flag",false);
                     // alert(3)
                    }
                 }
        }
        /////*/

  },
  clearLog: function ()
  {
    this.greslutsloginfos = "";
    results = document.getElementById("gresults");
    if (results)
      results.value = this.greslutsloginfos;
  },

  addlog: function (loginfo)
  {
    this.greslutsloginfos += loginfo;
    results = document.getElementById("gresults");
    //中文会出现乱码？
    if (results)
      results.value = this.greslutsloginfos;
  },
 };




