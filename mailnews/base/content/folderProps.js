/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource:///modules/gloda/gloda.js");

var gMsgFolder;
var gLockedPref = null;

// The folderPropsSink is the class that gets notified of an imap folder's properties

var gFolderPropsSink = {
    setFolderType: function(folderTypeString)
    {
      var typeLabel = document.getElementById("folderType.text");
      if (typeLabel)
      {
        typeLabel.setAttribute("value",folderTypeString);
      }
      // get the element for the folder type label and set value on it.
    },

    setFolderTypeDescription: function(folderDescription)
    {
      var folderTypeLabel = document.getElementById("folderDescription.text");
      if (folderTypeLabel)
        folderTypeLabel.setAttribute("value", folderDescription);
    },

    setFolderPermissions: function(folderPermissions)
    {
      var permissionsLabel = document.getElementById("folderPermissions.text");
      var descTextNode =  document.createTextNode(folderPermissions);
      permissionsLabel.appendChild(descTextNode);
    },

    serverDoesntSupportACL : function()
    {
      var typeLabel = document.getElementById("folderTypeLabel");
      if (typeLabel)
        typeLabel.setAttribute("hidden", "true");
      var permissionsLabel = document.getElementById("permissionsDescLabel");
      if (permissionsLabel)
        permissionsLabel.setAttribute("hidden", "true");

    },

    setQuotaStatus : function(folderQuotaStatus)
    {
      var quotaStatusLabel = document.getElementById("folderQuotaStatus");
      if(quotaStatusLabel)
        quotaStatusLabel.setAttribute("value", folderQuotaStatus);
    },

    showQuotaData : function(showData)
    {
      var quotaStatusLabel = document.getElementById("folderQuotaStatus");
      var folderQuotaData = document.getElementById("folderQuotaData");

      if(quotaStatusLabel && folderQuotaData)
      {
        quotaStatusLabel.hidden = showData;
        folderQuotaData.hidden = ! showData;
      }
    },

    setQuotaData : function(root, usedKB, maxKB)
    {
      var quotaRoot = document.getElementById("quotaRoot");
      if (quotaRoot)
        quotaRoot.setAttribute("value", '"' + root + '"');

      var percentage = (maxKB != 0) ? Math.round(usedKB / maxKB * 100) : 0;

      var quotaPercentageBar = document.getElementById("quotaPercentageBar");
      if (quotaPercentageBar)
        quotaPercentageBar.setAttribute("value", percentage);

      var bundle = document.getElementById("bundle_messenger");
      if(bundle)
      {
        var usedFreeCaption = bundle.getFormattedString("quotaUsedFree", [usedKB, maxKB], 2);
        var quotaCaption = document.getElementById("quotaUsedFree");
        if(quotaCaption)
          quotaCaption.setAttribute("value", usedFreeCaption);

        var percentUsedCaption = bundle.getFormattedString("quotaPercentUsed", [percentage], 1);
        var percentUsed = document.getElementById("quotaPercentUsed");
        if(percentUsed)
          percentUsed.setAttribute("value", percentUsedCaption);
      }
    }

};

function doEnabling()
{
  var nameTextbox = document.getElementById("name");
  document.documentElement.getButton("accept").disabled = !nameTextbox.value;
}

function folderPropsOKButton()
{
  if (gMsgFolder)
  {
    const nsMsgFolderFlags = Components.interfaces.nsMsgFolderFlags;
    // set charset attributes
    var folderCharsetList = document.getElementById("folderCharsetList");
    gMsgFolder.charset = folderCharsetList.getAttribute("value");
    gMsgFolder.charsetOverride = document.getElementById("folderCharsetOverride").checked;

    if(document.getElementById("offline.selectForOfflineFolder").checked ||
      document.getElementById("offline.selectForOfflineNewsgroup").checked)
      gMsgFolder.setFlag(nsMsgFolderFlags.Offline);
    else
      gMsgFolder.clearFlag(nsMsgFolderFlags.Offline);

    if(document.getElementById("folderCheckForNewMessages").checked)
      gMsgFolder.setFlag(nsMsgFolderFlags.CheckNew);
    else
      gMsgFolder.clearFlag(nsMsgFolderFlags.CheckNew);

    let glodaCheckbox = document.getElementById("folderIncludeInGlobalSearch");
    if (!glodaCheckbox.hidden) {
      if(glodaCheckbox.checked) {
        // We pass true here so that folders such as trash and junk can still
        // have a priority set.
        Gloda.resetFolderIndexingPriority(gMsgFolder, true);
      } else {
        Gloda.setFolderIndexingPriority(gMsgFolder,
          Gloda.getFolderForFolder(gMsgFolder).kIndexingNeverPriority);
      }
    }

    var retentionSettings = saveCommonRetentionSettings(gMsgFolder.retentionSettings);
    retentionSettings.useServerDefaults = document.getElementById("retention.useDefault").checked;
    gMsgFolder.retentionSettings = retentionSettings;

  }

  try
  {
    // This throws an exception when an illegal folder name was entered.
    okCallback(document.getElementById("name").value, window.arguments[0].name,
               gMsgFolder.URI);

    return true;
  }
  catch (e)
  {
    return false;
  }
}

function folderPropsOnLoad()
{
  // look in arguments[0] for parameters
  if (window.arguments && window.arguments[0]) {
    if ( window.arguments[0].title ) {
      document.title = window.arguments[0].title;
    }
    if ( window.arguments[0].okCallback ) {
      top.okCallback = window.arguments[0].okCallback;
    }
  }

  // fill in folder name, based on what they selected in the folder pane
  if (window.arguments[0].folder) {
    gMsgFolder = window.arguments[0].folder;
  } else {
    dump("passed null for folder, do nothing\n");
  }

  if(window.arguments[0].name)
  {
    // Initialize name textbox with the given name and remember this
    // value so we can tell whether the folder needs to be renamed
    // when the dialog is accepted.
    var nameTextbox = document.getElementById("name");
    nameTextbox.value = window.arguments[0].name;

//  name.setSelectionRange(0,-1);
//  name.focusTextField();
  }

  const nsMsgFolderFlags = Components.interfaces.nsMsgFolderFlags;
  const serverType = window.arguments[0].serverType;

  // Do this first, because of gloda we may want to override some of the hidden
  // statuses.
  hideShowControls(serverType);

  if (gMsgFolder) {
    // We really need a functioning database, so we'll detect problems
    // and create one if we have to.
    try {
      var db = gMsgFolder.getDatabase(null);
    }
    catch (e) {
      gMsgFolder.updateFolder(window.arguments[0].msgWindow);
    }

    var locationTextbox = document.getElementById("location");

    // Decode the displayed mailbox:// URL as it's useful primarily for debugging,
    // whereas imap and news urls are sent around.
    locationTextbox.value = (serverType == "imap" || serverType == "nntp") ?
        gMsgFolder.folderURL : decodeURI(gMsgFolder.folderURL);

    if (gMsgFolder.canRename)
      document.getElementById("name").removeAttribute("readonly");

    if (gMsgFolder.flags & nsMsgFolderFlags.Offline) {

      if(serverType == "imap" || serverType == "pop3")
        document.getElementById("offline.selectForOfflineFolder").checked = true;

      if(serverType == "nntp")
        document.getElementById("offline.selectForOfflineNewsgroup").checked = true;
    }
    else {
      if(serverType == "imap" || serverType == "pop3")
        document.getElementById("offline.selectForOfflineFolder").checked = false;

      if(serverType == "nntp")
        document.getElementById("offline.selectForOfflineNewsgroup").checked = false;
    }

    // select the menu item
    var folderCharsetList = document.getElementById("folderCharsetList");
    var elements = folderCharsetList.getElementsByAttribute("value", gMsgFolder.charset);
    folderCharsetList.selectedItem = elements[0];

    // set override checkbox
    document.getElementById("folderCharsetOverride").checked = gMsgFolder.charsetOverride;

    // set check for new mail checkbox
    document.getElementById("folderCheckForNewMessages").checked = gMsgFolder.flags & nsMsgFolderFlags.CheckNew;

    // if gloda indexing is off, hide the related checkbox
    var glodaCheckbox = document.getElementById("folderIncludeInGlobalSearch");
    var glodaEnabled = Services.prefs
      .getBoolPref("mailnews.database.global.indexer.enabled");
    if (!glodaEnabled || (gMsgFolder.flags & (nsMsgFolderFlags.Queue |
                                              nsMsgFolderFlags.Newsgroup))) {
      glodaCheckbox.hidden = true;
    } else {
      // otherwise, the user can choose whether this file gets indexed
      let glodaFolder = Gloda.getFolderForFolder(gMsgFolder);
      glodaCheckbox.checked =
        glodaFolder.indexingPriority != glodaFolder.kIndexingNeverPriority;
    }
  }

  if (serverType == "imap")
  {
    var imapFolder = gMsgFolder.QueryInterface(Components.interfaces.nsIMsgImapMailFolder);
    if (imapFolder)
      imapFolder.fillInFolderProps(gFolderPropsSink);
  }

  var retentionSettings = gMsgFolder.retentionSettings;
  initCommonRetentionSettings(retentionSettings);
  document.getElementById("retention.useDefault").checked = retentionSettings.useServerDefaults;

  // set folder sizes
  let numberOfMsgs = gMsgFolder.getTotalMessages(false);
  if (numberOfMsgs >= 0)
    document.getElementById("numberOfMessages").value = numberOfMsgs;

  try {
    let sizeOnDisk = Components.classes["@mozilla.org/messenger;1"]
                               .createInstance(Components.interfaces.nsIMessenger)
                               .formatFileSize(gMsgFolder.sizeOnDisk, true);
    document.getElementById("sizeOnDisk").value = sizeOnDisk;
  } catch (e) { }

  // select the initial tab
  if (window.arguments[0].tabID) {
    try {
      document.getElementById("folderPropTabBox").selectedTab =
        document.getElementById(window.arguments[0].tabID);
    }
    catch (ex) {}
  }
  onCheckKeepMsg();
  onUseDefaultRetentionSettings();
}

function hideShowControls(serverType)
{
  var controls = document.getElementsByAttribute("hidefor", "*");
  var len = controls.length;
  for (var i=0; i<len; i++) {
    var control = controls[i];
    var hideFor = control.getAttribute("hidefor");
    if (!hideFor)
      throw "hidefor empty";

    // hide unsupported server type
    // adding support for hiding multiple server types using hideFor="server1,server2"
    var hideForBool = false;
    var hideForTokens = hideFor.split(",");
    for (var j = 0; j < hideForTokens.length; j++) {
      if (hideForTokens[j] == serverType) {
        hideForBool = true;
        break;
      }
    }
    control.hidden = hideForBool;
  }

  // hide the priviliges button if the imap folder doesn't have an admin url
  // mabye should leave this hidden by default and only show it in this case instead
  try {
    var imapFolder = gMsgFolder.QueryInterface(Components.interfaces.nsIMsgImapMailFolder);
    if (imapFolder)
    {
      var privilegesButton = document.getElementById("imap.FolderPrivileges");
      if (privilegesButton)
      {
        if (!imapFolder.hasAdminUrl)
          privilegesButton.setAttribute("hidden", "true");
      }
    }
  }
  catch (ex) {}

  if (gMsgFolder)
  {
    const nsMsgFolderFlags = Components.interfaces.nsMsgFolderFlags;
    // Hide "check for new mail" checkbox if this is an Inbox.
    if (gMsgFolder.flags & nsMsgFolderFlags.Inbox)
      document.getElementById("folderCheckForNewMessages").hidden = true;
    // Retention policy doesn't apply to Drafts/Templates/Outbox.
    if (gMsgFolder.isSpecialFolder(nsMsgFolderFlags.Drafts |
                                   nsMsgFolderFlags.Templates |
                                   nsMsgFolderFlags.Queue, true))
      document.getElementById("Retention").hidden = true;
  }
}

function onOfflineFolderDownload()
{
  // we need to create a progress window and pass that in as the second parameter here.
  gMsgFolder.downloadAllForOffline(null, window.arguments[0].msgWindow);
}

function onFolderPrivileges()
{
  var imapFolder = gMsgFolder.QueryInterface(Components.interfaces.nsIMsgImapMailFolder);
  if (imapFolder)
    imapFolder.folderPrivileges(window.arguments[0].msgWindow);
  // let's try closing the modal dialog to see if it fixes the various problems running this url
  window.close();
}


function onUseDefaultRetentionSettings()
{
  var useDefault = document.getElementById("retention.useDefault").checked;
  document.getElementById('retention.keepMsg').disabled = useDefault;
  document.getElementById('retention.keepNewMsgMinLabel').disabled = useDefault;
  document.getElementById('retention.keepOldMsgMinLabel').disabled = useDefault;

  var keepMsg = document.getElementById("retention.keepMsg").value;
  const nsIMsgRetentionSettings = Components.interfaces.nsIMsgRetentionSettings;
  document.getElementById('retention.keepOldMsgMin').disabled =
    useDefault || (keepMsg != nsIMsgRetentionSettings.nsMsgRetainByAge);
  document.getElementById('retention.keepNewMsgMin').disabled =
    useDefault || (keepMsg != nsIMsgRetentionSettings.nsMsgRetainByNumHeaders);
}

function RebuildSummaryInformation()
{
  window.arguments[0].rebuildSummaryCallback(gMsgFolder);
}
