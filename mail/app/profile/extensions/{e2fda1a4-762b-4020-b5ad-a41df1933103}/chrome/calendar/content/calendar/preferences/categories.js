/**
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
Components.utils.import("resource://calendar/modules/calUtils.jsm");

var gCategoryList;
var prefService = Components.classes["@mozilla.org/preferences-service;1"]
                    .getService(Components.interfaces.nsIPrefService);
var categoryPrefBranch = prefService.getBranch("calendar.category.color.");

/**
 * Global Object to hold methods for the categories pref pane
 */
var gCategoriesPane = {

    /**
     * Initialize the categories pref pane. Sets up dialog controls to show the
     * categories saved in preferences.
     */
    init: function gCP_init() {
        // On non-instant-apply platforms, once this pane has been loaded,
        // attach our "revert all changes" function to the parent prefwindow's
        // "ondialogcancel" event.
        var parentPrefWindow = document.documentElement;
        if (!parentPrefWindow.instantApply) {
            var existingOnDialogCancel = parentPrefWindow.getAttribute("ondialogcancel");
            parentPrefWindow.setAttribute("ondialogcancel",
                                          "gCategoriesPane.panelOnCancel(); " + 
                                          existingOnDialogCancel);
        }

        // A list of preferences to be reverted when the dialog is cancelled.
        // It needs to be a property of the parent to be visible onCancel
        if (!("backupPrefList" in parent)) {
            parent.backupPrefList = [];
        }

        let categories = document.getElementById("calendar.categories.names").value;

        // If no categories are configured load a default set from properties file
        if (!categories) {
            categories = cal.setupDefaultCategories();
            document.getElementById("calendar.categories.names").value = categories;
        }

        gCategoryList = categoriesStringToArray(categories);
        
        // When categories is empty, split returns an array containing one empty
        // string, rather than an empty array. This results in an empty listbox
        // child with no corresponding category.
        if (gCategoryList.length == 1 && !gCategoryList[0].length) {
            gCategoryList.pop();
        }

        this.updateCategoryList();
    },

    /**
     * Updates the listbox containing the categories from the categories saved
     * in preferences.
     */

    updatePrefs: function gCP_updatePrefs() {
        cal.sortArrayByLocaleCollator(gCategoryList);
        document.getElementById("calendar.categories.names").value =
            categoriesArrayToString(gCategoryList);
    },

    updateCategoryList: function gCP_updateCategoryList () {
        this.updatePrefs();
        let listbox = document.getElementById("categorieslist");

        listbox.clearSelection();
        this.updateButtons();


        while (listbox.lastChild.id != "categoryColumns")
            listbox.removeChild(listbox.lastChild);

        for (var i=0; i < gCategoryList.length; i++) {
            var newListItem = document.createElement("listitem");
            var categoryName = document.createElement("listcell");
            categoryName.setAttribute("id", gCategoryList[i]);
            categoryName.setAttribute("label", gCategoryList[i]);
            var categoryNameFix = formatStringForCSSRule(gCategoryList[i]);
            var categoryColor = document.createElement("listcell");
            try {
                var colorCode = categoryPrefBranch.getCharPref(categoryNameFix);
                categoryColor.setAttribute("id", colorCode);
                categoryColor.setAttribute("style","background-color: "+colorCode+';');
            } catch (ex) {
                categoryColor.setAttribute("label", noneLabel);
            }
 
            newListItem.appendChild(categoryName);
            newListItem.appendChild(categoryColor);
            listbox.appendChild(newListItem);
        }
    },

    /**
     * Adds a category, opening the edit category dialog to prompt the user to
     * set up the category.
     */
    addCategory: function gCP_addCategory() {
        let listbox = document.getElementById("categorieslist");
        listbox.clearSelection();
        this.updateButtons();
        window.openDialog("chrome://calendar/content/preferences/editCategory.xul",
                          "addCategory", "modal,centerscreen,chrome,resizable=no",
                          "", null, addTitle);
    },

    /**
     * Edits the currently selected category using the edit category dialog.
     */
    editCategory: function gCP_editCategory() {
        var list = document.getElementById("categorieslist");
        var categoryNameFix = formatStringForCSSRule(gCategoryList[list.selectedIndex]);
        try {
            var currentColor = categoryPrefBranch.getCharPref(categoryNameFix);
        } catch (ex) {
            var currentColor = null;
        }
 
        if (list.selectedItem) {
            window.openDialog("chrome://calendar/content/preferences/editCategory.xul",
                              "editCategory", "modal,centerscreen,chrome,resizable=no",
                              gCategoryList[list.selectedIndex], currentColor, editTitle);
        }
    },

    /**
     * Removes the selected category.
     */
    deleteCategory: function gCP_deleteCategory() {
        let list = document.getElementById("categorieslist");
        if (list.selectedCount < 1) {
            return;
        }

        let categoryNameFix = formatStringForCSSRule(gCategoryList[list.selectedIndex]);
        this.backupData(categoryNameFix);
        try {
            categoryPrefBranch.clearUserPref(categoryNameFix);
        } catch (ex) {
        }

        // Remove category entry from listbox and gCategoryList.
        let newSelection = list.selectedItem.nextSibling ||
                           list.selectedItem.previousSibling;
        let selectedItems = Array.slice(list.selectedItems).concat([]);
        for (let i = list.selectedCount - 1; i >= 0; i--) {
            let item = selectedItems[i];
            if (item == newSelection) {
                newSelection = newSelection.nextSibling ||
                               newSelection.previousSibling;
            }
            gCategoryList.splice(list.getIndexOfItem(item), 1);
            list.removeChild(item);
        }
        list.selectedItem = newSelection;
        this.updateButtons();

        // Update the prefs from gCategoryList
        this.updatePrefs();
    },

    /**
     * Saves the given category to the preferences.
     *
     * @param categoryName      The name of the category.
     * @param categoryColor     The color of the category
     */
    saveCategory: function gCP_saveCateogry(categoryName, categoryColor) {
        var list = document.getElementById("categorieslist");
        // Check to make sure another category doesn't have the same name
        var toBeDeleted = -1;
        var promptService = 
             Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                       .getService(Components.interfaces.nsIPromptService);
        for (var i=0; i < gCategoryList.length; i++) {
            if (i == list.selectedIndex)
                continue;
            if (categoryName.toLowerCase() == gCategoryList[i].toLowerCase()) {
                if (promptService.confirm(null, overwriteTitle, overwrite)) {
                    if (list.selectedIndex != -1)
                        // Don't delete the old category yet. It will mess up indices.
                        toBeDeleted = list.selectedIndex;
                    list.selectedIndex = i;
                } else {
                    return;
                }
            }
        }

        if (categoryName.length == 0) {
            promptService.alert(null, null, noBlankCategories);
            return;
        }

        var categoryNameFix = formatStringForCSSRule(categoryName);
        if (list.selectedIndex == -1) {
            this.backupData(categoryNameFix);
            gCategoryList.push(categoryName);
            if (categoryColor)
                categoryPrefBranch.setCharPref(categoryNameFix, categoryColor);
        } else {
            this.backupData(categoryNameFix);
            gCategoryList.splice(list.selectedIndex, 1, categoryName);
            if (categoryColor) {
                categoryPrefBranch.setCharPref(categoryNameFix, categoryColor);
            } else {
                try {
                    categoryPrefBranch.clearUserPref(categoryNameFix);
                } catch (ex) {
                    dump("Exception caught in 'saveCategory': " + ex + "\n");
                }
            }
        }

        // If 'Overwrite' was chosen, delete category that was being edited
        if (toBeDeleted != -1) {
            list.selectedIndex = toBeDeleted;
            this.deleteCategory();
        }

        this.updateCategoryList();

        var updatedCategory = gCategoryList.indexOf(categoryName);
        list.ensureIndexIsVisible(updatedCategory); 
        list.selectedIndex = updatedCategory;
    },

    /**
     * Enable the edit and delete category buttons.
     */
    updateButtons: function  gCP_updateButtons() {
        let categoriesList = document.getElementById("categorieslist");
        document.getElementById("deleteCButton").disabled = (categoriesList.selectedCount <= 0);
        document.getElementById("editCButton").disabled = (categoriesList.selectedCount != 1)
    },

    /**
     * Backs up the category name in case the dialog is canceled.
     *
     * @see formatStringForCSSRule
     * @param categoryNameFix     The formatted category name.
     */
    backupData: function gCP_backupData(categoryNameFix) {
        var currentColor;
        try {
            currentColor = categoryPrefBranch.getCharPref(categoryNameFix);
        } catch (ex) {
            dump("Exception caught in 'backupData': " + ex + "\n");
            currentColor = "##NEW";
        }

        for (var i=0; i < parent.backupPrefList.length; i++) {
            if (categoryNameFix == parent.backupPrefList[i].name) {
                return;
            }
        }
        parent.backupPrefList[parent.backupPrefList.length] =
            { name : categoryNameFix, color : currentColor };
    },

    /**
     * Event Handler function to be called on doubleclick of the categories
     * list. If the edit function is enabled and the user doubleclicked on a
     * list item, then edit the selected category.
     */
    listOnDblClick: function gCP_listOnDblClick(event) {
        if (event.target.localName == "listitem" &&
            !document.getElementById("editCButton").disabled) {
            this.editCategory();
        }
    },

    /**
     * Reverts category preferences in case the cancel button is pressed.
     */
    panelOnCancel: function gCP_panelOnCancel() {
        for (var i=0; i < parent.backupPrefList.length; i++) {
            if (parent.backupPrefList[i].color == "##NEW") {
                try {
                   categoryPrefBranch.clearUserPref(parent.backupPrefList[i].name);
                } catch (ex) {
                    dump("Exception caught in 'panelOnCancel': " + ex + "\n");
                }
            } else {
                categoryPrefBranch.setCharPref(parent.backupPrefList[i].name,
                                               parent.backupPrefList[i].color);
            }
        }
    }
};
