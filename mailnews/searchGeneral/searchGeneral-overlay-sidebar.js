/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

var gLastShownCalendarView = null;

var searchGeneralTabMonitor = {
    monitorName: "searchGeneral",

    // Unused, but needed functions
    onTabTitleChanged: function() {},
    onTabOpened: function() {},
    onTabClosing: function() {},
    onTabPersist: function() {},
    onTabRestored: function() {},

    onTabSwitched: function onTabSwitched(aNewTab, aOldTab) {
        // Unfortunately, tabmail doesn't provide a hideTab function on the tab
        // type definitions. To make sure the commands are correctly disabled,
        // we want to update searchGeneral commands when switching away from
        // those tabs.
        if (aOldTab.mode.name == "searchGeneral") {
            //calendarController.updateCommands();
            //calendarController2.updateCommands();
        }
    }
};

var searchGeneralTabType = {
  name: "searchGeneral",
  panelId: "searchGeneralTabPanel",
  modes: {
    searchGeneral: {
      type: "searchGeneral",
      maxTabs: 1,
      openTab: function(aTab, aArgs) {
        aTab.title = aArgs["title"];
        if (!("background" in aArgs) || !aArgs["background"]) {
            // Only do calendar mode switching if the tab is opened in
            // foreground.
            ltnSwitch2SearchGeneral();
        }
      },

      showTab: function(aTab) {
        ltnSwitch2SearchGeneral();
      },
      closeTab: function(aTab) {
        if (gCurrentMode == "searchGeneral") {
          // Only revert menu hacks if closing the active tab, otherwise we
          // would switch to mail mode even if in task mode and closing the
          // calendar tab.
          ltnSwitch2Mail();
        }
      },

      persistTab: function(aTab) {
        let tabmail = document.getElementById("tabmail");
        return {
            // Since we do strange tab switching logic in ltnSwitch2Calendar,
            // we should store the current tab state ourselves.
            background: (aTab != tabmail.currentTabInfo)
        };
      },

      restoreTab: function(aTabmail, aState) {
				var bundle = document.getElementById("bundle_searchGeneral");
        aState.title = bundle.getString('tabTitleSearchGeneral');//ltnGetString("lightning", "tabTitleCalendar");
        aTabmail.openTab('searchGeneral', aState);
      },

      onTitleChanged: function(aTab) {
				var bundle = document.getElementById("bundle_searchGeneral");
        aTab.title = bundle.getString('tabTitleSearchGeneral');
      },

      supportsCommand: function (aCommand, aTab) calendarController2.supportsCommand(aCommand),
      isCommandEnabled: function (aCommand, aTab) calendarController2.isCommandEnabled(aCommand),
      doCommand: function(aCommand, aTab) calendarController2.doCommand(aCommand),
      onEvent: function(aEvent, aTab) calendarController2.onEvent(aEvent)
    },
  },

  /* because calendar does some direct menu manipulation, we need to change
   *  to the mail mode to clean up after those hacks.
   */
  saveTabState: function(aTab) {
    ltnSwitch2Mail();
  }
};

window.addEventListener("load", function(e) {
    let tabmail = document.getElementById('tabmail');
    tabmail.registerTabType(searchGeneralTabType);
    tabmail.registerTabMonitor(searchGeneralTabMonitor);
}, false);



/**
 * the current mode is set to a string defining the current
 * mode we're in. allowed values are:
 *  - 'mail'
 *  - 'searchGeneral'
 */
var gCurrentMode = 'mail';

/**
 * ltnSwitch2Mail() switches to the mail mode
 */

function ltnSwitch2Mail() {
  if (gCurrentMode != 'mail') {
    gCurrentMode = 'mail';
    document.getElementById("modeBroadcaster").setAttribute("mode", gCurrentMode);

    //document.commandDispatcher.updateCommands('calendar_commands');//??
    window.setCursor("auto");
  }
}

/**
 * ltnSwitch2SearchGeneral() switches to the searchGeneral mode
 */

function ltnSwitch2SearchGeneral() {
  if (gCurrentMode != 'searchGeneral') {
    gCurrentMode = 'searchGeneral';
    document.getElementById("modeBroadcaster").setAttribute("mode", gCurrentMode);

    // display the searchGeneral panel on the display deck
    //let deck = document.getElementById("calendarDisplayDeck");
    //deck.selectedPanel = document.getElementById("calendar-view-box");

    // show the last displayed type of calendar view
    switchToView(gLastShownCalendarView);

    //document.commandDispatcher.updateCommands('calendar_commands');???
    window.setCursor("auto");
  }
}

