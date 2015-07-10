
Components.utils.import("chrome://mailtools/content/fileHelper.jsm");
Components.utils.import("chrome://mailtools/content/logHelper.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/FileUtils.jsm");

var gNameSkinDialog = {
   pref: null,
   srcPath: null,
   objPath: null,
   oldName: null,
   newName: null,
   brandoldName: null,
   brandnewName: null,
   supVerlistArray: [],
   supCurVerName: null,
   supverjsonstring: null,
   allverMap: {},
   curVerFull: null,


  onLoad: function ()
  {
    this.init_supVerlistArray();

   

   // this.readVersionSrcDirPref();
    this.pref = Components.classes['@mozilla.org/preferences-service;1'].
getService(Components.interfaces.nsIPrefService);
      if (this.pref)
       {
                this.pref = this.pref.getBranch(null);
                var downloadFolder = document.getElementById("downloadFolder");
                var setok = this.pref.prefHasUserValue("version.src.dir");  
            if (setok)
                this.srcPath = this.pref.getCharPref("version.src.dir");

               var objpath = document.getElementById("objpath");
                setok = this.pref.prefHasUserValue("version.obj.dir");  
            if (setok)
                this.objPath = this.pref.getCharPref("version.obj.dir");

              var oldname = document.getElementById("oldname");
              setok = this.pref.prefHasUserValue("ns.oldname"); 
             if (setok) {
                 oldname.value = this.oldname = this.pref.getCharPref("ns.oldname");
                  }

                   var newname = document.getElementById("newname");
              setok = this.pref.prefHasUserValue("ns.newname"); 
             if (setok) {
                 newname.value = this.newname = this.pref.getCharPref("ns.newname");
                  }

                  var brandoldname = document.getElementById("brandoldname");
              setok = this.pref.prefHasUserValue("ns.brandoldname"); 
             if (setok) {
                 brandoldname.value = this.brandoldname = this.pref.getCharPref("ns.brandoldname");
                  }

                   var brandnewname = document.getElementById("brandnewname");
              setok = this.pref.prefHasUserValue("ns.brandnewname"); 
             if (setok) {
                 brandnewname.value = this.brandnewname = this.pref.getCharPref("ns.brandnewname");
                  }

              setok = this.pref.prefHasUserValue("version.curversion.full"); 
             if (setok) {
                 this.curVerFull = this.pref.getCharPref("version.curversion.full");
                  }

          }


          this.init_mozobjdir();
          this.init_supverlist_data();
          this.init_supverlist_sel();
          this.init_verFuncSupInfos();

          this.init_setInfos();

  },

  init_verFuncSupInfos: function ()
  {
    this.supverjsonstring = "{ \"ThinkMail_sgcc\": { \"PubEmail\": \"false\", \"StorageEncryption\": \"true\", \"SpecialDomain\": \"true\", \"CaleLocal\": \"false\" },\"ThinkMail_richinfo\": { \"PubEmail\": \"false\", \"StorageEncryption\": \"false\", \"SpecialDomain\": \"false\" , \"CaleLocal\": \"true\"  }, \"CMMail_mobile\": { \"PubEmail\": \"true\", \"StorageEncryption\": \"false\", \"SpecialDomain\": \"false\" , \"CaleLocal\": \"true\"  } }"
    document.getElementById("allsupverjsonstring").value = this.supverjsonstring;
    this.pref.setCharPref("all.ver.supfunc.infos",this.supverjsonstring);
    this.allverMap = eval("(" + this.supverjsonstring + ")");

            //alert(this.allverMap[this.supCurVerName].PubEmail);
  },

  init_supVerlistArray: function ()
  {
    this.supVerlistArray.push("ThinkMail_richinfo");
    this.supVerlistArray.push("ThinkMail_sgcc");
    this.supVerlistArray.push("CMMail_mobile");
  },

  init_mozobjdir: function ()
  {
    var filePath  = this.srcPath +"\\.mozconfig";
    var mozobjdir = "";
    mozobjdir = fileHelper.file_searchString(filePath,"mk_add_options MOZ_OBJDIR");
    document.getElementById("MOZ_OBJDIR").value = mozobjdir;
  },

  init_supverlist_data: function ()
  {
    var supverlist = document.getElementById("supverlist");
    for ( var i=0 ; i < this.supVerlistArray.length ; ++i )
    {
      supverlist.appendItem(this.supVerlistArray[i],this.supVerlistArray[i]);
    }
  },

  init_supverlist_sel: function ()
  {
     var filePath  = this.srcPath +"\\.mozconfig";
     var curselindex = 0;
    for ( var i=0 ; i < this.supVerlistArray.length ; ++i )
    {
      var cursearchres = "";
      cursearchres = fileHelper.file_searchString(filePath,this.supVerlistArray[i]);
      if(cursearchres != "" )
      {
        curselindex = i;
        this.pref.setCharPref("cur.build.version",this.supVerlistArray[i]);
        this.supCurVerName = this.supVerlistArray[i];
      }
    }
    var supverlist = document.getElementById("supverlist");
    supverlist.selectedIndex = curselindex;
  },

  init_setInfos: function ()
  {
    document.getElementById("setupcmdtext").value = "./setup.sh obj_"+this.supCurVerName + " 皮肤生成更新："+"./skin.sh obj_"+this.supCurVerName ;
    document.getElementById("signcmdtext").value = "./sign_setup.sh obj_"+this.supCurVerName;
  },

  selectChange: function ()
  {
    var supverlist = document.getElementById("supverlist");
    this.supCurVerName = supverlist.getItemAtIndex(supverlist.selectedIndex).value;
    this.init_setInfos();
  },

  verSwitch: function ()
  {
     var supverlist = document.getElementById("supverlist");
    // if(supverlist.selectedIndex <= 0 )
   //  {
    //  promptHelper.alert("请选择正确的版本!");
   //  }
     this.pref.setCharPref("cur.build.version",supverlist.getItemAtIndex(supverlist.selectedIndex).value);
    // alert(supverlist.getItemAtIndex(1).value);
   //  alert(supverlist.getItemAtIndex(1).label);
      this.Cover_mozconfig_file();
      this.init_mozobjdir();
  },

  installerReName: function ()
  {
    
    var ProgrameName = "";
    var Operator = "";
    var VersionNum = this.curVerFull;
    var supCurVerNameSplit =  this.supCurVerName;
      var supCurVerNameSplit = supCurVerNameSplit.split("_");
      if ( supCurVerNameSplit.length >= 2 )
      {
        ProgrameName = supCurVerNameSplit[0];
        Operator = supCurVerNameSplit[1];
      }
      //ThinkMail-1.3.5.zh-CN.win32.installer.exe
       var filePathsrc  = this.objPath +"\\mozilla\\dist\\install\\sea\\"+ProgrameName+"-"+VersionNum+".zh-CN.win32.installer.exe";
    //var  filePathobj = this.objPath +"\\mozilla\\dist\\install\\sea\\";
    var newName = ProgrameName+"_"+Operator+"-"+VersionNum+".zh-CN.win32.installer.exe";
    fileHelper.file_rename(filePathsrc,newName);
    logHelper.writelog("\n--------------文件重命名完成:"+newName);   

  },

  updateSwitch: function ()
  {
    logHelper.writelog("\n--------------updatecfg----------- ");   
    var filePath  = this.srcPath +"\\mozilla\\tools\\update-packaging\\updatecfg";
    var writedata = this.objPath.replace(/\\/g,"\/");
    writedata = writedata.replace(":","");
    writedata = "/" + writedata +"\n";
    fileHelper.file_rewrite(filePath,writedata);
  },

  logoSwitch: function ()
  {
    logHelper.writelog("\n--------------logoSwitch----------- ");   
     ///////////appLogo.png 文件
    this.mod_applogo_png();
  },

  iconSwitch: function ()
  {
    logHelper.writelog("\n--------------iconSwitch----------- ");   
     ///////////setup.ico 文件
    this.mod_setup_ico_7z();
    this.mod_setup_ico_nsis();
    this.mod_setup_ico_7zSD_sfx();

    ///////////newmail.ico 文件
    this.mod_newmail_ico();

    ///////////7zSD.sfx
     this.mod_7zSD_sfx();

     ////////////lockscreen.ico
     this.mod_lockscreen_ico();

     ////////////messengerWindow.ico
     this.mod_messengerWindow_ico();

     ////////////thunderbird.ico
     this.mod_thunderbird_ico();

  },

  mod_thunderbird_ico: function ()
  {
   logHelper.writelog("\n--------------thunderbird.ico---------- ");
   
    var filePathsrc  = this.srcPath +"\\Custom\\"+this.supCurVerName+"\\thunderbird.ico";
    var  filePathobj = this.srcPath +"\\mail\\branding\\richinfo\\thunderbird.ico";
    fileHelper.file_replacefile(filePathsrc,filePathobj,"thunderbird.ico");
  },

  mod_messengerWindow_ico: function ()
  {
   logHelper.writelog("\n--------------messengerWindow.ico---------- ");
   
    var filePathsrc  = this.srcPath +"\\Custom\\"+this.supCurVerName+"\\messengerWindow.ico";
    var  filePathobj = this.srcPath +"\\mail\\branding\\richinfo\\windows\\messengerWindow.ico";
    fileHelper.file_replacefile(filePathsrc,filePathobj,"messengerWindow.ico");
  },


  aboutfaceSwitch: function ()
  {
    logHelper.writelog("\n--------------aboutfaceSwitch----------- ");   
       ///////////about-wordmark.png 文件
      this.mod_about_wordmark_png();
      ///////aboutDialog.dtd
      this.mod_aboutDialog_dtd();

      /////baseMenuOverlay.dtd
       this.mod_baseMenuOverlay_dtd();
  },
   mod_baseMenuOverlay_dtd: function ()
    {
     logHelper.writelog("\n--------------baseMenuOverlay.dtd---------- ");
     
      var filePathsrc  = this.srcPath +"\\Custom\\"+this.supCurVerName+"\\about\\baseMenuOverlay.dtd";
      var  filePathobj = this.srcPath +"\\mail\\locales\\zh-CN\\zh-CN\\mail\\chrome\\messenger\\baseMenuOverlay.dtd";
      fileHelper.file_replacefile(filePathsrc,filePathobj,"baseMenuOverlay.dtd");
    },

  mod_aboutDialog_dtd: function ()
  {
   logHelper.writelog("\n--------------aboutDialog.dtd---------- ");
   
    var filePathsrc  = this.srcPath +"\\Custom\\"+this.supCurVerName+"\\about\\aboutDialog.dtd";
    var  filePathobj = this.srcPath +"\\mail\\locales\\zh-CN\\zh-CN\\mail\\chrome\\messenger\\aboutDialog.dtd";
    fileHelper.file_replacefile(filePathsrc,filePathobj,"aboutDialog.dtd");
  },

  mod_lockscreen_ico: function ()
  {
   logHelper.writelog("\n--------------lockscreen.ico---------- ");
   
    var filePathsrc  = this.srcPath +"\\Custom\\"+this.supCurVerName+"\\lockscreen.ico";
    var  filePathobj = this.srcPath +"\\mailnews\\build\\lockscreen.ico";
    fileHelper.file_replacefile(filePathsrc,filePathobj,"lockscreen.ico");
  },

  mod_lockscreen_ico: function ()
  {
   logHelper.writelog("\n--------------lockscreen.ico---------- ");
   
    var filePathsrc  = this.srcPath +"\\Custom\\"+this.supCurVerName+"\\lockscreen.ico";
    var  filePathobj = this.srcPath +"\\mailnews\\build\\lockscreen.ico";
    fileHelper.file_replacefile(filePathsrc,filePathobj,"lockscreen.ico");
  },
  mod_7zSD_sfx: function ()
  {
   logHelper.writelog("\n--------------7zSD.sfx---------- ");
   
    var filePathsrc  = this.srcPath +"\\Custom\\"+this.supCurVerName+"\\7zSD.sfx";
    var  filePathobj = this.srcPath +"\\other-licenses\\7zstub\\thunderbird\\7zSD.sfx";
    fileHelper.file_replacefile(filePathsrc,filePathobj,"7zSD.sfx");
  },

  mod_newmail_ico: function ()
  {
   logHelper.writelog("\n--------------newmail.ico---------- ");
   
    var filePathsrc  = this.srcPath +"\\Custom\\"+this.supCurVerName+"\\newmail.ico";
    var  filePathobj = this.srcPath +"\\mailnews\\build\\newmail.ico";
    fileHelper.file_replacefile(filePathsrc,filePathobj,"newmail.ico");
  },

  mod_setup_ico_7zSD_sfx: function ()
  {
   logHelper.writelog("\n--------------setup.ico  7z---------- ");
   
    var filePathsrc  = this.srcPath +"\\Custom\\"+this.supCurVerName+"\\setup.ico";
    var  filePathobj = this.srcPath +"\\mozilla\\other-licenses\\7zstub\\src\\7zip\\Bundles\\SFXSetup-moz\\setup.ico";
    fileHelper.file_replacefile(filePathsrc,filePathobj,"setup.ico");
  },

  mod_setup_ico_7z: function ()
  {
   logHelper.writelog("\n--------------setup.ico  7z---------- ");
   
    var filePathsrc  = this.srcPath +"\\Custom\\"+this.supCurVerName+"\\setup.ico";
    var  filePathobj = this.srcPath +"\\mozilla\\other-licenses\\7zstub\\src\\7zip\\Bundles\\SFXSetup-moz\\setup.ico";
    fileHelper.file_replacefile(filePathsrc,filePathobj,"setup.ico");
  },
  mod_setup_ico_nsis: function ()
  {
   logHelper.writelog("\n--------------setup.ico  nsis---------- ");
   
    var filePathsrc  = this.srcPath +"\\Custom\\"+this.supCurVerName+"\\setup.ico";
    var  filePathobj = this.srcPath +"\\mozilla\\toolkit\\mozapps\\installer\\windows\\nsis\\setup.ico";
    fileHelper.file_replacefile(filePathsrc,filePathobj,"setup.ico");
  },

  objPathSwitch: function ()
  {
  
    this.objPath = this.srcPath + "\\obj_" +this.supCurVerName;
     if (this.pref) {
         this.pref.setCharPref("version.obj.dir", this.objPath);
      }
      logHelper.writelog("\n--------------objPathSwitch----------- ");   
  },

  Cover_mozconfig_file: function ()
  {
    logHelper.writelog("\n--------------.mozconfig----------- ");
   
    var filePathsrc  = this.srcPath +"\\Custom\\"+this.supCurVerName+"\\building\\.mozconfig";
    var  filePathobj = this.srcPath +"\\.mozconfig";
    fileHelper.file_replacefile(filePathsrc,filePathobj,".mozconfig");
  },

  saveName: function ()
  {
    var oldname = document.getElementById("oldname");
     if (this.pref) {
         this.pref.setCharPref("ns.oldname", oldname.value);
         this.oldname = oldname.value;
        }

   var newname = document.getElementById("newname");
     if (this.pref) {
         this.pref.setCharPref("ns.newname", newname.value);
         this.newname = newname.value;
        }

        logHelper.writelog("\n--------------名称保存完成！----------- ");
  },

  savebrandName: function ()
  {
    var brandoldname = document.getElementById("brandoldname");
     if (this.pref) {
         this.pref.setCharPref("ns.brandoldname", brandoldname.value);
         this.brandoldname = brandoldname.value;
        }

   var brandnewname = document.getElementById("brandnewname");
     if (this.pref) {
         this.pref.setCharPref("ns.brandnewname", brandnewname.value);
         this.brandnewname = brandnewname.value;
        }

       logHelper.writelog("\n--------------发行信息保存完成！----------- ");
  },


  modbrandName: function ()
  {
    this.savebrandName();
   
    ///////////module.ver 文件
    this.mod_module_ver_brand();
    logHelper.writelog("\n--------------发行信息修改完成！----------- "); 
  },

  mod_module_ver_brand: function ()
  {
//WIN32_MODULE_COPYRIGHT=2013 Richinfo. All Rights Reserved.
//WIN32_MODULE_COMPANYNAME=Richinfo Corporation
//WIN32_MODULE_TRADEMARKS=CMMail is a Trademark of Richinfo.
//WIN32_MODULE_COMMENT=Richinfo CMMail Mail Client
   logHelper.writelog("\n--------------module.ver----------- ");   
    var filePath  = this.srcPath +"\\mail\\app\\module.ver";
    var srcStrArray = [];
    var objStrArray = [];
    var strString1 = "WIN32_MODULE_COPYRIGHT=2013 " + this.brandoldname + ". All Rights Reserved.";
    srcStrArray.push(strString1);
    var objString1 = "WIN32_MODULE_COPYRIGHT=2013 " + this.brandnewname + ". All Rights Reserved.";
    objStrArray.push(objString1);

  //  strString1 = "WIN32_MODULE_COMMENT=Richinfo " + this.oldname + " Mail Client";
  //  srcStrArray.push(strString1);
  //  objString1 = "WIN32_MODULE_COMMENT=Richinfo " + this.newname + " Mail Client";
  //  objStrArray.push(objString1);

    fileHelper.file_replace_arry(filePath,srcStrArray,objStrArray);

  },


  modName: function ()
  {
    this.saveName();
    logHelper.writelog("\n--------------修改名称开始----------- ");
    logHelper.writelog("\n--------------mail目录下的----------- ");
   
    ///////////appLogo.png 文件
    this.mod_applogo_png();
     ////////application.ini 文件
    this.mod_application_ini();
    ////////////module.ver 文件
    this.mod_module_ver();
   
   //startwithdel.bat
   this.mod_startwithdel_bat();
   //startwithlog.bat
   this.mod_startwithlog_bat();

   ///////////about-wordmark.png 文件
   this.mod_about_wordmark_png();

   //brand.dtd
   this.mod_brand_dtd();
   //brand.properties
   this.mod_brand_properties();

   //branding.nsi
   this.mod_branding_nsi();
   //configure.sh
   this.configure_sh();

   //defines.nsi.in
   this.defines_nsi_in();

   //installer.nsi
   this.installer_nsi();

   //uninstaller.nsi
   this.uninstaller_nsi();

   //loctime Makefile
   this.loctime_Makefile();
   //Makefile.in
   this.Makefile_in();
   //webtime Makefile
   this.webtime_Makefile();

   //abMainWindow.dtd
   this.abMainWindow_dtd();

   //confvars.sh
   this.confvars_sh();

   //welcome_title1.bmp
   this.welcome_title1_bmp();

   //un_welcome_title2.bmp
   this.un_welcome_title2_bmp();

   //un_welcome_title1.bmp
   this.un_welcome_title1_bmp();

   //un_finish_title1.bmp
   this.un_finish_title1_bmp();

    ////////////////
    logHelper.writelog("\n--------------mailnews目录下的----------- ");
     logHelper.writelog("\n--------------修改名称结束----------- ");
  },

   un_finish_title1_bmp: function ()
  {
   logHelper.writelog("\n--------------un_finish_title1.bmp----------- ");
   
    var filePathsrc  = this.srcPath +"\\Custom\\"+this.supCurVerName+"\\setup\\un_finish_title1.bmp";
    var  filePathobj = this.srcPath +"\\mail\\branding\\richinfo\\cm\\un_finish_title1.bmp";
    fileHelper.file_replacefile(filePathsrc,filePathobj,"un_finish_title1.bmp");
  },

  un_welcome_title1_bmp: function ()
  {
   logHelper.writelog("\n--------------un_welcome_title1.bmp----------- ");
   
    var filePathsrc  = this.srcPath +"\\Custom\\"+this.supCurVerName+"\\setup\\un_welcome_title1.bmp";
    var  filePathobj = this.srcPath +"\\mail\\branding\\richinfo\\cm\\un_welcome_title1.bmp";
    fileHelper.file_replacefile(filePathsrc,filePathobj,"un_welcome_title1.bmp");
  },

  un_welcome_title2_bmp: function ()
  {
   logHelper.writelog("\n--------------un_welcome_title2.bmp----------- ");
   
    var filePathsrc  = this.srcPath +"\\Custom\\"+this.supCurVerName+"\\setup\\un_welcome_title2.bmp";
    var  filePathobj = this.srcPath +"\\mail\\branding\\richinfo\\cm\\un_welcome_title2.bmp";
    fileHelper.file_replacefile(filePathsrc,filePathobj,"un_welcome_title2.bmp");
  },

  welcome_title1_bmp: function ()
  {
   logHelper.writelog("\n--------------welcome_title1.bmp----------- ");
   
    var filePathsrc  = this.srcPath +"\\Custom\\"+this.supCurVerName+"\\setup\\welcome_title1.bmp";
    var  filePathobj = this.srcPath +"\\mail\\branding\\richinfo\\cm\\welcome_title1.bmp";
    fileHelper.file_replacefile(filePathsrc,filePathobj,"welcome_title1.bmp");
  },

  mod_application_ini: function ()
  {

   logHelper.writelog("\n--------------application.ini----------- ");   
    var filePath  = this.srcPath +"\\mail\\app\\application.ini";
    var srcStrArray = [];
    var objStrArray = [];
    var strString1 = "Name=" + this.oldname;
    srcStrArray.push(strString1);
    var objString1 = "Name=" + this.newname;
    objStrArray.push(objString1);
    fileHelper.file_replace_arry(filePath,srcStrArray,objStrArray);

  },
  mod_applogo_png: function ()
  {
   logHelper.writelog("\n--------------applogo.png----------- ");
   
    var filePathsrc  = this.srcPath +"\\Custom\\"+this.supCurVerName+"\\skin\\appLogo.png";
    var  filePathobj = this.srcPath +"\\mail\\app\\profile\\extensions\\default@skin.richinfo.cn\\messenger\\appLogo.png";
    fileHelper.file_replacefile(filePathsrc,filePathobj,"appLogo.png");
  },

  mod_module_ver: function ()
  {
//WIN32_MODULE_TRADEMARKS=CMMail is a Trademark of Richinfo.
//WIN32_MODULE_COMMENT=Richinfo CMMail Mail Client
   logHelper.writelog("\n--------------module.ver----------- ");   
    var filePath  = this.srcPath +"\\mail\\app\\module.ver";
    var srcStrArray = [];
    var objStrArray = [];
    var strString1 = "WIN32_MODULE_TRADEMARKS=" + this.oldname + " is a Trademark of Richinfo.";
    srcStrArray.push(strString1);
    var objString1 = "WIN32_MODULE_TRADEMARKS=" + this.newname + " is a Trademark of Richinfo.";
    objStrArray.push(objString1);

    strString1 = "WIN32_MODULE_COMMENT=Richinfo " + this.oldname + " Mail Client";
    srcStrArray.push(strString1);
    objString1 = "WIN32_MODULE_COMMENT=Richinfo " + this.newname + " Mail Client";
    objStrArray.push(objString1);

    fileHelper.file_replace_arry(filePath,srcStrArray,objStrArray);

  },

   mod_startwithdel_bat: function ()
  {
   logHelper.writelog("\n--------------startwithdel.bat----------- ");   
    var filePath  = this.srcPath +"\\mail\\app\\startwithdel.bat";
    var srcStrArray = [];
    var objStrArray = [];
    //\obj_CMMail_mobile\
    var strString1 = "\\" + this.oldname + "\\";
    srcStrArray.push(strString1);
    var objString1 = "\\" + this.newname + "\\";
    objStrArray.push(objString1);

    srcStrArray.push(this.oldname+".exe");
    objStrArray.push(this.newname+".exe");

  

    fileHelper.file_replace_arry_all(filePath,srcStrArray,objStrArray);
  },

  mod_startwithlog_bat: function ()
  {
   logHelper.writelog("\n--------------startwithlog.bat----------- ");   
    var filePath  = this.srcPath +"\\mail\\app\\startwithlog.bat";
    var srcStrArray = [];
    var objStrArray = [];
    

    var strString1 = this.oldname;
    srcStrArray.push(strString1);
    var objString1 = this.newname;
    objStrArray.push(objString1);

      srcStrArray.push(strString1+".exe");
    objStrArray.push(objString1+".exe");

    fileHelper.file_replace_arry_all(filePath,srcStrArray,objStrArray);
  },
  mod_about_wordmark_png: function ()
  {
   logHelper.writelog("\n--------------about-wordmark.png ----------- ");
   
    var filePathsrc  = this.srcPath +"\\Custom\\"+this.supCurVerName+"\\branding\\about-wordmark.png";
    var  filePathobj = this.srcPath +"\\mail\\branding\\richinfo\\content\\about-wordmark.png";
    fileHelper.file_replacefile(filePathsrc,filePathobj,"about-wordmark.png");
  },
   mod_brand_dtd: function ()
  {
   logHelper.writelog("\n--------------brand.dtd----------- ");   
    var filePath  = this.srcPath +"\\mail\\branding\\richinfo\\locales\\en-US\\brand.dtd";
    var srcStrArray = [];
    var objStrArray = [];
    var strString1 = this.oldname;
    srcStrArray.push(strString1);
    var objString1 = this.newname;
    objStrArray.push(objString1);


    fileHelper.file_replace_arry_all(filePath,srcStrArray,objStrArray);
  },

  mod_brand_properties: function ()
  {
     logHelper.writelog("\n--------------brand.properties----------- ");   
    var filePath  = this.srcPath +"\\mail\\branding\\richinfo\\locales\\en-US\\brand.properties";
    var srcStrArray = [];
    var objStrArray = [];
    var strString1 = this.oldname;
    srcStrArray.push(strString1);
    var objString1 = this.newname;
    objStrArray.push(objString1);

    fileHelper.file_replace_arry_all(filePath,srcStrArray,objStrArray);
  },

  mod_branding_nsi: function ()
  {
    logHelper.writelog("\n--------------branding.nsi----------- ");   
    var filePath  = this.srcPath +"\\mail\\branding\\richinfo\\branding.nsi";
    var srcStrArray = [];
    var objStrArray = [];
    var strString1 = this.oldname;
    srcStrArray.push(strString1);
    var objString1 = this.newname;
    objStrArray.push(objString1);

    fileHelper.file_replace_arry_all(filePath,srcStrArray,objStrArray);
  },

  configure_sh: function ()
  {
    logHelper.writelog("\n--------------configure.sh----------- ");   
    var filePath  = this.srcPath +"\\mail\\branding\\richinfo\\configure.sh";
    var srcStrArray = [];
    var objStrArray = [];
    var strString1 = this.oldname;
    srcStrArray.push(strString1);
    var objString1 = this.newname;
    objStrArray.push(objString1);

    fileHelper.file_replace_arry_all(filePath,srcStrArray,objStrArray);
  },

  defines_nsi_in: function ()
  {
    logHelper.writelog("\n--------------defines.nsi.in----------- ");   
    var filePath  = this.srcPath +"\\mail\\installer\\windows\\nsis\\defines.nsi.in";
    var srcStrArray = [];
    var objStrArray = [];
    var strString1 = this.oldname;
    srcStrArray.push(strString1);
    var objString1 = this.newname;
    objStrArray.push(objString1);

    fileHelper.file_replace_arry_all(filePath,srcStrArray,objStrArray);
  },
  installer_nsi: function ()
  {
    //!define DEBUG_CMMail
    //${CleanUpdatesDir} "CMMail"
    //${AddHandlerValues} "$0\CMMailEML"  "$1" "$8,0" \
    //${AddHandlerValues} "$0\CMMail.Url.mailto"  "$2" "$8,0" \
    //${AddHandlerValues} "$0\CMMail.Url.news" "$3" "$8,0" \
    //${LogStartMenuShortcut} "CMMail"
    //CreateDirectory "$SMPROGRAMS\CMMail"
    //CreateShortCut "$SMPROGRAMS\CMMail\${BrandFullName}.lnk" "$INSTDIR\${FileMainEXE}" \
    //CreateShortCut "$SMPROGRAMS\CMMail\$(UN_CONFIRM_PAGE_TITLE).lnk" "$INSTDIR\uninstall\helper.exe" \
    logHelper.writelog("\n--------------installer.nsi----------- ");   
    var filePath  = this.srcPath +"\\mail\\installer\\windows\\nsis\\installer.nsi";
    var srcStrArray = [];
    var objStrArray = [];
     //!define DEBUG_CMMail
    var strString1 = "!define DEBUG_"+this.oldname;
    srcStrArray.push(strString1);
    var objString1 = "!define DEBUG_"+this.newname;
    objStrArray.push(objString1);
    //${CleanUpdatesDir} "CMMail"
    strString1 = "${CleanUpdatesDir} \""+this.oldname;
    srcStrArray.push(strString1);
    objString1 = "${CleanUpdatesDir} \""+this.newname;
    objStrArray.push(objString1);
    //${AddHandlerValues} "$0\CMMailEML"  "$1" "$8,0" \
    strString1 = "${AddHandlerValues} \"$0\\"+this.oldname+"EML\"  \"$1\" \"$8,0\" \\";
    srcStrArray.push(strString1);
    objString1 = "${AddHandlerValues} \"$0\\"+this.newname+"EML\"  \"$1\" \"$8,0\" \\";
    objStrArray.push(objString1);

//${AddHandlerValues} "$0\CMMail.Url.mailto"  "$2" "$8,0" \
  strString1 = "${AddHandlerValues} \"$0\\"+this.oldname+".Url.mailto";
    srcStrArray.push(strString1);
    objString1 = "${AddHandlerValues} \"$0\\"+this.newname+".Url.mailto";
    objStrArray.push(objString1);

    //${AddHandlerValues} "$0\CMMail.Url.news" "$3" "$8,0" \
      strString1 = "${AddHandlerValues} \"$0\\"+this.oldname+".Url.news";
    srcStrArray.push(strString1);
    objString1 = "${AddHandlerValues} \"$0\\"+this.newname+".Url.news";
    objStrArray.push(objString1);

    //${LogStartMenuShortcut} "CMMail"
      strString1 = "${LogStartMenuShortcut} \""+this.oldname+"\"";
    srcStrArray.push(strString1);
    objString1 = "${LogStartMenuShortcut} \""+this.newname+"\"";
    objStrArray.push(objString1);

    //CreateDirectory "$SMPROGRAMS\CMMail"
      strString1 = "CreateDirectory \"$SMPROGRAMS\\"+this.oldname;
    srcStrArray.push(strString1);
    objString1 = "CreateDirectory \"$SMPROGRAMS\\"+this.newname;
    objStrArray.push(objString1);

    //CreateShortCut "$SMPROGRAMS\CMMail\${BrandFullName}.lnk" "$INSTDIR\${FileMainEXE}" \
      strString1 = "CreateShortCut \"$SMPROGRAMS\\"+this.oldname+"\\${BrandFullName}";
    srcStrArray.push(strString1);
    objString1 = "CreateShortCut \"$SMPROGRAMS\\"+this.newname+"\\${BrandFullName}";
    objStrArray.push(objString1);

    //CreateShortCut "$SMPROGRAMS\CMMail\$(UN_CONFIRM_PAGE_TITLE).lnk" "$INSTDIR\uninstall\helper.exe" \
      strString1 = "CreateShortCut \"$SMPROGRAMS\\"+this.oldname+"\\$(UN_CONFIRM_PAGE_TITLE)";
    srcStrArray.push(strString1);
    objString1 = "CreateShortCut \"$SMPROGRAMS\\"+this.newname+"\\$(UN_CONFIRM_PAGE_TITLE)";
    objStrArray.push(objString1);


    //fileHelper.file_replace_arry_all(filePath,srcStrArray,objStrArray);//这个中文有乱码
    fileHelper.file_replace_arry_Encoding(filePath,srcStrArray,objStrArray,"windows-1252");
  },

  uninstaller_nsi: function ()
  {
    //${DeleteFile} "$APPDATA\Richinfo\CMMail\profiles.ini"
    //${un.RegCleanAppHandler} "CMMail.Url.mailto"
    //${un.RegCleanAppHandler} "CMMail.Url.news"
    //${un.RegCleanAppHandler} "CMMailEML"
    //ReadRegStr $R9 HKCR "CMMailEML" ""
    //${un.RegCleanFileHandler}  ".eml"   "CMMailEML"
    //${un.RegCleanFileHandler}  ".wdseml" "CMMailEML"
    //${un.CleanUpdatesDir} "CMMail"
    logHelper.writelog("\n--------------uninstaller.nsi----------- ");   
    var filePath  = this.srcPath +"\\mail\\installer\\windows\\nsis\\uninstaller.nsi";
    var srcStrArray = [];
    var objStrArray = [];

    //${DeleteFile} "$APPDATA\Richinfo\CMMail\profiles.ini"
    var strString1 = "${DeleteFile} \"$APPDATA\\Richinfo\\"+this.oldname;
    srcStrArray.push(strString1);
    var objString1 = "${DeleteFile} \"$APPDATA\\Richinfo\\"+this.newname;
    objStrArray.push(objString1);

    //${un.RegCleanAppHandler} "CMMail.Url.mailto"
     strString1 = "${un.RegCleanAppHandler} \""+this.oldname+".Url.mailto";
    srcStrArray.push(strString1);
    objString1 = "${un.RegCleanAppHandler} \""+this.newname+".Url.mailto";
    objStrArray.push(objString1);

    //${un.RegCleanAppHandler} "CMMail.Url.news"
     strString1 = "${un.RegCleanAppHandler} \""+this.oldname+".Url.news";
    srcStrArray.push(strString1);
    objString1 = "${un.RegCleanAppHandler} \""+this.newname+".Url.news";
    objStrArray.push(objString1);

    //${un.RegCleanAppHandler} "CMMailEML"
     strString1 = "${un.RegCleanAppHandler} \""+this.oldname+"EML";
    srcStrArray.push(strString1);
    objString1 = "${un.RegCleanAppHandler} \""+this.newname+"EML";
    objStrArray.push(objString1);

    //ReadRegStr $R9 HKCR "CMMailEML" ""
     strString1 = "ReadRegStr $R9 HKCR \""+this.oldname+"EML";
    srcStrArray.push(strString1);
    objString1 = "ReadRegStr $R9 HKCR \""+this.newname+"EML";
    objStrArray.push(objString1);

    //${un.RegCleanFileHandler}  ".eml"   "CMMailEML"
     strString1 = "${un.RegCleanFileHandler}  \".eml\"   \""+this.oldname;
    srcStrArray.push(strString1);
    objString1 = "${un.RegCleanFileHandler}  \".eml\"   \""+this.newname;
    objStrArray.push(objString1);

    //${un.RegCleanFileHandler}  ".wdseml" "CMMailEML"
     strString1 = "${un.RegCleanFileHandler}  \".wdseml\" \""+this.oldname;
    srcStrArray.push(strString1);
    objString1 = "${un.RegCleanFileHandler}  \".wdseml\" \""+this.newname;
    objStrArray.push(objString1);

    //${un.CleanUpdatesDir} "CMMail"
     strString1 = "${un.CleanUpdatesDir} \""+this.oldname;
    srcStrArray.push(strString1);
    objString1 = "${un.CleanUpdatesDir} \""+this.newname;
    objStrArray.push(objString1);

    fileHelper.file_replace_arry_Encoding(filePath,srcStrArray,objStrArray,"windows-1252");
  },
  loctime_Makefile: function ()
  {
    logHelper.writelog("\n--------------loctime Makefile----------- ");   
    var filePath  = this.srcPath +"\\mail\\installer\\windows\\loctime Makefile";
    var srcStrArray = [];
    var objStrArray = [];
    var strString1 = this.oldname;
    srcStrArray.push(strString1);
    var objString1 = this.newname;
    objStrArray.push(objString1);

    fileHelper.file_replace_arry_all(filePath,srcStrArray,objStrArray);
  },

  Makefile_in: function ()
  {
    logHelper.writelog("\n--------------Makefile.in----------- ");   
    var filePath  = this.srcPath +"\\mail\\installer\\windows\\Makefile.in";
    var srcStrArray = [];
    var objStrArray = [];
    var strString1 = this.oldname;
    srcStrArray.push(strString1);
    var objString1 = this.newname;
    objStrArray.push(objString1);

    fileHelper.file_replace_arry_all(filePath,srcStrArray,objStrArray);
  },

  webtime_Makefile: function ()
  {
    logHelper.writelog("\n--------------webtime Makefile----------- ");   
    var filePath  = this.srcPath +"\\mail\\installer\\windows\\webtime Makefile";
    var srcStrArray = [];
    var objStrArray = [];
    var strString1 = this.oldname;
    srcStrArray.push(strString1);
    var objString1 = this.newname;
    objStrArray.push(objString1);

    fileHelper.file_replace_arry_all(filePath,srcStrArray,objStrArray);
  },

  abMainWindow_dtd: function ()
  {
     logHelper.writelog("\n--------------abMainWindow.dtd----------- ");   
    var filePath  = this.srcPath +"\\mail\\locales\\zh-CN\\zh-CN\\mail\\chrome\\messenger\\addressbook\\abMainWindow.dtd";
    var srcStrArray = [];
    var objStrArray = [];
    var strString1 = this.oldname;
    srcStrArray.push(strString1);
    var objString1 = this.newname;
    objStrArray.push(objString1);

    //fileHelper.file_replace_arry_all(filePath,srcStrArray,objStrArray);
    fileHelper.file_replace_arry_Encoding(filePath,srcStrArray,objStrArray,"windows-1252");
  },

  confvars_sh: function ()
  {
    logHelper.writelog("\n--------------confvars.sh----------- ");   
    var filePath  = this.srcPath +"\\mail\\confvars.sh";
    var srcStrArray = [];
    var objStrArray = [];
    var strString1 = this.oldname;
    srcStrArray.push(strString1);
    var objString1 = this.newname;
    objStrArray.push(objString1);

    fileHelper.file_replace_arry_all(filePath,srcStrArray,objStrArray);
  },


 };




