/* All operations and setting related to user interface, detached from
   player.js. 
   Created by Zoominla, 12-25-2008         
*/
if(typeof(XULPLAYER_GUI_INCLUDE) != "boolean") {
	XULPLAYER_GUI_INCLUDE = true;
    
if (typeof(Cc) == "undefined")
    var Cc = Components.classes;
if (typeof(Ci) == "undefined")
    var Ci = Components.interfaces;
const IID_Prefs = "@mozilla.org/preferences-service;1";

//var ac = Components.classes["@mozilla.org/steel/application;1"].getService(Components.interfaces.steelIApplication).console;



var mainwnd;
var plMan = null;				// Play lists manager
var mm = null;					// Media library manager
var playFromLibrary = false;	// Indicate current play is from meida library or playlist

var progressbar;
var playButtons = new Array("play_icon", "stop_icon");
var listItemIds = new Array("mainTab");
var fileInfoXML = null;

var focusFromPlugin = false;
var homeURL = null;
var subfont = null;
var subFile = null;
var mediaLibPath;

var playType = PLAY_TYPE_IDLE;
var locale;
var holdOnProgress = false;

var localhost = "http://127.0.0.1:19830/";
var localHttpActive = false;

var hasInternet = false;

var mp = null;
var mc = null;
var swfPlayer = null;		// Control the playing of SWF file
var userDataPath = null;	// Place playlist file and lyrics file
var supportFormats = ["MPG", "MPEG", "MPE", "M1V", "M2V", "DAT", "TS", "TP",
					  "MP4", "M4V", "M4P", "MPV",
					  "VOB", "OGM", "MKV",
					  "RMVB", "RM",
					  "AVI", "WMV", "ASF", "WMP", "WM",
					  "MOV", "QT", "3GP", "3GP2", "3G2", "3GPP",
					  "FLV", "SWF",
					  "MP3", "OGG", "FLAC", "APE",
					  "AAC", "AC3", "M4A", "MPA",
					  "WAV", "MID", "MIDI", "WV", "WMA",
					  "MKA", "CDA",
					  "RAM", "RA",
					  "JPG", "JPEG", "PNG", "GIF", "BMP"];
var supportFormatString = supportFormats.join(',');

var transcodedFileObject = {};	// Save file fullpath and corresponding transcoded fullpath
var curTranscodeFile = '';		// Save currently transcoding file

var xmlhttp = new XMLHttpRequest;

//////////////////////////
var mediaResourcePath;
///////////////////////////

/*============================================================
  A list of File types used :
	   Image: image type (ex. BMP, GIF, PNG, JPG, etc)
	   Url: this type stands for web page (ex. html, asp, php)
	   Stream: Streams on the internet, (ex. http, rtsp, mms)
	   SWF: flash video
	   Common media type (ex. AVI, MP4, MP3, RMVB, MOV, etc.)
=============================================================*/

function init()
{
	//try { netscape.security.PrivilegeManager.enablePrivilege('UniversalXPConnect'); }
	//catch  (e) { alert ('Error setting privilege.'); }

	var prefs = Cc[IID_Prefs].getService(Ci.nsIPrefBranch);
	locale = prefs.getCharPref("general.useragent.locale");
	mediaResourcePath = Io.getSpecialDir("Resource", "CurProcD").path;
	//alert(mediaResourcePath) //E:\github\Firemail\ExMail\XULRUNNER_DEMO\myprojects\EE\Resource
/*
	mp = new CPlayer(player);
	mc = new CConvert(player);
	
	// Initialize the visible elements of the GUI, then startup
	//for (var x in listItemIds) loadSelectedIndex(listItemIds[x]);
*/
	
	//Initialize user data path
	initUserDataPath();
	//refreshEffectsTab();
	//mediaLibPath = Io.getSpecialDir("defaults", "CurProcD").path;
	//toggleMediaLib();

	plMan = new CPlaylistMan('listManager');
	plMan.enableRandom(isChecked('opt-shuffle'));
	plMan.enableRepeat(isChecked('opt-repeat'));
	plMan.initialize();
	//if (window.initProjectorOpts) initProjectorOpts();
	
	
	// initialize player and events handler after GUI show up
	mainwnd = $e("main");
	progressbar = $e("timebar");
	swfPlayer = new SwfPlayer();
	swfPlayer.setLoopCallback(swfPlayCallback);
	swfPlayer.setStopCallback(swfStopCallback);

	
	// initialize Drag & Drop Event
	//initDragDropEvent();
	//initSkinMenuItems();
	//window.addEventListener("focus", focusMethod, true);
	// initialize player
/*
	mp.initialize();
	mp.setStartCallback(onPlayerStart);
	mp.setStopCallback(onPlayerStop);
	mp.setPauseCallback(onPlayerPaused);
	mp.setLoopCallback(playLoop);

	// initialize converter
	mc.setLoopCallback(encodeLoopCallback);
	mc.setStopCallback(encodeStopCallback);

	// update volume scale
	$e("volscale").value = mp.getVolume();
*/
	// schedule a CPU info display
	//self.setTimeout(showCPUInfo, 3000);
/*	
	if (window.MEDIA_MANAGER_INCLUDE) {
		mm = new MediaMan();
	} else {
		$e("optMediaCenterMode").hidden = true;
	}
*/
	// load files from command line
	//parseCommandLine();
	initResources();
	/*
	showMediaInfo();
	//toggle DualMode after process command line
	toggleDualMode();

	// set view mode
	mode = parseInt($e('optViewMode').value);
	if (mode >= VIEW_TV) mode = VIEW_FULL;
	setViewMode(mode);

	// load mcex.org
	var mcexurl = "http://www.mcex.org/";
	if (locale.length >= 2) mcexurl += "?lang=" + locale.substr(0, 2);
	liteBrowserInit(mcexurl);
	//browseNavigate("http://127.0.0.1:19830/extensions/nokia_6122/index.htm");
	
	try {
		downloader = new CDownloader(player);
	}
	catch (e) {
		dumpErr("init():" + ex);
	}
	
	// show initial prefs sidebar
	showPrefsTab(false);
	*/
}

function uninit()
{
	if(plMan) plMan.save();
	dumpMsg("444444444444444");
	/*
	mp.unInitialize();
	if (window.uninitProjectorOpts) uninitProjectorOpts();
	try {
		if(mm) mm.closeMediaDb();
	} catch(e) {
		dumpMsg(e);
	}
	*/
}

const MCEX_MIN_WIDTH = 900;
const MCEX_MIN_HEIGHT = 720;
const NORMAL_MIN_WIDTH = 500;
const NORMAL_MIN_HEIGHT = 400;

function changeVideoBoxPage(page_index)
{
    $e("videoBox").selectedIndex = page_index;
    if(page_index != INDEX_TVU)
    {
        var nettv = $e("net_tv_frame");
        if(nettv.getAttribute("src"))
        {
            window.frames["net_tv_frame"].stop();
        }
    }
}

function startPlay(filename, type, bSinglePlay)
{
	//alert("startPlay "+filename+ " "+type+ " "+bSinglePlay)
	if(swfPlayer.isPlaying()) {
		swfPlayer.stop();
	}
	
	if (!filename) {
		if(playFromLibrary) {
			if(mm) filename = mm.getCurrentFile();
		} else {
			if(plMan) filename = plMan.getCurrentFile();
		}
		if(!filename) return;
	}
	/*
	mp.setPlayingFile(filename);
	if(bSinglePlay) {
		mp.enablePlayNext(false);
	} else {
		mp.enablePlayNext(true);
	}
	*/
	if (!type) {
		type = getFileType(filename);
	}
	if(type.indexOf("Image")!=-1 ) {    // Images
		mp.stop();
		loadPicture("html/navpic/onepicture.html", filename);
		mp.enablePlayNext(false);
        playType = PLAY_TYPE_IMAGE;
		return;
	} else if (type == "Web Page"){   // Local pages
        mp.stop();
		loadPage("file:///"+filename);
		mp.enablePlayNext(false);
        playType = PLAY_TYPE_URL;
        return;
    } else if (type == "Url") {         //Weg pages on internet
		mp.stop();
		loadPage(filename);
		playType = PLAY_TYPE_URL;
		return;
	} else if (type == "SWF") {
		//mp.stop();
		playSwf(filename);
        playType = PLAY_TYPE_SWF;
		return;
	}
	
	var opts = getPlayCommandOptions();
	var detached = isChecked("menuDetached");
	playType = PLAY_TYPE_FILE;
    if (filename.indexOf('dvd://') == 0 || filename.indexOf('vcd://') == 0 ||
        filename.indexOf('cd://') == 0) {
        var dvdpath;
        dvdpath = get_dvd_cmd(filename);
        mp.playDevice(dvdpath, opts, !detached);
    } else {
	playType = PLAY_TYPE_FILE;
        mp.play(opts, !detached);       
    }
	
    changeVideoBoxPage(INDEX_VIDEO);
	$e("stats").label = filename;
	
	if(!mp.isPlaying()) {
		loadPage(homeURL);
	}
	
	// look for lyric file and 
	findLyricAndDisplay();
	showMediaInfo(filename);
}

// Open Stream. ex: http, rtsp, mms
function openStream()
{
	var tips = getLocalFormatString("tips.openUrl.prompt", "Xulplayer", "http, rtsp, mms");
	var urls = prompt(tips, getLocalString("tips.openUrl.title"), "");
	if (urls) {
		addFile(urls, 0);
		startPlay(urls);
	}
}

function play_cmd()
{
    if(isTvMode())
    {
        getTvFrame().play();
        setAttr("play_icon", "command", "cmdPause");
        for (var i = 1; i < playButtons.length; i++) {
			enable(playButtons[i]);
		}
		var duration = 0;		
		setAttr("btn_screenshot", "disabled", true);
        return;
    }
    
	if(mp.isPaused() || mp.isPlaying()) {
		pause_cmd();
	} else {
		var playFile = null;
		if(playFromLibrary) {
			if(mm)  {
				playFile = mm.getCurrentFile();
				if(!playFile) playFile = mm.getSelectedFile();
			}
		} else {
			if(plMan) {
				playFile = plMan.getCurrentFile();
				if(!playFile) playFile = plMan.getSelectedFile();
			}
		}
		
		if(!playFile) {
			playFile_cmd();
			return;
		}
		startPlay(playFile);
	}
}

function pause_cmd()
{
    if(isTvMode())
    {
    }
    
	if(playType == PLAY_TYPE_SWF) {
		swfPlayer.pause();
	} else {
		mp.pause();
	}
}

// Open a file and play it
function playFile_cmd()
{
	var fileName = openFile();
	startPlay(fileName);
}

function stop_cmd()
{    
    if(isTvMode())
    {
        getTvFrame().stop();
        setAttr("play_icon", "command", "cmdPlay");
        disable("stop_icon");
        return;
    }
    
    mp.enablePlayNext(false);
	mp.stop();
	subFile = null;
	if(playType == PLAY_TYPE_SWF) {
		swfPlayer.stop();
		if(swfPlayer.isPlaying()) return;
	}
	if ($e("videoBox").selectedIndex == INDEX_PAGES) {
		loadPage("html/videobox.html");
	}
    playType = PLAY_TYPE_IDLE;
}

function prev_cmd()
{
    if(isTvMode())
    {
        getTvFrame().goNext(-1);
        return;
    }
    
	if(playFromLibrary) {
		if(mm) mm.movePrevious();
	} else {
		if(plMan) plMan.movePrevious();
	}
	startPlay();
}

function next_cmd()
{
    if(isTvMode())
    {
        getTvFrame().goNext(1);
        return;
    }
    
	if(playFromLibrary) {
		if(mm) mm.moveNext();
	} else {
		if(plMan) plMan.moveNext();
	}
	startPlay();
}

function opendir_cmd()
{
	var dir = Io.openDir('Open Folder');
	if (dir) {
		$e('dirtext').value = dir.path;
	}
}

function updatePlayerState()
{
	if (mp.isPlaying()) {
		setAttr("play_icon", "command", "cmdPause");
		//mainwnd.setAttribute("title", "&player.title; - " + playingFile[playerIndex]);
		for (var i = 1; i < playButtons.length; i++) {
			enable(playButtons[i]);
		}
		var duration = mp.getDuration();
		if (duration > 0) {
			enable("timebar");
			progressbar.setAttribute("max", duration);
		} else {
			disable("timebar");
		}
		setAttr("btn_screenshot", "disabled", !mp.hasVideo());
	} else {
		setAttr("play_icon", "command", "cmdPlay");
		for (var i = 1; i < playButtons.length; i++) {
			disable(playButtons[i]);
		}
		disable("timebar");
		showSystemTime();
		disable("btn_screenshot");
		if(!mp.playNextEnabled() && !swfPlayer.isPlaying()) {
		    loadPage(homeURL);
		}
		if(mp.playNextEnabled() && !dualmode && !swfPlayer.isPlaying()) {
			next_cmd();
		}
	}
}

function onPlayerStart()
{
	// load file info from plugin;
	var xmlInfo = mp.getClipInfoXML();
	if(xmlInfo) {
		fileInfoXML = new XMLDom();
		fileInfoXML.loadXML(xmlInfo);
	}
	showFileInfo();
	updatePlayerState();
}

function onPlayerStop()
{
	updatePlayerState();
}

function onPlayerPaused()
{
	if(!mp.isPaused()) {
		setAttr("play_icon", "command", "cmdPlay");
	} else {
		setAttr("play_icon", "command", "cmdPause");
	}
}

function playLoop()
{
	// update progress bar
	if (holdOnProgress) return;
	var pos = mp.getCurPos();
	var duration = mp.getDuration();
	if (pos >= 0) {
		$e("state").label = mp.getTimeString(pos / 1000) + " / " + mp.getTimeString(duration / 1000);
		progressbar.value = pos;
		//update lyric
		if(ifDisplayLyric) updateLyricDisplay(pos);
		if(fs) showOrHideProgressbar();
	} else {
		showSystemTime();
	}
}

function showSystemTime()
{
	var d = new Date();
	$e("state").label = padNumber2(d.getHours()) + ":" + padNumber2(d.getMinutes()) + ":" + padNumber2(d.getSeconds());
	progressbar.value = 0;
}

function seekToClickPos(e)
{
	// Progressbar event process
	if(!e) {
		e = window.event;
	}
	
	var barLeft = progressbar.boxObject.x;
	var barWidth = progressbar.boxObject.width;
	if(!barWidth && barWidth <= 0) return;
	var eventX = e.clientX;
	var percent = (eventX-barLeft)/barWidth;
	var pos = percent * mp.getDuration();
	if(playType == PLAY_TYPE_SWF) {
		swfPlayer.gotoFrame(pos);
	} else {
		pos /= 1000.0;
		mp.seekToPos(pos);
	}
	//$e("play-toolbar").focus();
}

function switchAudio()
{
	var ret = mp.switchAudio();
	$e("stats").label = ret;
}

function refreshEffectsTab()
{
	var s = $e("tabEffects").getElementsByTagName("scrollbar");
	var i;
	for (i = 0; i < s.length; i++) {
		s[i].onmouseup();
	}
}

function applyPlayerOptions()
{
	if (mp.isPlaying()) {
		var pos = mp.m_curPos;
		mp.stop();
		startPlay(mp.getPlayingFile());
		pos /= 1000.0;
		mp.seekToPos(pos);
	}
}

function switchPlayer(index)
{
	$e('dualVideoOptions').selectedIndex = index;
	mp.setActivePlayer(index);
	$e("stats").label = mp.getPlayingFile();
	updatePlayerState();
}

function takeScreenshot()
{
	var result = mp.screenshot();
	var pngName = new String();
	if(!result) return;
	var index1 = result.indexOf("'");
	var index2 = result.lastIndexOf("'");
	pngName = result.substring(index1+1, index2);
    var tipTitle = getLocalString("tips.shot.title");
    var promptStr = getLocalFormatString("tips.shot.prompt", pngName);
	if (isChecked("tipScreenshot")) msgbox(promptStr, tipTitle);
}

function takeImgCallBack(srcfile, inW, inH, ss, outW, outH, outWMax, outHMax, destDir, destName)
{
    mc.makeShot(srcfile, inW, inH, ss, outW, outH, outWMax, outHMax, destDir, destName);
}

function takeImgShots()
{
    var filePath = plMan.getSelectedFile();
    if(!filePath)
    {
        alert(getLocalString('imgshots.tips.nofile'));
        return;
    }
    //parse
     if(!mc.dumpMediaInfo) {
        alert(getLocalString('imgshots.tips.dumpinfofail'));
        return;
    }
    var resXml = mc.dumpMediaInfo(filePath, false);
    if(resXml == null)
    {
        alert(getLocalString('imgshots.tips.unsupportedfile'));
        return;
    }
    var xmlDoc = new XMLDom();
    xmlDoc.loadXML(resXml);
    var rootNode = DomHelper.getChildNode(xmlDoc);
    var node = DomHelper.getChildNode(rootNode);
    var vwidth = DomHelper.getNodeValueByKey(node, "video.width");
    vwidth = parseInt(vwidth);
    var vheight = DomHelper.getNodeValueByKey(node, "video.height");
    vheight = parseInt(vheight);
    var vdur = DomHelper.getNodeValueByKey(node, "overall.duration");
    vdur = parseInt(vdur) / 1000;
    vdur = parseInt(vdur);
   // alert(vdur);
    if(!vheight || !vwidth || !vdur)
    {
         alert(getLocalString('imgshots.tips.unsupportedfile'));
        return;
    }
    
    window.openDialog('tools/imgshots.xul','Super Shots',
                    'chrome, titlebar, centerscreen,modal=no,resizable=no,depended=yes,alwaysRaised=yes,outerHeight=0,outerWidth=0',
                    {width:vwidth,height:vheight,duration:vdur,filePath:filePath}, takeImgCallBack);
}

function addDvd()
{
    window.openDialog('dvdpicker/dvdpicker.xul','DVD picker',
                    'chrome, titlebar, centerscreen,modal=no,resizable=no,depended=yes,alwaysRaised=yes,outerHeight=0,outerWidth=0',
		    player, add_dvd);
	
}
function openDownloader()
{
    window.openDialog('downloader/manager.xul','Download Manager',
                    'chrome, centerscreen, dialog=no, dependent=yes', downloader);
}

function resetSubSync()
{
	var tipTitle = getLocalString("tips.syncSub.title");
    var promptStr = getLocalString("tips.syncSub.prompt");
    var delay = prompt(promptStr, tipTitle, 0);
	if(!delay) return;
	delay = parseInt(delay);
	mp.resetSubDelay(delay);
}

function resetAoSync()
{
    var tipTitle = getLocalString("tips.syncAudio.title");
    var promptStr = getLocalString("tips.syncAudio.prompt");
	var delay = prompt(promptStr, tipTitle, 0);
	if(!delay) return;
	delay = parseInt(delay);
	mp.resetAudioDelay(delay);
}

function loadSubFont()
{
	var loadFontTitle = getLocalString("tips.openFont.title");
	var files = Io.loadFile(loadFontTitle, 'Font|*.ttf');
	if(files instanceof Array) {
		subfont = files[0].path;
	}
	if(subfont) applyPlayerOptions();
 
}
function loadSubFile()
{
	var loadFileTitle = getLocalString("tips.openSub.title");
	var filter = 'Subtitle(*.srt, *.sub, *ssa, *ass)|*.srt; *.sub; *ssa; *ass';
	var files = Io.loadFile(loadFileTitle, filter);
	if(files instanceof Array) {
		subFile = files[0].path;
	}
	if(subFile) applyPlayerOptions();
}

function addFolder(recurse)
{
    var openDirTitle = getLocalString("tips.openDir.title");
    var dir = Io.openDir(openDirTitle);
	if(!dir) return;
	var filePaths = [];
	var fileSizes = [];
	addSubFolders(dir, recurse, filePaths, fileSizes);
	if(filePaths.length == 0) return;
	if (!mp.isPlaying()) {
		startPlay(filePaths[0]);
	}
	
	for(var i=0; i<filePaths.length; ++i) {
		addFile(filePaths[i], fileSizes[i] / 1024);
	}
	
	//$e("fullView").setAttribute("checked", true);
	//setViewMode();
}

// Enum dir and get all files in it and subfolders
// @Params: dir--the top directory;
//			recursive--a bool variable, indicate whether traverse all it's subfolders;
//			filePathArr--An output array, cache all file paths
//			fileSizeArr--An output array, cache all file size strings [Optional]
function addSubFolders(dir, recursive, filePathArr, fileSizeArr)
{
	if(!(filePathArr instanceof Array)) return;
	var files = dir.directoryEntries;
	while(files.hasMoreElements()) {
		var afile = files.getNext();
		afile.QueryInterface(Components.interfaces.nsIFile);
		//dumpMsg(afile.path);
		if(afile.isFile()) {
			var fullPath = afile.path;
			var ext = getFileExt(fullPath);
			if(ext && supportFormatString.indexOf(ext) != -1) {
				filePathArr.push(fullPath);
				if(fileSizeArr) {
					fileSizeArr.push(afile.fileSize);
				}
			}
		} else if(afile.isDirectory() && recursive) {	// Avoid script no responding when there is lots of folders
			addSubFolders(afile, recursive, filePathArr, fileSizeArr);
		}
	}
}

//--------------------------------------------------------------------------------------
// Add file to current playlist
function addFile(filename, filesize)
{
	if (!filename) return;
	var fileType = "";
	var fileSizeStr = "";
	
	if (filesize && filesize == -1) {		// url type
		fileType = "Url";
	} else {
		if(filesize) {
			fileSizeStr = normalizeFileSize(filesize);
		}
		fileType = getFileType(filename);
	}
	if(plMan) plMan.addFile(filename, fileSizeStr, fileType);
}

function addFileEx(gruoppath, filename, filesize)
{
	if (!filename) return;
	var fileType = "";
	var fileSizeStr = "";
	
	if (filesize && filesize == -1) {		// url type
		fileType = "Url";
	} else {
		if(filesize) {
			fileSizeStr = normalizeFileSize(filesize);
		}
		fileType = getFileType(filename);
	}
	if(plMan) plMan.addFileEx(gruoppath, filename, fileSizeStr, fileType);
}


// Open file dialog and add file to current playlist
function openFile()
{
	//the type of localFile is nsILocalFile
	var filters =  ['All|*.*', 'AVI|*.avi', 'MP4|*.mp4', 'Windows Media|*.asf; *.wmv; *.wma; *.wav',
					'MPEG|*.mpg; *.mpeg; *.mpe; *.m1v; *.m2v; *.dat; *.ts; *.tp;',
                    'MP3|*.mp3', 'FLAC|*.flac', 'Vorbis|*.ogg', 'APE|*.ape',
					'RealMedia|*.rmvb; *.rm', 'QuickTime|*.mov; *.mp4'
				   ];
	var localFiles = Io.loadFile("Play File", filters, true);
	var filename = null;
	if(localFiles instanceof Array && localFiles.length > 0) {
		/*if (!mp.isPlaying()) {
			startPlay(localFiles[0].path);
		}*/
		for(var i=0; i<localFiles.length; ++i) {
			var filesize = localFiles[i].fileSize;
			filesize /= 1024;	//Convert to KB
			filename = localFiles[i].path;
			// add file to playlist
			addFile(filename, filesize);
		}
	}
	return filename;
}

function add_dvd(dvdpath)
{
    if(plMan)
    {
        var colonIdx = dvdpath.indexOf(':');
        if(colonIdx != -1) {
            var typeStr = dvdpath.substring(0, colonIdx);
            var conttype = CPlayer.CONTENT_OTHER;
            switch(typeStr)
            {
               case "dvd":
                conttype = CPlayer.CONTENT_DVD;
                break;
             case "vcd":
                conttype = CPlayer.CONTENT_VCD;
                break;
             case "cd":
                conttype = CPlayer.CONTENT_CD;
                break;
            }
            plMan.addFile(dvdpath, null, conttype);
        }
    }    
}

function get_dvd_cmd(fileName)
{
    var infos = fileName.split(',');
    var path;
    var  track = infos[1];
    if(track == "0") {
        track ="";
    }
   
    if(fileName.indexOf('dvd://') == 0){
        path = infos[5];
        if(path.length == 1)
        {
            path += ":";
        }
        return infos[0] + track
        + ( infos[2] != "" && infos[2] != "-1" ? " -chapter " + infos[2] : "")
        + ( infos[3] != "" && infos[3] != "-1" ? " -aid " + infos[3] : "")
        + ( infos[4] != "" && infos[4] != "-1" ? " -sid " + infos[4] : "")
        + ' -dvd-device "' + path +'" ';
    } else if(fileName.indexOf('vcd://') == 0) {
        path = infos[2];
        if(path.length == 1) {
            path += ":";
        }
        return infos[0] + track + " -cdrom-device " + path +" ";
    }
    else if(fileName.indexOf('cd://') == 0) {
        path = infos[2];
        if(path.length == 1) {
            path += ":";
        }
        return infos[0] + track + " -cdrom-device " + path +" ";
    }

    return infos[0] + infos[1];
}

function get_dvd_file_xml(fileName)
{
   var doc = new XMLDom();
   var root = doc.createElement("item");
   var item = doc.appendChild(root);
   var cmds = get_dvd_cmd(fileName);
 
   var xmlInfo;
   var infoDoc = null;
   var usemp = false;
   if(fileInfoXML && plMan.isCurFileSelected())
   {
     mp.stop();
     infoDoc = fileInfoXML;
   }
   else
   {    
    mp.playDevice2(cmds);
    usemp = true;
    xmlInfo = mp.getClipInfoXML();
    if(xmlInfo) {
		infoDoc = new XMLDom();
		infoDoc.loadXML(xmlInfo);
    }
    else
       return "";
   }
   DomHelper.insertNode(doc, item, 'overall.file', get_dvd_cmd(fileName));   
   DomHelper.insertNode(doc, item, 'overall.duration', mp.getDuration());
   //Io.getFileWithPath()
   var inforoot = DomHelper.getChildNode(infoDoc);
   var discnode = DomHelper.findChildNode(inforoot, "disc");
   var volume = null;
   var args = fileName.split(",");
   args[0] = args[0].replace("://", "_");
   args[0] = args[0].toUpperCase();
   if(discnode)
   {
    volume = args[0] + DomHelper.getChildNodeValue(discnode, "volume") + "_Track";
   }
   else
   {
    volume = args[0] + "Admin_Track";
   }   
   volume += args[1];
   DomHelper.insertNode(doc, item, 'tags.title', volume );  
   
   var videonode = DomHelper.findChildNode(inforoot, "video");
   var val = DomHelper.getChildNodeValue(videonode, "fps");
   DomHelper.insertNode(doc, item, 'video.fps_num', val);
   val = DomHelper.getChildNodeValue(videonode, "width");
   DomHelper.insertNode(doc, item, 'video.width', val);
   val = DomHelper.getChildNodeValue(videonode, "height");
   DomHelper.insertNode(doc, item, 'video.height', val);
   if(usemp)mp.stop();
   return DomHelper.GetXmlContent(doc);
}

// Open playlist
function loadPlayList()
{ 
	//the type of localFile is nsILocalFile
	var filters =  ['xulplayer playlist(*.xpl)|*.xpl', 'm3u|*m3u'];
	var localFiles = Io.loadFile("Play File", filters);
	if(localFiles instanceof Array) {
		if(plMan) plMan.openList(localFiles[0].path);
	}
}

function newPlayList()
{
	var listName = prompt("Please input a name for creating new list", "New list");
	if(listName) plMan.newList(listName);
}

function renamePlayList()
{
	var listName = prompt("Please input a new name", "Rename list");
	if(listName) plMan.renameList(listName);
}

var requestingURL;

function httpStateChange()
{
	if (xmlhttp.readyState == 4) { // 4 = "loaded"
		if (xmlhttp.status == 200) { // 200 = OK
			hasInternet = true;
			if (requestingURL && curViewMode != VIEW_TV && curViewMode != VIEW_CENTER) {
				$e("webBrowser").setAttribute("src", requestingURL);
				changeVideoBoxPage(INDEX_PAGES);
				requestingURL = null;
			}
		}
	}
}

function loadPage(url)
{
	if(!url) return;
	if(url.indexOf("file://") != -1 || url.indexOf("://") < 0 || hasInternet) {
		$e("webBrowser").setAttribute("src", url);
		changeVideoBoxPage(INDEX_PAGES);
	} else if (!requestingURL) {
		requestingURL = url;
		xmlhttp.onreadystatechange = httpStateChange;
		xmlhttp.open("HEAD", requestingURL, true);
		xmlhttp.send(null);
	}
}

function imgLoadLoop(filename)
{
    var div = $e("webBrowser").contentDocument.getElementById("imageFlow");
    if(div) {
        filename = 'file:///' + filename;
        var img = new Image();
        img.src = filename;
        var webFrame = window.frames["webBrowser"];
        webFrame.waiting("imageFlow");
        var flx;
        if (img.complete) {
            flx = webFrame.createRef(img);
            webFrame.create("imageFlow", 0.15, 1.5, 10, img, flx);
            return;
        }
        img.onload = function () {
            flx = webFrame.createRef(img);
            webFrame.create("imageFlow", 0.15, 1.5, 10, img, flx);
        }
    }
    else
    {
        setTimeout(imgLoadLoop, 20, filename);
    }    
}

function loadPicture(url, filename)
{
    $e("webBrowser").setAttribute("src", url);
    imgLoadLoop(filename);
    changeVideoBoxPage(INDEX_PAGES);
}

function setLang(lang)   //language select
{
	try {
	  var prefs = Cc[IID_Prefs].getService(Ci.nsIPrefBranch);
	  prefs.setCharPref("general.useragent.locale", lang);
	}
	catch ( err ) {
	  alert( "SelectLanguage - " + err);
	}
    var tipTitle = getLocalString("tips.setLang.title");
    var promptStr = getLocalString("tips.setLang.prompt");
	if(isChecked("tipSetLang")) msgbox(promptStr, tipTitle);
}

function getFileInfo(type, info)
{
	var r = fileInfoXML.getElementsByTagName(type);
	if (r.length > 0) {
			r = r[0].getElementsByTagName(info);
			if (r.length > 0) return r[0].firstChild.nodeValue;
	}
	return null;
}

function showFileInfo()
{
	if(fileInfoXML) {
		var xsl = loadXML("xslt/fileinfo.xsl");
		transformXML(fileInfoXML, xsl, "tabInfo");
	}
}

function getPlayCommandOptions()
{
	var opts = " -framedrop -autosync 30 -vf screenshot";
	//if (isChecked("optEnableColor")) opts += " -vf-add eq2";
	var ppopts = getValue("optAutoLevel");
	if (document.getElementById("optDeint").selectedIndex > 0)
		ppopts += "/" + getAttr("optDeint", "value");
	if (isChecked("optDeblockH"))
		ppopts += "/" + getAttr("optDeblockH", "value");
	if (isChecked("optDeblockV"))
		ppopts += "/" + getAttr("optDeblockV", "value");
	if (isChecked("optDering"))
		ppopts += "/" + getAttr("optDering", "value");
	if (isChecked("optDenoise"))
		ppopts += "/" + getAttr("optDenoise", "value");
	if (ppopts.length > 1)
		opts += " -vf-add pp=" + ppopts;

	if(isChecked("rotate90")){              //Rotate / flip / mirror
		opts += " -vf-add rotate=2";
	} else if(isChecked("rotate270")) {
		opts += " -vf-add rotate=1";
	} else if(isChecked("rotate90Flip")) {
		opts += " -vf-add rotate=0";
	} else if(isChecked("rotate270Flip")) {
		opts += " -vf-add rotate=3";
	} else if(isChecked("mirror")) {
		opts += " -vf-add mirror";
	} else if(isChecked("flip")) {
		opts += " -vf-add flip";
	}
	//opts += " -identify";
	switch ($e("optAspect").selectedIndex) {
	case 0:
		if (!isChecked("keepAspect")) opts += " -nokeepaspect";
		break;
	case 1:
		opts += " -aspect 4:3";
		break;
	case 2:
		opts += " -aspect 16:9";
		break;
	case 3:
		opts += " -aspect 2.35";
		break;
	}
	var vo = $e("optVideoOut").selectedItem.value;
	// Dual mode, set vo as DirectX3D
	if (vo && vo != "") opts += " -vo " + vo;
	
	var ao = $e("optAudioOut").selectedItem.value;
	if (ao && ao != "") opts += " -ao " + ao;
	var channels = $e("optAudioChannels").selectedItem.value;
	if (channels != "") {
		if (channels == "1:0" || channels == "1:1") {
			opts += " -channels 2 -af-add channels=1:" + channels;
		} else {
			opts += " -channels " + channels;
		}
	}
	var priority = $e("optPriority").selectedItem.value;
	if (priority && priority != "") opts += " -priority " + priority;
	// specify subtitle options
	var syscp = mp.getCodePage();
	if(! subfont) subfont = (syscp == 936 ? "simhei.ttf" : getLocalString("subFont"));
	var windir = mp.getEnv("windir");
	opts += " -subfont-text-scale 3 -subfont-blur 2 -subfont-outline 2 -sub-fuzziness 1 -subcp cp" + syscp + " -subfont " + windir + "\\fonts\\" + subfont;
	if(subFile) opts += " -sub " + subFile;
	
	//if ($e("dualmon").checked) opts += " -adapter " + $e("monid").value + " -fs";
	// extra options
	var extra = getValue("extraCmdLine");
	if (extra) opts += " " + extra;
	return opts;
}

var cpuInfoRetry = 0;

function showCPUInfo()
{
	if (cpuInfoRetry > 5) return;
	var x = new XMLHttpRequest;
	try {
		x.open("GET", localhost + "sysinfo", false);
		x.send(null);
		alert(x.responseText.length)
		if (x.responseText.length > 0) {
			alert(x.responseText)
			$e("cpuinfo").label = x.responseText;
			localHttpActive = true;
		}
	} catch (e) {
		self.setTimeout(showCPUInfo, 1000);
	}
}

var lastParsedFile = null;

function showMediaInfo(filename)
{
	if (!filename) filename = plMan.getSelectedFile();
	if (filename && (!lastParsedFile || filename != lastParsedFile)) {
		$e("mediaInfoText").value = "";
		$e("mediaInfoText").value = mc.dumpMediaInfo(filename, true);
		lastParsedFile = filename;
	}
}

function encodeLoopCallback()
{
	var percent = mc.getPercent();
	$e("encode_progress").value = percent;
	$e("lblProgress").value = getLocalFormatString("tips.convert.percentdone", percent.toFixed(1).toString());
	var sec = mc.m_lasttime / 1000;
	var str = '';
	var unit1 = 60;
	var unit2 = 60 * unit1;
	var unit3 = 60 * unit2;
	var num1, num2, num3;
	num1 = num2 = num3 = 0;
	if(sec < unit3)
	{		
		num1 = parseInt(sec % unit1);
		num2 = sec % unit2; num2 = parseInt((num2 - num1) / unit1);
		num3 = parseInt(sec / unit2); 		
	}
    else
    {
        $e("lblRemainTime").value = "";
        return;
    }
	if(num2 != 0)
	{
		str = num2.toString() + getLocalString("time.minute.name.short");
		if(num2 < 10)
			str = ' ' + str;
	}
	if(num3 != 0)
	{
		str = num3.toString() + getLocalString("time.hour.name.short") + str;
		if(num3 < 10)
			str = ' ' + str;
	}
	if(num3 == 0 && num2 == 0)
	{
		str = '< 1' + getLocalString("time.minute.name.short");
	}
	$e("lblRemainTime").value = getLocalFormatString("tips.convert.timeremains", str);
}

function encodeStopCallback()
{
	$e("encode_progress").value = 100;
	$e("lblProgress").value = "";
	$e("lblRemainTime").value = "";
	$e("encodeProgressBar").hidden = true;
	if(mc.isAborted()) {
		alert(getLocalString("tips.convert.aborted"));
	} else if(mc.isFailed()) {
		alert(getLocalString("tips.convert.failed"));
	} else {
		transcodedFileObject[curTranscodeFile] = mc.m_plugin.covDestFile;
		$e('encode_preview').setAttribute('disabled', false);
		$e('encode_preview_dual').setAttribute('disabled', false);
		alert(getLocalFormatString("tips.convert.success", transcodedFileObject[curTranscodeFile]));
	}
	$e("encode_progress").hidden = true;
	$e("encode_start").setAttribute("command", "cmdEncodeStart");
}

function encodeChooseOutputDir()
{
	var dir = Io.openDir('Set Output Folder');
	if (dir) {
		var path = dir.path;
		if (path.length == 2 && path[1] == ":") path += "\\";
		mc.setPref("overall.task.destdir", path)
		alert("The output folder has been set to " + mc.getPref("overall.task.destdir"));
	}
}

function encodeStart(filename)
{
	try{
	 //detect Mencoder    
	 var mencoderPath = mc.getWorkDirectory() + "\\codecs\\mencoder.exe";
	 var mencoderFile = Io.getFileFrom(mencoderPath);
	 if(!mencoderFile || !mencoderFile.exists())
	     throw new Error(getLocalString('encode.start.nomencoder'));        
	}catch(e){
	   alert(e);
	   return;
	}
	while (!filename) {
		//alert(player.convertPrefs);
		filename = mp.getPlayingFile();
		if (filename) break;
		if(mm) {
			filename = mm.getSelectedFile();
		}
		if(plMan && !filename) {
			filename = plMan.getSelectedFile();
		}
		if (filename) break;        
		alert(getLocalString('encode.start.notransfile'));
		return;
	}
    
    var fileext =  getFileExt(filename);
    var contenttype = getContentType(fileext);
    if(   contenttype == CPlayer.CONTENT_AUDIO
       || contenttype == CPlayer.CONTENT_CD
       || contenttype == CPlayer.CONTENT_IMAGE
       || contenttype == CPlayer.CONTENT_OTHER)
    {
        if(contenttype == CPlayer.CONTENT_OTHER)
        {
            if(filename.indexOf("dvd://") != 0 && filename.indexOf("vcd://") != 0 && filename.indexOf("cd://") != 0)
            {
                return;
            }
        }    
        else
            return;
    }
    
	$e("encode_start").disabled = true;
	$e("encode_start").setAttribute("command", "cmdEncodeStop");

    var convertfile = filename;
    curTranscodeFile = convertfile;
    if(filename.indexOf("dvd://") == 0 ||filename.indexOf("vcd://") == 0||filename.indexOf("cd://") == 0)
    {
        convertfile = get_dvd_file_xml(filename);
    }
    mc.setLoopCallback(encodeLoopCallback);
	mc.setStopCallback(encodeStopCallback);
    mc.start(convertfile);	
    if(mc.isFailed())
    {
        encodeStop();
        $e("encode_start").disabled = false;
        alert(getLocalString("tips.convert.failed"));
        return;
    }
	$e("encode_progress").hidden = false;
	$e("encode_start").disabled = false;
	$e("encodeProgressBar").hidden = false;
}

function encodeStop()
{
	if(mc.isConverting() || mc.isWaiting()) {
		mc.end();
	}
	$e("encode_start").setAttribute("command", "cmdEncodeStart");
}

function encodePause()
{
	if(mc.isConverting() || mc.isWaiting()) {
		mc.pause();
	}
}

// Preview transcoded file
function preview()
{
	var lastTrancedSuccessFile = null;
	for each(var transcodedfile in transcodedFileObject) {
		lastTrancedSuccessFile = transcodedfile;
	}
	if(lastTrancedSuccessFile) startPlay(lastTrancedSuccessFile);
}

// Preview transcoded file and original file in dual mode
function previewDual()
{
	var lastTrancedSuccessFile = null;
	var lastOriginFile = null;
	for (var originFile in transcodedFileObject) {
		if(originFile) {
			lastOriginFile = originFile;
			lastTrancedSuccessFile = transcodedFileObject[originFile];
		}
	}
	if(lastTrancedSuccessFile && lastOriginFile) {
		setAttr("dualMode", "checked", true);
		toggleDualMode();
		dualChooseItem(0, lastOriginFile);
		dualChooseItem(1, lastTrancedSuccessFile);
		syncReplay();
	}
}

//-----------------------Helper function------------------------
function initUserDataPath()
{
	try {
		var appDataPath = Io.getSpecialDir("data", "CurProcD").path;
		userDataPath = appDataPath;
		var userDataPathObj = Io.getFileFrom(appDataPath);
		userDataPathObj.append('lyrics');
			if(userDataPathObj && !userDataPathObj.exists()) {
				userDataPathObj.create(Components.interfaces.nsIFile.DIRECTORY_TYPE, 420);
			}
			lyricPath = userDataPathObj.path;
			
		/*
		var appDataPath = mp.getEnv('appdata');
		var userDataPathObj = Io.getFileFrom(appDataPath);
		if(userDataPathObj) {
			userDataPathObj.append("Broad Intelligence");
			if(userDataPathObj && !userDataPathObj.exists()) {
				userDataPathObj.create(Components.interfaces.nsIFile.DIRECTORY_TYPE, 420);
			}
			userDataPathObj.append('Xulplayer');
			if(userDataPathObj && !userDataPathObj.exists()) {
				userDataPathObj.create(Components.interfaces.nsIFile.DIRECTORY_TYPE, 420);
			} 
			userDataPath = userDataPathObj.path;
			userDataPathObj.append('lyrics');
			if(userDataPathObj && !userDataPathObj.exists()) {
				userDataPathObj.create(Components.interfaces.nsIFile.DIRECTORY_TYPE, 420);
			}
			lyricPath = userDataPathObj.path;
		
		}
		*/
	} catch (e) {
		
	}
	
}

function initResources()
{
	//addFile("E:\\github\\Firemail\\ExMail\\XULRUNNER_DEMO\\myprojects\\EE\\Resource\\2500个汉字\\一.SWF");
	if(plMan.isEmpty())
	{
		plMan.clear();
		var ResourceDataPath = Io.getSpecialDir("Resource", "CurProcD").path;
		var ResourceDataPathDataPathObj = Io.getFileFrom(ResourceDataPath);
		var items = ResourceDataPathDataPathObj.directoryEntries;
		  while (items.hasMoreElements()) {
		    var item = items.getNext();
		     item.QueryInterface(Components.interfaces.nsIFile); 
		    if (item.isDirectory())
		    {
		      plMan.newList(item.leafName,"1");
		      	if(!item) continue;
				var filePaths = [];
				var fileSizes = [];
				addSubFolders(item, false, filePaths, fileSizes);
				if(filePaths.length == 0) continue;

				
				for(var i=0; i<filePaths.length; ++i) {
					addFileEx(item.path, filePaths[i], fileSizes[i] / 1024);
				}
		    }
		  }
	}


	

}

function parseCommandLine()
{
	nsCommandLine = window.arguments[0].QueryInterface(Components.interfaces.nsICommandLine);
	if(nsCommandLine && nsCommandLine.length > 0) {
		var toSwitchView = false;
		var firstParam = nsCommandLine.getArgument(0);
		var startParamIndex = 0;
		switch (firstParam) {
		case "-url":
			var fileType = "Url";
			playType = PLAY_TYPE_URL;
			// -1 indicate it's a url
			var filename = nsCommandLine.getArgument(1);
			if (filename.indexOf("application.ini") < 0) {
				addFile(filename, -1);
				startPlay(filename, fileType);
				toSwitchView = true;
			}
			break;
		case "-dual":	//dual play when start
			if(nsCommandLine.length == 3) {
				setAttr("dualMode", "checked", true);
				if(getAttr("deviceView", "checked")) setAttr("fullView", "checked", true);
				dualmode = true;
				var inFile = nsCommandLine.getArgument(1);
				dualChooseItem(0, inFile);
				inFile = nsCommandLine.getArgument(2);
				dualChooseItem(1, inFile);
				syncReplay();
				toSwitchView = true;
			}
			break;
		case "-app":	// ignore -app argument and skip the following one
			startParamIndex = 2;
		default:
			if (startParamIndex < nsCommandLine.length) {
				for (var i = startParamIndex; i < nsCommandLine.length; i++)
					addFile(nsCommandLine.getArgument(i));
				startPlay(nsCommandLine.getArgument(startParamIndex));
				toSwitchView = true;
			}
		}
		if (toSwitchView && (curViewMode == VIEW_DEVICE || curViewMode == VIEW_FULL))
			setViewMode(VIEW_COMPACT);

	} 
}
//-------------------------- SWF flash playback control -----------------------
// Main.xul isn't in the same domain with swf file, so can't control swf playback
// Need to be fixed
function playSwf(filename)
{
	swfPlayer.setPlayFile(filename);
	//mp.enablePlayNext(false);
	setTimeout(function() {
		swfPlayer.play();
		var totalFrames = swfPlayer.getTotalFrames();
		if ( totalFrames > 0) {
			enable("timebar");
			for (var i = 1; i < playButtons.length; i++) {
				enable(playButtons[i]);
			}
			progressbar.setAttribute("max", totalFrames);
			setAttr("play_icon", "command", "cmdPause");
		} else {
			disable("timebar");
		}
	}, 500);
	changeVideoBoxPage(INDEX_SWF);
}

function swfPlayCallback()
{
	var curFrmae = swfPlayer.getCurFrame();
	progressbar.value = curFrmae;
}

function swfStopCallback()
{
	//mp.enablePlayNext(true);
	//updatePlayerState();
}

function SwfPlayer()
{
	this.plugin = $e("swfVideo");
	this.file = null;		// file name
	this.loopCallback = null;
	this.stopCallback = null;
	this.timerId = null;
}

SwfPlayer.prototype.setPlayFile = function(fileName) {
	if(fileName == null) return;
	this.file = fileName;
	this.plugin.setAttribute('src', this.file);
	this.plugin.LoadMovie(0, this.file);
};

SwfPlayer.prototype.isPlaying = function() {
	return this.plugin.IsPlaying() && this.file;
};

SwfPlayer.prototype.play = function() {
	if(this.file == null) return;
	if(this.plugin.IsPlaying()) {
		this.plugin.StopPlay();
	}
	this.plugin.Play();
	if(this.isPlaying) {
		this.playLoop();
	}
};

SwfPlayer.prototype.pause = function() {
	if(this.file == null) return;
};

SwfPlayer.prototype.stop = function() {
	if(this.file == null) return;
	this.plugin.setAttribute('src', '');
	this.file = null;
};

SwfPlayer.prototype.getTotalFrames = function() {
	return this.plugin.TotalFrames();
};

SwfPlayer.prototype.getCurFrame = function() {
	return this.plugin.CurrentFrame();
};

SwfPlayer.prototype.gotoFrame = function(num) {
	if(typeof(num) == 'string') num = parseInt(num);
	if(num > 0 && num < this.getTotalFrames()) {
		this.plugin.GotoFrame(num);
	}
};

SwfPlayer.prototype.setLoopCallback = function(func) {
	if(typeof(func) == 'function') {
		this.loopCallback = func;
	}
};

SwfPlayer.prototype.setStopCallback = function(func) {
	if(typeof(func) == 'function') {
		this.stopCallback = func;
	}
};

SwfPlayer.prototype.playLoop = function() {
	if(typeof(this.loopCallback) == 'function') {
		this.loopCallback();
	}
	var me = this;
	if(this.isPlaying() && this.file) {
		this.timerId = setTimeout(function(){me.playLoop()}, 200);
	} else {
		if(typeof(this.stopCallback) == 'function') {
			this.stopCallback();
			clearTimeout(this.timerId);
		}
	}
};

}