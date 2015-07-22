// -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
Components.utils.import("resource://gre/modules/Services.jsm");
const nsPK11TokenDB = "@mozilla.org/security/pk11tokendb;1";
const nsIPK11TokenDB = Components.interfaces.nsIPK11TokenDB;
const nsIDialogParamBlock = Components.interfaces.nsIDialogParamBlock;
const nsPKCS11ModuleDB = "@mozilla.org/security/pkcs11moduledb;1";
const nsIPKCS11ModuleDB = Components.interfaces.nsIPKCS11ModuleDB;
const nsIPKCS11Slot = Components.interfaces.nsIPKCS11Slot;
const nsIPK11Token = Components.interfaces.nsIPK11Token;
const Cc = Components.classes;
const Ci = Components.interfaces;

var firstEnterPassword=true;
var firstClickEnterPassword=true;

var params;
var tokenName="";
var pw1;
var gErrMsgBox;
var x;
function init()
{
  var objMasterPassword = window.arguments[0];
  if (objMasterPassword.securityCall) {
    objMasterPassword.securityCall = false;

    var isMasterPasswordSet = false;
    var secModDb = Cc["@mozilla.org/security/pkcs11moduledb;1"]
                  .getService(Ci.nsIPKCS11ModuleDB);
    
    var slot = secModDb.findSlotByName("");
    if (slot) {
        const nsIPKCS11Slot = Components.interfaces.nsIPKCS11Slot;
        var status = slot.status;
        isMasterPasswordSet = ((status != nsIPKCS11Slot.SLOT_UNINITIALIZED) 
                               && (status != nsIPKCS11Slot.SLOT_READY));
        if (isMasterPasswordSet) {
          var bundle = document.getElementById("bundlePreferences");
          var title = bundle.getString("masterpassword_change");
          document.documentElement.setAttribute("title", title);
        }
    } 
  }

  pw1 = document.getElementById("pw1");
  gErrMsgBox = document.getElementById("masterPasswordError");
  process();
}


function process()
{
   var secmoddb = Components.classes[nsPKCS11ModuleDB].getService(nsIPKCS11ModuleDB);
   var bundle = document.getElementById("bundlePreferences");

   // If the token is unitialized, don't use the old password box.
   // Otherwise, do.

   var slot = secmoddb.findSlotByName(tokenName);
   if (slot) {
     var oldpwbox = document.getElementById("oldpw");
     var status = slot.status;
     if (status == nsIPKCS11Slot.SLOT_UNINITIALIZED
         || status == nsIPKCS11Slot.SLOT_READY) {
       //oldpwbox.setAttribute("hidden", "true");
         document.getElementById("oldPasswordRow").setAttribute("hidden", "true");

       if (status == nsIPKCS11Slot.SLOT_READY) {
         oldpwbox.setAttribute("inited", "empty");
       } else {
         oldpwbox.setAttribute("inited", "true");
       }
    
     } else {
       // Select old password field
       //oldpwbox.setAttribute("hidden", "false");
         document.getElementById("oldPasswordRow").removeAttribute("hidden");

       oldpwbox.setAttribute("inited", "false");
       oldpwbox.focus();
       // Show old masterpassword label
       var oldPassword = document.getElementById("oldPassword");
       oldPassword.setAttribute("hidden", false);
       oldPassword.setAttribute("collapsed", false);
     }
   }

  if (params) {
    // Return value 0 means "canceled"
    params.SetInt(1, 0);
  }
  
  checkPasswords();
}

function setPassword()
{
  var pk11db = Components.classes[nsPK11TokenDB].getService(nsIPK11TokenDB);
  var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                                .getService(Components.interfaces.nsIPromptService);
  var token = pk11db.findTokenByName(tokenName);
  dump("*** TOKEN!!!! (name = |" + token + "|\n");

  var oldpwbox = document.getElementById("oldpw");
  var initpw = oldpwbox.getAttribute("inited");
  var bundle = document.getElementById("bundlePreferences");
  
  var success = false;
  
  if (initpw == "false" || initpw == "empty") {
    try {
      var oldpw = "";
      var passok = 0;
      
      if (initpw == "empty") {
        passok = 1;
      } else {
        oldpw = oldpwbox.value;
        passok = token.checkPassword(oldpw);
      }
      
      if (passok) {
        if (initpw == "empty" && pw1.value == "") {
          // This makes no sense that we arrive here, 
          // we reached a case that should have been prevented by checkPasswords.
        } else {
              if (pw1.value == "") {
                  var secmoddb = Components.classes[nsPKCS11ModuleDB].getService(nsIPKCS11ModuleDB);
                  if (secmoddb.isFIPSEnabled) {
                    // empty passwords are not allowed in FIPS mode
                    /*promptService.alert(window,
                                        bundle.getString("pw_change_failed_title"),
                                        bundle.getString("pw_change2empty_in_fips_mode"));*/
                    passok = 0;
                  }
              }

              if (passok) {
                  if (pw1.value == "") {
                      // the password changed can not be null
                      pw1.focus();
                      gErrMsgBox.value = bundle.getString("incorrect_pw_empty");
                      gErrMsgBox.removeAttribute("hidden");
                      return false;
                      /*promptService.alert(window,
                                          bundle.getString("pw_change_success_title"),
                                          bundle.getString("pw_erased_ok") 
                                          + " " + bundle.getString("pw_empty_warning"));*/
                  } else {
                      success = true;
                      token.changePassword(oldpw, pw1.value);
                      /*promptService.alert(window,
                                          bundle.getString("pw_change_success_title"),
                                          bundle.getString("pw_change_ok"));*/
                  }
              }
        }
      } else {
        oldpwbox.focus();
        oldpwbox.setAttribute("value", "");
        gErrMsgBox.value = bundle.getString("incorrect_pw");
          gErrMsgBox.removeAttribute("hidden");
        return false;
        /*promptService.alert(window,
                            bundle.getString("pw_change_failed_title"),
                            bundle.getString("incorrect_pw"));*/
      }
    } catch (e) {
          gErrMsgBox.value = bundle.getString("failed_pw_change");
        gErrMsgBox.removeAttribute("hidden");
          return false;
          /*promptService.alert(window,
                              bundle.getString("pw_change_failed_title"),
                              bundle.getString("failed_pw_change"));*/
    }
  } else {
    success = true;
    token.initPassword(pw1.value);
    if (pw1.value == "") {
      /*promptService.alert(window,
                          bundle.getString("pw_change_success_title"),
                          bundle.getString("pw_not_wanted")
                          + " " + bundle.getString("pw_empty_warning"));*/
    }
  }

  // Terminate dialog
  if (success) {
      var pref = Components.classes['@mozilla.org/preferences-service;1'].getService(Components.interfaces.nsIPrefService);
      if (pref) {
          pref = pref.getBranch(null);
          pref.setCharPref("richinfo.masterPass",pw1.value);
          pref.setBoolPref("richinfo.masterPass.modify", true); 
          Services.prefs.savePrefFile(null);
      }
      
      if (pw1.value != "") {
          var objMasterPassword = window.arguments[0];
          if (objMasterPassword) {
              objMasterPassword.setOk = true;
          }
          
          var observerService = Components.classes["@mozilla.org/observer-service;1"]
                                .getService(Components.interfaces.nsIObserverService);
          observerService.notifyObservers(null, "doLockClient", null); 
      }

      window.close();
  }
}

function _clearPw1PasswordTips()
{
    var enterPasswordBox = document.getElementById("pw1");
    enterPasswordBox.value="";
    enterPasswordBox.focus();
    enterPasswordBox.type = "password";
}

function pw1ClearEnterPasswordTips()
{
    if (firstEnterPassword) {
        firstEnterPassword=false;
        _clearPw1PasswordTips();
    }
}

function pw1KeyDown()
{
    if (firstEnterPassword) {
        firstEnterPassword=false;
        _clearPw1PasswordTips();
    }
    
    const limit = 19;
    var len = document.getElementById("pw1").value.length;
    if (len >= limit) {  // max len = 20
        document.getElementById("pw1").value = document.getElementById("pw1").value.substring(0,limit);
    }
}

function setPasswordStrength()
{//alert("setPasswordStrength");
// Here is how we weigh the quality of the password
// number of characters
// numbers
// non-alpha-numeric chars
// upper and lower case characters

  var pw=document.getElementById('pw1').value;

//length of the password
  var pwlength=(pw.length);
  if (pwlength>5)
    pwlength=5;


//use of numbers in the password
  var numnumeric = pw.replace (/[0-9]/g, "");
  var numeric=(pw.length - numnumeric.length);
  if (numeric>3)
    numeric=3;

//use of symbols in the password
  var symbols = pw.replace (/\W/g, "");
  var numsymbols=(pw.length - symbols.length);
  if (numsymbols>3)
    numsymbols=3;

//use of uppercase in the password
  var numupper = pw.replace (/[A-Z]/g, "");
  var upper=(pw.length - numupper.length);
  if (upper>3)
    upper=3;


  var pwstrength=((pwlength*10)-20) + (numeric*10) + (numsymbols*15) + (upper*10);

  // make sure we're give a value between 0 and 100
  if ( pwstrength < 0 ) {
    pwstrength = 0;
  }
  
  if ( pwstrength > 100 ) {
    pwstrength = 100;
  }

  var mymeter=document.getElementById('pwmeter');
  mymeter.value = pwstrength;

  return;
}

function checkPasswords()
{//alert("checkPasswords");
  var pw1=document.getElementById('pw1').value;
  var pw2=document.getElementById('pw2').value;
  var ok=document.documentElement.getButton("accept");

  var oldpwbox = document.getElementById("oldpw");
  if (oldpwbox) {
    var initpw = oldpwbox.getAttribute("inited");

    if (initpw == "empty" && pw1 == "") {
      // The token has already been initialized, therefore this dialog
      // was called with the intention to change the password.
      // The token currently uses an empty password.
      // We will not allow changing the password from empty to empty.
      ok.setAttribute("disabled","true");
      return;
    }
  }

  if (pw1 == pw2){
    ok.setAttribute("disabled","false");
  } else
  {
    ok.setAttribute("disabled","true");
  }

}
