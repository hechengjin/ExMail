/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#filter substitution

// Turns on basic calendar logging.
pref("calendar.debug.log", false);
// Turns on verbose calendar logging.
pref("calendar.debug.log.verbose", false);

// SYNTAX HINTS:  dashes are delimiters.  Use underscores instead.
//  The first character after a period must be alphabetic.

pref("calendar.alarms.show", true);
pref("calendar.alarms.showmissed", true);
pref("calendar.alarms.playsound", true);
pref("calendar.alarms.soundURL", "chrome://calendar/content/sound.wav");
pref("calendar.alarms.onforevents", 0); //XXX this should be a bool
pref("calendar.alarms.onfortodos", 0); //XXX this should be a bool
pref("calendar.alarms.eventalarmlen", 15);
pref("calendar.alarms.todoalarmlen", 15);
pref("calendar.alarms.eventalarmunit", "minutes");
pref("calendar.alarms.todoalarmunit", "minutes");
pref("calendar.alarms.defaultsnoozelength", 5);
pref("calendar.alarms.indicator.show", true);
pref("calendar.alarms.indicator.totaltime", 3600);
pref("calendar.date.format", 0);
pref("calendar.event.defaultlength", 60);
// Do NOT set this.  If it is unset, we guess the timezone from the system
//pref("calendar.timezone.local", "America/New_York);

// Recent timezone list
pref("calendar.timezone.recent", "[]");

// start and end work hour for day and week views
pref("calendar.view.daystarthour", 8);
pref("calendar.view.dayendhour", 17);

// number of visible hours for day and week views
pref("calendar.view.visiblehours", 9);

// If true, mouse scrolling via shift+wheel will be enabled
pref("calendar.view.mousescroll", true);

pref("calendar.weeks.inview", 4);
pref("calendar.previousweeks.inview", 0);

// Disable use of worker threads. Restart needed.
pref("calendar.threading.disabled", false);

// Enable support for multiple realms on one server with the payoff that you
// will get multiple password dialogs (one for each calendar)
pref("calendar.network.multirealm", false);

// default transparency of allday items; could be switched to e.g. "OPAQUE":
pref("calendar.allday.defaultTransparency", "TRANSPARENT");

// whether "notify" is checked by default when creating new events/todos with attendees
pref("calendar.itip.notify", true);

// whether the organizer propagates replies of attendees
pref("calendar.itip.notify-replies", false);

// whether CalDAV (experimental) scheduling is enabled or not.
pref("calendar.caldav.sched.enabled", false);

// pref("startup.homepage_override_url","chrome://browser-region/locale/region.properties");
pref("general.startup.calendar", true);

pref("toolkit.defaultChromeURI","chrome://sunbird/content/calendar.xul");
pref("browser.hiddenWindowChromeURL", "chrome://sunbird/content/hiddenWindow.xul");

pref("xpinstall.dialog.confirm", "chrome://mozapps/content/xpinstall/xpinstallConfirm.xul");
pref("xpinstall.dialog.progress", "chrome://mozapps/content/downloads/downloads.xul");
pref("xpinstall.dialog.progress.skin", "chrome://mozapps/content/extensions/extensions.xul");
pref("xpinstall.dialog.progress.chrome", "chrome://mozapps/content/extensions/extensions.xul");
pref("xpinstall.dialog.progress.type.skin", "Extension:Manager");
pref("xpinstall.dialog.progress.type.chrome", "Extension:Manager");
pref("xpinstall.whitelist.add", "update.mozilla.org");
pref("xpinstall.whitelist.add.103", "addons.mozilla.org");

// App-specific update preferences

// Whether or not app updates are enabled
pref("app.update.enabled", true);

// This preference turns on app.update.mode and allows automatic download and
// install to take place. We use a separate boolean toggle for this to make
// the UI easier to construct.
pref("app.update.auto", true);

// Defines how the Application Update Service notifies the user about updates:
//
// AUS Set to:  Minor Releases:     Major Releases:
// 0            download no prompt  download no prompt
// 1            download no prompt  download no prompt if no incompatibilities
// 2            download no prompt  prompt
//
// See chart in nsUpdateService.js.in for more details
//
pref("app.update.mode", 1);

// If set to true, the Update Service will present no UI for any event.
pref("app.update.silent", false);

// Update service URLs:
pref("app.update.url", "https://aus2-community.mozilla.org/update/2/%PRODUCT%/%VERSION%/%BUILD_ID%/%BUILD_TARGET%/%LOCALE%/%CHANNEL%/%OS_VERSION%/update.xml");
// URL user can browse to manually if for some reason all update installation
// attempts fail.
pref("app.update.url.manual", "http://www.mozilla.org/projects/calendar/sunbird/");
// A default value for the "More information about this update" link
// supplied in the "An update is available" page of the update wizard. 
pref("app.update.url.details", "http://www.mozilla.org/projects/calendar/sunbird/");

// User-settable override to app.update.url for testing purposes.
//pref("app.update.url.override", "");

// Interval: Time between checks for a new version (in seconds)
//           default=1 day
pref("app.update.interval", 86400);
// Interval: Time before prompting the user again to restart to install the
//           latest download (in seconds) default=1 day
pref("app.update.nagTimer.restart", 86400);
// Interval: When all registered timers should be checked (in milliseconds)
//           default=10 minutes
pref("app.update.timer", 600000);
// Give the user x seconds to react before showing the big UI. default=12 hrs
pref("app.update.promptWaitTime", 43200);
// Show the Update Checking/Ready UI when the user was idle for x seconds
pref("app.update.idletime", 60);

// Whether or not we show a dialog box informing the user that the update was
// successfully applied.
pref("app.update.showInstalledUI", true);

// 0 = suppress prompting for incompatibilities if there are updates available
//     to newer versions of installed addons that resolve them.
// 1 = suppress prompting for incompatibilities only if there are VersionInfo
//     updates available to installed addons that resolve them, not newer
//     versions.
pref("app.update.incompatible.mode", 0);

// Symmetric (can be overridden by individual extensions) update preferences.
// e.g.
//  extensions.{GUID}.update.enabled
//  extensions.{GUID}.update.url
//  extensions.{GUID}.update.interval
//  .. etc ..
//
pref("extensions.update.enabled", true);
pref("extensions.update.url", "https://versioncheck.addons.mozilla.org/update/VersionCheck.php?reqVersion=%REQ_VERSION%&id=%ITEM_ID%&version=%ITEM_VERSION%&maxAppVersion=%ITEM_MAXAPPVERSION%&status=%ITEM_STATUS%&appID=%APP_ID%&appVersion=%APP_VERSION%&appOS=%APP_OS%&appABI=%APP_ABI%&locale=%APP_LOCALE%&currentAppVersion=%CURRENT_APP_VERSION%");
pref("extensions.update.interval", 86400);

// Non-symmetric (not shared by extensions) extension-specific [update] preferences
pref("extensions.getMoreExtensionsURL", "https://%LOCALE%.add-ons.mozilla.com/%LOCALE%/%APP%/%VERSION%/extensions/");
pref("extensions.getMoreThemesURL", "https://%LOCALE%.add-ons.mozilla.com/%LOCALE%/%APP%/%VERSION%/themes/");
pref("extensions.getMorePluginsURL", "https://%LOCALE%.add-ons.mozilla.com/%LOCALE%/%APP%/%VERSION%/plugins/");
pref("extensions.dss.enabled", false);          // Dynamic Skin Switching                                               
pref("extensions.dss.switchPending", false);    // Non-dynamic switch pending after next
                                                // restart.

// Developers can set this to |true| if they are constantly changing files in their 
// extensions directory so that the extension system does not constantly think that
// their extensions are being updated and thus reregistered every time the app is
// started.
pref("extensions.ignoreMTimeChanges", false);

// Enables some extra Extension System Logging (can reduce performance)
pref("extensions.logging.enabled", false);

// Preferences for the Get Add-ons pane
pref("extensions.getAddons.showPane", true);
pref("extensions.getAddons.browseAddons", "https://%LOCALE%.add-ons.mozilla.com/%LOCALE%/%APP%");
pref("extensions.getAddons.maxResults", 5);
pref("extensions.getAddons.recommended.browseURL", "https://%LOCALE%.add-ons.mozilla.com/%LOCALE%/%APP%/recommended");
pref("extensions.getAddons.recommended.url", "https://services.addons.mozilla.org/%LOCALE%/%APP%/api/%API_VERSION%/list/featured/all/10/%OS%/%VERSION%");
pref("extensions.getAddons.search.browseURL", "https://%LOCALE%.add-ons.mozilla.com/%LOCALE%/%APP%/search?q=%TERMS%");
pref("extensions.getAddons.search.url", "https://services.addons.mozilla.org/%LOCALE%/%APP%/api/%API_VERSION%/search/%TERMS%/all/10/%OS%/%VERSION%");
pref("extensions.getAddons.cache.enabled", true);

// Extension blocklist preferences
pref("extensions.blocklist.enabled", true);
pref("extensions.blocklist.interval", 86400);
pref("extensions.blocklist.url", "https://addons.mozilla.org/blocklist/2/%APP_ID%/%APP_VERSION%/%PRODUCT%/%BUILD_ID%/%BUILD_TARGET%/%LOCALE%/%CHANNEL%/%OS_VERSION%/%DISTRIBUTION%/%DISTRIBUTION_VERSION%/");
pref("extensions.blocklist.detailsURL", "http://%LOCALE%.www.mozilla.com/%LOCALE%/blocklist/");

pref("general.useragent.locale", "@AB_CD@");
pref("general.skins.selectedSkin", "classic/1.0");
pref("calendar.useragent.extra", "@APP_UA_NAME@/@APP_VERSION@");

// Scripts & Windows prefs
pref("dom.disable_open_during_load",        true);
pref("javascript.options.showInConsole",    true);

// l12n and i18n
pref("intl.accept_languages", "chrome://global/locale/intl.properties");
pref("intl.charsetmenu.browser.static", "chrome://global/locale/intl.properties");
pref("intl.charsetmenu.browser.more1",  "chrome://global/locale/intl.properties");
pref("intl.charsetmenu.browser.more2",  "chrome://global/locale/intl.properties");
pref("intl.charsetmenu.browser.more3",  "chrome://global/locale/intl.properties");
pref("intl.charsetmenu.browser.more4",  "chrome://global/locale/intl.properties");
pref("intl.charsetmenu.browser.more5",  "chrome://global/locale/intl.properties");
pref("intl.charsetmenu.browser.unicode",  "chrome://global/locale/intl.properties");
pref("intl.charset.detector", "chrome://global/locale/intl.properties");
pref("intl.charset.default",  "chrome://global-platform/locale/intl.properties");
pref("font.language.group", "chrome://global/locale/intl.properties");
pref("intl.menuitems.alwaysappendaccesskeys","chrome://global/locale/intl.properties");
pref("intl.menuitems.insertseparatorbeforeaccesskeys","chrome://global/locale/intl.properties");

// 0=lines, 1=pages, 2=history , 3=text size
pref("mousewheel.withcontrolkey.action", 3);
pref("mousewheel.withshiftkey.action", 0);
pref("mousewheel.withaltkey.action", 0);
pref("mousewheel.withnokey.action", 0);

pref("profile.allow_automigration", false);   // setting to false bypasses automigration in the profile code

// pref to control the alert notification 
pref("alerts.slideIncrement", 1);
pref("alerts.slideIncrementTime", 10);
pref("alerts.totalOpenTime", 4000);

pref("signon.rememberSignons",              true);
pref("signon.SignonFileName",               "signons.txt");
pref("signon.SignonFileName2",              "signons2.txt");
pref("signon.SignonFileName3",              "signons3.txt");

// We want to make sure mail URLs are handled externally...
pref("network.protocol-handler.external.mailto", true); // for mail
pref("network.protocol-handler.external.news", true);   // for news
pref("network.protocol-handler.external.snews", true);  // for secure news
pref("network.protocol-handler.external.nntp", true);   // also news
// ...without warning dialogs
pref("network.protocol-handler.warn-external.http", false);
pref("network.protocol-handler.warn-external.https", false);
pref("network.protocol-handler.warn-external.ftp", false);
pref("network.protocol-handler.warn-external.mailto", false);
pref("network.protocol-handler.warn-external.news", false);
pref("network.protocol-handler.warn-external.snews", false);
pref("network.protocol-handler.warn-external.nntp", false);

pref("network.protocol-handler.expose-all", false);
pref("network.protocol-handler.expose.webcal", true);

pref("offline.autoDetect", true);

// Default security warning dialogs to show once.
pref("security.warn_entering_secure.show_once", true);
pref("security.warn_entering_weak.show_once", true);
pref("security.warn_leaving_secure.show_once", true);
pref("security.warn_viewing_mixed.show_once", true);
pref("security.warn_submit_insecure.show_once", true);

// Preference viewer
#ifdef XP_WIN
pref("browser.preferences.instantApply", false);
#else
pref("browser.preferences.instantApply", true);
#endif
#ifdef XP_MACOSX
pref("browser.preferences.animateFadeIn", true);
#else
pref("browser.preferences.animateFadeIn", false);
#endif

// Used by view-source
pref("accessibility.typeaheadfind.flashBar", 1);
pref("view_source.editor.path", "");
pref("view_source.editor.external", false);
pref("view_source.syntax_highlight", true);
pref("view_source.wrap_long_lines", false);

// The breakpad report server to link to in about:crashes
pref("breakpad.reportURL", "http://crash-stats.mozilla.com/report/index/");

// The Necko Disk Cache. Disable the cache until Sunbird makes correct use of it
// and offers UI preferences to control it.
pref("browser.cache.disk.enable", false);
