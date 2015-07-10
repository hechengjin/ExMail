const EXPORTED_SYMBOLS = [ "fileHelper" ];

Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/FileUtils.jsm");
Components.utils.import("chrome://mailtools/content/logHelper.jsm");
///nsIFile 
var fileHelper = {
    test: function(name) {
        try {
           
        } catch (ex) {
            return name;
        }
    },
  file_searchString: function (filePath,searchString)
  {
  	var searchResStr = "";
  		var file  = new FileUtils.File(filePath);
      if (!file.exists()) {    // 若不存在，先创建一个空文件
        //alert(filePath + "文件不存在!");
        logHelper.writelog("\n----ERROR:"+ filePath + " file not existence! ");
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
	      		searchResStr = line.value;
	      		break;
	      }
	    } while(hasmore);
	    istream.close();
      //logHelper.writelog("\n----INFO:"+ filePath + " file search ok! ");
      return searchResStr;
  },


   file_replace_arry_Encoding: function (filePath,srcStrArray,objStrArray,Encoding)
  {
    ////////////////////读文件///////////////////////
      var file  = new FileUtils.File(filePath);
      if (!file.exists()) { 
        logHelper.writelog("\n----ERROR:"+ filePath + " file not existence! ");
        return;
        }      

     var fstream = Components.classes["@mozilla.org/network/file-input-stream;1"].
          createInstance(Components.interfaces.nsIFileInputStream);
      var cstream = Components.classes["@mozilla.org/intl/converter-input-stream;1"].
          createInstance(Components.interfaces.nsIConverterInputStream);
      fstream.init(file, -1, 0, 0);
      cstream.init(fstream, Encoding, 0, 0);

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
      cstream2.init(fstream2, Encoding, 0, 0);

      for ( var i=0 ; i < srcStrArray.length ; ++i ) 
      {
        data = data.replace(srcStrArray[i],objStrArray[i]);
      }
    
      cstream2.writeString(data);
      cstream2.close();
     // logHelper.writelog("\n----INFO:"+ filePath + " file mod ok! ");
     // this.reslutsinfos += "\n----INFO:"+ filePath + "文件处理完成!";
  },
  file_rewrite: function (filePath,writedata)
  {
  var file  = new FileUtils.File(filePath);
      if (!file.exists()) {    // 若不存在，先创建一个空文件
        //alert(filePath + "文件不存在!");
        logHelper.writelog("\n----ERROR:"+ filePath + " file not existence! ");
        return;
        }      
    var fstream2 = Components.classes["@mozilla.org/network/file-output-stream;1"].
          createInstance(Components.interfaces.nsIFileOutputStream);
      var cstream2 = Components.classes["@mozilla.org/intl/converter-output-stream;1"].
          createInstance(Components.interfaces.nsIConverterOutputStream);
      fstream2.init(file, -1, 0, 0);
      cstream2.init(fstream2, "UTF-8", 0, 0);
    
      cstream2.writeString(writedata);
      cstream2.close();
  },

  file_replace_arry: function (filePath,srcStrArray,objStrArray)
  {
    ////////////////////读文件///////////////////////
      var file  = new FileUtils.File(filePath);
      if (!file.exists()) {    // 若不存在，先创建一个空文件
        //alert(filePath + "文件不存在!");
        logHelper.writelog("\n----ERROR:"+ filePath + " file not existence! ");
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
      //logHelper.writelog("\n----INFO:"+ filePath + " file mod ok! ");
     // this.reslutsinfos += "\n----INFO:"+ filePath + "文件处理完成!";
  },
//一行里出现两个单键字，只能替换第一个。
  file_replace_arry_all: function (filePath,srcStrArray,objStrArray)
  {
    ////////////////////读文件///////////////////////
    var data = "";
  		var file  = new FileUtils.File(filePath);
      if (!file.exists()) {    // 若不存在，先创建一个空文件
        //alert(filePath + "文件不存在!");
         logHelper.writelog("\n----ERROR:"+ filePath + " file not existence! ");
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
	       var linedata = line.value;
           for ( var i=0 ; i < srcStrArray.length ; ++i ) 
	      {
	        linedata = linedata.replace(srcStrArray[i],objStrArray[i]);
	      }
	      	linedata += "\n"
	      data +=linedata;
	    } while(hasmore);
	    istream.close();


      

///////////////////写文件///////////////

      var fstream2 = Components.classes["@mozilla.org/network/file-output-stream;1"].
          createInstance(Components.interfaces.nsIFileOutputStream);
      var cstream2 = Components.classes["@mozilla.org/intl/converter-output-stream;1"].
          createInstance(Components.interfaces.nsIConverterOutputStream);
      fstream2.init(file, -1, 0, 0);
      cstream2.init(fstream2, "UTF-8", 0, 0);
    
      cstream2.writeString(data);
      cstream2.close();
      //logHelper.writelog("\n----INFO:"+ filePath + " file mod ok! ");
     // this.reslutsinfos += "\n----INFO:"+ filePath + "文件处理完成!";
  },

  file_replace_arry_all_Encoding: function (filePath,srcStrArray,objStrArray,Encoding)
  {
    ////////////////////读文件///////////////////////
    var data = "";
  		var file  = new FileUtils.File(filePath);
      if (!file.exists()) {    // 若不存在，先创建一个空文件
        //alert(filePath + "文件不存在!");
        logHelper.writelog("\n----ERROR:"+ filePath + " file not existence! ");
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
	       var linedata = line.value;
           for ( var i=0 ; i < srcStrArray.length ; ++i ) 
	      {
	        linedata = linedata.replace(srcStrArray[i],objStrArray[i]);
	      }
	      	linedata += "\r"
	      data +=linedata;
	    } while(hasmore);
	    istream.close();


      

///////////////////写文件///////////////

      var fstream2 = Components.classes["@mozilla.org/network/file-output-stream;1"].
          createInstance(Components.interfaces.nsIFileOutputStream);
      var cstream2 = Components.classes["@mozilla.org/intl/converter-output-stream;1"].
          createInstance(Components.interfaces.nsIConverterOutputStream);
      fstream2.init(file, -1, 0, 0);
      cstream2.init(fstream2, Encoding, 0, 0);
      cstream2.writeString(data);
      cstream2.close();
      //logHelper.writelog("\n----INFO:"+ filePath + " file mod ok! ");
     // this.reslutsinfos += "\n----INFO:"+ filePath + "文件处理完成!";
  },


  file_addto: function(filePath,addcontent)
  {
    ////////////////////读文件///////////////////////
      var file  = new FileUtils.File(filePath);
      if (!file.exists()) {    // 若不存在，先创建一个空文件
        //alert(filePath + "文件不存在!");
        logHelper.writelog("\n----ERROR:"+ filePath + " file not existence! ");
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
      ///////////////////
      data += "\r" + addcontent;
///////////////////写文件///////////////

      var fstream2 = Components.classes["@mozilla.org/network/file-output-stream;1"].
          createInstance(Components.interfaces.nsIFileOutputStream);
      var cstream2 = Components.classes["@mozilla.org/intl/converter-output-stream;1"].
          createInstance(Components.interfaces.nsIConverterOutputStream);
      fstream2.init(file, -1, 0, 0);
      cstream2.init(fstream2, "UTF-8", 0, 0);

    
      cstream2.writeString(data);
      cstream2.close();
      //logHelper.writelog("\n----INFO:"+ filePath + " file add to ok! ");
  },

  file_replacefile: function (filesrcPath,fileobjPath,newfilename)
  {
    try
    {
      var filesrc  = new FileUtils.File(filesrcPath);
      if (!filesrc.exists()) {    // 若不存在，先创建一个空文件
        //alert(filesrcPath + "文件不存在!");
        logHelper.writelog("\n----ERROR:"+ filesrcPath + " file not existence! ");
        return;
        }      
      var fileobj  = new FileUtils.File(fileobjPath);
      if (!fileobj.exists()) {    // 若不存在，先创建一个空文件
        //alert(fileobjPath + "文件不存在!");
        logHelper.writelog("\n----ERROR:"+ fileobjPath + " file not existence! ");
        return;
        }      

        filesrc.copyTo(fileobj.parent,newfilename);
    }
    catch(e)
    {
        logHelper.writelog("\n----ERROR:"+ e );
    }    
  },

  file_rename: function (filesrcPath,newfilename)
  {
    try
    {
      var filesrc  = new FileUtils.File(filesrcPath);
      if (!filesrc.exists()) {    // 若不存在，先创建一个空文件
        //alert(filesrcPath + "文件不存在!");
        logHelper.writelog("\n----ERROR:"+ filesrcPath + " file not existence! ");
        return;
        }      
        filesrc.moveTo(filesrc.parent,newfilename);
    }
    catch(e)
    {
        logHelper.writelog("\n----ERROR:"+ e );
    }    
  },

};

