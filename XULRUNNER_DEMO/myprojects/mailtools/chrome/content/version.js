
Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/FileUtils.jsm");
Components.utils.import("chrome://mailtools/content/fileHelper.jsm");
Components.utils.import("chrome://mailtools/content/logHelper.jsm");



var gVersionDialog = {
   pref: null,
   srcPath: null,
   objPath: null,
   //reslutsinfos: null,
   curVerFull: null,
   curVerFullEx: null,
   curVerStart: null,
   objVerFull: null,
   objVerFullEx: null,
   objVerStart: null,


  savePrefile: function ()
    {
      /*
      Components.classes["@mozilla.org/preferences-service;1"]
              .getService(Components.interfaces.nsIPrefService)
              .savePrefFile(null);
              alert("savePrefile");
              */
    },
  onLoad: function ()
  {
    //this.reslutsinfos = "--------------start------------"
   // this.readVersionSrcDirPref();
    this.pref = Components.classes['@mozilla.org/preferences-service;1'].
getService(Components.interfaces.nsIPrefService);
      if (this.pref)
       {
        /*
         var file = FileUtils.getDir("ProfD", []);
            file.append("prefs.js");
            if (!file.exists()) {    // 若不存在，先创建一个空文件
              alert(filePath + "文件不存在!");
             // this.reslutsinfos +=   "\n----ERROR:"+filePath + "文件不存在!";
              return;
              }      
                this.pref.readUserPrefs(file);
                */
                this.pref = this.pref.getBranch(null);
                var downloadFolder = document.getElementById("downloadFolder");
                var setok = this.pref.prefHasUserValue("version.src.dir");  
            if (setok)
                downloadFolder.label = this.srcPath = this.pref.getCharPref("version.src.dir");

               var objpath = document.getElementById("objpath");
                setok = this.pref.prefHasUserValue("version.obj.dir");  
            if (setok)
                objpath.label = this.objPath = this.pref.getCharPref("version.obj.dir");

              //alert(this.objPath);

              var curversion = document.getElementById("curversion");
              setok = this.pref.prefHasUserValue("version.curversion.full"); 
             if (setok) {
                 curversion.value = this.curVerFull = this.pref.getCharPref("version.curversion.full");
                  }

                   var objversion = document.getElementById("objversion");
              setok = this.pref.prefHasUserValue("version.objversion.full"); 
             if (setok) {
                 objversion.value = this.objVerFull = this.pref.getCharPref("version.objversion.full");
                  }

                  setok = this.pref.prefHasUserValue("version.curversion.start"); 
             if (setok) {
                this.curVerStart = this.pref.getCharPref("version.curversion.start");
                  }

                  setok = this.pref.prefHasUserValue("version.objversion.start"); 
             if (setok) {
                this.objVerStart = this.pref.getCharPref("version.objversion.start");
                  }


                  curversion = document.getElementById("curversionEx");
              setok = this.pref.prefHasUserValue("version.curversionEx.full"); 
             if (setok) {
                 curversion.value = this.curVerFullEx = this.pref.getCharPref("version.curversionEx.full");
                  }

                   objversion = document.getElementById("objversionEx");
              setok = this.pref.prefHasUserValue("version.objversionEx.full"); 
             if (setok) {
                 objversion.value = this.objVerFullEx = this.pref.getCharPref("version.objversionEx.full");
                  }
          }

  },
   chooseFolder: function (who)
  {
    const nsIFilePicker = Components.interfaces.nsIFilePicker;
    var fp = Components.classes["@mozilla.org/filepicker;1"]
                       .createInstance(nsIFilePicker);
    fp.init(window, "select", nsIFilePicker.modeGetFolder);

    const nsILocalFile = Components.interfaces.nsILocalFile;
    fp.appendFilters(nsIFilePicker.filterAll);
    if (fp.show() == nsIFilePicker.returnOK) {
      var file = fp.file.QueryInterface(nsILocalFile);

      if( who == "src")
      {
        var downloadFolder = document.getElementById("downloadFolder");
         downloadFolder.label = file.path;
          if (this.pref) {
         this.pref.setCharPref("version.src.dir", file.path);
         this.srcPath = file.path;
          }
          var ios = Components.classes["@mozilla.org/network/io-service;1"]
                            .getService(Components.interfaces.nsIIOService);
        var fph = ios.getProtocolHandler("file")
                     .QueryInterface(Components.interfaces.nsIFileProtocolHandler);
        var urlspec = fph.getURLSpecFromFile(file);
        downloadFolder.image = "moz-icon://" + urlspec + "?size=16";
      }
      else
      {
        var objpath = document.getElementById("objpath");
         objpath.label = file.path;
          if (this.pref) {
         this.pref.setCharPref("version.obj.dir", file.path);
         this.objPath = file.path;
          }
          var ios = Components.classes["@mozilla.org/network/io-service;1"]
                            .getService(Components.interfaces.nsIIOService);
        var fph = ios.getProtocolHandler("file")
                     .QueryInterface(Components.interfaces.nsIFileProtocolHandler);
        var urlspec = fph.getURLSpecFromFile(file);
        objpath.image = "moz-icon://" + urlspec + "?size=16";
      }
    

    }

    this.savePrefile();
  },

  saveVer: function ()
  {
    var curversion = document.getElementById("curversion");
     if (this.pref) {
         this.pref.setCharPref("version.curversion.full", curversion.value);
         this.curVerFull = curversion.value;

         var strs= new Array(); 
          strs=this.curVerFull.split("."); 
          //strs[strs.length-1]="*";
          var newstr="";
          for (i=0;i<strs.length-1 ;i++ ) 
          { 
            newstr  += strs[i]+".";           
          } 
          newstr  += "*";
          this.curVerStart = newstr;
          this.pref.setCharPref("version.curversion.start", this.curVerStart);
        }



   var objversion = document.getElementById("objversion");
     if (this.pref) {
         this.pref.setCharPref("version.objversion.full", objversion.value);
         this.objVerFull = objversion.value;

          var strs= new Array(); 
          strs=this.objVerFull.split("."); 
          //strs[strs.length-1]="*";
          var newstr="";
          for (i=0;i<strs.length-1 ;i++ ) 
          { 
            newstr  += strs[i]+".";           
          } 
          newstr  += "*";
          this.objVerStart = newstr;
          this.pref.setCharPref("version.objversion.start", this.objVerStart);
        }

  },

saveVerEx: function ()
  {
    var curversion = document.getElementById("curversionEx");
     if (this.pref) {
         this.pref.setCharPref("version.curversionEx.full", curversion.value);
         this.curVerFullEx = curversion.value;
        }



   var objversion = document.getElementById("objversionEx");
     if (this.pref) {
         this.pref.setCharPref("version.objversionEx.full", objversion.value);
         this.objVerFullEx = objversion.value;
        }

  },

  clearLog: function ()
  {
    this.reslutsinfos = "";
    results = document.getElementById("results");
    if (results)
      results.value = this.reslutsinfos;
  },

  modExVersion: function ()
  {
    this.saveVerEx();
    //this.reslutsinfos +=   "\n--------------修改扩展版本号-----------";
    logHelper.writelog("\n--------------modExVersion----------- ");
    this.modExVersion_skin();
    this.modExVersion_mailmerge();
    this.modExVersion_mminimize();

  //  results = document.getElementById("results");
  //  if ( this.reslutsinfos )
   //   results.value = this.reslutsinfos;
  },

  modExVersion_skin: function ()
  {
    //this.reslutsinfos +=   "\n--------------修改---皮肤---扩展版本号-----------";
     logHelper.writelog("\n--------------mod skin ExVersion----------- ");
    var filePath  = this.srcPath +"\\mail\\app\\profile\\extensions\\default@skin.richinfo.cn\\install.rdf.in";
//em:version="3.7.7"
    var srcStrArray = [];
    var objStrArray = [];
    var curversionExValue = "em:version=\"" + this.curVerFullEx + "\"";
    srcStrArray.push(curversionExValue);
    var objversionExValue = "em:version=\"" + this.objVerFullEx + "\"";
    objStrArray.push(objversionExValue);
    this.saveVerEx();
    this.modVersion_comm(filePath,srcStrArray,objStrArray);

  },

  modExVersion_mailmerge: function ()
  {
    //this.reslutsinfos +=   "\n--------------修改---群发单显---扩展版本号-----------";
    logHelper.writelog("\n--------------mod mailmerge ExVersion----------- ");
    var filePath  = this.srcPath +"\\mail\\app\\profile\\extensions\\mailmerge@example.net\\install.rdf.in";
//  <em:version>3.7.7</em:version>
    var srcStrArray = [];
    var objStrArray = [];
    var curversionExValue = "<em:version>" + this.curVerFullEx + "</em:version>";
    srcStrArray.push(curversionExValue);
    var objversionExValue = "<em:version>" + this.objVerFullEx + "</em:version>";
    objStrArray.push(objversionExValue);
    this.saveVerEx();
    this.modVersion_comm(filePath,srcStrArray,objStrArray);

  },

  modExVersion_mminimize: function ()
  {
    //this.reslutsinfos +=   "\n--------------修改---最小化---扩展版本号-----------";
    logHelper.writelog("\n--------------mod minimize ExVersion----------- ");
    var filePath  = this.srcPath +"\\mail\\app\\profile\\extensions\\minimize@richinfo.cn\\install.rdf";
//  <em:version>3.7.7</em:version>
    var srcStrArray = [];
    var objStrArray = [];
    var curversionExValue = "<em:version>" + this.curVerFullEx + "</em:version>";
    srcStrArray.push(curversionExValue);
    var objversionExValue = "<em:version>" + this.objVerFullEx + "</em:version>";
    objStrArray.push(objversionExValue);
    this.saveVerEx();
    this.modVersion_comm(filePath,srcStrArray,objStrArray);

  },


  modVersion_comm: function (filePath,srcStrArray,objStrArray)
  {
    fileHelper.file_replace_arry(filePath,srcStrArray,objStrArray);
    /*
    ////////////////////读文件///////////////////////
      var file  = new FileUtils.File(filePath);
      if (!file.exists()) {    // 若不存在，先创建一个空文件
        alert(filePath + "文件不存在!");
        this.reslutsinfos +=   "\n----ERROR:"+filePath + "文件不存在!";
        return;
        }      

     var fstream = Components.classes["@mozilla.org/network/file-input-stream;1"].
          createInstance(Components.interfaces.nsIFileInputStream);
      var cstream = Components.classes["@mozilla.org/intl/converter-input-stream;1"].
          createInstance(Components.interfaces.nsIConverterInputStream);
      fstream.init(file, -1, 0, 0);
      cstream.init(fstream, "UTF-8", 0, 0);

      var data = "";
      var read = 0;
      do {
          var str = {};
          read = cstream.readString(0xffffffff, str);
          data += str.value;
      } while (read != 0);
      cstream.close(); // this closes fstream
      //alert(data);

///////////////////写文件///////////////

      var fstream2 = Components.classes["@mozilla.org/network/file-output-stream;1"].
          createInstance(Components.interfaces.nsIFileOutputStream);
      var cstream2 = Components.classes["@mozilla.org/intl/converter-output-stream;1"].
          createInstance(Components.interfaces.nsIConverterOutputStream);
      fstream2.init(file, -1, 0, 0);
      cstream2.init(fstream2, "UTF-8", 0, 0);

      for ( var i=0 ; i < srcStrArray.length ; ++i ) 
      {
        data = data.replace(srcStrArray[i],objStrArray[i]);
      }
    
      cstream2.writeString(data);
      cstream2.close();

      this.reslutsinfos += "\n----INFO:"+ filePath + "文件处理完成!";
    */
  },


  modVersion: function ()
  {
    this.saveVer();
    //this.reslutsinfos +=   "\n--------------修改版本号-----------";
    logHelper.writelog("\n--------------mod Version----------- ");
    this.modVersion_version_Txt();
    this.modVersion_autoconf_mk();
    this.modVersion_application_ini();

    results = document.getElementById("results");
    if ( this.reslutsinfos )
      results.value = this.reslutsinfos;
  },

  modVersion_application_ini: function ()
  {
    //this.reslutsinfos +=   "\n--------------修改application.ini文件版本号-----------";
     logHelper.writelog("\n--------------mod application.ini Version----------- ");
    /////////////////////////////
    var filePath  = this.objPath +"\\mozilla\\dist\\bin\\application.ini";
    //data = data.replace("Version=1.3.3","Version=1.4.0");
    var srcStrArray = [];
    var objStrArray = [];
    var curversionValue = "Version=" + this.curVerFull;
    srcStrArray.push(curversionValue);
    var objversionValue = "Version=" + this.objVerFull;
    objStrArray.push(objversionValue);  
    this.modVersion_comm(filePath,srcStrArray,objStrArray);
  },

   modVersion_autoconf_mk: function ()
  {
    // this.reslutsinfos +=   "\n--------------修改autoconf.mk文件版本号-----------";
    logHelper.writelog("\n--------------mod autoconf.mk Version----------- ");
    /////////////////////////////
    var filePath  = this.objPath +"\\config\\autoconf.mk";
   // data = data.replace("MOZ_APP_MAXVERSION = 1.3.*","OZ_APP_MAXVERSION = 1.4.*");
   // data = data.replace("MOZ_APP_VERSION = 1.3.3","MOZ_APP_VERSION = 1.4.0");
   // data = data.replace("THUNDERBIRD_VERSION = 1.3.3","THUNDERBIRD_VERSION = 1.4.0");
    var srcStrArray = [];
    var objStrArray = [];
    var curversionValue = "MOZ_APP_MAXVERSION = " + this.curVerStart;
     srcStrArray.push(curversionValue);
    curversionValue = "MOZ_APP_VERSION = " + this.curVerFull;
    srcStrArray.push(curversionValue);
    curversionValue = "THUNDERBIRD_VERSION = " + this.curVerFull;
    srcStrArray.push(curversionValue);


    var objversionValue = "MOZ_APP_MAXVERSION = " + this.objVerStart;
    objStrArray.push(objversionValue);
    objversionValue = "MOZ_APP_VERSION = " + this.objVerFull;
    objStrArray.push(objversionValue);  
    objversionValue = "THUNDERBIRD_VERSION = " + this.objVerFull;
    objStrArray.push(objversionValue); 

 
    this.modVersion_comm(filePath,srcStrArray,objStrArray);
  },

  modVersion_version_Txt: function ()
  {
     //this.reslutsinfos +=   "\n--------------修改version.txt文件版本号-----------";
      logHelper.writelog("\n--------------mod version.txt Version----------- ");
    /////////////////////////////
    var filePath  = this.srcPath +"\\mail\\config\\version.txt";
    //var filePath  = this.srcPath +"\\test\\version.txt";
    //data = data.replace("1.3.4","1.3.5");
    var srcStrArray = [];
    var objStrArray = [];
    var curversionValue = this.curVerFull;
    srcStrArray.push(curversionValue);
    var objversionValue = this.objVerFull;
    objStrArray.push(objversionValue);  
    this.modVersion_comm(filePath,srcStrArray,objStrArray);

  },

 };




