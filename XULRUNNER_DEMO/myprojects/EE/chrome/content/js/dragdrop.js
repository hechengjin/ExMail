//Drag & Drop Processing Module

if(typeof(DRAG_DROP_PROCESS) != "boolean") {
	DRAG_DROP_PROCESS = true;
    
    var dragSession;
    var dragService;
    function dragDropHandler (evt) {
     transferObject =
        Components.classes["@mozilla.org/widget/transferable;1"]
          .createInstance();
    
      var transferObject = transferObject.QueryInterface(Components.interfaces.nsITransferable);
      transferObject.addDataFlavor("application/x-moz-file");
      var numItems = dragSession.numDropItems;
	
	  var firstFilePath = null;
      for (var i = 0; i < numItems; i++)
      {
        dragSession.getData(transferObject, i);
        var dataObj = new Object();
        var dropSizeObj = new Object();
        transferObject.getTransferData("application/x-moz-file", dataObj, dropSizeObj);
        var droppedFile = dataObj.value.QueryInterface(Components.interfaces.nsIFile);
        if(numItems == 1) stop_cmd();
		if(firstFilePath == null) firstFilePath = droppedFile.path
        addFile(droppedFile.path, droppedFile.fileSize/1024);
      }
	  //if(!mp.isPlaying()) startPlay(firstFilePath);
      dragService.canDrop = false;
	  evt.stopPropagation();
      //evt.preventBubble();
    }
    function dragEnterHandler(evt)
    {
        dragService = Components.classes["@mozilla.org/widget/dragservice;1"]
                        .getService(Components.interfaces.nsIDragService);
        if(dragService) {
            dragSession = dragService.getCurrentSession();
            if(dragSession) dragSession.canDrop = true;
        }
        evt.stopPropagation();
    }
    function dragExitHandler(evt)
    {
        dragService.canDrop = false;
        evt.stopPropagation();
    }
    function dragOverHandler(evt)
    {
        dragService.canDrop = true;
        evt.stopPropagation();
    }
    function initDragDropEvent()
    {
      window.addEventListener("dragenter", dragEnterHandler, true);
      window.addEventListener("dragdrop",  dragDropHandler, true);
      window.addEventListener("dragexit",  dragExitHandler, true);
      window.addEventListener("dragover",  dragOverHandler, true);
    }
}