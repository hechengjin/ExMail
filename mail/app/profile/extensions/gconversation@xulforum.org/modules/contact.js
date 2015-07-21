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

var EXPORTED_SYMBOLS = ['ContactManager', 'Contacts', 'defaultPhotoURI']

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/Services.jsm"); // https://developer.mozilla.org/en/JavaScript_code_modules/Services.jsm
Cu.import("resource:///modules/iteratorUtils.jsm"); // for fixIterator
Cu.import("resource:///modules/StringBundle.js"); // for StringBundle
Cu.import("resource:///modules/mailServices.js");
Cu.import("resource:///modules/gloda/utils.js");
Cu.import("resource:///modules/gloda/gloda.js");

Cu.import("resource://conversations/modules/stdlib/compose.js");
Cu.import("resource://conversations/modules/stdlib/misc.js");
Cu.import("resource://conversations/modules/stdlib/msgHdrUtils.js");
Cu.import("resource://conversations/modules/log.js");
Cu.import("resource://conversations/modules/misc.js");

const clipboardService = Cc["@mozilla.org/widget/clipboardhelper;1"]
                         .getService(Ci.nsIClipboardHelper);

const Contacts = {
  kFrom: 0,
  kTo: 1
}

const defaultPhotoURI = "chrome://messenger/skin/addressbook/icons/contact-generic.png";

let Log = setupLogging("Conversations.Contact");
let strings = new StringBundle("chrome://conversations/locale/message.properties");

function ContactManager() {
  this._cache = {};
  this._colorCache = {};
  this._count = 0;
}

ContactManager.prototype = {
  getContactFromNameAndEmail: function _ContactManager_getContactFromEmail(name, email, position) {
    // [name] and [email] are from the message header
    let self = this;
    email = (email+"").toLowerCase();
    // Might change in the future... who knows? ...
    let key = email;
    let cache = function _cache (name, contact) {
      for each (let [, email] in Iterator(contact.emails)) {
        email = (email+"").toLowerCase();
        self._cache[key] = contact;
      }
    };
    if (key in this._cache) {
      if (name)
        this._cache[key].enrichWithName(name);
      return this._cache[key];
    } else {
      let contact = new ContactFromAB(this, name, email, position, this._colorCache[email]);
      // Only cache contacts which are in the address book. This avoids weird
      //  phenomena such as a bug tracker sending emails with different names
      //  but with the same email address, resulting in people all sharing the
      //  same name.
      // For those that need to be in the address book (because we want to
      //  display images, for instance), the user still has the option to uncheck
      //  "prefer display name over header name".
      if (contact._useCardName) {
        cache(name, contact);
      } else {
        // We still want to cache the color...
        if (!(email in this._colorCache))
          this._colorCache[email] = contact.color;
      }
      return contact;
    }
  },

  freshColor: function _ContactManager_freshColor(aIsMe) {
    if (aIsMe) {
      return "#ed6666";
    } else {
      let predefinedColors = ["#ed8866", "#ccc15e", "#9ec269",
        "#69c2ac", "#66b7ed", "#668ced", "#8866ed", "#cb66ed", "#ed66d9"];
      if (this._count < predefinedColors.length) {
        return predefinedColors[this._count++];
      } else {
        let r, g, b;
        // Avoid colors that are too light or too dark.
        do {
          r = Math.random();
          g = Math.random();
          b = Math.random();
        } while (Math.sqrt(r*r + b*b + g*g) > .8 || Math.sqrt(r*r + b*b + g*g) < .2)
        return "rgb("+parseInt(r*255)+","+parseInt(g*255)+","+parseInt(b*255)+")";
      }
    }
  },
}

let ContactMixIn = {
  /**
   * The aEmail parameter is here because the same contact object is shared for
   * all instances of a contact, even though the original email address is
   * different. This allows one to share a common color for a same card in the
   * address book.
   */
  toTmplData: function _ContactMixIn_toInlineHtml (aUseColor, aPosition, aEmail, aIsDetail) {
    let name = this.getName(aPosition, aIsDetail);
    let tooltipName = this.getTooltipName(aPosition);
    let data = {
      showMonospace: aPosition == Contacts.kFrom,
      name: escapeHtml(name),
      tooltipName: escapeHtml((tooltipName != aEmail) ? tooltipName : ""),
      email: escapeHtml(aEmail),
      avatar: escapeHtml(this.avatar),
      profiles: this._profiles,
      // Parameter aUseColor is optional, and undefined means true
      colorStyle: ((aUseColor === false)
        ? ""
        : ("color :" + this.color)),
      writeBr: aIsDetail,
      star: false,
    };
    if (aIsDetail) {
      data.name = escapeHtml(name != aEmail
        ? MailServices.headerParser.makeFullAddress(name, aEmail)
        : aEmail);
      data.star = this._card != null;
    }
    return data;
  },

  onAddedToDom: function _ContactMixIn_onAddedToDom(aDomNode) {
    let self = this;
    this._domNode = aDomNode; // makes the line below possible
    let mainWindow = topMail3Pane(this);

    aDomNode.parentNode.getElementsByClassName("moreExpander")[0].addEventListener("click", function (event) {
      if (aDomNode.parentNode.getElementsByClassName("hiddenFooter")[0].style.display == "none") {
        aDomNode.parentNode.getElementsByClassName("hiddenFooter")[0].style.display = "block";
        event.originalTarget.firstChild.textContent = "-";
      } else {
        aDomNode.parentNode.getElementsByClassName("hiddenFooter")[0].style.display = "none";
        event.originalTarget.firstChild.textContent = "+";
      }
      event.stopPropagation();
    }, false);

    /* Register the "send message" link */
    this.register(".sendEmail", function (event) {
      let dest = (this._name == this._email || !this._name)
        ? this._email
        : MailServices.headerParser.makeFullAddress(this._name, this._email);
      dump(dest+"\n\n");
      composeMessageTo(dest, mainWindow.gFolderDisplay.displayedFolder);
      event.stopPropagation();
    }.bind(this));

    // XXX We already called getCardForEmail if we're runnning without contacts
    //  installed...
    // Please note that cardAndBook is never overridden, so that the closure for
    //  the editContact event listener actually sees the updated fields of the
    //  object once the addContact event listener has updated them.
    let cardAndBook = mainWindow.getCardForEmail(self._email);
    if (cardAndBook.card)
      aDomNode.parentNode.classList.add("inAddressBook");
    this.register(".addContact", function (event) {
      let args = {
        primaryEmail: self._email,
        displayName: self._name,
        allowRemoteContent: true,
        // This is too messed up, there's no easy way to interact with this
        //  dialog, just forget about it. RegisterSaveListener seems to be
        //  uncallable... and okCallback just short-circuit the whole logic
      };
      mainWindow.openDialog(
        "chrome://messenger/content/addressbook/abNewCardDialog.xul",
        "", "chrome,resizable=no,titlebar,modal,centerscreen", args
      );
      // This is an approximation, but it should be good enough
      let newCardAndBook = mainWindow.getCardForEmail(self._email);
      if (newCardAndBook.card) {
        cardAndBook.card = newCardAndBook.card;
        cardAndBook.book = newCardAndBook.book;
        aDomNode.parentNode.classList.add("inAddressBook");
      }
    });
    this.register(".editContact", function (event) {
      let args = {
        abURI: cardAndBook.book.URI,
        card: cardAndBook.card,
      };
      mainWindow.openDialog(
        "chrome://messenger/content/addressbook/abEditCardDialog.xul",
        "", "chrome,modal,resizable=no,centerscreen", args
      );
    });
    this.register(".copyEmail", function (event) {
      clipboardService.copyString(self._email);
    });
    this.register(".showInvolving", function (event) {
      let q1 = Gloda.newQuery(Gloda.NOUN_IDENTITY);
      q1.kind("email");
      q1.value(self._email);
      q1.getCollection({
        onItemsAdded: function _onItemsAdded(aItems, aCollection) { },
        onItemsModified: function _onItemsModified(aItems, aCollection) { },
        onItemsRemoved: function _onItemsRemoved(aItems, aCollection) { },
        onQueryCompleted: function _onQueryCompleted(aCollection) {
          if (!aCollection.items.length)
            return;

          let q2 = Gloda.newQuery(Gloda.NOUN_MESSAGE);
          q2.involves.apply(q2, aCollection.items);
          q2.getCollection({
            onItemsAdded: function _onItemsAdded(aItems, aCollection) { },
            onItemsModified: function _onItemsModified(aItems, aCollection) { },
            onItemsRemoved: function _onItemsRemoved(aItems, aCollection) { },
            onQueryCompleted: function _onQueryCompleted(aCollection) {
              let tabmail = mainWindow.document.getElementById("tabmail");
              tabmail.openTab("glodaList", {
                collection: aCollection,
                title: strings.get("involvingTabTitle").replace("#1", self._name),
                background: false
              });
            }
          });
        }
      });
    });
    this.register(".createFilter", function (event) {
      mainWindow.MsgFilters(self._email, null);
    });

    /* The links to various profiles */
    for each (let [, a] in Iterator(aDomNode.getElementsByTagName("a"))) {
      let (a = a) { // I hate you Javascript! I hate you!!!
        a.addEventListener("click",
          a.classList.contains("profile-link")
          ? function _link_listener (event) (
              mainWindow.document.getElementById("tabmail").openTab("contentTab", {
                contentPage: a.href, // (cf. supra)
                clickHandler: "specialTabs.defaultClickHandler(event);"
              }),
              event.preventDefault()
            )
          : function _link_listener (event) (
              mainWindow.specialTabs.siteClickHandler(event, /^mailto:/),
              event.preventDefault()
            ),
          false);
      }
    }
  },

  getTooltipName: function _ContactMixIn_getName (aPosition) {
    Log.assert(aPosition === Contacts.kFrom || aPosition === Contacts.kTo,
      "Someone did not set the 'position' properly");
    if (this._email in gIdentities)
      return strings.get("meFromMeToSomeone");
    else
      return this._name || this._email;
  },

  getName: function _ContactMixIn_getName (aPosition, aIsDetail) {
    Log.assert(aPosition === Contacts.kFrom || aPosition === Contacts.kTo,
      "Someone did not set the 'position' properly");
    if ((this._email in gIdentities) && !aIsDetail)
      return ((aPosition === Contacts.kFrom)
        ? strings.get("meFromMeToSomeone")
        : strings.get("meFromSomeoneToMe")
      );
    else
      return this._name || this._email;
  },

  enrichWithName: function _ContactMixIn_enrichWithName (aName) {
    if (this._name == this._email || !this._name)
      this._name = aName;
  },
};

function ContactFromAB(manager, name, email, /* unused */ position, color) {
  this.emails = [];
  this.color = color || manager.freshColor(email in gIdentities);

  this._manager = manager;
  this._name = name; // Initially, the displayed name. Might be enhanced later.
  this._email = email; // The original email. Use to pick a gravatar.
  this._profiles = {};
  this._card = null;
  this._useCardName = false;

  this.fetch();
}

ContactFromAB.prototype = {
  fetch: function _ContactFromAB_fetch() {
    let card = GlodaUtils.getCardForEmail(this._email);
    this._card = card;
    if (card) {
      // getProperty may return "0" or "1" which must be "== false"'d to be
      //  properly evaluated
      this._useCardName = (card.getProperty("PreferDisplayName", true) == true);
      this.emails = [card.primaryEmail, card.getProperty("SecondEmail", "")];
      // Prefer:
      // - displayName
      // - firstName lastName (if one of these is non-empty)
      // - the parsed name
      // - the email
      if (this._useCardName && card.displayName)
        this._name = card.displayName;
      if (this._useCardName && (card.firstName || card.lastName))
        this._name = card.firstName + " " + card.lastName;
      if (!this._name)
        this._name = this._email;
    } else {
      this.emails = [this._email];
      this._name = this._name || this._email;
    }
  },

  get avatar () {
    if (this._card) {
      let photoURI = this._card.getProperty("PhotoURI", "");
      if (photoURI)
        return photoURI;
    }
    return defaultPhotoURI;
  },
}

MixIn(ContactFromAB, ContactMixIn);
MixIn(ContactFromAB, EventHelperMixIn);
