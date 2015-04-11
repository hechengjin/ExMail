/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var Ci = Components.interfaces;
var Cc = Components.classes;
var Cu = Components.utils;

var elib = {};
Cu.import('resource://mozmill/modules/elementslib.js', elib);
var mozmill = {};
Cu.import('resource://mozmill/modules/mozmill.js', mozmill);
var EventUtils = {};
Cu.import('resource://mozmill/stdlib/EventUtils.js', EventUtils);
var utils = {};
Cu.import('resource://mozmill/modules/utils.js', utils);

const MODULE_NAME = 'account-manager-helpers';
const RELATIVE_ROOT = '../shared-modules';

// we need this for the main controller
const MODULE_REQUIRES = ['folder-display-helpers', 'window-helpers'];

var wh, fdh, mc;

function setupModule() {
  fdh = collector.getModule('folder-display-helpers');
  mc = fdh.mc;
  wh = collector.getModule('window-helpers');
}

function installInto(module) {
  setupModule();

  // Now copy helper functions
  module.open_advanced_settings = open_advanced_settings;
  module.open_advanced_settings_from_account_wizard =
    open_advanced_settings_from_account_wizard;
  module.click_account_tree_row = click_account_tree_row;
}

/**
 * Opens the Account Manager.
 *
 * @param callback Callback for the modal dialog that is opened.
 */
function open_advanced_settings(aCallback, aController) {
  if (aController === undefined)
    aController = mc;

  wh.plan_for_modal_dialog("mailnews:accountmanager", aCallback);
  aController.click(mc.eid("menu_accountmgr"));
  return wh.wait_for_modal_dialog("mailnews:accountmanager");
}

/**
 * Opens the Account Manager from the mail account setup wizard.
 *
 * @param callback Callback for the modal dialog that is opened.
 */
function open_advanced_settings_from_account_wizard(aCallback, aController) {
  wh.plan_for_modal_dialog("mailnews:accountmanager", aCallback);
  aController.e("manual-edit_button").click();
  aController.e("advanced-setup_button").click();
  return wh.wait_for_modal_dialog("mailnews:accountmanager");
}

/**
 * Click a row in the account settings tree
 *
 * @param controller the Mozmill controller for the account settings dialog
 * @param rowIndex the row to click
 */
function click_account_tree_row(controller, rowIndex) {
  utils.waitFor(function () controller.window.currentAccount != null,
                "Timeout waiting for currentAccount to become non-null");

  var tree = controller.window.document.getElementById("accounttree");
  var selection = tree.view.selection;
  selection.select(rowIndex);
  tree.treeBoxObject.ensureRowIsVisible(rowIndex);

  // get cell coordinates
  var x = {}, y = {}, width = {}, height = {};
  var column = tree.columns[0];
  tree.treeBoxObject.getCoordsForCellItem(rowIndex, column, "text",
                                           x, y, width, height);

  controller.sleep(0);
  EventUtils.synthesizeMouse(tree.body, x.value + 4, y.value + 4,
                             {}, tree.ownerDocument.defaultView);
  controller.sleep(0);

  utils.waitFor(function () controller.window.pendingAccount == null,
                "Timeout waiting for pendingAccount to become null");
}
