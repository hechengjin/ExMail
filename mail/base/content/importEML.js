var gEMLTotal;
var gEMLimported;
var gPromptAlready;
var gSync;

function importEMLs()
{
    gPromptAlready = false;
    gSync = true;

    var selectFolders = GetSelectedMsgFolders();
    if (selectFolders.length == 0) {
        return;
    }

    var msgFolder = selectFolders[0];
    if (msgFolder.isServer)
        return;

    var bundle = document.getElementById("bundle_messenger");
    var importpoptitleInfo = bundle.getString("importpoptitle");
    var importEMLstartInfo = bundle.getString("importEMLstart");

    var nsIFilePicker = Components.interfaces.nsIFilePicker;
    var fp = Components.classes["@mozilla.org/filepicker;1"].createInstance(nsIFilePicker);
    fp.init(window, importpoptitleInfo, nsIFilePicker.modeOpenMultiple);
    fp.appendFilter("eml files", "*.eml; *.emlx; *.nws");
    var res=fp.show();
    if (res==nsIFilePicker.returnOK)
    {
        var thefiles=fp.files;
        var fileArray = new Array;
        // Files are stored in an array, so that they can be imported one by one
        while(thefiles.hasMoreElements())
        {
            var onefile= thefiles.getNext();
            onefile = onefile.QueryInterface(Components.interfaces.nsILocalFile);
            fileArray.push(onefile);
        }

        gEMLimported = 0;
        gEMLTotal = fileArray.length;
        IETwritestatus(importEMLstartInfo);
        var dir = fileArray[0].parent;
        beginToImportEML(fileArray[0],msgFolder,false, fileArray, false);
    }
    if( !gSync )
    {
        var observerService = Components.classes["@mozilla.org/observer-service;1"]
                .getService(Components.interfaces.nsIObserverService);
        observerService.notifyObservers(null, "importemailfinish", null);

        
    }


    

}

function manualSelectSync()
{
    gPromptAlready = true;

    var prompts = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
        .getService(Components.interfaces.nsIPromptService);

    var result = prompts.confirm(null, "同步到服务器", "当前使用的是IMAP协议，请问是否将导入文件同步到服务器");

    return result;
}

function beginToImportEML(file, msgFolder, removeFile, fileArray, allEML)
{
    if (file.path.indexOf(".emlx") > -1) {
        file =  IETemlx2eml(file);
    }

    var listener = importEMLlistener;
    importEMLlistener.msgFolder = msgFolder;
    importEMLlistener.removeFile = removeFile;
    importEMLlistener.file = file;
    importEMLlistener.fileArray = fileArray;
    importEMLlistener.allEML = allEML;


    if (msgFolder.server.type == "imap" )
    {
        importEMLlistener.imap = true;
        var cs = Components.classes["@mozilla.org/messenger/messagecopyservice;1"]
            .getService(Components.interfaces.nsIMsgCopyService);
        cs.CopyFileMessage(file, msgFolder, null, false, 1, "", importEMLlistener, msgWindow);
        if (! removeFile) {
            //gEMLimported = gEMLimported + 1;
            var bundle = document.getElementById("bundle_messenger");
            var numEMLInfo = bundle.getString("numEML");
            IETwritestatus(numEMLInfo+gEMLimported+"/"+gEMLTotal);
        }
        /*
        //  if newMsgsFlags + 0x8000, it means that don't sync server
        if (!gPromptAlready)
        {
            if (manualSelectSync())
            {
                gSync = true;
                cs.CopyFileMessage(file, msgFolder, null, false, 1, "import", importEMLlistener, msgWindow);
            }
            else
            {
                gSync = false;
                cs.CopyFileMessage(file, msgFolder, null, false, 1 + 0x8000, "import", importEMLlistener, msgWindow);
            }
        }
        else
        {
            if (gSync)
                cs.CopyFileMessage(file, msgFolder, null, false, 1, "import", importEMLlistener, msgWindow);
            else
                cs.CopyFileMessage(file, msgFolder, null, false, 1 + 0x8000, "import", importEMLlistener, msgWindow);
        }
        */
    }
    else
    {
        importEMLlistener.imap = false;
        var ios = Components.classes["@mozilla.org/network/io-service;1"]
            .getService(Components.interfaces.nsIIOService);
        var fileURI = ios.newFileURI(file);
        var channel = ios.newChannelFromURI(fileURI);
        channel.asyncOpen(listener, null);
    }
}


// only call OnStartCopy , onStopCopy when server.type = "imap"
var importEMLlistener = {

    OnStartCopy : function() {},

    OnStopCopy : function() {
        if (this.removeFile)
            this.file.remove(false);

        gEMLimported = gEMLimported + 1;
        importEMLlistener.next();
    },

    SetMessageKey : function(aKey) {},

    onStartRequest: function (aRequest, aContext) {
        this.mData = "";
    },

    onDataAvailable: function (aRequest, aContext, aStream, aSourceOffset, aLength) {
        // Here it's used the nsIBinaryInputStream, because it can read also null bytes
        var bis = Components.classes['@mozilla.org/binaryinputstream;1']
            .createInstance(Components.interfaces.nsIBinaryInputStream);
        bis.setInputStream(aStream);
        this.mData += bis.readBytes(aLength);
    },

    onStopRequest: function (aRequest, aContext, aStatus) {
        var text = this.mData;
        try {
            var index = text.search(/\r\n\r\n/);
            var header = text.substring(0,index);
            if (header.indexOf("Date: =?") > -1) {
                var mime2DecodedService = Components.classes["@mozilla.org/network/mime-hdrparam;1"]
                    .getService(Components.interfaces.nsIMIMEHeaderParam);
                var dateOrig = header.match(/Date: \=\?.+\?\=\r\n/).toString();
                var dateDecoded = "Date: "+mime2DecodedService.getParameter(dateOrig.substring(6), null, "", false, {value: null})+"\r\n";
                header = header.replace(dateOrig, dateDecoded);
            }
            var data = header+text.substring(index);
            var data = text;
        }
        catch(e) {}

        if (! this.imap)
        {
            writeDataToFolder(data,this.msgFolder,this.file,this.removeFile);
        }
        importEMLlistener.next();
    },

    next : function() {
        /*
        if (this.allEML && gEMLimported < gFileEMLarray.length) {
            var nextFile = gFileEMLarray[gEMLimported].file;
            beginToImportEML(nextFile,gFileEMLarray[gEMLimported].msgFolder,this.removeFile, this.fileArray,this.allEML);
        }
        else
        */
        if (this.fileArray && gEMLimported < this.fileArray.length) {
            var nextFile = this.fileArray[gEMLimported];
            beginToImportEML(nextFile,this.msgFolder,this.removeFile, this.fileArray, false);
        }
        else {
            // At the end we update the fodler view and summary
            this.msgFolder.updateFolder(msgWindow);
            this.msgFolder.updateSummaryTotals(true);
            var bundle = document.getElementById("bundle_messenger");
            var prompts = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                .getService(Components.interfaces.nsIPromptService);
            var importpopInfo = bundle.getString("rtnRptsFailedTitle");
            var importedfinishedInfo = bundle.getString("importedfinished");
            var result = prompts.alert(null, importpopInfo, importedfinishedInfo);

            this.msgFolder.updateFolder(msgWindow);
            this.msgFolder.updateSummaryTotals(true);
        }
    },

    QueryInterface : function(aIID) {
        if (aIID.equals(Components.interfaces.nsISupports) ||
            aIID.equals(Components.interfaces.nsIInterfaceRequestor) ||
            aIID.equals(Components.interfaces.nsIChannelEventSink) ||
            aIID.equals(Components.interfaces.nsIProgressEventSink) ||
            aIID.equals(Components.interfaces.nsIHttpEventSink) ||
            aIID.equals(Components.interfaces.nsIStreamListener))
            return this;

        throw Components.results.NS_NOINTERFACE;
    }
};

function writeDataToFolder(data,msgFolder,file,removeFile) {
    var msgLocalFolder = msgFolder.QueryInterface(Components.interfaces.nsIMsgLocalMailFolder);

    // strip off the null characters, that break totally import and display
    data = data.replace(/\x00/g, "");
    var now = new Date;
    try {
        var nowString = now.toString().match(/.+:\d\d/);
        nowString = nowString.toString().replace(/\d{4} /, "");
        nowString = nowString + " " + now.getFullYear();
    }
    catch(e) {
        var nowString = now.toString().replace(/GMT.+/, "");
    }

    var top  = data.substring(0,2000);

    // Fix for crazy format returned by Hotmail view-source
    if (top.match(/X-Message-Delivery:.+\r?\n\r?\n/) || top.match(/X-Message-Info:.+\r?\n\r?\n/) )
        data = data.replace(/(\r?\n\r?\n)/g, "\n");

    // Fix for some not-compliant date headers
    if (top.match(/Posted-Date\:/))
        data = data.replace("Posted-Date:", "Date:");
    if (top.match(/X-OriginalArrivalTime:.+\r?\n\r?\n/))
        data = data.replace("X-OriginalArrivalTime:", "Date:");
    // Some eml files begin with "From <something>"
    // This causes that Thunderbird will not handle properly the message
    // so in this case the first line is deleted
    data = data.replace(/^From\s+.+\r?\n/, "");

    // Prologue needed to add the message to the folder
    var prologue = "From - " + nowString + "\n"; // The first line must begin with "From -", the following is not important
    // If the message has no X-Mozilla-Status, we add them to it
    if (data.indexOf("X-Mozilla-Status") < 0)
        var prologue = prologue + "X-Mozilla-Status: 0000\nX-Mozilla-Status2: 00000000\n";
    else {
        // Reset the X-Mozilla status
        data = data.replace(/X-Mozilla-Status: \d{4}/, "X-Mozilla-Status: 0000");
        data = data.replace(/X-Mozilla-Status2: \d{8}/, "X-Mozilla-Status2: 00000000");
    }

    // If the message has no X-Account-Key, we add it to it, taking it from the account selected
    if (data.indexOf("X-Account-Key") < 0) {
        var myAccountManager = Components.classes["@mozilla.org/messenger/account-manager;1"]
            .getService(Components.interfaces.nsIMsgAccountManager);
        var myAccount = myAccountManager.FindAccountForServer(msgFolder.server);
        prologue = prologue + "X-Account-Key: " + myAccount.key + "\n";
    }

    data = IETescapeBeginningFrom(data);

    // Add the prologue to the EML text
    data = prologue + data + "\n";
    // Add the email to the folder

    msgLocalFolder.addMessage(data);
    gEMLimported = gEMLimported + 1;

    if (removeFile)
        file.remove(false);

}

function IETescapeBeginningFrom(data) {
    // Workaround to fix the "From " in beginning line problem in body messages
    // See https://bugzilla.mozilla.org/show_bug.cgi?id=119441 and
    // https://bugzilla.mozilla.org/show_bug.cgi?id=194382
    // TB2 has uncorrect beahviour with html messages
    // This is not very fine, but I didnt' find anything better...
    var datacorrected = data.replace(/\nFrom /g, "\n From ");
    return datacorrected;
}
