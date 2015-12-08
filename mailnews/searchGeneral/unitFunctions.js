function $(eltId) {
    return document.getElementById(eltId);
}
var strHelper = {
    trim: function(str) {
        return str.replace(/^\s+|\s+$/g, "");
    },

    ltrim: function(str) {
        return str.replace(/^\s+/, "");
    },

    rtrim: function(str) {
        return str.replace(/\s+$/, "");
    }
};

