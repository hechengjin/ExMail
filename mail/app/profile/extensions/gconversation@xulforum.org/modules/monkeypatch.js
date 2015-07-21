/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Thunderbird Conversations
 *
 * The Initial Developer of the Original Code is
 *  Jonathan Protzenko <jonathan.protzenko@gmail.com>
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

"use strict";

var EXPORTED_SYMBOLS = ['MonkeyPatch', 'BOOTSTRAP_REASONS']

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/AddonManager.jsm");
Cu.import("resource:///modules/StringBundle.js"); // for StringBundle

Cu.import("resource://conversations/modules/stdlib/misc.js");
Cu.import("resource://conversations/modules/stdlib/msgHdrUtils.js");
Cu.import("resource://conversations/modules/assistant.js");
Cu.import("resource://conversations/modules/misc.js"); // for joinWordList
Cu.import("resource://conversations/modules/prefs.js");
Cu.import("resource://conversations/modules/log.js");

Cu.import("resource://gre/modules/Services.jsm");

const kMultiMessageUrl = "chrome://messenger/content/multimessageview.xhtml";

const BOOTSTRAP_REASONS = {
  APP_STARTUP     : 1,
  APP_SHUTDOWN    : 2,
  ADDON_ENABLE    : 3,
  ADDON_DISABLE   : 4,
  ADDON_INSTALL   : 5,
  ADDON_UNINSTALL : 6,
  ADDON_UPGRADE   : 7,
  ADDON_DOWNGRADE : 8
};

let strings = new StringBundle("chrome://conversations/locale/message.properties");

let Log = setupLogging("Conversations.MonkeyPatch");

let shouldPerformUninstall;

function MonkeyPatch(aWindow, aConversation) {
  this._Conversation = aConversation;
  this._window = aWindow;
  this._markReadTimeout = null;
  this._beingUninstalled = false;
  this._undoFuncs = [];
}

MonkeyPatch.prototype = {

  pushUndo: function _MonkeyPatch_pushUndo(f) {
    this._undoFuncs.push(f);
  },

  undo: function _MonkeyPatch_undo(aReason) {
    let f;
    while (f = this._undoFuncs.pop()) {
      try {
        f(aReason);
      } catch (e) {
        Log.error("Failed to undo some customization", e);
        dumpCallStack(e);
      }
    }
  },

  applyOverlay: function _MonkeyPatch_applyOverlay (window) {
    // Wow! I love restartless! Now I get to create all the items by hand!
    let strings = new StringBundle("chrome://conversations/locale/overlay.properties");

    // 1) Get a context menu in the multimessage
    window.document.getElementById("multimessage").setAttribute("context", "mailContext");

    // 2) View > Conversation View
    let menuitem = window.document.createElement("menuitem");
    for each (let [k, v] in Iterator({
      type: "checkbox",
      id: "menuConversationsEnabled",
      label: strings.get("menuConversationsEnabled"),
    })) menuitem.setAttribute(k, v);
    let after = window.document.getElementById("viewMessagesMenu");
    let parent1 = window.document.getElementById("menu_View_Popup");
    parent1.insertBefore(menuitem, after.nextElementSibling);
    this.pushUndo(function () parent1.removeChild(menuitem));

    // 3) Keyboard shortcut
    let key = window.document.createElement("key");
    for each (let [k, v] in Iterator({
      id: "key_conversationsQuickCompose",
      key: "n",
      modifiers: "accel,shift",
      oncommand: "Conversations.quickCompose();",
    })) key.setAttribute(k, v);
    let parent2 = window.document.getElementById("mailKeys");
    parent2.appendChild(key);
    this.pushUndo(function () parent2.removeChild(key));

    // 4) Tree column
    let treecol = window.document.createElement("treecol");
    for each (let [k, v] in Iterator({
      id: "betweenCol",
      hidden: "false",
      flex: "4",
      persist: "width hidden ordinal",
      label: strings.get("betweenColumnName"),
      tooltiptext: strings.get("betweenColumnTooltip"),
    })) treecol.setAttribute(k, v);
    let parent3 = window.document.getElementById("threadCols");
    parent3.appendChild(treecol);
    this.pushUndo(function () parent3.removeChild(treecol));
    let splitter = window.document.createElement("splitter");
    splitter.classList.add("tree-splitter");
    parent3.appendChild(splitter);
    this.pushUndo(function () parent3.removeChild(splitter));
  },


  registerColumn: function _MonkeyPatch_registerColumn () {
    // This has to be the first time that the documentation on MDC
    //  1) exists and
    //  2) is actually relevant!
    // 
    //            OMG !
    //
    // https://developer.mozilla.org/en/Extensions/Thunderbird/Creating_a_Custom_Column
    let window = this._window;

    let participants = function (msgHdr) {
      let format = function (x, p) {
        if ((x.email+"").toLowerCase() in gIdentities)
          return (p
            ? strings.get("meBetweenMeAndSomeone")
            : strings.get("meBetweenSomeoneAndMe")
          );
        else
          return x.name || x.email;
      };
      let seenAlready = {};
      let r = [
        [format(x, p) for each ([, x] in Iterator(parseMimeLine(msgHdr[prop])))]
        for each ([prop, p] in [["mime2DecodedAuthor", true], ["mime2DecodedRecipients", false], ["ccList", false], ["bccList", false]])
        if (msgHdr[prop])
      ].filter(function (x) {
        // Wow, a nice side-effect, I just hope the implementation of filter is
        //  as I think it is. Yes, I live dangerously!
        let r = !(x in seenAlready);
        seenAlready[x] = true;
        return r;
      }).reduce(function (x, y) x.concat(y));
      return joinWordList(r);
    };

    let columnHandler = {
      getCellText: function(row, col) {
        let msgHdr = window.gDBView.getMsgHdrAt(row);
        return participants(msgHdr);    
      },
      getSortStringForRow: function(hdr) {
        return participants(msgHdr);
      },
      isString: function() {
        return true;
      },
      getCellProperties: function(row, col, props) {},
      getRowProperties: function(row, props) {},
      getImageSrc: function(row, col) {
        return null;
      },
      getSortLongForRow: function(hdr) {
        return 0;
      }
    };

    // The main window is loaded when the monkey-patch is applied
    Services.obs.addObserver({
      observe: function(aMsgFolder, aTopic, aData) {  
        window.gDBView.addColumnHandler("betweenCol", columnHandler);
      }
    }, "MsgCreateDBView", false);
    try {
      window.gDBView.addColumnHandler("betweenCol", columnHandler);
    } catch (e) {
      // This is really weird, but rkent does it for junquilla, and this solves
      //  the issue of enigmail breaking us... don't wanna know why it works,
      //  but it works.
      // After investigating, it turns out that without enigmail, we have the
      //  following sequence of events:
      // - jsm load
      // - onload
      // - msgcreatedbview
      // With enigmail, this sequence is modified
      // - jsm load
      // - msgcreatedbview
      // - onlaod
      // So our solution kinda works, but registering the thing at jsm load-time
      //  would work as well.
    }
    this.pushUndo(function () window.gDBView.removeColumnHandler("betweenCol"));
  },

  registerFontPrefObserver: function _MonkeyPatch_registerFontPref (aHtmlpane) {
    let prefBranch = Cc["@mozilla.org/preferences-service;1"]
      .getService(Ci.nsIPrefService)
      .getBranch(null);
    prefBranch.QueryInterface(Ci.nsIPrefBranch2);
    let observer = {
      observe: function (aSubject, aTopic, aData) {
        if (aTopic == "nsPref:changed"
            && aData == "font.size.variable.x-western") {
          aHtmlpane.setAttribute("src", "about:blank");
        }
      },
    };
    prefBranch.addObserver("", observer, false);
    this._window.addEventListener("close", function () {
      prefBranch.removeObserver("", observer);
    }, false);
    this.pushUndo(function () prefBranch.removeObserver("", observer));
  },

  clearTimer: function () {
    // If we changed conversations fast, clear the timeout
    if (this.markReadTimeout)
      this._window.clearTimeout(this.markReadTimeout);
  },

  // Don't touch. Too tricky.
  determineScrollMode: function () {
    let window = this._window;
    let scrollMode = Prefs.kScrollUnreadOrLast;

    let isExpanded = false;
    let msgIndex = window.gFolderDisplay ? window.gFolderDisplay.selectedIndices[0] : -1;
    if (msgIndex >= 0) {
      try {
        let viewThread = window.gDBView.getThreadContainingIndex(msgIndex);
        let rootIndex = window.gDBView
          .findIndexOfMsgHdr(viewThread.getChildHdrAt(0), false);
        if (rootIndex >= 0) {
          isExpanded = window.gDBView.isContainer(rootIndex)
            && !window.gFolderDisplay.view.isCollapsedThreadAtIndex(rootIndex);
        }
      } catch (e) {
        Log.debug("Error in the onLocationChange handler "+e+"\n");
        dumpCallStack(e);
      }
    } 
    if (window.gFolderDisplay.view.showThreaded) {
      // The || is for the wicked case that Standard8 sent me a screencast for.
      // This makes sure we *always* mark the selected message as read, even in
      //  the tricky case where we recycle a conversation \emph{and} end up with
      //  only one message left in the thread pane.
      if (isExpanded || window.gFolderDisplay.selectedMessages.length == 1)
        scrollMode = Prefs.kScrollSelected;
      else
        scrollMode = Prefs.kScrollUnreadOrLast;
    } else {
      scrollMode = Prefs.kScrollSelected;
    }

    return scrollMode;
  },

  registerUndoCustomizations: function () {
    shouldPerformUninstall = true;

    let self = this;
    this.pushUndo(function (aReason) {
      // We don't want to undo all the customizations in the case of an
      // upgrade... but if the user disables the conversation view, or
      // uninstalls the addon, then we should revert them indeed.
      if (shouldPerformUninstall && aReason != BOOTSTRAP_REASONS.ADDON_UPGRADE) {
        // Switch to a 3pane view (otherwise the "display threaded"
        // customization is not reverted)
        let mainWindow = getMail3Pane();
        let tabmail = mainWindow.document.getElementById("tabmail");
        if (tabmail.tabContainer.selectedIndex != 0)
          tabmail.tabContainer.selectedIndex = 0;
        // This is asynchronous, leave it a second
        mainWindow.setTimeout(function () self.undoCustomizations(), 1000);
        // Since this is called once per window, we don't want to uninstall
        // multiple times...
        shouldPerformUninstall = false;
      }
    });
  },

  undoCustomizations: function () {
    let uninstallInfos = JSON.parse(Prefs.getString("conversations.uninstall_infos"));
    for each (let [k, v] in Iterator(Customizations)) {
      if (k in uninstallInfos) {
        try {
          Log.debug("Uninstalling", k, uninstallInfos[k]);
          v.uninstall(uninstallInfos[k]);
        } catch (e) {
          Log.error("Failed to uninstall", k, e);
          dumpCallStack(e);
        }
      }
    }
    Prefs.setString("conversations.uninstall_infos", "{}");
    Prefs.setInt("conversations.version", 0);
  },

  activateMenuItem: function (window) {
    let menuItem = window.document.getElementById("menuConversationsEnabled");
    menuItem.setAttribute("checked", Prefs.enabled);
    Prefs.watch(function (aPrefName, aPrefValue) {
      if (aPrefName == "enabled")
        menuItem.setAttribute("checked", aPrefValue);
    });
    menuItem.addEventListener("command", function (event) {
      let checked = menuItem.hasAttribute("checked") &&
        menuItem.getAttribute("checked") == "true";
      Prefs.setBool("conversations.enabled", checked);
      window.gMessageDisplay.onSelectedMessagesChanged.call(window.gMessageDisplay);
    });
  },

  apply: function () {
    let window = this._window;
    // First of all: "apply" the "overlay"
    this.applyOverlay(window);

    let self = this;
    let htmlpane = window.document.getElementById("multimessage");
    let oldSummarizeMultipleSelection = window["summarizeMultipleSelection"];
    let oldSummarizeThread = window["summarizeThread"];

    // Do this at least once at overlay load-time
    fillIdentities();

    // Register our new column type
    this.registerColumn();
    this.registerFontPrefObserver(htmlpane);

    this.activateMenuItem(window);

    // Undo all our customizations at uninstall-time
    this.registerUndoCustomizations();

    // Below is the code that intercepts the double-click-on-a-message event,
    //  and reroutes the control flow to our conversation reader.
    let oldThreadPaneDoubleClick = window.ThreadPaneDoubleClick;
    window.ThreadPaneDoubleClick = function () {
      if (!Prefs.enabled) {
        oldThreadPaneDoubleClick();
        return;
      }

      let tabmail = window.document.getElementById("tabmail");
      // ThreadPaneDoubleClick calls OnMsgOpenSelectedMessages. We don't want to
      // replace the whole ThreadPaneDoubleClick function, just the line that
      // calls OnMsgOpenSelectedMessages in that function. So we do that weird
      // thing here.
      let oldMsgOpenSelectedMessages = window.MsgOpenSelectedMessages;
      let msgHdrs = window.gFolderDisplay.selectedMessages;
      if (!msgHdrs.some(msgHdrIsRss) && !msgHdrs.some(msgHdrIsNntp)) {
        window.MsgOpenSelectedMessages = function () {
          let urls = [msgHdrGetUri(x) for each (x in msgHdrs)].join(",");
          let scrollMode = self.determineScrollMode();
          let queryString = "?urls="+window.encodeURIComponent(urls) +
            "&scrollMode="+scrollMode;
          tabmail.openTab("chromeTab", {
            chromePage: kStubUrl+queryString,
          });
        };
      }
      oldThreadPaneDoubleClick();
      window.MsgOpenSelectedMessages = oldMsgOpenSelectedMessages;
    };
    this.pushUndo(function() window.ThreadPaneDoubleClick = oldThreadPaneDoubleClick);

    // Because we're not even fetching the conversation when the message pane is
    //  hidden, we need to trigger it manually when it's un-hidden.
    let unhideListener = function () {
      if (!Prefs.enabled)
        return;

      window.summarizeThread(window.gFolderDisplay.selectedMessages);
    };
    window.addEventListener("messagepane-unhide", unhideListener, true);
    this.pushUndo(function () window.removeEventListener("messagepane-unhide", unhideListener, true));

    window.summarizeMultipleSelection =
      function _summarizeMultiple_patched (aSelectedMessages, aListener) {
        if (!Prefs.enabled) {
          oldSummarizeMultipleSelection(aSelectedMessages, aListener);
          return;
        }

        window.gSummaryFrameManager.loadAndCallback(kMultiMessageUrl, function () {
          oldSummarizeMultipleSelection(aSelectedMessages, aListener);
        });
      };
    this.pushUndo(function () window.summarizeMultipleSelection = oldSummarizeMultipleSelection);

    let previouslySelectedUris = [];
    let previousScrollMode = null;

    // This one completely nukes the original summarizeThread function, which is
    //  actually the entry point to the original ThreadSummary class.
    window.summarizeThread =
      function _summarizeThread_patched (aSelectedMessages, aListener) {
        if (!Prefs.enabled) {
          oldSummarizeThread(aSelectedMessages, aListener);
          return;
        }

        if (!aSelectedMessages.length)
          return;

        if (!window.gMessageDisplay.visible) {
          Log.debug("Message pane is hidden, not fetching...");
          return;
        }

        window.gSummaryFrameManager.loadAndCallback(kStubUrl, function (isRefresh) {
          // See issue #673
          if (htmlpane.contentDocument && htmlpane.contentDocument.body)
            htmlpane.contentDocument.body.hidden = false;

          if (isRefresh) {
            // Invalidate the previous selection
            previouslySelectedUris = [];
            // Invalidate any remaining conversation
            window.Conversations.currentConversation = null;
            // Make the stub aware of the Conversations object it's currently
            //  representing.
            htmlpane.contentWindow.Conversations = window.Conversations;
            // The DOM window is fresh, it needs an event listener to forward
            //  keyboard shorcuts to the main window when the conversation view
            //  has focus.
            // It's crucial we register a non-capturing event listener here,
            //  otherwise the individual message nodes get no opportunity to do
            //  their own processing.
            htmlpane.contentWindow.addEventListener("keypress", function (event) {
              try {
                window.dispatchEvent(event);
              } catch (e) {
                //Log.debug("We failed to dispatch the event, don't know why...", e);
              }
            }, false);
          }

          try {
            // Should cancel most intempestive view refreshes, but only after we
            //  made sure the multimessage pane is shown. The logic behind this
            //  is the conversation in the message pane is already alive, and
            //  the gloda query is updating messages just fine, so we should not
            //  worry about message which are not in the view.
            let newlySelectedUris = [msgHdrGetUri(x) for each (x in aSelectedMessages)];
            let scrollMode = self.determineScrollMode();
            // If the scroll mode changes, we should go a little bit further
            //  down that code path, so that we can figure out that the message
            //  set is the same, but that we ought to do a round of
            //  expand/collapse + "scroll to the right message" on the current
            //  message list. We could optimize that, but I'll assume that since
            //  the message set is the same, the resulting gloda query will be
            //  fast, and this doesn't impact performance too much.
            // Anyways, this is just for the weird edge case where the user has
            //  expanded the thread and selected all messages, and then
            //  collapses the thread, which does not change the selection, but
            //  ony the scroll mode.
            if (arrayEquals(newlySelectedUris, previouslySelectedUris)
                && previousScrollMode == scrollMode) {
              Log.debug("Hey, know what? The selection hasn't changed, so we're good!");
              Services.obs.notifyObservers(null, "Conversations", "Displayed");
              return;
            }

            let freshConversation = new self._Conversation(
              window,
              aSelectedMessages,
              scrollMode,
              ++window.Conversations.counter
            );
            if (window.Conversations.currentConversation)
              Log.debug("Current conversation is", Colors.red,
                window.Conversations.currentConversation.counter, Colors.default);
            else
              Log.debug("First conversation");
            freshConversation.outputInto(htmlpane, function (aConversation) {
              if (aConversation.messages.length == 0) {
                Log.debug(Colors.red, "0 messages in aConversation");
                return;
              }
              // So we've been promoted to be the new conversation! Remember
              //  that and update the currently selected URIs to prevent further
              //  reflows.
              previouslySelectedUris = newlySelectedUris;
              previousScrollMode = scrollMode;
              // One nasty behavior of the folder tree view is that it calls us
              //  every time a new message has been downloaded. So if you open
              //  your inbox all of a sudden and select a conversation, it's not
              //  uncommon to see the conversation being rebuilt 5 times in a
              //  row because sumarizeThread is constantly re-called.
              // To workaround this, even though we create a fresh conversation,
              //  that conversation might end up recycling the old one as long
              //  as the old conversation's message set is a prefix of that of
              //  the new conversation. So because we're not sure
              //  freshConversation will actually end up being used, we take the
              //  new conversation as parameter.
              Log.assert(aConversation.scrollMode == scrollMode, "Someone forgot to put the right scroll mode!");
              // So we force a GC cycle if we change conversations, so that the
              //  previous collection is actually deleted and we don't vomit a
              //  ton of errors from the listener that tries to modify the DOM
              //  nodes and fails at it because they don't exist anymore.
              let needsGC = window.Conversations.currentConversation
                && (window.Conversations.currentConversation.counter != aConversation.counter);
              let isDifferentConversation = !window.Conversations.currentConversation
                  || (window.Conversations.currentConversation.counter != aConversation.counter);
              let prevCounter = window.Conversations.currentConversation
                ? window.Conversations.currentConversation.counter
                : 0;
              // Make sure we have a global root --> conversation --> persistent
              //  query chain to prevent the Conversation object (and its inner
              //  query) to be collected. The Conversation keeps watching the
              //  Gloda query for modified items (read/unread, starred, tags...).
              window.Conversations.currentConversation = aConversation;
              if (isDifferentConversation) {
                // Here, put the final touches to our new conversation object.
                htmlpane.contentWindow.newComposeSessionByDraftIf();
                aConversation.completed = true;
                htmlpane.contentWindow.registerQuickReply();
              }
              if (needsGC)
                Cu.forceGC();

              Services.obs.notifyObservers(null, "Conversations", "Displayed");

              // Make sure we respect the user's preferences.
              self.markReadTimeout = window.setTimeout(function () {
                if (Prefs.getBool("mailnews.mark_message_read.auto")) {
                  // The idea is that usually, we're selecting a thread (so we
                  //  have kScrollUnreadOrLast). This means we mark the whole
                  //  conversation as read. However, sometimes the user selects
                  //  individual messages. In that case, don't do something weird!
                  //  Just mark the selected messages as read.
                  if (scrollMode == Prefs.kScrollUnreadOrLast) {
                    // Did we juste change conversations? If we did, it's ok to
                    //  mark as read. Otherwise, it's not, since we may silently
                    //  mark new messages as read.
                    if (isDifferentConversation) {
                      Log.debug(Colors.red, "Marking the whole conversation as read", Colors.default);
                      aConversation.read = true;
                    }
                  } else if (scrollMode == Prefs.kScrollSelected) {
                    // We don't seem to have a reflow when the thread is expanded
                    //  so no risk of silently marking conversations as read.
                    Log.debug(Colors.red, "Marking selected messages as read", Colors.default);
                    msgHdrsMarkAsRead(aSelectedMessages, true);
                  } else {
                    Log.assert(false, "GIVE ME ALGEBRAIC DATA TYPES!!!");
                  }
                  self.markReadTimeout = null;
                }
                // Hehe, do that now, because the conversation potentially
                //  includes messages that are not in the gloda collection and
                //  that do not trigger the "conversation updated" notification.
                aConversation._updateConversationButtons();
              }, Prefs.getInt("mailnews.mark_message_read.delay.interval")
                * Prefs.getBool("mailnews.mark_message_read.delay") * 1000);
            });
          } catch (e) {
            Log.error(e);
            dumpCallStack(e);
          }
        });
      };
    this.pushUndo(function () window.summarizeThread = oldSummarizeThread);

    // Because we want to replace the standard message reader, we need to always
    //  fire up the conversation view instead of deferring to the regular
    //  display code. The trick is that re-using the original function's name
    //  allows us to intercept the calls to the thread summary in regular
    //  situations (where a normal thread summary would kick in) as a
    //  side-effect. That means we don't need to hack into gMessageDisplay too
    //  much.
    let originalOnSelectedMessagesChanged =
      window.MessageDisplayWidget.prototype.onSelectedMessagesChanged;
    window.MessageDisplayWidget.prototype.onSelectedMessagesChanged =
      function _onSelectedMessagesChanged_patched () {
        if (!Prefs.enabled) {
          return originalOnSelectedMessagesChanged.call(this);
        }

        try {
          // What a nice pun! If bug 320550 was fixed, I could print
          // \u2633\u1f426 and that would be very smart.
          // dump("\n"+Colors.red+"\u2633 New Conversation"+Colors.default+"\n");
          if (!this.active)
            return true;
          window.ClearPendingReadTimer();
          self.clearTimer();

          let selectedCount = this.folderDisplay.selectedCount;
          Log.debug("Intercepted message load, ", selectedCount, " message(s) selected");
          /*dump(Colors.red);
          for each (let msgHdr in this.folderDisplay.selectedMessages)
            dump("  " + msgHdr.folder.URI + "#" + msgHdr.messageKey + "\n");
          dump(Colors.default);*/

          if (selectedCount == 0) {
            // So we're not copying the code here. This changes nothing, and the
            // execution stays the same. But if someone (say, the account
            // summary extension) decides to redirect the code to _showSummary
            // in the case of selectedCount == 0 by monkey-patching
            // onSelectedMessagesChanged, we give it a chance to run.
            originalOnSelectedMessagesChanged.call(this);
          } else if (selectedCount == 1) {
            // Here starts the part where we modify the original code.
            let msgHdr = this.folderDisplay.selectedMessage;
            // We can't display NTTP messages and RSS messages properly yet, so
            // leave it up to the standard message reader. If the user explicitely
            // asked for the old message reader, we give up as well.
            if (msgHdrIsRss(msgHdr) || msgHdrIsNntp(msgHdr)) {
              // Use the default pref.
              self.markReadTimeout = window.setTimeout(function () {
                if (Prefs.getBool("mailnews.mark_message_read.auto"))
                  msgHdrsMarkAsRead([msgHdr], true);
                self.markReadTimeout = null;
              }, Prefs.getInt("mailnews.mark_message_read.delay.interval")
                * Prefs.getBool("mailnews.mark_message_read.delay") * 1000);
              this.singleMessageDisplay = true;
              return false;
            } else {
              // Otherwise, we create a thread summary.
              // We don't want to call this._showSummary because it has a built-in check
              // for this.folderDisplay.selectedCount and returns immediately if
              // selectedCount == 1
              this.singleMessageDisplay = false;
              window.summarizeThread(this.folderDisplay.selectedMessages, this);
              return true;
            }
          }

          // Else defer to showSummary to work it out based on thread selection.
          // (This might be a MultiMessageSummary after all!)
          return this._showSummary();
        } catch (e) {
          Log.error(e);
          dumpCallStack(e);
        }
      };
    this.pushUndo(function ()
      window.MessageDisplayWidget.prototype.onSelectedMessagesChanged = originalOnSelectedMessagesChanged);

    // Ok, this is slightly tricky. The C++ code notifies the global msgWindow
    //  when content has been blocked, and we can't really afford to just
    //  replace the code, because that would defeat the standard reader (e.g. in
    //  a new tab). So we must find the message in the conversation and notify
    //  it if needed.
    let oldOnMsgHasRemoteContent = window.messageHeaderSink.onMsgHasRemoteContent;
    window.messageHeaderSink.onMsgHasRemoteContent = function _onMsgHasRemoteContent_patched (aMsgHdr) {
      let msgListeners = window.Conversations.msgListeners;
      let messageId = aMsgHdr.messageId;
      if (messageId in msgListeners) {
        for each (let [i, listener] in Iterator(msgListeners[messageId])) {
          let obj = listener.get();
          if (obj)
            obj.onMsgHasRemoteContent();
        }
        msgListeners[messageId] = msgListeners[messageId].filter(function (x) (x.get() != null));
      }
      // Wicked case: we have the conversation and another tab with a message
      //  from the conversation in that tab. So to be safe, forward the call.
      oldOnMsgHasRemoteContent(aMsgHdr);
    };
    this.pushUndo(function () window.messageHeaderSink.onMsgHasRemoteContent = oldOnMsgHasRemoteContent);

    let messagepane = window.document.getElementById("messagepane");
    let fightAboutBlank = function () {
      if (messagepane.contentWindow.location.href == "about:blank") {
        Log.debug("Hockey-hack");
        // Workaround the "feature" that disables the context menu when the
        // messagepane points to about:blank
        messagepane.contentWindow.location.href = "about:blank?";
      }
    };
    messagepane.addEventListener("load", fightAboutBlank, true);
    this.pushUndo(function () messagepane.removeEventListener("load", fightAboutBlank, true));
    fightAboutBlank();

    Log.debug("Monkey patch successfully applied.");
  },
}
