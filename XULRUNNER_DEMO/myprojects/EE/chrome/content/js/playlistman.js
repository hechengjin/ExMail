/* Play list Manager class. Provide add/remove file list operation
   Created by Zoominla, 01-08-2009         
*/

if(typeof(XULPLAYER_PLAYLIST_MANAGER_INCLUDE) != "boolean") {
	XULPLAYER_PLAYLIST_MANAGER_INCLUDE = true;
    
    function CPlaylistMan(listManId)
    {
        this.element = document.getElementById(listManId);
		this.lists = [];
		this.doc = null;
		this.path = null;
		this.selIdx = -1;
		this.bRandom = $e('plModeShuffle').getAttribute('checked');
		this.bRepeat = $e('plModeRepeat').getAttribute('checked');
		this.dirty = false;
    }
    
	CPlaylistMan.prototype.initialize = function () {
		// Open listmgr.xml
		var listManPath = Io.getFileFrom(userDataPath);
		if(listManPath) {
			listManPath.append("playlists");
			if(listManPath && !listManPath.exists()) {
				listManPath.create(Components.interfaces.nsIFile.DIRECTORY_TYPE, 420);
			}
		}
		
		if(listManPath) {
			listManPath.append('listmgr.xml');
			this.path = listManPath.path;
			var xmlContent = null;
			if(listManPath.exists()) {
				var txtLineArr = Io.openUTF8File(listManPath);
				if(txtLineArr) {
					xmlContent = txtLineArr.join();	
				}
			}
			if(!xmlContent) {
				xmlContent = '<?xml version="1.0" encoding="UTF-8" ?> <playlists lastPlayIndex="-1"> </playlists>';
			}
			
			this.doc = new XMLDom();
			this.doc.async = false;
			this.doc.preserveWhiteSpace=false;
			this.doc.loadXML(xmlContent);
	
			// Add playlists from doc to list manager's list
			var rootNode = DomHelper.getChildNode(this.doc);
			// Get last play list index
			var lastPlayIndex = -1;
			var lastIdx = rootNode.getAttribute("lastPlayIndex");
			if(lastIdx) {
				lastPlayIndex = parseInt(lastIdx);
			}

			var child = DomHelper.getChildNode(rootNode);
			for(;child;child=DomHelper.getNextNode(child)) {
				var listName = child.getAttribute("key");
				var listPath = child.getAttribute("value");
				var listFile = Io.getFileFrom(listPath);
				if(!listFile || !listFile.exists()) continue;
				
				this.element.appendItem(listName, listPath);
				var alist = new CFileList(listPath);
				alist.initialize();
				alist.enableRandom(this.bRandom);
				alist.enableRepeat(this.bRepeat);
				this.lists.push(alist);
			}
			if(!this.isEmpty()) {
				if(lastPlayIndex >= 0) {
					this.selIdx = lastPlayIndex;
				} else {
					this.selIdx = this.lists.length - 1 ;
				}
				this.element.selectedIndex = this.selIdx;
				this.lists[this.selIdx].show();
			}
		}
	};
	
	CPlaylistMan.prototype.openList = function (listPath) {
		if(listPath.indexOf(CFileList.FILE_EXTENSION) >= 0) {	// Open .xpl file
            this.openNativeList(listPath);
        } else if(listPath.indexOf('.m3u') >= 0) {				// Open .m3u file
			this.openM3uList(listPath);
		}
	};
	
	CPlaylistMan.prototype.renameList = function (newName) {
		if(!newName) return;
		if(this.selIdx >= 0) {
			this.element.selectedItem.label = newName;
			// Modify the doc element
			var rootNode = DomHelper.getChildNode(this.doc);
			var leafNode = DomHelper.getChildNode(rootNode);
			for(i=0; i<this.selIdx; i++) {		
				leafNode=DomHelper.getNextNode(leafNode);
			}
			if(leafNode) leafNode.setAttribute("key", newName);
			this.dirty = true;
		}
	};
	
    CPlaylistMan.prototype.newList = function (listPath,sysflag) {
        // Check if file exists
		if(!listPath) return;
		var listDir = getFileDir(this.path);
		var dirObj = Io.getFileFrom(listDir);
		if(listPath.indexOf(CFileList.FILE_EXTENSION) < 0) {
            listPath += CFileList.FILE_EXTENSION;
        }
		dirObj.append(listPath);
		var newListPath = dirObj.path;
        if(dirObj && dirObj.exists() && newListPath.indexOf('default.xpl')<0) {
			var overwriteTipText = 'File already exists, would you overwrite it?';
			if(!confirm(overwriteTipText)) return;
		}
		var initXML = '<?xml version="1.0" encoding="UTF-8" ?> <queue> </queue>';
		Io.saveUTF8File(initXML, dirObj);
		this.openNativeList(newListPath,null,sysflag);
    };
	
	CPlaylistMan.prototype.removeList = function() {
        if(this.isEmpty()) return;
		var selIdx = this.element.selectedIndex;
		if(selIdx < 0) return;
		// Remove item from listmgr.xml and update it
		if(this.doc) {
			var rootNode = DomHelper.getChildNode(this.doc); 
			DomHelper.removeChildByIndex(rootNode, selIdx);
		}
		this.dirty = true;
		
		var newSelIdx = selIdx;
		if(selIdx == this.getLength()-1) {
			newSelIdx = selIdx-1;
		}
		
		// Remove playlist file
		this.lists[selIdx].clear();
		// Remove list manager item and update it
		this.element.removeItemAt(selIdx);
		Io.deleteFile(this.lists[selIdx].path);
		this.lists.splice(selIdx, 1);
		if(!this.isEmpty()) {
			if(newSelIdx < 0) newSelIdx = 0;
		}
		this.element.selectedIndex = newSelIdx;
		if(newSelIdx >= 0) {
			this.lists[newSelIdx].show();
		}
		this.selIdx = newSelIdx;
    };
	
	CPlaylistMan.prototype.enableRandom = function() {
		this.bRandom = $e('plModeShuffle').getAttribute('checked');
		for(var i=0; i<this.lists.length; ++i) {
			this.lists[i].enableRandom(this.bRandom);
		}
	};
	
	CPlaylistMan.prototype.enableRepeat = function() {
		this.bRepeat = $e('plModeRepeat').getAttribute('checked');
		for(var i=0; i<this.lists.length; ++i) {
			this.lists[i].enableRepeat(this.bRepeat);
		}
	};
	
	CPlaylistMan.prototype.clear = function() {
		if(this.isEmpty()) return;
		// Clear listMgr.xml
		var rootNode = DomHelper.getChildNode(this.doc);
		this.dirty = true;
		
		// Clear all play list
		for(var i=0; i<this.lists.length; ++i) {
			Io.deleteFile(this.lists[i].path);
			this.lists[i].clear();
			this.lists = [];
		}

///
		var child = DomHelper.getChildNode(rootNode);
			for(;child;child=DomHelper.getNextNode(child)) {
				var listName = child.getAttribute("key");
				var listPath = child.getAttribute("value");
				var sysflag = child.getAttribute("sysflag");
				var listFile = Io.getFileFrom(listPath);
				if(!listFile || !listFile.exists()) continue;
				//if( sysflag == "1")
					Io.deleteFile(listPath);
			}

		
		// Clear file manager list
		while (this.element.removeItemAt(0));
		this.element.selectedIndex = -1;
	};

	CPlaylistMan.prototype.isEmpty = function() {
		return this.element.getRowCount() == 0;
	};
	
	CPlaylistMan.prototype.getLength = function() {
		return this.element.getRowCount();
	};
	
	CPlaylistMan.prototype.save = function() {
		for(var i=0; i<this.lists.length; ++i) {
			this.lists[i].save();
		}
		if(!this.dirty) return;
		var rootNode = DomHelper.getChildNode(this.doc);
		// Get last play list index
		var lastIdx = rootNode.getAttribute("lastPlayIndex");
		if(lastIdx == null) {
			DomHelper.addAttribute(this.doc, rootNode, "lastPlayIndex", this.selIdx);
		}
		var docContent = DomHelper.GetXmlContent(this.doc);
		if(docContent.indexOf('version="1.0"') < 0) {
			docContent = '<?xml version="1.0" encoding="UTF-8" ?>' + docContent;
		}
		Io.saveUTF8File(docContent, this.path, true);
	};
	
	// Helper function, open xulplayer native playlist (.xpl type)
	CPlaylistMan.prototype.openNativeList = function (listPath, listName,sysflag) {
		if(!listPath) return;
		//Add new list name to lists manager list
		if(!listName) listName = getFileTitle(listPath);
		this.element.appendItem(listName, listPath);
		var listCount = this.getLength();
		if(listCount > 0) {
			this.element.selectedIndex = listCount-1;
			this.selIdx = listCount-1;
		}
		//Append lists manager node
		if(this.doc) {
			var rootNode = DomHelper.getChildNode(this.doc);
			// key = listName, value = listPath
			DomHelper.insertNode(this.doc, rootNode, listName, listPath,sysflag);
			this.dirty = true;
		}
		
		// Add current list items
		var alist = new CFileList(listPath);
        alist.initialize();
		alist.show();
		alist.enableRandom(this.bRandom);
		alist.enableRepeat(this.bRepeat);
		this.lists.push(alist);
	};
	
	// Helper function, open m3u playlist (.m3u type)
	CPlaylistMan.prototype.openM3uList = function (listPath) {
		if(!listPath) return;
		var txtLines = Io.openTextFile(listPath);
		//txtLines = txtLines.split('\n');
		var listName = getFileTitle(listPath);
		var dirObj = Io.getFileFrom(this.path);
		var listFileName = listName + CFileList.FILE_EXTENSION;
		dirObj.append(listFileName);
		dirObj.createUnique(Components.interfaces.nsIFile.NORMAL_FILE_TYPE, 420);
		var listContent = '<?xml version="1.0" encoding="UTF-8" ?> <queue>';
		var fileTitle = '';
		var duration = 0;
		//var ls = /^\s*/;  // Leading space regular expression
		//var ts = /\s*$/;  // Trailing space regular expression
		for(var i=0; i<txtLines.length; ++i) {
			var txt = txtLines[i];
			//txt.replace(ls, "").replace(ts, "");
			if (txt.match(/^#EXTINF:(\d+),(\S.+)$/i)) {
				duration = RegExp.$1;
				fileTitle = RegExp.$2;
			} else if (!txt.match(/^(#|\s*$)/)) {
				var afile = Io.getFileFrom(txt);
				if(afile && afile.exists()) {
					var fileType = getFileType(txt);
					var fileSize = afile.fileSize/1024;	//convert to KB
					fileSize = normalizeFileSize(fileSize);
					if(fileTitle == '') fileTitle = getFileTitle(txt);
					listContent += '<item>' +
				                '<node key="overall.file" value="'+txt+'"/>' +
								'<node key="tags.title" value="'+fileTitle+'"/>' +
								'<node key="overall.size" value="'+fileSize+'"/>' +
								'<node key="overall.type" value="'+fileType+'"/>' +
								'</item>';
				}
				fileTitle = '';
				duration = 0;
			}
		}
		listContent += '</queue>';
		Io.saveUTF8File(listContent, dirObj, true);
		this.openNativeList(dirObj.path, listName);
	};
	
	//--------------------- Operation on current playlist----------------------
	CPlaylistMan.prototype.changeList = function() {
		if(this.selIdx == this.element.selectedIndex) return;
		this.selIdx = this.element.selectedIndex;
		if(this.selIdx >= 0) {	// show selected playlist
			this.lists[this.selIdx].show();
		}
		this.dirty = true;
	};
	
	CPlaylistMan.prototype.changeListSelIndex = function() {
		if(this.selIdx >= 0) {	// show selected playlist
			this.lists[this.selIdx].changeSelIndex();
			playFromLibrary = false;	// play from playlist
		}
	};
	
	CPlaylistMan.prototype.addFile = function(fileName, fileSize, fileType) {
		if(!fileName) return;
		if(this.selIdx < 0) {
			if(this.isEmpty()) {
				this.newList('default');
			} else {
				this.element.selectedIndex = 0;
				this.selIdx = 0;
			}
		}
		var fileTitle = getFileTitle(fileName);
		this.lists[this.selIdx].addFile(fileName, fileSize, fileType, fileTitle);
	};

	CPlaylistMan.prototype.addFileEx = function(gruoppath, fileName, fileSize, fileType) {
		if(!fileName) return;
		if(this.selIdx < 0) {
			if(this.isEmpty()) {
				this.newList('default');
			} else {
				this.element.selectedIndex = 0;
				this.selIdx = 0;
			}
		}
		gruoppath = gruoppath.substr(gruoppath.lastIndexOf('\\')+1);  
		gruoppath += ".xpl";
		for(var i=0; i<this.lists.length; ++i) {
			var curpathname = this.lists[i].path.substr(this.lists[i].path.lastIndexOf('\\')+1);  
			if(gruoppath == curpathname)
			{
				var fileTitle = getFileTitle(fileName);
				this.lists[i].addFile(fileName, fileSize, fileType, fileTitle);
				break;
			}
		}
		//var fileTitle = getFileTitle(fileName);
		//this.lists[this.selIdx].addFile(fileName, fileSize, fileType, fileTitle);
	};

	CPlaylistMan.prototype.removeFile = function() {
		if(this.selIdx >= 0) {
			this.lists[this.selIdx].removeSelectedItem();
		}
	};
	
	//Get Current file for playing in current playlist
	CPlaylistMan.prototype.getCurrentFile = function() {
		if(this.selIdx >= 0) {
			return this.lists[this.selIdx].getCurrentFile();
		}
		return null;
	};
	
	CPlaylistMan.prototype.getCurrentItemNode = function() {
		if(this.selIdx >= 0) {
			return this.lists[this.selIdx].getCurItemNode();
		}
		return null;
	};
	
	//Get filename of the selected iterm in current playlist
	CPlaylistMan.prototype.getSelectedFile = function() {
		if(this.selIdx >= 0) {
			return this.lists[this.selIdx].getSelectedFile();
		}
		return null;
	};
	
	CPlaylistMan.prototype.isCurFileSelected = function() {
	return this.getCurrentFile() == this.getSelectedFile();
	};
	
	CPlaylistMan.prototype.moveNext = function() {
		if(this.selIdx >= 0) {
			this.lists[this.selIdx].moveNext();
		}
	};
	
	CPlaylistMan.prototype.movePrevious = function() {
		if(this.selIdx >= 0) {
			this.lists[this.selIdx].movePrevious();
		}
	};
	
	CPlaylistMan.prototype.clearCurList = function() {
		if(this.selIdx >= 0) {
			this.lists[this.selIdx].clear();
		}
	};
	
	CPlaylistMan.prototype.getCurDoc = function() {
		if(this.selIdx >= 0) {
			return this.lists[this.selIdx].doc;
		}
		return null;
	};
}
