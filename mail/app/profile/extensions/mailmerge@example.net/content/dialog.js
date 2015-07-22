"use strict";
Components.utils.import("resource://gre/modules/Services.jsm");

var mailmerge = {
	
	init: function() {
		
		var prefs = Services.prefs.getBranch("extensions.mailmerge.");
		var stringbundle = document.getElementById("mailmerge-stringbundle");
		
		document.getElementById("mailmerge-source").appendItem(stringbundle.getString("mailmerge.dialog.source.addressbook"), "AddressBook");
		document.getElementById("mailmerge-source").appendItem(stringbundle.getString("mailmerge.dialog.source.csv"), "CSV");
		document.getElementById("mailmerge-source").selectedIndex = 0;
		
		document.getElementById("mailmerge-delivermode").appendItem(stringbundle.getString("mailmerge.dialog.delivermode.saveasdraft"), "SaveAsDraft");
		document.getElementById("mailmerge-delivermode").appendItem(stringbundle.getString("mailmerge.dialog.delivermode.sendlater"), "Later");
		document.getElementById("mailmerge-delivermode").appendItem(stringbundle.getString("mailmerge.dialog.delivermode.sendnow"), "Now");
		document.getElementById("mailmerge-delivermode").selectedIndex = 0;
		
		document.getElementById("mailmerge-format").appendItem(stringbundle.getString("mailmerge.dialog.format.both"), "Both");
		document.getElementById("mailmerge-format").appendItem(stringbundle.getString("mailmerge.dialog.format.html"), "HTML");
		document.getElementById("mailmerge-format").appendItem(stringbundle.getString("mailmerge.dialog.format.plaintext"), "PlainText");
		document.getElementById("mailmerge-format").selectedIndex = 0;
		
		document.getElementById("mailmerge-characterset").appendItem(stringbundle.getString("mailmerge.dialog.characterset.utf8"), "UTF-8");
		document.getElementById("mailmerge-characterset").appendItem(stringbundle.getString("mailmerge.dialog.characterset.windows1252"), "Windows-1252");
		document.getElementById("mailmerge-characterset").selectedIndex = 0;
		
		document.getElementById("mailmerge-fielddelimiter").appendItem(",", ",", stringbundle.getString("mailmerge.dialog.fielddelimiter.comma"));
		document.getElementById("mailmerge-fielddelimiter").appendItem(";", ";", stringbundle.getString("mailmerge.dialog.fielddelimiter.semicolon"));
		document.getElementById("mailmerge-fielddelimiter").appendItem(":", ":", stringbundle.getString("mailmerge.dialog.fielddelimiter.colon"));
		document.getElementById("mailmerge-fielddelimiter").appendItem("Tab", "\t", stringbundle.getString("mailmerge.dialog.fielddelimiter.tab"));
		document.getElementById("mailmerge-fielddelimiter").selectedIndex = 0;
		
		document.getElementById("mailmerge-textdelimiter").appendItem("\"", "\"", stringbundle.getString("mailmerge.dialog.textdelimiter.doublequote"));
		document.getElementById("mailmerge-textdelimiter").appendItem("\'", "\'", stringbundle.getString("mailmerge.dialog.textdelimiter.singlequote"));
		document.getElementById("mailmerge-textdelimiter").appendItem("", "", stringbundle.getString("mailmerge.dialog.textdelimiter.none"));
		document.getElementById("mailmerge-textdelimiter").selectedIndex = 0;
		
		document.getElementById("mailmerge-recur").appendItem(stringbundle.getString("mailmerge.dialog.recur.none"), "");
		document.getElementById("mailmerge-recur").appendItem(stringbundle.getString("mailmerge.dialog.recur.daily"), "daily");
		document.getElementById("mailmerge-recur").appendItem(stringbundle.getString("mailmerge.dialog.recur.weekly"), "weekly");
		document.getElementById("mailmerge-recur").appendItem(stringbundle.getString("mailmerge.dialog.recur.monthly"), "monthly");
		document.getElementById("mailmerge-recur").appendItem(stringbundle.getString("mailmerge.dialog.recur.yearly"), "yearly");
		document.getElementById("mailmerge-recur").selectedIndex = 0;
		
		document.getElementById("mailmerge-source").value = prefs.getCharPref("source");
		document.getElementById("mailmerge-delivermode").value = prefs.getCharPref("delivermode");
		document.getElementById("mailmerge-format").value = prefs.getCharPref("format");
		document.getElementById("mailmerge-attachments").value = prefs.getCharPref("attachments");
		document.getElementById("mailmerge-file").value = prefs.getCharPref("file");
		document.getElementById("mailmerge-characterset").value = prefs.getCharPref("characterset");
		document.getElementById("mailmerge-fielddelimiter").value = prefs.getCharPref("fielddelimiter");
		document.getElementById("mailmerge-textdelimiter").value = prefs.getCharPref("textdelimiter");
		document.getElementById("mailmerge-start").value = prefs.getCharPref("start");
		document.getElementById("mailmerge-stop").value = prefs.getCharPref("stop");
		document.getElementById("mailmerge-pause").value = prefs.getCharPref("pause");
		document.getElementById("mailmerge-at").value = prefs.getCharPref("at");
		document.getElementById("mailmerge-recur").value = prefs.getCharPref("recur");
		document.getElementById("mailmerge-every").value = prefs.getCharPref("every");
		document.getElementById("mailmerge-debug").checked = prefs.getBoolPref("debug");
		
		mailmerge.update();
		
	},
	
	accept: function() {
		
		var prefs = Services.prefs.getBranch("extensions.mailmerge.");
		
		prefs.setCharPref("source", document.getElementById("mailmerge-source").value);
		prefs.setCharPref("delivermode", document.getElementById("mailmerge-delivermode").value);
		prefs.setCharPref("format", document.getElementById("mailmerge-format").value);
		prefs.setCharPref("attachments", document.getElementById("mailmerge-attachments").value);
		prefs.setCharPref("file", document.getElementById("mailmerge-file").value);
		prefs.setCharPref("characterset", document.getElementById("mailmerge-characterset").value);
		prefs.setCharPref("fielddelimiter", document.getElementById("mailmerge-fielddelimiter").value);
		prefs.setCharPref("textdelimiter", document.getElementById("mailmerge-textdelimiter").value);
		prefs.setCharPref("start", document.getElementById("mailmerge-start").value);
		prefs.setCharPref("stop", document.getElementById("mailmerge-stop").value);
		prefs.setCharPref("pause", document.getElementById("mailmerge-pause").value);
		prefs.setCharPref("at", document.getElementById("mailmerge-at").value);
		prefs.setCharPref("recur", document.getElementById("mailmerge-recur").value);
		prefs.setCharPref("every", document.getElementById("mailmerge-every").value);
		prefs.setBoolPref("debug", document.getElementById("mailmerge-debug").checked);
		
		window.arguments[0].accept = true;
		
	},
	
	help: function() {
		
		window.openDialog("chrome://mailmerge/content/about.xul", "_blank", "chrome,dialog,modal,centerscreen", null);
		
	},
	
	browse: function() {
		
		var filePicker = Components.classes["@mozilla.org/filepicker;1"].createInstance(Components.interfaces.nsIFilePicker);
		filePicker.init(window, "Mail Merge", Components.interfaces.nsIFilePicker.modeOpen);
		filePicker.appendFilter("CSV", "*.csv");
		filePicker.appendFilter("*", "*.*");
		
		if(filePicker.show() == Components.interfaces.nsIFilePicker.returnOK) {
			document.getElementById("mailmerge-file").value = filePicker.file.path;
		}
		
	},
	
	update: function() {
		
		document.getElementById("mailmerge-csv").hidden = !(document.getElementById("mailmerge-source").value == "CSV");
		document.getElementById("mailmerge-sendlater").hidden = !(document.getElementById("mailmerge-delivermode").value == "SaveAsDraft" && window.opener.Sendlater3Util);
		window.sizeToContent();
		
	}
	
}
