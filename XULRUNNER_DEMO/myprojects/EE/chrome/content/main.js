/*

var gswfPlayer = null;   // Control the playing of SWF filevar swfPlayer = null;   // Control the playing of SWF file


var resizers = ["resizer-left", "resizer-right", "resizer-top", "resizer-bottom", "resizer-topleft", "resizer-bottomright"];

function onload() {
  gswfPlayer = new SwfPlayer();
  gswfPlayer.setLoopCallback(swfPlayCallback);
  gswfPlayer.setStopCallback(swfStopCallback);

  
}

function resizerhidden()
{
  for(var i in resizers)
            $e(resizers[i]).hidden = true;
}



function swfPlayCallback()
{
  var curFrmae = gswfPlayer.getCurFrame();
  //progressbar.value = curFrmae;
}

function swfStopCallback()
{
 // mp.enablePlayNext(true);
  //updatePlayerState();
}


function loadswf()
{
  //playSwf("E:\\svn\\branch\\xulrunner_faststart\\test\\mybrowser\\chrome\\content\\a.swf");
  var fileName = openFile();
  //alert(fileName)
  playSwf(fileName)
}

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
//    if (!mp.isPlaying()) {
  //    startPlay(localFiles[0].path);
  //  }
    for(var i=0; i<localFiles.length; ++i) {
      var filesize = localFiles[i].fileSize;
      filesize /= 1024; //Convert to KB
      filename = localFiles[i].path;
      // add file to playlist
      //addFile(filename, filesize);
    }
  }
  return filename;
}

function playSwf(filename)
{
 // alert(filename)
  //alert(gswfPlayer.setPlayFile)
  gswfPlayer.setPlayFile(filename);
 // mp.enablePlayNext(false);
  setTimeout(function() {
    gswfPlayer.play();
    var totalFrames = gswfPlayer.getTotalFrames();
 
   // if ( totalFrames > 0) {
   //   enable("timebar");
   //   for (var i = 1; i < playButtons.length; i++) {
   //     enable(playButtons[i]);
   //   }
   //   progressbar.setAttribute("max", totalFrames);
   //   setAttr("play_icon", "command", "cmdPause");
   // } else {
   //   disable("timebar");
   // }
  }, 500);
  //changeVideoBoxPage(INDEX_SWF);
}

//////SwfPlayer start/////
function SwfPlayer()
{
  this.plugin = document.getElementById("swfVideo");
  this.file = null;   // file name
  this.loopCallback = null;
  this.stopCallback = null;
  this.timerId = null;
}

SwfPlayer.prototype.setPlayFile = function(fileName) {
  if(fileName == null) return;
  this.file = fileName;
  //alert(this.file)
  //alert(this.plugin)
  //alert(this.plugin.LoadMovie)
  this.plugin.setAttribute('src', this.file);
  this.plugin.LoadMovie(0, this.file);
  //this.plugin.setAttribute('src', this.file);
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
////////SwfPlayer end////

addEventListener("load", onload, false);

*/