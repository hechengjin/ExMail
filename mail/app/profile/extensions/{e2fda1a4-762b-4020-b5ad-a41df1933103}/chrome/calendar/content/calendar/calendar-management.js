/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://calendar/modules/calUtils.jsm");
Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

/**
 * Get this window's currently selected calendar.
 *
 * @return      The currently selected calendar.
 */
function getSelectedCalendar() {
    return getCompositeCalendar().defaultCalendar;
}

/**
 * Deletes the passed calendar, prompting the user if he really wants to do
 * this. If there is only one calendar left, no calendar is removed and the user
 * is not prompted.
 *
 * @param aCalendar     The calendar to delete.
 */
function promptDeleteCalendar(aCalendar) {
    let calendars = cal.getCalendarManager().getCalendars({});
    if (calendars.length <= 1) {
        // If this is the last calendar, don't delete it.
        return;
    }

    let promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                                  .getService(Components.interfaces.nsIPromptService);
    let ok = promptService.confirm(window,
                                   calGetString("calendar", "unsubscribeCalendarTitle"),
                                   calGetString("calendar", "unsubscribeCalendarMessage",
                                                [aCalendar.name]),
                                   {});

    if (ok) {
        let calMgr = cal.getCalendarManager();
        calMgr.unregisterCalendar(aCalendar);
        calMgr.deleteCalendar(aCalendar);
    }
}

/**
 * Called to initialize the calendar manager for a window.
 */
function loadCalendarManager() {
    // Set up the composite calendar in the calendar list widget.
    let tree = document.getElementById("calendar-list-tree-widget");
    let compositeCalendar = getCompositeCalendar();
    tree.compositeCalendar = compositeCalendar;

    // Initialize our composite observer
    compositeCalendar.addObserver(compositeObserver);

    // Create the home calendar if no calendar exists.
    let calendars = cal.getCalendarManager().getCalendars({});
    if (!calendars.length) {
        initHomeCalendar();
    }
}

/**
 * Creates the initial "Home" calendar if no calendar exists.
 */
function initHomeCalendar() {
    let calMgr = cal.getCalendarManager();
    let composite = getCompositeCalendar();
    let url = cal.makeURL("moz-storage-calendar://");
    let homeCalendar = calMgr.createCalendar("storage", url);
    homeCalendar.name = calGetString("calendar", "homeCalendarName");
    calMgr.registerCalendar(homeCalendar);
    cal.setPref("calendar.list.sortOrder", homeCalendar.id);
    composite.addCalendar(homeCalendar);

    // Wrapping this in a try/catch block, as if any of the migration code
    // fails, the app may not load.
    if (cal.getPrefSafe("calendar.migrator.enabled", true)) {
        try {
            gDataMigrator.checkAndMigrate();
        } catch (e) {
            Components.utils.reportError("Migrator error: " + e);
        }
    }

    return homeCalendar;
}

/**
 * Called to clean up the calendar manager for a window.
 */
function unloadCalendarManager() {
    let compositeCalendar = getCompositeCalendar();
    compositeCalendar.setStatusObserver(null, null);
    compositeCalendar.removeObserver(compositeObserver);
}

/**
 * Updates the sort order preference based on the given event. The event is a
 * "SortOrderChanged" event, emitted from the calendar-list-tree binding. You
 * can also pass in an object like { sortOrder: "Space separated calendar ids" }
 *
 * @param event     The SortOrderChanged event described above.
 */
function updateSortOrderPref(event) {
    let sortOrderString = event.sortOrder.join(" ");
    cal.setPref("calendar.list.sortOrder", sortOrderString);
}

/**
 * Handler function to call when the tooltip is showing on the calendar list.
 *
 * @param event     The DOM event provoked by the tooltip showing.
 */
function calendarListTooltipShowing(event) {
    let tree = document.getElementById("calendar-list-tree-widget");
    let calendar = tree.getCalendarFromEvent(event);
    let tooltipText = false;
    if (calendar) {
        let currentStatus = calendar.getProperty("currentStatus");
        if (!Components.isSuccessCode(currentStatus)){
            tooltipText = calGetString("calendar", "tooltipCalendarDisabled", [calendar.name]);
        } else if (calendar.readOnly) {
            tooltipText = calGetString("calendar", "tooltipCalendarReadOnly", [calendar.name]);
        }
    }
    setElementValue("calendar-list-tooltip", tooltipText, "label");
    return (tooltipText != false);
}

/**
 * A handler called to set up the context menu on the calendar list.
 *
 * @param event         The DOM event that caused the context menu to open.
 * @return              Returns true if the context menu should be shown.
 */
function calendarListSetupContextMenu(event) {
    let col = {};
    let row = {};
    let calendar;
    let calendars = getCalendarManager().getCalendars({});
    let treeNode = document.getElementById("calendar-list-tree-widget");

    if (document.popupNode.localName == "tree") {
        // Using VK_APPS to open the context menu will target the tree
        // itself. In that case we won't have a client point even for
        // opening the context menu. The "target" element should then be the
        // selected calendar.
        row.value =  treeNode.tree.currentIndex;
        col.value = treeNode.getColumn("calendarname-treecol");
        calendar = treeNode.getCalendar(row.value);
    } else {
        // Using the mouse, the context menu will open on the treechildren
        // element. Here we can use client points.
        calendar = treeNode.getCalendarFromEvent(event, col, row);
    }

    if (col.value &&
        col.value.element.getAttribute("anonid") == "checkbox-treecol") {
        // Don't show the context menu if the checkbox was clicked.
        return false;
    }

    document.getElementById("list-calendars-context-menu").contextCalendar = calendar;

    // Only enable calendar search if there's actually the chance of finding something:
    let hasProviders = getCalendarSearchService().getProviders({}).length < 1 && "true";
    setElementValue("list-calendars-context-find", hasProviders, "collapsed");

    if (calendar) {
        enableElement("list-calendars-context-edit");
        enableElement("list-calendars-context-publish");
        // Only enable the delete calendars item if there is more than one
        // calendar. We don't want to have the last calendar deleted.
        if (calendars.length > 1) {
            enableElement("list-calendars-context-delete");
        }
    } else {
        disableElement("list-calendars-context-edit");
        disableElement("list-calendars-context-publish");
        disableElement("list-calendars-context-delete");
    }
    return true;
}

/**
 * Makes sure the passed calendar is visible to the user
 *
 * @param aCalendar   The calendar to make visible.
 */
function ensureCalendarVisible(aCalendar) {
    // We use the main window's calendar list to ensure that the calendar is visible
    document.getElementById("calendar-list-tree-widget").ensureCalendarVisible(aCalendar);
}

var compositeObserver = {
    QueryInterface: XPCOMUtils.generateQI([Components.interfaces.calIObserver,
                                           Components.interfaces.calICompositeObserver]),

    onStartBatch: function() {},
    onEndBatch: function () {},
    onAddItem: function() {},
    onModifyItem: function() {},
    onDeleteItem: function() {},
    onError: function() {},
    onPropertyChanged: function() {},
    onPropertyDeleting: function() {},

    onLoad: function() {
        calendarUpdateNewItemsCommand();
        document.commandDispatcher.updateCommands("calendar_commands");
    },

    onCalendarAdded: function cO_onCalendarAdded(aCalendar) {
        // Update the calendar commands for number of remote calendars and for
        // more than one calendar
        document.commandDispatcher.updateCommands("calendar_commands");
    },

    onCalendarRemoved: function cO_onCalendarRemoved(aCalendar) {
        // Update commands to disallow deleting the last calendar and only
        // allowing reload remote calendars when there are remote calendars.
        document.commandDispatcher.updateCommands("calendar_commands");
    },

    onDefaultCalendarChanged: function cO_onDefaultCalendarChanged(aNewCalendar) {
        // A new default calendar may mean that the new calendar has different
        // ACLs. Make sure the commands are updated.
        calendarUpdateNewItemsCommand();
        document.commandDispatcher.updateCommands("calendar_commands");
    }
};

/**
 * Opens the subscriptions dialog modally.
 */
function openCalendarSubscriptionsDialog() {
    // the dialog will reset this to auto when it is done loading
    window.setCursor("wait");

    // open the dialog modally
    window.openDialog("chrome://calendar/content/calendar-subscriptions-dialog.xul",
                      "_blank",
                      "chrome,titlebar,modal,resizable");
}

/**
 * Calendar Offline Manager
 */
var calendarOfflineManager = {
    QueryInterface: function cOM_QueryInterface(aIID) {
        return cal.doQueryInterface(this, null, aIID,
                                    [Components.interfaces.nsIObserver,
                                     Components.interfaces.nsISupports]);
    },

    init: function cOM_init() {
        if (this.initialized) {
            throw Components.results.NS_ERROR_ALREADY_INITIALIZED;
        }
        let os = Components.classes["@mozilla.org/observer-service;1"]
                           .getService(Components.interfaces.nsIObserverService);
        os.addObserver(this, "network:offline-status-changed", false);

        this.updateOfflineUI(!this.isOnline());
        this.initialized = true;
    },

    uninit: function cOM_uninit() {
        if (!this.initialized) {
            throw Components.results.NS_ERROR_NOT_INITIALIZED;
        }
        let os = Components.classes["@mozilla.org/observer-service;1"]
                           .getService(Components.interfaces.nsIObserverService);
        os.removeObserver(this, "network:offline-status-changed", false);
        this.initialized = false;
    },

    isOnline: function cOM_isOnline() {
        return (!getIOService().offline);

    },

    updateOfflineUI: function cOM_updateOfflineUI(aIsOffline) {
        // Refresh the current view
        currentView().goToDay(currentView().selectedDay);

        // Set up disabled locks for offline
        document.commandDispatcher.updateCommands("calendar_commands");
    },

    observe: function cOM_observe(aSubject, aTopic, aState) {
        if (aTopic == "network:offline-status-changed") {
            this.updateOfflineUI(aState == "offline");
        }
    }
};
