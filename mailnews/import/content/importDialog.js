/* -*- Mode: Javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var importType = null;
var gImportMsgsBundle;
var gFeedsBundle;
var importService = 0;
var successStr = null;
var errorStr = null;
var inputStr = null ;
var progressInfo = null;
var selectedModuleName = null;
var addInterface = null ;
var newFeedAcctCreated = false;

const nsISupportsString = Components.interfaces.nsISupportsString;

function OnLoadImportDialog()
{
  gImportMsgsBundle = document.getElementById("bundle_importMsgs");
  gFeedsBundle = document.getElementById("bundle_feeds");
  importService = Components.classes["@mozilla.org/import/import-service;1"]
                            .getService(Components.interfaces.nsIImportService);

  progressInfo = { };
  progressInfo.progressWindow = null;
  progressInfo.importInterface = null;
  progressInfo.mainWindow = window;
  progressInfo.intervalState = 0;
  progressInfo.importSuccess = false;
  progressInfo.importType = null;
  progressInfo.localFolderExists = false;

  // look in arguments[0] for parameters
  if (window.arguments && window.arguments.length >= 1 &&
            "importType" in window.arguments[0] && window.arguments[0].importType)
  {
    // keep parameters in global for later
    importType = window.arguments[0].importType;
    progressInfo.importType = top.importType;
  }
  else
  {
    importType = "all";
    progressInfo.importType = "all";
  }

  SetUpImportType();

  // on startup, set the focus to the control element
  // for accessibility reasons.
  // if we used the wizardOverlay, we would get this for free.
  // see bug #101874
  document.getElementById("importFields").focus();
}


function SetUpImportType()
{
  // set dialog title
  document.getElementById("importFields").value = importType;
  
  // Mac migration not working right now, so disable it
  if (navigator.platform.match("^Mac"))
  {
    document.getElementById("allRadio").setAttribute("disabled", "true");
    if (importType == "all")
      document.getElementById("importFields").value = "addressbook";
  }

  let descriptionDeck = document.getElementById("selectDescriptionDeck");
  descriptionDeck.setAttribute("selectedIndex", "0");
  if (importType == "feeds")
  {
    descriptionDeck.setAttribute("selectedIndex", "1");
    ListFeedAccounts();
  }
  else
    ListModules();
}


function SetDivText(id, text)
{
  var div = document.getElementById(id);

  if (div) {
    if (!div.hasChildNodes()) {
      var textNode = document.createTextNode(text);
      div.appendChild(textNode);
    }
    else if ( div.childNodes.length == 1 ) {
      div.childNodes[0].nodeValue = text;
    }
  }
}

function CheckIfLocalFolderExists()
{
  var acctMgr = Components.classes["@mozilla.org/messenger/account-manager;1"].getService(Components.interfaces.nsIMsgAccountManager);
  if (acctMgr) {
    try {
      if (acctMgr.localFoldersServer)
        progressInfo.localFolderExists = true; 
    }
    catch (ex) {
      progressInfo.localFolderExists = false;
    }
  }
}

function ImportDialogOKButton()
{
  var listbox = document.getElementById('moduleList');
  var deck = document.getElementById("stateDeck");
  var header = document.getElementById("header");
  var progressMeterEl = document.getElementById("progressMeter");
  progressMeterEl.setAttribute("mode", "determined");
  var progressStatusEl = document.getElementById("progressStatus");
  var progressTitleEl = document.getElementById("progressTitle");

  // better not mess around with navigation at this point
  var nextButton = document.getElementById("forward");
  nextButton.setAttribute("disabled", "true");
  var backButton = document.getElementById("back");
  backButton.setAttribute("disabled", "true");

  if ( listbox && listbox.selectedItems && (listbox.selectedItems.length == 1) )
  {
    importType = document.getElementById("importFields").value;
    var index = listbox.selectedItems[0].getAttribute('list-index');
    if (importType == "feeds")
      var module = "Feeds";
    else
    {
      var module = importService.GetModule(importType, index);
      var name = importService.GetModuleName(importType, index);
    }
    selectedModuleName = name;
    if (module)
    {
      // Fix for Bug 57839 & 85219
      // We use localFoldersServer(in nsIMsgAccountManager) to check if Local Folder exists.
      // We need to check localFoldersServer before importing "mail", "settings", or "filters".
      // Reason: We will create an account with an incoming server of type "none" after 
      // importing "mail", so the localFoldersServer is valid even though the Local Folder 
      // is not created.
      if (importType == "mail" || importType == "settings" || importType == "filters")
        CheckIfLocalFolderExists();

      var meterText = "";
      switch(importType)
      {
        case "mail":
          top.successStr = Components.classes["@mozilla.org/supports-string;1"]
                                     .createInstance(nsISupportsString);
          top.errorStr = Components.classes["@mozilla.org/supports-string;1"]
                                   .createInstance(nsISupportsString);

          if (ImportMail( module, top.successStr, top.errorStr) == true)
          {
            // We think it was a success, either, we need to
            // wait for the import to finish
            // or we are done!
            if (top.progressInfo.importInterface == null) {
              ShowImportResults(true, 'Mail');
              return( true);
            }
            else {
              meterText = gImportMsgsBundle.getFormattedString('MailProgressMeterText',
                                                               [ name ]);
              header.setAttribute("description", meterText);

              progressStatusEl.setAttribute("label", "");
              progressTitleEl.setAttribute("label", meterText);

              deck.setAttribute("selectedIndex", "2");
              progressInfo.progressWindow = top.window;
              progressInfo.intervalState = setInterval("ContinueImportCallback()", 100);
              return( true);
            }
          }
          else
          {
            ShowImportResults(false, 'Mail');
            // enable back and next buttons so that users can retry or pick other import options.
            nextButton.removeAttribute("disabled");
            backButton.removeAttribute("disabled");
            return( false);
          }
          break;

        case "feeds":
          if (ImportFeeds())
          {
            // Successful completion of pre processing and launch of async import.
            meterText = document.getElementById("description").textContent;
            header.setAttribute("description", meterText);

            progressStatusEl.setAttribute("label", "");
            progressTitleEl.setAttribute("label", meterText);
            progressMeterEl.setAttribute("mode", "undetermined");

            deck.setAttribute("selectedIndex", "2");
            return true;
          }
          else
          {
            // Cancelled or error.
            nextButton.removeAttribute("disabled");
            backButton.removeAttribute("disabled");
            return false;
          }
          break;

        case "addressbook":
          top.successStr = Components.classes["@mozilla.org/supports-string;1"]
                                     .createInstance(nsISupportsString);
          top.errorStr = Components.classes["@mozilla.org/supports-string;1"]
                                   .createInstance(nsISupportsString);
          top.inputStr = Components.classes["@mozilla.org/supports-string;1"]
                                   .createInstance(nsISupportsString);

          if (ImportAddress( module, top.successStr, top.errorStr) == true) {
            // We think it was a success, either, we need to
            // wait for the import to finish
            // or we are done!
            if (top.progressInfo.importInterface == null) {
              ShowImportResults(true, 'Address');
              return( true);
            }
            else {
              meterText = gImportMsgsBundle.getFormattedString('AddrProgressMeterText',
                                                               [ name ]);
              header.setAttribute("description", meterText);

              progressStatusEl.setAttribute("label", "");
              progressTitleEl.setAttribute("label", meterText);

              deck.setAttribute("selectedIndex", "2");
              progressInfo.progressWindow = top.window;
              progressInfo.intervalState = setInterval("ContinueImportCallback()", 100);

              return( true);
            }
          }
          else
          {
            ShowImportResults(false, 'Address');
            // re-enable the next button, as we are here
            // because the user cancelled when picking an addressbook file to import.
            // enable next, so they can try again
            nextButton.removeAttribute("disabled");
            // also enable back button so that users can pick other import options.
            backButton.removeAttribute("disabled");
            return( false);
          }
          break;

        case "settings":
          var error = new Object();
          error.value = null;
          var newAccount = new Object();
          if (!ImportSettings( module, newAccount, error))
          {
            if (error.value)
              ShowImportResultsRaw(gImportMsgsBundle.getString('ImportSettingsFailed'), null, false);
            // the user canceled the operation, shoud we dismiss
            // this dialog or not?
            return false;
          }
          else
            ShowImportResultsRaw(gImportMsgsBundle.getFormattedString('ImportSettingsSuccess', [ name ]), null, true);
          break;

        case "filters":
          var error = new Object();
          error.value = null;
          if (!ImportFilters( module, error))
          {
            if (error.value)
              ShowImportResultsRaw(gImportMsgsBundle.getFormattedString('ImportFiltersFailed', [ name ]), error.value, false);
            // the user canceled the operation, shoud we dismiss
            // this dialog or not?
            return false;
          }
          else
          {
            if (error.value)
              ShowImportResultsRaw(gImportMsgsBundle.getFormattedString('ImportFiltersPartial', [ name ]), error.value, true);
            else
              ShowImportResultsRaw(gImportMsgsBundle.getFormattedString('ImportFiltersSuccess', [ name ]), null, true);
          }
          break;
      }
    }
  }

  return true;
}

function SetStatusText( val)
{
  var progressStatus = document.getElementById("progressStatus");
  progressStatus.setAttribute( "label", val);
}

function SetProgress( val)
{
  var progressMeter = document.getElementById("progressMeter");
  progressMeter.setAttribute( "value", val);
}

function ContinueImportCallback()
{
  progressInfo.mainWindow.ContinueImport( top.progressInfo);
}

function ImportSelectionChanged()
{
  let listbox = document.getElementById('moduleList');
  let acctNameBox = document.getElementById('acctName-box');
  if ( listbox && listbox.selectedItems && (listbox.selectedItems.length == 1) )
  {
    let index = listbox.selectedItems[0].getAttribute('list-index');
    acctNameBox.setAttribute('style', 'visibility: hidden;');
    if (importType == 'feeds')
    {
      if (index == 0)
      {
        SetDivText('description', gFeedsBundle.getString('ImportFeedsNewAccount'));
        let defaultName = gFeedsBundle.getString("feeds-accountname");
        document.getElementById( "acctName").value = defaultName;
        acctNameBox.removeAttribute('style');
      }
      else
        SetDivText('description', gFeedsBundle.getString('ImportFeedsExistingAccount'));
    }
    else
      SetDivText('description', top.importService.GetModuleDescription(top.importType, index));
  }
}

function CompareImportModuleName(a, b)
{
  if (a.name > b.name)
    return 1;
  if (a.name < b.name)
    return -1;
  return 0;
}

function ListModules() {
  if (top.importService == null)
    return;

  var body = document.getElementById( "moduleList");
  while (body.hasChildNodes()) {
    body.removeChild(body.lastChild);
  }

  var count = top.importService.GetModuleCount(top.importType);
  var i;

  var moduleArray = new Array(count);
  for (i = 0; i < count; i++) {
    moduleArray[i] = {name:top.importService.GetModuleName(top.importType, i), index:i };
  }

  // sort the array of modules by name, so that they'll show up in the right order
  moduleArray.sort(CompareImportModuleName);

  for (i = 0; i < count; i++) {
    AddModuleToList(moduleArray[i].name, moduleArray[i].index);
  }
}

function AddModuleToList(moduleName, index)
{
  var body = document.getElementById("moduleList");

  var item = document.createElement('listitem');
  item.setAttribute('label', moduleName);
  item.setAttribute('list-index', index);

  body.appendChild(item);
}

function ListFeedAccounts() {
  let body = document.getElementById( "moduleList");
  while (body.hasChildNodes())
    body.removeChild(body.lastChild);

  // Add item to allow for new account creation.
  let item = document.createElement("listitem");
  item.setAttribute("label", gFeedsBundle.getString('ImportFeedsCreateNewListItem'));
  item.setAttribute("list-index", 0);
  body.appendChild(item);

  let index = 0;
  let feedRootFolders = FeedUtils.getAllRssServerRootFolders();

  feedRootFolders.forEach(function(rootFolder) {
    item = document.createElement("listitem");
    item.setAttribute("label", rootFolder.prettyName);
    item.setAttribute("list-index", ++index);
    item.server = rootFolder.server;
    body.appendChild(item);
  }, this);

  if (index)
    // If there is an existing feed account, select the first one.
    body.selectedIndex = 1;
}

function ContinueImport( info) {
  var isMail = info.importType == 'mail' ? true : false;
  var clear = true;
  var deck;
  var pcnt;

  if (info.importInterface) {
    if (!info.importInterface.ContinueImport()) {
      info.importSuccess = false;
      clearInterval( info.intervalState);
      if (info.progressWindow != null) {
        deck = document.getElementById("stateDeck");
        deck.setAttribute("selectedIndex", "3");
        info.progressWindow = null;
      }

      ShowImportResults(false, isMail ? 'Mail' : 'Address');
    }
    else if ((pcnt = info.importInterface.GetProgress()) < 100) {
      clear = false;
      if (info.progressWindow != null) {
        if (pcnt < 5)
          pcnt = 5;
        SetProgress( pcnt);
        if (isMail) {
          var mailName = info.importInterface.GetData( "currentMailbox");
          if (mailName) {
            mailName = mailName.QueryInterface( Components.interfaces.nsISupportsString);
            if (mailName)
              SetStatusText( mailName.data);
          }
        }
      }
    }
    else {
      dump("*** WARNING! sometimes this shows results too early. \n");
      dump("    something screwy here. this used to work fine.\n");
      clearInterval( info.intervalState);
      info.importSuccess = true;
      if (info.progressWindow) {
        deck = document.getElementById("stateDeck");
        deck.setAttribute("selectedIndex", "3");
        info.progressWindow = null;
      }

      ShowImportResults(true, isMail ? 'Mail' : 'Address');
    }
  }
  if (clear) {
    info.intervalState = null;
    info.importInterface = null;
  }
}


function ShowResults(doesWantProgress, result)
{
       if (result)
       {
               if (doesWantProgress)
               {
                       var deck = document.getElementById("stateDeck");
                       var header = document.getElementById("header");
                       var progressStatusEl = document.getElementById("progressStatus");
                       var progressTitleEl = document.getElementById("progressTitle");

                       var meterText = gImportMsgsBundle.getFormattedString('AddrProgressMeterText', [ name ]);
                       header.setAttribute("description", meterText);

                       progressStatusEl.setAttribute("label", "");
                       progressTitleEl.setAttribute("label", meterText);

                       deck.setAttribute("selectedIndex", "2");
                       progressInfo.progressWindow = top.window;
                       progressInfo.intervalState = setInterval("ContinueImportCallback()", 100);
               }
               else
               {
                       ShowImportResults(true, 'Address');
               }
       }
       else
       {
        ShowImportResults(false, 'Address');
       }

       return true ;
}


function ShowImportResults(good, module)
{
  // String keys for ImportSettingsSuccess, ImportSettingsFailed,
  // ImportMailSuccess, ImportMailFailed, ImportAddressSuccess,
  // ImportAddressFailed, ImportFiltersSuccess, and ImportFiltersFailed.
  var modSuccess = 'Import' + module + 'Success';
  var modFailed = 'Import' + module + 'Failed';

  // The callers seem to set 'good' to true even if there's something
  // in the error log. So we should only make it a success case if
  // error log/str is empty.
  var results, title;
  if (good && !errorStr.data) {
    title = gImportMsgsBundle.getFormattedString(modSuccess, [ selectedModuleName ? selectedModuleName : '' ]);
    results = successStr.data;
  }
  else if (errorStr.data) {
    title = gImportMsgsBundle.getFormattedString(modFailed, [ selectedModuleName ? selectedModuleName : '' ]);
    results = errorStr.data;
  }

  if (results && title)
    ShowImportResultsRaw(title, results, good);
}

function ShowImportResultsRaw(title, results, good)
{
  SetDivText("status", title);
  var header = document.getElementById("header");
  header.setAttribute("description", title);
  dump("*** results = " + results + "\n");
  attachStrings("results", results);
  var deck = document.getElementById("stateDeck");
  deck.setAttribute("selectedIndex", "3");
  var nextButton = document.getElementById("forward");
  nextButton.label = nextButton.getAttribute("finishedval");
  nextButton.removeAttribute("disabled");
  var cancelButton = document.getElementById("cancel");
  cancelButton.setAttribute("disabled", "true");
  var backButton = document.getElementById("back");
  backButton.setAttribute("disabled", "true");

  // If the Local Folder is not existed, create it after successfully 
  // import "mail" and "settings"
  var checkLocalFolder = (top.progressInfo.importType == 'mail' || top.progressInfo.importType == 'settings') ? true : false;
  if (good && checkLocalFolder && !top.progressInfo.localFolderExists) {
    var am = Components.classes["@mozilla.org/messenger/account-manager;1"].getService(Components.interfaces.nsIMsgAccountManager);
    if (am)
      am.createLocalMailAccount();
  }
}

function attachStrings(aNode, aString)
{
  var attachNode = document.getElementById(aNode);
  if (!aString) {
    attachNode.parentNode.setAttribute("hidden", "true");
    return;
  }
  var strings = aString.split("\n");
  for (var i = 0; i < strings.length; i++) {
    if (strings[i]) {
      var currNode = document.createTextNode(strings[i]);
      attachNode.appendChild(currNode);
      var br = document.createElementNS("http://www.w3.org/1999/xhtml", 'br');
      attachNode.appendChild( br);
    }
  }
}

function ShowAddressComplete( good)
{
  var str = null;
  if (good == true) {
    if ((top.selectedModuleName != null) && (top.selectedModuleName.length > 0))
      str = gImportMsgsBundle.getFormattedString('ImportAddressSuccess', [ top.selectedModuleName ]);
    else
      str = gImportMsgsBundle.getFormattedString('ImportAddressSuccess', [ "" ]);
    str += "\n";
    str += "\n" + top.successStr.data;
  }
  else {
    if ((top.errorStr.data != null) && (top.errorStr.data.length > 0)) {
      if ((top.selectedModuleName != null) && (top.selectedModuleName.length > 0))
        str = gImportMsgsBundle.getFormattedString('ImportAddressFailed', [ top.selectedModuleName ]);
      else
        str = gImportMsgsBundle.getFormattedString('ImportAddressFailed', [ "" ]);
      str += "\n" + top.errorStr.data;
    }
  }

  if (str != null)
    alert( str);
}

/*
  Import Settings from a specific module, returns false if it failed
  and true if successful.  A "local mail" account is returned in newAccount.
  This is only useful in upgrading - import the settings first, then
  import mail into the account returned from ImportSettings, then
  import address books.
  An error string is returned as error.value
*/
function ImportSettings( module, newAccount, error) {
  var setIntf = module.GetImportInterface( "settings");
  if (setIntf != null)
    setIntf = setIntf.QueryInterface( Components.interfaces.nsIImportSettings);
  if (setIntf == null) {
    error.value = gImportMsgsBundle.getString('ImportSettingsBadModule');
    return( false);
  }

  // determine if we can auto find the settings or if we need to ask the user
  var location = new Object();
  var description = new Object();
  var result = setIntf.AutoLocate( description, location);
  if (result == false) {
    // In this case, we couldn't not find the settings
    if (location.value != null) {
      // Settings were not found, however, they are specified
      // in a file, so ask the user for the settings file.
      var filePicker = Components.classes["@mozilla.org/filepicker;1"].createInstance();
      if (filePicker != null) {
        filePicker = filePicker.QueryInterface( Components.interfaces.nsIFilePicker);
        if (filePicker != null) {
          var file = null;
          try {
            filePicker.init( top.window, gImportMsgsBundle.getString('ImportSelectSettings'), Components.interfaces.nsIFilePicker.modeOpen);
            filePicker.appendFilters( Components.interfaces.nsIFilePicker.filterAll);
            filePicker.show();
            file = filePicker.file;
          }
          catch(ex) {
            file = null;
            error.value = null;
            return( false);
          }
          if (file != null) {
            setIntf.SetLocation( file);
          }
          else {
            error.value = null;
            return( false);
          }
        }
        else {
          error.value = gImportMsgsBundle.getString('ImportSettingsNotFound');
          return( false);
        }
      }
      else {
        error.value = gImportMsgsBundle.getString('ImportSettingsNotFound');
        return( false);
      }
    }
    else {
      error.value = gImportMsgsBundle.getString('ImportSettingsNotFound');
      return( false);
    }
  }

  // interesting, we need to return the account that new
  // mail should be imported into?
  // that's really only useful for "Upgrade"
  result = setIntf.Import( newAccount);
  if (result == false) {
    error.value = gImportMsgsBundle.getString('ImportSettingsFailed');
  }
  return( result);
}

function ImportMail( module, success, error) {
  if (top.progressInfo.importInterface || top.progressInfo.intervalState) {
    error.data = gImportMsgsBundle.getString('ImportAlreadyInProgress');
    return( false);
  }

  top.progressInfo.importSuccess = false;

  var mailInterface = module.GetImportInterface( "mail");
  if (mailInterface != null)
    mailInterface = mailInterface.QueryInterface( Components.interfaces.nsIImportGeneric);
  if (mailInterface == null) {
    error.data = gImportMsgsBundle.getString('ImportMailBadModule');
    return( false);
  }

  var loc = mailInterface.GetData( "mailLocation");

  if (loc == null) {
    // No location found, check to see if we can ask the user.
    if (mailInterface.GetStatus( "canUserSetLocation") != 0) {
      var filePicker = Components.classes["@mozilla.org/filepicker;1"].createInstance();
      if (filePicker != null) {
        filePicker = filePicker.QueryInterface( Components.interfaces.nsIFilePicker);
        if (filePicker != null) {
          try {
            filePicker.init( top.window, gImportMsgsBundle.getString('ImportSelectMailDir'), Components.interfaces.nsIFilePicker.modeGetFolder);
            filePicker.appendFilters( Components.interfaces.nsIFilePicker.filterAll);
            filePicker.show();
            if (filePicker.file && (filePicker.file.path.length > 0))
              mailInterface.SetData( "mailLocation", filePicker.file);
            else
              return( false);
          } catch( ex) {
            // don't show an error when we return!
            return( false);
          }
        }
        else {
          error.data = gImportMsgsBundle.getString('ImportMailNotFound');
          return( false);
        }
      }
      else {
        error.data = gImportMsgsBundle.getString('ImportMailNotFound');
        return( false);
      }
    }
    else {
      error.data = gImportMsgsBundle.getString('ImportMailNotFound');
      return( false);
    }
  }

  if (mailInterface.WantsProgress()) {
   if (mailInterface.BeginImport(success, error)) {	
      top.progressInfo.importInterface = mailInterface;
      // top.intervalState = setInterval( "ContinueImport()", 100);
      return true;
    }
    else
      return false;
  }
  else
    return mailInterface.BeginImport(success, error);
}


// The address import!  A little more complicated than the mail import
// due to field maps...
function ImportAddress( module, success, error) {
  if (top.progressInfo.importInterface || top.progressInfo.intervalState) {
    error.data = gImportMsgsBundle.getString('ImportAlreadyInProgress');
    return( false);
  }

  top.progressInfo.importSuccess = false;

  addInterface = module.GetImportInterface( "addressbook");
  if (addInterface != null)
    addInterface = addInterface.QueryInterface( Components.interfaces.nsIImportGeneric);
  if (addInterface == null) {
    error.data = gImportMsgsBundle.getString('ImportAddressBadModule');
    return( false);
  }

  var path ;
  var loc = addInterface.GetStatus( "autoFind");
  if (loc == false) {
    loc = addInterface.GetData( "addressLocation");
    if (loc != null) {
      loc = loc.QueryInterface( Components.interfaces.nsIFile);
      if (loc != null) {
        if (!loc.exists)
          loc = null;
      }
    }
  }

  if (loc == null) {
    // Couldn't find the address book, see if we can
    // as the user for the location or not?
    if (addInterface.GetStatus( "canUserSetLocation") == 0) {
      // an autofind address book that could not be found!
      error.data = gImportMsgsBundle.getString('ImportAddressNotFound');
      return( false);
    }

    var filePicker = Components.classes["@mozilla.org/filepicker;1"].createInstance();
    if (filePicker != null) {
      filePicker = filePicker.QueryInterface( Components.interfaces.nsIFilePicker);
      if (filePicker == null) {
        error.data = gImportMsgsBundle.getString('ImportAddressNotFound');
        return( false);
      }
    }
    else {
      error.data = gImportMsgsBundle.getString('ImportAddressNotFound');
      return( false);
    }

    // The address book location was not found.
    // Determine if we need to ask for a directory
    // or a single file.
    var file = null;
    var fileIsDirectory = false;
    if (addInterface.GetStatus( "supportsMultiple") != 0) {
      // ask for dir
      try {
        filePicker.init( top.window, gImportMsgsBundle.getString('ImportSelectAddrDir'), Components.interfaces.nsIFilePicker.modeGetFolder);
        filePicker.appendFilters( Components.interfaces.nsIFilePicker.filterAll);
        filePicker.show();
        if (filePicker.file && (filePicker.file.path.length > 0)) {
          file = filePicker.file;
          fileIsDirectory = true;
        }
        else {
          file = null;
        }
      } catch( ex) {
        file = null;
      }
    }
    else {
      // ask for file
      try {
        filePicker.init(top.window,
                        gImportMsgsBundle.getString('ImportSelectAddrFile'),
                        Components.interfaces.nsIFilePicker.modeOpen);
        if (selectedModuleName ==
            document.getElementById("bundle_vcardImportMsgs")
                    .getString("vCardImportName")) {
          var addressbookBundle = document.getElementById("bundle_addressbook");
          filePicker.appendFilter(addressbookBundle.getString('VCFFiles'), "*.vcf");
        } else {
          var addressbookBundle = document.getElementById("bundle_addressbook");
          filePicker.appendFilter(addressbookBundle.getString('LDIFFiles'), "*.ldi; *.ldif");
          filePicker.appendFilter(addressbookBundle.getString('CSVFiles'), "*.csv");
          filePicker.appendFilter(addressbookBundle.getString('TABFiles'), "*.tab; *.txt");
          filePicker.appendFilters(Components.interfaces.nsIFilePicker.filterAll);
        }

        if (filePicker.show() == Components.interfaces.nsIFilePicker.returnCancel)
          return false;

        if (filePicker.file && (filePicker.file.path.length > 0))
          file = filePicker.file;
        else
          file = null;
      } catch( ex) {
        dump("ImportAddress(): failure when picking a file to import:  " + ex + "\n");
        file = null;
      }
    }

    path = filePicker.file.leafName;
	
    if (file == null) {
      return( false);
    }

    if ( !fileIsDirectory && (file.fileSize == 0) ) {
      var errorText = gImportMsgsBundle.getFormattedString('ImportEmptyAddressBook', [path]);
      var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService(Components.interfaces.nsIPromptService);

      promptService.alert(window, document.title, errorText);
      return false;
    }
    addInterface.SetData("addressLocation", file);
  }

  var map = addInterface.GetData( "fieldMap");
  if (map != null) {
    map = map.QueryInterface( Components.interfaces.nsIImportFieldMap);
    if (map != null) {
      var result = new Object();
      result.ok = false;
      top.window.openDialog(
        "chrome://messenger/content/fieldMapImport.xul",
        "",
        "chrome,modal,titlebar",
        {fieldMap: map,
         addInterface: addInterface,
         result: result});
    }
    if (result.ok == false)
      return( false);
  }

  if (addInterface.WantsProgress()) {
    if (addInterface.BeginImport(success, error)) {
      top.progressInfo.importInterface = addInterface;
      // top.intervalState = setInterval( "ContinueImport()", 100);
      return true;
    }
    return false;
  }

  return addInterface.BeginImport(success, error);
}

/*
  Import filters from a specific module.
  Returns false if it failed and true if it succeeded.
  An error string is returned as error.value.
*/
function ImportFilters( module, error)
{
  if (top.progressInfo.importInterface || top.progressInfo.intervalState) {
    error.data = gImportMsgsBundle.getString('ImportAlreadyInProgress');
    return( false);
  }

  top.progressInfo.importSuccess = false;

  var filtersInterface = module.GetImportInterface("filters");
  if (filtersInterface != null)
    filtersInterface = filtersInterface.QueryInterface(Components.interfaces.nsIImportFilters);
  if (filtersInterface == null) {
    error.data = gImportMsgsBundle.getString('ImportFiltersBadModule');
    return( false);
  }
  
  return filtersInterface.Import(error);
}

/*
  Import feeds.
*/
function ImportFeeds()
{
  // Get file to open from filepicker.
  let openFile = FeedSubscriptions.opmlPickOpenFile();
  if (!openFile)
    return false;

  let acctName;
  let acctNewExist = gFeedsBundle.getString("ImportFeedsExisting");
  let fileName = openFile.path;
  let server = document.getElementById("moduleList").selectedItem.server;
  newFeedAcctCreated = false;

  if (!server)
  {
    // Create a new Feeds account.
    acctName = document.getElementById("acctName").value;
    server = FeedUtils.createRssAccount(acctName).incomingServer;
    acctNewExist = gFeedsBundle.getString("ImportFeedsNew");
    newFeedAcctCreated = true;
  }

  acctName = server.rootFolder.prettyName;

  let callback = function(aStatusReport, aLastFolder , aFeedWin)
  {
    let message = gFeedsBundle.getFormattedString("ImportFeedsDone",
                                                  [fileName, acctNewExist, acctName]);
    ShowImportResultsRaw(message + "  " + aStatusReport, null, true);
    document.getElementById("back").removeAttribute("disabled");

    let subscriptionsWindow = Services.wm.getMostRecentWindow("Mail:News-BlogSubscriptions");
    if (subscriptionsWindow)
    {
      let feedWin = subscriptionsWindow.FeedSubscriptions;
      if (aLastFolder)
        feedWin.FolderListener.folderAdded(aLastFolder);
      feedWin.mActionMode = null;
      feedWin.updateButtons(feedWin.mView.currentItem);
      feedWin.clearStatusInfo();
      feedWin.updateStatusItem("statusText", aStatusReport);
    }
  }

  if (!FeedSubscriptions.importOPMLFile(openFile, server, callback))
    return false;

  let subscriptionsWindow = Services.wm.getMostRecentWindow("Mail:News-BlogSubscriptions");
  if (subscriptionsWindow)
  {
    let feedWin = subscriptionsWindow.FeedSubscriptions;
    feedWin.mActionMode = feedWin.kImportingOPML;
    feedWin.updateButtons(null);
    let statusReport = gFeedsBundle.getString("subscribe-loading");
    feedWin.updateStatusItem("statusText", statusReport);
    feedWin.updateStatusItem("progressMeter", "?");
  }

  return true;
}

function SwitchType( newType)
{
  if (top.importType == newType)
    return;

  top.importType = newType;
  top.progressInfo.importType = newType;

  SetUpImportType();

  SetDivText('description', "");
}


function next()
{
  var deck = document.getElementById("stateDeck");
  var index = deck.getAttribute("selectedIndex");
  switch (index) {
  case "0":
    var backButton = document.getElementById("back");
    backButton.removeAttribute("disabled");
    var radioGroup = document.getElementById("importFields");

    if (radioGroup.value == "all")
    {
#ifdef MOZ_THUNDERBIRD
      window.openDialog("chrome://messenger/content/migration/migration.xul",
                        "", "chrome,dialog,modal,centerscreen");
#else
      window.openDialog("chrome://communicator/content/migration/migration.xul",
                        "", "chrome,dialog,modal,centerscreen");
#endif
      close();
    }
    else
    {
      SwitchType(radioGroup.value);
      deck.setAttribute("selectedIndex", "1");
      SelectFirstItem();
      enableAdvance();
    }
    break;
  case "1":
    ImportDialogOKButton();
    break;
  case "3":
    close();
    break;
  }
}

function SelectFirstItem()
{
  var listbox = document.getElementById("moduleList");
  if (listbox.selectedIndex == -1)
    listbox.selectedIndex = 0;
  ImportSelectionChanged();
}

function enableAdvance()
{
  var listbox = document.getElementById("moduleList");
  var nextButton = document.getElementById("forward");
  if (listbox.selectedItems.length)
    nextButton.removeAttribute("disabled");
  else
    nextButton.setAttribute("disabled", "true");
}

function back()
{
  var deck = document.getElementById("stateDeck");
  switch (deck.getAttribute("selectedIndex")) {
  case "1":
    var backButton = document.getElementById("back");
    backButton.setAttribute("disabled", "true");
    var nextButton = document.getElementById("forward");
    nextButton.label = nextButton.getAttribute("nextval");
    nextButton.removeAttribute("disabled");
    deck.setAttribute("selectedIndex", "0");
    break;
  case "3":
    // Clear out the results box.
    var results = document.getElementById("results");
    while (results.hasChildNodes())
      results.removeChild(results.lastChild);

    // Reset the next button.
    var nextButton = document.getElementById("forward");
    nextButton.label = nextButton.getAttribute("nextval");
    nextButton.removeAttribute("disabled");

    // Enable the cancel button again.
    document.getElementById("cancel").removeAttribute("disabled");

    // If a new Feed account has been created, rebuild the list.
    if (newFeedAcctCreated)
      ListFeedAccounts();

    // Now go back to the second page.
    deck.setAttribute("selectedIndex", "1");
    break;
  }
}
