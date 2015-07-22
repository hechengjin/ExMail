/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/ */

const EXPORTED_SYMBOLS = ['MinTrayR'];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const module = Cu.import;

module("resource://mintrayr/trayservice.jsm");

function MinTrayR(menu, menuMapaPlusLock, pref, funcMpPrompt, func) {
  if (!menu || !menuMapaPlusLock) {
    throw Error("no menu");
  }
  // ÓÒ¼ü²Ëµ¥
  this.menu = menu;

  this.document = menu.ownerDocument;
  this.window = this.document.defaultView;

  // ËøÆÁ²Ëµ¥
  this.menulock = menuMapaPlusLock;
  this.documentlock = menuMapaPlusLock.ownerDocument;
  this.windowlock = this.documentlock.defaultView;

// Ë«»÷
  let tp = this;
  this.window.addEventListener(
    'TrayDblClick',
    function(event) {
          return;
      if (event.button == 0 && !!tp.prefs.getExt('dblclickrestore', true)) {
        tp.restore();
      }
    },
    true
  );
// ×ó»÷
  this.window.addEventListener(
    'TrayClick',
    function(event) {
      if (tp.trayClickNotify) {
          tp.trayClickNotify=false;
          funcMpPrompt.call(this);
      }
      else if (event.button == 0 && !tp.prefs.getExt('dblclickrestore', true)) {
          if (true == tp.locked){
              funcMpPrompt.call(this);
              //if (tp.prefs.getExt('messenger.domplogin', false)) {
                  //tp.restore();
                  //tp.showWindows();
                  //tp.locked = false;
              //}
          }
          else{
              tp.restore();
          }
      }
    },
    true
  );
// ÓÒ»÷
  this.window.addEventListener(
    'TrayClick',
    function(event) {
      if (event.button == 2 && tp.prefs.getExt('showcontext', true)) {
        Cu.reportError("show context/" + event.screenX + "_" + event.screenY);
              if (true == tp.locked){
                  tp.showMenuLock(event.screenX, event.screenY);
              }
              else{
                  tp.restore();
                  tp.showMenu(event.screenX, event.screenY);
              }
      }
    },
    true
  );

  if (typeof pref == 'boolean' && pref) {
    this.watch();
  }
  else if (pref) {
    this._watchPref = pref;
    this.prefs.addObserver('extensions.mintrayr.' + this._watchPref, this);
    this.observe(null, null, pref);
  }

  func.call(this);
}

MinTrayR.prototype = {
  _watchPref: null,
  _watched: false,
  prefs: {},
  locked: false,
  trayClickNotify: false,

  showWindows: function() {
      var bMainWindow = true;
      let enumerator = Cc["@mozilla.org/appshell/window-mediator;1"]
                      .getService(Ci.nsIWindowMediator)
                      .getEnumerator(null);

        while(enumerator.hasMoreElements()) {
            let win = enumerator.getNext();
            if (true == bMainWindow) {
                bMainWindow = false;
                this.restore();
            }
            else {
                win.restore();
                win.focus();
            }
        }
  },

  showMenuLock:function MinTrayR_showMenu(x, y) {
    this.menulock.showPopup(
      this.documentlock.documentElement,
      x,
      y,
      "context",
      "",
      "bottomleft"
    );
  },

  showMenu: function MinTrayR_showMenu(x, y) {
    this.menu.showPopup(
      this.document.documentElement,
      x,
      y,
      "context",
      "",
      "bottomleft"
    );
  },
  minimize: function MinTrayR_minimize() {
    this.minimizeWindow(this.window);
  },
  showTray: function MinTrayR_ShowTray() {
      TrayService.showTray(this.window);
  },
  restore: function MinTrayR_restore() {
    this.restoreWindow(this.window);
  },

    change2IconLock: function MinTrayR_change2IconLock() {
        TrayService.change2IconLock();
    },

    change2IconNewMail: function MinTrayR_change2IconNewMail() {
        TrayService.change2IconNewMail();
    },

    restoreIcon: function MinTrayR_restoreIcon() {
        TrayService.restoreIcon();
    },

  get isWatched() this._watched,
  watch: function MinTrayR_watch() {
    if (!this._watched) {
      this.watchWindow(this.window);
      this._watched = true;
    }
  },
  unwatch: function MinTrayR_watch() {
    if (this._watched) {
      this.unwatchWindow(this.window);
      this._watched = false;
    }
  },
  cloneToMenu: function MinTrayR_cloneToMenu(ref, items, bottom) {
    ref = this.document.getElementById(ref);
    if (bottom) {
      ref = ref.nextSibling;
    }
    let rv = []
    for each (let id in items) {
      try {
        let node;
        if (typeof id == 'string') {
          node = this.document.getElementById(id).cloneNode(true);
        }
        else {
          node = id;
        }

        node.setAttribute('mintrayr_origid', node.id);
        node.setAttribute('id', 'MinTrayR_' + node.id);
        if (node.hasAttribute('hidden')) {
          node.removeAttribute('hidden');
        }
        this.menu.insertBefore(node, ref);
        rv.push(node);
      }
      catch (ex) {
        Cu.reportError("Failed to clone " + id);
      }
    }
    return rv;
  },
  addToMenu: function(ref, attrs) {
    ref = this.document.getElementById(ref);
    if (bottom) {
      ref = ref.nextSibling;
    }
    try {
      let node = this.document.createElement('menuitem');
      for (let attr in attrs) {
        node.setAttribute(attr, attrs[attr]);
      }
      this.menu.insertBefore(node, ref);
      return [node];
    }
    catch (ex) {
      Cu.reportError("Failed to create node");
    }
    return [];
  },
  show: function(){
    for (let i = 0; i < arguments.length; ++i) {
      let n = this.document.getElementById(arguments[i]);
      if (n && n.hasAttribute('hidden')) {
        n.removeAttribute('hidden');
      }
    }
  },
  hide: function(){
    for (let i = 0; i < arguments.length; ++i) {
      let n = this.document.getElementById(arguments[i]);
      if (n) {
        n.setAttribute('hidden', true);
      }
    }
  },
  observe: function(subject, topic, data) {
    if (data.indexOf(this._watchPref) != -1) {
      try {
        if (this.prefs.getExt(this._watchPref)) {
          this.watch();
        }
        else {
          this.unwatch();
        }
      }
      catch (ex) {
        Cu.reportError(ex);
      }
    }
  },
  minimizeWindow: function(window) {
    TrayService.minimize(window, false);
  },

  restoreWindow: function(window) {
    TrayService.restore(window);
  },
  restoreAll: function() {
    TrayService.restoreAll();
  },
  watchWindow: function(window) {
    TrayService.watchMinimize(window);
  },
  unwatchWindow: function(window) {
    TrayService.unwatchMinimize(window);
  }
};
module('resource://mintrayr/preferences.jsm', MinTrayR.prototype.prefs);
