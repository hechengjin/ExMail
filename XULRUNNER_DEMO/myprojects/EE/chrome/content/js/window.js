if(typeof(WINDOW_CHANGE) != "boolean") {
	WINDOW_CHANGE = true;

if (typeof(Cc) == "undefined")
  var Cc = Components.classes;
if (typeof(Ci) == "undefined")
  var Ci = Components.interfaces;

//Components.utils.import("resource://gre/modules/ctypes.jsm");
  
var fs = false;
var curViewMode;
var tvMode = TV_START;
var noEventTime = 5100;
var winMaxed = false;
var resizers = ["resizer-left", "resizer-right", "resizer-top", "resizer-bottom", "resizer-topleft", "resizer-bottomright"];

function toggleFullScreen()
{
    fs = !fs;
	if(curViewMode == VIEW_CENTER) {
		$e('tabMediaLib').hidden = fs;
		return;
	}
	
    noEventTime = 5100;
    if (fs) {
        for(var i in resizers)
            $e(resizers[i]).hidden = true;
        //mp.setWindowTop(true);
        hideAllExceptVideo(true);
        window.maximize();
    } else {
		for(var i in resizers){
			$e(resizers[i]).hidden = false;
		}
		window.restore();
        setViewMode();
		//dumpMsg('quit fullscreen');
        //toggleMediaLib();
        //if(!isChecked("windowTop"))
        //    mp.setWindowTop(false);
    }
	if(isTvMode())
	{
		 if (fs) {
			getTvFrame().setFullScreen();
		 }
		 else
		 {
			getTvFrame().setNoFullScreen();
		 }
	}
}

function toggleMaximize()
{
	winMaxed = !winMaxed;
	if (winMaxed)
		window.maximize();
	else
		window.restore();
}

function toggleDualMode()
{
    dualmode = isChecked("dualMode");
    $e('dualVideoOptions').hidden = !dualmode;
    if (!dualmode) {
        mp.setActivePlayer(0);
		mp.exitDualMode();
    } 
    //window.sizeToContent();
}

/*
function slideViewMode(mode)
{
	if (!curViewMode) return;
	switch (mode) {
	case VIEW_FULL:
		$e("fullView").setAttribute("checked", true);
		break;
	case VIEW_SIMPLE:
		$e("simpleView").setAttribute("checked", true);
		break;
	case VIEW_DEVICE:
		$e("deviceView").setAttribute("checked", true);
		break;
	case VIEW_COMPACT:
		$e("compactView").setAttribute("checked", true);
		break;
	}
	setViewMode(mode);
}
*/

function isCenterMode()
{
	return curViewMode == VIEW_CENTER;
}

function isTvMode()
{
	return curViewMode == VIEW_TV;
}

function getTvFrame()
{
	return window.frames["net_tv_frame"];
}

function setViewMode(mode)
{
	var titlebar = $e('titlebar');
	var tabs = $e('mainTab');
	var playToolbar = $e("bottomBar");
	var splitter = $e("panelSplit");
	var mcexContainer = $e("mcexBox");
	var mcexSplitter = $e("mcexSplitter");
	var medialibPanel = $e("tabMediaLib");
	if (!mode) {
		mode = parseInt($e('optViewMode').value);
	} else {
		$e('optViewMode').value = mode;
	}
	
	switch (mode) {
	case VIEW_FULL:
	case VIEW_DEVICE:
		titlebar.hidden = false;
		tabs.hidden = false;
		playToolbar.hidden = false;
		splitter.hidden = false;
		break;
	case VIEW_SIMPLE:
		titlebar.hidden = false;
		tabs.hidden = true;
		toolbar.hidden = true;
		playToolbar.hidden = true;
		splitter.hidden = true;
		break;
	case VIEW_TV:
	case VIEW_COMPACT:
		titlebar.hidden = false;
		tabs.hidden = true;
		toolbar.hidden = true;
		playToolbar.hidden = false;
		splitter.hidden = true;
		break;
	case VIEW_CENTER:
		titlebar.hidden = true;
		tabs.hidden = true;
		toolbar.hidden = true;
		playToolbar.hidden = true;
		splitter.hidden = true;
		break;
	}
		
	if (mode != curViewMode) {
		// view mode has been changed
		var isDeviceMode = (mode == VIEW_DEVICE);
		mcexContainer.hidden = !isDeviceMode;
		mcexSplitter.hidden = !isDeviceMode;
		var isMediaCenter = (mode == VIEW_CENTER);
		for(var i in resizers) {
			var aResizer = $e(resizers[i]);
			aResizer.hidden = isMediaCenter;
		}
		
		curViewMode = mode;
		if (isMediaCenter) {
			if (!winMaxed) toggleMaximize();
			setTVMode(tvMode);
		} else if (mode == VIEW_TV) {
			mp.stop();
			openNetTv();
		} else {
			$e('tabMediaLib').hidden = true;
			homeURL = getLocalString("homeURL");
			if (!homeURL) homeURL = "http://xulplayer.sourceforge.net/start";
			if (winMaxed) toggleMaximize();
			if(!mp.isPlaying()) {
				loadPage(homeURL);
			}
		}
	}

	onResize();
}

function setTVMode(aTvMode, category)
{
	tvMode = aTvMode;
	switch(tvMode) {
		case TV_START:
			$e('tabMediaLib').hidden = true;
			loadPage("tv_utility/start/start.html");
			break;
		case TV_FULL:
			$e('tabMediaLib').hidden = false;
			loadPage("tv_utility/start/mediacenter.html");
			break;
	}
	
	if(mm && category != undefined) {
		mm.LocateTopCategoryById(category);
	}
}

function openNetTv()
{
	var nettv = $e("net_tv_frame");
	changeVideoBoxPage(INDEX_TVU);
	
	if (curViewMode == VIEW_CENTER) $e("tabMediaLib").hidden = false;
	if(nettv.getAttribute("src") == "")
	{
		var tvuUrl;
		// check TVU plugin	
		if (checkPlugin("TVU")) {
			tvuUrl = getLocalString("tvu.url.start");
		} else {
			tvuUrl = getLocalString("tvu.url.noplugin");
		}

		nettv.setAttribute("src","html/loading.html");
		setTimeout(function(){nettv.setAttribute("src", tvuUrl);}, 100);
	}
}

function zoomVideo()
{
    if (isChecked("keep25")) {
        mp.zoom(0.25);
    } else if (isChecked("keep50")) {
        mp.zoom(0.5);
    } else if (isChecked("keep150")) {
        mp.zoom(1.5);
    } else if (isChecked("keep200")) {
        mp.zoom(2);
    } else if (isChecked("keep100")) {
        mp.zoom(1);
    } else if (isChecked("keepvideo")){
        mp.zoom(-1.5);  // Need a number < 0 , but can't be negative integer
        fitVideoSize(false);
    } else {
        mp.zoom(0);
    }
}

function fitVideoSize(ifOriginal)
{
    var vh = 0, vw = 0;
    if(ifOriginal){
        vh = mp.getVideoHeight(); vw = mp.getVideoWidth();
    } else {
        vw = mp.getRealVideoWidth(); vh = mp.getRealVideoHeight(); 
    }
    if(vh <= 0 || vw <= 0 || vh > window.screen.height || vw > window.screen.width) return;
    var boxh = $e("videoBox").boxObject.height;
    var boxw = $e("videoBox").boxObject.width;
    var h = window.innerHeight, w = window.innerWidth;
    window.innerWidth = w - boxw + vw;
    window.innerHeight = h - boxh + vh;
    onResize();
}

function setWindowTop()
{
	var winTopmost = isChecked("windowTop");
    mp.setWindowTop(winTopmost);
}

function setWindowTopByKey()
{
	var winTopmost = isChecked("windowTop");
	$e('windowTop').setAttribute('checked', !winTopmost);
	mp.setWindowTop(!winTopmost);
}

function focusMethod()
{
    if(curViewMode == VIEW_CENTER) return;
	if(focusFromPlugin){
		focusFromPlugin = false;
        $e('windowIcon').focus();
        if(fs){
            noEventTime = 0;
            $e("bottomBar").hidden = false;
        }
    }
}

function onResize()
{
    if (winMaxed) return true;
    switch (curViewMode) {
    case VIEW_DEVICE:
        if (window.outerWidth < 1000)
            window.outerWidth=1000;
        if (window.outerHeight < 550)
            window.outerHeight=550;
	break;
    case VIEW_FULL:
        if (window.outerWidth < 720)
            window.outerWidth=720;
        if (window.outerHeight < 510)
            window.outerHeight=510;
	break;
    case VIEW_COMPACT:
    case VIEW_SIMPLE:
        if (window.outerWidth < 720 )
            window.outerWidth=720;
        if (window.outerHeight < 270)
            window.outerHeight=270;
		break;
	case VIEW_CENTER:
		if(!winMaxed) toggleMaximize();
		break;
    }
    return true;  
}

function hideAllExceptVideo(ifHide)
{
    var elements = [$e("titlebar"), $e('mainTab'), 
                    $e('bottomBar'), $e("panelSplit"), $e("tabMediaLib")];
    for each(var elem in elements){
		elem.hidden = ifHide;
    }
}
function showOrHideProgressbar()    //when noEvent time is bigger than 1500, hide progressbar
{
    noEventTime += mp.m_timeInterval;
    if(noEventTime > 5000)
        $e("bottomBar").hidden = true;
    else
        $e("bottomBar").hidden = false;
    if(noEventTime > 20000) noEventTime = 20000;
}       

/*
function toggleMediaLib()
{
    var mlbar = $e("media-lib-bar");
	mlbar.hidden = true;
    mlbar.nextSibling.hidden = true;
    if ($e("menu-show-ml").getAttribute("checked") == "true") {
            mlbar.hidden = false;
            mlbar.nextSibling.hidden = false;
            var xsl = loadXML("medialib/catalog.xsl");
            var xml = loadXML("file:///" + mediaLibPath + "\\filelist.xml");
            if (xsl && xml) transformXML(xml, xsl, "file-list");
    } else {
            mlbar.hidden = true;
            mlbar.nextSibling.hidden = true;
    }
}*/

// Attempt to restart the app
function restartApp()
{
	// Notify all windows that an application quit has been requested.
	var os = Cc["@mozilla.org/observer-service;1"].getService(Ci.nsIObserverService);
	var cancelQuit = Cc["@mozilla.org/supports-PRBool;1"].createInstance(Ci.nsISupportsPRBool);
	os.notifyObservers(cancelQuit, "quit-application-requested", "restart");
  
	// Something aborted the quit process.
	if (cancelQuit.data)
	  return;
  
	// attempt to restart
	var as = Cc["@mozilla.org/toolkit/app-startup;1"].getService(Ci.nsIAppStartup);
	as.quit(Components.interfaces.nsIAppStartup.eRestart |
			Components.interfaces.nsIAppStartup.eAttemptQuit);
  
	closeWindow();
}

function quitApp(aForceQuit)
{
  var appStartup = Components.classes['@mozilla.org/toolkit/app-startup;1'].
    getService(Components.interfaces.nsIAppStartup);

  // eAttemptQuit will try to close each XUL window, but the XUL window can cancel the quit
  // process if there is unsaved data. eForceQuit will quit no matter what.
  var quitSeverity = aForceQuit ? Components.interfaces.nsIAppStartup.eForceQuit :
                                  Components.interfaces.nsIAppStartup.eAttemptQuit;
  appStartup.quit(quitSeverity);
}


function closeWindow()
{
	//mp.setTrayIcon(false);
	window.close();
}


function mysetTrayIcon()
{
	var traylib = ctypes.open("E:\\github\\Firemail\\ExMail\\XULRUNNER_DEMO\\myprojects\\EE\\bin\\tray_x86-msvc.dll");

	const mouseevent_t = ctypes.StructType(
			  "mouseevent_t",
			  [
			  {"button": ctypes.int},
			  {"clickCount": ctypes.int},
			  {"x": ctypes.long},
			  {"y": ctypes.long},
			  {"keys": ctypes.int},
			  ]
			  );

	const mouseevent_callback_t = ctypes.FunctionType(
		  ctypes.default_abi,
		  ctypes.void_t, // retval
		  [
		  ctypes.voidptr_t, // handle
		  mouseevent_t.ptr, // event
		  ]
		  ).ptr;

	var _SetWatchMode = traylib.declare(
		  "mintrayr_SetWatchMode",
		  ctypes.default_abi,
		  ctypes.void_t, // retval handle
		  ctypes.int // mode
		);

	var _MinimizeWindow = traylib.declare(
		  "mintrayr_MinimizeWindow",
		 ctypes.default_abi,
		  ctypes.void_t, // retval BOOL
		   ctypes.voidptr_t // handle
		  );
	var _GetBaseWindowHandle = traylib.declare(
		  "mintrayr_GetBaseWindow",
		  ctypes.default_abi,
		   ctypes.voidptr_t, // retval handle
		  ctypes.jschar.ptr // title
		  );

	var _CreateIcon = traylib.declare(
		  "mintrayr_CreateIcon",
		  ctypes.default_abi,
		  ctypes.int, // retval BOOL
		  ctypes.voidptr_t, // handle
		  mouseevent_callback_t // callback
		  );

	
	let baseWindow = window
    .QueryInterface(Ci.nsIInterfaceRequestor)
    .getInterface(Ci.nsIWebNavigation)
    .QueryInterface(Ci.nsIBaseWindow);

     // Tag the base window
  let oldTitle = baseWindow.title;
  let uuidGenerator = Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator);
  baseWindow.title = uuidGenerator.generateUUID().toString();

  var _handle;
  try {
    // Search the window by the new title
    _handle = _GetBaseWindowHandle(baseWindow.title);
    if (_handle.isNull()) {
      throw new Error("Window not found!");
    }
  }
  finally {
    // Restore
    baseWindow.title = oldTitle;
  }

 // var _icons = [];
  //let icon = new TrayIcon(window, aCloseOnRestore);
   // _icons.push(icon);

  const mouseevent_callback = mouseevent_callback_t(function mouseevent_callback(handle, event) {
  try {
    event = event.contents;
    for (let [,w] in Iterator(_icons)) {
      if (!ptrcmp(w.handle, handle)) {
        continue;
      }
      let document = w.window.document;
      let e = document.createEvent("MouseEvents");
      let et = "TrayClick";
      if (event.clickCount == 2) {
        et = "TrayDblClick";
      }
      else if (event.clickCount > 2) {
        et = "TrayTriClick";
      }
      e.initMouseEvent(
        et,
        true,
        true,
        w.window,
        0,
        event.x,
        event.y,
        0,
        0,
        (event.keys & (1<<0)) != 0,
        (event.keys & (1<<1)) != 0,
        (event.keys & (1<<2)) != 0,
        (event.keys & (1<<3)) != 0,
        event.button,
        document
        );
      document.dispatchEvent(e);
      return;
    }
    throw new Error("Window for mouse event not found!" + _icons.toSource());
  }
  catch (ex) {
    Cu.reportError(ex);
  }
});


    _CreateIcon(_handle, mouseevent_callback);
 _SetWatchMode(0);
 
	  _MinimizeWindow(_handle);

	  //

	// traylib.close();
}

function getStyle(elem,styleProp)
{
  var v = null;
  if (elem) {
    var s = document.defaultView.getComputedStyle(elem,null);
    v = s.getPropertyValue(styleProp);
  }
  return v;
}

/**
 * Simple JS container that holds rectangle information
 * for (x, y) and (width, height).
 */
function CScreenRect(inWidth, inHeight, inX, inY) {
  this.width = inWidth;
  this.height = inHeight;
  this.x = inX;
  this.y = inY;
}

/**
 * Get the current maximum available screen rect based on which screen
 * the window is currently on.
 * @return A |CScreenRect| object containing the max available 
 *         coordinates of the current screen.
 */
function getCurMaxScreenRect() {
  var screenManager = Cc["@mozilla.org/gfx/screenmanager;1"]
                        .getService(Ci.nsIScreenManager);
                        
  var curX = parseInt(document.documentElement.boxObject.screenX);
  var curY = parseInt(document.documentElement.boxObject.screenY);
  var curWidth = parseInt(document.documentElement.boxObject.width);
  var curHeight = parseInt(document.documentElement.boxObject.height);
  
  var curScreen = screenManager.screenForRect(curX, curY, curWidth, curHeight);
  var x = {}, y = {}, width = {}, height = {};
  curScreen.GetAvailRect(x, y, width, height);
  
  return new CScreenRect(width.value, height.value, x.value, y.value);
}

}
