Components.utils.import("chrome://mailtools/content/fileHelper.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/FileUtils.jsm");
Components.utils.import("chrome://mailtools/content/logHelper.jsm");
//all-thunderbird.js中的数据不能直接绑定到g_srcPrefFile进行文件操作，不符合用户定义的PrefFile 
	      //只能采用文件内容中查找和追加的方法 
//getBoolPref(  getIntPref
 var gConfigDialog = {
 	pref: null,
   srcPath: null,
   SRS_debug: null,//searchResultString_debug: null,

    onLoad: function ()
  	{
  		//alert("config load");
   // this.reslutsinfos = "--------------start------------"

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
	               this.srcPath = this.pref.getCharPref("version.src.dir");

	               var objpath = document.getElementById("objpath");
	                setok = this.pref.prefHasUserValue("version.obj.dir");  
	            if (setok)
	                this.objPath = this.pref.getCharPref("version.obj.dir");

	           //alert(this.objPath);

	       }

 			this.init_debug_info();
 			this.init_func_sup_pubmail_info();
	       ////////////////////////////////////
	     
	    
	       
	},
	init_debug_info: function()
	{
		var valueref = "user_pref(\"version.release.flag\", false);";
		 document.getElementById("version.release.flag_ref").value = valueref;

		var searchString = "version.release.flag";
		this.SRS_debug = "";

		var filePath  = this.objPath +"\\mozilla\\dist\\bin\\profile\\prefs.js";

		var file  = new FileUtils.File(filePath);
      if (!file.exists()) {    // 若不存在，先创建一个空文件
        alert(filePath + "文件不存在!");
        return;
        }      
	    // open an input stream from file
	    var istream = Components.classes["@mozilla.org/network/file-input-stream;1"].
	                  createInstance(Components.interfaces.nsIFileInputStream);
	    istream.init(file, 0x01, 0444, 0);
	    istream.QueryInterface(Components.interfaces.nsILineInputStream);

	    // read lines into array
	    var line = {}, hasmore;
	    do {
	      hasmore = istream.readLine(line);
	      if ( line.value.indexOf(searchString) > -1 )
	      {
	      		this.SRS_debug = line.value;
	      		break;
	      }
	    } while(hasmore);
	    istream.close();

	    document.getElementById("version.release.flag_old").value = this.SRS_debug;
	    if( this.SRS_debug == "" )
	    {
	    	document.getElementById("version.release.flag_new").value = valueref;
	    }
	    else
	    	document.getElementById("version.release.flag_new").value = this.SRS_debug;

	},
	settoDebug: function ()
	  {
	  	 var filePath  = this.objPath +"\\mozilla\\dist\\bin\\profile\\prefs.js";
		var SRS_new = document.getElementById("version.release.flag_new").value;
		if (this.SRS_debug == "" )
		{
			fileHelper.file_addto(filePath,SRS_new);	 
		}
		else
		{
			 var srcStrArray = [];
		    var objStrArray = [];
		    srcStrArray.push( this.SRS_debug );
		    objStrArray.push(SRS_new);
		    fileHelper.file_replace_arry(filePath,srcStrArray,objStrArray);
		}
	   this.init_debug_info();
	    promptHelper.alert("修改完成!");
	  },

	searchStringFromSrc: function(searchString)
	{
		var searchResult = "";
		var filePath  = this.srcPath +"\\mail\\app\\profile\\all-thunderbird.js";

			searchResult = fileHelper.file_searchString(filePath,searchString);

		    return searchResult;
	},
	searchStringFromPref: function(searchString)
	{
		var searchResult = "";
		var filePath  = this.objPath +"\\mozilla\\dist\\bin\\profile\\prefs.js";

			searchResult = fileHelper.file_searchString(filePath,searchString);

		    return searchResult;
	},


	init_func_sup_pubmail_info: function()
	{
		var valueref = "user_pref(\"func_sup_pubmail\", true);";
		 document.getElementById("func_sup_pubmail_ref").value = valueref;

		var searchString = "func_sup_pubmail";

		this.searchResStr_fsp = "";
		this.searchResStr_fsp = this.searchStringFromPref(searchString);

	    document.getElementById("func_sup_pubmail_old").value = this.searchResStr_fsp;
	    if( this.searchResStr_fsp == "" )
	    {
	    	document.getElementById("func_sup_pubmail_new").value = valueref;
	    	/*
	    	//如果个性化配置中找不到，则到源码的默认文件中查找---不去查找了，查找后，字符串的前缀不同 user_
			this.searchResStr_fsp = this.searchStringFromSrc(searchString);
		    document.getElementById("func_sup_pubmail_old").value = this.searchResStr_fsp;
		    if( this.searchResStr_fsp == "" )
		    {
		    	//如果个性化配置中找不到，则到源码的默认文件中查找
		    	document.getElementById("func_sup_pubmail_new").value = valueref;
		    }
		    else
		    	document.getElementById("func_sup_pubmail_new").value = this.searchResStr_fsp;
		    */
	    }
	    else
	    	document.getElementById("func_sup_pubmail_new").value = this.searchResStr_fsp;

	},

	mod_func_sup_pubmail: function ()
	  {
	  	 var filePath  = this.objPath +"\\mozilla\\dist\\bin\\profile\\prefs.js";
		var SRS_new = document.getElementById("func_sup_pubmail_new").value;
		if (this.searchResStr_fsp == "" )
		{
			fileHelper.file_addto(filePath,SRS_new);	 
		}
		else
		{
			 var srcStrArray = [];
		    var objStrArray = [];
		    srcStrArray.push( this.searchResStr_fsp );
		    objStrArray.push(SRS_new);
		    fileHelper.file_replace_arry(filePath,srcStrArray,objStrArray);
		}
	   //修改完成后，要再初始化一下，防止重复添加
	   this.init_func_sup_pubmail_info();
	    promptHelper.alert("修改完成!");
	  },


 };