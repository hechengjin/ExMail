/* File list class. Provide add/remove file operation, meamwhile update xml doc
  associated with the filelist.
  It should be created with two params: id of file list element, full path of the file list
   Created by Zoominla, 01-08-2009         
*/

if(typeof(XULPLAYER_PLAYLIST_INCLUDE) != "boolean") {
	XULPLAYER_PLAYLIST_INCLUDE = true;

function CFileList(queuePath)
{
	this.list = document.getElementById('playlist');
	this.path = queuePath;
	// doc of queue.xml
	this.doc = null;
	this.fileNode = null;			// Current doc node
	this.bRandom = false;
	this.bRepeat = false;			
	this.selIdx = -1;				// current file index
	this.count = 0;					// count of files in the list
	this.dirty = false;				// whether the list is modified
}

CFileList.FILE_EXTENSION = '.xpl';

CFileList.prototype.initialize = function() {
	this.doc = DomHelper.LoadLocalXMLFile(this.path);
	if(!this.doc) return;
	this.doc.async = false;
	this.doc.preserveWhiteSpace=false;
	//this.show();
	if(!this.isEmpty()) {
		this.list.selectedIndex = 0;
		this.selIdx = 0;
		this.fileNode = this.getCurItemNode();
	}
};

CFileList.prototype.isEmpty = function() {
	return this.count == 0;
};

CFileList.prototype.getLength = function() {
	return this.count;
};

// Return a nsIFile Object (dir type)
CFileList.prototype.getDefaultDir = function() {
	return this.path;
};

//Get Current file for playing
CFileList.prototype.getCurrentFile = function() {
	var fileName = null;
	if(this.fileNode) {
		var node = DomHelper.getChildNode(this.fileNode);
		if(node) fileName = DomHelper.getNodeValueByKey(node, 'overall.file');
	}
	return fileName;
};

//Get filename of the selected iterm
CFileList.prototype.getSelectedFile = function() {
	var filename = null;
	var selIdx = this.list.selectedIndex;
	if(this.doc) {
		var rootNode = DomHelper.getChildNode(this.doc);		
		var node = DomHelper.getChildByIndex(rootNode, selIdx);		
		node = DomHelper.getChildNode(node);
		if(node) {			
			filename = DomHelper.getNodeValueByKey(node, 'overall.file');
		}
	}
	return filename;
};

// This is a private method (helper function for other methods)
CFileList.prototype.appendItem = function(fileName, fileSize, fileType, fileTitle) {
	if(!fileName) return;
	var item = document.createElement("listitem");
	var cell = document.createElement("listcell");
	if(!fileTitle) fileTitle = getFileTitle(fileName);
	cell.setAttribute("label", fileTitle);
	item.appendChild(cell);
	
	cell = document.createElement("listcell");
	cell.setAttribute("label", fileSize);
	item.appendChild(cell);
	
	cell = document.createElement("listcell");
	if(!fileType) fileType = getFileType(fileName);
	cell.setAttribute("label", fileType);
	item.appendChild(cell);
	
	cell = document.createElement("listcell");
	cell.setAttribute("label", fileName);
	item.appendChild(cell);
	this.list.appendChild(item);
	this.count++ ;
};
	
// Params: fileType and fileTitle is optional
CFileList.prototype.addFile = function(fileName, fileSize, fileType, fileTitle) {
	if(!fileName) return;
	if(!fileTitle) fileTitle = getFileTitle(fileName);
	if(!fileType) fileType = getFileType(fileName);
	if(this.doc) {
		var rootNode = DomHelper.getChildNode(this.doc); 
		var node = DomHelper.addElementNode(this.doc, rootNode, 'item');
		DomHelper.insertNode(this.doc, node, 'overall.file', fileName);
		DomHelper.insertNode(this.doc, node, 'tags.title', fileTitle);
		DomHelper.insertNode(this.doc, node, 'overall.size', fileSize);
		DomHelper.insertNode(this.doc, node, 'overall.type', fileType);
		this.dirty = true;
	}
	if(parseInt(fileType) == CPlayer.CONTENT_DVD)
	{
		var infos = new Array();
		infos = fileName.split(',');
		if(infos[1] == "0")
		{
			fileTitle = "DVD Disc"
		}
		else
			fileTitle = "Track" + infos[1];
		if(infos[5].length == 1)
			infos[5] += ":";
		fileName = infos[5] + "\\" + fileTitle;
		fileType = "DVD";
	}
	else if(parseInt(fileType) == CPlayer.CONTENT_VCD)
	{
		var infos = new Array();
		infos = fileName.split(',');
		if(infos[1] == "0")
		{
			fileTitle = "VCD Disc"
		}
		else
			fileTitle = "Track" + infos[1];
		if(infos[2].length == 1)
			infos[2] += ":";
		fileName = infos[2] + "\\" + fileTitle;
		fileType = "VCD";
	}
	else if(parseInt(fileType) == CPlayer.CONTENT_CD )
	{
		var infos = new Array();
		infos = fileName.split(',');
		fileTitle = "Track" + infos[1];
		if(infos[2].length == 1)
			infos[2] += ":";
		fileName = infos[2] + "\\" + fileTitle;
		fileType = "CD";
	}
	this.appendItem(fileName, fileSize, fileType, fileTitle);
	if(!this.isEmpty()) {
		if(this.selIdx == -1) this.selIdx = 0;
		this.fileNode = this.getCurItemNode();
	}
};

CFileList.prototype.changeSelIndex = function() {
	var selIdx = this.list.selectedIndex;
	if(selIdx == this.selIdx) return;
	if(selIdx >= 0) {
		this.selIdx = selIdx;
		this.fileNode = this.getCurItemNode();
	}
};

CFileList.prototype.moveNext = function() {
	var itemCount = this.count;
	if(itemCount < 1) return;
	
	var oldIdx = this.selIdx;
	var newSelIdx = -1;
	if(this.bRandom) {		// Random play items
		newSelIdx = rand(itemCount);
		if(newSelIdx==oldIdx && itemCount>1) {
			newSelIdx = oldIdx + 1;
		}
	} else {
		newSelIdx = oldIdx + 1;
	}
	
	if(newSelIdx == itemCount) {
		if(this.bRepeat) {
			newSelIdx = 0;
		} else {
			newSelIdx = -1;
		}
	}
		
	this.list.selectedIndex = newSelIdx;
	this.selIdx = newSelIdx;
	this.fileNode = this.getCurItemNode();
};

CFileList.prototype.movePrevious = function() {
	var itemCount = this.count;
	if(itemCount < 1) return;
	
	var oldIdx = this.selIdx;
	var newSelIdx = -1;
	if(this.bRandom) {		// Random play items
		newSelIdx = rand(itemCount);
		if(newSelIdx==oldIdx && itemCount>1) {
			newSelIdx = oldIdx - 1;
		}
	} else {
		newSelIdx = oldIdx - 1;
	}
	if(this.bRepeat) {
		if(newSelIdx < 0) newSelIdx += itemCount;
	}
	this.list.selectedIndex = newSelIdx;
	this.selIdx = newSelIdx;
	this.fileNode = this.getCurItemNode();
};

CFileList.prototype.enableRandom = function(bEnable) {
	this.bRandom = bEnable;
};

CFileList.prototype.enableRepeat = function(bEnable) {
	this.bRepeat = bEnable;
};

CFileList.prototype.removeSelectedItem = function() {
	if(this.isEmpty()) return;
	var selIdx = this.list.selectedIndex;
	if(selIdx < 0) return;
	// Remove item from queue.xml and update it
	if(this.doc) {
		var rootNode = DomHelper.getChildNode(this.doc); 
		var node = DomHelper.getChildNode(rootNode);
		for(i=0;i<selIdx;i++) {		
			node=DomHelper.getNextNode(node);
		}	
		if(node) {
			var newNode = DomHelper.getPreviousNode(node);
			if(newNode) this.fileNode = newNode;
			else this.fileNode = DomHelper.getNextNode(node);
			rootNode.removeChild(node);
		}
		this.dirty = true;
	}
	
	// Remove file list item and update it
	this.list.removeItemAt(selIdx);
	var newSelIdx = selIdx;
	if(newSelIdx == this.count) newSelIdx -= 1;
	this.list.selectedIndex = newSelIdx;
	this.selIdx = newSelIdx;
	this.count--;
};

CFileList.prototype.clear = function() {
	if(this.isEmpty()) return;
	// Clear queue.xml
	if(this.doc) {
		var rootNode = DomHelper.getChildNode(this.doc);
		DomHelper.removeChildren(rootNode);
		this.fileNode = null;
		this.dirty = true;
	}
	// Clear playlist
	while (this.list.removeItemAt(0));
	
	this.list.selectedIndex = -1;
	this.selIdx = -1;
	this.count = 0;
};

CFileList.prototype.getDocContent = function() {
	if(this.doc) {
		var docContent = DomHelper.GetXmlContent(this.doc);
		return docContent;
	}
	return null;
};

CFileList.prototype.getCurItemInfo = function() {
	
	var node = this.getCurItemNode();
	if(node) {
		return DomHelper.GetXmlContent(node);
	}
	return null;
};

CFileList.prototype.getCurItemNode = function() {
	if(this.doc && this.selIdx >= 0) {
		var rootNode = DomHelper.getChildNode(this.doc);
		var node = DomHelper.getChildNode(rootNode);
		for(i=0;i<this.selIdx;i++) {
			node=DomHelper.getNextNode(node);
		}
		return node;
	}
	return null;
};

CFileList.prototype.show = function() {
	if(this.list.getRowCount() > 0) {
		while (this.list.removeItemAt(0));
	}
	var rootNode = DomHelper.getChildNode(this.doc);
	var child = DomHelper.getChildNode(rootNode);
	for(;child;child=DomHelper.getNextNode(child)) {
		var node = DomHelper.getChildNode(child);
		var valnode = DomHelper.getNodeByAttribute(node,"node","key","overall.file");
		var tagnode = DomHelper.getNodeByAttribute(node,"node","key","tags.title");
		var sizenode = DomHelper.getNodeByAttribute(node,"node","key","overall.size");
		var typenode = DomHelper.getNodeByAttribute(node,"node","key","overall.type");
		var fileType = typenode.getAttribute("value");
		var filePath = valnode.getAttribute("value");
		var fileSize = sizenode.getAttribute("value");		
		var fileTitle = tagnode.getAttribute("value");
		if(parseInt(fileType) == CPlayer.CONTENT_DVD)
		{
			var infos = new Array();
			infos = filePath.split(',');
			if(infos[1] == "0")
			{
				fileTitle = "DVD Disc"
			}
			else
				fileTitle = "Track" + infos[1];
			if(infos[5].length == 1)
			infos[5] += ":";
			filePath = infos[5] + "\\" + fileTitle;
			fileType = "DVD";
		}
		else if(parseInt(fileType) == CPlayer.CONTENT_VCD )
		{
			var infos = new Array();
			infos = filePath.split(',');
			if(infos[1] == "0")
			{
				fileTitle = "VCD Disc"
			}
			else
				fileTitle = "Track" + infos[1];
			if(infos[2].length == 1)
				infos[2] += ":";
			filePath = infos[2] + "\\" + fileTitle;
			fileType = "VCD";
		}
		else if(parseInt(fileType) == CPlayer.CONTENT_CD )
		{
			var infos = new Array();
			infos = filePath.split(',');
			fileTitle = "Track" + infos[1];
			if(infos[2].length == 1)
				infos[2] += ":";
			filePath = infos[2] + "\\" + fileTitle;
			fileType = "CD";
		}
		this.appendItem(filePath, fileSize, fileType, fileTitle);
	}
	this.list.selectedIndex = this.selIdx;
};

CFileList.prototype.save = function() {
	if(this.doc && this.dirty) {
		var docContent = DomHelper.GetXmlContent(this.doc);
		if(docContent.indexOf('version="1.0"') < 0) {
			docContent = '<?xml version="1.0" encoding="UTF-8" ?>' + docContent;
		}
		if(getLocalFileOrDir(this.path)) {	// if file doesn't exsit then it's deleted
			Io.saveUTF8File(docContent, this.path, true);
		}
	}
};

}
