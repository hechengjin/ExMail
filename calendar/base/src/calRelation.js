/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://calendar/modules/calIteratorUtils.jsm");
Components.utils.import("resource://calendar/modules/calUtils.jsm");

/**
 * calRelation prototype definition
 *
 * @implements calIRelation
 * @constructor
 */
function calRelation() {
    this.wrappedJSObject = this;
    this.mProperties = new calPropertyBag();
}

calRelation.prototype = {
    mType: null,
    mId: null,

    /**
     * @see nsISupports
     */
    QueryInterface: function (aIID) {
        return doQueryInterface(this,
                                calRelation.prototype,
                                aIID,
                                null,
                                this);
    },

    /**
     * @see nsIClassInfo
     */
    getInterfaces: function cR_getInterfaces(aCount) {
        let ifaces = [
            Components.interfaces.nsISupports,
            Components.interfaces.calIRelation,
            Components.interfaces.nsIClassInfo
        ];
        aCount.value = ifaces.length;
        return ifaces;
    },

    getHelperForLanguage: function cR_getHelperForLanguage(language) {
        return null;
    },

    contractID: "@mozilla.org/calendar/relation;1",
    classDescription: "Calendar Item Relation",
    classID: Components.ID("{76810fae-abad-4019-917a-08e95d5bbd68}"),
    implementationLanguage: Components.interfaces.nsIProgrammingLanguage.JAVASCRIPT,
    flags: 0,

    /**
     * @see calIRelation
     */

    get relType() {
        return this.mType;
    },
    set relType(aType) {
        return (this.mType = aType);
    },

    get relId() {
        return this.mId;
    },
    set relId(aRelId) {
        return (this.mId = aRelId);
    },

    get icalProperty() {
        let icssvc = getIcsService();
        let icalatt = icssvc.createIcalProperty("RELATED-TO");
        if (this.mId) {
            icalatt.value = this.mId;
        }

        if (this.mType) {
            icalatt.setParameter("RELTYPE", this.mType);
        }

        for each (let [key, value] in this.mProperties) {
            try {
                icalatt.setParameter(key, value);
            } catch (e if e.result == Components.results.NS_ERROR_ILLEGAL_VALUE) {
                // Illegal values should be ignored, but we could log them if
                // the user has enabled logging.
                cal.LOG("Warning: Invalid relation property value " + key + "=" + value);
            }
        }
        return icalatt;
    },

    set icalProperty(attProp) {
        // Reset the property bag for the parameters, it will be re-initialized
        // from the ical property.
        this.mProperties = new calPropertyBag();

        if (attProp.value) {
            this.mId = attProp.value;
        }
        for each (let [name, value] in cal.ical.paramIterator(attProp)) {
            if (name == "RELTYPE") {
                this.mType = value;
                continue;
            }

            this.setParameter(name, value);
        }
    },

    getParameter: function (aName) {
        return this.mProperties.getProperty(aName);
    },

    setParameter: function (aName, aValue) {
        return this.mProperties.setProperty(aName, aValue);
    },

    deleteParameter: function (aName) {
        return this.mProperties.deleteProperty(aName);
    },

    clone: function cR_clone() {
        let newRelation = new calRelation();
        newRelation.mId = this.mId;
        newRelation.mType = this.mType;
        for each (let [name, value] in this.mProperties) {
            newRelation.mProperties.setProperty(name, value);
        }
        return newRelation;
    }
};
