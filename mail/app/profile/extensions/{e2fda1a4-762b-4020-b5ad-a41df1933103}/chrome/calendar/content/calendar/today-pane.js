/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://calendar/modules/calUtils.jsm");

/**
 * Namespace object to hold functions related to the today pane.
 */
var TodayPane = {
    paneViews: null,
    start: null,
    cwlabel: null,
    previousMode:  null,

    /**
     * Load Handler, sets up the today pane controls.
     */
    onLoad: function onLoad() {
        TodayPane.paneViews = [ cal.calGetString("calendar", "eventsandtasks"),
                                cal.calGetString("calendar", "tasksonly"),
                                cal.calGetString("calendar", "eventsonly") ];
        agendaListbox.setupCalendar();
        TodayPane.initializeMiniday();
        TodayPane.setShortWeekdays();

        document.getElementById("modeBroadcaster").addEventListener("DOMAttrModified", TodayPane.onModeModified, false);
        TodayPane.setTodayHeader();

        document.getElementById("today-splitter").addEventListener("command", onCalendarViewResize, false);
        TodayPane.updateSplitterState();
        TodayPane.previousMode = document.getElementById("modeBroadcaster").getAttribute("mode");
    },

    /**
     * Unload handler, cleans up the today pane on window unload.
     */
    onUnload: function onUnload() {
        document.getElementById("modeBroadcaster").removeEventListener("DOMAttrModified", TodayPane.onModeModified, false);
        document.getElementById("today-splitter").removeEventListener("command", onCalendarViewResize, false);
    },

    /**
     * Sets up the label for the switcher that allows switching between today pane
     * views. (event+task, task only, event only)
     */
    setTodayHeader: function setTodayHeader() {
        let currentMode = document.getElementById("modeBroadcaster").getAttribute("mode");
        let agendaIsVisible = document.getElementById("agenda-panel").isVisible(currentMode);
        let todoIsVisible = document.getElementById("todo-tab-panel").isVisible(currentMode);
        let index = 2;
        if (agendaIsVisible && todoIsVisible) {
            index = 0;
        } else if (!agendaIsVisible && todoIsVisible) {
            index = 1;
        } else if (agendaIsVisible && !todoIsVisible) {
            index = 2;
        } else { // agendaIsVisible == false && todoIsVisible == false:
            // In this case something must have gone wrong
            // - probably in the previous session - and no pane is displayed.
            // We set a default by only displaying agenda-pane.
            agendaIsVisible = true;
            document.getElementById("agenda-panel").setVisible(agendaIsVisible);
            index = 2;
        }
        let todayHeader = document.getElementById("today-pane-header");
        todayHeader.setAttribute("index", index);
        todayHeader.setAttribute("value", this.paneViews[index]);
        let todayPaneSplitter = document.getElementById("today-pane-splitter");
        setBooleanAttribute(todayPaneSplitter, "hidden", (index != 0));
        let todayIsVisible = document.getElementById("today-pane-panel").isVisible();

        // Disable or enable the today pane menuitems that have an attribute
        // name="minidisplay" depending on the visibility of elements.
        let menu = document.getElementById("ltnTodayPaneMenuPopup");
        if (menu) {
            setAttributeToChildren(menu, "disabled", (!todayIsVisible || !agendaIsVisible), "name", "minidisplay");
        }

        onCalendarViewResize();
    },

    /**
     * Sets up the miniday display in the today pane.
     */
    initializeMiniday: function initializeMiniday() {
        // initialize the label denoting the current month, year and calendarweek
        // with numbers that are supposed to consume the largest width
        // in order to guarantee that the text will not be cropped when modified
        // during runtime
        const kYEARINIT= "5555";
        const kCALWEEKINIT= "55";
        let monthdisplaydeck = document.getElementById("monthNameContainer");
        let childNodes = monthdisplaydeck.childNodes;

        for (let i = 0; i < childNodes.length; i++) {
            let monthlabel = childNodes[i];
            this.setMonthDescription(monthlabel, i,  kYEARINIT, kCALWEEKINIT);
        }

        agendaListbox.addListener(this);
        this.setDay(cal.now());
    },

    /**
     * Helper function to set the month description on the today pane header.
     *
     * @param aMonthLabel       The XUL node to set the month label on.
     * @param aIndex            The month number, 0-based.
     * @param aYear             The year this month should be displayed with
     * @param aCalWeek          The calendar week that should be shown.
     * @return                  The value set on aMonthLabel.
     */
    setMonthDescription: function setMonthDescription(aMonthLabel, aIndex, aYear, aCalWeek) {
        if (this.cwlabel == null) {
            this.cwlabel = cal.calGetString("calendar", "shortcalendarweek");
        }
        return aMonthLabel.value = cal.getDateFormatter().shortMonthName(aIndex)
          + " " + aYear +  ", " + this.cwlabel + " " +  aCalWeek;
    },

    /**
     * Cycle the view shown in the today pane (event+task, event, task).
     *
     * @param aCycleForward     If true, the views are cycled in the forward
     *                            direction, otherwise in the opposite direction
     */
    cyclePaneView: function cyclePaneView(aCycleForward) {
        if (this.paneViews == null) {
            return;
        }
        let index = parseInt(document.getElementById("today-pane-header").getAttribute("index"));
        index = index + aCycleForward;
        let nViewLen = this.paneViews.length;
        if (index >= nViewLen) {
            index = 0;
        } else if (index == -1) {
            index = nViewLen - 1;
        }
        let agendaPanel = document.getElementById("agenda-panel");
        let todoPanel = document.getElementById("todo-tab-panel");
        let currentMode = document.getElementById("modeBroadcaster").getAttribute("mode");
        let isTodoPanelVisible = (index != 2 && todoPanel.isVisibleInMode(currentMode));
        let isAgendaPanelVisible = (index != 1 && agendaPanel.isVisibleInMode(currentMode));
        todoPanel.setVisible(isTodoPanelVisible);
        agendaPanel.setVisible(isAgendaPanelVisible);
        this.setTodayHeader();
    },

    /**
     * Shows short weekday names in the weekdayNameContainer
     */
    setShortWeekdays: function setShortWeekdays() {
        let weekdisplaydeck = document.getElementById("weekdayNameContainer");
        let childNodes = weekdisplaydeck.childNodes;
        for (let i = 0; i < childNodes.length; i++) {
            childNodes[i].setAttribute("value", cal.calGetString("dateFormat","day." + (i+1) + ".Mmm"));
        }
    },

    /**
     * Sets the shown date from a JSDate.
     *
     * @param aNewDate      The date to show.
     */
    setDaywithjsDate: function setDaywithjsDate(aNewDate) {
        let newdatetime = cal.jsDateToDateTime(aNewDate, cal.floating());
        newdatetime = newdatetime.getInTimezone(cal.calendarDefaultTimezone());
        this.setDay(newdatetime, true);
    },

    /**
     * Sets the first day shown in the today pane.
     *
     * @param aNewDate                  The calIDateTime to set.
     * @param aDontUpdateMinimonth      If true, the minimonth will not be
     *                                    updated to show the same date.
     */
    setDay: function setDay(aNewDate, aDontUpdateMinimonth) {
        this.start = aNewDate.clone();

        let daylabel = document.getElementById("datevalue-label");
        daylabel.value = this.start.day;

        let weekdaylabel = document.getElementById("weekdayNameContainer");
        weekdaylabel.selectedIndex = this.start.weekday;

        let monthnamedeck = document.getElementById("monthNameContainer");
        monthnamedeck.selectedIndex = this.start.month;

        let selMonthPanel = monthnamedeck.selectedPanel;
        this.setMonthDescription(selMonthPanel,
                                 this.start.month,
                                 this.start.year,
                                 cal.getWeekInfoService().getWeekTitle(this.start));
        if (!aDontUpdateMinimonth) {
            document.getElementById("today-Minimonth").value = this.start.jsDate;
        }
        this.updatePeriod();
    },

    /**
     * Advance by a given number of days in the today pane.
     *
     * @param aDir      The number of days to advance. Negative numbers advance
     *                    backwards in time.
     */
    advance: function advance(aDir) {
        this.start.day += aDir;
        this.setDay(this.start);
    },

    /**
     * Checks if the today pane is showing today's date.
     */
    showsToday: function showsToday() {
        return (cal.sameDay(cal.now(), this.start));
    },

    /**
     * Update the period headers in the agenda listbox using the today pane's
     * start date.
     */
    updatePeriod: function updatePeriod() {
        agendaListbox.refreshPeriodDates(this.start.clone());
        updateCalendarToDoUnifinder();
    },

    /**
     * Display a certain section in the minday/minimonth part of the todaypane.
     *
     * @param aSection      The section to display
     */
    displayMiniSection: function displayMiniSection(aSection) {
        document.getElementById("today-minimonth-box").setVisible(aSection == "minimonth");
        document.getElementById("mini-day-box").setVisible(aSection == "miniday");
        document.getElementById("today-none-box").setVisible(aSection == "none");
        setBooleanAttribute(document.getElementById("today-Minimonth"), "freebusy", aSection == "minimonth");
    },

    /**
     * Handler function for the DOMAttrModified event used to observe the
     * todaypane-splitter.
     *
     * @param aEvent        The DOM event occurring on attribute modification.
     */
    onModeModified: function onModeModified(aEvent) {
        if (aEvent.attrName == "mode") {
            let todaypane = document.getElementById("today-pane-panel");
            // Store the previous mode panel's width.
            todaypane.setModeAttribute("modewidths", todaypane.width, TodayPane.previousMode);

            TodayPane.setTodayHeader();
            TodayPane.updateSplitterState();
            todaypane.width = todaypane.getModeAttribute("modewidths", "width");
            TodayPane.previousMode = document.getElementById("modeBroadcaster").getAttribute("mode");
        }
    },

    /**
     * Toggle the today-pane and update its visual appearance.
     *
     * @param aEvent        The DOM event occuring on activated command.
     */
    toggleVisibility: function toggleVisbility(aEvent) {
        document.getElementById("today-pane-panel").togglePane(aEvent);
        TodayPane.setTodayHeader();
        TodayPane.updateSplitterState();
    },

    /**
     * Update the today-splitter state and today-pane width with saved
     * mode-dependent values.
     */
    updateSplitterState: function updateSplitterState() {
        let splitter = document.getElementById("today-splitter");
        let todaypaneVisible = document.getElementById("today-pane-panel").isVisible();
        setElementValue(splitter, !todaypaneVisible && "true", "hidden");
        if (todaypaneVisible) {
            splitter.setAttribute("state", "open");
        }
    },

    /**
     * Generates the todaypane toggle command when the today-splitter
     * is being collapsed or uncollapsed.
     */
    onCommandTodaySplitter: function onCommandTodaySplitter() {
        let todaypane = document.getElementById("today-pane-panel");
        let splitterState = document.getElementById('today-splitter').getAttribute("state");
        if (splitterState == "collapsed" && todaypane.isVisible() ||
            splitterState != "collapsed" && !todaypane.isVisible()) {
              document.getElementById('calendar_toggle_todaypane_command').doCommand();
        }
    }
};

window.addEventListener("load", TodayPane.onLoad, false);
window.addEventListener("unload", TodayPane.onUnload, false);
