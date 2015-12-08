
var calXbl = {
    createElt: function(tagName) {
        const XULNS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
        return document.createElementNS(XULNS, tagName);
    },

    getEltByAttr: function(xblElt, attrName, attrValue) {
        return document.getAnonymousElementByAttribute(xblElt, attrName, attrValue);
    },

    getElt: function(xblElt, anonid) {
        return this.getEltByAttr(xblElt, "anonid", anonid);
    },

    fireEvent: function(elt, eventName, args) {
        var event = document.createEvent("Events");
        event.initEvent(eventName, true, true);
        event.args = args;
        elt.dispatchEvent(event);
    },

    get calHelper() {
        return calHelper;
    },

    get calLunar() {
        return calLunar;
    },

    get calEventHelper() {
        return calEventHelper;
    },

    get calCalendarHelper() {
        return calCalendarHelper;
    }

};

