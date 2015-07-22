"use strict";
Components.utils.import("resource://gre/modules/Services.jsm");

var mailmerge = {
	
	menu: function(event) {
		
		event.stopPropagation();
		
		var params = { accept: false }
		window.openDialog("chrome://mailmerge/content/dialog.xul", "_blank", "chrome,dialog,modal,centerscreen", params);
		if(params.accept) {
			
			SaveAsTemplate();
			
			if(!gMsgCompose.compFields.to) {
				
				var string = document.getElementById("bundle_composeMsgs").getString("12511");
				Services.prompt.alert(window, "Mail Merge: Error", string);
				return;
				
			}
			
			mailmerge.editor = gMsgCompose.editor.outputToString("text/html", 4);
			window.setTimeout(function() { mailmerge.compose(); }, 1000);
			
		}
		
	},
	
	toolbar: function(event) {
		
		event.stopPropagation();
		
		var params = { accept: false }
		window.openDialog("chrome://mailmerge/content/dialog.xul", "_blank", "chrome,dialog,modal,centerscreen", params);
		if(params.accept) {
			
			SaveAsTemplate();
			
			if(!gMsgCompose.compFields.to) {
				
				var string = document.getElementById("bundle_composeMsgs").getString("12511");
				Services.prompt.alert(window, "Mail Merge: Error", string);
				return;
				
			}
			
			mailmerge.editor = gMsgCompose.editor.outputToString("text/html", 4);
			window.setTimeout(function() { mailmerge.compose(); }, 1000);
			
		}
		
	},
	
	compose: function() {
		
		if(window.gSendOrSaveOperationInProgress || window.gSendOperationInProgress || window.gSaveOperationInProgress) { window.setTimeout(function() { mailmerge.compose(); }, 1000); return; }
		
		window.openDialog("chrome://mailmerge/content/compose.xul", "_blank", "chrome,dialog,modal,centerscreen", null);
		window.close();
		
	}
	
}
