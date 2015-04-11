/* -*- Mode: JavaScript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var gGeneralPane = {
  mPane: null,
  mStartPageUrl: "",

  init: function ()
  {
    this.mPane = document.getElementById("paneGeneral");
    
    this.updatePlaySound();
  },
    
  /**
   * Restores the default start page as the user's start page
   */
  restoreDefaultStartPage: function()
  {
    var startPage = document.getElementById("mailnews.start_page.url");
    startPage.value = startPage.defaultValue;
  },
  
  /**
   * Returns a formatted url corresponding to the value of mailnews.start_page.url 
   * Stores the original value of mailnews.start_page.url 
   */
  readStartPageUrl: function()
  {
    var pref = document.getElementById("mailnews.start_page.url");
    this.mStartPageUrl = pref.value;
    var formatter = Components.classes["@mozilla.org/toolkit/URLFormatterService;1"].
                               getService(Components.interfaces.nsIURLFormatter);
    return formatter.formatURL(this.mStartPageUrl);
  },
  
  /**
   * Returns the value of the mailnews start page url represented by the UI.
   * If the url matches the formatted version of our stored value, then 
   * return the unformatted url.
   */
  writeStartPageUrl: function()
  {
    var startPage = document.getElementById('mailnewsStartPageUrl');
    var formatter = Components.classes["@mozilla.org/toolkit/URLFormatterService;1"].
                               getService(Components.interfaces.nsIURLFormatter);
    return formatter.formatURL(this.mStartPageUrl) == startPage.value ? this.mStartPageUrl : startPage.value;         
  },
  
  customizeMailAlert: function()
  {
    document.documentElement
            .openSubDialog("chrome://messenger/content/preferences/notifications.xul",
                            "", null);
  },
  
  convertURLToLocalFile: function(aFileURL)
  {
    // convert the file url into a nsILocalFile
    if (aFileURL)
    {
      var ios = Components.classes["@mozilla.org/network/io-service;1"].getService(Components.interfaces.nsIIOService);
      var fph = ios.getProtocolHandler("file").QueryInterface(Components.interfaces.nsIFileProtocolHandler);
      return fph.getFileFromURLSpec(aFileURL);
    } 
    else
      return null;
  },

  readSoundLocation: function()
  {
    var soundUrlLocation = document.getElementById("soundUrlLocation");
    soundUrlLocation.value = document.getElementById("mail.biff.play_sound.url").value;
    if (soundUrlLocation.value)
    {
      soundUrlLocation.label = this.convertURLToLocalFile(soundUrlLocation.value).leafName;
      soundUrlLocation.image = "moz-icon://" + soundUrlLocation.label + "?size=16";
    }
    return undefined;
  },
  
  previewSound: function ()
  {  
    sound = Components.classes["@mozilla.org/sound;1"].createInstance(Components.interfaces.nsISound);
    
    var soundLocation;
    soundLocation = document.getElementById('soundType').value == 1 ? 
                    document.getElementById('soundUrlLocation').value : "_moz_mailbeep"

    if (soundLocation.indexOf("file://") == -1) 
      sound.playSystemSound(soundLocation);
    else 
    {
      var ioService = Components.classes["@mozilla.org/network/io-service;1"].getService(Components.interfaces.nsIIOService);
      sound.play(ioService.newURI(soundLocation, null, null));
    }
  },
  
  browseForSoundFile: function ()
  {
    const nsIFilePicker = Components.interfaces.nsIFilePicker;
    var fp = Components.classes["@mozilla.org/filepicker;1"].createInstance(nsIFilePicker);

    // if we already have a sound file, then use the path for that sound file
    // as the initial path in the dialog.
    var localFile = this.convertURLToLocalFile(document.getElementById('soundUrlLocation').value);
    if (localFile)
      fp.displayDirectory = localFile.parent;

    // XXX todo, persist the last sound directory and pass it in
    fp.init(window, document.getElementById("bundlePreferences").getString("soundFilePickerTitle"), nsIFilePicker.modeOpen);
    fp.appendFilter("*.wav", "*.wav");
    
    // On Mac, allow AIFF files too
    if (Application.platformIsMac) {
      fp.appendFilter("*.aif", "*.aif");
      fp.appendFilter("*.aiff", "*.aiff");
    }

    var ret = fp.show();
    if (ret == nsIFilePicker.returnOK) 
    {
      // convert the nsILocalFile into a nsIFile url 
      document.getElementById("mail.biff.play_sound.url").value = fp.fileURL.spec;
      this.readSoundLocation(); // XXX We shouldn't have to be doing this by hand
      this.updatePlaySound();
    }
  },
  
  updatePlaySound: function()
  {
    // update the sound type radio buttons based on the state of the play sound checkbox
    var soundsDisabled = !document.getElementById('newMailNotification').checked;
    var soundTypeEl = document.getElementById('soundType');
    soundTypeEl.disabled = soundsDisabled;
    document.getElementById('browseForSound').disabled = soundsDisabled || soundTypeEl.value != 1;
    document.getElementById('playSound').disabled = soundsDisabled || soundTypeEl.value != 1; 
  }
};
