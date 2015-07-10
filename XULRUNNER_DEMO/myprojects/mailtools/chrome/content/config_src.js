Components.utils.import("chrome://mailtools/content/fileHelper.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/FileUtils.jsm");
Components.utils.import("chrome://mailtools/content/logHelper.jsm");
 var gConfigSrcDialog = {
 	pref: null,
   srcPath: null,
   searchResStr_fsp:null,
   searchResStr_pubreq:null,
   searchResStr_cmenreq:null,
   srcPrefFileFullName:null,
   searchResStr_upthurl:null,
   searchResStr_upthurlchname:null,
   cur_build_version:null,
   allverMap: {},
   searchResStr_mboxar:null,
   searchResStr_specdom:null,
   searchResStr_calloc: null,
   searchResStr_atma: null,
   searchResStr_ppsmtp: null,

    onLoad: function ()
  	{

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

	             setok = this.pref.prefHasUserValue("cur.build.version");  
	            if (setok)
	                this.cur_build_version = this.pref.getCharPref("cur.build.version");

	            

	       }
	       //this.srcPrefFileFullName = this.srcPath +"\\mail\\app\\profile\\all-thunderbird.js";
	       this.srcPrefFileFullName = this.objPath +"\\mozilla\\dist\\bin\\defaults\\pref\\all-thunderbird.js";
	       this.init_all();
 			
	},

	init_all: function ()
	{
		this.init_func_sup_pubmail_info();
		this.init_pubemail_request();
		this.init_cm_enterprise_dir_request();
		this.init_update_thinkmail_url();
		this.init_update_thinkmail_url_channelName();
		this.init_labcurverinfo();

		this.init_verFuncSupInfos();

		this.init_mailbox_async_raw();

		this.init_func_specialdomain_control();
		this.init_func_calendar_local();
		this.init_func_attach_manage();
		this.init_func_protocol_private_smtp();

		this.init_version_sgcc();
	},

	ch2binatjs: function ()
	{
		this.srcPrefFileFullName = this.srcPath +"\\mail\\app\\profile\\all-thunderbird.js";
		 this.init_all();
	},
	//smtp私有协议控制
	init_func_protocol_private_smtp: function ()
	{
		var valueref="pref(\"func.protocol.private.smtp\", true);";
		document.getElementById("func.protocol.private.smtp_ref").value = valueref;
		var searchString = "func.protocol.private.smtp";
		this.searchResStr = "";
		this.searchResStr_ppsmtp = "";
		this.searchResStr_ppsmtp = fileHelper.file_searchString(this.srcPrefFileFullName,searchString);

	    document.getElementById("func.protocol.private.smtp_old").value = this.searchResStr_ppsmtp;
	    if( this.searchResStr_ppsmtp == "" )
	    {
	    	document.getElementById("func.protocol.private.smtp_new").value = valueref;
	    }
	    else
	    	document.getElementById("func.protocol.private.smtp_new").value = this.searchResStr_ppsmtp;

	},
	

	mod_func_protocol_private_smtp: function ()
	  {
		var SRS_new = document.getElementById("func.protocol.private.smtp_new").value;
		if (this.searchResStr_ppsmtp == "" )
		{
			fileHelper.file_addto(this.srcPrefFileFullName,SRS_new);	 
		}
		else
		{
			 var srcStrArray = [];
		    var objStrArray = [];
		    srcStrArray.push( this.searchResStr_ppsmtp );
		    objStrArray.push(SRS_new);
		    fileHelper.file_replace_arry(this.srcPrefFileFullName,srcStrArray,objStrArray);
		}
	   this.init_func_protocol_private_smtp();
	   logHelper.writelog("\n----INFO:smtp私有协议设置完成！");
	  },
	  init_version_sgcc: function ()
	{
		var valueref="pref(\"version.sgcc\", true);";
		document.getElementById("version.sgcc_ref").value = valueref;
		var searchString = "version.sgcc";
		this.searchResStr = "";
		this.searchResStr_ppsmtp = "";
		this.searchResStr_ppsmtp = fileHelper.file_searchString(this.srcPrefFileFullName,searchString);

	    document.getElementById("version.sgcc_old").value = this.searchResStr_ppsmtp;
	    if( this.searchResStr_ppsmtp == "" )
	    {
	    	document.getElementById("version.sgcc_new").value = valueref;
	    }
	    else
	    	document.getElementById("version.sgcc_new").value = this.searchResStr_ppsmtp;

	},
	  mod_version_sgcc: function ()
	  {
		var SRS_new = document.getElementById("version.sgcc_new").value;
		if (this.searchResStr_ppsmtp == "" )
		{
			fileHelper.file_addto(this.srcPrefFileFullName,SRS_new);	 
		}
		else
		{
			 var srcStrArray = [];
		    var objStrArray = [];
		    srcStrArray.push( this.searchResStr_ppsmtp );
		    objStrArray.push(SRS_new);
		    fileHelper.file_replace_arry(this.srcPrefFileFullName,srcStrArray,objStrArray);
		}
	   this.init_version_sgcc();
	   logHelper.writelog("\n----INFO:smtp私有协议设置完成--功能！");
	  },
	//附件管理控制
	init_func_attach_manage: function ()
	{
		var valueref="pref(\"func.attach.manage\", true);";
		document.getElementById("func.attach.manage_ref").value = valueref;
		var searchString = "func.attach.manage";
		this.searchResStr = "";
		this.searchResStr_atma = "";
		this.searchResStr_atma = fileHelper.file_searchString(this.srcPrefFileFullName,searchString);

	    document.getElementById("func.attach.manage_old").value = this.searchResStr_atma;
	    if( this.searchResStr_atma == "" )
	    {
	    	document.getElementById("func.attach.manage_new").value = valueref;
	    }
	    else
	    	document.getElementById("func.attach.manage_new").value = this.searchResStr_atma;

	},

	mod_func_attach_manage: function ()
	  {
		var SRS_new = document.getElementById("func.attach.manage_new").value;
		if (this.searchResStr_atma == "" )
		{
			fileHelper.file_addto(this.srcPrefFileFullName,SRS_new);	 
		}
		else
		{
			 var srcStrArray = [];
		    var objStrArray = [];
		    srcStrArray.push( this.searchResStr_atma );
		    objStrArray.push(SRS_new);
		    fileHelper.file_replace_arry(this.srcPrefFileFullName,srcStrArray,objStrArray);
		}
	   this.init_func_attach_manage();
	   logHelper.writelog("\n----INFO:附件管理设置完成！");
	  },
	//本地日程控制
	init_func_calendar_local: function ()
	{
		var valueref="pref(\"module.calendar\", true);";
		document.getElementById("func.calendar.local_ref").value = valueref;
		var searchString = "module.calendar";
		this.searchResStr = "";
		this.searchResStr_calloc = "";
		this.searchResStr_calloc = fileHelper.file_searchString(this.srcPrefFileFullName,searchString);

	    document.getElementById("func.calendar.local_old").value = this.searchResStr_calloc;
	    if( this.searchResStr_calloc == "" )
	    {
	    	document.getElementById("func.calendar.local_new").value = valueref;
	    }
	    else
	    	document.getElementById("func.calendar.local_new").value = this.searchResStr_calloc;

	},

	mod_func_calendar_local: function ()
	  {
		var SRS_new = document.getElementById("func.calendar.local_new").value;
		if (this.searchResStr_calloc == "" )
		{
			fileHelper.file_addto(this.srcPrefFileFullName,SRS_new);	 
		}
		else
		{
			 var srcStrArray = [];
		    var objStrArray = [];
		    srcStrArray.push( this.searchResStr_calloc );
		    objStrArray.push(SRS_new);
		    fileHelper.file_replace_arry(this.srcPrefFileFullName,srcStrArray,objStrArray);
		}
	   this.init_func_calendar_local();
	   logHelper.writelog("\n----INFO:本地日程功能设置完成！");
	  },

	//专用域控制
	init_func_specialdomain_control: function ()
	{
		var valueref="pref(\"func.specialdomain.control\", false);";
		document.getElementById("func.specialdomain.control_ref").value = valueref;
		var searchString = "func.specialdomain.control";
		this.searchResStr = "";
		this.searchResStr_specdom = "";
		this.searchResStr_specdom = fileHelper.file_searchString(this.srcPrefFileFullName,searchString);

	    document.getElementById("func.specialdomain.control_old").value = this.searchResStr_specdom;
	    if( this.searchResStr_specdom == "" )
	    {
	    	document.getElementById("func.specialdomain.control_new").value = valueref;
	    }
	    else
	    	document.getElementById("func.specialdomain.control_new").value = this.searchResStr_specdom;

	},

	mod_func_specialdomain_control: function ()
	  {
		var SRS_new = document.getElementById("func.specialdomain.control_new").value;
		if (this.searchResStr_specdom == "" )
		{
			fileHelper.file_addto(this.srcPrefFileFullName,SRS_new);	 
		}
		else
		{
			 var srcStrArray = [];
		    var objStrArray = [];
		    srcStrArray.push( this.searchResStr_specdom );
		    objStrArray.push(SRS_new);
		    fileHelper.file_replace_arry(this.srcPrefFileFullName,srcStrArray,objStrArray);
		}
	   this.init_func_specialdomain_control();
	   logHelper.writelog("\n----INFO:专用域控制设置完成！");
	  },

//存储加密配置方案
	init_mailbox_async_raw: function ()
	{
		var valueref="pref(\"mailbox.async.raw\", false);";
		document.getElementById("mailbox.async.raw_ref").value = valueref;
		var searchString = "mailbox.async.raw";
		this.searchResStr = "";
		this.searchResStr_mboxar = "";
		this.searchResStr_mboxar = fileHelper.file_searchString(this.srcPrefFileFullName,searchString);

	    document.getElementById("mailbox.async.raw_old").value = this.searchResStr_mboxar;
	    if( this.searchResStr_mboxar == "" )
	    {
	    	document.getElementById("mailbox.async.raw_new").value = valueref;
	    }
	    else
	    	document.getElementById("mailbox.async.raw_new").value = this.searchResStr_mboxar;

	},

	mod_mailbox_async_raw: function ()
	  {
		var SRS_new = document.getElementById("mailbox.async.raw_new").value;
		if (this.searchResStr_mboxar == "" )
		{
			fileHelper.file_addto(this.srcPrefFileFullName,SRS_new);	 
		}
		else
		{
			 var srcStrArray = [];
		    var objStrArray = [];
		    srcStrArray.push( this.searchResStr_mboxar );
		    objStrArray.push(SRS_new);
		    fileHelper.file_replace_arry(this.srcPrefFileFullName,srcStrArray,objStrArray);
		}
	   this.init_mailbox_async_raw();
	   logHelper.writelog("\n----INFO:邮件加密存储设置完成！");
	  },
	  //初始化默认版本支持的功能
	 init_verFuncSupInfos: function ()
	  {
	     var setok = this.pref.prefHasUserValue("all.ver.supfunc.infos");  
        if (setok)
        {
            var supverjsonstring = this.pref.getCharPref("all.ver.supfunc.infos");
	    	this.allverMap = eval("(" + supverjsonstring + ")");
        }

	            //alert(this.allverMap[this.cur_build_version].PubEmail);
	  },

	init_labcurverinfo: function ()
	{
		document.getElementById("labcurverinfo").value=this.cur_build_version;
	},
	refcurverinfo: function ()
	{
		 var setok = this.pref.prefHasUserValue("cur.build.version");  
        if (setok)
            this.cur_build_version = this.pref.getCharPref("cur.build.version");
        this.init_labcurverinfo();
	},
	setcurverinfo: function ()
	{
		this.produce_modobjver(this.cur_build_version);
	},

	modcurverinfo: function ()
	{
		//公共邮箱
		this.mod_func_sup_pubmail();
		//存储加密
		this.mod_mailbox_async_raw();
		//专用域控制
		this.mod_func_specialdomain_control();
		//本地日程控制
		this.mod_func_calendar_local();
	},

	produce_modobjver: function (str)
	{
	  document.getElementById("broadbuidverchange").setAttribute("newver",str);
	},

	consume_modobjver: function (obj)
	{
		if (obj.getAttribute("newver") != "default")
		{
			this.changeVerInfoSet(this.allverMap[this.cur_build_version]);
		}
	},

	changeVerInfoSet: function ( obj )
	{
		//公共邮箱
		if ( obj.PubEmail=="true")
		{
			var valueenable = "pref(\"func_sup_pubmail\", true);";
			document.getElementById("func_sup_pubmail_new").value =valueenable;
		}
		else
		{
			var valuedisenable = "pref(\"func_sup_pubmail\", false);";
			document.getElementById("func_sup_pubmail_new").value =valuedisenable;

		}
		//加密存储
		if ( obj.StorageEncryption=="true")
		{
			var valueenable = "pref(\"mailbox.async.raw\", true);";
			document.getElementById("mailbox.async.raw_new").value =valueenable;
		}
		else
		{
			var valuedisenable = "pref(\"mailbox.async.raw\", false);";
			document.getElementById("mailbox.async.raw_new").value =valuedisenable;

		}
		//专用域
		if ( obj.SpecialDomain=="true")
		{
			var valueenable = "pref(\"func.specialdomain.control\", true);";
			document.getElementById("func.specialdomain.control_new").value =valueenable;
		}
		else
		{
			var valuedisenable = "pref(\"func.specialdomain.control\", false);";
			document.getElementById("func.specialdomain.control_new").value =valuedisenable;
		}
		//专用域
		if ( obj.CaleLocal=="true")
		{
			var valueenable = "pref(\"module.calendar\", true);";
			document.getElementById("func.calendar.local_new").value =valueenable;
		}
		else
		{
			var valuedisenable = "pref(\"module.calendar\", false);";
			document.getElementById("func.calendar.local_new").value =valuedisenable;
		}
		
	},


	init_func_sup_pubmail_info: function()
	{
		var valueref = "pref(\"func_sup_pubmail\", true);";
		 document.getElementById("func_sup_pubmail_ref").value = valueref;

		var searchString = "func_sup_pubmail";
		this.searchResStr = "";

		//var filePath  = this.srcPath +"\\mail\\app\\profile\\all-thunderbird.js";
		this.searchResStr_fsp = "";
		this.searchResStr_fsp = fileHelper.file_searchString(this.srcPrefFileFullName,searchString);

	    document.getElementById("func_sup_pubmail_old").value = this.searchResStr_fsp;
	    if( this.searchResStr_fsp == "" )
	    {
	    	document.getElementById("func_sup_pubmail_new").value = valueref;
	    }
	    else
	    	document.getElementById("func_sup_pubmail_new").value = this.searchResStr_fsp;

	},

	mod_func_sup_pubmail: function ()
	  {
	  	 //var filePath  = this.srcPath +"\\mail\\app\\profile\\all-thunderbird.js";
		var SRS_new = document.getElementById("func_sup_pubmail_new").value;
		if (this.searchResStr_fsp == "" )
		{
			fileHelper.file_addto(this.srcPrefFileFullName,SRS_new);	 
		}
		else
		{
			 var srcStrArray = [];
		    var objStrArray = [];
		    srcStrArray.push( this.searchResStr_fsp );
		    objStrArray.push(SRS_new);
		    fileHelper.file_replace_arry(this.srcPrefFileFullName,srcStrArray,objStrArray);
		}
	   	this.init_func_sup_pubmail_info();
	    //promptHelper.alert("修改完成!");
	    logHelper.writelog("\n----INFO:公共邮箱设置完成！");

	    
	  },

//公共邮箱请求地址
	init_pubemail_request: function ()
	{
		var valueref="http://www.oatest.cn/webmail/service/serviceapi.do?func=serviceapi:CheckLogin";
		document.getElementById("publicEmail.request_ref").value = valueref;
		var searchString = "publicEmail.request";
		this.searchResStr = "";
		//var filePath  = this.srcPath +"\\mail\\app\\profile\\all-thunderbird.js";
		this.searchResStr_pubreq = "";
		this.searchResStr_pubreq = fileHelper.file_searchString(this.srcPrefFileFullName,searchString);

	    document.getElementById("publicEmail.request_old").value = this.searchResStr_pubreq;
	    if( this.searchResStr_pubreq == "" )
	    {
	    	document.getElementById("publicEmail.request_new").value = valueref;
	    }
	    else
	    	document.getElementById("publicEmail.request_new").value = this.searchResStr_pubreq;

	},

	mod_pubemail_request: function ()
	  {
	  	// var filePath  = this.srcPath +"\\mail\\app\\profile\\all-thunderbird.js";
		var SRS_new = document.getElementById("publicEmail.request_new").value;
		if (this.searchResStr_pubreq == "" )
		{
			fileHelper.file_addto(this.srcPrefFileFullName,SRS_new);	 
		}
		else
		{
			 var srcStrArray = [];
		    var objStrArray = [];
		    srcStrArray.push( this.searchResStr_pubreq );
		    objStrArray.push(SRS_new);
		    fileHelper.file_replace_arry(this.srcPrefFileFullName,srcStrArray,objStrArray);
		}
	   this.init_pubemail_request();
	    promptHelper.alert("修改完成!");
	  },

//中国移动企业通讯录请求地址
	init_cm_enterprise_dir_request: function ()
	{
		var valueref="http://www.oatest.cn/webmail/service/serviceapi.do?func=serviceapi:corpAddr&sid=lJ8g9RCqKRVRHRLxLrKSRRnUbMlsOhBL000001&r=816cb4fba8de414e9fcd72098e28e8bf";
		document.getElementById("cm.enterprise.dir.request_ref").value = valueref;
		var searchString = "cm.enterprise.dir.request";
		this.searchResStr = "";
		//var filePath  = this.srcPath +"\\mail\\app\\profile\\all-thunderbird.js";
		this.searchResStr_cmenreq = "";
		this.searchResStr_cmenreq = fileHelper.file_searchString(this.srcPrefFileFullName,searchString);

	    document.getElementById("cm.enterprise.dir.request_old").value = this.searchResStr_cmenreq;
	    if( this.searchResStr_cmenreq == "" )
	    {
	    	document.getElementById("cm.enterprise.dir.request_new").value = valueref;
	    }
	    else
	    	document.getElementById("cm.enterprise.dir.request_new").value = this.searchResStr_cmenreq;
	},
	mod_cm_enterprise_dir_request: function ()
	  {
	  	// var filePath  = this.srcPath +"\\mail\\app\\profile\\all-thunderbird.js";
		var SRS_new = document.getElementById("cm.enterprise.dir.request_new").value;
		if (this.searchResStr_cmenreq == "" )
		{
			fileHelper.file_addto(this.srcPrefFileFullName,SRS_new);	 
		}
		else
		{
			 var srcStrArray = [];
		    var objStrArray = [];
		    srcStrArray.push( this.searchResStr_cmenreq );
		    objStrArray.push(SRS_new);
		    fileHelper.file_replace_arry(this.srcPrefFileFullName,srcStrArray,objStrArray);
		}
	   this.init_cm_enterprise_dir_request();
	    promptHelper.alert("修改完成!");
	  },



	  //程序新版本检测地址
	init_update_thinkmail_url: function ()
	{
		var valueref="http://demo2.thinkcloud.cn/AppClientUpgrade/upgradeCheck.do";
		document.getElementById("update.thinkmail.url_ref").value = valueref;
		var searchString = "update.thinkmail.url\"";
		this.searchResStr = "";
		//var filePath  = this.srcPath +"\\mail\\app\\profile\\all-thunderbird.js";
		this.searchResStr_upthurl = "";
		this.searchResStr_upthurl = fileHelper.file_searchString(this.srcPrefFileFullName,searchString);

	    document.getElementById("update.thinkmail.url_old").value = this.searchResStr_upthurl;
	    if( this.searchResStr_upthurl == "" )
	    {
	    	document.getElementById("update.thinkmail.url_new").value = valueref;
	    }
	    else
	    	document.getElementById("update.thinkmail.url_new").value = this.searchResStr_upthurl;
	},
	mod_update_thinkmail_url: function ()
	  {
	  	// var filePath  = this.srcPath +"\\mail\\app\\profile\\all-thunderbird.js";
		var SRS_new = document.getElementById("update.thinkmail.url_new").value;
		if (this.searchResStr_upthurl == "" )
		{
			fileHelper.file_addto(this.srcPrefFileFullName,SRS_new);	 
		}
		else
		{
			 var srcStrArray = [];
		    var objStrArray = [];
		    srcStrArray.push( this.searchResStr_upthurl );
		    objStrArray.push(SRS_new);
		    fileHelper.file_replace_arry(this.srcPrefFileFullName,srcStrArray,objStrArray);
		}
	   this.init_update_thinkmail_url(); //修改后执行下查询，以更新显示和防止下次再修改查找不到
	    promptHelper.alert("修改完成!");
	  },

	  init_update_thinkmail_url_channelName: function ()
	  {
	  	var valueref="Mobile_Beta";
		document.getElementById("update.thinkmail.url.channelName_ref").value = valueref;
		var searchString = "update.thinkmail.url.channelName";
		this.searchResStr = "";
		//var filePath  = this.srcPath +"\\mail\\app\\profile\\all-thunderbird.js";
		this.searchResStr_upthurlchname = "";
		this.searchResStr_upthurlchname = fileHelper.file_searchString(this.srcPrefFileFullName,searchString);

	    document.getElementById("update.thinkmail.url.channelName_old").value = this.searchResStr_upthurlchname;
	    if( this.searchResStr_upthurlchname == "" )
	    {
	    	document.getElementById("update.thinkmail.url.channelName_new").value = valueref;
	    }
	    else
	    	document.getElementById("update.thinkmail.url.channelName_new").value = this.searchResStr_upthurlchname;
	  },
	  mod_update_thinkmail_url_channelName: function ()
	  {
		var SRS_new = document.getElementById("update.thinkmail.url.channelName_new").value;
		if (this.searchResStr_upthurlchname == "" )
		{
			fileHelper.file_addto(this.srcPrefFileFullName,SRS_new);	 
		}
		else
		{
			 var srcStrArray = [];
		    var objStrArray = [];
		    srcStrArray.push( this.searchResStr_upthurlchname );
		    objStrArray.push(SRS_new);
		    fileHelper.file_replace_arry(this.srcPrefFileFullName,srcStrArray,objStrArray);
		}
	   this.init_update_thinkmail_url_channelName(); //修改后执行下查询，以更新显示和防止下次再修改查找不到
	    promptHelper.alert("修改完成!");
	  },

 };