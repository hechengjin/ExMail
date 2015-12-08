
Cu.import("resource:///modules/gloda/gloda.js");
Cu.import("resource:///modules/gloda/datastore.js");


function SearchListener(itemsAddedCallback,
                        itemsModifiedCallback,
                        itemsRemovedCallback,
                        queryCompletedCallback) {
    this._itemsAddedCallback = itemsAddedCallback;
    this._itemsModifiedCallback = itemsModifiedCallback;
    this._itemsRemovedCallback = itemsRemovedCallback;
    this._queryCompletedCallback = queryCompletedCallback;
}

SearchListener.prototype = {
    onItemsAdded: function(items, collection) {
        if (this._itemsAddedCallback) {
            this._itemsAddedCallback(items, collection);
        }
    },
    onItemsModified: function(items, collection) {
        if (this._itemsModifiedCallback) {
            this._itemsModifiedCallback(items, collection);
        }
    },
    onItemsRemoved: function(items, collection) {
        if (this._itemsRemovedCallback) {
            tis._itemsRemovedCallback(items, collection);
        }
    },
    onQueryCompleted: function(messages) {
        if (this._queryCompletedCallback) {
            this._queryCompletedCallback(messages);
        }
    }
};

var advSearchUtils = {
    formatDate: function(date) {     // 日期格式化
        return date.getFullYear() + "-"+ (date.getMonth() + 1) + "-" + date.getDate();
    },

    // 得到当天零点时刻
    toEarliestTime: function(date) {
        var newDate = new Date(date);
        newDate.setHours(0);
        newDate.setMinutes(0);
        newDate.setSeconds(0);
        return newDate;
    },

    // 得到当天23:59:59时刻
    toLatestTime: function(date) {
        var newDate = new Date(date);
        newDate.setHours(23);
        newDate.setMinutes(59);
        newDate.setSeconds(59);
        return newDate;
    },

    // 按空格拆分成多个单词，去掉空字符串，使之紧凑
    splitToCompactWords: function(text) {
        var words = text.split(" ");
        var compactWords = [];
        for (var i = 0; i < words.length; i++) {
            if (words[i] != "") {
                compactWords.push(words[i]);
            }
        }

        return compactWords;
    }
};

var advSearch = {
    curFolderSynView: {},
    curAccountSynView: {},
    globalSynView: {},

    _sqlPrefix: "",

    init: function() {
        this._sqlPrefix = "SELECT * FROM " +
            "(messages INNER JOIN messagesText ON messages.id=messagesText.rowid) " +
            "INNER JOIN folderLocations ON messages.folderID=folderLocations.id WHERE ";

        var observerService = Components.classes["@mozilla.org/observer-service;1"]
                                .getService(Components.interfaces.nsIObserverService);
        observerService.addObserver(this, "FolderShowed", false);       // 监听邮件夹显示事件

        // 初始化自定义开始和结束日期
        var now = new Date();
        var yesterday = new Date(now);
        yesterday.setDate(now.getDate() - 1);
        $("advSearchTimeFrom").date = advSearchUtils.toEarliestTime(yesterday);
        $("advSearchTimeTo").date = advSearchUtils.toLatestTime(now);

        var me = this;

        // 这些文本框在敲回车时触发搜索
        var detectedTextboxes = [$("advSearchSubject"), $("advSearchSender"), $("advSearchRecipient"),
                                 $("advSearchFullText"), $("advSearchAttachmentsNames")];
        for (var i = 0; i < detectedTextboxes.length; i++) {
            detectedTextboxes[i].addEventListener("keydown", function(event) {
                if (event.keyCode == event.DOM_VK_RETURN) {
                    me.startSearch();
                }
            });
        }
    },

    onFromDateChanged: function(target) {
			alert(1)
        var fromDate = $("advSearchTimeFrom").date;
				alert(2)
        $("advSearchTimeFrom").date = advSearchUtils.toEarliestTime(fromDate);

        if ($("advSearchTimeFrom").date.getTime() > $("advSearchTimeTo").date.getTime()) {
            $("advSearchTimeTo").date = advSearchUtils.toLatestTime(fromDate);
        }
    },

    onToDateChanged: function(target) {
        var toDate = $("advSearchTimeTo").date;
        $("advSearchTimeTo").date = advSearchUtils.toLatestTime(toDate);

        if ($("advSearchTimeFrom").date.getTime() > $("advSearchTimeTo").date.getTime()) {
            $("advSearchTimeFrom").date = advSearchUtils.toEarliestTime(toDate);
        }
    },

    onAttachmentTypeChanged: function(target) {
        if (target.selectedIndex == 1) {    // 有附件，才可填附件名
            $("advSearchAttachmentsNames").disabled = false;
        } else {
            $("advSearchAttachmentsNames").disabled = true;
        }
    },

    // 时间范围下拉列表变化
    onTimeRangeChanged: function(target) {
        if (target.value == "custom") {
            $("advSearchTimePrompt").hidden = false;
            $("advSearchTimeFrom").hidden = false;
            $("advSearchToLabel").hidden = false;
            $("advSearchTimeTo").hidden = false;
        } else {
            $("advSearchTimePrompt").hidden = true;
            $("advSearchTimeFrom").hidden = true;
            $("advSearchToLabel").hidden = true;
            $("advSearchTimeTo").hidden = true;
        }
    },

    startSearch: function() {
        var [curFolderSql, curAccountSql, globalSql] = this._generateSql();
        // 统计查询完成的次数，全部查询完成时显示结果
        var completedCount = 0;
        function queryCompleted() {
            completedCount++;
            if (completedCount == 3) {
                $("advSearchLocationBox").hidden = false;
                $("advSearchLocList").selectedIndex = 0;
                $("advSearchLocList").doCommand();
            }
        }

        var me = this;

        // 三个监听对象
        var curFolderListener = new SearchListener(null, null, null, function(messages) {
            var args = {
                collection: Gloda.explicitCollection(Gloda.NOUN_MESSAGE, messages.items)
            };

            me.curFolderSynView = new GlodaSyntheticView(args);
            queryCompleted();
        });

        var curAccountListener = new SearchListener(null, null, null, function(messages) {
            var args = {
                collection: Gloda.explicitCollection(Gloda.NOUN_MESSAGE, messages.items)
            };

            me.curAccountSynView = new GlodaSyntheticView(args);
            queryCompleted();
        });

        var globalListener = new SearchListener(null, null, null, function(messages) {
            var args = {
                collection: Gloda.explicitCollection(Gloda.NOUN_MESSAGE, messages.items)
            };

            me.globalSynView = new GlodaSyntheticView(args);
            queryCompleted();
        });
        // 在三个位置查询
        var query = Gloda.newQuery(Gloda.NOUN_MESSAGE);
        var messageNoun = Gloda.lookupNounDef("message");

        GlodaDatastore._queryFromSQLString(curFolderSql,
            [], messageNoun, query, curFolderListener, null, null, null);

        GlodaDatastore._queryFromSQLString(curAccountSql,
            [], messageNoun, query, curAccountListener, null, null, null);

        GlodaDatastore._queryFromSQLString(globalSql,
            [], messageNoun, query, globalListener, null, null, null);

    },

    // 当前邮件夹、当前帐户、全局之间的变化
    onLocationChanged: function(target) {
        var index = target.selectedIndex;
        if (index == 0) {
            gFolderDisplay.show(this.curFolderSynView);
        } else if (index == 1) {
            gFolderDisplay.show(this.curAccountSynView);
        } else if (index == 2) {
            gFolderDisplay.show(this.globalSynView);
        }

        gFolderDisplay.makeActive();
    },

    // 产生多个模糊匹配表达式，keyArray用AND连接，即同时满足多个关键字
    //  fieldNameArray用OR连接，即其中一个字段满足即可
    _generateFuzzyMatch: function(fieldNameArray, keyArray) {
        var newFieldNames = new Array().concat(fieldNameArray);

        for (var f = 0; f < newFieldNames.length; f++) {
            var newKeys = new Array().concat(keyArray);

            for (var k = 0; k < newKeys.length; k++) {
                newKeys[k] = "(" + newFieldNames[f] + " LIKE '%" + newKeys[k] + "%')";
            }

            newFieldNames[f] = "(" + newKeys.join(" AND ") + ")";
        }

        return "(" + newFieldNames.join(" OR ") + ")";
    },

    // 用于关键字高亮
    _matchKeys: {},
    get matchKeys() {
        return this._matchKeys;
    },

    _combineSearchConditionSqlString: function() {
        var conditionArray = ["(deleted=0)"];

        this._matchKeys = {
            subject: [],
            sender: [],
            recipient: [],
            body: []
        };

        var subject = strHelper.trim($("advSearchSubject").value);
        if (subject != "") {
            var keyArray = advSearchUtils.splitToCompactWords(subject);
            this._matchKeys.subject = keyArray;

            conditionArray.push(this._generateFuzzyMatch(["subject"], keyArray));
        }

        var sender = strHelper.trim($("advSearchSender").value);
        if (sender != "") {
            var keyArray = advSearchUtils.splitToCompactWords(sender);
            this._matchKeys.sender = keyArray;

            conditionArray.push(this._generateFuzzyMatch(["author"], keyArray));
        }

        var recipient = strHelper.trim($("advSearchRecipient").value);
        if (recipient != "") {
            var keyArray = advSearchUtils.splitToCompactWords(recipient);
            this._matchKeys.recipient = keyArray;

            conditionArray.push(this._generateFuzzyMatch(["recipients"], keyArray));
        }

        // 全文匹配，逻辑关系为：主题/发件人/收件人/正文 其中之一满足关键字
        var fullText = strHelper.trim($("advSearchFullText").value);
        if (fullText != "") {
            var keyArray = advSearchUtils.splitToCompactWords(fullText);
            this._matchKeys.subject     = this._matchKeys.subject.concat(keyArray);
            this._matchKeys.sender      = this._matchKeys.sender.concat(keyArray);
            this._matchKeys.recipient   = this._matchKeys.recipient.concat(keyArray);
            this._matchKeys.body        = keyArray;

            conditionArray.push(this._generateFuzzyMatch(
                ["subject", "author", "recipients", "body"], keyArray));
        }

        // 附件
        var attachType = $("advSearchAttachmentType").selectedIndex;
        if (attachType == 1) {  // 有附件
            var attachName = strHelper.trim($("advSearchAttachmentsNames").value);
            if (attachName != "") {
                conditionArray.push("(attachmentNames LIKE '%" + attachName + "%')");
            } else {
                conditionArray.push("(attachmentNames<>'')");
            }
        } else if (attachType == 2) {   // 无附件
            conditionArray.push("(attachmentNames='')");
        }

        // 时间范围
        var timeValue = $("advSearchTimeRange").value;
        if (timeValue == "unlimited") { // 不限，不添加条件

        } else if (timeValue == "custom") {     // 自定义，计算两个指定的时间框
            var timeFrom = $("advSearchTimeFrom").date;
            var timeTo = $("advSearchTimeTo").date;

            conditionArray.push("(date >= " + timeFrom * 1000 + ")");
            conditionArray.push("(date <= " + timeTo * 1000 + ")");
        } else {    // 其他选项，根据value，往前推时间
            // 比如今天的值为1，起始时间为当天零点，而不是昨天零点，所以这里要有个减1
            var daysBefore = parseInt(timeValue) - 1;
            var timeFrom = advSearchUtils.toEarliestTime(new Date()).getTime() - (daysBefore * 24 * 3600 * 1000);

            // Date对象最小单位是毫秒，数据库存的是微妙
            conditionArray.push("(date >= " + timeFrom * 1000 + ")");
        }

        var condition = "";
        if (conditionArray.length == 0) {
            condition = "(1)";
        } else {
            condition = "(" + conditionArray.join(" AND ") + ")";
        }

        return condition;
    },

    _generateSql: function() {
        var selectedFolders = gFolderTreeView.getSelectedFolders();
        var curFolder = null;
        var curRootFolder = null;

        var fixedSql = this._sqlPrefix + this._combineSearchConditionSqlString();
        var curFolderSql = "",
            curAccountSql = "",
            globalSql = fixedSql;

        if (selectedFolders.length > 0) {
            curFolder = selectedFolders[0];
            curRootFolder = curFolder.rootFolder;

            curFolderSql = fixedSql + " AND folderURI='" + curFolder.URI + "'";
            curAccountSql = fixedSql + " AND folderURI LIKE '%" +
                curRootFolder.URI.replace(/%/g, "%%") + "%'";   // 转义特殊字符%
        }

        return [curFolderSql, curAccountSql, globalSql];
    },

    observe: function(subject, topic, data) {
        if (topic == "FolderShowed") {  // 从搜索结果跳转到普通邮件夹/帐户，隐藏下拉框
            if (data == "account" || data == "folder") {
                $("advSearchLocationBox").hidden = true;
            }
        }
    }
};

window.addEventListener("load", function() {
    advSearch.init();		
}, false);

