
Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/FileUtils.jsm");
const {classes: Cc, interfaces: Ci, utils: Cu, Constructor: CC} = Components;
var promptHelper = {
  alert: function(text) {
        var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
            .getService(Components.interfaces.nsIPromptService);
        return promptService.alert(window, "mailtools", text);
    },
    alertEx: function(title, text) {
        var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
            .getService(Components.interfaces.nsIPromptService);
        return promptService.alert(window, title, text);
    },

    confirm: function(title, text) {
        var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
            .getService(Components.interfaces.nsIPromptService);

        return promptService.confirm(window, title, text);
    },

    confirmCheck: function(title, text, checkLabel, checkValue) {
        var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
            .getService(Components.interfaces.nsIPromptService);

        return promptService.confirmCheck(window, title, text, checkLabel, checkValue);
    },

    confirmSave: function(title, text) {
        var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
            .getService(Components.interfaces.nsIPromptService);

        return promptService.confirmEx(window, title, text,
            (promptService.BUTTON_TITLE_SAVE * promptService.BUTTON_POS_0) +
            (promptService.BUTTON_TITLE_CANCEL * promptService.BUTTON_POS_1) +
            (promptService.BUTTON_TITLE_DONT_SAVE * promptService.BUTTON_POS_2),
            null, null, null, null, {value:0});
    }
};
