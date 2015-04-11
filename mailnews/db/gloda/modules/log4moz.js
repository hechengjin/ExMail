/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const EXPORTED_SYMBOLS = ['Log4Moz'];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

const MODE_RDONLY   = 0x01;
const MODE_WRONLY   = 0x02;
const MODE_CREATE   = 0x08;
const MODE_APPEND   = 0x10;
const MODE_TRUNCATE = 0x20;

const PERMS_FILE      = parseInt("0644", 8);
const PERMS_DIRECTORY = parseInt("0755", 8);

const ONE_BYTE = 1;
const ONE_KILOBYTE = 1024 * ONE_BYTE;
const ONE_MEGABYTE = 1024 * ONE_KILOBYTE;

const DEFAULT_NETWORK_TIMEOUT_DELAY = 5;

const CDATA_START = "<![CDATA[";
const CDATA_END = "]]>";
const CDATA_ESCAPED_END = CDATA_END + "]]&gt;" + CDATA_START;

let Log4Moz = {
  Level: {
    Fatal:  70,
    Error:  60,
    Warn:   50,
    Info:   40,
    Config: 30,
    Debug:  20,
    Trace:  10,
    All:    0,
    Desc: {
      70: "FATAL",
      60: "ERROR",
      50: "WARN",
      40: "INFO",
      30: "CONFIG",
      20: "DEBUG",
      10: "TRACE",
      0:  "ALL"
    }
  },

  /**
   * Create a logger and configure it with dump and console appenders as
   * specified by prefs based on the logger name.
   *
   * E.g., if the loggername is foo, then look for prefs
   *   foo.logging.console
   *   foo.logging.dump
   *
   * whose values can be empty: no logging of that type; or any of
   * 'Fatal', 'Error', 'Warn', 'Info', 'Config', 'Debug', 'Trace', 'All',
   * in which case the logging level for each appender will be set accordingly
   *
   * Parameters:
   *
   * @param loggername The name of the logger
   * @param level (optional) the level of the logger itself
   * @param consoleLevel (optional) the level of the console appender
   * @param dumpLevel (optional) the level of the dump appender
   *
   * As described above, well-named prefs override the last two parameters
   **/

  getConfiguredLogger: function(loggername, level, consoleLevel, dumpLevel) {
    let log = Log4Moz.repository.getLogger(loggername);
    if (log._configured)
      return log

    let formatter = new Log4Moz.BasicFormatter();

    level = level || Log4Moz.Level.Error;

    consoleLevel = consoleLevel || -1;
    dumpLevel = dumpLevel || -1;
    let branch = Cc["@mozilla.org/preferences-service;1"].
                 getService(Ci.nsIPrefService).getBranch(loggername + ".logging.");
    if (branch)
    {
      try {
        // figure out if event-driven indexing should be enabled...
        let consoleLevelString = branch.getCharPref("console");
        if (consoleLevelString) {
          // capitalize to fit with Log4Moz.Level expectations
          consoleLevelString =  consoleLevelString.charAt(0).toUpperCase() +
             consoleLevelString.substr(1).toLowerCase();
          consoleLevel = (consoleLevelString == 'None') ?
                          100 : Log4Moz.Level[consoleLevelString];
        }
      } catch (ex) {
        // Ignore if preference is not found
      }
      try {
        let dumpLevelString = branch.getCharPref("dump");
        if (dumpLevelString) {
          // capitalize to fit with Log4Moz.Level expectations
          dumpLevelString =  dumpLevelString.charAt(0).toUpperCase() +
             dumpLevelString.substr(1).toLowerCase();
          dumpLevel = (dumpLevelString == 'None') ?
                       100 : Log4Moz.Level[dumpLevelString];
        }
      } catch (ex) {
        // Ignore if preference is not found
      }
    }

    if (consoleLevel != 100) {
      if (consoleLevel == -1)
        consoleLevel = Log4Moz.Level.Error;
      let capp = new Log4Moz.ConsoleAppender(formatter);
      capp.level = consoleLevel;
      log.addAppender(capp);
    }

    if (dumpLevel != 100) {
      if (dumpLevel == -1)
        dumpLevel = Log4Moz.Level.Error;
      let dapp = new Log4Moz.DumpAppender(formatter);
      dapp.level = dumpLevel;
      log.addAppender(dapp);
    }

    log.level = Math.min(level, Math.min(consoleLevel, dumpLevel));

    log._configured = true;

    return log;
  },

  get repository() {
    delete Log4Moz.repository;
    Log4Moz.repository = new LoggerRepository();
    return Log4Moz.repository;
  },
  set repository(value) {
    delete Log4Moz.repository;
    Log4Moz.repository = value;
  },

  get LogMessage() { return LogMessage; },
  get Logger() { return Logger; },
  get LoggerRepository() { return LoggerRepository; },

  get Formatter() { return Formatter; },
  get BasicFormatter() { return BasicFormatter; },
  get XMLFormatter() { return XMLFormatter; },
  get JSONFormatter() { return JSONFormatter; },
  get Appender() { return Appender; },
  get DumpAppender() { return DumpAppender; },
  get ConsoleAppender() { return ConsoleAppender; },
  get TimeAwareMemoryBucketAppender() { return TimeAwareMemoryBucketAppender; },
  get FileAppender() { return FileAppender; },
  get SocketAppender() { return SocketAppender; },
  get RotatingFileAppender() { return RotatingFileAppender; },
  get ThrowingAppender() { return ThrowingAppender; },

  // Logging helper:
  // let logger = Log4Moz.repository.getLogger("foo");
  // logger.info(Log4Moz.enumerateInterfaces(someObject).join(","));
  enumerateInterfaces: function Log4Moz_enumerateInterfaces(aObject) {
    let interfaces = [];

    for (i in Ci) {
      try {
        aObject.QueryInterface(Ci[i]);
        interfaces.push(i);
      }
      catch(ex) {}
    }

    return interfaces;
  },

  // Logging helper:
  // let logger = Log4Moz.repository.getLogger("foo");
  // logger.info(Log4Moz.enumerateProperties(someObject).join(","));
  enumerateProperties: function Log4Moz_enumerateProps(aObject,
                                                       aExcludeComplexTypes) {
    let properties = [];

    for (p in aObject) {
      try {
        if (aExcludeComplexTypes &&
            (typeof aObject[p] == "object" || typeof aObject[p] == "function"))
          continue;
        properties.push(p + " = " + aObject[p]);
      }
      catch(ex) {
        properties.push(p + " = " + ex);
      }
    }

    return properties;
  }
};

function LoggerContext() {
  this._started = this._lastStateChange = Date.now();
  this._state = "started";
}
LoggerContext.prototype = {
  _jsonMe: true,
  _id: "unknown",
  setState: function LoggerContext_state(aState) {
    this._state = aState;
    this._lastStateChange = Date.now();
    return this;
  },
  finish: function LoggerContext_finish() {
    this._finished = Date.now();
    this._state = "finished";
    return this;
  },
  toString: function LoggerContext_toString() {
    return "[Context: " + this._id + " state: " + this._state + "]";
  }
};


/*
 * LogMessage
 * Encapsulates a single log event's data
 */
function LogMessage(loggerName, level, messageObjects){
  this.loggerName = loggerName;
  this.messageObjects = messageObjects;
  this.level = level;
  this.time = Date.now();
}
LogMessage.prototype = {
  get levelDesc() {
    if (this.level in Log4Moz.Level.Desc)
      return Log4Moz.Level.Desc[this.level];
    return "UNKNOWN";
  },

  toString: function LogMsg_toString(){
    return "LogMessage [" + this.time + " " + this.level + " " +
      this.messageObjects + "]";
  }
};

/*
 * Logger
 * Hierarchical version.  Logs to all appenders, assigned or inherited
 */

function Logger(name, repository) {
  this._init(name, repository);
}
Logger.prototype = {
  _init: function Logger__init(name, repository) {
    if (!repository)
      repository = Log4Moz.repository;
    this._name = name;
    this.children = [];
    this.ownAppenders = [];
    this.appenders = [];
    this._repository = repository;
  },

  get name() {
    return this._name;
  },

  _level: null,
  get level() {
    if (this._level != null)
      return this._level;
    if (this.parent)
      return this.parent.level;
    dump("log4moz warning: root logger configuration error: no level defined\n");
    return Log4Moz.Level.All;
  },
  set level(level) {
    this._level = level;
  },

  _parent: null,
  get parent() this._parent,
  set parent(parent) {
    if (this._parent == parent) {
      return;
    }
    // Remove ourselves from parent's children
    if (this._parent) {
      let index = this._parent.children.indexOf(this);
      if (index != -1) {
        this._parent.children.splice(index, 1);
      }
    }
    this._parent = parent;
    parent.children.push(this);
    this.updateAppenders();
  },

  updateAppenders: function updateAppenders() {
    if (this._parent) {
      let notOwnAppenders = this._parent.appenders.filter(function(appender) {
        return this.ownAppenders.indexOf(appender) == -1;
      }, this);
      this.appenders = notOwnAppenders.concat(this.ownAppenders);
    } else {
      this.appenders = this.ownAppenders.slice();
    }

    // Update children's appenders.
    for (let i = 0; i < this.children.length; i++) {
      this.children[i].updateAppenders();
    }
  },

  addAppender: function Logger_addAppender(appender) {
    if (this.ownAppenders.indexOf(appender) != -1) {
      return;
    }
    this.ownAppenders.push(appender);
    this.updateAppenders();
  },

  _nextContextId: 0,
  newContext: function Logger_newContext(objWithProps) {
    if (!("_id" in objWithProps))
      objWithProps._id = this._name + ":" + (++this._nextContextId);
    objWithProps.__proto__ = LoggerContext.prototype;
    objWithProps._isContext = true;
    LoggerContext.call(objWithProps);
    return objWithProps;
  },

  log: function Logger_log(message) {
    if (this.level > message.level)
      return;
    let appenders = this.appenders;
    for (let i = 0; i < appenders.length; i++){
      appenders[i].append(message);
    }
  },

  removeAppender: function Logger_removeAppender(appender) {
    let index = this.ownAppenders.indexOf(appender);
    if (index == -1) {
      return;
    }
    this.ownAppenders.splice(index, 1);
    this.updateAppenders();
  },

  log: function Logger_log(level, args) {
    if (this.level > level)
      return;

    // Hold off on creating the message object until we actually have
    // an appender that's responsible.
    let message;
    let appenders = this.appenders;
    for (let i = 0; i < appenders.length; i++){
      let appender = appenders[i];
      if (appender.level > level)
        continue;

      if (!message)
        message = new LogMessage(this._name, level,
                                 Array.prototype.slice.call(args));

      appender.append(message);
    }
  },

  fatal: function Logger_fatal() {
    this.log(Log4Moz.Level.Fatal, arguments);
  },
  error: function Logger_error() {
    this.log(Log4Moz.Level.Error, arguments);
  },
  warn: function Logger_warn() {
    this.log(Log4Moz.Level.Warn, arguments);
  },
  info: function Logger_info(string) {
    this.log(Log4Moz.Level.Info, arguments);
  },
  config: function Logger_config(string) {
    this.log(Log4Moz.Level.Config, arguments);
  },
  debug: function Logger_debug(string) {
    this.log(Log4Moz.Level.Debug, arguments);
  },
  trace: function Logger_trace(string) {
    this.log(Log4Moz.Level.Trace, arguments);
  }
};

/*
 * LoggerRepository
 * Implements a hierarchy of Loggers
 */

function LoggerRepository() {}
LoggerRepository.prototype = {
  _loggers: {},

  _rootLogger: null,
  get rootLogger() {
    if (!this._rootLogger) {
      this._rootLogger = new Logger("root", this);
      this._rootLogger.level = Log4Moz.Level.All;
    }
    return this._rootLogger;
  },
  set rootLogger(logger) {
    throw "Cannot change the root logger";
  },

  _updateParents: function LogRep__updateParents(name) {
    let pieces = name.split('.');
    let cur, parent;

    // find the closest parent
    // don't test for the logger name itself, as there's a chance it's already
    // there in this._loggers
    for (let i = 0; i < pieces.length - 1; i++) {
      if (cur)
        cur += '.' + pieces[i];
      else
        cur = pieces[i];
      if (cur in this._loggers)
        parent = cur;
    }

    // if we didn't assign a parent above, there is no parent
    if (!parent)
      this._loggers[name].parent = this.rootLogger;
    else
      this._loggers[name].parent = this._loggers[parent];

    // trigger updates for any possible descendants of this logger
    for (let logger in this._loggers) {
      if (logger != name && logger.indexOf(name) == 0)
        this._updateParents(logger);
    }
  },

  getLogger: function LogRep_getLogger(name) {
    if (name in this._loggers)
      return this._loggers[name];
    this._loggers[name] = new Logger(name, this);
    this._updateParents(name);
    return this._loggers[name];
  }
};

/*
 * Formatters
 * These massage a LogMessage into whatever output is desired
 * Only the BasicFormatter is currently implemented
 */

// Abstract formatter
function Formatter() {}
Formatter.prototype = {
  format: function Formatter_format(message) {}
};

// services' log4moz lost the date formatting default...
function BasicFormatter(dateFormat) {
  if (dateFormat)
    this.dateFormat = dateFormat;
}
BasicFormatter.prototype = {
  __proto__: Formatter.prototype,

  _dateFormat: null,

  get dateFormat() {
    if (!this._dateFormat)
      this._dateFormat = "%Y-%m-%d %H:%M:%S";
    return this._dateFormat;
  },

  set dateFormat(format) {
    this._dateFormat = format;
  },

  format: function BF_format(message) {
    let date = new Date(message.time);
    // The trick below prevents errors further down because mo is null or
    //  undefined.
    let messageString = [
      ("" + mo) for each
      ([,mo] in Iterator(message.messageObjects))].join(" ");
    return date.toLocaleFormat(this.dateFormat) + "\t" +
      message.loggerName + "\t" + message.levelDesc + "\t" +
      messageString + "\n";
  }
};

/*
 * XMLFormatter
 * Format like log4j's XMLLayout.  The intent is that you can hook this up to
 * a SocketAppender and point them at a Chainsaw GUI running with an
 * XMLSocketReceiver running.  Then your output comes out in Chainsaw.
 * (Chainsaw is log4j's GUI that displays log output with niceties such as
 * filtering and conditional coloring.)
 */

function XMLFormatter() {}
XMLFormatter.prototype = {
  __proto__: Formatter.prototype,

  format: function XF_format(message) {
    let cdataEscapedMessage =
      [((typeof(mo) == "object") ? mo.toString() : mo) for each
       ([,mo] in Iterator(message.messageObjects))]
        .join(" ")
        .replace(CDATA_END, CDATA_ESCAPED_END, "g");
    return "<log4j:event logger='" + message.loggerName + "' " +
                        "level='" + message.levelDesc + "' thread='unknown' " +
                        "timestamp='" + message.time + "'>" +
      "<log4j:message><![CDATA[" + cdataEscapedMessage + "]]></log4j:message>" +
      "</log4j:event>";
  }
};

function JSONFormatter() {
}
JSONFormatter.prototype = {
  __proto__: Formatter.prototype,

  format: function JF_format(message) {
    // XXX I did all kinds of questionable things in here; they should be
    //  resolved...
    // 1) JSON does not walk the __proto__ chain; there is no need to clobber
    //   it.
    // 2) Our net mutation is sorta redundant messageObjects alongside
    //   msgObjects, although we only serialize one.
    let origMessageObjects = message.messageObjects;
    message.messageObjects = [];
    let reProto = [];
    for each (let [, messageObject] in Iterator(origMessageObjects)) {
      if (messageObject)
        if (messageObject._jsonMe) {
          message.messageObjects.push(messageObject);
          // temporarily strip the prototype to avoid JSONing the impl.
          reProto.push([messageObject, messageObject.__proto__]);
          messageObject.__proto__ = undefined;
        }
        else
          message.messageObjects.push(messageObject.toString());
      else
        message.messageObjects.push(messageObject);
    }
    let encoded = JSON.stringify(message) + "\r\n";
    message.msgObjects = origMessageObjects;
    for each (let [,objectAndProtoPair] in Iterator (reProto)) {
      objectAndProtoPair[0].__proto__ = objectAndProtoPair[1];
    }
    return encoded;
  }
};


/*
 * Appenders
 * These can be attached to Loggers to log to different places
 * Simply subclass and override doAppend to implement a new one
 */

function Appender(formatter) {
  this._name = "Appender";
  this._formatter = formatter? formatter : new BasicFormatter();
}
Appender.prototype = {
  _level: Log4Moz.Level.All,

  append: function App_append(message) {
    this.doAppend(this._formatter.format(message));
  },
  toString: function App_toString() {
    return this._name + " [level=" + this._level +
      ", formatter=" + this._formatter + "]";
  },
  doAppend: function App_doAppend(message) {}
};

/*
 * DumpAppender
 * Logs to standard out
 */

function DumpAppender(formatter) {
  this._name = "DumpAppender";
  this._formatter = formatter? formatter : new BasicFormatter();
}
DumpAppender.prototype = {
  __proto__: Appender.prototype,

  doAppend: function DApp_doAppend(message) {
    dump(message);
  }
};

/**
 * An in-memory appender that always logs to its in-memory bucket and associates
 * each message with a timestamp.  Whoever creates us is responsible for causing
 * us to switch to a new bucket using whatever criteria is appropriate.
 *
 * This is intended to be used roughly like an in-memory circular buffer.  The
 * expectation is that we are being used for unit tests and that each unit test
 * function will get its own bucket.  In the event that a test fails we would
 * be asked for the contents of the current bucket and some portion of the
 * previous bucket using up to some duration.
 */
function TimeAwareMemoryBucketAppender() {
  this._name = "TimeAwareMemoryBucketAppender";
  this._level = Log4Moz.Level.All;

  this._lastBucket = null;
  // to minimize object construction, even indices are timestamps, odd indices
  //  are the message objects.
  this._curBucket = [];
  this._curBucketStartedAt = Date.now();
}
TimeAwareMemoryBucketAppender.prototype = {
  get level() { return this._level; },
  set level(level) { this._level = level; },

  append: function TAMBA_append(message) {
    if (this._level <= message.level)
      this._curBucket.push(message);
  },

  newBucket: function() {
    this._lastBucket = this._curBucket;
    this._curBucketStartedAt = Date.now();
    this._curBucket = [];
  },

  getPreviousBucketEvents: function(aNumMS) {
    let lastBucket = this._lastBucket;
    if (lastBucket == null || !lastBucket.length)
      return [];
    let timeBound = this._curBucketStartedAt - aNumMS;
    // seek backwards through the list...
    let i;
    for (i = lastBucket.length - 1; i >= 0; i --) {
      if (lastBucket[i].time < timeBound)
        break;
    }
    return lastBucket.slice(i+1);
  },

  getBucketEvents: function() {
    return this._curBucket.concat();
  },

  toString: function() {
    return "[TimeAwareMemoryBucketAppender]";
  },
};

/*
 * ConsoleAppender
 * Logs to the javascript console
 */

function ConsoleAppender(formatter) {
  this._name = "ConsoleAppender";
  this._formatter = formatter;
}
ConsoleAppender.prototype = {
  __proto__: Appender.prototype,

  // override to send Error and higher level messages to Components.utils.reportError()
  append: function CApp_append(message) {
    let stringMessage = this._formatter.format(message);
    if (message.level > Log4Moz.Level.Warn) {
      Cu.reportError(stringMessage);
    }
    this.doAppend(stringMessage);
  },

  doAppend: function CApp_doAppend(message) {
    Cc["@mozilla.org/consoleservice;1"].
      getService(Ci.nsIConsoleService).logStringMessage(message);
  }
};

/*
 * FileAppender
 * Logs to a file
 */

function FileAppender(file, formatter) {
  this._name = "FileAppender";
  this._file = file; // nsIFile
  this._formatter = formatter? formatter : new BasicFormatter();
}
FileAppender.prototype = {
  __proto__: Appender.prototype,

  __fos: null,
  get _fos() {
    if (!this.__fos)
      this.openStream();
    return this.__fos;
  },

  openStream: function FApp_openStream() {
    this.__fos = Cc["@mozilla.org/network/file-output-stream;1"].
      createInstance(Ci.nsIFileOutputStream);
    let flags = MODE_WRONLY | MODE_CREATE | MODE_APPEND;
    this.__fos.init(this._file, flags, PERMS_FILE, 0);
  },

  closeStream: function FApp_closeStream() {
    if (!this.__fos)
      return;
    try {
      this.__fos.close();
      this.__fos = null;
    } catch(e) {
      dump("Failed to close file output stream\n" + e);
    }
  },

  doAppend: function FApp_doAppend(message) {
    if (message === null || message.length <= 0)
      return;
    try {
      this._fos().write(message, message.length);
    } catch(e) {
      dump("Error writing file:\n" + e);
    }
  },

  clear: function FApp_clear() {
    this.closeStream();
    this._file.remove(false);
  }
};

/*
 * RotatingFileAppender
 * Similar to FileAppender, but rotates logs when they become too large
 */

function RotatingFileAppender(file, formatter, maxSize, maxBackups) {
  if (maxSize === undefined)
    maxSize = ONE_MEGABYTE * 2;

  if (maxBackups === undefined)
    maxBackups = 0;

  this._name = "RotatingFileAppender";
  this._file = file; // nsIFile
  this._formatter = formatter? formatter : new BasicFormatter();
  this._maxSize = maxSize;
  this._maxBackups = maxBackups;
}
RotatingFileAppender.prototype = {
  __proto__: FileAppender.prototype,

  doAppend: function RFApp_doAppend(message) {
    if (message === null || message.length <= 0)
      return;
    try {
      this.rotateLogs();
      this._fos.write(message, message.length);
    } catch(e) {
      dump("Error writing file:\n" + e);
    }
  },
  rotateLogs: function RFApp_rotateLogs() {
    if(this._file.exists() &&
       this._file.fileSize < this._maxSize)
      return;

    this.closeStream();

    for (let i = this.maxBackups - 1; i > 0; i--){
      let backup = this._file.parent.clone();
      backup.append(this._file.leafName + "." + i);
      if (backup.exists())
        backup.moveTo(this._file.parent, this._file.leafName + "." + (i + 1));
    }

    let cur = this._file.clone();
    if (cur.exists())
      cur.moveTo(cur.parent, cur.leafName + ".1");

    // Note: this._file still points to the same file
  }
};

/*
 * SocketAppender
 * Logs via TCP to a given host and port.  Attempts to automatically reconnect
 * when the connection drops or cannot be initially re-established.  Connection
 * attempts will happen at most every timeoutDelay seconds (has a sane default
 * if left blank).  Messages are dropped when there is no connection.
 */

function SocketAppender(host, port, formatter, timeoutDelay) {
  this._name = "SocketAppender";
  this._host = host;
  this._port = port;
  this._formatter = formatter? formatter : new BasicFormatter();
  this._timeout_delay = timeoutDelay || DEFAULT_NETWORK_TIMEOUT_DELAY;

  this._socketService = Cc["@mozilla.org/network/socket-transport-service;1"]
                          .getService(Ci.nsISocketTransportService);
  this._mainThread =
    Cc["@mozilla.org/thread-manager;1"].getService().mainThread;
}
SocketAppender.prototype = {
  __proto__: Appender.prototype,

  __nos: null,
  get _nos() {
    if (!this.__nos)
      this.openStream();
    return this.__nos;
  },
  _nextCheck: 0,
  openStream: function SApp_openStream() {
    let now = Date.now();
    if (now <= this._nextCheck) {
      return;
    }
    this._nextCheck = now + this._timeout_delay * 1000;
    try {
      this._transport = this._socketService.createTransport(
        null, 0, // default socket type
        this._host, this._port,
        null); // no proxy
      this._transport.setTimeout(Ci.nsISocketTransport.TIMEOUT_CONNECT,
                                 this._timeout_delay);
      // do not set a timeout for TIMEOUT_READ_WRITE. The timeout is not
      //  entirely intuitive; your socket will time out if no one reads or
      //  writes to the socket within the timeout.  That, as you can imagine,
      //  is not what we want.
      this._transport.setEventSink(this, this._mainThread);

      let outputStream = this._transport.openOutputStream(
        0, // neither blocking nor unbuffered operation is desired
        0, // default buffer size is fine
        0 // default buffer count is fine
        );

      let uniOutputStream = Cc["@mozilla.org/intl/converter-output-stream;1"]
                              .createInstance(Ci.nsIConverterOutputStream);
      uniOutputStream.init(outputStream, "utf-8", 0, 0x0000);

      this.__nos = uniOutputStream;
    } catch (ex) {
      dump("Unexpected SocketAppender connection problem: " +
           ex.fileName + ":" + ex.lineNumber + ": " + ex + "\n");
    }
  },

  closeStream: function SApp_closeStream() {
    if (!this._transport)
      return;
    try {
      this._connected = false;
      this._transport = null;
      let nos = this.__nos;
      this.__nos = null;
      nos.close();
    } catch(e) {
      // this shouldn't happen, but no one cares
    }
  },

  doAppend: function SApp_doAppend(message) {
    if (message === null || message.length <= 0)
      return;
    try {
      let nos = this._nos;
      if (nos)
        nos.writeString(message);
    } catch(e) {
      if (this._transport && !this._transport.isAlive()) {
        this.closeStream();
      }
    }
  },

  clear: function SApp_clear() {
    this.closeStream();
  },

  /* nsITransportEventSink */
  onTransportStatus: function SApp_onTransportStatus(aTransport, aStatus,
      aProgress, aProgressMax) {
    if (aStatus == 0x804b0004) // STATUS_CONNECTED_TO is not a constant.
      this._connected = true;
  },
};

/**
 * Throws an exception whenever it gets a message.  Intended to be used in
 * automated testing situations where the code would normally log an error but
 * not die in a fatal manner.
 */
function ThrowingAppender(thrower, formatter) {
  this._name = "ThrowingAppender";
  this._formatter = formatter? formatter : new BasicFormatter();
  this._thrower = thrower;
}
ThrowingAppender.prototype = {
  __proto__: Appender.prototype,

  doAppend: function TApp_doAppend(message) {
    if (this._thrower)
      this._thrower(message);
    else
      throw message;
  }
};
