"use strict";
Components.utils.import("resource://gre/modules/Services.jsm");

var gMsgCompose;

var gProgressListener = {
	
	onStateChange: function(aWebProgress, aRequest, aStateFlags, aStatus) {
		
		//mailmerge.debug("gProgressListener: onStateChange" + "\n" + aWebProgress + "\n" + aRequest + "\n" + aStateFlags + "\n" + aStatus);
		
	},
	
	onProgressChange: function(aWebProgress, aRequest, aCurSelfProgress, aMaxSelfProgress, aCurTotalProgress, aMaxTotalProgress) {
		
		//mailmerge.debug("gProgressListener: onProgressChange" + "\n" + aWebProgress + "\n" + aRequest + "\n" + aCurSelfProgress + "\n" + aMaxSelfProgress + "\n" + aCurTotalProgress + "\n" + aMaxTotalProgress);
		
	},
	
	onLocationChange: function(aWebProgress, aRequest, aLocation) {
		
		//mailmerge.debug("gProgressListener: onLocationChange" + "\n" + aWebProgress + "\n" + aRequest + "\n" + aLocation);
		
	},
	
	onStatusChange: function(aWebProgress, aRequest, aStatus, aMessage) {
		
		//mailmerge.debug("gProgressListener: onStatusChange" + "\n" + aWebProgress + "\n" + aRequest + "\n" + aStatus + "\n" + aMessage);
		
		document.getElementById("mailmerge-status").value = aMessage;
		
		var string = window.opener.document.getElementById("bundle_composeMsgs").getString("12539");
		if(string == aMessage) { mailmerge.connections--; }
		
	},
	
	onSecurityChange: function(aWebProgress, aRequest, aState) {
		
		//mailmerge.debug("gProgressListener: onSecurityChange" + "\n" + aWebProgress + "\n" + aRequest + "\n" + aState);
		
	}
	
}

var mailmerge = {
	
	init: function() {
		
		mailmerge.prefs();
		
		/* gMsgCompose start */
		gMsgCompose = window.opener.gMsgCompose;
		gMsgCompose.checkAndPopulateRecipients(true, false, {});
		/* gMsgCompose end */
		
		window.setTimeout(function() { mailmerge.delay(); }, 1000);
		
	},
	
	delay: function() {
		
		/* dialog start */
		var dialog = "";
		dialog += "source: " + mailmerge.prefs.source + "\n";
		dialog += "delivermode: " + mailmerge.prefs.delivermode + "\n";
		dialog += "format: " + mailmerge.prefs.format + "\n";
		dialog += "attachments: " + mailmerge.prefs.attachments + "\n";
		dialog += "file: " + mailmerge.prefs.file + "\n";
		dialog += "characterset: " + mailmerge.prefs.characterset + "\n";
		dialog += "fielddelimiter: " + mailmerge.prefs.fielddelimiter + "\n";
		dialog += "textdelimiter: " + mailmerge.prefs.textdelimiter + "\n";
		dialog += "start: " + mailmerge.prefs.start + "\n";
		dialog += "stop: " + mailmerge.prefs.stop + "\n";
		dialog += "pause: " + mailmerge.prefs.pause + "\n";
		dialog += "at: " + mailmerge.prefs.at + "\n";
		dialog += "recur: " + mailmerge.prefs.recur + "\n";
		dialog += "every: " + mailmerge.prefs.every + "\n";
		dialog += "connections: " + mailmerge.prefs.connections + "\n";
		dialog += "debug: " + mailmerge.prefs.debug + "\n";
		mailmerge.debug("Mail Merge: Dialog" + "\n" + dialog);
		/* dialog end */
		
		/* debug start */
		mailmerge.debug("Mail Merge: To" + "\n" + gMsgCompose.compFields.to);
		mailmerge.debug("Mail Merge: Cc" + "\n" + gMsgCompose.compFields.cc);
		mailmerge.debug("Mail Merge: Bcc" + "\n" + gMsgCompose.compFields.bcc);
		mailmerge.debug("Mail Merge: Reply" + "\n" + gMsgCompose.compFields.replyTo);
		mailmerge.debug("Mail Merge: Subject" + "\n" + gMsgCompose.compFields.subject);
		mailmerge.debug("Mail Merge: Body" + "\n" + window.opener.mailmerge.editor);
		/* debug end */
		
		if(mailmerge.prefs.source == "AddressBook") {
			mailmerge.addressbook();
		}
		if(mailmerge.prefs.source == "CSV") {
			mailmerge.csv();
		}
		
		/* init start */
		mailmerge.connections = 0;
		
		mailmerge.start = (mailmerge.prefs.start == "") ? 1 : parseInt(mailmerge.prefs.start);
		if(mailmerge.start < 1) { mailmerge.start = 1; }
		
		mailmerge.stop = (mailmerge.prefs.stop == "") ? mailmerge.to.length - 1 : parseInt(mailmerge.prefs.stop);
		if(mailmerge.stop > mailmerge.to.length - 1) { mailmerge.stop = mailmerge.to.length - 1; }
		
		mailmerge.index = mailmerge.start - 1;
		/* init end */
		
		/* update start */
		mailmerge.update();
		/* update end */
		
		/* time start */
		var time = new Date();
		mailmerge.time(time, time);
		/* time end */
		
		window.setInterval(function() { mailmerge.time(time, new Date()); }, 1000);
		window.setTimeout(function() { mailmerge.compose(); }, 50);
		
	},
	
	prefs: function() {
		
		var prefs = Services.prefs.getBranch("extensions.mailmerge.");
		
		mailmerge.prefs.source = prefs.getCharPref("source");
		mailmerge.prefs.delivermode = prefs.getCharPref("delivermode");
		mailmerge.prefs.format = prefs.getCharPref("format");
		mailmerge.prefs.attachments = prefs.getCharPref("attachments");
		mailmerge.prefs.file = prefs.getCharPref("file");
		mailmerge.prefs.characterset = prefs.getCharPref("characterset");
		mailmerge.prefs.fielddelimiter = prefs.getCharPref("fielddelimiter");
		mailmerge.prefs.textdelimiter = prefs.getCharPref("textdelimiter");
		mailmerge.prefs.start = prefs.getCharPref("start");
		mailmerge.prefs.stop = prefs.getCharPref("stop");
		mailmerge.prefs.pause = prefs.getCharPref("pause");
		mailmerge.prefs.at = prefs.getCharPref("at");
		mailmerge.prefs.recur = prefs.getCharPref("recur");
		mailmerge.prefs.every = prefs.getCharPref("every");
		mailmerge.prefs.connections = prefs.getIntPref("connections");
		mailmerge.prefs.debug = prefs.getBoolPref("debug");
		
	},
	
	time: function(start, stop) {
		
		var time = Math.round((stop - start) / 1000);
		
		var hours = Math.floor(time / 3600);
		if(hours < 10) { hours = "0" + hours; }
		
		var minutes = Math.floor(time % 3600 / 60);
		if(minutes < 10) { minutes = "0" + minutes; }
		
		var seconds = Math.floor(time % 3600 % 60);
		if(seconds < 10) { seconds = "0" + seconds; }
		
		document.getElementById("mailmerge-time").value = hours + ":" + minutes + ":" + seconds;
		
	},
	
	update: function() {
		
		var current = mailmerge.index - mailmerge.start + 1;
		var total = mailmerge.stop - mailmerge.start + 1;
		
		document.getElementById("mailmerge-current").value = current;
		document.getElementById("mailmerge-total").value = total;
		
		document.getElementById("mailmerge-progressmeter").value = Math.round(current / total * 1000);
		document.getElementById("mailmerge-progress").value = Math.round(current / total * 100) + " %";
		
	},
	
	addressbook: function() {
		
		mailmerge.to = [""];
		mailmerge.object = [""];
		
		/* to start */
		var to = gMsgCompose.compFields.to;
		to = to.replace(new RegExp('"', 'g'), '');
		to = to.split(",");
		/* to end */
		
		/* array start */
		for(var i = 0; i < to.length; i++) {
			
			if(to[i] == "" || to[i].indexOf("@") == -1) { continue; }
			
			/* addressbook start */
			try {
				
				var card;
				
				var objPattern = new RegExp("(?:(.*) <(.*)>|(.*))", "g");
				var arrMatches = objPattern.exec(to[i]);
				arrMatches = (arrMatches[2] || arrMatches[3]);
				
				var addressbooks = Components.classes["@mozilla.org/abmanager;1"].getService(Components.interfaces.nsIAbManager).directories;
				while(addressbooks.hasMoreElements()) {
					
					var addressbook = addressbooks.getNext();
					if(addressbook instanceof Components.interfaces.nsIAbDirectory && !addressbook.isRemote) {
						
						card = addressbook.getCardFromProperty("PrimaryEmail", arrMatches, null);
						if(card) { break; }
						
					}
					
				}
				
			} catch(e) {
				
				Services.prompt.alert(window, "Mail Merge: Error", e.message);
				mailmerge.debug("Mail Merge: Error" + "\n" + e);
				window.close();
				return;
				
			}
			/* addressbook end */
			
			mailmerge.to.push(to[i]);
			mailmerge.object.push(card);
			
		}
		/* array end */
		
	},
	
	csv: function() {
		
		mailmerge.to = [""];
		mailmerge.object = [""];
		
		/* file start */
		try {
			
			var localFile = Components.classes["@mozilla.org/file/local;1"].createInstance(Components.interfaces.nsIFile);
			localFile.initWithPath(mailmerge.prefs.file);
			
			var fileInputStream = Components.classes["@mozilla.org/network/file-input-stream;1"].createInstance(Components.interfaces.nsIFileInputStream);
			fileInputStream.init(localFile, -1, 0, 0);
			
			var converterInputStream = Components.classes["@mozilla.org/intl/converter-input-stream;1"].createInstance(Components.interfaces.nsIConverterInputStream);
			converterInputStream.init(fileInputStream, mailmerge.prefs.characterset, 0, 0);
			
			var file = "", string = {};
			while(converterInputStream.readString(4096, string) != 0) {
				file += string.value;
			}
			
			converterInputStream.close();
			
		} catch(e) {
			
			Services.prompt.alert(window, "Mail Merge: Error", e.message);
			mailmerge.debug("Mail Merge: Error" + "\n" + e);
			window.close();
			return;
			
		}
		/* file end */
		
		mailmerge.debug("Mail Merge: File" + "\n" + file);
		file = mailmerge.file(file, mailmerge.prefs.fielddelimiter, mailmerge.prefs.textdelimiter);
		
		/* array start */
		for(var row = 1; row < file.length; row++) {
			
			/* object start */
			var object = {};
			for(var column = 0; column < file[0].length; column++) {
				
				if(file[0][column] == "") { continue; }
				object[file[0][column]] = (file[row][column] || "");
				
			}
			/* object end */
			
			/* to start */
			var to = gMsgCompose.compFields.to;
			to = to.replace(new RegExp('"', 'g'), '');
			to = mailmerge.substitute(to, object);
			/* to end */
			
			if(to == "" || to.indexOf("@") == -1) { continue; }
			
			mailmerge.to.push(to);
			mailmerge.object.push(object);
			
		}
		/* array end */
		
	},
	
	file: function(string, fielddelimiter, textdelimiter) {
		
		/*
			Thanks to Ben Nadel
			http://www.bennadel.com/
		*/
		
		/*
			/(^|,|\r\n|\r|\n)(?:["]([^"]*(?:["]["][^"]*)*)["]|([^,"\r\n]*))/g
			/(^|,|\r\n|\r|\n)(?:[']([^']*(?:[']['][^']*)*)[']|([^,'\r\n]*))/g
			
			/(^|;|\r\n|\r|\n)(?:["]([^"]*(?:["]["][^"]*)*)["]|([^;"\r\n]*))/g
			/(^|;|\r\n|\r|\n)(?:[']([^']*(?:[']['][^']*)*)[']|([^;'\r\n]*))/g
			
			/(^|:|\r\n|\r|\n)(?:["]([^"]*(?:["]["][^"]*)*)["]|([^:"\r\n]*))/g
			/(^|:|\r\n|\r|\n)(?:[']([^']*(?:[']['][^']*)*)[']|([^:'\r\n]*))/g
		*/
		
		if(!string) { return [[]]; }
		
		var objPattern = new RegExp("(^|" + fielddelimiter + "|\r\n|\r|\n)(?:[" + textdelimiter + "]([^" + textdelimiter + "]*(?:[" + textdelimiter + "][" + textdelimiter + "][^" + textdelimiter + "]*)*)[" + textdelimiter + "]|([^" + fielddelimiter + textdelimiter + "\r\n]*))", "g");
		
		var arrData = [[]], arrMatches = [];
		while(arrMatches = objPattern.exec(string)) {
			
			var strMatchedValue = "";
			
			if(arrMatches[1].length && arrMatches[1] != fielddelimiter) {
				arrData.push([]);
			}
			
			if(arrMatches[2]) {
				strMatchedValue = arrMatches[2].replace(new RegExp(textdelimiter + textdelimiter, "g"), textdelimiter);
			}
			
			if(arrMatches[3]) {
				strMatchedValue = arrMatches[3];
			}
			
			arrData[arrData.length - 1].push(strMatchedValue);
			
		}
		
		return arrData;
		
	},
	
	compose: function() {
		
		if(mailmerge.connections == mailmerge.prefs.connections) { window.setTimeout(function() { mailmerge.compose(); }, 50); return; }
		
		/* update start */
		document.getElementById("mailmerge-status").value = "";
		/* update end */
		
		/* index start */
		mailmerge.index++;
		if(mailmerge.index > mailmerge.stop) { window.setTimeout(function() { window.close(); }, 1000); return; }
		/* index end */
		
		/* object start */
		var object = mailmerge.object[mailmerge.index];
		/* object end */
		
		/* to start */
		var to = mailmerge.to[mailmerge.index];
		/* to end */
		
		/* cc start */
		var cc = gMsgCompose.compFields.cc;
		cc = cc.replace(new RegExp('"', 'g'), '');
		cc = mailmerge.substitute(cc, object);
		/* cc end */
		
		/* bcc start */
		var bcc = gMsgCompose.compFields.bcc;
		bcc = bcc.replace(new RegExp('"', 'g'), '');
		bcc = mailmerge.substitute(bcc, object);
		/* bcc end */
		
		/* reply start */
		var reply = gMsgCompose.compFields.replyTo;
		reply = reply.replace(new RegExp('"', 'g'), '');
		reply = mailmerge.substitute(reply, object);
		/* reply end */
		
		/* subject start */
		var subject = gMsgCompose.compFields.subject;
		subject = mailmerge.substitute(subject, object);
		/* subject end */
		
		/* body start */
		var body = window.opener.mailmerge.editor;
		body = body.replace(new RegExp('<body style="[^"]*">', 'g'), '<body>');
		body = body.replace(new RegExp('%7B', 'g'), '{');
		body = body.replace(new RegExp('%7C', 'g'), '|');
		body = body.replace(new RegExp('%7D', 'g'), '}');
		body = mailmerge.substitute(body, object);
		/* body end */
		
		/* attachments start */
		var attachments = mailmerge.prefs.attachments;
		attachments = mailmerge.substitute(attachments, object);
		/* attachments end */
		
		/* at start */
		var at = mailmerge.prefs.at;
		at = mailmerge.substitute(at, object);
		/* at end */
		
		/* pause start */
		var pause = mailmerge.prefs.pause;
		pause = mailmerge.substitute(pause, object);
		pause = (pause == "") ? 50 : parseInt(pause * 1000);
		/* pause end */
		
		/* debug start */
		//mailmerge.debug("Mail Merge: To" + "\n" + to);
		//mailmerge.debug("Mail Merge: Cc" + "\n" + cc);
		//mailmerge.debug("Mail Merge: Bcc" + "\n" + bcc);
		//mailmerge.debug("Mail Merge: Reply" + "\n" + reply);
		//mailmerge.debug("Mail Merge: Subject" + "\n" + subject);
		//mailmerge.debug("Mail Merge: Body" + "\n" + body);
		//mailmerge.debug("Mail Merge: Attachments" + "\n" + attachments);
		//mailmerge.debug("Mail Merge: At" + "\n" + at);
		//mailmerge.debug("Mail Merge: Pause" + "\n" + pause);
		/* debug end */
		
		window.setTimeout(function() { mailmerge.send(to, cc, bcc, reply, subject, body, attachments, at); }, pause);
		
	},
	
	send: function(to, cc, bcc, reply, subject, body, attachments, at) {
		
		/* update start */
		mailmerge.update();
		/* update end */
		
		var compose = Components.classes["@mozilla.org/messengercompose/compose;1"].createInstance(Components.interfaces.nsIMsgCompose);
		var composeParams = Components.classes["@mozilla.org/messengercompose/composeparams;1"].createInstance(Components.interfaces.nsIMsgComposeParams);
		var compFields = Components.classes["@mozilla.org/messengercompose/composefields;1"].createInstance(Components.interfaces.nsIMsgCompFields);
		
		/* progress start */
		var progress = Components.classes["@mozilla.org/messenger/progress;1"].createInstance(Components.interfaces.nsIMsgProgress);
		progress.registerListener(gProgressListener);
		/* progress end */
		
		/* composeParams start */
		composeParams.type = Components.interfaces.nsIMsgCompType.New;
		composeParams.format = Components.interfaces.nsIMsgCompFormat.HTML;
		composeParams.identity = window.opener.getCurrentIdentity();
		composeParams.composeFields = compFields;
		/* composeParams end */
		
		/* compose start */
		compose.initialize(composeParams);
		compose.initEditor(gMsgCompose.editor, window.opener.content);
		/* compose end */
		
		/* compFields start */
		compFields.to = to;
		compFields.cc = cc;
		compFields.bcc = bcc;
		compFields.replyTo = reply;
		compFields.subject = subject;
		
		compFields.attachVCard = gMsgCompose.compFields.attachVCard;
		compFields.characterSet = gMsgCompose.compFields.characterSet;
		compFields.DSN = gMsgCompose.compFields.DSN;
		compFields.from = gMsgCompose.compFields.from;
		compFields.organization = gMsgCompose.compFields.organization;
		compFields.priority = gMsgCompose.compFields.priority;
		compFields.returnReceipt = gMsgCompose.compFields.returnReceipt;
		compFields.securityInfo = gMsgCompose.compFields.securityInfo;
		
		compFields.otherRandomHeaders = gMsgCompose.compFields.otherRandomHeaders;
		/* compFields end */
		
		/* sendlater start */
		if(mailmerge.prefs.delivermode == "SaveAsDraft" && window.opener.Sendlater3Util && at) {
			
			try {
				
				Components.utils.import("resource://sendlater3/dateparse.jsm");
				
				var recur = mailmerge.prefs.recur;
				if(recur == "monthly") {
					recur += " " + sendlater3DateParse(at).getDate();
				}
				if(recur == "yearly") {
					recur += " " + sendlater3DateParse(at).getMonth() + " " + sendlater3DateParse(at).getDate();
				}
				
				var every = mailmerge.prefs.every;
				if(every) {
					every = " " + "/" + " " + parseInt(every);
				}
				
				compFields.otherRandomHeaders += "X-Send-Later-At: " + sendlater3DateParse(at) + "\n";
				compFields.otherRandomHeaders += "X-Send-Later-Uuid: " + window.opener.Sendlater3Util.getInstanceUuid() + "\n";
				compFields.otherRandomHeaders += "X-Send-Later-Recur: " + recur + every + "\n";
				
			} catch(e) {
				
				Services.prompt.alert(window, "Mail Merge: Error", e.message);
				mailmerge.debug("Mail Merge: Error" + "\n" + e);
				window.close();
				return;
				
			}
			
		}
		/* sendlater end */
		
		/* attachments start */
		var bucket = window.opener.document.getElementById("attachmentBucket");
		for(var i = 0; i < bucket.itemCount; i++) {
			
			try {
				
				compFields.addAttachment(bucket.getItemAtIndex(i).attachment);
				
			} catch(e) {
				
				Services.prompt.alert(window, "Mail Merge: Error", e.message);
				mailmerge.debug("Mail Merge: Error" + "\n" + e);
				window.close();
				return;
				
			}
			
		}
		/* attachments end */
		
		/* attachments start */
		var attachments = attachments.split(",");
		for(var i = 0; i < attachments.length; i++) {
			
			try {
				
				/* compatibility start */
				attachments[i] = attachments[i].replace(new RegExp('^\\s*', 'g'), '');
				attachments[i] = attachments[i].replace(new RegExp('^file\:\/\/', 'g'), '');
				/* compatibility end */
				
				if(attachments[i] == "") { continue; }
				
				var localFile = Components.classes["@mozilla.org/file/local;1"].createInstance(Components.interfaces.nsIFile);
				localFile.initWithPath(attachments[i]);
				
				if(!localFile.exists() || !localFile.isFile()) { continue; }
				
				var attachment = Components.classes["@mozilla.org/messengercompose/attachment;1"].createInstance(Components.interfaces.nsIMsgAttachment);
				attachment.url = "file://" + attachments[i];
				
				compFields.addAttachment(attachment);
				
			} catch(e) {
				
				Services.prompt.alert(window, "Mail Merge: Error", e.message);
				mailmerge.debug("Mail Merge: Error" + "\n" + e);
				window.close();
				return;
				
			}
			
		}
		/* attachments end */
		
		/* delivermode start */
		switch(mailmerge.prefs.delivermode) {
			
			case "SaveAsDraft":
				
				/* Components.interfaces.nsIMsgCompDeliverMode.SaveAsDraft */
				var delivermode = Components.interfaces.nsIMsgCompDeliverMode.SaveAsDraft;
				break;
				
			case "Later":
				
				/* Components.interfaces.nsIMsgCompDeliverMode.Later */
				var delivermode = Components.interfaces.nsIMsgCompDeliverMode.Later;
				break;
				
			case "Now":
				
				/* Components.interfaces.nsIMsgCompDeliverMode.Now */
				var delivermode = Components.interfaces.nsIMsgCompDeliverMode.Now;
				break;
				
			default: ;
			
		}
		/* delivermode end */
		
		/* format start */
		switch(mailmerge.prefs.format) {
			
			case "Both":
				
				/* Components.interfaces.nsIMsgCompSendFormat.Both */
				compFields.forcePlainText = false;
				compFields.useMultipartAlternative = true;
				break;
				
			case "HTML":
				
				/* Components.interfaces.nsIMsgCompSendFormat.HTML */
				compFields.forcePlainText = false;
				compFields.useMultipartAlternative = false;
				break;
				
			case "PlainText":
				
				/* Components.interfaces.nsIMsgCompSendFormat.PlainText */
				compFields.forcePlainText = true;
				compFields.useMultipartAlternative = false;
				break;
				
			default: ;
			
		}
		/* format end */
		
		/* editor start */
		try {
			
			compose.editor.QueryInterface(Components.interfaces.nsIHTMLEditor);
			compose.editor.documentCharacterSet = gMsgCompose.compFields.characterSet;
			compose.editor.rebuildDocumentFromSource("");
			compose.editor.rebuildDocumentFromSource(body);
			
		} catch(e) {
			
			Services.prompt.alert(window, "Mail Merge: Error", e.message);
			mailmerge.debug("Mail Merge: Error" + "\n" + e);
			window.close();
			return;
			
		}
		/* editor end */
		
		try {
			
			mailmerge.connections++;
			compose.SendMsg(delivermode, window.opener.getCurrentIdentity(), window.opener.getCurrentAccountKey(), null, progress);
			
		} catch(e) {
			
			Services.prompt.alert(window, "Mail Merge: Error", e.message);
			mailmerge.debug("Mail Merge: Error" + "\n" + e);
			window.close();
			return;
			
		}
		
		/* editor start */
		try {
			
			gMsgCompose.editor.QueryInterface(Components.interfaces.nsIHTMLEditor);
			gMsgCompose.editor.documentCharacterSet = gMsgCompose.compFields.characterSet;
			gMsgCompose.editor.rebuildDocumentFromSource("");
			gMsgCompose.editor.rebuildDocumentFromSource(window.opener.mailmerge.editor);
			
		} catch(e) {
			
			Services.prompt.alert(window, "Mail Merge: Error", e.message);
			mailmerge.debug("Mail Merge: Error" + "\n" + e);
			window.close();
			return;
			
		}
		/* editor end */
		
		window.setTimeout(function() { mailmerge.compose(); }, 50);
		
	},
	
	substitute: function(string, object) {
		
		//var objPattern = new RegExp("(?:[{][{]([^|{}]*)[}][}])", "g");
		//var objPattern = new RegExp("(?:[{][{]([^|{}]*)[}][}]|[{][{]([^|{}]*)[|]([^|{}]*)[|]([^|{}]*)[}][}])", "g");
		//var objPattern = new RegExp("(?:[{][{]([^|{}]*)[}][}]|[{][{]([^|{}]*)[|]([^|{}]*)[|]([^|{}]*)[}][}]|[{][{]([^|{}]*)[|]([^|{}]*)[|]([^|{}]*)[|]([^|{}]*)[}][}])", "g");
		//var objPattern = new RegExp("(?:[{][{]([^|{}]*)[}][}]|[{][{]([^|{}]*)[|]([^|{}]*)[|]([^|{}]*)[}][}]|[{][{]([^|{}]*)[|]([^|{}]*)[|]([^|{}]*)[|]([^|{}]*)[}][}]|[{][{]([^|{}]*)[|]([^|{}]*)[|]([^|{}]*)[|]([^|{}]*)[|]([^|{}]*)[}][}])", "g");
		var objPattern = new RegExp("(?:[{][{]([^|{}]*)[}][}]|[{][{]([^|{}]*)[|]([^|{}]*)[|]([^|{}]*)[}][}]|[{][{]([^|{}]*)[|]([^|{}]*)[|]([^|{}]*)[|]([^|{}]*)[}][}]|[{][{]([^|{}]*)[|]([^|{}]*)[|]([^|{}]*)[|]([^|{}]*)[|]([^|{}]*)[}][}]|[{][{]([^{}]*)[}][}])", "g");
		
		var arrMatches = objPattern.exec(string);
		if(!arrMatches) { return string; }
		
		/* workaround start */
		for(var i = 1; i < arrMatches.length; i++) {
			
			if(!arrMatches[i]) { continue; }
			arrMatches[i] = arrMatches[i].replace(new RegExp('\n(  )*', 'g'), ' ');
			
		}
		/* workaround end */
		
		if(mailmerge.prefs.source == "AddressBook" && object) {
			
			var card = object;
			
			if(arrMatches[1]) {
				
				/* {{name}} */
				string = string.replace(arrMatches[0], card.getProperty(arrMatches[1], ""));
				return mailmerge.substitute(string, card);
				
			}
			
			if(arrMatches[2]) {
				
				/* {{name|if|then}} */
				string = (card.getProperty(arrMatches[2], "") == arrMatches[3]) ? string.replace(arrMatches[0], arrMatches[4]) : string.replace(arrMatches[0], "");
				return mailmerge.substitute(string, card);
				
			}
			
			if(arrMatches[5]) {
				
				/* {{name|if|then|else}} */
				string = (card.getProperty(arrMatches[5], "") == arrMatches[6]) ? string.replace(arrMatches[0], arrMatches[7]) : string.replace(arrMatches[0], arrMatches[8]);
				return mailmerge.substitute(string, card);
				
			}
			
			if(arrMatches[9]) {
				
				if(arrMatches[10] == "*") {
					
					/* {{name|*|if|then|else}} */
					string = (card.getProperty(arrMatches[9], "").match(arrMatches[11])) ? string.replace(arrMatches[0], arrMatches[12]) : string.replace(arrMatches[0], arrMatches[13]);
					return mailmerge.substitute(string, card);
					
				}
				
				if(arrMatches[10] == "^") {
					
					/* {{name|^|if|then|else}} */
					string = (card.getProperty(arrMatches[9], "").match("^" + arrMatches[11])) ? string.replace(arrMatches[0], arrMatches[12]) : string.replace(arrMatches[0], arrMatches[13]);
					return mailmerge.substitute(string, card);
					
				}
				
				if(arrMatches[10] == "$") {
					
					/* {{name|$|if|then|else}} */
					string = (card.getProperty(arrMatches[9], "").match(arrMatches[11] + "$")) ? string.replace(arrMatches[0], arrMatches[12]) : string.replace(arrMatches[0], arrMatches[13]);
					return mailmerge.substitute(string, card);
					
				}
				
			}
			
			if(arrMatches[9]) {
				
				if(arrMatches[10] == "==") {
					
					/* {{name|==|if|then|else}} */
					string = (parseFloat(card.getProperty(arrMatches[9], "").replace(",",".")) == parseFloat(arrMatches[11].replace(",","."))) ? string.replace(arrMatches[0], arrMatches[12]) : string.replace(arrMatches[0], arrMatches[13]);
					return mailmerge.substitute(string, card);
					
				}
				
				if(arrMatches[10] == ">" || arrMatches[10] == "&gt;") {
					
					/* {{name|>|if|then|else}} */
					string = (parseFloat(card.getProperty(arrMatches[9], "").replace(",",".")) > parseFloat(arrMatches[11].replace(",","."))) ? string.replace(arrMatches[0], arrMatches[12]) : string.replace(arrMatches[0], arrMatches[13]);
					return mailmerge.substitute(string, card);
					
				}
				
				if(arrMatches[10] == ">=" || arrMatches[10] == "&gt;=") {
					
					/* {{name|>=|if|then|else}} */
					string = (parseFloat(card.getProperty(arrMatches[9], "").replace(",",".")) >= parseFloat(arrMatches[11].replace(",","."))) ? string.replace(arrMatches[0], arrMatches[12]) : string.replace(arrMatches[0], arrMatches[13]);
					return mailmerge.substitute(string, card);
					
				}
				
				if(arrMatches[10] == "<" || arrMatches[10] == "&lt;") {
					
					/* {{name|<|if|then|else}} */
					string = (parseFloat(card.getProperty(arrMatches[9], "").replace(",",".")) < parseFloat(arrMatches[11].replace(",","."))) ? string.replace(arrMatches[0], arrMatches[12]) : string.replace(arrMatches[0], arrMatches[13]);
					return mailmerge.substitute(string, card);
					
				}
				
				if(arrMatches[10] == "<=" || arrMatches[10] == "&lt;=") {
					
					/* {{name|<=|if|then|else}} */
					string = (parseFloat(card.getProperty(arrMatches[9], "").replace(",",".")) <= parseFloat(arrMatches[11].replace(",","."))) ? string.replace(arrMatches[0], arrMatches[12]) : string.replace(arrMatches[0], arrMatches[13]);
					return mailmerge.substitute(string, card);
					
				}
				
			}
			
		}
		
		if(mailmerge.prefs.source == "CSV" && object) {
			
			var file = object;
			
			if(arrMatches[1]) {
				
				/* {{name}} */
				string = string.replace(arrMatches[0], file[arrMatches[1]]);
				return mailmerge.substitute(string, file);
				
			}
			
			if(arrMatches[2]) {
				
				/* {{name|if|then}} */
				string = (file[arrMatches[2]] == arrMatches[3]) ? string.replace(arrMatches[0], arrMatches[4]) : string.replace(arrMatches[0], "");
				return mailmerge.substitute(string, file);
				
			}
			
			if(arrMatches[5]) {
				
				/* {{name|if|then|else}} */
				string = (file[arrMatches[5]] == arrMatches[6]) ? string.replace(arrMatches[0], arrMatches[7]) : string.replace(arrMatches[0], arrMatches[8]);
				return mailmerge.substitute(string, file);
				
			}
			
			if(arrMatches[9]) {
				
				if(arrMatches[10] == "*") {
					
					/* {{name|*|if|then|else}} */
					string = (file[arrMatches[9]].match(arrMatches[11])) ? string.replace(arrMatches[0], arrMatches[12]) : string.replace(arrMatches[0], arrMatches[13]);
					return mailmerge.substitute(string, file);
					
				}
				
				if(arrMatches[10] == "^") {
					
					/* {{name|^|if|then|else}} */
					string = (file[arrMatches[9]].match("^" + arrMatches[11])) ? string.replace(arrMatches[0], arrMatches[12]) : string.replace(arrMatches[0], arrMatches[13]);
					return mailmerge.substitute(string, file);
					
				}
				
				if(arrMatches[10] == "$") {
					
					/* {{name|$|if|then|else}} */
					string = (file[arrMatches[9]].match(arrMatches[11] + "$")) ? string.replace(arrMatches[0], arrMatches[12]) : string.replace(arrMatches[0], arrMatches[13]);
					return mailmerge.substitute(string, file);
					
				}
				
			}
			
			if(arrMatches[9]) {
				
				if(arrMatches[10] == "==") {
					
					/* {{name|==|if|then|else}} */
					string = (parseFloat(file[arrMatches[9]].replace(",",".")) == parseFloat(arrMatches[11].replace(",","."))) ? string.replace(arrMatches[0], arrMatches[12]) : string.replace(arrMatches[0], arrMatches[13]);
					return mailmerge.substitute(string, file);
					
				}
				
				if(arrMatches[10] == ">" || arrMatches[10] == "&gt;") {
					
					/* {{name|>|if|then|else}} */
					string = (parseFloat(file[arrMatches[9]].replace(",",".")) > parseFloat(arrMatches[11].replace(",","."))) ? string.replace(arrMatches[0], arrMatches[12]) : string.replace(arrMatches[0], arrMatches[13]);
					return mailmerge.substitute(string, file);
					
				}
				
				if(arrMatches[10] == ">=" || arrMatches[10] == "&gt;=") {
					
					/* {{name|>=|if|then|else}} */
					string = (parseFloat(file[arrMatches[9]].replace(",",".")) >= parseFloat(arrMatches[11].replace(",","."))) ? string.replace(arrMatches[0], arrMatches[12]) : string.replace(arrMatches[0], arrMatches[13]);
					return mailmerge.substitute(string, file);
					
				}
				
				if(arrMatches[10] == "<" || arrMatches[10] == "&lt;") {
					
					/* {{name|<|if|then|else}} */
					string = (parseFloat(file[arrMatches[9]].replace(",",".")) < parseFloat(arrMatches[11].replace(",","."))) ? string.replace(arrMatches[0], arrMatches[12]) : string.replace(arrMatches[0], arrMatches[13]);
					return mailmerge.substitute(string, file);
					
				}
				
				if(arrMatches[10] == "<=" || arrMatches[10] == "&lt;=") {
					
					/* {{name|<=|if|then|else}} */
					string = (parseFloat(file[arrMatches[9]].replace(",",".")) <= parseFloat(arrMatches[11].replace(",","."))) ? string.replace(arrMatches[0], arrMatches[12]) : string.replace(arrMatches[0], arrMatches[13]);
					return mailmerge.substitute(string, file);
					
				}
				
			}
			
		}
		
		string = string.replace(arrMatches[0], "");
		return mailmerge.substitute(string, object);
		
	},
	
	debug: function(string) {
		
		if(!mailmerge.prefs.debug) { return; }
		Services.console.logStringMessage(string);
		
	}
	
}
