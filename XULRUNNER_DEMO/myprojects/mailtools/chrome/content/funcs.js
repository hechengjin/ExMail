
Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/FileUtils.jsm");




var gFuncDialog = {
  file:null,
   
  onLoad: function ()
  {
    

  },
   
   /**
   * Convert an nsILocalFile instance into an nsIMsgAttachment.
   *
   * @param file the nsILocalFile
   * @return an attachment pointing to the file
   */
  FileToAttachment: function (file)
  {

    let fileHandler = Services.io.getProtocolHandler("file")
                              .QueryInterface(Components.interfaces.nsIFileProtocolHandler);
    let attachment = Components.classes["@mozilla.org/messengercompose/attachment;1"]
                               .createInstance(Components.interfaces.nsIMsgAttachment);

    var attachmentUrl = fileHandler.getURLSpecFromFile(file);
    return attachmentUrl;
  },

   browseForSelImFile: function ()
    {
      var restulsinfo = "";
      const nsIFilePicker = Components.interfaces.nsIFilePicker;
      var fp = Components.classes["@mozilla.org/filepicker;1"].createInstance(nsIFilePicker);

      fp.init(window, "选择导入的文件", nsIFilePicker.modeOpen);
      fp.appendFilter("*.*", "*.*");

      var ret = fp.show();
      var importFileUrlLocation = document.getElementById("importFileUrlLocation");
      if (ret == nsIFilePicker.returnOK ) 
      {
        if (fp.file && (fp.file.path.length > 0))
          this.file = fp.file;
        else
          this.file = null;

        restulsinfo +="\nfp.file.fileSize:"+fp.file.fileSize+"字节";
        
        restulsinfo +="\nfp.file.leafName:"+fp.file.leafName;
        
        restulsinfo +="\nfp.fileURL.spec:"+fp.fileURL.spec;

       // restulsinfo +="\n------------附件的url------";
       // var attachmentUrl = this.FileToAttachment(fp.file);
      //  restulsinfo +="\nattachmentUrl:"+attachmentUrl;
       restulsinfo +="\n------------decodeURI哪里定义?------";
       restulsinfo +="\ndecodeURI:"+ decodeURI(fp.fileURL.spec);

      }

        importFileUrlLocation.value = fp.fileURL.spec;
        if (importFileUrlLocation.value)
        {
           restulsinfo +="\n------------nnsILocalFile------";
            var shortname = this.convertURLToLocalFile(importFileUrlLocation.value).leafName;          
            var FullPathName = this.convertURLToLocalFile(importFileUrlLocation.value).path;  
            restulsinfo +="\nnsILocalFile.leafName:"+shortname;       
            restulsinfo +="\nnsILocalFile.path:"+FullPathName;       
            importFileUrlLocation.label = FullPathName;         
            importFileUrlLocation.image = "moz-icon://" + shortname + "?size=16";
        }
      
      document.getElementById("results").value = restulsinfo;
    },

    convertURLToLocalFile: function(aFileURL)
    {
      // convert the file url into a nsILocalFile
      if (aFileURL)
      {
        var ios = Components.classes["@mozilla.org/network/io-service;1"].getService(Components.interfaces.nsIIOService);
        var fph = ios.getProtocolHandler("file").QueryInterface(Components.interfaces.nsIFileProtocolHandler);
        return fph.getFileFromURLSpec(aFileURL);
      } 
      else
        return null;
    },
 };




