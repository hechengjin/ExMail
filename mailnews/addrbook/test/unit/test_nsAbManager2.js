/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * Test suite for nsAbManager functions relating to add/delete directories and
 * getting the list of directories..
 */

const nsIAbDirectory = Components.interfaces.nsIAbDirectory;
const nsIAbListener = Components.interfaces.nsIAbListener;
const numListenerOptions = 4;

var gAblAll;
var gAblSingle = new Array(numListenerOptions);

function abL() {}

abL.prototype = {
 mReceived: 0,
 mDirectory: null,
 mAutoRemoveItem: false,

  onItemAdded: function (parentItem, item) {
    this.mReceived |= nsIAbListener.itemAdded;
    this.mDirectory = item;
    if (this.mAutoRemoveItem)
      MailServices.ab.removeAddressBookListener(this);
  },
  onItemRemoved: function (parentItem, item) {
    this.mReceived |= nsIAbListener.directoryRemoved;
    this.mDirectory = item;
    if (this.mAutoRemoveItem)
      MailServices.ab.removeAddressBookListener(this);
  },
  onItemPropertyChanged: function (item, property, oldValue, newValue) {
    this.mReceived |= nsIAbListener.itemChanged;
    this.mDirectory = item;
    if (this.mAutoRemoveItem)
      MailServices.ab.removeAddressBookListener(this);
  }
};

function checkDirs(aDirs, aDirArray) {
  // Don't modify the passed in array.
  var dirArray = aDirArray.concat();

  while (aDirs.hasMoreElements()) {
    var dir = aDirs.getNext().QueryInterface(nsIAbDirectory);
    var loc = dirArray.indexOf(dir.URI);

    do_check_eq(MailServices.ab.getDirectory(dir.URI), dir);

    if (loc == -1)
      do_throw("Unexpected directory " + dir.URI + " found in address book list");
    else
      dirArray[loc] = null;
  }

  dirArray.forEach(function(value) { do_check_eq(value, null); });
}

function addDirectory(dirName) {
  // Add the directory
  MailServices.ab.newAddressBook(dirName, "", kPABData.dirType);

  // Check for correct notifications
  do_check_eq(gAblAll.mReceived, nsIAbListener.itemAdded);

  var newDirectory = gAblAll.mDirectory.QueryInterface(nsIAbDirectory);

  gAblAll.mReceived = 0;
  gAblAll.mDirectory = null;

  for (var i = 0; i < numListenerOptions; ++i) {
    if (1 << i == nsIAbListener.itemAdded) {
      do_check_eq(gAblSingle[i].mReceived, nsIAbListener.itemAdded);
      gAblSingle[i].mReceived = 0;
    }
    else
      do_check_eq(gAblSingle[i].mReceived, 0);
  }

  return newDirectory;
}

function removeDirectory(directory) {
  // Remove the directory
  MailServices.ab.deleteAddressBook(directory.URI);

  // Check correct notifications
  do_check_eq(gAblAll.mReceived, nsIAbListener.directoryRemoved);
  do_check_eq(gAblAll.mDirectory, directory);

  gAblAll.mReceived = 0;
  gAblAll.mDirectory = null;

  for (var i = 0; i < numListenerOptions; ++i) {
    if (1 << i == nsIAbListener.directoryRemoved) {
      do_check_eq(gAblSingle[i].mReceived, nsIAbListener.directoryRemoved);
      gAblSingle[i].mReceived = 0;
    }
    else
      do_check_eq(gAblSingle[i].mReceived, 0);
  }
}

function run_test() {
  var i;

  // Set up listeners
  gAblAll = new abL;
  MailServices.ab.addAddressBookListener(gAblAll, nsIAbListener.all);

  for (i = 0; i < numListenerOptions; ++i) {
    gAblSingle[i] = new abL;
    MailServices.ab.addAddressBookListener(gAblSingle[i], 1 << i);
  }

  var expectedABs = [kPABData.URI, kCABData.URI];

  // Check the OS X Address Book if available
  if ("@mozilla.org/addressbook/directory;1?type=moz-abosxdirectory" in
      Components.classes)
    expectedABs.push(kOSXData.URI);

  // Test - Check initial directories

  checkDirs(MailServices.ab.directories, expectedABs);

  // Test - Add a directory

  var newDirectory1 = addDirectory("testAb1");

  // Test - Check new directory list
  expectedABs.push(newDirectory1.URI);

  checkDirs(MailServices.ab.directories, expectedABs);

  // Test - Repeat for a second directory

  var newDirectory2 = addDirectory("testAb2");

  // Test - Check new directory list
  expectedABs.push(newDirectory2.URI);

  checkDirs(MailServices.ab.directories, expectedABs);

  // Test - Remove a directory

  var pos = expectedABs.indexOf(newDirectory1.URI);

  expectedABs.splice(pos, 1);

  removeDirectory(newDirectory1);
  newDirectory1 = null;

  // Test - Check new directory list

  checkDirs(MailServices.ab.directories, expectedABs);

  // Test - Repeat the removal

  removeDirectory(newDirectory2);
  newDirectory2 = null;

  expectedABs.pop();

  // Test - Check new directory list
  checkDirs(MailServices.ab.directories, expectedABs);

  // Test - Clear the listeners down

  MailServices.ab.removeAddressBookListener(gAblAll);

  for (i = 0; i< numListenerOptions; ++i)
    MailServices.ab.removeAddressBookListener(gAblSingle[i]);
};
