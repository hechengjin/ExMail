/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource:///modules/folderUtils.jsm");
Components.utils.import("resource:///modules/iteratorUtils.jsm");
Components.utils.import("resource:///modules/MailUtils.js");

const kDefaultMode = "all";

var nsMsgFolderFlags = Components.interfaces.nsMsgFolderFlags;

/**
 * This file contains the controls and functions for the folder pane.
 * The following definitions will be useful to know:
 *
 * gFolderTreeView - the controller for the folder tree.
 * ftvItem  - folder tree view item, representing a row in the tree
 * mode - folder view type, e.g., all folders, favorite folders, MRU...
 */

/**
 * An interface that needs to be implemented in order to add a new view to the
 * folder tree. For default behavior, it is recommended that implementers
 * subclass this interface instead of relying on duck typing.
 *
 * For implementation examples, see |gFolderTreeView._modes|. For how to
 * register this mode with |gFolderTreeView|, see
 * |gFolderTreeView.registerFolderTreeMode|.
 */
let IFolderTreeMode = {
  /**
   * Generates the folder map for this mode.
   *
   * @param aFolderTreeView The gFolderTreeView for which this mode is being
   *     activated.
   *
   * @returns An array containing ftvItem instances representing the top-level
   *     folders in this view.
   */
  generateMap: function IFolderTreeMode_generateMap(aFolderTreeView) {
    return null;
  },

  /**
   * Given an nsIMsgFolder, returns its parent in the map. The default behaviour
   * is to return the folder's actual parent (aFolder.parent). Folder tree modes
   * may decide to override it.
   *
   * If the parent isn't easily computable given just the folder, you may
   * consider generating the entire ftvItem tree at once and using a map from
   * folders to ftvItems.
   *
   * @returns an nsIMsgFolder representing the parent of the folder in the view,
   *     or null if the folder is a top-level folder in the map. It is expected
   *     that the returned parent will have the given folder as one of its
   *     children.
   * @note This function need not guarantee that either the folder or its parent
   *       is actually in the view.
   */
  getParentOfFolder: function IFolderTreeMode_getParentOfFolder(aFolder) {
    return aFolder.parent;
  },

  /**
   * Given an nsIMsgDBHdr, returns the folder it is considered to be contained
   * in, in this mode. This is usually just the physical folder it is contained
   * in (aMsgHdr.folder), but some modes may decide to override this. For
   * example, combined views like Smart Folders return the smart inbox for any
   * messages in any inbox.
   *
   * The folder returned doesn't need to be in the view.

   * @returns The folder the message header is considered to be contained in, in
   *     this mode. The returned folder may or may not actually be in the view
   *     -- however, given a valid nsIMsgDBHdr, it is expected that a) a
   *     non-null folder is returned, and that b) the folder that is returned
   *     actually does contain the message header.
   */
  getFolderForMsgHdr: function IFolderTreeMode_getFolderForMsgHdr(aMsgHdr) {
    return aMsgHdr.folder;
  },

  /**
   * Notified when a folder is added. The default behavior is to add it as a
   * child of the parent item, but some views may decide to override this. For
   * example, combined views like Smart Folders add any new inbox as a child of
   * the smart inbox.
   *
   * @param aParent The parent of the folder that was added.
   * @param aFolder The folder that was added.
   */
  onFolderAdded: function IFolderTreeMode_onFolderAdded(aParent, aFolder) {
    gFolderTreeView.addFolder(aParent, aFolder);
  }
};

/**
 * This is our controller for the folder-tree. It includes our nsITreeView
 * implementation, as well as other control functions.
 */
let gFolderTreeView = {
  /**
   * Called when the window is initially loaded.  This function initializes the
   * folder-pane to the view last shown before the application was closed.
   */
  load: function ftv_load(aTree, aJSONFile) {
    const Cc = Components.classes;
    const Ci = Components.interfaces;
    this._treeElement = aTree;

    // the folder pane can be used for other trees which may not have these elements.
    if (document.getElementById("folderpane_splitter"))
      document.getElementById("folderpane_splitter").collapsed = false;
    if (document.getElementById("folderPaneBox"))
      document.getElementById("folderPaneBox").collapsed = false;

    try {
      // Normally our tree takes care of keeping the last selected by itself.
      // However older versions of TB stored this in a preference, which we need
      // to migrate
      let prefB = Cc["@mozilla.org/preferences-service;1"]
                     .getService(Ci.nsIPrefBranch);
      let modeIndex = prefB.getIntPref("mail.ui.folderpane.view");
      this._mode = this._modeNames[modeIndex];
      prefB.deleteBranch("mail.ui.folderpane");
    } catch(ex) {
      // This is ok.  If we've already migrated we'll end up here
    }

    if (document.getElementById('folderpane-title')) {
      let string;
        if (this.mode in this._modeDisplayNames)
          string = this._modeDisplayNames[this.mode];
        else {
          let key = "folderPaneModeHeader_" + this.mode;
          string = document.getElementById("bundle_messenger").getString(key);
        }
      document.getElementById('folderpane-title').value = string;
    }

    if (aJSONFile) {
      // Parse our persistent-open-state json file
      let file = Cc["@mozilla.org/file/directory_service;1"]
                    .getService(Ci.nsIProperties).get("ProfD", Ci.nsIFile);
      file.append(aJSONFile);

      if (file.exists()) {
        let data = "";
        let fstream = Cc["@mozilla.org/network/file-input-stream;1"]
                         .createInstance(Ci.nsIFileInputStream);
        let sstream = Cc["@mozilla.org/scriptableinputstream;1"]
                         .createInstance(Ci.nsIScriptableInputStream);
        fstream.init(file, -1, 0, 0);
        sstream.init(fstream);

        while (sstream.available())
          data += sstream.read(4096);

        sstream.close();
        fstream.close();
        try {
          this._persistOpenMap = JSON.parse(data);
        } catch (x) {
          Components.utils.reportError(
            document.getElementById("bundle_messenger")
                    .getFormattedString("failedToReadFile", [aJSONFile, x]));
        }
      }
    }

    // Load our data
    this._rebuild();
    // And actually draw the tree
    aTree.view = this;

    // Add this listener so that we can update the tree when things change
    let session = Cc["@mozilla.org/messenger/services/session;1"]
                     .getService(Ci.nsIMsgMailSession);
    session.AddFolderListener(this, Ci.nsIFolderListener.all);
  },

  /**
   * Called when the window is being torn down.  Here we undo everything we did
   * onload.  That means removing our listener and serializing our JSON.
   */
  unload: function ftv_unload(aJSONFile) {
    const Cc = Components.classes;
    const Ci = Components.interfaces;

    // Remove our listener
    let session = Cc["@mozilla.org/messenger/services/session;1"]
                     .getService(Ci.nsIMsgMailSession);
    session.RemoveFolderListener(this);

    if (aJSONFile) {
      // Write out our json file...
      let data = JSON.stringify(this._persistOpenMap);
      let file = Cc["@mozilla.org/file/directory_service;1"]
                    .getService(Ci.nsIProperties).get("ProfD", Ci.nsIFile);
      file.append(aJSONFile);
      let foStream = Cc["@mozilla.org/network/safe-file-output-stream;1"]
                        .createInstance(Ci.nsIFileOutputStream);

      foStream.init(file, 0x02 | 0x08 | 0x20, parseInt("0666", 8), 0);
      // safe-file-output-stream appears to throw an error if it doesn't write everything at once
      // so we won't worry about looping to deal with partial writes
      foStream.write(data, data.length);
      foStream.QueryInterface(Ci.nsISafeOutputStream).finish();
      foStream.close();
    }
  },

  /**
   * Extensions can use this function to add a new mode to the folder pane.
   *
   * @param aCommonName  an internal name to identify this mode. Must be unique
   * @param aMode An implementation of |IFolderTreeMode| for this mode.
   * @param aDisplayName  a localized name for this mode
   */
  registerFolderTreeMode: function ftv_registerFolderTreeMode(aCommonName,
                                                              aMode,
                                                              aDisplayName) {
    this._modeNames.push(aCommonName);
    this._modes[aCommonName] = aMode;
    this._modeDisplayNames[aCommonName] = aDisplayName;
  },

  /**
   * Unregisters a previously registered mode. Since common-names must be unique
   * this is all that need be provided to unregister.
   * @param aCommonName  the common-name with which the mode was previously
   *                     registered
   */
  unregisterFolderTreeMode: function ftv_unregisterFolderTreeMode(aCommonName) {
    this._modeNames.splice(this._modeNames.indexOf(aCommonName), 1);
    delete this._modes[aCommonName];
    delete this._modeDisplayNames[aCommonName];
    if (this._mode == aCommonName)
      this.mode = kDefaultMode;
  },
  
  /**
   * Retrieves a specific mode object
   * @param aCommonName  the common-name with which the mode was previously
   *                     registered
   */
  getFolderTreeMode: function ftv_getFolderTreeMode(aCommonName) {
    return this._modes[aCommonName];
  },

  /**
   * Called to move to the next/prev folder-mode in the list
   *
   * @param aForward  whether or not we should move forward in the list
   */
  cycleMode: function ftv_cycleMode(aForward) {
    let index = this._modeNames.indexOf(this.mode);
    let offset = aForward ? 1 : this._modeNames.length - 1;
    index = (index + offset) % this._modeNames.length;

    this.mode = this._modeNames[index];
  },

  /**
   * If the hidden pref is set, then double-clicking on a folder should open it
   *
   * @param event  the double-click event
   */
  onDoubleClick: function ftv_onDoubleClick(aEvent) {
    if (aEvent.button != 0 || aEvent.originalTarget.localName == "twisty" ||
        aEvent.originalTarget.localName == "slider" ||
        aEvent.originalTarget.localName == "scrollbarbutton")
      return;

    let row = gFolderTreeView._treeElement.treeBoxObject.getRowAt(aEvent.clientX,
                                                                  aEvent.clientY);
    let folderItem = gFolderTreeView._rowMap[row];
    if (folderItem)
      folderItem.command();

    // Don't let the double-click toggle the open state of the folder here
    aEvent.stopPropagation();
  },

  getFolderAtCoords: function ftv_getFolderAtCoords(aX, aY) {
    let row = gFolderTreeView._treeElement.treeBoxObject.getRowAt(aX, aY);
    if (row in gFolderTreeView._rowMap)
      return gFolderTreeView._rowMap[row]._folder;
    return null;
  },

  /**
   * A string representation for the current display-mode.  Each value here must
   * correspond to an entry in _modes
   */
  _mode: null,
  get mode() {
    if (!this._mode) {
      this._mode = this._treeElement.getAttribute("mode");
      // this can happen when an extension is removed
      if (!(this._mode in this._modes))
        this._mode = kDefaultMode;
    }
    return this._mode;
  },
  set mode(aMode) {
    this._mode = aMode;

    let string;
    if (this._mode in this._modeDisplayNames)
      string = this._modeDisplayNames[this._mode];
    else {
      let key = "folderPaneModeHeader_" + aMode;
      string = document.getElementById("bundle_messenger").getString(key);
    }
    document.getElementById('folderpane-title').value = string;

    this._treeElement.setAttribute("mode", aMode);
    this._rebuild();
  },

  /**
   * Selects a given nsIMsgFolder in the tree.  This function will also ensure
   * that the folder is actually being displayed (that is, that none of its
   * ancestors are collapsed.
   *
   * @param aFolder  the nsIMsgFolder to select
   * @param [aForceSelect] Whether we should switch to the default mode to
   *      select the folder in case we didn't find the folder in the current
   *      view. Defaults to false.
   * @returns true if the folder selection was successful, false if it failed
   *     (probably because the folder isn't in the view at all)
   */
  selectFolder: function ftv_selectFolder(aFolder, aForceSelect) {
    // "this" inside the nested function refers to the function...
    // Also note that openIfNot is recursive.
    let tree = this;
    let folderTreeMode = this._modes[this._mode];
    function openIfNot(aFolderToOpen) {
      let index = tree.getIndexOfFolder(aFolderToOpen);
      if (index != null) {
        if (!tree._rowMap[index].open)
          tree._toggleRow(index, false);
        return true;
      }

      // not found, so open the parent
      let parent = folderTreeMode.getParentOfFolder(aFolderToOpen);
      if (parent && openIfNot(parent)) {
        // now our parent is open, so we can open ourselves
        index = tree.getIndexOfFolder(aFolderToOpen);
        if (index != null) {
          tree._toggleRow(index, false);
          return true;
        }
      }

      // No way we can find the folder now.
      return false;
    }
    let parent = folderTreeMode.getParentOfFolder(aFolder);
    if (parent)
      openIfNot(parent);

    let folderIndex = tree.getIndexOfFolder(aFolder);
    if (folderIndex == null) {
      if (aForceSelect) {
        // Switch to the default mode. The assumption here is that the default
        // mode can display every folder
        this.mode = kDefaultMode;
        // We don't want to get stuck in an infinite recursion, so pass in false
        return this.selectFolder(aFolder, false);
      }

      return false;
    }

    this.selection.select(folderIndex);
    this._treeElement.treeBoxObject.ensureRowIsVisible(folderIndex);
    return true;
  },

  /**
   * Returns the index of a folder in the current display.
   *
   * @param aFolder  the folder whose index should be returned.
   * @returns The index of the folder in the view (a number).
   * @note If the folder is not in the display (perhaps because one of its
   *       anscetors is collapsed), this function returns null.
   */
  getIndexOfFolder: function ftv_getIndexOfFolder(aFolder) {
    for each (let [iRow, row] in Iterator(this._rowMap)) {
      if (row.id == aFolder.URI)
        return iRow;
    }
    return null;
  },

  /**
   * Returns the folder for an index in the current display.
   *
   * @param aIndex the index for which the folder should be returned.
   * @note If the index is out of bounds, this function returns null.
   */
  getFolderForIndex: function ftv_getFolderForIndex(aIndex) {
    if (aIndex < 0 || aIndex >= this._rowMap.length)
      return null;
    return this._rowMap[aIndex]._folder;
  },

  /**
   * Returns the parent of a folder in the current view. This may be, but is not
   * necessarily, the actual parent of the folder (aFolder.parent). In
   * particular, in the smart view, special folders are usually children of the
   * smart folder of that kind.
   *
   * @param aFolder The folder to get the parent of.
   * @returns The parent of the folder, or null if the parent wasn't found.
   * @note This function does not guarantee that either the folder or its parent
   *       is actually in the view.
   */
  getParentOfFolder: function ftv_getParentOfFolder(aFolder) {
    return this._modes[this._mode].getParentOfFolder(aFolder);
  },

  /**
   * Given an nsIMsgDBHdr, returns the folder it is considered to be contained
   * in, in the current mode. This is usually, but not necessarily, the actual
   * folder the message is in (aMsgHdr.folder). For more details, see
   * |IFolderTreeMode.getFolderForMsgHdr|.
   */
  getFolderForMsgHdr: function ftv_getFolderForMsgHdr(aMsgHdr) {
    return this._modes[this._mode].getFolderForMsgHdr(aMsgHdr);
  },

  /**
   * Returns the |ftvItem| for an index in the current display. Intended for use
   * by folder tree mode implementers.
   *
   * @param aIndex The index for which the ftvItem should be returned.
   * @note If the index is out of bounds, this function returns null.
   */
  getFTVItemForIndex: function ftv_getFTVItemForIndex(aIndex) {
    return this._rowMap[aIndex];
  },

  /**
   * Returns an array of nsIMsgFolders corresponding to the current selection
   * in the tree
   */
  getSelectedFolders: function ftv_getSelectedFolders() {
    let selection = this.selection;
    if (!selection)
      return [];

    let folderArray = [];
    let rangeCount = selection.getRangeCount();
    for (let i = 0; i < rangeCount; i++) {
      let startIndex = {};
      let endIndex = {};
      selection.getRangeAt(i, startIndex, endIndex);
      for (let j = startIndex.value; j <= endIndex.value; j++) {
        if (j < this._rowMap.length)
          folderArray.push(this._rowMap[j]._folder);
      }
    }
    return folderArray;
  },

  /**
   * Adds a new child |ftvItem| to the given parent |ftvItem|. Intended for use
   * by folder tree mode implementers.
   *
   * @param aParentItem The parent ftvItem. It is assumed that this is visible
   *     in the view.
   * @param aParentIndex The index of the parent ftvItem in the view.
   * @param aItem The item to add.
   */
  addChildItem: function ftv_addChildItem(aParentItem, aParentIndex, aItem) {
    this._addChildToView(aParentItem, aParentIndex, aItem);
  },

  // ****************** Start of nsITreeView implementation **************** //

  get rowCount() {
    return this._rowMap.length;
  },

  /**
   * drag drop interfaces
   */
  canDrop: function ftv_canDrop(aRow, aOrientation) {
    const Cc = Components.classes;
    const Ci = Components.interfaces;
    let targetFolder = gFolderTreeView._rowMap[aRow]._folder;
    if (!targetFolder)
      return false;
    let dt = this._currentTransfer;
    let types = dt.mozTypesAt(0);
    if (Array.indexOf(types, "text/x-moz-message") != -1) {
      if (aOrientation != Ci.nsITreeView.DROP_ON)
        return false;
      // Don't allow drop onto server itself.
      if (targetFolder.isServer)
        return false;
      // Don't allow drop into a folder that cannot take messages.
      if (!targetFolder.canFileMessages)
        return false;
      let messenger = Cc["@mozilla.org/messenger;1"].createInstance(Ci.nsIMessenger);
      for (let i = 0; i < dt.mozItemCount; i++) {
        let msgHdr = messenger.msgHdrFromURI(dt.mozGetDataAt("text/x-moz-message", i));
        // Don't allow drop onto original folder.
        if (msgHdr.folder == targetFolder)
          return false;
      }
      return true;
    }
    else if (Array.indexOf(types, "text/x-moz-folder") != -1) {
      if (aOrientation != Ci.nsITreeView.DROP_ON)
        return false;
      // If cannot create subfolders then don't allow drop here.
      if (!targetFolder.canCreateSubfolders)
        return false;
      for (let i = 0; i < dt.mozItemCount; i++) {
        let folder = dt.mozGetDataAt("text/x-moz-folder", i)
                       .QueryInterface(Ci.nsIMsgFolder);
        // Don't allow to drop on itself.
        if (targetFolder == folder)
          return false;
        // Don't copy within same server.
        if ((folder.server == targetFolder.server) &&
             (dt.dropEffect == 'copy'))
          return false;
        // Don't allow immediate child to be dropped onto its parent.
        if (targetFolder == folder.parent)
          return false;
        // Don't allow dragging of virtual folders across accounts.
        if ((folder.flags & nsMsgFolderFlags.Virtual) &&
            folder.server != targetFolder.server)
          return false;
        // Don't allow parent to be dropped on its ancestors.
        if (folder.isAncestorOf(targetFolder))
          return false;
        // If there is a folder that can't be renamed, don't allow it to be
        // dropped if it is not to "Local Folders" or is to the same account.
        if (!folder.canRename && (targetFolder.server.type != "none" ||
                                  folder.server == targetFolder.server))
          return false;
      }
      return true;
    }
    else if (Array.indexOf(types, "text/x-moz-newsfolder") != -1) {
      // Don't allow dragging onto element.
      if (aOrientation == Ci.nsITreeView.DROP_ON)
        return false;
      // Don't allow drop onto server itself.
      if (targetFolder.isServer)
        return false;
      for (let i = 0; i < dt.mozItemCount; i++) {
        let folder = dt.mozGetDataAt("text/x-moz-newsfolder", i)
                       .QueryInterface(Ci.nsIMsgFolder);
        // Don't allow dragging newsgroup to other account.
        if (targetFolder.rootFolder != folder.rootFolder)
          return false;
        // Don't allow dragging newsgroup to before/after itself.
        if (targetFolder == folder)
          return false;
        // Don't allow dragging newsgroup to before item after or
        // after item before.
        let row = aRow + aOrientation;
        if (row in gFolderTreeView._rowMap &&
            (gFolderTreeView._rowMap[row]._folder == folder))
          return false;
      }
      return true;
    }
    // Allow subscribing to feeds by dragging an url to a feed account.
    else if (targetFolder.server.type == "rss" && dt.mozItemCount == 1)
      return FeedUtils.getFeedUriFromDataTransfer(dt) ? true : false;
    else if (Array.indexOf(types, "application/x-moz-file") != -1) {
      if (aOrientation != Ci.nsITreeView.DROP_ON)
        return false;
      // Don't allow drop onto server itself.
      if (targetFolder.isServer)
        return false;
      // Don't allow drop into a folder that cannot take messages.
      if (!targetFolder.canFileMessages)
        return false;
      for (let i = 0; i < dt.mozItemCount; i++) {
        let extFile = dt.mozGetDataAt("application/x-moz-file", i)
                        .QueryInterface(Ci.nsILocalFile);
        return extFile.isFile();
      }
    }
    return false;
  },
  drop: function ftv_drop(aRow, aOrientation) {
    const Cc = Components.classes;
    const Ci = Components.interfaces;
    let targetFolder = gFolderTreeView._rowMap[aRow]._folder;

    let dt = this._currentTransfer;
    let count = dt.mozItemCount;
    let cs = Cc["@mozilla.org/messenger/messagecopyservice;1"]
                .getService(Ci.nsIMsgCopyService);

    // we only support drag of a single flavor at a time.
    let types = dt.mozTypesAt(0);
    if (Array.indexOf(types, "text/x-moz-folder") != -1) {
      for (let i = 0; i < count; i++) {
        let folders = new Array;
        folders.push(dt.mozGetDataAt("text/x-moz-folder", i)
                       .QueryInterface(Ci.nsIMsgFolder));
        let array = toXPCOMArray(folders, Ci.nsIMutableArray);
        cs.CopyFolders(array, targetFolder,
                      (folders[0].server == targetFolder.server), null,
                       msgWindow);
      }
    }
    else if (Array.indexOf(types, "text/x-moz-newsfolder") != -1) {
      // Start by getting folders into order.
      let folders = new Array;
      for (let i = 0; i < count; i++) {
        let folder = dt.mozGetDataAt("text/x-moz-newsfolder", i)
                       .QueryInterface(Ci.nsIMsgFolder);
        folders[this.getIndexOfFolder(folder)] = folder;
      }
      let newsFolder = targetFolder.rootFolder
                                   .QueryInterface(Ci.nsIMsgNewsFolder);
      // When moving down, want to insert first one last.
      // When moving up, want to insert first one first.
      let i = (aOrientation == 1) ? folders.length - 1 : 0;
      while (i >= 0 && i < folders.length) {
        let folder = folders[i];
        if (folder) {
          newsFolder.moveFolder(folder, targetFolder, aOrientation);
          this.selection.toggleSelect(this.getIndexOfFolder(folder));
        }
        i -= aOrientation;
      }
    }
    else if (Array.indexOf(types, "text/x-moz-message") != -1) {
      let array = Cc["@mozilla.org/array;1"]
                    .createInstance(Ci.nsIMutableArray);
      let sourceFolder;
      let messenger = Cc["@mozilla.org/messenger;1"].createInstance(Ci.nsIMessenger);
      for (let i = 0; i < count; i++) {
        let msgHdr = messenger.msgHdrFromURI(dt.mozGetDataAt("text/x-moz-message", i));
        if (!i)
          sourceFolder = msgHdr.folder;
        array.appendElement(msgHdr, false);
      }
      let prefBranch = Cc["@mozilla.org/preferences-service;1"]
                          .getService(Ci.nsIPrefService).getBranch("mail.");
      let isMove = Cc["@mozilla.org/widget/dragservice;1"]
                      .getService(Ci.nsIDragService).getCurrentSession()
                      .dragAction == Ci.nsIDragService.DRAGDROP_ACTION_MOVE;
      if (!sourceFolder.canDeleteMessages)
        isMove = false;

      prefBranch.setCharPref("last_msg_movecopy_target_uri", targetFolder.URI);
      prefBranch.setBoolPref("last_msg_movecopy_was_move", isMove);
      // ### ugh, so this won't work with cross-folder views. We would
      // really need to partition the messages by folder.
      cs.CopyMessages(sourceFolder, array, targetFolder, isMove, null,
                        msgWindow, true);
    }
    else if (Array.indexOf(types, "application/x-moz-file") != -1) {
      for (let i = 0; i < count; i++) {
        let extFile = dt.mozGetDataAt("application/x-moz-file", i)
                        .QueryInterface(Ci.nsILocalFile);
        if (extFile.isFile()) {
          let len = extFile.leafName.length;
          if (len > 4 && extFile.leafName.substr(len - 4).toLowerCase() == ".eml")
            cs.CopyFileMessage(extFile, targetFolder, null, false, 1, "", null, msgWindow);
        }
      }
    }

    if (targetFolder.server.type == "rss" && count == 1) {
      // This is a potential rss feed.  A link image as well as link text url
      // should be handled; try to extract a url from non moz apps as well.
      let validUri = FeedUtils.getFeedUriFromDataTransfer(dt);

      if (validUri)
        Cc["@mozilla.org/newsblog-feed-downloader;1"]
           .getService(Ci.nsINewsBlogFeedDownloader)
           .subscribeToFeed(validUri.spec, targetFolder, msgWindow);
    }
  },

  _onDragStart: function ftv_dragStart(aEvent) {
    // Ugh, this is ugly but necessary
    let view = gFolderTreeView;

    if (aEvent.originalTarget.localName != "treechildren")
      return;

    let folders = view.getSelectedFolders();
    folders = folders.filter(function(f) { return !f.isServer; });
    for (let i in folders) {
      let flavor = folders[i].server.type == "nntp" ? "text/x-moz-newsfolder" :
                                                      "text/x-moz-folder";
      aEvent.dataTransfer.mozSetDataAt(flavor, folders[i], i);
    }
    aEvent.dataTransfer.effectAllowed = "copyMove";
    aEvent.dataTransfer.addElement(aEvent.originalTarget);
    return;
  },

  _onDragOver: function ftv_onDragOver(aEvent) {
    this._currentTransfer = aEvent.dataTransfer;
  },

  /**
   * CSS files will cue off of these.  Note that we reach into the rowMap's
   * items so that custom data-displays can define their own properties
   */
  getCellProperties: function ftv_getCellProperties(aRow, aCol, aProps) {
    this._rowMap[aRow].getProperties(aProps, aCol);
  },

  /**
   * The actual text to display in the tree
   */
  getCellText: function ftv_getCellText(aRow, aCol) {
    if (aCol.id == "folderNameCol")
      return this._rowMap[aRow].text;
    return "";
  },

  /**
   * The ftvItems take care of assigning this when building children lists
   */
  getLevel: function ftv_getLevel(aIndex) {
    return this._rowMap[aIndex].level;
  },

  /**
   * This is easy since the ftv items assigned the _parent property when making
   * the child lists
   */
  getParentIndex: function ftv_getParentIndex(aIndex) {
    return this._rowMap.indexOf(this._rowMap[aIndex]._parent);
  },

  /**
   * This is duplicative for our normal ftv views, but custom data-displays may
   * want to do something special here
   */
  getRowProperties: function ftv_getRowProperties(aIndex, aProps) {
    this._rowMap[aIndex].getProperties(aProps);
  },

  /**
   * Check whether there are any more rows with our level before the next row
   * at our parent's level
   */
  hasNextSibling: function ftv_hasNextSibling(aIndex, aNextIndex) {
    var currentLevel = this._rowMap[aIndex].level;
    for (var i = aNextIndex + 1; i < this._rowMap.length; i++) {
      if (this._rowMap[i].level == currentLevel)
        return true;
      if (this._rowMap[i].level < currentLevel)
        return false;
    }
    return false;
  },

  /**
   * All folders are containers, so we can drag drop messages to them.
   */
  isContainer: function ftv_isContainer(aIndex) {
    return true;
  },

  isContainerEmpty: function ftv_isContainerEmpty(aIndex) {
    // If the folder has no children, the container is empty.
    return !this._rowMap[aIndex].children.length;
  },

  /**
   * Just look at the ftvItem here
   */
  isContainerOpen: function ftv_isContainerOpen(aIndex) {
    return this._rowMap[aIndex].open;
  },
  isEditable: function ftv_isEditable(aRow, aCol) {
    // We don't support editing rows in the tree yet.  We may want to later as
    // an easier way to rename folders.
    return false;
  },
  isSeparator: function ftv_isSeparator(aIndex) {
    // There are no separators in our trees
    return false;
  },
  isSorted: function ftv_isSorted() {
    // We do our own customized sorting
    return false;
  },
  setTree: function ftv_setTree(aTree) {
    this._tree = aTree;
  },

  /**
   * Opens or closes a folder with children.  The logic here is a bit hairy, so
   * be very careful about changing anything.
   */
  toggleOpenState: function ftv_toggleOpenState(aIndex) {
    this._toggleRow(aIndex, true);
  },

  _toggleRow: function toggleRow(aIndex, aExpandServer)
  {
    // Ok, this is a bit tricky.
    this._rowMap[aIndex].open = !this._rowMap[aIndex].open;
    if (!this._rowMap[aIndex].open) {
      // We're closing the current container.  Remove the children

      // Note that we can't simply splice out children.length, because some of
      // them might have children too.  Find out how many items we're actually
      // going to splice
      let count = 0;
      let i = aIndex + 1;
      let row = this._rowMap[i];
      while (row && row.level > this._rowMap[aIndex].level) {
        count++;
        row = this._rowMap[++i];
      }
      this._rowMap.splice(aIndex + 1, count);

      // Remove us from the persist map
      let index = this._persistOpenMap[this.mode]
                      .indexOf(this._rowMap[aIndex].id);
      if (index != -1)
        this._persistOpenMap[this.mode].splice(index, 1);

      // Notify the tree of changes
      if (this._tree) {
        this._tree.rowCountChanged(aIndex + 1, (-1) * count);
        this._tree.invalidateRow(aIndex);
      }
    } else {
      // We're opening the container.  Add the children to our map

      // Note that these children may have been open when we were last closed,
      // and if they are, we also have to add those grandchildren to the map
      let oldCount = this._rowMap.length;
      function recursivelyAddToMap(aChild, aNewIndex, tree) {
        // When we add sub-children, we're going to need to increase our index
        // for the next add item at our own level
        let count = 0;
        if (aChild.children.length && aChild.open) {
          for (let [i, child] in Iterator(tree._rowMap[aNewIndex].children)) {
            count++;
            var index = Number(aNewIndex) + Number(i) + 1;
            tree._rowMap.splice(index, 0, child);

            let kidsAdded = recursivelyAddToMap(child, index, tree);
            count += kidsAdded;
            // Somehow the aNewIndex turns into a string without this
            aNewIndex = Number(aNewIndex) + kidsAdded;
          }
        }
        return count;
      }
      // work around bug 658534 by passing in "this" instead of let tree = this;
      recursivelyAddToMap(this._rowMap[aIndex], aIndex, this);

      // Add this folder to the persist map
      if (!this._persistOpenMap[this.mode])
        this._persistOpenMap[this.mode] = [];
      let id = this._rowMap[aIndex].id;
      if (this._persistOpenMap[this.mode].indexOf(id) == -1)
        this._persistOpenMap[this.mode].push(id);

      // Notify the tree of changes
      if (this._tree) {
        this._tree.rowCountChanged(aIndex + 1, this._rowMap.length - oldCount);
        this._tree.invalidateRow(aIndex);
      }
      // if this was a server that was expanded, let it update its counts
      let folder = this._rowMap[aIndex]._folder;
      if (aExpandServer) {
        if (folder.isServer)
          folder.server.performExpand(msgWindow);
        else if (folder instanceof Components.interfaces.nsIMsgImapMailFolder)
          folder.performExpand(msgWindow);
      }
    }
  },

  _subFoldersWithStringProperty: function ftv_subFoldersWithStringProperty(folder, folders, aFolderName, deep)
  {
    for each (let child in fixIterator(folder.subFolders, Components.interfaces.nsIMsgFolder)) {
      // if the folder selection is based on a string propery, use that
      if (aFolderName == getSmartFolderName(child)) {
        folders.push(child);
        // Add sub-folders if requested.
        if (deep)
          this.addSubFolders(child, folders);
      }
      else
        // if this folder doesn't have a property set, check Its children
        this._subFoldersWithStringProperty(child, folders, aFolderName, deep);
    }
  },

  _allFoldersWithStringProperty: function ftv_getAllFoldersWithProperty(accounts, aFolderName, deep)
  {
    let folders = [];
    for each (let acct in accounts) {
      let folder = acct.incomingServer.rootFolder;
      this._subFoldersWithStringProperty(folder, folders, aFolderName, deep);
    }
    return folders;
  },

  _allFoldersWithFlag: function ftv_getAllFolders(accounts, aFolderFlag, deep)
  {
    let folders = [];
    for each (let acct in accounts) {
      let foldersWithFlag = acct.incomingServer.rootFolder.getFoldersWithFlags(aFolderFlag);
      if (foldersWithFlag.length > 0) {
        for each (let folderWithFlag in fixIterator(foldersWithFlag.enumerate(),
                                                Components.interfaces.nsIMsgFolder)) {
          folders.push(folderWithFlag);
          // Add sub-folders of Sent and Archive to the result.
          if (deep && (aFolderFlag & (nsMsgFolderFlags.SentMail | nsMsgFolderFlags.Archive)))
            this.addSubFolders(folderWithFlag, folders);
        }
      }
    }
    return folders;
  },

  /**
   * get folders by flag or property based on the value of flag
   */
  _allSmartFolders: function ftv_allSmartFolders(accounts, flag, folderName, deep) {
    return flag ?
      gFolderTreeView._allFoldersWithFlag(accounts, flag, deep) :
      gFolderTreeView._allFoldersWithStringProperty(accounts, folderName, deep);
  },

  /**
   * Add a smart folder for folders with the passed flag set. But if there's
   * only one folder with the flag set, just put it at the top level.
   *
   * @param map array to add folder item to.
   * @param accounts array of accounts.
   * @param smartRootFolder root folder of the smart folders server
   * @param flag folder flag to create smart folders for
   * @param folderName name to give smart folder
   * @param position optional place to put folder item in map. If not specified,
   *                 folder item will be appended at the end of map.
   * @returns The smart folder's ftvItem if one was added, null otherwise.
   */
  _addSmartFoldersForFlag: function ftv_addSFForFlag(map, accounts, smartRootFolder,
                                                     flag, folderName, position)
  {
    // If there's only one subFolder, just put it at the root.
    let subFolders = gFolderTreeView._allSmartFolders(accounts, flag, folderName, false);
    if (flag && subFolders.length == 1) {
      let folderItem = new ftvItem(subFolders[0]);
      folderItem._level = 0;
      if (flag & nsMsgFolderFlags.Inbox)
        folderItem.__defineGetter__("children", function() []);
      if (position == undefined)
        map.push(folderItem);
      else
        map[position] = folderItem;
      // No smart folder was added
      return null;
    }

    let smartFolder;
    try {
      let folderUri = smartRootFolder.URI + "/" + encodeURI(folderName);
      smartFolder = smartRootFolder.getChildWithURI(folderUri, false, true);
    } catch (ex) {
        smartFolder = null;
    };
    if (!smartFolder) {
      let searchFolders = gFolderTreeView._allSmartFolders(accounts, flag, folderName, true);
      let searchFolderURIs = "";
      for each (let searchFolder in searchFolders) {
        if (searchFolderURIs.length)
          searchFolderURIs += '|';
        searchFolderURIs +=  searchFolder.URI;
      }
      if (!searchFolderURIs.length)
        return null;
      smartFolder = gFolderTreeView._createVFFolder(folderName, smartRootFolder,
                                                    searchFolderURIs, flag);
    }

    let smartFolderItem = new ftvItem(smartFolder);
    smartFolderItem._level = 0;
    if (position == undefined)
      map.push(smartFolderItem);
    else
      map[position] = smartFolderItem;
    // Add the actual special folders as sub-folders of the saved search.
    // By setting _children directly, we bypass the normal calculation
    // of subfolders.
    smartFolderItem._children = [new ftvItem(f) for each (f in subFolders)];

    let prevChild = null;
    // Each child is a level one below the smartFolder
    for each (let child in smartFolderItem._children) {
      child._level = smartFolderItem._level + 1;
      child._parent = smartFolderItem;
      // don't show sub-folders of the inbox, but I think Archives/Sent, etc
      // should have the sub-folders.
      if (flag & nsMsgFolderFlags.Inbox)
        child.__defineGetter__("children", function() []);
      // If we have consecutive children with the same server, then both
      // should display as folder - server.
      if (prevChild && (child._folder.server == prevChild._folder.server)) {
        child.addServerName = true;
        prevChild.addServerName = true;
        prevChild.useServerNameOnly = false;
      }
      else if (flag)
        child.useServerNameOnly = true;
      else
        child.addServerName = true;
      prevChild = child;
    }
    // new custom folders from addons may contain lots of children, sort them
    if (flag == 0)
      sortFolderItems(smartFolderItem._children);
    return smartFolderItem;
  },
  _createVFFolder: function ftv_createVFFolder(newName, parentFolder,
                                               searchFolderURIs, folderFlag)
  {
    let newFolder;
    try {
      if (parentFolder instanceof(Components.interfaces.nsIMsgLocalMailFolder))
        newFolder = parentFolder.createLocalSubfolder(newName);
      else
        newFolder = parentFolder.addSubfolder(newName);
      newFolder.setFlag(nsMsgFolderFlags.Virtual);
      // provide a way to make the top level folder just a container, not
      // a search folder
      let type = this._modes["smart"].getSmartFolderTypeByName(newName);
      if (type[3]) { // isSearch
        let vfdb = newFolder.msgDatabase;
        let dbFolderInfo = vfdb.dBFolderInfo;
        // set the view string as a property of the db folder info
        // set the original folder name as well.
        dbFolderInfo.setCharProperty("searchStr", "ALL");
        dbFolderInfo.setCharProperty("searchFolderUri", searchFolderURIs);
        dbFolderInfo.setUint32Property("searchFolderFlag", folderFlag);
        dbFolderInfo.setBooleanProperty("searchOnline", true);
        vfdb.summaryValid = true;
        vfdb.Close(true);
      }
      parentFolder.NotifyItemAdded(newFolder);
      Components.classes["@mozilla.org/messenger/account-manager;1"]
        .getService(Components.interfaces.nsIMsgAccountManager)
        .saveVirtualFolders();
    }
    catch(e) {
       throw(e);
       dump ("Exception : creating virtual folder \n");
    }
    return newFolder;
  },

  // We don't implement any of these at the moment
  performAction: function ftv_performAction(aAction) {},
  performActionOnCell: function ftv_performActionOnCell(aAction, aRow, aCol) {},
  performActionOnRow: function ftv_performActionOnRow(aAction, aRow) {},
  selectionChanged: function ftv_selectionChanged() {},
  setCellText: function ftv_setCellText(aRow, aCol, aValue) {},
  setCellValue: function ftv_setCellValue(aRow, aCol, aValue) {},
  getCellValue: function ftv_getCellValue(aRow, aCol) {},
  getColumnProperties: function ftv_getColumnProperties(aCol, aProps) {},
  getImageSrc: function ftv_getImageSrc(aRow, aCol) {},
  getProgressMode: function ftv_getProgressMode(aRow, aCol) {},
  cycleCell: function ftv_cycleCell(aRow, aCol) {},
  cycleHeader: function ftv_cycleHeader(aCol) {},

  // ****************** End of nsITreeView implementation **************** //

  //
  // WARNING: Everything below this point is considered private.  Touch at your
  //          own risk.

  /**
   * This is an array of all possible modes for the folder tree. You should not
   * modify this directly, but rather use registerFolderTreeMode.
   */
  _modeNames: ["all", "unread", "favorite", "recent", "smart"],
  _modeDisplayNames: {},

  /**
   * This is a javaascript map of which folders we had open, so that we can
   * persist their state over-time.  It is designed to be used as a JSON object.
   */
  _persistOpenMap: {},

  _restoreOpenStates: function ftv__persistOpenStates() {
    if (!(this.mode in this._persistOpenMap))
      return;

    let curLevel = 0;
    let tree = this;
    function openLevel() {
      let goOn = false;
      // We can't use a js iterator because we're changing the array as we go.
      // So fallback on old trick of going backwards from the end, which
      // doesn't care when you add things at the end.
      for (let i = tree._rowMap.length - 1; i >= 0; i--) {
        let row = tree._rowMap[i];
        if (row.level != curLevel)
          continue;

        let map = tree._persistOpenMap[tree.mode];
        if (map && map.indexOf(row.id) != -1) {
          tree._toggleRow(i, false);
          goOn = true;
        }
      }

      // If we opened up any new kids, we need to check their level as well.
      curLevel++;
      if (goOn)
        openLevel();
    }
    openLevel();
  },

  _tree: null,
  selection: null,
  /**
   * An array of ftvItems, where each item corresponds to a row in the tree
   */
  _rowMap: null,

  /**
   * Completely discards the current tree and rebuilds it based on current
   * settings
   */
  _rebuild: function ftv__rebuild() {
    let newRowMap;
    try {
      newRowMap = this._modes[this.mode].generateMap(this);
    } catch(ex) {
      Components.classes["@mozilla.org/consoleservice;1"]
                .getService(Components.interfaces.nsIConsoleService)
                .logStringMessage("generator " + this.mode + " failed with exception: " + ex);
      this.mode = "all";
      newRowMap = this._modes[this.mode].generateMap(this);
    }
    let selectedFolders = this.getSelectedFolders();
    if (this.selection)
      this.selection.clearSelection();
    // There's a chance the call to the map generator altered this._rowMap, so
    // evaluate oldCount after calling it rather than before
    let oldCount = this._rowMap ? this._rowMap.length : null;
    this._rowMap = newRowMap;

    let evt = document.createEvent("Events");
    evt.initEvent("mapRebuild", true, false);
    this._treeElement.dispatchEvent(evt);

    if (this._tree)
    {
      if (oldCount !== null)
          this._tree.rowCountChanged(0, this._rowMap.length - oldCount);
      this._tree.invalidate();
    }
    this._restoreOpenStates();
    // restore selection.
    for (let [, folder] in Iterator(selectedFolders)) {
      if (folder) {
        let index = this.getIndexOfFolder(folder);
        if (index != null)
          this.selection.toggleSelect(index);
      }
    }
  },

  _sortedAccounts: function ftv_getSortedAccounts()
  {
      const Cc = Components.classes;
      const Ci = Components.interfaces;
      let acctMgr = Cc["@mozilla.org/messenger/account-manager;1"]
                       .getService(Ci.nsIMsgAccountManager);
      let accounts = [a for each
                      (a in fixIterator(acctMgr.accounts, Ci.nsIMsgAccount))];
      // Bug 41133 workaround
      accounts = accounts.filter(function fix(a) { return a.incomingServer; });

      // Don't show deferred pop accounts
      accounts = accounts.filter(function isNotDeferred(a) {
        let server = a.incomingServer;
        return !(server instanceof Ci.nsIPop3IncomingServer &&
                 server.deferredToAccount);
      });

      // Don't show IM accounts
      accounts = accounts.filter(function(a) a.incomingServer.type != "im");

      function sortAccounts(a, b) {
        if (a.key == acctMgr.defaultAccount.key)
          return -1;
        if (b.key == acctMgr.defaultAccount.key)
          return 1;
        let aIsNews = a.incomingServer.type == "nntp";
        let bIsNews = b.incomingServer.type == "nntp";
        if (aIsNews && !bIsNews)
          return 1;
        if (bIsNews && !aIsNews)
          return -1;

        let aIsLocal = a.incomingServer.type == "none";
        let bIsLocal = b.incomingServer.type == "none";
        if (aIsLocal && !bIsLocal)
          return 1;
        if (bIsLocal && !aIsLocal)
          return -1;
        return 0;
      }
      accounts.sort(sortAccounts);
      return accounts;
  },

  /**
   * Contains the set of modes registered with the folder tree, initially those
   * included by default. This is a map from names of modes to their
   * implementations of |IFolderTreeMode|.
   */
  _modes: {
    /**
     * The all mode returns all folders, arranged in a hierarchy
     */
    all: {
      __proto__: IFolderTreeMode,

      generateMap: function ftv_all_generateMap(ftv) {
        let accounts = gFolderTreeView._sortedAccounts();
        // force each root folder to do its local subfolder discovery.
        MailUtils.discoverFolders();

        return [new ftvItem(acct.incomingServer.rootFolder)
                for each (acct in accounts)];
      }
    },

    /**
     * The unread mode returns all folders that are not root-folders and that
     * have unread items.  Also always keep the currently selected folder
     * so it doesn't disappear under the user.
     */
    unread: {
      __proto__: IFolderTreeMode,

      generateMap: function ftv_unread_generateMap(ftv) {
        let map = [];
        let currentFolder = gFolderTreeView.getSelectedFolders()[0];
        const outFolderFlagMask = nsMsgFolderFlags.SentMail |
          nsMsgFolderFlags.Drafts | nsMsgFolderFlags.Queue |
          nsMsgFolderFlags.Templates;
        for each (let folder in ftv._enumerateFolders) {
          if (!folder.isSpecialFolder(outFolderFlagMask, true) &&
              (!folder.isServer && folder.getNumUnread(false) > 0) ||
              (folder == currentFolder))
            map.push(new ftvItem(folder));
        }

        // There are no children in this view!
        for each (let folder in map) {
          folder.__defineGetter__("children", function() []);
          folder.addServerName = true;
        }
        sortFolderItems(map);
        return map;
      },

      getParentOfFolder: function ftv_unread_getParentOfFolder(aFolder) {
        // This is a flat view, so no folders have parents.
        return null;
      }
    },

    /**
     * The favorites mode returns all folders whose flags are set to include
     * the favorite flag
     */
    favorite: {
      __proto__: IFolderTreeMode,

      generateMap: function ftv_favorite_generateMap(ftv) {
        let faves = [];
        for each (let folder in ftv._enumerateFolders) {
          if (folder.flags & nsMsgFolderFlags.Favorite)
            faves.push(new ftvItem(folder));
        }

        // There are no children in this view!
        // And we want to display the account name to distinguish folders w/
        // the same name. (only for folders with duplicated names)
        let uniqueNames = new Object();
        for each (let item in faves) {
          let name = item._folder.abbreviatedName.toLowerCase();
          item.__defineGetter__("children", function() []);
          if (!uniqueNames[name])
            uniqueNames[name] = 0;
          uniqueNames[name]++;
        }
        for each (let item in faves) {
          let name = item._folder.abbreviatedName.toLowerCase();
          item.addServerName = (uniqueNames[name] > 1) ? true : false;
        }
        sortFolderItems(faves);
        return faves;
      },

      getParentOfFolder: function ftv_unread_getParentOfFolder(aFolder) {
        // This is a flat view, so no folders have parents.
        return null;
      }
    },

    recent: {
      __proto__: IFolderTreeMode,

      generateMap: function ftv_recent_generateMap(ftv) {
        const MAXRECENT = 15;

        /**
         * Sorts our folders by their recent-times.
         */
        function sorter(a, b) {
          return Number(a.getStringProperty("MRUTime")) <
            Number(b.getStringProperty("MRUTime"));
        }

        /**
         * This function will add a folder to the recentFolders array if it
         * is among the 15 most recent.  If we exceed 15 folders, it will pop
         * the oldest folder, ensuring that we end up with the right number
         *
         * @param aFolder the folder to check
         */
        let recentFolders = [];
        let oldestTime = 0;
        function addIfRecent(aFolder) {
          let time;
          try {
            time = Number(aFolder.getStringProperty("MRUTime")) || 0;
          } catch (ex) {return;}
          if (time <= oldestTime)
            return;

          if (recentFolders.length == MAXRECENT) {
            recentFolders.sort(sorter);
            recentFolders.pop();
            let oldestFolder = recentFolders[recentFolders.length - 1];
            oldestTime = Number(oldestFolder.getStringProperty("MRUTime"));
          }
          recentFolders.push(aFolder);
        }

        for each (let folder in ftv._enumerateFolders)
          addIfRecent(folder);

        // Sort the folder names alphabetically.
        recentFolders.sort(function rf_sort(a, b){
          var aLabel = a.prettyName;
          var bLabel = b.prettyName;
          if (aLabel == bLabel) {
            aLabel = a.server.prettyName;
            bLabel = b.server.prettyName;
          }
          return aLabel.toLocaleLowerCase() > bLabel.toLocaleLowerCase();
        });

        let items = [new ftvItem(f) for each (f in recentFolders)];

        // There are no children in this view!
        // And we want to display the account name to distinguish folders w/
        // the same name.
        for each (let folder in items) {
          folder.__defineGetter__("children", function() []);
          folder.addServerName = true;
        }

        return items;
      },

      getParentOfFolder: function ftv_unread_getParentOfFolder(aFolder) {
        // This is a flat view, so no folders have parents.
        return null;
      }
    },

    /**
     * The smart folder mode combines special folders of a particular type
     * across accounts into a single cross-folder saved search.
     */
    smart: {
      __proto__: IFolderTreeMode,

      /**
       * The smart server. This will create the server if it doesn't exist.
       */
      get _smartServer() {
        let acctMgr = Components.classes["@mozilla.org/messenger/account-manager;1"]
                                .getService(Components.interfaces.nsIMsgAccountManager);
        let smartServer;
        try {
          smartServer = acctMgr.FindServer("nobody", "smart mailboxes", "none");
        }
        catch (ex) {
          smartServer = acctMgr.createIncomingServer("nobody",
                                                     "smart mailboxes", "none");
          // We don't want the "smart" server/account leaking out into the ui in
          // other places, so set it as hidden.
          smartServer.hidden = true;
          let account = acctMgr.createAccount();
          account.incomingServer = smartServer;
        }
        delete this._smartServer;
        return this._smartServer = smartServer;
      },

      /**
       * A list of [flag, name, isDeep, isSearch] for smart folders. isDeep ==
       * false means that subfolders are displayed as subfolders of the account,
       * not of the smart folder. This list is expected to be constant through a
       * session.
       */
      _flagNameList: [
        [nsMsgFolderFlags.Inbox, "Inbox", false, true],
        [nsMsgFolderFlags.Drafts, "Drafts", false, true],
        [nsMsgFolderFlags.SentMail, "Sent", true, true],
        [nsMsgFolderFlags.Trash, "Trash", true, true],
        [nsMsgFolderFlags.Templates, "Templates", false, true],
        [nsMsgFolderFlags.Archive, "Archives", true, true],
        [nsMsgFolderFlags.Junk, "Junk", false, true],
        [nsMsgFolderFlags.Queue, "Outbox", true, true]
      ],
      // hard code, this could be build from _flagNameList dynamically
      _smartFlags: nsMsgFolderFlags.Inbox | nsMsgFolderFlags.Drafts |
                    nsMsgFolderFlags.Trash | nsMsgFolderFlags.SentMail |
                    nsMsgFolderFlags.Templates |
                    nsMsgFolderFlags.Junk |
                    nsMsgFolderFlags.Archive,

      /**
       * support for addons to add special folder types, this must be called
       * prior to onload.
       * 
       * @param aFolderName  name of the folder
       * @param isDeep  include subfolders
       * @param folderOptions  object with searchStr and searchOnline options, or null
       */
      addSmartFolderType: function ftv_addSmartFolderType(aFolderName, isDeep, isSearchFolder) {
        this._flagNameList.push([0, aFolderName, isDeep, isSearchFolder]);
      },

      /**
       * Returns a triple describing the smart folder if the given folder is a
       * special folder, else returns null.
       */
      getSmartFolderTypeByName: function ftv_smart__getSmartFolderType(aName) {
        for each (let [, type,,] in Iterator(this._flagNameList)) {
          if (type[1] == aName)
            return type;
        }
        return null;
      },
      /**
       * check to see if a folder is a smart folder
       */
      isSmartFolder: function ftv_smart__isSmartFolder(aFolder) {
        if (aFolder.flags & this._smartFlags)
            return true;
        // Also check the folder name itself, as containers do not
        // have the smartFolderName property.  We check all folders here, since
        // a "real" folder might be marked as a child of a smart folder.
        let smartFolderName = getSmartFolderName(aFolder);
        return smartFolderName && this.getSmartFolderTypeByName(smartFolderName) ||
            this.getSmartFolderTypeByName(aFolder.name);
      },

      /**
       * All the flags above, bitwise ORed.
       */
      get _allFlags() {
        delete this._allFlags;
        return this._allFlags = this._flagNameList.reduce(
          function (res, [flag,, isDeep,]) res | flag, 0);
      },

      /**
       * All the "shallow" flags above (isDeep set to false), bitwise ORed.
       */
      get _allShallowFlags() {
        delete this._allShallowFlags;
        return this._allShallowFlags = this._flagNameList.reduce(
          function (res, [flag,, isDeep,]) isDeep ? res : (res | flag), 0);
      },

      /**
       * Returns a triple describing the smart folder if the given folder is a
       * special folder, else returns null.
       */
      _getSmartFolderType: function ftv_smart__getSmartFolderType(aFolder) {
        let smartFolderName = getSmartFolderName(aFolder);
        for each (let [, type] in Iterator(this._flagNameList)) {
          if (smartFolderName) {
            if (type[1] == smartFolderName)
              return type;
            continue;
          }
          if (aFolder.flags & type[0])
            return type;
        }
        return null;
      },

      /**
       * Returns the smart folder with the given name.
       */
      _getSmartFolderNamed: function ftv_smart__getSmartFolderNamed(aName) {
        let smartRoot = this._smartServer.rootFolder;
        return smartRoot.getChildWithURI(smartRoot.URI + "/" + encodeURI(aName), false,
                                         true);
      },

      generateMap: function ftv_smart_generateMap(ftv) {
        let map = [];
        let acctMgr = Components.classes["@mozilla.org/messenger/account-manager;1"]
                                .getService(Components.interfaces.nsIMsgAccountManager);
        let accounts = gFolderTreeView._sortedAccounts();
        let smartServer = this._smartServer;
        smartServer.prettyName = document.getElementById("bundle_messenger")
                                         .getString("unifiedAccountName");
        smartServer.canHaveFilters = false;

        let smartRoot = smartServer.rootFolder;
        let smartChildren = [];
        for each (let [, [flag, name,,]] in Iterator(this._flagNameList)) {
          gFolderTreeView._addSmartFoldersForFlag(smartChildren, accounts,
                                                  smartRoot, flag, name);
        }

        sortFolderItems(smartChildren);
        for each (let smartChild in smartChildren)
          map.push(smartChild);

        MailUtils.discoverFolders();

        for each (let acct in accounts)
          map.push(new ftv_SmartItem(acct.incomingServer.rootFolder));

        return map;
      },

      /**
       * Returns the parent of a folder in the view.
       *
       * - The smart mailboxes are all top-level, so there's no parent.
       * - For one of the special folders, it is the smart folder of that kind
       *   if we're showing it (this happens when there's more than one folder
       *   of the kind). Otherwise it's a top-level folder, so there isn't a
       *   parent.
       * - For a child of a "shallow" special folder (see |_flagNameList| for
       *   the definition), it is the account.
       * - Otherwise it is simply the folder's actual parent.
       */
      getParentOfFolder: function ftv_smart_getParentOfFolder(aFolder) {
        let smartServer = this._smartServer;
        if (aFolder.server == smartServer)
          // This is a smart mailbox
          return null;

        let smartType = this._getSmartFolderType(aFolder);
        if (smartType) {
          // This is a special folder
          let [, name,] = smartType;
          let smartFolder = this._getSmartFolderNamed(name);
          if (smartFolder &&
              gFolderTreeView.getIndexOfFolder(smartFolder) != null)
            return smartFolder;

          return null;
        }

        let parent = aFolder.parent;
        if (parent && parent.isSpecialFolder(this._allShallowFlags, false)) {
          // Child of a shallow special folder
          return aFolder.server.rootFolder;
        }

        return parent;
      },

      /**
       * For a folder of a particular type foo, this returns the smart folder of
       * that type (if it's displayed). Otherwise this returns the folder the
       * message is in.
       */
      getFolderForMsgHdr: function ftv_smart_getFolderForMsgHdr(aMsgHdr) {
        let folder = aMsgHdr.folder;

        let smartType = this._getSmartFolderType(folder);
        if (smartType) {
          let [, name,] = smartType;
          let smartFolder = this._getSmartFolderNamed(name);
          if (smartFolder &&
              gFolderTreeView.getIndexOfFolder(smartFolder) != null)
            return smartFolder;
        }
        return folder;
      },

      /**
       * Handles the case of a new folder being added.
       *
       * - If a new special folder is added, we need to add it as a child of the
       *   corresponding smart folder.
       * - If the parent is a shallow special folder, we need to add it as a
       *   top-level folder in its account.
       * - Otherwise, we need to add it as a child of its parent (as normal).
       */
      onFolderAdded: function ftv_smart_onFolderAdded(aParent, aFolder) {
        if (aFolder.flags & this._allFlags) {
          // add as child of corresponding smart folder
          let smartServer = this._smartServer;
          let smartRoot = smartServer.rootFolder;
          // In theory, a folder can have multiple flags set, so we need to
          // check each flag separately.
          for each (let [, [flag, name,,]] in Iterator(this._flagNameList)) {
            if (aFolder.flags & flag)
              gFolderTreeView._addSmartSubFolder(aFolder, smartRoot, name,
                                                 flag);
          }
        }
        else if (aParent.isSpecialFolder(this._allShallowFlags, false)) {
          // add as a child of the account
          let rootIndex = gFolderTreeView.getIndexOfFolder(
            aFolder.server.rootFolder);
          let root = gFolderTreeView._rowMap[rootIndex];
          if (!root)
            return;

          let newChild = new ftv_SmartItem(aFolder);
          root.children.push(newChild);
          newChild._level = root._level + 1;
          newChild._parent = root;
          sortFolderItems(root._children);

          gFolderTreeView._addChildToView(root, rootIndex, newChild);
        }
        else {
          // add as normal
          gFolderTreeView.addFolder(aParent, aFolder);
        }
      }
    }
  },

  /**
   * This is a helper attribute that simply returns a flat list of all folders
   */
  get _enumerateFolders() {
    const Cc = Components.classes;
    const Ci = Components.interfaces;
    let folders = [];

    let acctMgr = Cc["@mozilla.org/messenger/account-manager;1"]
                     .getService(Ci.nsIMsgAccountManager);
    for each (let server in fixIterator(acctMgr.allServers, Ci.nsIMsgIncomingServer)) {
      // Skip deferred accounts
      if (server instanceof Ci.nsIPop3IncomingServer &&
          server.deferredToAccount)
        continue;

      let rootFolder = server.rootFolder;
      folders.push(rootFolder);
      this.addSubFolders(rootFolder, folders);
    }
    return folders;
  },

  /**
   * This is a recursive function to add all subfolders to the array. It
   * assumes that the passed in folder itself has already been added.
   *
   * @param aFolder  the folder whose subfolders should be added
   * @param folders  the array to add the folders to.
   */
  addSubFolders : function ftv_addSubFolders (folder, folders) {
    for each (let f in fixIterator(folder.subFolders, Components.interfaces.nsIMsgFolder)) {
      folders.push(f);
      this.addSubFolders(f, folders);
    }
  },

  /**
   * This updates the rowmap and invalidates the right row(s) in the tree
   */
  _addChildToView: function ftl_addChildToView(aParent, aParentIndex, aNewChild) {
    if (aParent.open) {
      let newChildIndex;
      let newChildNum = aParent._children.indexOf(aNewChild);
      // only child - go right after our parent
      if (newChildNum == 0) {
        newChildIndex = Number(aParentIndex) + 1
      }
      // if we're not the last child, insert ourselves before the next child.
      else if (newChildNum < aParent._children.length - 1) {
        newChildIndex = this.getIndexOfFolder(aParent._children[Number(newChildNum) + 1]._folder);
      }
      // otherwise, go after the last child
      else {
        let lastChild = aParent._children[newChildNum - 1];
        let lastChildIndex = this.getIndexOfFolder(lastChild._folder);
        newChildIndex = Number(lastChildIndex) + 1;
        while (newChildIndex < this.rowCount &&
               this._rowMap[newChildIndex].level > this._rowMap[lastChildIndex].level)
          newChildIndex++;
      }
      this._rowMap.splice(newChildIndex, 0, aNewChild);
      this._tree.rowCountChanged(newChildIndex, 1);
    } else {
      this._tree.invalidateRow(aParentIndex);
    }
  },
  _addSmartSubFolder: function ftl_addSmartSubFolder(aItem, aSmartRoot, aName, aFlag) {
    let smartFolder = aSmartRoot.getChildWithURI(aSmartRoot.URI + "/" + encodeURI(aName),
                                                 false, true);
    let parent = null;
    let parentIndex = -1;
    let newChild;
    let newChildIndex = 0;
    if (!smartFolder || this.getIndexOfFolder(smartFolder) == null) {
      newChild = new ftv_SmartItem(aItem);
      newChild._level = 0;
      while (newChildIndex < this.rowCount) {
        if (this._rowMap[newChildIndex]._folder.flags & aFlag) {
          // This type of folder seems to already exist, so replace the row
          // with a smartFolder.
          this._addSmartFoldersForFlag(this._rowMap, this._sortedAccounts(),
                                       aSmartRoot, aFlag, aName, newChildIndex);
          return;
        }
        if (this._rowMap[newChildIndex]._folder.isServer)
          break;
        newChildIndex++;
      }
    } else {
      parentIndex = this.getIndexOfFolder(smartFolder);
      parent = this._rowMap[parentIndex];
      if (!parent)
         return;

      newChild = new ftv_SmartItem(aItem);
      parent.children.push(newChild);
      newChild._level = parent._level + 1;
      newChild._parent = parent;
      sortFolderItems(parent._children);
      newChild.useServerNameOnly = true;
    }
    if (aItem.flags & nsMsgFolderFlags.Inbox)
      newChild.__defineGetter__("children", function() []);
    if (parent)
      this._addChildToView(parent, parentIndex, newChild);
    else {
      this._rowMap.splice(newChildIndex, 0, newChild);
      this._tree.rowCountChanged(newChildIndex, 1);
    }
  },
  /**
   * This is our implementation of nsIMsgFolderListener to watch for changes
   */
  OnItemAdded: function ftl_add(aParentItem, aItem) {
    // Ignore this item if it's not a folder, or we knew about it.
    if (!(aItem instanceof Components.interfaces.nsIMsgFolder) ||
        this.getIndexOfFolder(aItem) != null)
      return;

    // if no parent, this is an account, so let's rebuild.
    if (!aParentItem) {
      if (!aItem.server.hidden) // ignore hidden server items
        this._rebuild();
      return;
    }
    this._modes[this._mode].onFolderAdded(
      aParentItem.QueryInterface(Components.interfaces.nsIMsgFolder), aItem);
  },
  addFolder: function ftl_add_folder(aParentItem, aItem)
  {
    let parentIndex = this.getIndexOfFolder(aParentItem);
    let parent = this._rowMap[parentIndex];
    if (!parent)
       return;

    // Getting these children might have triggered our parent to build its
    // array just now, in which case the added item will already exist
    let children = parent.children;
    var newChild;
    for each (let child in children) {
      if (child._folder == aItem) {
        newChild = child;
        break;
      }
    }
    if (!newChild) {
      newChild = new ftvItem(aItem);
      parent.children.push(newChild);
      newChild._level = parent._level + 1;
      newChild._parent = parent;
      sortFolderItems(parent._children);
    }
    // If the parent is open, add the new child into the folder pane.
    // Otherwise, just invalidate the parent row. Note that this code doesn't
    // get called for the smart folder case.
    if (!parent.open) {
      // Special case adding a special folder when the parent is collapsed.
      // Expand the parent so the user can see the special child.
      // Expanding the parent is sufficient to add the folder to the view,
      // because either we knew about it, or we will have added a child item
      // for it above.
      if (newChild._folder.flags & nsMsgFolderFlags.SpecialUse) {
        this._toggleRow(parentIndex, false);
        return;
      }
    }
    this._addChildToView(parent, parentIndex, newChild);
  },

  OnItemRemoved: function ftl_remove(aRDFParentItem, aItem) {
    if (!(aItem instanceof Components.interfaces.nsIMsgFolder))
      return;

    let persistMapIndex = this._persistOpenMap[this.mode].indexOf(aItem.URI);
    if (persistMapIndex != -1)
      this._persistOpenMap[this.mode].splice(persistMapIndex, 1);

    let index = this.getIndexOfFolder(aItem);
    if (index == null)
      return;
    // forget our parent's children; they'll get rebuilt
    if (aRDFParentItem)
      this._rowMap[index]._parent._children = null;
    let kidCount = 1;
    let walker = Number(index) + 1;
    while (walker < this.rowCount &&
           this._rowMap[walker].level > this._rowMap[index].level) {
      walker++;
      kidCount++;
    }
    this._rowMap.splice(index, kidCount);
    this._tree.rowCountChanged(index, -1 * kidCount);
    this._tree.invalidateRow(index);
  },

  OnItemPropertyChanged: function(aItem, aProperty, aOld, aNew) {},
  OnItemIntPropertyChanged: function(aItem, aProperty, aOld, aNew) {
    // we want to rebuild only if we're in unread mode, and we have a
    // newly unread folder, and we didn't already have the folder.
    if (this._mode == "unread" &&
        aProperty == "TotalUnreadMessages" && aOld == 0 &&
        this.getIndexOfFolder(aItem) == null) {
      this._rebuild();
      return;
    }

    if (aItem instanceof Components.interfaces.nsIMsgFolder) {
      let index = this.getIndexOfFolder(aItem);
      let folder = aItem;
      let folderTreeMode = this._modes[this._mode];
      // look for first visible ancestor
      while (index == null) {
        folder = folderTreeMode.getParentOfFolder(folder);
        if (!folder)
          break;
        index = this.getIndexOfFolder(folder);
      }
      if (index != null)
        this._tree.invalidateRow(index);
    }
  },

  OnItemBoolPropertyChanged: function(aItem, aProperty, aOld, aNew) {
    let index = this.getIndexOfFolder(aItem);
    if (index != null)
      this._tree.invalidateRow(index);
  },
  OnItemUnicharPropertyChanged: function(aItem, aProperty, aOld, aNew) {},
  OnItemPropertyFlagChanged: function(aItem, aProperty, aOld, aNew) {},
  OnItemEvent: function(aFolder, aEvent) {
    let index = this.getIndexOfFolder(aFolder);
    if (index != null)
      this._tree.invalidateRow(index);
  }
};

/**
 * The ftvItem object represents a single row in the tree view. Because I'm lazy
 * I'm just going to define the expected interface here.  You are free to return
 * an alternative object in a _mapGenerator, provided that it matches this
 * interface:
 *
 * id (attribute) - a unique string for this object. Must persist over sessions
 * text (attribute) - the text to display in the tree
 * level (attribute) - the level in the tree to display the item at
 * open (rw, attribute) - whether or not this container is open
 * children (attribute) - an array of child items also conforming to this spec
 * getProperties (function) - a call from getRowProperties or getCellProperties
 *                            for this item will be passed into this function
 * command (function) - this function will be called when the item is double-
 *                      clicked
 */
function ftvItem(aFolder) {
  this._folder = aFolder;
  this._level = 0;
}

ftvItem.prototype = {
  open: false,
  addServerName: false,
  useServerNameOnly: false,

  get id() {
    return this._folder.URI;
  },
  get text() {
    let text;
    if (this.useServerNameOnly) {
      text = this._folder.server.prettyName;
    }
    else {
      text = this._folder.abbreviatedName;
      if (this.addServerName)
        text += " - " + this._folder.server.prettyName;
    }
    // Yeah, we hard-code this, but so did the old code...
    let unread = this._folder.getNumUnread(false);
    if (unread > 0)
      text += " (" + unread + ")";
    return text;
  },

  get level() {
    return this._level;
  },

  getProperties: function ftvItem_getProperties(aProps) {
    // From folderUtils.jsm
    setPropertyAtoms(this._folder, aProps);
    if (this._folder.flags & nsMsgFolderFlags.Virtual) {
        aProps.AppendElement(Components.classes["@mozilla.org/atom-service;1"]
                             .getService(Components.interfaces.nsIAtomService)
                             .getAtom("specialFolder-Smart"));
        // a second possibility for customized smart folders
        aProps.AppendElement(Components.classes["@mozilla.org/atom-service;1"]
                             .getService(Components.interfaces.nsIAtomService)
                             .getAtom("specialFolder-"+this._folder.name.replace(' ','')));
    }
    // if there is a smartFolder name property, add it
    let smartFolderName = getSmartFolderName(this._folder);
    if (smartFolderName) {
      aProps.AppendElement(Components.classes["@mozilla.org/atom-service;1"]
                         .getService(Components.interfaces.nsIAtomService)
                         .getAtom("specialFolder-"+smartFolderName.replace(' ','')));
    }
  },

  command: function fti_command() {
    let pref = Components.classes["@mozilla.org/preferences-service;1"]
                         .getService(Components.interfaces.nsIPrefBranch);
    if (!pref.getBoolPref("mailnews.reuse_thread_window2"))
      MsgOpenNewWindowForFolder(this._folder.URI, -1 /* key */);
  },

  _children: null,
  get children() {
    const Ci = Components.interfaces;
    // We're caching our child list to save perf.
    if (!this._children) {
      let iter;
      try {
        iter = fixIterator(this._folder.subFolders, Ci.nsIMsgFolder);
      } catch (ex) {
        Components.classes["@mozilla.org/consoleservice;1"].getService(Ci.nsIConsoleService)
          .logStringMessage("Discovering children for " + this._folder.URI + " failed with " +
                            "exception: " + ex);
        iter = [];
      }
      this._children = [new ftvItem(f) for each (f in iter)];

      sortFolderItems(this._children);
      // Each child is a level one below us
      for each (let child in this._children) {
        child._level = this._level + 1;
        child._parent = this;
      }
    }
    return this._children;
  }
};

/**
 * This handles the invocation of most commmands dealing with folders, based off
 * of the current selection, or a passed in folder.
 */
let gFolderTreeController = {
  /**
   * Opens the dialog to create a new sub-folder, and creates it if the user
   * accepts
   *
   * @param aParent (optional)  the parent for the new subfolder
   */
  newFolder: function ftc_newFolder(aParent) {
    let folder = aParent || gFolderTreeView.getSelectedFolders()[0];

    // Make sure we actually can create subfolders
    if (!folder.canCreateSubfolders) {
      // Check if we can create them at the root
      let rootMsgFolder = folder.server.rootMsgFolder;
      if (rootMsgFolder.canCreateSubfolders)
        folder = rootMsgFolder;
      else // just use the default account
        folder = GetDefaultAccountRootFolder();
    }

    let dualUseFolders = true;
    if (folder.server instanceof Components.interfaces.nsIImapIncomingServer)
      dualUseFolders = folder.server.dualUseFolders;

    function newFolderCallback(aName, aFolder) {
      if (aName)
        aFolder.createSubfolder(aName, msgWindow);
    }

    window.openDialog("chrome://messenger/content/newFolderDialog.xul",
                      "",
                      "chrome,modal,resizable=no,centerscreen",
                      {folder: folder, dualUseFolders: dualUseFolders,
                       okCallback: newFolderCallback});
  },

  /**
   * Opens the dialog to edit the properties for a folder
   *
   * @param aTabID  (optional) the tab to show in the dialog
   * @param aFolder (optional) the folder to edit, if not the selected one
   */
  editFolder: function ftc_editFolder(aTabID, aFolder) {
    let folder = aFolder || gFolderTreeView.getSelectedFolders()[0];

    // If this is actually a server, send it off to that controller
    if (folder.isServer) {
      MsgAccountManager(null);
      return;
    }

    if (folder.flags & nsMsgFolderFlags.Virtual) {
      this.editVirtualFolder(folder);
      return;
    }

    let title = document.getElementById("bundle_messenger")
                        .getString("folderProperties");

    //xxx useless param
    function editFolderCallback(aNewName, aOldName, aUri) {
      if (aNewName != aOldName)
        folder.rename(aNewName, msgWindow);
    }

    //xxx useless param
    function rebuildSummary(aFolder) {
      // folder is already introduced in our containing function and is
      //  lexically captured and available to us.
      if (folder.locked) {
        folder.throwAlertMsg("operationFailedFolderBusy", msgWindow);
        return;
      }
      if (folder.supportsOffline) {
        // Remove the offline store, if any.
        let offlineStore = folder.filePath;
        if (offlineStore.exists())
          offlineStore.remove(false);
      }
      gFolderDisplay.view.close();

      // Send a notification that we are triggering a database rebuild.
      let notifier =
        Components.classes["@mozilla.org/messenger/msgnotificationservice;1"]
                  .getService(
                    Components.interfaces.nsIMsgFolderNotificationService);
      notifier.notifyItemEvent(folder, "FolderReindexTriggered", null);

      folder.msgDatabase.summaryValid = false;

      var msgDB = folder.msgDatabase;
      msgDB.summaryValid = false;
      try {
        folder.closeAndBackupFolderDB("");
      }
      catch(e) {
        // In a failure, proceed anyway since we're dealing with problems
        folder.ForceDBClosed();
      }
      folder.updateFolder(msgWindow);
      gFolderDisplay.show(folder);
    }

    window.openDialog("chrome://messenger/content/folderProps.xul",
                      "",
                      "chrome,modal,centerscreen",
                      {folder: folder, serverType: folder.server.type,
                       msgWindow: msgWindow, title: title,
                       okCallback: editFolderCallback,
                       tabID: aTabID, name: folder.prettyName,
                       rebuildSummaryCallback: rebuildSummary});
  },

  /**
   * Opens the dialog to rename a particular folder, and does the renaming if
   * the user clicks OK in that dialog
   *
   * @param aFolder (optional)  the folder to rename, if different than the
   *                            currently selected one
   */
  renameFolder: function ftc_rename(aFolder) {
    let folder = aFolder || gFolderTreeView.getSelectedFolders()[0];

    //xxx no need for uri now
    let controller = this;
    function renameCallback(aName, aUri) {
      if (aUri != folder.URI)
        Components.utils.reportError("got back a different folder to rename!");

      controller._tree.view.selection.clearSelection();

      // Actually do the rename
      folder.rename(aName, msgWindow);
    }
    window.openDialog("chrome://messenger/content/renameFolderDialog.xul",
                      "",
                      "chrome,modal,centerscreen",
                      {preselectedURI: folder.URI,
                       okCallback: renameCallback, name: folder.prettyName});
  },

  /**
   * Deletes a folder from its parent. Also handles unsubscribe from newsgroups
   * if the selected folder/s happen to be nntp.
   *
   * @param aFolder (optional) the folder to delete, if not the selected one
   */
  deleteFolder: function ftc_delete(aFolder) {
    const Ci = Components.interfaces;
    let folders = aFolder ? [aFolder] : gFolderTreeView.getSelectedFolders();
    let folder = folders[0];

    // For newsgroups, "delete" means "unsubscribe".
    if (folder.server.type == "nntp") {
      MsgUnsubscribe(folders);
      return;
    }

    var canDelete = (folder.isSpecialFolder(nsMsgFolderFlags.Junk, false)) ?
      CanRenameDeleteJunkMail(folder.URI) : folder.deletable;

    if (!canDelete)
      throw new Error("Can't delete folder: " + folder.name);

    if (folder.flags & nsMsgFolderFlags.Virtual) {
      let confirmation = document.getElementById("bundle_messenger")
                                 .getString("confirmSavedSearchDeleteMessage");
      let title = document.getElementById("bundle_messenger")
                          .getString("confirmSavedSearchTitle");
      let IPS = Components.interfaces.nsIPromptService;
      if (Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
            .getService(IPS)
            .confirmEx(window, title, confirmation, IPS.STD_YES_NO_BUTTONS + IPS.BUTTON_POS_1_DEFAULT,
                       "", "", "", "", {}) != 0) /* the yes button is in position 0 */
        return;
    }

    let array = toXPCOMArray([folder], Ci.nsIMutableArray);
    folder.parent.deleteSubFolders(array, msgWindow);
  },

  /**
   * Prompts the user to confirm and empties the trash for the selected folder
   *
   * @param aFolder (optional)  the trash folder to empty
   * @note Calling this function on a non-trash folder will result in strange
   *       behavior!
   */
  emptyTrash: function ftc_emptyTrash(aFolder) {
    let folder = aFolder || gFolderTreeView.getSelectedFolders()[0];

    if (this._checkConfirmationPrompt("emptyTrash")) {
      // Check if this is a top-level smart folder. If so, we're going
      // to empty all the trash folders.
      if (folder.server.hostName == "smart mailboxes" &&
          folder.parent.isServer) {
        let subFolders = gFolderTreeView
                           ._allFoldersWithFlag(gFolderTreeView._sortedAccounts(),
                            nsMsgFolderFlags.Trash, false);
        for each (let trash in subFolders)
          trash.emptyTrash(msgWindow, null);
      }
      else {
        folder.emptyTrash(msgWindow, null);
      }
    }
  },

  /**
   * Deletes everything (folders and messages) in this folder
   *
   * @param aFolder (optional)  the folder to empty
   */
  emptyJunk: function ftc_emptyJunk(aFolder) {
    const Ci = Components.interfaces;
    let folder = aFolder || gFolderTreeView.getSelectedFolders()[0];

    if (!this._checkConfirmationPrompt("emptyJunk"))
      return;

    // Delete any subfolders this folder might have
    let iter = folder.subFolders;
    while (iter.hasMoreElements())
      folder.propagateDelete(iter.getNext(), true, msgWindow);

    // Now delete the messages
    let iter = fixIterator(folder.messages);
    let messages = [m for each (m in iter)];
    let children = toXPCOMArray(messages, Ci.nsIMutableArray);
    folder.deleteMessages(children, msgWindow, true, false, null, false);
  },

  /**
   * Compacts either particular folder/s, or selected folders.
   *
   * @param aFolders (optional) the folders to compact, if different than the
   *                            currently selected ones
   */
  compactFolders: function ftc_compactFolders(aFolders) {
    let folders = aFolders || gFolderTreeView.getSelectedFolders();
    for (let i = 0; i < folders.length; i++) {
      // Can't compact folders that have just been compacted.
      if (folders[i].server.type != "imap" && !folders[i].expungedBytes)
        continue;

      folders[i].compact(null, msgWindow);
    }
  },

  /**
   * Compacts all folders for accounts that the given folders belong
   * to, or all folders for accounts of the currently selected folders.
   *
   * @param aFolders (optional) the folders for whose accounts we should compact
   *                            all folders, if different than the currently
   *                            selected ones
   */
  compactAllFoldersForAccount: function ftc_compactAllFoldersOfAccount(aFolders) {
    let folders = aFolders || gFolderTreeView.getSelectedFolders();
    for (let i = 0; i < folders.length; i++) {
      folders[i].compactAll(null, msgWindow, folders[i].server.type == "imap" ||
                                             folders[i].server.type == "nntp");
    }
  },

  /**
   * Opens the dialog to create a new virtual folder
   *
   * @param aName - the default name for the new folder
   * @param aSearchTerms - the search terms associated with the folder
   * @param aParent - the folder to run the search terms on
   */
  newVirtualFolder: function ftc_newVFolder(aName, aSearchTerms, aParent) {
    let folder = aParent || gFolderTreeView.getSelectedFolders()[0];
    if (!folder)
      folder = GetDefaultAccountRootFolder();

    let name = folder.prettyName;
    if (aName)
      name += "-" + aName;

    window.openDialog("chrome://messenger/content/virtualFolderProperties.xul",
                      "",
                      "chrome,modal,centerscreen",
                      {folder: folder, searchTerms: aSearchTerms,
                       newFolderName: name});
  },

  editVirtualFolder: function ftc_editVirtualFolder(aFolder) {
    let folder = aFolder || gFolderTreeView.getSelectedFolders()[0];

    //xxx should pass the folder object
    function editVirtualCallback(aURI) {
      // we need to reload the folder if it is the currently loaded folder...
      if (gFolderDisplay.displayedFolder &&
          aURI == gFolderDisplay.displayedFolder.URI)
        FolderPaneSelectionChange();
    }
    window.openDialog("chrome://messenger/content/virtualFolderProperties.xul",
                      "",
                      "chrome,modal,centerscreen",
                      {folder: folder, editExistingFolder: true,
                       onOKCallback: editVirtualCallback,
                       msgWindow: msgWindow});
  },

  /**
   * Opens a search window with the given folder, or the selected one if none
   * is given.
   *
   * @param [aFolder] the folder to open the search window for, if different
   *                  from the selected one
   */
  searchMessages: function ftc_searchMessages(aFolder) {
    MsgSearchMessages(aFolder || gFolderTreeView.getSelectedFolders()[0]);
  },

  /**
   * Prompts for confirmation, if the user hasn't already chosen the "don't ask
   * again" option.
   *
   * @param aCommand - the command to prompt for
   */
  _checkConfirmationPrompt: function ftc_confirm(aCommand) {
    const Cc = Components.classes;
    const Ci = Components.interfaces;
    let showPrompt = true;
    try {
      let pref = Cc["@mozilla.org/preferences-service;1"]
                    .getService(Ci.nsIPrefBranch);
      showPrompt = !pref.getBoolPref("mail." + aCommand + ".dontAskAgain");
    } catch (ex) {}

    if (showPrompt) {
      let checkbox = {value:false};
      let promptService = Cc["@mozilla.org/embedcomp/prompt-service;1"]
                             .getService(Ci.nsIPromptService);
      let bundle = document.getElementById("bundle_messenger");
      let ok = promptService.confirmEx(window,
                                       bundle.getString(aCommand + "Title"),
                                       bundle.getString(aCommand + "Message"),
                                       promptService.STD_YES_NO_BUTTONS,
                                       null, null, null,
                                       bundle.getString(aCommand + "DontAsk"),
                                       checkbox) == 0;
      if (checkbox.value)
        pref.setBoolPref("mail." + aCommand + ".dontAskAgain", true);
      if (!ok)
        return false;
    }
    return true;
  },

  get _tree() {
    let tree = document.getElementById("folderTree");
    delete this._tree;
    return this._tree = tree;
  }
};

function ftv_SmartItem(aFolder)
{
  ftvItem.call(this, aFolder);
  this._level = 0;
}

ftv_SmartItem.prototype =
{
  get children() {
    const Ci = Components.interfaces;
    let smartMode = gFolderTreeView.getFolderTreeMode('smart');

    // We're caching our child list to save perf.
    if (!this._children) {
      this._children = [];
      let iter = fixIterator(this._folder.subFolders, Ci.nsIMsgFolder);
      for (let folder in iter) {
        if (!smartMode.isSmartFolder(folder)) {
          this._children.push(new ftv_SmartItem(folder));
        }
        else if (folder.flags & nsMsgFolderFlags.Inbox) {
          let subIter = fixIterator(folder.subFolders, Ci.nsIMsgFolder);
          for (let subfolder in subIter) {
            if (!smartMode.isSmartFolder(subfolder))
              this._children.push(new ftv_SmartItem(subfolder));
          }
        }
      }
      sortFolderItems(this._children);
      // Each child is a level one below us
      for each (let child in this._children) {
        child._level = this._level + 1;
        child._parent = this;
      }
    }
    return this._children;
  }
}

extend(ftv_SmartItem, ftvItem);

/**
 * Sorts the passed in array of folder items using the folder sort key
 *
 * @param aFolders - the array of ftvItems to sort.
 */
function sortFolderItems (aFtvItems) {
  function sorter(a, b) {
    let sortKey = a._folder.compareSortKeys(b._folder);
    if (sortKey)
      return sortKey;
    return a.text.toLowerCase() > b.text.toLowerCase();
  }
  aFtvItems.sort(sorter);
}

function getSmartFolderName(aFolder) {
  try {
    return aFolder.getStringProperty("smartFolderName");
  } catch (ex) {
    Components.utils.reportError(ex);
    return null;
  }
}

/**
 * Create a subtype - maybe this wants to be in a shared .jsm file somewhere.
 */
function extend(child, supertype)
{
  child.prototype.__proto__ = supertype.prototype;
}
