/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/ */

Components.utils.import("resource://gre/modules/Services.jsm");
var gPrefBranch = Components.classes["@mozilla.org/preferences-service;1"].getService(Components.interfaces.nsIPrefBranch);
var gSelf=null;
var gRemoveMPWindowEnable=false;
var gMinTrayR = {};
const gMasterPasswordDlgH = 150;
const gMasterPasswordDlgW = 300;
const gTopGap = 48;
const gLeftGap = 190;
addEventListener(
  "load",
  function() {
      function $(id) document.getElementById(id);
      removeEventListener("load", arguments.callee, true);

      Components.utils.import("resource://mintrayr/mintrayr.jsm", gMinTrayR);
      gMinTrayR = new gMinTrayR.MinTrayR(
      document.getElementById('MinTrayR_context'),
      document.getElementById('MinTrayR_mapapluslock'),  // "解除锁定"菜单
      "messenger.watchmessenger",
      // 左击托盘响应,显示输入锁屏密码对话框
      function() {
          (function(self) {
              if (gRemoveMPWindowEnable) {
                  return;
              }

              let acctMgr = Cc["@mozilla.org/messenger/account-manager;1"]
                            .getService(Ci.nsIMsgAccountManager);

              let count = acctMgr.accounts.Count();
              if (count > 0) {
                  let dlgH = gMasterPasswordDlgH;
                  let dlgW = gMasterPasswordDlgW;
                  let scrH = window.screen.availHeight;
                  let scrW = window.screen.availWidth;
                  let top = (scrH-gTopGap-dlgH)/2;
                  let left = (scrW-gLeftGap-dlgW)/2;
                  window.openDialog("chrome://mintrayr/content/messenger/mpprompt.xul",
                        "masterPasswordPromptDialog",
                        'top='+top+',left='+left+', modal=yes'); 
              }

              var t = gPrefBranch.getBoolPref("extensions.mintrayr.messenger.domplogin");
              if(t) {
                  gMinTrayR.restoreIcon();
                  gMinTrayR.showWindows();
                  gMinTrayR.locked=false;
              }
          })(this);
      },

      function() {
        try {
          // Seamonkey hack
          let n = document.querySelector('#menu_NewPopup menuitem');
          if (n && !n.id)  {
            n.id = 'newNewMsgCmd';
          }
        }
        catch (ex) {
          // no-op
        }

        // Correctly tray-on-close, as Thunderbird 17 owner-draws the window controls in non-aero windows.
        // This will remove the commands from the close button widgets and add an own window handler.
        // Sucks, but there is not really another way.
        (function(self) {
          if (!gSelf) {
              gSelf = self;
          }
          function MinTrayRTryCloseWindow(event){
            if (self.prefs.getExt("messenger.watchmessenger", true)
              && (self.prefs.getExt('minimizeon', 1) & (1<<1))) {
              self.minimize();
              event.preventDefault();
              event.stopPropagation();
              return false;
            }
            // must be in sync with the original command
            return goQuitApplication();
          }
          
          function MinTrayRTryMinimizeWindow(event) {
            if (self.prefs.getExt("messenger.watchmessenger", true)
                && (self.prefs.getExt('minimizeon', 1) & (1<<0))) {
                self.minimize();
                event.preventDefault();
                event.stopPropagation();
                return false;
              }
              // must be in sync with the original command
              return window.minimize();
          }

          function hijackButton(newCommand, id) {
            let button = $(id);
            if (!button) {
              // Only available in Thunderbird 17
              return;
            }

            // Remove old command(s)
            button.removeAttribute('command');
            button.removeAttribute('oncommand');

            // Add ourselves
            button.addEventListener('command', newCommand, false);
          }

          //['titlebar-close'].forEach(hijackButton.bind(null, MinTrayRTryCloseWindow));
          //['titlebar-min'].forEach(hijackButton.bind(null, MinTrayRTryMinimizeWindow));

            var closeButton = $("titlebox").closeButton;
            closeButton.removeAttribute("oncommand");
            closeButton.addEventListener("command", function() {
                MinTrayRTryCloseWindow();
            });

        })(this);

/*
        this.cloneToMenu('MinTrayR_sep-top', ["newNewMsgCmd", "button-getAllNewMsg", "addressBook"], false);
        this.cloneToMenu('MinTrayR_sep-bottom', ['menu_FileQuitItem'], true);
        document
          .getElementById('MinTrayR_newNewMsgCmd')
          .setAttribute(
            'label',
            this.menu.getAttribute('mintrayr_newmessage')
          );
*/
      });
	  
	  gMinTrayR.showTray();

      var mailListener = {
          OnItemIntPropertyChanged: function(aItem, aProperty, aOldValue, aNewValue) {
              if (aProperty == "BiffState") {
                  if (aNewValue == Components.interfaces.nsIMsgFolder.nsMsgBiffState_NewMail) {
                      gMinTrayR.change2IconNewMail();
                  } else if (aNewValue == Components.interfaces.nsIMsgFolder.nsMsgBiffState_NoMail) {
                      if (!gMinTrayR.locked){
                          gMinTrayR.restoreIcon();
                      }
                  }
              }
          }
      };

      let session = Cc["@mozilla.org/messenger/services/session;1"]
                          .getService(Ci.nsIMsgMailSession);
      session.AddFolderListener(mailListener, Components.interfaces.nsIFolderListener.intPropertyChanged);
      // 添加锁屏监听
      addObserver(masterPasswordPlusListener,"doLockClient", false);
      addObserver(masterPasswordPlusListener,"doUnLockClient", false);
      addObserver(masterPasswordPlusListener,"removeMasterPassword", false);
      addObserver(masterPasswordPlusListener,"masterPasswordPrompt", false);
      addObserver(masterPasswordPlusListener, "removeMasterPasswordOk", false);
  },
  false
);

function onAccept()
{
    const Cc = Components.classes;
    const Ci = Components.interfaces;

    let acctMgr = Cc["@mozilla.org/messenger/account-manager;1"]
                  .getService(Ci.nsIMsgAccountManager);

    let count = acctMgr.accounts.Count();
    if (count > 0) {
        var pk11db = Cc["@mozilla.org/security/pk11tokendb;1"]
                    .getService(Ci.nsIPK11TokenDB);
        var _token = pk11db.getInternalKeyToken();
        var masterPassword = document.getElementById("mpTextBox").value;
        var mainPassword = Cc["@mozilla.org/preferences-service;1"]
                   .getService(Ci.nsIPrefService)
                   .getBranch(null).getCharPref("richinfo.masterPass");
      
        if (mainPassword != masterPassword) {
              document.getElementById("masterPasswordErr").setAttribute("hidden", false);
        }

        var errCode = _token.loginEx(true, masterPassword);
        document.getElementById("masterPasswordErr").setAttribute("hidden", true);
        gPrefBranch.setBoolPref("extensions.mintrayr.messenger.domplogin", true);
        Services.prefs.savePrefFile(null);
    }
}

function onCancel()
{
    gPrefBranch.setBoolPref("extensions.mintrayr.messenger.domplogin", false);
    Services.prefs.savePrefFile(null);
    window.close();
}

function removeMasterPasswordPrompt()
{
    window.close();
    notifyMasterPasswordPlus("removeMasterPassword");
}

function openRemoveMasterPasswordDialog()
{
    gRemoveMPWindowEnable = true;
    let securityPane = false;
    let mpLogin = false;
    let dlgH = gMasterPasswordDlgH;
    let dlgW = gMasterPasswordDlgW;
    let scrH = window.screen.availHeight;
    let scrW = window.screen.availWidth;
    let top = (scrH-gTopGap-dlgH)/2;
    let left = (scrW-gLeftGap-dlgW)/2;
    window.openDialog("chrome://mozapps/content/preferences/removemp.xul",
          "removeMPWindow",
          'top='+top+',left='+left+', modal=no',
          {securityPane:securityPane,
           mpLogin:mpLogin}); 
    //gRemoveMPWindowEnable = false;
}

function removeMasterPasswordOk()
{
    gRemoveMPWindowEnable = false;
    gSelf.locked = false;
    gSelf.trayClickNotify=false;
    gSelf.showWindows();
    gSelf.restoreIcon();

    gPrefBranch.setBoolPref("extensions.mintrayr.messenger.domplogin", true);
    gPrefBranch.setCharPref("richinfo.masterPass","");
    Services.prefs.savePrefFile(null);
}


function masterPasswordPrompt()
{
    notifyMasterPasswordPlus("masterPasswordPrompt");
}

function openMasterPasswordDialog()
{
    gSelf.trayClickNotify=true;
    gRemoveMPWindowEnable = false;
    setTimeout(function(){
        var domWin = document.getElementById("MinTrayR_context").ownerDocument.defaultView;
        let event = domWin.document.createEvent("Events");
        event.initEvent("TrayClick", true, true);
        domWin.dispatchEvent(event);
    }, 50);
}

function lock()
{
    notifyMasterPasswordPlus("doLockClient");
}

function doLock()
{
    try {
        if (!gSelf) {
            return;
        }

        var count = getAccountsCount();
        if (count < 1) {
            return;
        }

        collectAccountInfo();
        if (!isSetMasterPassword()) {
            changeMasterPasswordPrompt();
            return;
        }

        hideWindows();
        gSelf.locked = true;
        gSelf.change2IconLock();

        gPrefBranch.setBoolPref("extensions.mintrayr.messenger.domplogin", false);
        Services.prefs.savePrefFile(null);
    }
    catch(e) {
        throw new Error("doLock fucking error");
    }
}

function unlock()
{
    notifyMasterPasswordPlus("doUnLockClient");
}

function doUnLock()
{
    try {
        if (gRemoveMPWindowEnable || !gSelf) {
            return;
        }

        var count = getAccountsCount();
        var set = isSetMasterPassword();
        if (!set || count < 1) {
            return;
        }

        let dlgH = gMasterPasswordDlgH;
        let dlgW = gMasterPasswordDlgW;
        let scrH = window.screen.availHeight;
        let scrW = window.screen.availWidth;
        let top = (scrH-gTopGap-dlgH)/2;
        let left = (scrW-gLeftGap-dlgW)/2;
        window.openDialog("chrome://mintrayr/content/messenger/mpprompt.xul",
                          "masterPasswordPromptDialog",
                          'top='+top+',left='+left+', modal=yes');

        // window.openDialog("chrome://mintrayr/content/messenger/mpprompt.xul", 
        //                   "masterPasswordPromptDialog",
        //                   "modal, center");

        var t = gPrefBranch.getBoolPref("extensions.mintrayr.messenger.domplogin");
        if (true == t) {
            gSelf.locked = false;
            gSelf.showWindows();
            gSelf.restoreIcon();
        }
    }
    catch(e) {
        throw new Error("doUnLock fucking error");
    }
}

// 屏蔽系统的锁屏密码弹窗
function hideSystemMasterPasswordPrompt()
{
    var pk11db = Cc["@mozilla.org/security/pk11tokendb;1"]
                 .getService(Ci.nsIPK11TokenDB);
    var _token = pk11db.getInternalKeyToken();
    var login = _token.needsLogin() ? true : false;
    if (login){
        var mainPassword = Cc["@mozilla.org/preferences-service;1"]
                           .getService(Ci.nsIPrefService)
                           .getBranch(null).getCharPref("richinfo.masterPass");
        _token.loginEx(true, mainPassword);
    }
}

// 收集邮箱账户信息以备删除密码之需
function collectAccountInfo()
{
    hideSystemMasterPasswordPrompt();
    let passwordmanager = Cc["@mozilla.org/login-manager;1"].getService(Ci.nsILoginManager);
    let logins = passwordmanager.getAllLogins();
    let fullEmail;
    let username;
    let hostname;
    let fullinfo = "[";
    for (let i = 0; i < logins.length; i++)
    {
        username = logins[i].username;
        hostname = logins[i].hostname;
         let fullEmail = username + "@";
        let token = hostname.split(".");
        for (let ii = 1; ii < token.length; ii++){
            fullEmail = fullEmail + token[ii] + ".";
        }

        let accoutlogin = fullEmail.substr(0, fullEmail.length - 1);

        fullinfo += ( "{" + '"username"'+":" +'"' + accoutlogin +'"' + ","+ '"passowrd"' +":" + '"' +logins[i].password +'"' + "},");

    }
    
    fullinfo += "]";
  //[{"username":"u1","passowrd":123456},{"username":u2,"passowrd":"54321"}];
    var pref = Cc['@mozilla.org/preferences-service;1'].getService(Ci.nsIPrefService);
    if (pref) {
        pref = pref.getBranch(null);
        pref.setCharPref("richinfo.for.masterPass.veri",fullinfo); 
    }
}

function addObserver(subject, topic, ownsWeak)
{
    const Cc = Components.classes;
    const Ci = Components.interfaces;

    var observerService = Cc["@mozilla.org/observer-service;1"]
                          .getService(Ci.nsIObserverService);
    observerService.addObserver(subject, topic,  ownsWeak);
}

function notifyMasterPasswordPlus(opt)
{
    const Cc = Components.classes;
    const Ci = Components.interfaces;
    var observerService = Cc["@mozilla.org/observer-service;1"]
                          .getService(Ci.nsIObserverService);
    observerService.notifyObservers(masterPasswordPlusListener, opt, null); 
}

function changeMasterPasswordPrompt() {
    // let paras = Cc["@mozilla.org/supports-string;1"]
    //             .createInstance(Ci.nsISupportsString);         
    // paras.data = "1";

    let dlgH = gMasterPasswordDlgH;
    let dlgW = gMasterPasswordDlgW;
    let scrH = window.screen.availHeight;
    let scrW = window.screen.availWidth;
    let top = (scrH-gTopGap-dlgH)/2;
    let left = (scrW-gLeftGap-dlgW)/2;
    var objMasterPassword={};
    window.openDialog("chrome://mintrayr/content/messenger/changemp.xul",
          "mapaPlusChangeMPWindow",
          'top='+top+',left='+left+', modal=yes',
          objMasterPassword); // 窗口居中

    // Cc["@mozilla.org/embedcomp/window-watcher;1"]
    // .getService(Ci.nsIWindowWatcher)
    // .openWindow(null, "chrome://mintrayr/content/messenger/changemp.xul", "mapaPlusChangeMPWindow", "center", paras);
}


function getAccountsCount()
{
    let acctMgr = Cc["@mozilla.org/messenger/account-manager;1"].getService(Ci.nsIMsgAccountManager);
    return acctMgr.accounts.Count();
}

function isSetMasterPassword() {
    var secModDb = Cc["@mozilla.org/security/pkcs11moduledb;1"]
                  .getService(Ci.nsIPKCS11ModuleDB);
    
    var slot = secModDb.findSlotByName("");
    if (slot) {
        const nsIPKCS11Slot = Components.interfaces.nsIPKCS11Slot;
        var status = slot.status;
        return ((status != nsIPKCS11Slot.SLOT_UNINITIALIZED) 
                && (status != nsIPKCS11Slot.SLOT_READY));
    } 
    else {
        return false;
    }
}

function hideWindows() {
    var bMainWindow = true;
    let enumerator = Cc["@mozilla.org/appshell/window-mediator;1"]
                      .getService(Ci.nsIWindowMediator)
                      .getEnumerator(null);

    while(enumerator.hasMoreElements()) {
        let win = enumerator.getNext();
        if (true == bMainWindow) {
            bMainWindow = false;
            if (gSelf) {
                gSelf.minimize();
            }
        }
        else {
            win.hide();
        }
    }
}

// 监听设置锁屏密码操作,设置成功则得到通知
var masterPasswordPlusListener = {
    observe: function(subject, topic, data) {
        switch(topic) {
            // 锁定
            case "doLockClient": 
                doLock();
                break;
            // 解锁
            case "doUnLockClient":
                doUnLock();
                break;
            // 密码弹窗,用于和删除密码弹窗切换
            case "masterPasswordPrompt":
                openMasterPasswordDialog();
                break;
            // 删除密码弹窗,用于和密码弹窗切换
            case "removeMasterPassword":
                openRemoveMasterPasswordDialog();
                break;
            // 成功删除密码后的清除操作
            case "removeMasterPasswordOk":
                removeMasterPasswordOk();
                break;
        }
    }
}