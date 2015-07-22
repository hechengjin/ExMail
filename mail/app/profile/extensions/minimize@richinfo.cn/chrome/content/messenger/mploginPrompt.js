Components.utils.import("resource://gre/modules/Services.jsm");
var gPrefBranch = Components.classes["@mozilla.org/preferences-service;1"].getService(Components.interfaces.nsIPrefBranch);
const gMasterPasswordLoginDlgH = 150;
const gMasterPasswordLoginDlgW = 300;
function onLoad()
{
    addObserver(MPLoginDialogListener, "removeLoginMasterPasswordOk", false);
}

function onAccept()
{
    let acctMgr = Components.classes["@mozilla.org/messenger/account-manager;1"]
                  .getService(Components.interfaces.nsIMsgAccountManager);

    let count = acctMgr.accounts.Count();
    if (count > 0) {
        var pk11db = Components.classes["@mozilla.org/security/pk11tokendb;1"]
                    .getService(Components.interfaces.nsIPK11TokenDB);
        var _token = pk11db.getInternalKeyToken();
        var masterPassword = document.getElementById("mpTextBox").value;
        var checkOk = _token.checkPassword(masterPassword)
        if (!checkOk) {
            document.getElementById("masterPasswordErr").setAttribute("hidden", false);
        }

        _token.loginEx(true, masterPassword);
        gPrefBranch.setBoolPref("extensions.mintrayr.messenger.domplogin", true);
        Services.prefs.savePrefFile(null);
    }
}

function onCancel()
{
    const Cc = Components.classes;
    const Ci = Components.interfaces;
    gPrefBranch.setBoolPref("extensions.mintrayr.messenger.domplogin", false);
    Services.prefs.savePrefFile(null);
    try{
        Cc["@mozilla.org/toolkit/app-startup;1"].getService(Ci.nsIAppStartup).quit(Ci.nsIAppStartup.eForceQuit);
    }
    catch(e) {
        try {
            Cc["@mozilla.org/toolkit/app-startup;1"].getService(Ci.nsIAppStartup).quit(Ci.nsIAppStartup.eForceQuit);
        }
        catch(e) {
            Cc["@mozilla.org/embedcomp/prompt-service;1"]
            .getService(Ci.nsIPromptService).alert(null, "Master Password+", "You should not see this!\n\n" + e);
        }
        return;
    } 
}

function addObserver(subject, topic, ownsWeak)
{
    const Cc = Components.classes;
    const Ci = Components.interfaces;

    var observerService = Cc["@mozilla.org/observer-service;1"].getService(Ci.nsIObserverService);
    observerService.addObserver(subject, topic, ownsWeak);
}

function hideAllWindows()
{
    let enumerator = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                     .getService(Components.interfaces.nsIWindowMediator)
                     .getEnumerator(null);
    while(enumerator.hasMoreElements()) {
        let win = enumerator.getNext();
        win.hide();
    }
}

function removeMasterPassword()
{
    hideAllWindows();  // 用于锁屏登录对话框和删除锁屏密码对话框切换
    let securityPane = false;
    let mpLogin = true;
    let dlgH = gMasterPasswordLoginDlgH;
    let dlgW = gMasterPasswordLoginDlgW;
    let scrH = window.screen.availHeight;
    let scrW = window.screen.availWidth;
    let top = (scrH-30-dlgH)/2;
    let left = (scrW-10-dlgW)/2;
    window.openDialog("chrome://mozapps/content/preferences/removemp.xul", 
                      "removeMPWindow",
                      'top='+top+', left='+left+'',
                      {securityPane:securityPane,
                        mpLogin:mpLogin});
}

function login()
{
    try {
        var pk11db = Components.classes["@mozilla.org/security/pk11tokendb;1"]
                      .getService(Components.interfaces.nsIPK11TokenDB);
        var _token = pk11db.getInternalKeyToken();
        var mainPassword = Components.classes["@mozilla.org/preferences-service;1"]
                          .getService(Components.interfaces.nsIPrefService)
                          .getBranch(null).getCharPref("richinfo.masterPass");    
        _token.loginEx(true, mainPassword);
        gPrefBranch.setBoolPref("extensions.mintrayr.messenger.domplogin", true);
        Services.prefs.savePrefFile(null);
    }
    catch(e) {
        alert(e)
        window.close(); 
    }
}
/**
* @date: 2015.02.26
* @author: wangpeng
* @describe: 检测是否已经设置了锁屏密码
* @return: 设置过锁屏密码返回true，否则返回false
*/
function isNeed2TipsMasterPass()
{
    try
    {
        var pk11db = Components.classes["@mozilla.org/security/pk11tokendb;1"]
                      .getService(Components.interfaces.nsIPK11TokenDB);
        var _token = pk11db.getInternalKeyToken();
        var login = _token.needsLogin() ? true : false;

        let masterPassBSet = Components.classes["@mozilla.org/preferences-service;1"]
                .getService(Components.interfaces.nsIPrefService)
                .getBranch(null).prefHasUserValue("richinfo.masterPass"); 
         if (!masterPassBSet) {
             var prefs = Components.classes["@mozilla.org/preferences-service;1"]
                .getService(Components.interfaces.nsIPrefBranch);
            prefs.setCharPref("richinfo.masterPass","");
        }
        return login;

    }
    catch(e)
    {
    }
	
	/*
    const MASTER_PASSWORD = "richinfo.masterPass";
    var prefs = Components.classes["@mozilla.org/preferences-service;1"]
                .getService(Components.interfaces.nsIPrefBranch);

    let need2Tips = false;
    let masterPassBSet = Components.classes["@mozilla.org/preferences-service;1"]
                .getService(Components.interfaces.nsIPrefService)
                .getBranch(null).prefHasUserValue(MASTER_PASSWORD); 

    if (!masterPassBSet) {
        prefs.setCharPref(MASTER_PASSWORD,"");
    }
    else{
        var masterPass = prefs.getCharPref(MASTER_PASSWORD);
        if (masterPass != "") {
            need2Tips = true;
        }
    }

    return need2Tips;
*/
    
}

function startUp()
{
    doStartUp();
}

function doStartUp()
{
    let acctMgr = Components.classes["@mozilla.org/messenger/account-manager;1"]
                  .getService(Components.interfaces.nsIMsgAccountManager);
    let count = acctMgr.accounts.Count();
    if (count > 0) {
        if (isNeed2TipsMasterPass()) {
            let dlgH = gMasterPasswordLoginDlgH;
            let dlgW = gMasterPasswordLoginDlgW;
            let scrH = window.screen.availHeight;
            let scrW = window.screen.availWidth;
            let top = (scrH-30-dlgH)/2;
            let left = (scrW-10-dlgW)/2;
            // window.openDialog("chrome://mintrayr/content/messenger/mploginPrompt.xul", 
            //                   "masterPasswordPromptDialog",
            //                   "center, modal");
            window.openDialog("chrome://mintrayr/content/messenger/mploginPrompt.xul",
                        "masterPasswordPromptDialog",
                        'top='+top+', left='+left+', modal=yes'); 
        }
        else{
            login();
        }
    }
}

var MPLoginDialogListener = {
  observe: function(subject, topic, data) {
      switch(topic) {
          case "removeLoginMasterPasswordOk":
              login();
              window.close();
              break;
          default:
              break;
      }
  }
}