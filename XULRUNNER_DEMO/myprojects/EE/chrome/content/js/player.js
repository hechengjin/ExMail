/*Wrapper class for npxulplayer plugin (player part)
  Thus the player class stands alone from gui specific elements.
  Created by zoominla, 12-28-2008 
*/

function CPlayer(pluginObj)
{
    this.m_plugin = pluginObj;
    this.m_timeInterval = 100;
    this.m_timerId = null;
    
    this.m_bPlayNext = false;
    this.m_bSeeking = false;
    this.m_playerState = 0;         // idle
    
    this.m_duration = 0;            //duration of video (ms)
    this.m_curPos   = 0;            //current position of video (ms)
    this.m_playerIndex = 0;
    this.m_playingFile = ['', ''];
    
    // Call back functions
    this.startCallback = null;
    this.stopCallback = null;
    this.pauseCallback = null;
    this.loopCallback = null;
}

// Player state enum
CPlayer.PLAYER_IDLE = 0;
CPlayer.PLAYER_PLAYING = 1;
CPlayer.PLAYER_PAUSED = 2;	
CPlayer.PLAYER_STOPPING = 3;

// Player info enum
CPlayer.PLAYER_POS = 0;
CPlayer.PLAYER_STATE = 1;
CPlayer.PLAYER_PERCENTAGE = 2;
CPlayer.PLAYER_DURATION = 3;
CPlayer.PLAYER_HAS_AUDIO = 4;
CPlayer.PLAYER_HAS_VIDEO = 5;

// Content Type enum
CPlayer.CONTENT_AUDIO = 0x1;
CPlayer.CONTENT_VIDEO = 0x2;
CPlayer.CONTENT_IMAGE = 0x4;
CPlayer.CONTENT_OTHER = 0x8;
CPlayer.CONTENT_CD = 0x10;
CPlayer.CONTENT_VCD = 0x20;
CPlayer.CONTENT_DVD = 0x40;



CPlayer.prototype.initialize = function() {
    this.m_plugin.init();
    //this.m_plugin.setTrayIcon(true);
};

CPlayer.prototype.unInitialize = function() {
    this.m_plugin.setTrayIcon(false);
    //this.m_plugin.stop();
    this.m_plugin.uninit();
    clearTimeout(this.m_timerId);
	this.m_timerId = null;
};

CPlayer.prototype.play = function(optionCmd, bDetach) {
    this.m_plugin.play(this.getPlayingFile(), optionCmd, bDetach);
    this.m_playerState = this.m_plugin.getInfo(CPlayer.PLAYER_STATE);
    this.m_duration = this.m_plugin.getInfo(CPlayer.PLAYER_DURATION);
    if(this.isPlaying()) {
	//dumpMsg("playing");
        if(typeof(this.startCallback) == "function") {
            this.startCallback();
        }
        this.playLoop();
    } else {
	//dumpMsg("not playing");
    }
};

CPlayer.prototype.playDevice = function(dvdCmd, optionCmd, bDetach) {
	if(curTranscodeFile.indexOf("dvd://") == 0 || curTranscodeFile.indexOf("vcd://") == 0 || curTranscodeFile.indexOf("cd://") == 0) {
		return;
	}
    this.m_plugin.play("", dvdCmd + optionCmd, bDetach);
    this.m_playerState = this.m_plugin.getInfo(CPlayer.PLAYER_STATE);
    this.m_duration = this.m_plugin.getInfo(CPlayer.PLAYER_DURATION);
    if(this.isPlaying()) {
	//dumpMsg("playing");
        if(typeof(this.startCallback) == "function") {
            this.startCallback();
        }
        this.playLoop();
    } else {
	//dumpMsg("not playing");
    }
};

CPlayer.prototype.playDevice2 = function(dvdCmd) {
    var opts = " -nosound -identify -frames 0";
    this.m_plugin.play("", dvdCmd + opts, false);
    this.m_playerState = this.m_plugin.getInfo(CPlayer.PLAYER_STATE);
    this.m_duration = this.m_plugin.getInfo(CPlayer.PLAYER_DURATION);
    if(this.isPlaying()) {
	//dumpMsg("playing");
        if(typeof(this.startCallback) == "function") {
            this.startCallback();
        }
        this.playLoop();
    } else {
	//dumpMsg("not playing");
    }
};

CPlayer.prototype.pause = function() {
    this.m_plugin.pause();
    if(typeof(this.pauseCallback) == "function") {
        this.pauseCallback();
    }
};

CPlayer.prototype.stop = function() {
    this.m_plugin.stop();
};

// Seek to absolute position, the unit of "pos" is s
CPlayer.prototype.seekToPos = function(pos) {
    if(this.m_bSeeking) return;
    this.m_bSeeking = true;
    this.m_plugin.seek(pos, 2);
    this.m_bSeeking = false;
};

// Forward, the unit of "secs" is s
CPlayer.prototype.forward = function(secs) {
    if(this.m_bSeeking) return;
    this.m_bSeeking = true;
    this.m_plugin.seek(secs);
    this.m_bSeeking = false;
};

// Rewind, the unit of "secs" is s
CPlayer.prototype.rewind = function(secs) {
    if(this.m_bSeeking) return;
    this.m_bSeeking = true;
    this.m_plugin.seek(-secs);
    this.m_bSeeking = false;
};

// Set active player. Player index can be 0 or 1, default is 0.
CPlayer.prototype.setActivePlayer = function(playerIdx) {
    this.m_playerIndex = playerIdx;
    this.m_plugin.setPlayer(playerIdx);
	// When switch player, we should update state and duration
	this.m_playerState = this.m_plugin.getInfo(CPlayer.PLAYER_STATE);
    this.m_duration = this.m_plugin.getInfo(CPlayer.PLAYER_DURATION);
};

CPlayer.prototype.getPlayerIdx = function() {
    return this.m_playerIndex;
};

// Exit dual player mode
CPlayer.prototype.exitDualMode = function() {
    this.sendCommand('quit', false, 1);
    this.m_plugin.exitDualMode();
};

// Query state
CPlayer.prototype.getPlayerState = function() {
    return this.m_playerState;
};

CPlayer.prototype.getDuration = function() {
    return this.m_duration;
};

CPlayer.prototype.getCurPos = function() {
    return this.m_curPos;
};

CPlayer.prototype.isPlaying = function() {
    return this.m_playerState == CPlayer.PLAYER_PLAYING;
};

CPlayer.prototype.isPaused = function() {
    return this.m_playerState == CPlayer.PLAYER_PAUSED;
};

CPlayer.prototype.isStopped = function() {
    return this.m_playerState == CPlayer.PLAYER_STOPPING;
};
CPlayer.prototype.isIdle = function() {
    return this.m_playerState == CPlayer.PLAYER_IDLE;
};

// Set current playing file
CPlayer.prototype.setPlayingFile = function(fileName) {
    this.m_playingFile[this.m_playerIndex] = fileName;
    if (this.m_playerIndex == 0) this.m_plugin.playingFile = fileName;
};

// Get current playing file
CPlayer.prototype.getPlayingFile = function() {
    //this.m_playerIndex == 0 ? this.m_plugin.playingFile : this.m_playingFile[this.m_playerIndex]
	return this.m_playingFile[this.m_playerIndex];
};

CPlayer.prototype.hasAudio = function() {
    return this.m_plugin.getInfo(CPlayer.PLAYER_HAS_AUDIO);
};

CPlayer.prototype.hasVideo = function() {
    return this.m_plugin.getInfo(CPlayer.PLAYER_HAS_VIDEO);
};

CPlayer.prototype.getPercentage = function() {
    var ret = null;
    if(this.m_playerState == CPlayer.PLAYER_PLAYING) {
        ret = this.m_plugin.getInfo(CPlayer.PLAYER_PERCENTAGE);
    }
    return ret;
};

// Original video size
CPlayer.prototype.getVideoWidth = function() {
    return this.m_plugin.width;
};

CPlayer.prototype.getVideoHeight = function() {
    return this.m_plugin.height;
};

// Real video size (considering zoom)
CPlayer.prototype.getRealVideoWidth = function() {
    return this.m_plugin.realWidth;
};

CPlayer.prototype.getRealVideoHeight = function() {
    return this.m_plugin.realHeight;
};

//---------------- Player command ------------------------------------
CPlayer.prototype.enablePlayNext = function(bEnable) {
    this.m_bPlayNext = bEnable;
};
CPlayer.prototype.playNextEnabled = function() {
    return this.m_bPlayNext;
};

CPlayer.prototype.zoom = function(zoomFactor) {
   this.m_plugin.zoom(zoomFactor);
};

CPlayer.prototype.sendCommand = function(cmdStr, hasResult, idx) {
    var ret = null;
    if(this.isPlaying()) {
        ret = this.m_plugin.sendCommand(cmdStr, hasResult, idx);
    }
    return ret;
};

CPlayer.prototype.switchAudio = function() {
    var ret = null;
    if(this.hasAudio()) {
        ret = this.sendCommand("switch_audio", true);
    }
    return ret;
};

CPlayer.prototype.screenshot = function() {
    var ret = null;
    if(this.hasVideo()) {
        ret = this.sendCommand("screenshot 0", true);
    }
    return ret;
};

CPlayer.prototype.switchOsd = function() {
    this.sendCommand('osd', false);
};
CPlayer.prototype.switchSubtitle = function() {
    this.sendCommand('sub_select', false);
};

// Increase speed 10%
CPlayer.prototype.speedUp = function() {
    this.sendCommand('speed_incr 0.1', false);
};
// Decrease speed 10%
CPlayer.prototype.speedDown = function() {
    this.sendCommand('speed_incr -0.1', false);
};
// Set speed (original is 1)
CPlayer.prototype.setSpeed = function(speedVal) {
    var cmdStr = 'speed_set ' + speedVal;
    this.sendCommand(cmdStr, false);
};

CPlayer.prototype.increaseVolume = function() {
    this.sendCommand("volume 1", false);
};
CPlayer.prototype.decreaseVolume = function() {
    this.sendCommand("volume -1", false);
};
CPlayer.prototype.setVolume = function(volValue) {
    this.m_plugin.setVolume(volValue);
};
CPlayer.prototype.getVolume = function() {
    return this.m_plugin.getVolume();
};

// Increase subtitle font
CPlayer.prototype.increaseSubFont = function() {
    this.sendCommand('sub_scale 1', false);
};
// Decrease subtitle font
CPlayer.prototype.decreaseSubFont = function() {
    this.sendCommand('sub_scale -1', false);
};

CPlayer.prototype.subPosUp = function() {
    this.sendCommand('sub_pos 1', false);
};
CPlayer.prototype.subPosDown = function() {
    this.sendCommand('sub_pos -1', false);
};

// Delay subtitle 0.1s
CPlayer.prototype.delaySub = function() {
    this.sendCommand('sub_delay 0.1', false);
};
// Advance subtitle 0.1s
CPlayer.prototype.advanceSub = function() {
    this.sendCommand('sub_delay -0.1', false);
};
// Restet subtitle delay (delayTime unit: ms)
CPlayer.prototype.resetSubDelay = function(delayTime) {
    var cmdStr = "sub_delay " + delayTime/1000 + " 1";
    this.sendCommand(cmdStr, false);
};

// Delay subtitle 0.1s
CPlayer.prototype.delayAudio = function() {
    this.sendCommand('audio_delay 0.1', false);
};
// Advance subtitle 0.1s
CPlayer.prototype.advanceAudio = function() {
    this.sendCommand('audio_delay -0.1', false);
};
// Restet audio delay (delayTime unit: ms)
CPlayer.prototype.resetAudioDelay = function(delayTime) {
    var cmdStr = "audio_delay " + delayTime/1000 + " 1";
    this.sendCommand(cmdStr, false);
};

//------------------- Set Video picture property ------------------
CPlayer.prototype.increaseContrast = function() {
    this.sendCommand('step_property contrast', false);
};
CPlayer.prototype.decreaseContrast = function() {
    this.sendCommand('step_property contrast 0 -1', false);
};
CPlayer.prototype.setContrast = function(val) {
    var cmdStr = 'set_property contrast '+ val;
    this.sendCommand(cmdStr, false);
};

CPlayer.prototype.increaseBright = function() {
    this.sendCommand('step_property brightness', false);
};
CPlayer.prototype.decreaseBright = function() {
    this.sendCommand('step_property brightness 0 -1', false);
};
CPlayer.prototype.setBright = function(val) {
    var cmdStr = 'set_property brightness '+ val;
    this.sendCommand(cmdStr, false);
};

CPlayer.prototype.increaseHue = function() {
    this.sendCommand('step_property hue', false);
};
CPlayer.prototype.decreaseHue = function() {
    this.sendCommand('step_property hue 0 -1', false);
};
CPlayer.prototype.setBright = function(val) {
    var cmdStr = 'set_property hue '+ val;
    this.sendCommand(cmdStr, false);
};

CPlayer.prototype.increaseSaturation = function() {
    this.sendCommand('step_property saturation', false);
};
CPlayer.prototype.decreaseSaturation = function() {
    this.sendCommand('step_property saturation 0 -1', false);
};
CPlayer.prototype.setSaturation = function(val) {
    var cmdStr = 'set_property saturation '+ val;
    this.sendCommand(cmdStr, false);
};

CPlayer.prototype.increaseGamma = function() {
    this.sendCommand('step_property gamma', false);
};
CPlayer.prototype.decreaseGamma = function() {
    this.sendCommand('step_property gamma 0 -1', false);
};
CPlayer.prototype.setGamma = function(val) {
    var cmdStr = 'set_property gamma '+ val;
    this.sendCommand(cmdStr, false);
};

//------------------- Callback function ---------------------------

CPlayer.prototype.setStartCallback = function(func) {
    this.startCallback = func;
};

CPlayer.prototype.setPauseCallback = function(func) {
    this.pauseCallback = func;
};

CPlayer.prototype.setStopCallback = function(func) {
    this.stopCallback = func;
};

CPlayer.prototype.setLoopCallback = function(func) {
    this.loopCallback = func;
};

// Main loop when playing file
CPlayer.prototype.playLoop = function() {
    this.m_curPos = this.m_plugin.getInfo(CPlayer.PLAYER_POS, this.m_playerIndex);
    this.m_playerState = this.m_plugin.getInfo(CPlayer.PLAYER_STATE, this.m_playerIndex);
	this.m_duration = this.m_plugin.getInfo(CPlayer.PLAYER_DURATION, this.m_playerIndex);
    if(!this.isStopped() && !this.isIdle()) {
	//dumpMsg("playing");
        if(typeof(this.loopCallback) == "function") {
            this.loopCallback();
        }
        var me = this;
        this.m_timerId = setTimeout(function(){me.playLoop();}, this.m_timeInterval);
    } else {
	//dumpMsg("stopped");
        if(typeof(this.stopCallback) == "function") {
            this.stopCallback();
        }
    }
};
// Set loop interval (unit: ms)
CPlayer.prototype.setLoopInterval = function(timeInterval) {
    this.m_timeInterval = timeInterval;
};

//----------------------- Helper function --------------------------------------
// Translate seconds to hh:mm:ss format
CPlayer.prototype.getTimeString = function(totalSecs) {
    return this.m_plugin.getTimeString(totalSecs);
};

CPlayer.prototype.getCPUInfo = function() {
    return this.m_plugin.getCPUInfo();
};

// Get current video info (xml format)
CPlayer.prototype.getClipInfoXML = function() {
    return this.m_plugin.getClipInfoXML();
};

// Get system environment path (ex. "windir")
CPlayer.prototype.getEnv = function(typeStr) {
    return this.m_plugin.getEnv(typeStr);
};

// Get system code page
CPlayer.prototype.getCodePage = function() {
    return this.m_plugin.syscp;
}

// Get window handle
CPlayer.prototype.getHwnd = function() {
    return this.m_plugin.hwnd;
};

// Get window background color
CPlayer.prototype.getBgColor = function() {
    return this.m_plugin.bgColor;
};

// Get window border color
CPlayer.prototype.getBorderColor = function() {
    return this.m_plugin.borderColor;
};

// Get window border width
CPlayer.prototype.getBorderWidth = function() {
    return this.m_plugin.borderWidth;
};

// Get if keep aspect of video
CPlayer.prototype.getIfKeepAspect = function() {
    return this.m_plugin.keepAspect;
};

// Get if keep aspect of video
CPlayer.prototype.setIfKeepAspect = function(bKeepAspect) {
    this.m_plugin.keepAspect = bKeepAspect;
};

// Whether set window topmost
CPlayer.prototype.setWindowTop = function(bTopmost) {
    this.m_plugin.setWindowTop(bTopmost);
};

// Execute a program
CPlayer.prototype.shellExecute = function(cmd) {
    this.m_plugin.shellExecute(cmd);
};
CPlayer.prototype.enumDir = function(dir, enumDirFunc, bSeekSubDir) {
    this.m_plugin.enumFolder(dir, enumDirFunc, bSeekSubDir);
};

// Minimize the window to system tray
CPlayer.prototype.setTrayIcon = function(bSetToTray) {
    this.m_plugin.setTrayIcon(bSetToTray);
};

