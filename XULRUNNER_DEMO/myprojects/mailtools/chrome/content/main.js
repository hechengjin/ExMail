Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/FileUtils.jsm");
Components.utils.import("resource://gre/modules/AddonManager.jsm");

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

   IETgetPickerModeFolder: function() {
    var dir = null;
    var nsIFilePicker = Components.interfaces.nsIFilePicker;
    var fp = Components.classes["@mozilla.org/filepicker;1"].createInstance(nsIFilePicker);
    var bundle = document.getElementById("bundle_messenger");
    var filePickerExportInfo ="filePickerExport";
    fp.init(window, filePickerExportInfo, nsIFilePicker.modeGetFolder);
    var res=fp.show();
    if (res==nsIFilePicker.returnOK) {
        dir = fp.file;
        if ( ! dir.isWritable()) {
            alert("nowritable");
            dir = null;
        }
    }
    return dir;
  },
  getallfiles: function ()
  {
     var destdirNSIFILE = this.IETgetPickerModeFolder();

      //alert(destdirNSIFILE.isDirectory())

     var jsonString="";

      var dirEnum = destdirNSIFILE.directoryEntries;
       while (dirEnum.hasMoreElements()) {
           var profile = dirEnum.getNext().QueryInterface(Components.interfaces.nsIFile);
           if (profile.isFile()) {
               jsonString += profile.leafName + "\r\n";
           } else {
              continue;
           }
       }
       alert(jsonString)


      let file = Components.classes["@mozilla.org/file/directory_service;1"].
                 getService(Components.interfaces.nsIProperties).
                 get("Desk", Components.interfaces.nsIFile);

    file.append("allfileinfos.txt");
    if (!file.exists()){
        file.create(Components.interfaces.nsIFile.NORMAL_FILE_TYPE, parseInt("0600", 8));  
    } 
    
    let foStream = Components.classes["@mozilla.org/network/file-output-stream;1"].
                   createInstance(Components.interfaces.nsIFileOutputStream);
    // use 0x02 | 0x10 to open file for appending.
    foStream.init(file, 0x02 | 0x08 | 0x20, 0666, 0); 
    foStream.write(jsonString, jsonString.length);
    foStream.close();
  },

  addlog: function (loginfo)
  {
    this.greslutsloginfos += loginfo;
    results = document.getElementById("gresults");
    //中文会出现乱码？
    if (results)
      results.value = this.greslutsloginfos;
  },

  openExtensionManager: function ()
  {
    /* 
     const EMTYPE = "Extension:Manager";
     var wm = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                        .getService(Components.interfaces.nsIWindowMediator);
     var theEM = wm.getMostRecentWindow(EMTYPE);
     if (theEM) {
         theEM.focus();
         return;
     }

     const EMURL = "chrome://mozapps/content/extensions/extensions.xul";
     const EMFEATURES = "chrome,menubar,extra-chrome,toolbar,dialog=no,resizable";
     window.openDialog(EMURL, "", EMFEATURES);
     */

     const nsIFilePicker = Ci.nsIFilePicker;
        var fp = Cc["@mozilla.org/filepicker;1"]
                   .createInstance(nsIFilePicker);
        fp.init(window,"select ex", nsIFilePicker.modeOpenMultiple);
        try {
          fp.appendFilter(gStrings.ext.GetStringFromName("附件组件"),
                          "*.xpi;*.jar");
          fp.appendFilters(nsIFilePicker.filterAll);
        } catch (e) { }

        if (fp.show() != nsIFilePicker.returnOK)
          return;

        var files = fp.files;
        var installs = [];

        function buildNextInstall() {
          if (!files.hasMoreElements()) {
            if (installs.length > 0) {
              // Display the normal install confirmation for the installs
              AddonManager.installAddonsFromWebpage("application/x-xpinstall",
                                                    window, null, installs);
            }
            return;
          }

          var file = files.getNext();
          AddonManager.getInstallForFile(file, function cmd_installFromFile_getInstallForFile(aInstall) {
            installs.push(aInstall);
            buildNextInstall();
          });
        }

        buildNextInstall();
  },

 };




