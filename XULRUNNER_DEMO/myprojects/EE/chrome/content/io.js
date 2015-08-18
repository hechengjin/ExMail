/* Module for File System IO operation. The class IO is just a namespace
   Created by Zoominla, 01-13-2009         
*/

if(typeof(IO_INCLUDE) != "boolean") {
	IO_INCLUDE = true;
    
    function Io()
    {
    }
    
    Io.IID_FilePicker = '@mozilla.org/filepicker;1';
    Io.IID_LocalFile = '@mozilla.org/file/local;1';
    Io.IID_InSteam = '@mozilla.org/network/file-input-stream;1';
    Io.IID_InSteamConvert = '@mozilla.org/intl/converter-input-stream;1';
    Io.IID_OutSteam = '@mozilla.org/network/file-output-stream;1';
	Io.IID_OutSteamConvert = '@mozilla.org/intl/converter-output-stream;1';
    Io.IID_DirService = '@mozilla.org/file/directory_service;1';
	
    Io.NORMAL_FILE_TYPE = Components.interfaces.nsIFile.NORMAL_FILE_TYPE;
    
    Io.IFile = Components.interfaces.nsIFile;
    Io.ILocalFile = Components.interfaces.nsILocalFile;
    Io.IFilePicker = Components.interfaces.nsIFilePicker;
    Io.IInStream = Components.interfaces.nsIFileInputStream;
    Io.IInStreamConverter = Components.interfaces.nsIConverterInputStream;
    Io.IInStreamUnicharLine = Components.interfaces.nsIUnicharLineInputStream;
    Io.IOutStream = Components.interfaces.nsIFileOutputStream;
	Io.IOutStreamConverter = Components.interfaces.nsIConverterOutputStream;
    
    // @Function: Open file dialog and get selected file or files
    // @Param: #title: Title for the open file dialog
	//         #filters: An array of filter strings, each item has a format("Text File|*.txt, *.xml") [optional]
    //         #bMultiple: Specify if enable multiple selection [optional]
    // @Return: An array of nsILocalFile objects
    Io.loadFile = function(title, filters, bMultiple) {
        var fp = Components.classes[Io.IID_FilePicker].createInstance(Io.IFilePicker);
        var openMode = bMultiple ? Io.IFilePicker.modeOpenMultiple : Io.IFilePicker.modeOpen;
        fp.init(window, title, openMode);
		
		if(filters instanceof Array) {
			for(var i=0; i<filters.length; ++i) {
				var filterTxt = filters[i];
				var splitIdx = filterTxt.indexOf('|');
				var filterTitle = filterTxt.substring(0, splitIdx);
				var filter = filterTxt.substr(splitIdx+1);
				fp.appendFilter(filterTitle, filter);
			}
		} else if(typeof(filters) == 'string'){
			var splitIdx = filters.indexOf('|');
			var filterTitle = filters.substring(0, splitIdx);
			var filter = filters.substr(splitIdx+1);
			fp.appendFilter(filterTitle, filter);
		} else {
			fp.appendFilters(Io.IFilePicker.filterAll);
		}
       
        var fileArr = new Array();
        var rv = fp.show();
        if (rv == Io.IFilePicker.returnOK) {
            if(bMultiple) {
                var files = fp.files;
                while (files.hasMoreElements()) {
                    var arg = files.getNext().QueryInterface(Io.ILocalFile);
                    fileArr.push(arg);
                }
            } else {
                fileArr.push(fp.file);
            }
        } 
        return fileArr;
    };

	// @Function: Get a folder
    // @Param: #title: open dialog title
    // @Return: nsILocalFile object
	Io.openDir = function(title)
	{
		var fp = Components.classes[Io.IID_FilePicker].createInstance(Io.IFilePicker);
		fp.init(window, title, Io.IFilePicker.modeGetFolder);
		var res = fp.show();
		if (res == Io.IFilePicker.returnOK){
			var dir = fp.file;
			return dir;
		}
		return null;
	}

    // @Function: Save text file
    // @Param: #strContent: The content of text
    //         #iFileOrPath: nsILocalFile object or save file path
    // @Return: undefine
    Io.saveFile = function(strContent, iFileOrPath, bOverwrite)
    {
        var outputStream = getOutputStream(iFileOrPath, bOverwrite);
        if(!outputStream) return;
        var result = outputStream.write(strContent, strContent.length);
        outputStream.close();
    };
	
	// @Function: Open text file
    // @Param:  #iFileOrPath: nsILocalFile object or save file path
    // @Return: A string array, each element is a line of text.
    Io.openTextFile = function(iFileOrPath)
    {
        var file = Io.getFileFrom(iFileOrPath);
        if(!file) return null;
		var istream = Components.classes[Io.IID_InSteam].createInstance(Io.IInStream);
		istream.init(file, 0x01, 0444, 0);
		istream.QueryInterface(Components.interfaces.nsILineInputStream);
		// read lines into array
		var line = {}, lines = [], hasmore = false;
		do {
			hasmore = istream.readLine(line);
			lines.push(line.value);
		} while(hasmore);
		istream.close();
		return lines;
    };
    
    // @Function: Save text file
    // @Param: #fnameOrIFile: nsILocalFile object or save file path
    // @Return: A string array, each element is a line of text.
    Io.openUTF8File = function(fnameOrIFile)
    {
        var charset = "UTF-8";
        if(!fnameOrIFile) return null;
        var file = Io.getFileFrom(fnameOrIFile);
        var istream = Components.classes[Io.IID_InSteam].createInstance(Io.IInStream);
        istream.init(file, 0x01, 0444, 0);
        istream.QueryInterface(Components.interfaces.nsILineInputStream);
        var iconvertStream = Components.classes[Io.IID_InSteamConvert].createInstance(Io.IInStreamConverter);
        iconvertStream.init(istream, charset, 1024, 0xFFFD);
        if (iconvertStream instanceof Io.IInStreamUnicharLine) {
            var line = {}, wholeContent = [];
            var hasmore = false;
            do {
                hasmore = iconvertStream.readLine(line);
                wholeContent.push(line.value);
            } while (hasmore);
            iconvertStream.close();
            return wholeContent;
        }
        istream.close();
        return null;
    };
    
	Io.create = function(iFileOrPath)
    {
        var localFile = Io.getFileFrom(iFileOrPath);
        if(localFile && !localFile.exists) localFile.create(Io.NORMAL_FILE_TYPE, 420);
    };
	
	// @Function: Create file. If file exists already then change file's leaf name, until create a unique file
    // @Param: #iFileOrPath: nsILocalFile object or save file path
    // @Return: nsILocalFile object whose file name may be changed.
	Io.createUnique = function(iFileOrPath)
    {
        var localFile = Io.getFileFrom(iFileOrPath);
		try {
			if(localFile) localFile.createUnique(Io.NORMAL_FILE_TYPE, 420);
		} catch(e) {
			return null;
		}
        return localFile;
    };
	
    Io.deleteFile = function(iFileOrPath)
    {
        var localFile = Io.getFileFrom(iFileOrPath);
        if(localFile) localFile.remove(false);
    };
    
	// @Function: Save UTF-8 text file
	// @Param: #strContent: text content
    // @Param: #iFileOrPath: nsILocalFile object or save file path
	// @Param: #bOverwrite: specify if overwrite file that exists already.
    // @Return: undefined.
    Io.saveUTF8File = function(strContent, iFileOrPath, bOverwrite)
    {
        var charset = "UTF-8";
        var outputStream = Io.getOutputStream(iFileOrPath, bOverwrite);
        if(!outputStream) return;
        var os = Components.classes[Io.IID_OutSteamConvert].createInstance(Io.IOutStreamConverter);
        os.init(outputStream, charset, 0, 0x0000);
        os.writeString(strContent);
        os.close(); 
        outputStream.close();
    };
    
	// @Function: Get custom dir under specil install dir 
	// @Param: #customDirName: (ex. 'playlists', 'lyrics')
    // @Param: #sysPathStringId: nsILocalFile object or save file path
    // @Return: nsILocalFile object.
	Io.getSpecialDir = function(customDirName, sysPathStringId)   	
	{
	  var installDir = Components.classes[Io.IID_DirService]
				   .getService(Components.interfaces.nsIProperties)
				   .get(sysPathStringId, Components.interfaces.nsIFile);
	  if(customDirName) installDir.append(customDirName);
	  return (installDir && installDir.exists()) ? installDir : null;
	}

    Io.getOutputStream = function(iFileOrPath, bOverwrite)
    {
		if(!iFileOrPath) return null;
		var file = Io.getFileFrom(iFileOrPath);
		if(!file) return null;
		
        var outputStream = null;
		var bindFile = null;
		
		if (!file.exists()) {
            file.create( Io.NORMAL_FILE_TYPE, 420 );
			bindFile = file;
		} else if(file.exists() && bOverwrite) {
            bindFile = file;
        } else {
			return null;
		}
		
        if(bindFile) {
            outputStream = Components.classes[Io.IID_OutSteam].createInstance(Io.IOutStream);
            outputStream.init(bindFile, 0x04 | 0x08 | 0x20, 420, 0 );
        }
        return outputStream;
    };
    
	
    // @Function: Ask privilege (helper function)
    Io.askPrivilege = function() {
        try {
            netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
        } catch (e) {
            alert("Permission to save file was denied.\n" + e);
        }
    };
    
    // @Function: Get the nsILocalFile object from a file object or a path (helper function)
    // @Param: #fnameOrIFile: nsILocalFile object or save file path
    // @Return: nsILocalFile object, if failed return null
    Io.getFileFrom = function(fnameOrIFile) {
        var afile = null;
        if(typeof(fnameOrIFile) == 'string'){
            try {
                afile = Components.classes[Io.IID_LocalFile].createInstance(Io.ILocalFile);
                afile.initWithPath(fnameOrIFile);
            } catch (e){
                return null;
            }
        } else if(fnameOrIFile && (fnameOrIFile instanceof Io.ILocalFile)){
            afile = fnameOrIFile;
        }
        return afile;
    };
}