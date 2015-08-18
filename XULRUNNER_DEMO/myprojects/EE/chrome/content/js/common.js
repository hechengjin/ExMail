var rndtoday = new Date(); 
var rndseed = rndtoday.getTime(); 

function rnd() { 
	rndseed = (rndseed*9301+49297) % 233280; 
	return rndseed/(233280.0); 
}; 

function rand(number) { 
	return Math.floor(rnd()*number); 
};

/**
 * Get DOM Element(s) by Id. Missing ids are silently ignored!
 * 
 * @param ids
 *          One of more Ids
 * @return Either the element when there was just one parameter, or an array of
 *         elements.
 */
function $e() {
	if (arguments.length == 1) {
		return document.getElementById(arguments[0]);
	}
	let elements = [];
	for (let i = 0, e = arguments.length; i < e; ++i) {
		let element = document.getElementById(arguments[i]);
		if (element) {
			elements.push(element);
		}
		else {
			dumpErr("requested a non-existing element: " + arguments[i]);
		}
	}
	return elements;
}

function loadXML(xmlFile)
{
	try {
	var xmlhttp = new XMLHttpRequest;
	xmlhttp.open('GET', xmlFile, false);
	//xmlhttp.overrideMimeType('text/xml');
	xmlhttp.send(null);
	var xml = xmlhttp.responseXML;
	if (!xml) {
		alert("Unable to load "+xmlFile);
		return null;
	}
	return xml;
	} catch (e) {
		return null;
	}
}

function transformXML(xmlDoc, xslDoc, element)
{
	var XSLT = new XSLTProcessor;
	XSLT.importStylesheet(xslDoc);
	var e = $e(element);
	if (e) {
		while (e.firstChild) e.removeChild(e.firstChild);
		e.appendChild(XSLT.transformToFragment(xmlDoc, document));
	}
}

function setContent(xmlDoc, element)
{
	var e = document.getElementById(element);
	if (e) {
		while (e.firstChild) e.removeChild(e.firstChild);
		e.appendChild(xmlDoc);
	}
}


/*only for getting list item value*/
function get_item_id(elementId)
{
	var e = document.getElementById(elementId);
	var ret = -1;
	
	if (e == null) return ret;

	if (e.selectedItem != null) {
		ret = parseInt(e.selectedItem.value);
	} else if (e.getItemAtIndex(0) != null) {
		ret = parseInt(e.getItemAtIndex(0).value);
	}
	
	return ret;
}

function get_item_ids(elementId)
{
	var e = document.getElementById(elementId);
	var ret = new Array();
	
	if (e == null) return ret;

	if (e.selectedItems.length > 0) {
		for (i = 0; i < e.selectedItems.length; i++) {
			ret.push(parseInt(e.selectedItems[i].value));
		}

	} else if (e.getItemAtIndex(0) != null) {
		ret.push(parseInt(e.getItemAtIndex(0).value));
	}
	
	return ret;
}

/*only for list*/
function selectDefault(elementId, idx)
{
	var e = document.getElementById(elementId);
	if (e == null) return false;
	var count = e.getRowCount();
	if (count <= 0 ) return false;
	if (idx <0 || idx >= count) idx = 0;
	e.selectedIndex = idx;
	return true;	
}

function timeStr2Int(strTime)
{ /*xx:xx:xx.xxx to msec*/
	if (strTime == null) return 0;
	
	var h = parseInt(strTime.substring(0, 2), 10);
	var m = parseInt(strTime.substring(3, 5), 10);
	var s = parseInt(strTime.substring(6, 8), 10);
	var r = parseInt(strTime.substring(9, 12), 10);
//	alert( strTime + "->"+ h + " " + m + " " + s + " " + r);
	return (h * 3600 + m * 60 + s) * 1000 + r;
}

// decimal digits to hex
function decToHex(dec)
{
    var hexChars="0123456789ABCDEF";
    var hex="";
    while (dec>15)
    {
        var tmp = dec-(Math.floor(dec/16))*16;
        hex = hexChars.charAt(tmp)+hex;
        dec = Math.floor(dec/16);
    }
    hex = '0x' + hexChars.charAt(dec)+hex;
    return hex;
}

function refreshSubElements(id)
{
	var e = document.getElementById(id);
	var sub;
	if (!e) return;
	for (e = e.firstChild; e; e = e.nextSibling) {
		for (sub = e.firstChild; sub; sub = sub.nextSibling) {
			switch (sub.nodeName) {
			case "menulist":
				sub = sub.selectedItem;
				found = true;
				break;
			default:
				continue;
			}
			break;
		}
		if (!sub) continue;
		if (sub.onclick)
			sub.onclick();
		else if (e.onchange)
			sub.onchange();
	}
}

function moz_innerXML(node,strXml,isXul,insertBefore)
{
	var appendChild = true;
	if(insertBefore) {appendChild = false;}
	var doc = "";
	if(isXul) {
		var boxContainerOpen = '<box id="dvdripXulContainer" xmlns="http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul">'
		var boxContainerClose = '</box>';
		doc = (new DOMParser()).parseFromString(boxContainerOpen + strXml + boxContainerClose, "text/xml");
	}
	else{
		doc = (new DOMParser()).parseFromString("<root>"+strXml+"</root>", "text/xml");
	}

	if (doc.firstChild.tagName=="parsererror"){
		var parseError = new Error();
		parseError.name = 'ParseErrorEx';
		parseError.message += doc.firstChild.childNodes[0].nodeValue;
		throw(parseError);
	}
	else{
		doc=doc.firstChild;
		for (var i=0;i < doc.childNodes.length;i++) {
			if(appendChild) {
				node.appendChild(node.ownerDocument.importNode(doc.childNodes[i],true));
			}
			else{
				node.insertBefore(node.ownerDocument.importNode(doc.childNodes[i],true),node.firstChild);
			}
		}
	}
}

function ask(text, caption)
{
        var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                            .getService(Components.interfaces.nsIPromptService);
        var checkResult = {};
        var ret = promptService.confirmCheck(window, caption, text, null, checkResult);
        //return checkResult.value
        return ret;
        
}

function msgbox(text, caption)
{
        var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                            .getService(Components.interfaces.nsIPromptService);
        var ret = promptService.alert(window, caption, text);
}

function prompt(text, caption, defaultValue)
{
		var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                            .getService(Components.interfaces.nsIPromptService);
        var checkResult = {};
		var input = {value: defaultValue};
        var ret = promptService.prompt(window, caption, text, input, null, checkResult);
        return ret ? input.value : null;
}

function isChecked(id)
{
	var e = document.getElementById(id);
	return e && e.getAttribute("checked") == "true";
}

function getValue(id)
{
	return document.getElementById(id).value;
}

function setValue(id, value)
{
	return document.getElementById(id).value = value;
}

function getValueInt(id)
{
	return parseInt(document.getElementById(id).value);
}

function getValueFloat(id)
{
	return parseFloat(document.getElementById(id).value);
}

function getIndex(id)
{
	return document.getElementById(id).selectedIndex;
}

function getFileTitle(filename)
{
	var i = filename.lastIndexOf('\\');
	var str;
	if (i > 0)
		str = filename.substr(i + 1);
	else
		str = filename;
	i = str.lastIndexOf('.');
	if (i > 0)
		str = str.substr(0, i);

	/*		
	i = str.indexOf(" - ");
	if (i > 0) str = str.substr(i + 3);
	i = str.indexOf("-");
	if (i > 0) str = str.substr(i + 1);
	*/
	// str = replaceXmlChars(str);
	return str;
}

function getFileExt(filename)
{
	var i = filename.lastIndexOf('.');
	if (i < 0) return "";
	return filename.substr(i + 1).toUpperCase();
}

function getFileType(filename)
{
	/*var prefix = filename.substr(0, 4).toUpperCase();
	if(prefix == "HTTP") return "BrowserType";*/
	var ext = getFileExt(filename);
	switch (ext) {
	case "MPG":
	case "MPEG":
		return "MPEG";
	case "VOB":
		return "MPEG-2";
	case "TS":
	case "TP":
		return "MPEG-2 TS";
	case "RMVB":
	case "RM":
		return "RealMedia";
	case "WMV":
		return "Windows Media";
	case "MOV":
		return "QuickTime";
	case "JPG":
		return "JPEG Image";
	case "PNG":
		return "PNG Image";
	case "GIF":
		return "GIF Image";
	case "BMP":
		return "BMP Image";
	case "HTML":
	case "HTM":
	case "PHP":
	case "ASP":
	case "TXT":
	case "XSL":
	case "XML":
		return "Web Page";
	default:
		return ext;
	}
}

function getContentType(ext)
{
	switch (ext) {
	case "MPG":
	case "MPEG":
	case "MPE":
	case "M1V":
	case "M2V":
	case "DAT":
	case "TS":
	case "TP":
	case "MP4":
	case "M4V":
	case "M4P":
	case "MPV":
	case "VOB":
	case "OGM":
		
	case "MKV":
		
	case "RMVB":
	case "RM":
	
	case "AVI":	
	case "WMV":
	case "ASF":
	case "WMP":
	case "WM":
		
	case "MOV":
	case "QT":
	case "3GP":
	case "3GP2":
	case "3G2":
	case "3GPP":
		
	case "FLV":
	case "SWF":
		return CPlayer.CONTENT_VIDEO;
	
	case "MP3":
	case "AAC":
	case "AC3":
	case "OGG":
	case "M4A":
	case "MPA":
	case "WAV":
	case "MID":
	case "MIDI":
	
	case "WV":
	case "MKA":
	case "FLAC":
	case "RAM":
	case "RA":
		
	case "CDA":
		return CPlayer.CONTENT_AUDIO
	case "JPG":
	case "JPEG":
	case "PNG":
	case "GIF":
	case "BMP":
		return CPlayer.CONTENT_IMAGE;
	}
	return CPlayer.CONTENT_OTHER;
}


function loadValue(id)
{
	var e = document.getElementById(id);
	if (e) {
		var v = e.getAttribute("data");
		if (v) e.value = v;
	}
}

function saveValue(id)
{
	var e = document.getElementById(id);
	if (e) e.setAttribute("data", e.value);
}

function loadSelectedIndex(id)
{
	var e = document.getElementById(id);
	var v= e.getAttribute("data");
	if (v) e.selectedIndex = v;
}

function clearListBox(id)
{
	var i;
	var listbox = document.getElementById(id);
	while (listbox.removeItemAt(0));
}

function getAttr(id, attr)
{
	var e = document.getElementById(id); 
	if (e) return e.getAttribute(attr);
	return null;
}

function setAttr(id, attr, val)
{
	return document.getElementById(id).setAttribute(attr, val);
}

function enable(id)
{
	document.getElementById(id).setAttribute("disabled", false);
}

function disable(id)
{
	document.getElementById(id).setAttribute("disabled", true);	
}

function dupAttr(id, attrFrom, attrTo)
{
	var e = document.getElementById(id);
	if (e) {
		var v = e.getAttribute(attrFrom);
		if (v) e.setAttribute(attrTo, v);
	}
}

function getAttrInt(id, attr)
{
	var e = document.getElementById(id); 
	if (e) return parseInt(e.getAttribute(attr));
	return null;
}

function getAttrFloat(id, attr)
{
	var e = document.getElementById(id); 
	if (e) return parseFloat(e.getAttribute(attr));
	return null;
}

function padNumber2(num)
{
	return (num < 10 ? "0" : "") + num;
}

//Clear Array object
function clearArray(arr)
{
	var len = arr.length;
	for(var i=0; i<len; ++i)
		arr.pop();
}

//get a nsILocalFile object by given filePath (a dir or full file path)
function getLocalFileOrDir(filepath)
{
	var aFile = Components.classes["@mozilla.org/file/local;1"].createInstance();
	if(aFile) {
		aFile.QueryInterface(Components.interfaces.nsILocalFile);
        aFile.initWithPath(filepath);
		if(aFile && aFile.exists())
			return aFile;
	}
	return null;
}

/* 	Usage: Enum files in a folder
	Parameters: dir------------> directory path or a nsIfile object (directory)
				bRecursive-----> bool var, if true enum child folders, else only enum current dir.
				fileArr--------> for recursive call use. When we call this function, it can always be null.
	Return value: An array of files (every element is a nsIFile object)
*/
function enumFolder(dir, bRecursive, fileArr)
{
	if(!fileArr) fileArr = new Array();
	var topDir = null;
	if(!dir) return null;
	if(typeof(dir) == "string")
		topDir = Io.getFileFrom(dir);
	else
		topDir = dir;
	var files = topDir.directoryEntries;
	while(files.hasMoreElements()) {
		var afile = files.getNext();
		afile.QueryInterface(Components.interfaces.nsIFile);
		if(afile.isFile()) fileArr.push(afile);
		if(bRecursive && afile.isDirectory()) enumFolder(afile, bRecursive, fileArr);
	}
	return fileArr;
}

//retrieve the dir of filename
function getFileDir(filename)
{
	var i = filename.lastIndexOf('\\');
	var str = null;
	if (i > 0)
		str = filename.substr(0, i);
	return str;
}
//Get string form local properties
function getLocalString(stringId)
{
  //var src = "chrome://xulplayer/locale/xulplayer.properties";
  //var stringBundleService = Components.classes["@mozilla.org/intl/stringbundle;1"]
                    //.getService(Components.interfaces.nsIStringBundleService);
  //var bundle = stringBundleService.createBundle(src);
  //var str = bundle.GetStringFromName("subSyncTitle");
  var str = $e("localStrings").getString(stringId);
  return str ? str : null;
}

//Get formatted string from local properties
function getLocalFormatString(strId, otherStrs)
{
	var retStr = null;
	var strArr = new Array();
	if(arguments.length >=2){
		for(var i=1; i<arguments.length; ++i)
			strArr.push(arguments[i]);
	}
	//alert(typeof($e("localStrings").getFormattedString));
	retStr = $e("localStrings").getFormattedString(strId, strArr);
	return retStr;
}

// replace XML reserved characters
function replaceXmlChars(str)
{
	for (var i = 0; i < str.length; i++) {
		switch (str.charAt(i)) {
		case "&":
			str = str.substr(0, i) + "&amp;" + str.substr(i + 1)
			i += 4;
			break;
		case "<":
			str = str.substr(0, i) + "&lt;" + str.substr(i + 1)
			i += 3;
			break;
		case ">":
			str = str.substr(0, i) + "&gt;" + str.substr(i + 1)
			i += 3;
			break;
		}
	}
	return str;
}

// @Function: Convert filesize to string, append 'KB' or 'MB' to it
// @Param: Convert filesize to string, append 'KB' or 'MB' to it
// @return: string (normalized filesize string)
function normalizeFileSize(filesize)
{
	if(!filesize) return null;
	if(typeof(filesize) == 'string') {
		filesize = parseFloat(filesize);
	}
	var fileSizeStr = null;
	if (filesize > 1024) {
		filesize /= 1024;
		fileSizeStr = filesize.toFixed(1) + " MB";
	} else {
		fileSizeStr = filesize.toFixed(1) + " KB";
	}
	return fileSizeStr;
}

// Report error in javascript console under debug mode
if (typeof(dumpErr) == "undefined")
	const dumpErr = Components.utils.reportError;

// Report message in javascript console under debug mode
function dumpMsg(msgStr)
{
	Components.classes['@mozilla.org/consoleservice;1'].getService(Components.interfaces.nsIConsoleService).logStringMessage(msgStr);
}

function checkPlugin(name)
{
	var plugin;
	navigator.plugins.refresh(false);
	for (i in navigator.plugins) {
		plugin = navigator.plugins[i];
		if (plugin.name && plugin.name.indexOf(name) >= 0) {
			return true;
		}
	}
	return false;
}
