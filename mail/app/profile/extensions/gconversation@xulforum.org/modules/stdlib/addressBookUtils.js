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
 * The Original Code is Mail utility functions for GMail Conversation View
 *
 * The Initial Developer of the Original Code is
 * Jonathan Protzenko
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

/**
 * @fileoverview This small file provides conveniences wrappers around legacy
 * address book interfaces.
 * @author Jonathan Protzenko
 */

var EXPORTED_SYMBOLS = [
  'kPersonalAddressBookUri', 'kCollectedAddressBookUri',
  'getAddressBookFromUri', 'saveEmailInAddressBook'
];

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;
const Cr = Components.results;

const rdfService = Cc["@mozilla.org/rdf/rdf-service;1"]
                   .getService(Ci.nsIRDFService);
const abManager = Cc["@mozilla.org/abmanager;1"]
                  .getService(Ci.nsIAbManager);

/**
 * The "Personal addresses" address book
 * @const
 */
const kPersonalAddressBookUri = "moz-abmdbdirectory://abook.mab";
/**
 * The "Collected addresses" address book
 * @const
 */
const kCollectedAddressBookUri = "moz-abmdbdirectory://history.mab";

/**
 * Get one of the predefined address books through their URI.
 * @param aUri the URI of the address book, use one of the consts above
 * @return nsIAbDirectory the address book
 */
function getAddressBookFromUri(aUri) {
  return abManager.getDirectory(aUri);
}

/**
 * Just a one-liner to add an email to the given address book, without any extra
 *  properties.
 * @param aBook the nsIAbDirectory
 * @param aEmail the email
 * @param aName (optional) the name
 * @return nsIAbCard
 */
function saveEmailInAddressBook(aBook, aEmail, aName) {
  let card = Cc["@mozilla.org/addressbook/cardproperty;1"]  
             .createInstance(Ci.nsIAbCard);
  //card.setProperty("FirstName", "John");
  //card.setProperty("LastName", "Smith");
  card.displayName = aName;
  card.primaryEmail = aEmail;
  card.setProperty("AllowRemoteContent", true);
  return aBook.addCard(card);
}
