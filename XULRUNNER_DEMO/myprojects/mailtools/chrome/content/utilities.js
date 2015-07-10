
Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/FileUtils.jsm");
const {classes: Cc, interfaces: Ci, utils: Cu, Constructor: CC} = Components;
var promptHelper = {
  alert: function(text) {
        var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
            .getService(Components.interfaces.nsIPromptService);
        return promptService.alert(window, "mailtools", text);
    },
    alertEx: function(title, text) {
        var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
            .getService(Components.interfaces.nsIPromptService);
        return promptService.alert(window, title, text);
    },

    confirm: function(title, text) {
        var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
            .getService(Components.interfaces.nsIPromptService);

        return promptService.confirm(window, title, text);
    },

    confirmCheck: function(title, text, checkLabel, checkValue) {
        var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
            .getService(Components.interfaces.nsIPromptService);

        return promptService.confirmCheck(window, title, text, checkLabel, checkValue);
    },

    confirmSave: function(title, text) {
        var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
            .getService(Components.interfaces.nsIPromptService);

        return promptService.confirmEx(window, title, text,
            (promptService.BUTTON_TITLE_SAVE * promptService.BUTTON_POS_0) +
            (promptService.BUTTON_TITLE_CANCEL * promptService.BUTTON_POS_1) +
            (promptService.BUTTON_TITLE_DONT_SAVE * promptService.BUTTON_POS_2),
            null, null, null, null, {value:0});
    }
};

/*
var FileOperater = {

 modFile_AllReadthenWrite: function (filePath,srcStrArray,objStrArray)
  {
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
  },

  modFile_readbylinethenWrite: function (filePath,srcStrArray,objStrArray)
  {
      var file  = new FileUtils.File(filePath);
      if (!file.exists()) {    // 若不存在，先创建一个空文件
        alert(filePath + "文件不存在!");
        this.reslutsinfos +=   "\n----ERROR:"+filePath + "文件不存在!";
        return;
        }      
    // open an input stream from file
    var istream = Components.classes["@mozilla.org/network/file-input-stream;1"].
                  createInstance(Components.interfaces.nsIFileInputStream);
    istream.init(file, 0x01, 0444, 0);
    istream.QueryInterface(Components.interfaces.nsILineInputStream);

    // read lines into array
    var line = {}, lines = [], hasmore;
    do {
      hasmore = istream.readLine(line);
      lines.push(line.value); 
    } while(hasmore);

    istream.close();

    // do something with read data
    alert(lines);
  },
   
};
*/