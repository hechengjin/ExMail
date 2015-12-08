
Components.utils.import("resource://gre/modules/Services.jsm");

var ac = Components.classes["@mozilla.org/steel/application;1"]
    .getService(Components.interfaces.steelIApplication).console;


var gIsWideStyle = true;     // 是否是宽风格
var gIsSenderDisplay = true;    // 当前邮件夹是否显示发件人列而不是收件人列

// 保存到全局，是因为dbview为空时也可记忆上一次排序状态
var gSortType = null;       // 当前dbview的排序类型
var gSortOrder = null;      // 当前dbview的升降序

var cellType = {
    status:             1,
    flagged:            2,
    attach:             3,
    senderRecipient:    4,
    subject:            5,
    time:               6,
    size:               7
};

var itemPosition = {
    top:    1,
    center: 2,
    bottom: 3
};

var loadReason = {
    noReason        : 0,
    created         : 10,
    wideToNarrow    : 20,
    narrowToWide    : 30,
    msgAdded        : 40,
    msgsClassified  : 50,
    msgsDeleted     : 60,
    msgsMoveCopy    : 70,
    sort            : 80
};

const ONE_DAY_MILLISECONDS = 24 * 60 * 60 * 1000;

var msgDateTimeHelper = {
    convertPRTimeToString: function(prTime) {
        var now = new Date();
        var dateTime = new Date(prTime / 1000);

        if (gIsWideStyle) {
            var dateStr = "";
            var timeStr = "";

            if (dateTime.getFullYear() == now.getFullYear() &&
                dateTime.getMonth() == now.getMonth() &&
                dateTime.getDate() == now.getDate()) {  // 今天，不格式化日期
                dateStr = "";
            } else {
                dateStr = dateTime.getFullYear() + "-" + (dateTime.getMonth() + 1) + "-" + dateTime.getDate() + " ";
            }

            var hour = dateTime.getHours();
            var minute = dateTime.getMinutes();
            if (hour < 10) {
                hour = "0" + hour;
            }

            if (minute < 10) {
                minute = "0" + minute;
            }

            timeStr = hour + ":" + minute;

            return dateStr + timeStr;
        } else {
            var now = new Date();
            var dateStr = "";

            if (dateTime.getFullYear() == now.getFullYear() &&
                dateTime.getMonth() == now.getMonth() &&
                dateTime.getDate() == now.getDate()) {
                var hour = dateTime.getHours();
                var minute = dateTime.getMinutes();
                if (hour < 10) {
                    hour = "0" + hour;
                }

                if (minute < 10) {
                    minute = "0" + minute;
                }

                return hour + ":" + minute;
            } else {
                dateStr = dateTime.getFullYear() + "-" + (dateTime.getMonth() + 1) + "-" + dateTime.getDate();
                return dateStr;
            }


        }
    },

    isToday: function(msgHdr) {
        var now = new Date();
        var dt = new Date(msgHdr.date / 1000);
        var todayZero = new Date(now.getFullYear(), now.getMonth(), now.getDate());

        return (dt >= todayZero);
    },

    isYesterday: function(msgHdr) {
        var yesterday = new Date(Date.now() - ONE_DAY_MILLISECONDS);
        var dt = new Date(msgHdr.date / 1000);

        return (yesterday.getFullYear() == dt.getFullYear() &&
            yesterday.getMonth() == dt.getMonth() &&
            yesterday.getDate() == dt.getDate());
    },

    // Date.getDay() 将0表示为周天，用此函数将0表示为周一，方便归类
    getDay: function(date) {
        return (date.getDay() + 6) % 7;
    },

    isThisWeekday: function(msgHdr, day) {
        var prTime = msgHdr.date / 1000;

        var thisMonday = new Date();
        while (this.getDay(thisMonday) > 0) {
            thisMonday = new Date(thisMonday.getTime() - ONE_DAY_MILLISECONDS);
        }

        thisMonday = new Date(thisMonday.getFullYear(),
            thisMonday.getMonth(),
            thisMonday.getDate());

        return (prTime >= (thisMonday.getTime() + day * ONE_DAY_MILLISECONDS) &&
            prTime < (thisMonday.getTime() + (day+1) * ONE_DAY_MILLISECONDS));
    },

    isThisFriday: function(msgHdr) {
        return this.isThisWeekday(msgHdr, 4);
    },

    isThisThursday: function(msgHdr) {
        return this.isThisWeekday(msgHdr, 3);
    },

    isThisWednesday: function(msgHdr) {
        return this.isThisWeekday(msgHdr, 2);
    },

    isThisTuesday: function(msgHdr) {
        return this.isThisWeekday(msgHdr, 1);
    },

    isThisMonday: function(msgHdr) {
        return this.isThisWeekday(msgHdr, 0);
    },

    isLastWeek: function(msgHdr) {
        var prTime = msgHdr.date / 1000;

        var thisMonday = new Date();
        while (this.getDay(thisMonday) > 0) {
            thisMonday = new Date(thisMonday.getTime() - ONE_DAY_MILLISECONDS);
        }

        thisMonday = new Date(thisMonday.getFullYear(),
            thisMonday.getMonth(),
            thisMonday.getDate());

        var lastMonday = new Date(thisMonday.getTime() - 7 * ONE_DAY_MILLISECONDS);

        return (prTime >= lastMonday.getTime() &&
            prTime < thisMonday.getTime());
    },

    isThisMonth: function(msgHdr) {
        var now = new Date();
        var dt = new Date(msgHdr.date / 1000);

        return (now.getMonth() == dt.getMonth());
    },

    isLastMonth: function(msgHdr) {
        var now = new Date();
        var dt = new Date(msgHdr.date / 1000);

        var monthSpan = (12 * now.getFullYear() + now.getMonth())
            - (12 * dt.getFullYear() + dt.getMonth());

        return (monthSpan == 1);
    },

    isEarly: function(msgHdr) {
        var now = new Date();
        var months = now.getFullYear() * 12 + now.getMonth() - 1;
        var lastMonth = new Date(months / 12, months % 12);

        return ((msgHdr.date / 1000) < lastMonth.getTime());
    }
};

var timeSpanClassified = [
    { func: msgDateTimeHelper.isToday,          title: "today" },
    { func: msgDateTimeHelper.isYesterday,      title: "yesterday" },
    { func: msgDateTimeHelper.isThisFriday,     title: "this Friday" },
    { func: msgDateTimeHelper.isThisThursday,   title: "this Thursday" },
    { func: msgDateTimeHelper.isThisWednesday,  title: "this Wednesday" },
    { func: msgDateTimeHelper.isThisTuesday,    title: "this Tuesday" },
    { func: msgDateTimeHelper.isThisMonday,     title: "this Monday" },
    { func: msgDateTimeHelper.isLastWeek,       title: "last week" },
    { func: msgDateTimeHelper.isThisMonth,      title: "this month" },
    { func: msgDateTimeHelper.isLastMonth,      title: "last month" },
    { func: msgDateTimeHelper.isEarly,          title: "early" }
];

const MB_SIZE = 1024 * 1024;
const KB_SIZE = 1024;

var msgSizeHelper = {
    isHuge: function(hdr) {
        return (hdr.messageSize >= 5 * MB_SIZE);
    },

    isVeryLarge: function(hdr) {
        return (hdr.messageSize >= 1 * MB_SIZE &&
            hdr.messageSize < 5 * MB_SIZE);
    },

    isLarge: function(hdr) {
        return (hdr.messageSize >= 500 * KB_SIZE &&
            hdr.messageSize < 1 * MB_SIZE);
    },

    isMedium: function(hdr) {
        return (hdr.messageSize >= 100 * KB_SIZE &&
            hdr.messageSize < 500 * KB_SIZE);
    },

    isSmall: function(hdr) {
        return (hdr.messageSize >= 25 * KB_SIZE &&
            hdr.messageSize < 100 * KB_SIZE);
    },

    isTiny: function(hdr) {
        return (hdr.messageSize < 25 * KB_SIZE);
    },

    formatSize: function(size) {
        var afterDot = 1;

        if (size < 800) {
            return size + " B";
        } else {
            var kb = size / 1024;
            if (kb < 800) {
                return kb.toFixed(afterDot) + " KB";
            } else {
                var mb = kb / 1024;
                if (mb < 800) {
                    return mb.toFixed(afterDot) + " MB";
                } else {
                    var gb = mb / 1024;
                    return gb.toFixed(afterDot) + " GB";
                }
            }
        }

        return "";
    }
};

var sizeClassified = [
    { func: msgSizeHelper.isHuge,       title: "huge" },
    { func: msgSizeHelper.isVeryLarge,  title: "very large" },
    { func: msgSizeHelper.isLarge,      title: "large" },
    { func: msgSizeHelper.isMedium,     title: "medium" },
    { func: msgSizeHelper.isSmall,      title: "small" },
    { func: msgSizeHelper.isTiny,       title: "tiny" }
];

var eltHelper = {
    computedStyle: function(elt) {
        return elt.ownerDocument.defaultView.getComputedStyle(elt);
    },

    getWidth: function(elt) {
        return this.computedStyle(elt).width;
    },

    getHeight: function(elt) {
        return this.computedStyle(elt).height;
    },

    /* 样式管理 */
    hasClass: function(ele,cls) {
        return ele.className.match(new RegExp('(\\s|^)'+cls+'(\\s|$)'));
    },

    addClass: function(ele,cls) {
        if (!this.hasClass(ele,cls)) ele.className += " "+cls;
    },

    removeClass: function(ele,cls) {
        if (this.hasClass(ele,cls)) {
            var reg = new RegExp('(\\s|^)'+cls+'(\\s|$)');
            ele.className=ele.className.replace(reg,' ');
        }
    },

    toggleClass: function(ele,cls) {
        if (this.hasClass(ele,cls)){
            this.removeClass(ele,cls);
        }
        else
            this.addClass(ele,cls);
    },

    changeClass: function(ele,oldcls,newcls) {
        if (!this.hasClass(ele,newcls)) {
            if(this.hasClass(ele,oldcls)){
                this.removeClass(ele,oldcls);
            }
            this.addClass(ele,newcls);
        }
    },
    /* 样式管理 结束 */

    createElt: function(tag) {
        return document.createElementNS("http://www.w3.org/1999/xhtml", tag);
    },

    createText: function(text) {
        return document.createTextNode(text);
    },

    remove: function(elt) {
        elt.parentNode.removeChild(elt);
    },

    clearChildNodes: function(elt) {
        while (elt.firstChild != null) {
            elt.removeChild(elt.firstChild);
        }
    }

};

var strHelper = {
    trim: function(str) {
        return str.replace(/^\s+|\s+$/g, "");
    },

    ltrim: function(str) {
        return str.replace(/^\s+/, "");
    },

    rtrim: function(str) {
        return str.replace(/\s+$/, "");
    }
};

var trHelper = {
    _bundle: null,

    init: function() {
        var strBundleService = Components.classes["@mozilla.org/intl/stringbundle;1"].
            getService(Components.interfaces.nsIStringBundleService);
        this._bundle = strBundleService.createBundle(
            "chrome://messenger/locale/searchGeneral/searchGeneral.properties");
    },

    tr: function(src) {
        return this._bundle.GetStringFromName(src);
    }

};

var abHelper = {
    _abManager: null,
    _allAbDirs: null,
    _recorded: {},

    init: function() {
        this._abManager = Components.classes["@mozilla.org/abmanager;1"]
            .getService(Components.interfaces.nsIAbManager);

        this._allAbDirs = this._abManager.directories;
    },

    searchNameByEmail: function(email) {
        var recordedName = this._recorded[email];
        if (recordedName != undefined) {
            return recordedName;
        }

        var query = "(or(PrimaryEmail,c," + encodeURIComponent(email) + "))";

        while (this._allAbDirs.hasMoreElements()) {
            var dir = this._allAbDirs.getNext();
            if (dir instanceof Ci.nsIAbDirectory) {
                var cards = this._abManager.getDirectory(dir.URI + "?" + query).childCards;
                if (cards.hasMoreElements()) {
                    var aCard = cards.getNext();
                    if (aCard instanceof Ci.nsIAbCard) {
                        this._recorded[email] = aCard.displayName;
                        return aCard.displayName;
                    }
                }
            }
        }

        this._recorded[email] = undefined;
        return "";
    },

    parseAbName: function(senderRaw) {
        var regEmail = /^[^<]*<\s*([^> ]*)\s*>\s*$/i;
        var regName = /^([^<]*)<.*$/i;

        var email = senderRaw.match(regEmail);
        if (email == null) {
            email = senderRaw;
        } else {
            email = email[1];
        }

        var name = senderRaw.match(regName);
        if (name == null) {
            name = "";
        } else {
            name = strHelper.trim(name[1]);
            name = name.replace(/"/g, "");
        }

        var senderText = "";
        if (name != "") {
            senderText = name;
        } else {
            var abName = this.searchNameByEmail(email);
            if (abName == "") {
                senderText = email;
            } else {
                senderText = abName;
            }
        }

        if (email != "") {
            var abName = this.searchNameByEmail(email);
            if (abName == "") {
                if (name == "") {
                    senderText = email;
                } else {
                    senderText = name;
                }
            } else {
                senderText = abName;
            }
        } else {
            senderText = email;
        }

        return senderText;
    }
};


var htmlThreadList = {
    threadTree: null,
    dbView: null,

    _allMsgObjs: [],   // 所有邮件头，元素为 { elt, hdr, data }

    _msgListBox: null,
    _msgListBody: null,

    _loadReason: 0,

    init: function() {
        this.threadTree = $("threadTree");
        this._msgListBox = $("htmlThreadListBox");
        this._msgListBody = $("htmlThreadListBody");

        trHelper.init();
        abHelper.init();

        this.clear();

        this.startSizeWatcher();
        this.bindColsEvent();

        $("narrowSortType").onmousedown = function(event) {
            $("narrowColsPopup").openPopup(this, 'after_start', 0, 0, true);
        };

        $("narrowSortAsc").onmousedown = function(event) {
            $("narrowColsPopup").openPopup($("narrowSortType"), 'after_start', 0, 0, true);
        };

        $("narrowSortDesc").onmousedown = function(event) {
            $("narrowColsPopup").openPopup($("narrowSortType"), 'after_start', 0, 0, true);
        };

        var me = this;
        this._msgListBox.onscroll = function(event) {
            var scrollTop = me._msgListBox.scrollTop;
            me.recordScroll({scrollTop: scrollTop,
                scrollPercent: scrollTop / me._msgListBox.scrollHeight});
            me.cut(scrollTop);
        };

        window.addEventListener("keydown", function(event) {
            me.onKeyDown(event);
        });

        window.addEventListener("resize", function(event) {
        }, false);

        var observerService = Cc["@mozilla.org/observer-service;1"]
            .getService(Ci.nsIObserverService);
        observerService.addObserver(this, "MsgCreateDBView", false);    // 监听DBView创建事件
        observerService.addObserver(this, "MsgTagsChanged", false);     // 监听标签变化事件
        observerService.addObserver(this, "TagSettingChanged", false);  // 监听标签设置的变化
        observerService.addObserver(this, "MsgMarkRead", false);        // 监听邮件已读/未读状态改变
        observerService.addObserver(this, "MsgMarkFlagged", false);     // 监听邮件星标改变
        observerService.addObserver(this, "FolderShowed", false);       // 监听邮件夹显示事件
        observerService.addObserver(this, "TreeSelectionChanged", false);   // 监听原列表的选中变化
        observerService.addObserver(this, "MsgMarkAllRead", false);     // 监听标记邮件夹为已读操作
        observerService.addObserver(this, "DBViewDoCommand", false);    // 监听FolderDisplayWidget_doCommand
        observerService.addObserver(this, "am_openmail", false);    // 监听 附件管理打开邮件
        observerService.addObserver(this, "am_selnattach", false);    // 监听 附件管理选择了附件
		observerService.addObserver(this, "recallAction", false);
		observerService.addObserver(this, "FolderRefresh", false);

        // 监听邮件通知
        var notificationService =
            Components.classes["@mozilla.org/messenger/msgnotificationservice;1"].
                getService(Components.interfaces.nsIMsgFolderNotificationService);
        notificationService.addListener(this,
            Components.interfaces.nsIMsgFolderNotificationService.msgAdded |
                Components.interfaces.nsIMsgFolderNotificationService.msgsClassified |
                Components.interfaces.nsIMsgFolderNotificationService.msgsDeleted |
                Components.interfaces.nsIMsgFolderNotificationService.msgsMoveCopyCompleted |
                Components.interfaces.nsIMsgFolderNotificationService.msgKeyChanged |
                Components.interfaces.nsIMsgFolderNotificationService.folderAdded |
                Components.interfaces.nsIMsgFolderNotificationService.folderDeleted |
                Components.interfaces.nsIMsgFolderNotificationService.folderMoveCopyCompleted |
                Components.interfaces.nsIMsgFolderNotificationService.folderRenamed |
                Components.interfaces.nsIMsgFolderNotificationService.itemEvent);

        $("narrowSortTitle").textContent = trHelper.tr("sort");

        $("senderRecipientSortAsc").hidden = true;
        $("senderRecipientSortDesc").hidden = true;
        $("subjectSortAsc").hidden = true;
        $("subjectSortDesc").hidden = true;
        $("timeSortAsc").hidden = true;
        $("timeSortDesc").hidden = true;
        $("sizeSortAsc").hidden = true;
        $("sizeSortDesc").hidden = true;

    },

    loadMessages: function(reason) {
        var beginTime, endTime;

        try {
            beginTime = new Date().getTime();

            this._selectedIndices = [];

            this._loadReason = reason;

            gSortType = this.dbView.sortType;
            gSortOrder = this.dbView.sortOrder;

            var folder = this.dbView.msgFolder;

            // 草稿箱和已发送要显示收件人列，而不是默认的发送人列
            if (folder) {
                if (folder.getFlag(Ci.nsMsgFolderFlags.Drafts) ||
                    folder.getFlag(Ci.nsMsgFolderFlags.SentMail)) {
                    gIsSenderDisplay = false;
                } else {
                    gIsSenderDisplay = true;
                }
            } else {
                gIsSenderDisplay = true;
            }

            this.updateHeader();

            this.clear();

            var msgCount = this.dbView.rowCount;

            if (this.isAdvSearchDBView() && msgCount == 0) {
                this.showNoSearchResults(); // 显示无搜索结果
            }

            ac.log("msgCount: " + msgCount);

            for (var i = 0; i < msgCount; i++) {
                var msgHdr = this.dbView.getMsgHdrAt(i);

                var itemElt = this.createItemElt(i, msgHdr);
                this._msgListBody.appendChild(itemElt);

                var msgObj = {};
                msgObj.elt = itemElt;
                msgObj.hdr = msgHdr;

                msgObj.data = {};
                msgObj.data.senderRecipient = this.getSenderRecipientContent(msgHdr);
                msgObj.data.subject = this.getSubjectContent(msgHdr);

                this._allMsgObjs.push(msgObj);
            }
        } catch (ex) {
            ac.log("loadMessages: " + ex);
        }

        this.showMoreMailsPrompt();
        this.insertClassifiedLines();   // 插入分类行
        this.updateSelection();     // 同步当前选中项

        // 初始化元素数据，为优化加载做准备
        (function() {
            var rows = this._msgListBody.childNodes;
            var msgItemHeight = (gIsWideStyle ? 28 : 67);
            var classifiedItemHeight = 20;
            var height = 0;
            for (var i = 1; i < rows.length; i++) {
                var rowElt = rows[i];
                var type = rowElt.getAttribute("type");
                var rowHeight = (type == "MsgItem") ? msgItemHeight : classifiedItemHeight;

                // 记录偏移高度和元素高度，因为元素在隐藏时offsetTop和clientHeight是为0的
                if (type == "MsgItem") {
                    var msgIndex = parseInt(rowElt.getAttribute("msgIndex"));
                    this._allMsgObjs[msgIndex].data.offsetTop = height;
                    this._allMsgObjs[msgIndex].data.clientHeight = rowHeight;
                }

                height += rowHeight;

                // 所有行先全部隐藏
                rowElt.style.display = "none";
            }
        }).apply(this);

        var preScrollTop = 0;
        var scroll = this.getScroll();
        if (scroll) {
            preScrollTop = scroll.scrollTop;
        } else {
            preScrollTop = 0;
        }

        switch (this._loadReason) {
            case loadReason.created:
                this.locate(preScrollTop);
                break;
            case loadReason.sort:
            case loadReason.wideToNarrow:
            case loadReason.narrowToWide:
                if (this._selectedIndices.length) { // 如果有选中项，让第一个选中项居中显示
                    var index = this._selectedIndices[0];
                    var itemElt = this.findItemEltByIndex(index);
                    this.ensureItemVisible(itemElt, itemPosition.center);
                } else {
                    this.locate(preScrollTop);
                }
                break;
            default:
                this.locate(preScrollTop);
                break;
        }

        endTime = new Date().getTime();
        ac.log("loading time: " + (endTime - beginTime));
    },

    delayLoadMessages: function(reason) {
        var me = this;
        setTimeout(function() {
            me.loadMessages(reason);
        }, 10);
    },

    clear: function() {
        this._allMsgObjs = [];
        while (true) {
            var firstItem = this._msgListBody.firstChild;
            if (firstItem == null) {
                break;
            }

            firstItem.onmousedown = null;
            firstItem.ondblclick = null;
            this._msgListBody.removeChild(firstItem);
        }

        this.insertFirstRow();
        this._msgListBox.style.paddingTop = "0";
        this._msgListBox.style.paddingBottom = "0";

    },

    createItemElt: function(index, msgHdr) {
        var itemElt = null;
        if (gIsWideStyle) {
            itemElt = this.createWideItemElt(msgHdr);
        } else {
            itemElt = this.createNarrowItemElt(msgHdr);
        }

        itemElt.setAttribute("type", "MsgItem");    // 类型，MsgItem或ClassifiedItem
        itemElt.setAttribute("msgIndex", index);    // 序号，重要！可在_allMsgObjs中取相应对象

        // 标记内容未填充，交给fillItemElt来做
        itemElt.setAttribute("completed", "false");

        if (!(msgHdr.flags & Ci.nsMsgMessageFlags.Read)) {
            eltHelper.addClass(itemElt, "htmlMsgUnread");
        } else {
            eltHelper.removeClass(itemElt, "htmlMsgUnread");
        }

        var me = this;

        // 元素事件处理
        itemElt.onmousedown = function(event) {
            $("messengerWindow").focus();       // 为了方便键盘控制，此处转移焦点
            me.onItemSelected(this, event);
        };

        itemElt.ondblclick = function(event) {
            folder = msgHdr.folder;
            if (folder.getFlag(Ci.nsMsgFolderFlags.Drafts)) {
                MsgComposeDraftMessage();       // 草稿箱邮件双击为编辑操作
            } else {
                MsgOpenNewWindowForMessage();   // 其他邮件夹为查看邮件
            }
        };

        var statusCell = this.getCellElt(itemElt, cellType.status);
        statusCell.onmousedown = function(event) {
            me.markMessageAsRead(itemElt,
                !(msgHdr.flags & Ci.nsMsgMessageFlags.Read));

            event.stopPropagation();
        };

        var flaggedCell = this.getCellElt(itemElt, cellType.flagged);
        flaggedCell.onmousedown = function(event) {
            var newFlagged = !msgHdr.isFlagged;
            me.markMessageAsFlagged(itemElt, newFlagged);

            event.stopPropagation();
        };

        return itemElt;
    },

    createWideItemElt: function(msgHdr) {
        var itemElt =  eltHelper.createElt("tr");
        itemElt.className = "itemElt";
        itemElt.setAttribute("wideMode", "true");

        var statusCell = eltHelper.createElt("td");
        statusCell.setAttribute("wideMode", "true");

        var flaggedCell = eltHelper.createElt("td");
        flaggedCell.setAttribute("wideMode", "true");

        var attachCell = eltHelper.createElt("td");
        attachCell.setAttribute("wideMode", "true");

        var senderRecipientCell = eltHelper.createElt("td");
        senderRecipientCell.setAttribute("wideMode", "true");

        var subjectCell = eltHelper.createElt("td");
        subjectCell.setAttribute("wideMode", "true");

        var timeCell = eltHelper.createElt("td");
        timeCell.setAttribute("wideMode", "true");

        var sizeCell = eltHelper.createElt("td");
        sizeCell.setAttribute("wideMode", "true");

        itemElt.appendChild(statusCell);
        itemElt.appendChild(flaggedCell);
        itemElt.appendChild(attachCell);
        itemElt.appendChild(senderRecipientCell);
        itemElt.appendChild(subjectCell);
        itemElt.appendChild(timeCell);
        itemElt.appendChild(sizeCell);

        return itemElt;
    },

    createNarrowItemElt: function(msgHdr) {
        var itemElt =  eltHelper.createElt("tr");

        itemElt.setAttribute("wideMode", "false");
        itemElt.className = "itemElt";
        var itemCell = eltHelper.createElt("td");

        var upLine = eltHelper.createElt("div");
        upLine.className = "narrowUpLine";
        var downLine = eltHelper.createElt("div");
        downLine.className = "narrowDownLine";

        var statusCell = eltHelper.createElt("div");
        statusCell.setAttribute("wideMode", "false");

        var senderRecipientCell = eltHelper.createElt("div");
        senderRecipientCell.setAttribute("wideMode", "false");

        var attachCell = eltHelper.createElt("div");
        attachCell.setAttribute("wideMode", "false");

        var timeCell = eltHelper.createElt("div");
        timeCell.setAttribute("wideMode", "false");

        var flaggedCell = eltHelper.createElt("div");
        flaggedCell.setAttribute("wideMode", "false");

        var subjectCell = eltHelper.createElt("div");
        subjectCell.setAttribute("wideMode", "false");

        upLine.appendChild(statusCell);
        upLine.appendChild(senderRecipientCell);
        upLine.appendChild(attachCell);
        upLine.appendChild(timeCell);

        downLine.appendChild(flaggedCell);
        downLine.appendChild(subjectCell);

        itemCell.appendChild(upLine);
        itemCell.appendChild(downLine);

        itemElt.appendChild(itemCell);

        return itemElt;
    },

    getSenderRecipientContent: function(msgHdr) {
        if (gIsSenderDisplay) { // 发件人只有一个
            return abHelper.parseAbName(msgHdr.mime2DecodedAuthor);
        } else {    // 收件人可能有多个
            var recs = msgHdr.mime2DecodedRecipients.split(",");
            var result = [];
            for (var i in recs) {
                result.push(abHelper.parseAbName(recs[i]));
            }

            return result.join(", ");
        }
    },

    getSubjectContent: function(msgHdr) {
        if (msgHdr.mime2DecodedSubject == "") {
            return trHelper.tr("no subject");
        } else {
            return msgHdr.mime2DecodedSubject;
        }
    },

    getCellElt: function(itemElt, cltype) {
        try {
            if (gIsWideStyle) {
                switch (cltype) {
                    case cellType.status:
                        return itemElt.childNodes[0];
                        break;
                    case cellType.flagged:
                        return itemElt.childNodes[1];
                        break;
                    case cellType.attach:
                        return itemElt.childNodes[2];
                        break;
                    case cellType.senderRecipient:
                        return itemElt.childNodes[3];
                        break;
                    case cellType.subject:
                        return itemElt.childNodes[4];
                        break;
                    case cellType.time:
                        return itemElt.childNodes[5];
                        break;
                    case cellType.size:
                        return itemElt.childNodes[6];
                        break;
                    default:
                        break;
                }
            } else {
                switch (cltype) {
                    case cellType.status:
                        return itemElt.childNodes[0].childNodes[0].childNodes[0];
                        break;
                    case cellType.flagged:
                        return itemElt.childNodes[0].childNodes[1].childNodes[0];
                        break;
                    case cellType.attach:
                        return itemElt.childNodes[0].childNodes[0].childNodes[2];
                        break;
                    case cellType.senderRecipient:
                        return itemElt.childNodes[0].childNodes[0].childNodes[1];
                        break;
                    case cellType.subject:
                        return itemElt.childNodes[0].childNodes[1].childNodes[1];
                        break;
                    case cellType.time:
                        return itemElt.childNodes[0].childNodes[0].childNodes[3];
                        break;
                    case cellType.size:
                        return null;
                    default:
                        break;
                }
            }
        } catch(ex) {
            return null;
        }

        return null;
    },

    fillItemElt: function(itemElt, msgHdr) {
        var statusCell = this.getCellElt(itemElt, cellType.status);
        this.fillStatusCell(statusCell, msgHdr);

        var flaggedCell = this.getCellElt(itemElt, cellType.flagged);
        this.fillFlaggedCell(flaggedCell, msgHdr);

        var attachCell = this.getCellElt(itemElt, cellType.attach);
        this.fillAttachCell(attachCell, msgHdr);

        var senderRecipientCell = this.getCellElt(itemElt, cellType.senderRecipient);
        this.fillSenderRecipientCell(itemElt, senderRecipientCell, msgHdr);

        var subjectCell = this.getCellElt(itemElt, cellType.subject);
        this.fillSubjectCell(itemElt, subjectCell, msgHdr);

        var timeCell = this.getCellElt(itemElt, cellType.time);
        this.fillTimeCell(timeCell, msgHdr);

        var sizeCell = this.getCellElt(itemElt, cellType.size);
        this.fillSizeCell(sizeCell, msgHdr);
    },

    fillStatusCell: function(statusCell, msgHdr) {
        if (!statusCell)
            return;

        eltHelper.clearChildNodes(statusCell);

        statusCell.className = "statusCell";
        var statusDiv = eltHelper.createElt("div");
		
		// 邮件召回 定时邮件 敏感词邮件状态显示
		let timingMail = msgHdr.getStringProperty("TimeMail");
		let recallMail = msgHdr.getStringProperty("RecallMail");
		let sensitiveMail = msgHdr.getStringProperty("SensitiveMail");
		
		var timing = (timingMail != "0" && timingMail != "" && timingMail != null);
		var allBack = (recallMail == "1");
		var partBack = (recallMail == "2");
		var allNotBack = (recallMail == "0");
		var sensitive = (sensitiveMail == "1");
		/*
		if (timingMail || allBack || partBack || allNotBack || sensitive)
			ac.log(msgHdr.mime2DecodedSubject + " test = " + timing + " " + allBack + " " + partBack + " " + allNotBack + " " + sensitive);
		*/
		
        var readed = Ci.nsMsgMessageFlags.Read;
        var replied = Ci.nsMsgMessageFlags.Replied;
        var forwarded = Ci.nsMsgMessageFlags.Forwarded;

        var hdrFlag = msgHdr.flags;
		if (timing) {
			statusCell.setAttribute("status", "timing");
		} else if (allBack) {
			statusCell.setAttribute("status", "allBack");
		} else if (partBack) {
			statusCell.setAttribute("status", "partBack");
		} else if (allNotBack) {
			statusCell.setAttribute("status", "allNotBack");
		} else if (sensitive) {
			statusCell.setAttribute("status", "sensitive");
		} else if (!(hdrFlag & readed)) {
			statusCell.setAttribute("status", "unread");
		} else if ((hdrFlag & replied) && (hdrFlag & forwarded)) {
            statusCell.setAttribute("status", "repliedAndForward");
        } else if (hdrFlag & replied) {
            statusCell.setAttribute("status", "replied");
        } else if (hdrFlag & forwarded) {
            statusCell.setAttribute("status", "forwarded");
        } else {
            statusCell.setAttribute("status", "normal");
        }

        statusCell.appendChild(statusDiv);
    },

    fillFlaggedCell: function(flaggedCell, msgHdr) {
        if (!flaggedCell)
            return;

        eltHelper.clearChildNodes(flaggedCell);

        flaggedCell.className = "flaggedCell";
        var flaggedDiv = eltHelper.createElt("div");

        if (msgHdr.isFlagged) {
            flaggedCell.setAttribute("flagged", "true");
        } else {
            flaggedCell.setAttribute("flagged", "false");
        }

        flaggedCell.appendChild(flaggedDiv);
    },

    fillAttachCell: function(attachCell, msgHdr) {
        if (!attachCell)
            return;

        eltHelper.clearChildNodes(attachCell);

        attachCell.className = "attachCell";
        attachCell.appendChild(eltHelper.createElt("div"));
        attachCell.setAttribute("attach",
            msgHdr.flags & Ci.nsMsgMessageFlags.Attachment ? "true" : "false");
    },

    fillSenderRecipientCell: function(itemElt, senderRecipientCell, msgHdr) {
        if (!senderRecipientCell)
            return;

        eltHelper.clearChildNodes(senderRecipientCell);

        senderRecipientCell.className = "senderRecipientCell";
        var wrapperDiv = eltHelper.createElt("div");

        var index = parseInt(itemElt.getAttribute("msgIndex"));
        var textNode = eltHelper.createText(this._allMsgObjs[index].data.senderRecipient);
        wrapperDiv.appendChild(textNode);

        // 发件人/收件人高亮匹配
        if (this.isAdvSearchDBView()) {
            var matchKeys = advSearch.matchKeys;
            var UtilsModule = {};
            Components.utils.import("resource://gre/modules/accessibility/Utils.jsm", UtilsModule);

            UtilsModule.HighlightUtil.highlight(wrapperDiv,
                gIsSenderDisplay ? matchKeys.sender : matchKeys.recipient);
        }

        senderRecipientCell.appendChild(wrapperDiv);
    },

    fillSubjectCell: function(itemElt, subjectCell, msgHdr) {
        if (!subjectCell)
            return;

        eltHelper.clearChildNodes(subjectCell);

        subjectCell.className = "subjectCell";
        var subTextDiv = eltHelper.createElt("div");

        var index = parseInt(itemElt.getAttribute("msgIndex"));
        var subText = eltHelper.createText(this._allMsgObjs[index].data.subject);
        subTextDiv.appendChild(subText);

        // 匹配关键字高亮
        if (this.isAdvSearchDBView()) {
            var matchKeys = advSearch.matchKeys;
            var UtilsModule = {};
            Components.utils.import("resource://gre/modules/accessibility/Utils.jsm", UtilsModule);
            UtilsModule.HighlightUtil.highlight(subTextDiv, matchKeys.subject);
        }

        eltHelper.addClass(subTextDiv, "subjectElt");
        subjectCell.appendChild(subTextDiv);

        var tags = this.createTags(msgHdr);
        if (tags != null) {     // 有标签
            subjectCell.setAttribute("hasTags", "true");
            subjectCell.appendChild(tags);
        } else {
            subjectCell.setAttribute("hasTags", "false");
            subTextDiv.style.width = "100%";
        }
    },

    fillTimeCell: function(timeCell, msgHdr) {
        if (!timeCell)
            return;

        eltHelper.clearChildNodes(timeCell);

        timeCell.className = "timeCell";
        var timeTxtElt = eltHelper.createElt("div");
        timeTxtElt.appendChild(eltHelper.createText(msgDateTimeHelper.convertPRTimeToString(msgHdr.date)));
        timeCell.appendChild(timeTxtElt);
    },

    fillSizeCell: function(sizeCell, msgHdr) {
        if (!sizeCell)
            return;

        eltHelper.clearChildNodes(sizeCell);

        sizeCell.className = "sizeCell";
        var sizeTxtElt = eltHelper.createElt("div");
        sizeTxtElt.appendChild(eltHelper.createText(msgSizeHelper.formatSize(msgHdr.messageSize)));
        sizeCell.appendChild(sizeTxtElt);

    },

    createTags: function(msgHdr) {
        var keywords = msgHdr.getStringProperty("keywords");
        if (keywords == "") {
            return null;
        }

        var wrapper = eltHelper.createElt("div");
        eltHelper.addClass(wrapper, "tagsWrapper");

        var tagService = Components.classes["@mozilla.org/messenger/tagservice;1"]
            .getService(Components.interfaces.nsIMsgTagService);

        var keywordArray = keywords.split(" ");
        // 后添加的标签摆在前面
        for (var i = keywordArray.length - 1; i >= 0; i--) {
            try {
                var aKeyword = keywordArray[i];
                var tagName = tagService.getTagForKey(aKeyword);    // 碰到坏的tag，抛出异常，不做处理
                var tagColor = tagService.getColorForKey(aKeyword);

                var tagElt = eltHelper.createElt("div");
                var textNode = eltHelper.createText(tagName);
                tagElt.appendChild(textNode);
                tagElt.style.backgroundColor = tagColor;
                eltHelper.addClass(tagElt, "tagElt");

                wrapper.appendChild(tagElt);
            } catch (ex) {
            }
        }

        return wrapper;
    },

    refreshTaggedItems: function() {
        for (var i in this._allMsgObjs) {
            var itemElt = this._allMsgObjs[i].elt;
            var msgHdr = this._allMsgObjs[i].hdr;

            var subjectCell = this.getCellElt(itemElt, cellType.subject);
            if (subjectCell.getAttribute("hasTags") == "true") {
                eltHelper.clearChildNodes(subjectCell);
                this.fillSubjectCell(itemElt, subjectCell, msgHdr);
                this.adjustItemElt(itemElt);
            }
        }
    },

    adjustItemElt: function(itemElt) {
        // 主题和标签拼在一起，调整一下排列
        var subjectCell = this.getCellElt(itemElt, cellType.subject);
        if (subjectCell == null) {
            return;
        }

        if (subjectCell.getAttribute("hasTags") == "true") {
            var subElt = subjectCell.childNodes[0];
            var tagsElt = subjectCell.childNodes[1];

            subElt.style.width = "calc(100% - " + tagsElt.clientWidth + "px)";
        }
    },

    _folderScroll: {},  // 邮件夹URI和scroll对象的键值对
    recordScroll: function(scroll) {
        if (this.dbView) {
            var folder = this.dbView.msgFolder;
            if (folder) {
                this._folderScroll[folder.URI] = scroll;
            }
        }
    },

    getScroll: function() {
        if (this.dbView) {
            var folder = this.dbView.msgFolder;
            if (folder) {
                return this._folderScroll[folder.URI];
            }
        }

        return null;
    },

    locate: function(scrollTop) {
        // 这两行顺序不能颠倒
        this.cut(scrollTop);
        this._msgListBox.scrollTop = scrollTop;
    },

    cut: function(scrollTop) {
        if (scrollTop === undefined) {
            scrollTop = this._msgListBox.scrollTop;
        }

        if (scrollTop < 0) {
            scrollTop = 0;
        }

        var rows = this._msgListBody.childNodes;

        var paddingTop = 0,
            paddingBottom = 0;

        var clientHeight = this._msgListBox.clientHeight;

        var msgItemHeight = (gIsWideStyle ? 28 : 67);
        var classifiedItemHeight = 20;

        var height = 0;
        for (var i = 1; i < rows.length; i++) {
            var rowElt = rows[i];

            // 碰到折叠的item，不计算其高度，并将其隐藏
            var shouldHide = rowElt.getAttribute("shouldHide");
            if (shouldHide == "true") {
                rowElt.style.display = "none";
                continue;
            }

            var type = rowElt.getAttribute("type");
            var rowHeight = (type == "MsgItem") ? msgItemHeight : classifiedItemHeight;

            height += rowHeight;

            if (scrollTop - height > 100) {     // 上方的隐藏
                paddingTop += rowHeight;
                rowElt.style.display = "none";
            } else if (height - (scrollTop + clientHeight) > 100) {     // 下方的隐藏
                paddingBottom += rowHeight;
                rowElt.style.display = "none";
            } else {    // 客户区内的显示，如果是首次显示，加载细节信息
                if (rowElt.getAttribute("completed") == "false") {
                    var msgIndex = parseInt(rowElt.getAttribute("msgIndex"));
                    this.fillItemElt(rowElt, this._allMsgObjs[msgIndex].hdr);
                    rowElt.removeAttribute("completed");
                }

                rowElt.style.display = "table-row";
                this.adjustItemElt(rowElt);
            }
        }

        this._msgListBox.style.paddingTop = paddingTop + "px";
        this._msgListBox.style.paddingBottom = paddingBottom + "px";
    },

    onDBViewCreated: function(dbView) {
        if (!gDBView) {
            return;
        }

        if (!(gDBView instanceof Ci.nsIMsgDBView)) {
            return;
        }

        // 使用传入的dbView有时会出错。如打开一封邮件，再关闭邮件窗口，
        //  拖拽列表使其使宽变窄，或由窄变宽，加载的为空
        this.dbView = gDBView;

        this.loadMessages(loadReason.created);
    },

    isQuickSearchDBView: function() {
        return this.dbView.viewType == Components.interfaces.nsMsgViewType.eShowQuickSearchResults;
    },

    isAdvSearchDBView: function() {
        return this.dbView.viewType == Components.interfaces.nsMsgViewType.eShowSearch;
    },

    markMessageAsRead: function(itemElt, read) {
        var msgHdr = this.findMsgHdrByElt(itemElt);
        if (msgHdr == null) {
            return;
        }

        var array = Components.classes["@mozilla.org/array;1"]
            .createInstance(Components.interfaces.nsIMutableArray);
        array.appendElement(msgHdr, false);

        msgHdr.folder.markMessagesRead(array, read);

        if (read) {     // 标为已读
            eltHelper.removeClass(itemElt, "htmlMsgUnread");
        } else {        // 标为未读
            eltHelper.addClass(itemElt, "htmlMsgUnread");
        }

        // 更新状态图标
        var statusCell = this.getCellElt(itemElt, cellType.status);
        this.fillStatusCell(statusCell, msgHdr);
    },

    markMessageAsFlagged: function(itemElt, flagged) {
        var msgHdr = this.findMsgHdrByElt(itemElt);
        if (msgHdr == null) {
            return;
        }

        var array = Components.classes["@mozilla.org/array;1"]
            .createInstance(Components.interfaces.nsIMutableArray);
        array.appendElement(msgHdr, false);
        msgHdr.folder.markMessagesFlagged(array, flagged);

        var flaggedCell = this.getCellElt(itemElt, cellType.flagged);
        this.fillFlaggedCell(flaggedCell, msgHdr);

        // 当前邮件星标改变，通知给读信面板和读信窗口
        if (this.getSelectionArray().length == 1 && this.itemIsSelected(itemElt)) {
            var observerService = Components.classes["@mozilla.org/observer-service;1"]
                .getService(Components.interfaces.nsIObserverService);
            observerService.notifyObservers(null, "CurrentMsgFlagChanged", "");
        }
    },

    _classifiedItems: [],    // 归类条目
    insertClassifiedItem: function(text, objItem) {
        var insertItem = eltHelper.createElt("tr");

        insertItem.className = "htmlClassified";
        var td = eltHelper.createElt("td");
        td.setAttribute("colspan", gIsWideStyle ? "7" : "1");

        insertItem.setAttribute("type", "ClassifiedItem");
        insertItem.setAttribute("expandChildren", "true");

        // 三角图片和分类文字
        var expandedDiv = eltHelper.createElt("div");
        expandedDiv.className = "classifiedExpanded";
        var collapsedDiv = eltHelper.createElt("div");
        collapsedDiv.className = "classifiedCollapsed";
        collapsedDiv.hidden = true;
        var textDiv = eltHelper.createElt("div");
        textDiv.className = "classifiedText";
        textDiv.appendChild(eltHelper.createText(text));

        td.appendChild(expandedDiv);
        td.appendChild(collapsedDiv);
        td.appendChild(textDiv);
        insertItem.appendChild(td);

        var me = this;
        insertItem.onmousedown = function(event) {
            if (event.button != 0) {
                return;
            }

            var hasExpand = this.getAttribute("expandChildren");
            if (hasExpand == "true") {
                hasExpand = "false";
                this.firstChild.childNodes[0].hidden = true;
                this.firstChild.childNodes[1].hidden = false;
            } else {
                hasExpand = "true";
                this.firstChild.childNodes[0].hidden = false;
                this.firstChild.childNodes[1].hidden = true;
            }

            var sibling = this.nextSibling;
            while (true) {
                if (sibling == null)
                    break;

                if (sibling.hasAttribute("expandChildren")) {
                    break;
                }

                if (hasExpand == "true") {
                    sibling.removeAttribute("shouldHide");
                } else {
                    sibling.setAttribute("shouldHide", "true");
                }

                sibling = sibling.nextSibling;
            }

            this.setAttribute("expandChildren", hasExpand);

            me.cut();
        };

        objItem.parentNode.insertBefore(insertItem, objItem);
        this._classifiedItems.push(insertItem);
    },

    // 插入首行，为了控制各列宽
    insertFirstRow: function() {
        var firstRow = eltHelper.createElt("tr");
        firstRow.setAttribute("id", "htmlFirstRow");

        var colCount = gIsWideStyle ? 7 : 1;
        for (var i = 0; i < colCount; i++) {
            var td = eltHelper.createElt("td");
            firstRow.appendChild(td);
        }

        $("htmlThreadListBody").appendChild(firstRow);
        this.adjustBodyCols();
    },

    showNoSearchResults: function() {
        var tr = eltHelper.createElt("tr");
        var td = eltHelper.createElt("td");
        td.setAttribute("colspan", gIsWideStyle ? "7" : "1");

        var div = eltHelper.createElt("div");
        div.setAttribute("align", "center");
        var text = eltHelper.createText(trHelper.tr("no results"));

        div.appendChild(text);
        td.appendChild(div);
        tr.appendChild(td);

        this._msgListBody.appendChild(tr);
    },

    showMoreMailsPrompt: function() {
        var dbView = this.dbView;
        if (!dbView) {
            return;
        }

        var folder = this.dbView.msgFolder;
        if (!folder) {
            return;
        }

        let protocol = folder.server.type;
        if ("pop3" == protocol) {
            const pop3PartialFetchMsg = "pop3.partial.fetch.message";
            //let isPartialFetch = Services.prefs.getBoolPref(pop3PartialFetchMsg);
						isPartialFetch =false;
            if (!isPartialFetch) {
                return;
            }
        }
        else if ("imap" == protocol) {
            const imapPartialFetchMsg = "imap.partial.fetch.message";
            //let isPartialFetch = Services.prefs.getBoolPref(imapPartialFetchMsg);
						isPartialFetch =false;
            if (!isPartialFetch) {
                return;
            }
        }
        else {
            return;
        }

        var showMore = folder.getStringProperty("fetchMsgPromptShow");
        var leftMailsCount = folder.getStringProperty("msgNumLeftOnServer");

        if (showMore != "1") {
            return;
        }

        var tr = eltHelper.createElt("tr");
        tr.setAttribute("class", "showMore");
        tr.setAttribute("expandChildren", "false");
        var td = eltHelper.createElt("td");
        td.setAttribute("colspan", gIsWideStyle ? "7" : "1");

        var div = eltHelper.createElt("div");
        div.setAttribute("align", "center");
        var text = eltHelper.createText(leftMailsCount + trHelper.tr("mail(s) not received"));

        div.appendChild(text);
        td.appendChild(div);
        tr.appendChild(td);

        var me = this;
        tr.addEventListener("click", function(mouseEvent) {
            var args = {};
            args.curServer = folder.server;
            args.curFolder = folder;

            let protocol = folder.server.type;
            if ("imap" == protocol) {
                window.openDialog("chrome://messenger/content/getImapMsgsPrompt.xul", "", "centerscreen", args);
            }
            else if ("pop3" == protocol) {
            	window.openDialog("chrome://messenger/content/getPop3MsgsPrompt.xul", "", "centerscreen", args);
            }
        });

        this._msgListBody.appendChild(tr);
    },

    // 对相同内容进行归类，如发/收件人，主题
    insertClassifiedForSame: function() {
        if (this._allMsgObjs.length == 0) {
            return;
        }

        function getClassifiedText(itemElt) {
            var index = parseInt(itemElt.getAttribute("msgIndex"));
            //var subText = eltHelper.createText(this._allMsgObjs[i].data.subject);

            switch (this.dbView.sortType) {
                case Ci.nsMsgViewSortType.byAuthor:
                case Ci.nsMsgViewSortType.byRecipient:
                    return this._allMsgObjs[index].data.senderRecipient;
                    break;
                case Ci.nsMsgViewSortType.bySubject:
                    return this._allMsgObjs[index].data.subject;
                    break;
                default:
                    return null;
            }

            return null;
        }

        var headItem = this._allMsgObjs[0].elt;
        var count = 1;
        var classifiedText = getClassifiedText.call(this, this._allMsgObjs[0].elt);

        for (var i = 1; i < this._allMsgObjs.length; i++) {
            var curItem = this._allMsgObjs[i].elt;
            var curText = getClassifiedText.call(this, curItem);

            if (curText == classifiedText) {
                count++;
            } else {
                this.insertClassifiedItem(classifiedText +
                    " (" + count + " " + trHelper.tr("message(s)") + ")", headItem);

                classifiedText = curText;
                headItem = curItem;
                count = 1;
            }
        }

        this.insertClassifiedItem(classifiedText +
            " (" + count + " " + trHelper.tr("message(s)") + ")", headItem);
    },

    // 对于数值范围进行归类，如时间/大小
    insertClassifiedForGroup: function() {
        if (this._allMsgObjs.length == 0) {
            return;
        }

        var classified = [];

        // 只归类时间和大小排序
        if (this.dbView.sortType == Ci.nsMsgViewSortType.byDate) {
            classified = timeSpanClassified;
        } else if (this.dbView.sortType == Ci.nsMsgViewSortType.bySize) {
            classified = sizeClassified;
        } else {
            return;
        }

        var headItem = null;
        var tailItem = null;
        var headFound = false;
        var countFromHead = 0;
        var clIndex = 0;
        var i;

        if (this.dbView.sortOrder == Ci.nsMsgViewSortOrder.descending) {
            i = -1;
        } else {
            i = this._allMsgObjs.length;
        }

        function insertOneClassified() {
            if (this.dbView.sortOrder == Ci.nsMsgViewSortOrder.descending) {
                this.insertClassifiedItem(trHelper.tr(classified[clIndex].title) +
                    " (" + countFromHead + " " + trHelper.tr("message(s)") + ")", headItem);
            } else {
                this.insertClassifiedItem(trHelper.tr(classified[clIndex].title) +
                    " : " + countFromHead + " " + trHelper.tr("message(s)"), tailItem);
            }
        }

        while (true) {
            if (this.dbView.sortOrder == Ci.nsMsgViewSortOrder.descending) {
                if (i >= (this._allMsgObjs.length - 1)) {
                    tailItem = this._allMsgObjs[i].elt;
                    break;
                }

                i++;
            } else {
                if (i <= 0) {
                    tailItem = this._allMsgObjs[i].elt;
                    break;
                }

                i--;
            }

            var itemObj = this._allMsgObjs[i];
            if (classified[clIndex].func.call(msgDateTimeHelper, itemObj.hdr)) {		// 当前时间已匹配
                if (headFound) {	// 已找到head的情况下，继续
                    countFromHead++;
                } else {	// 还未找到head的情况下，标记head
                    headItem = itemObj.elt;
                    headFound = true;
                    countFromHead = 1;
                }
            } else {	// 当前时间不匹配
                if (headFound) {	// 已找到head的情况下，说明此时间段已匹配完成，标记tail，添加时间分类，游标前移，使用下一个时间匹配函数
                    if (this.dbView.sortOrder == Ci.nsMsgViewSortOrder.descending) {
                        i--;
                    } else {
                        i++;
                    }

                    tailItem = this._allMsgObjs[i].elt;
                    headFound = false;

                    insertOneClassified.call(this);

                    clIndex++;
                } else {	// 还未找到head，则找到能匹配当前时间的函数，找到后游标前移
                    while (!classified[clIndex].func.call(msgDateTimeHelper, itemObj.hdr)) {
                        clIndex++;
                    }

                    if (this.dbView.sortOrder == Ci.nsMsgViewSortOrder.descending) {
                        i--;
                    } else {
                        i++;
                    }
                }
            }
        }

        insertOneClassified.call(this);
    },

    /* 已读/未读，有附件/无附件，有星标/无星标排序，适用于此类二元归类
     * context的key有：
     * matchFlag：邮件头标志位
     * matchLabel：匹配标志位的显示标签
     * mismatchLabel：不匹配标志位的显示标签
     */
    insertClassifiedForBinaryStatus: function(context) {
        if (this._allMsgObjs.length == 0) {
            return;
        }

        var record = {
            match: {
                head: null,
                count: 0
            },
            mismatch: {
                head: null,
                count: 0
            }
        };

        for (var i = 0; i < this._allMsgObjs.length; i++) {
            var curItem = this._allMsgObjs[i];
            var curElt = curItem.elt;
            var curFlag = curItem.hdr.flags;

            if (curFlag & context.matchFlag) {
                if (record.match.head == null) {
                    record.match.head = curElt;
                }
                record.match.count++;
            } else {
                if (record.mismatch.head == null) {
                    record.mismatch.head = curElt;
                }
                record.mismatch.count++;
            }
        }

        if (record.match.head != null) {
            this.insertClassifiedItem(context.matchLabel +
                " (" + record.match.count + trHelper.tr("message(s)") + ")",
                record.match.head);
        }

        if (record.mismatch.head != null) {
            this.insertClassifiedItem(context.mismatchLabel +
                " (" + record.mismatch.count + trHelper.tr("message(s)") + ")",
                record.mismatch.head);
        }
    },

    // 对于不同排序，使用不同归类方法
    insertClassifiedLines: function() {
        if (this.dbView == null)
            return;

        this._classifiedItems = [];     // 先清空

        switch (this.dbView.sortType) {
            case Ci.nsMsgViewSortType.byUnread:
                var context = {
                    matchFlag: Ci.nsMsgMessageFlags.Read,
                    matchLabel: trHelper.tr("readed"),
                    mismatchLabel: trHelper.tr("unread")
                };

                this.insertClassifiedForBinaryStatus(context);
                break;
            case Ci.nsMsgViewSortType.byAttachments:
                var context = {
                    matchFlag: Ci.nsMsgMessageFlags.Attachment,
                    matchLabel: trHelper.tr("has attached"),
                    mismatchLabel: trHelper.tr("no attached")
                };

                this.insertClassifiedForBinaryStatus(context);
                break;
            case Ci.nsMsgViewSortType.byFlagged:
                var context = {
                    matchFlag: Ci.nsMsgMessageFlags.Marked,
                    matchLabel: trHelper.tr("has flag"),
                    mismatchLabel: trHelper.tr("no flag")
                };

                this.insertClassifiedForBinaryStatus(context);
                break;
            case Ci.nsMsgViewSortType.byDate:
            case Ci.nsMsgViewSortType.bySize:
                this.insertClassifiedForGroup();
                break;
            case Ci.nsMsgViewSortType.byAuthor:
            case Ci.nsMsgViewSortType.byRecipient:
            case Ci.nsMsgViewSortType.bySubject:
                this.insertClassifiedForSame();
            default:
                break;
        }
    },

    findMsgHdrByElt: function(itemElt) {
        for (var i in this._allMsgObjs) {
            if (itemElt == this._allMsgObjs[i].elt) {
                return this._allMsgObjs[i].hdr;
            }
        }

        return null;
    },

    findItemEltByMsgHdr: function(msgHdr) {
        for (var i in this._allMsgObjs) {
            if (msgHdr == this._allMsgObjs[i].hdr) {
                return this._allMsgObjs[i].elt;
            }
        }

        return null;
    },

    findItemEltByIndex: function(index) {
        if (index < 0 || index >= this._allMsgObjs.length) {
            return null;
        }

        return this._allMsgObjs[index].elt;
    },

    _threadListSize: {
        width: 0,
        height: 0
    },
    startSizeWatcher: function() {
        var me = this;
        setInterval(function() {
            if (eltHelper.computedStyle($("htmlThreadList")).display == "none") {
                return;
            }

            var newWidth, newHeight;
            newWidth = parseInt(eltHelper.getWidth($("htmlThreadList")));
            newHeight = parseInt(eltHelper.getHeight($("htmlThreadList")));

            if (newWidth != me._threadListSize.width ||
                newHeight != me._threadListSize.height) {
                me._threadListSize.width = newWidth;
                me._threadListSize.height = newHeight;
                me.onResize();
            }
        }, 50);
    },

    onResize: function() {
        var scrollbarWidth = $("htmlThreadListBox").scrollWidth -
            $("htmlThreadListBox").clientWidth;

        $("htmlThreadListWideCols").style.width = $("htmlThreadListBox").clientWidth + "px";

        $("htmlStatusCol").style.width = "21px";
        $("htmlFlaggedCol").style.width = "22px";
        $("htmlAttachCol").style.width = "20px";

        var lineWidth = parseInt($("htmlThreadListWideCols").style.width);

        // 按比例分配列宽
        $("htmlSenderRecipientCol").style.width = parseInt((lineWidth - 69) * 0.16) + "px";
        $("htmlSubjectCol").style.width = parseInt((lineWidth - 69) * 0.35) + "px";
        $("htmlTimeCol").style.width = parseInt((lineWidth - 69) * 0.27) + "px";
        $("htmlSizeCol").style.width = parseInt((lineWidth - 69) * 0.22) + "px";

        //this.updateHeader();

        var listWidth = $("htmlThreadList").clientWidth;

        var preWideStyle = gIsWideStyle;
        if (listWidth < 410) {
            gIsWideStyle = false;

            if (preWideStyle) {
                ac.log("wide to narrow");
                this.delayLoadMessages(loadReason.wideToNarrow);
            }
        } else {
            gIsWideStyle = true;

            if (!preWideStyle) {
                ac.log("narrow to wide");
                this.delayLoadMessages(loadReason.narrowToWide);
            }
        }

        this.adjustBodyCols();
        this.cut();     // 尺寸变化，listBox的高度可能变化，要重新计算

    },

    // 调整列宽
    adjustBodyCols: function() {
        var firstLine = $("htmlFirstRow");
        if (!firstLine) {
            return;
        }

        $("htmlThreadListBody").style.width = eltHelper.getWidth($("htmlThreadListWideCols"));

        var colsLeft = [];

        if (gIsWideStyle) {
            colsLeft = [
                $("htmlStatusCol").offsetLeft,
                $("htmlFlaggedCol").offsetLeft,
                $("htmlAttachCol").offsetLeft,
                $("htmlSenderRecipientCol").offsetLeft,
                $("htmlSubjectCol").offsetLeft,
                $("htmlTimeCol").offsetLeft,
                $("htmlSizeCol").offsetLeft
            ];
        } else {
            colsLeft = [
                $("htmlThreadListNarrowCols").offsetLeft,
                $("htmlThreadListNarrowCols").offsetLeft + $("htmlThreadListNarrowCols").clientWidth
            ];
        }

        var firstCells = firstLine.childNodes;
        for (var i = 0; i < firstCells.length - 1; i++) {
            firstCells[i].style.width = (colsLeft[i+1] - colsLeft[i]) + "px";
        }

        for (var i = 0; i < this._allMsgObjs.length; i++) {
            this.adjustItemElt(this._allMsgObjs[i].elt);
        }
    },

    // 更新排序模式在表头的体现
    updateHeader: function() {
        if (gIsWideStyle) {      // 宽模式
            $("htmlThreadListWideCols").hidden = false;
            $("htmlThreadListNarrowCols").hidden = true;

            // 根据邮箱显示发件人或收件人列
            if (gIsSenderDisplay) {
                $("senderRecipientColText").innerHTML = trHelper.tr("sender");
            } else {
                $("senderRecipientColText").innerHTML = trHelper.tr("recipient");
            }

            $("subjectColText").innerHTML = trHelper.tr("subject");
            $("timeColText").innerHTML = trHelper.tr("time");
            $("sizeColText").innerHTML = trHelper.tr("size");

            $("statusColImg").removeAttribute("sort");
            $("flaggedColImg").removeAttribute("sort");
            $("attachColImg").removeAttribute("sort");
            $("senderRecipientSortAsc").hidden = true;
            $("senderRecipientSortDesc").hidden = true;
            $("subjectSortAsc").hidden = true;
            $("subjectSortDesc").hidden = true;
            $("timeSortAsc").hidden = true;
            $("timeSortDesc").hidden = true;
            $("sizeSortAsc").hidden = true;
            $("sizeSortDesc").hidden = true;

            // 升降序标记图片
            if (gSortOrder == Ci.nsMsgViewSortOrder.ascending) {
                switch (gSortType) {
                    case Ci.nsMsgViewSortType.byUnread:
                        $("statusColImg").setAttribute("sort", "asc");
                        break;
                    case Ci.nsMsgViewSortType.byFlagged:
                        $("flaggedColImg").setAttribute("sort", "asc");
                        break;
                    case Ci.nsMsgViewSortType.byAttachments:
                        $("attachColImg").setAttribute("sort", "asc");
                        break;
                    case Ci.nsMsgViewSortType.byAuthor:
                    case Ci.nsMsgViewSortType.byRecipient:
                        $("senderRecipientSortAsc").hidden = false;
                        break;
                    case Ci.nsMsgViewSortType.bySubject:
                        $("subjectSortAsc").hidden = false;
                        break;
                    case Ci.nsMsgViewSortType.byDate:
                        $("timeSortAsc").hidden = false;
                        break;
                    case Ci.nsMsgViewSortType.bySize:
                        $("sizeSortAsc").hidden = false;
                        break;
                    default:
                        break;
                }
            } else {
                switch (gSortType) {
                    case Ci.nsMsgViewSortType.byUnread:
                        $("statusColImg").setAttribute("sort", "desc");
                        break;
                    case Ci.nsMsgViewSortType.byFlagged:
                        $("flaggedColImg").setAttribute("sort", "desc");
                        break;
                    case Ci.nsMsgViewSortType.byAttachments:
                        $("attachColImg").setAttribute("sort", "desc");
                        break;
                    case Ci.nsMsgViewSortType.byAuthor:
                    case Ci.nsMsgViewSortType.byRecipient:
                        $("senderRecipientSortDesc").hidden = false;
                        break;
                    case Ci.nsMsgViewSortType.bySubject:
                        $("subjectSortDesc").hidden = false;
                        break;
                    case Ci.nsMsgViewSortType.byDate:
                        $("timeSortDesc").hidden = false;
                        break;
                    case Ci.nsMsgViewSortType.bySize:
                        $("sizeSortDesc").hidden = false;
                        break;
                    default:
                        break;
                }
            }
        } else {    // 窄模式
            $("htmlThreadListWideCols").hidden = true;
            $("htmlThreadListNarrowCols").hidden = false;

            if (gIsSenderDisplay) {
                $("narrowPopupSenderRecipent").setAttribute("label", trHelper.tr("bySender"));
            } else {
                $("narrowPopupSenderRecipent").setAttribute("label", trHelper.tr("byRecipient"));
            }

            $("narrowPopupSubject").setAttribute("label", trHelper.tr("bySubject"));
            $("narrowPopupTime").setAttribute("label", trHelper.tr("byTime"));
            $("narrowPopupSize").setAttribute("label", trHelper.tr("bySize"));
            $("narrowPopupUnread").setAttribute("label", trHelper.tr("byUnread"));
            $("narrowPopupAttached").setAttribute("label", trHelper.tr("byAttached"));
            $("narrowPopupFlagged").setAttribute("label", trHelper.tr("byFlagged"));

            $("narrowPopupAsc").setAttribute("label", trHelper.tr("sort asc"));
            $("narrowPopupDesc").setAttribute("label", trHelper.tr("sort desc"));

            $("narrowPopupSenderRecipent").setAttribute("checked", "false");
            $("narrowPopupSubject").setAttribute("checked", "false");
            $("narrowPopupTime").setAttribute("checked", "false");
            $("narrowPopupSize").setAttribute("checked", "false");
            $("narrowPopupUnread").setAttribute("checked", "false");
            $("narrowPopupAttached").setAttribute("checked", "false");
            $("narrowPopupFlagged").setAttribute("checked", "false");
            $("narrowPopupAsc").setAttribute("checked", "false");
            $("narrowPopupDesc").setAttribute("checked", "false");

            // 根据排序模式，对菜单项上打勾
            switch (gSortType) {
                case Ci.nsMsgViewSortType.byAuthor:
                    $("narrowSortType").textContent = trHelper.tr("sender");
                    $("narrowPopupSenderRecipent").setAttribute("checked", "true");
                    break;
                case Ci.nsMsgViewSortType.byRecipient:
                    $("narrowSortType").textContent = trHelper.tr("recipient");
                    $("narrowPopupSenderRecipent").setAttribute("checked", "true");
                    break;
                case Ci.nsMsgViewSortType.bySubject:
                    $("narrowSortType").textContent = trHelper.tr("subject");
                    $("narrowPopupSubject").setAttribute("checked", "true");
                    break;
                case Ci.nsMsgViewSortType.byDate:
                    $("narrowSortType").textContent = trHelper.tr("time");
                    $("narrowPopupTime").setAttribute("checked", "true");
                    break;
                case Ci.nsMsgViewSortType.bySize:
                    $("narrowSortType").textContent = trHelper.tr("size");
                    $("narrowPopupSize").setAttribute("checked", "true");
                    break;
                case Ci.nsMsgViewSortType.byUnread:
                    $("narrowSortType").textContent = trHelper.tr("unread");
                    $("narrowPopupUnread").setAttribute("checked", "true");
                    break;
                case Ci.nsMsgViewSortType.byAttachments:
                    $("narrowSortType").textContent = trHelper.tr("attached");
                    $("narrowPopupAttached").setAttribute("checked", "true");
                    break;
                case Ci.nsMsgViewSortType.byFlagged:
                    $("narrowSortType").textContent = trHelper.tr("flagged");
                    $("narrowPopupFlagged").setAttribute("checked", "true");
                    break;
                default:
                    break;
            }

            if (gSortOrder == Ci.nsMsgViewSortOrder.ascending) {
                $("narrowSortAsc").hidden = false;
                $("narrowSortDesc").hidden = true;
                $("narrowPopupAsc").setAttribute("checked", "true");
            } else {
                $("narrowSortAsc").hidden = true;
                $("narrowSortDesc").hidden = false;
                $("narrowPopupDesc").setAttribute("checked", "true");
            }

        }
    },

    getSelectionArray: function() {
        var selection = this.threadTree.view.selection;
        var array = [];

        var start = new Object();
        var end = new Object();
        var rangeCount = selection.getRangeCount();
        for (var i = 0; i < rangeCount; i++) {
            selection.getRangeAt(i, start, end);
            for (var v = start.value; v <= end.value; v++) {
                array.push(v);
            }
        }

        return array;
    },

    _selectedIndices: [],   // 当前选中的item序号数组

    // 根据当前选中项更新item样式，而不对树做真正的选中操作
    updateSelection: function() {
        // 清除上一次选中item的样式
        for (var i = 0; i < this._selectedIndices.length; i++) {
            var index = this._selectedIndices[i];
            var itemElt = this.findItemEltByIndex(index);
            eltHelper.removeClass(itemElt, "htmlItemSelected");
        }

        this._selectedIndices = this.getSelectionArray();
        for (var i = 0; i < this._selectedIndices.length; i++) {
            var index = this._selectedIndices[i];
            var itemElt = this.findItemEltByIndex(index);

            // 选中邮件有时被置为已读，此时根据标记更新样式
            var msgHdr = this.findMsgHdrByElt(itemElt);
            var statusCell = this.getCellElt(itemElt, cellType.status);

            var me = this;
            setTimeout(function() {
                if (msgHdr.flags & Ci.nsMsgMessageFlags.Read) {
                    eltHelper.removeClass(itemElt, "htmlMsgUnread");
                } else {
                    eltHelper.addClass(itemElt, "htmlMsgUnread");
                }

                me.fillStatusCell(statusCell, msgHdr);
            }, 30);

            eltHelper.addClass(itemElt, "htmlItemSelected");
        }
    },

    itemIsSelected: function(itemElt) {
        var selection = this.threadTree.view.selection;
        var index = parseInt(itemElt.getAttribute("msgIndex"));

        return selection.isSelected(index);
    },

    selectItem: function(item, onlyOne) {
        if (item == null) {
            return;
        }

        if (onlyOne === undefined) {
            onlyOne = true;
        }

        var msgIndex = parseInt(item.getAttribute("msgIndex"));

        if (onlyOne) {  // 只选中当前的
            this.threadTree.view.selection.select(msgIndex);
            this.markMessageAsRead(item, true);     // 单选邮件时，标识为已读
        } else {    // 追加新选中项
            this.threadTree.view.selection.toggleSelect(msgIndex);
        }
    },

    rangedSelectItems: function(beginItem, endItem) {
        var beginIndex = parseInt(beginItem.getAttribute("msgIndex"));
        var endIndex = parseInt(endItem.getAttribute("msgIndex"));

        var minIndex = Math.min(beginIndex, endIndex);
        var maxIndex = Math.max(beginIndex, endIndex);

        this.threadTree.view.selection.rangedSelect(minIndex, maxIndex, false);
    },

    _lastSelectedItem: null,
    _keyReachedItem: null,  // 用键盘选择时，最后一个item

    onItemSelected: function(item, event) {
        if (event.button == 0) {    // 左键
            if (event.ctrlKey) {    // 按下ctrl键
                this.selectItem(item, false);
                this._lastSelectedItem = item;
                this._keyReachedItem = item;
            } else if (event.shiftKey) {    // shift范围多选
                this.rangedSelectItems(this._lastSelectedItem, item);
            } else {    // 没有辅助键，左键单选
                this.selectItem(item, true);
                this._lastSelectedItem = item;
                this._keyReachedItem = item;
            }
        }
        else if (event.button == 2) { // 右键
            if (!this.itemIsSelected(item)) {
                this.selectItem(item, true);
                this._lastSelectedItem = item;
                this._keyReachedItem = item;
            }
        }
    },

    onKeyDown: function(event) {
        if (event.target.id != "messengerWindow") {
            return;
        }

        if (this._lastSelectedItem == null) {
            return;
        }

        switch (event.which) {
            case event.DOM_VK_UP:
                if (this._lastSelectedItem == null) {
                    return;
                }

                var lastIndex = parseInt(this._lastSelectedItem.getAttribute("msgIndex"));
                if (lastIndex == 0) {   // 已是第一项
                    return;
                }

                var curIndex = lastIndex - 1;
                var curItem = this.findItemEltByIndex(curIndex);

                if (event.shiftKey) {   // 按住shift，多选
                    var keyReachIndex = parseInt(this._keyReachedItem.getAttribute("msgIndex"));
                    if (keyReachIndex == 0) {
                        return;
                    }

                    keyReachIndex--;

                    this._keyReachedItem = this.findItemEltByIndex(keyReachIndex);
                    this.rangedSelectItems(this._lastSelectedItem, this._keyReachedItem);
                } else {
                    this.selectItem(curItem, true);
                    this._lastSelectedItem = curItem;
                    this._keyReachedItem = curItem;
                }

                // 确保当前项在顶部
                if (!this.isItemInSight(this._keyReachedItem)) {
                    this.ensureItemVisible(this._keyReachedItem, itemPosition.top);
                }

                break;
            case event.DOM_VK_DOWN:
                if (this._lastSelectedItem == null) {
                    return;
                }

                var lastIndex = parseInt(this._lastSelectedItem.getAttribute("msgIndex"));

                if (lastIndex >= (this._allMsgObjs.length - 1)) {   // 已是最后一项
                    return;
                }

                var curIndex = lastIndex + 1;
                var curItem = this.findItemEltByIndex(curIndex);// this._allMsgObjs[curIndex].elt;

                if (event.shiftKey) {   // 按住shift，多选
                    var keyReachIndex = parseInt(this._keyReachedItem.getAttribute("msgIndex"));
                    if (keyReachIndex >= this._allMsgObjs.length) {
                        return;
                    }

                    keyReachIndex++;

                    this._keyReachedItem = this.findItemEltByIndex(keyReachIndex);// this._allMsgObjs[keyReachIndex].elt;
                    this.rangedSelectItems(this._lastSelectedItem, this._keyReachedItem);
                } else {
                    this.selectItem(curItem, true);
                    this._lastSelectedItem = curItem;
                    this._keyReachedItem = curItem;
                }

                // 确保当前项在底部
                if (!this.isItemInSight(this._keyReachedItem)) {
                    this.ensureItemVisible(this._keyReachedItem, itemPosition.bottom);
                }

                break;
            case event.DOM_VK_HOME:
                var rows = this._msgListBody.childNodes;
                for (var i = 1; i < rows.length; i++) {     // 从前往后，找到第一个未被折叠的item
                    var row = rows[i];
                    var type = row.getAttribute("type");
                    if (type == "MsgItem") {
                        var shouldHide = row.getAttribute("shouldHide");
                        if (shouldHide != "true") {
                            this.selectItem(row, true);
                            if (!this.isItemInSight(row)) {
                                this.ensureItemVisible(row, itemPosition.top);
                            }

                            break;
                        }
                    }
                }

                break;
            case event.DOM_VK_END:
                var rows = this._msgListBody.childNodes;
                for (var i = rows.length - 1; i > 0; i--) {    // 从后往前，找到第一个未被折叠的item
                    var row = rows[i];
                    var type = row.getAttribute("type");
                    if (type == "MsgItem") {
                        var shouldHide = row.getAttribute("shouldHide");
                        if (shouldHide != "true") {
                            this.selectItem(row, true);
                            if (!this.isItemInSight(row)) {
                                this.ensureItemVisible(row, itemPosition.bottom);
                            }

                            break;
                        }
                    }
                }
                break;
            default:
                break;
        }

    },

    // 元素是否在可见区域内
    isItemInSight: function(itemElt) {
        //var paddingTop = parseInt(this._msgListBox.style.paddingTop);
        var msgIndex = parseInt(itemElt.getAttribute("msgIndex"));
        var itemOffsetTop = this._allMsgObjs[msgIndex].data.offsetTop;
        var itemClientHeight = this._allMsgObjs[msgIndex].data.clientHeight;

        var keepTop = ((itemOffsetTop /*+ paddingTop*/) >= this._msgListBox.scrollTop);
        var keepBottom = ((itemOffsetTop /*+ paddingTop*/ + itemClientHeight) <=
            (this._msgListBox.scrollTop + this._msgListBox.clientHeight));

        return keepTop && keepBottom;
    },

    ensureItemVisible: function(itemElt, pos) {
        var msgIndex = parseInt(itemElt.getAttribute("msgIndex"));
        var itemOffsetTop = this._allMsgObjs[msgIndex].data.offsetTop;
        var itemClientHeight = this._allMsgObjs[msgIndex].data.clientHeight;

        var scrollTop = 0;
        if (pos == itemPosition.top) {
            scrollTop = itemOffsetTop;
        } else if (pos == itemPosition.bottom) {
            scrollTop = itemOffsetTop - this._msgListBox.clientHeight + itemClientHeight;
        } else if (pos == itemPosition.center) {
            scrollTop = itemOffsetTop - this._msgListBox.clientHeight / 2;
        }

        if (scrollTop < 0) {
            scrollTop = 0;
        }
        this.locate(scrollTop);
    },

    /*** nsIMsgFolderListener 的实现 ***/
    _msgAddedTimeout: null,     // 借助计时器，防止频繁加载邮件
    msgAdded: function(msgHdr) {
        if (this._msgAddedTimeout != null) {
            return;
        }

        var me = this;
        this._msgAddedTimeout = setTimeout(function() {
            me.delayLoadMessages(loadReason.msgAdded);
            me._msgAddedTimeout = null;
        }, 500);
    },

    msgsClassified: function(msgHdr, junkProcessed, traitProcessed) {
        this.delayLoadMessages(loadReason.msgsClassified);
    },

    _msgsDeletedTimeout: null,
    msgsDeleted: function(msgHdrArray) {
        if (this._msgsDeletedTimeout != null) {
            return;
        }

        var me = this;
        this._msgsDeletedTimeout = setTimeout(function() {
            me.delayLoadMessages(loadReason.msgsDeleted);
            me._msgsDeletedTimeout = null;
        }, 100);

    },

    msgsMoveCopyCompleted: function(isMove, srcMsgHdrArray, destFolder, destMsgHdrArray) {
        this.delayLoadMessages(loadReason.msgsMoveCopy);
    },

    msgKeyChanged: function(oldMsgKey, newMsgHdr) {
    },

    folderAdded: function(addedFolder) {
    },

    folderDeleted: function(deletedFolder) {
        if (this.dbView.msgFolder && this.dbView.msgFolder == deletedFolder) {
            this.clear();
        }
    },

    folderMoveCopyCompleted: function(isMove, srcFolder, destFolder) {
    },

    folderRenamed: function(oldFolder, newFolder) {
    },

    itemEvent: function(item, event, data) {
    },
    /*** nsIMsgFolderListener 实现结束 ***/

    /*** nsIObserver 的实现 ***/
    observe: function(subject, topic, data) {
        if (topic == "MsgCreateDBView") {   // DBView创建，来自folderDisplay.js
            var me = this;

            // 直接调用不生效，因此使用计时器
            setTimeout(function() {
                me.onDBViewCreated(subject);
            }, 0);
        } else if (topic == "MsgTagsChanged") {     // 标签改变，来自msgHdrViewOverlay.js
            var msgHdrArray = subject.QueryInterface(Ci.nsIArray);
            for (var i = 0; i < msgHdrArray.length; i++) {
                var msgHdr = msgHdrArray.queryElementAt(i, Ci.nsIMsgDBHdr);
                if (msgHdr != null) {
                    var itemElt = this.findItemEltByMsgHdr(msgHdr);
                    var subjectCell = this.getCellElt(itemElt, cellType.subject);
                    this.fillSubjectCell(itemElt, subjectCell, msgHdr);
                    this.adjustItemElt(itemElt);
                }
            }
        } else if (topic == "MsgMarkRead") {
            var msgHdrArray = subject.QueryInterface(Ci.nsIArray);
            for (var i = 0; i < msgHdrArray.length; i++) {
                var msgHdr = msgHdrArray.queryElementAt(i, Ci.nsIMsgDBHdr);
                if (msgHdr != null) {
                    var itemElt = this.findItemEltByMsgHdr(msgHdr);
                    this.markMessageAsRead(itemElt, data == "true" ? true : false);
                }
            }
        } else if (topic == "MsgMarkFlagged") {
            var msgHdrArray = subject.QueryInterface(Ci.nsIArray);
            for (var i = 0; i < msgHdrArray.length; i++) {
                var msgHdr = msgHdrArray.queryElementAt(i, Ci.nsIMsgDBHdr);
                if (msgHdr != null) {
                    var itemElt = this.findItemEltByMsgHdr(msgHdr);
                    this.markMessageAsFlagged(itemElt, data == "true" ? true : false);
                }
            }
        } else if (topic == "FolderShowed") {
            if (data == "account") {    // 显示帐户，清空
                this.dbView = null;
                this.clear();
            }
        } else if (topic == "TreeSelectionChanged") {
            this.updateSelection();
        } else if (topic == "TagSettingChanged") {
            this.refreshTaggedItems();
            OnTagsChange();     // 同时更新邮件头的标签
        } else if (topic == "MsgMarkAllRead") {
            if (subject instanceof Ci.nsIMsgFolder) {
                if (this.dbView.msgFolder && subject == this.dbView.msgFolder) {
                    for (var i in this._allMsgObjs) {
                        this.markMessageAsRead(this._allMsgObjs[i].elt, true);
                    }
                }
            }
        } else if (topic == "DBViewDoCommand") {
            var obj = subject.wrappedJSObject;

            var nsMsgViewCommandType = Components.interfaces.nsMsgViewCommandType;
            if (obj.command == nsMsgViewCommandType.markAllRead) {
                for (var i in this._allMsgObjs) {
                    this.markMessageAsRead(this._allMsgObjs[i].elt, true);
                }
            }
        }
       else if (topic == "am_openmail") {
                var msgHdr = subject;
                if (msgHdr != null) {

                    var itemElt = this.findItemEltByMsgHdr(msgHdr);
                    var index = itemElt.getAttribute("msgIndex");
                    this.threadTree.view.selection.select(index);
                    MsgOpenNewWindowForMessage();
                }
         }
        else if (topic == "am_selnattach") {
            var msgHdr = subject;
            if (msgHdr != null) {

                var itemElt = this.findItemEltByMsgHdr(msgHdr);
                var index = itemElt.getAttribute("msgIndex");
                this.threadTree.view.selection.select(index);
            }
        }  else if (topic == "recallAction") {
			var msgHdr = subject.QueryInterface(Ci.nsIMsgDBHdr);
			let recallMail = msgHdr.getStringProperty("RecallMail");
			
			var itemElt = this.findItemEltByMsgHdr(msgHdr);
			if (itemElt) {
				var statusCell = this.getCellElt(itemElt, cellType.status);
				this.fillStatusCell(statusCell, msgHdr);
			}
		} else if (topic == "FolderRefresh") {
			var msgFolder = subject;
			if (msgFolder instanceof Ci.nsIMsgFolder) {
				if (this.dbView.msgFolder == msgFolder) {
					this.delayLoadMessages(loadReason.refresh);
				}
			}
		}
    },
    /*** nsIObserver 实现结束 ***/

    triggerSort: function(sortType, sortOrder) {
        var newSortOrder = sortOrder;
        if (!newSortOrder) {    // 升序降序未定义
            if (this.dbView.sortType == sortType) {   // 若是相同排序类型，则反转顺序
                if (this.dbView.sortOrder == Ci.nsMsgViewSortOrder.descending) {
                    newSortOrder = Ci.nsMsgViewSortOrder.ascending;
                } else {
                    newSortOrder = Ci.nsMsgViewSortOrder.descending;
                }
            } else {    // 切换到另一个排序类型，默认为降序
                newSortOrder = Ci.nsMsgViewSortOrder.descending;
            }
        }

        // 排序是在上一次序列的基础上排序，所以如果碰到特征值很少的排序，比如已读/未读，星标等类型，
        //  相同特征值的邮件就不是按时间顺序来排了。所以这里先按时间逆序排一下，这样可以做到相同特
        //  征值的邮件按时间逆序来排。
        if (sortType != Ci.nsMsgViewSortType.byDate) {
            this.dbView.sort(Ci.nsMsgViewSortType.byDate, Ci.nsMsgViewSortOrder.descending);
        }

        this.dbView.sort(sortType, newSortOrder);
    },

    bindColsEvent: function() {
        var me = this;

        // 宽模式列的事件绑定
        $("htmlStatusCol").onclick = function(event) {
            me.triggerSort(Ci.nsMsgViewSortType.byUnread);
            me.delayLoadMessages(loadReason.sort);
        };

        $("htmlFlaggedCol").onclick = function(event) {
            me.triggerSort(Ci.nsMsgViewSortType.byFlagged);
            me.delayLoadMessages(loadReason.sort);
        };

        $("htmlAttachCol").onclick = function(event) {
            me.triggerSort(Ci.nsMsgViewSortType.byAttachments);
            me.delayLoadMessages(loadReason.sort);
        };

        $("htmlSenderRecipientCol").onclick = function(event) {
            if (gIsSenderDisplay) {
                me.triggerSort(Ci.nsMsgViewSortType.byAuthor);
            } else {
                me.triggerSort(Ci.nsMsgViewSortType.byRecipient);
            }

            me.delayLoadMessages(loadReason.sort);
        };

        $("htmlSubjectCol").onclick = function(event) {
            me.triggerSort(Ci.nsMsgViewSortType.bySubject);
            me.delayLoadMessages(loadReason.sort);
        };

        $("htmlTimeCol").onclick = function(event) {
            me.triggerSort(Ci.nsMsgViewSortType.byDate);
            me.delayLoadMessages(loadReason.sort);
        };

        $("htmlSizeCol").onclick = function(event) {
            me.triggerSort(Ci.nsMsgViewSortType.bySize);
            me.delayLoadMessages(loadReason.sort);
        };

        // 窄模式弹出菜单的事件绑定
        $("narrowPopupSenderRecipent").onclick = function(event) {
            if (gIsSenderDisplay) {
                me.triggerSort(Ci.nsMsgViewSortType.byAuthor, me.dbView.sortOrder);
            } else {
                me.triggerSort(Ci.nsMsgViewSortType.byRecipient, me.dbView.sortOrder);
            }

            me.delayLoadMessages(loadReason.sort);
        };

        $("narrowPopupSubject").onclick = function(event) {
            me.triggerSort(Ci.nsMsgViewSortType.bySubject, me.dbView.sortOrder);
            me.delayLoadMessages();
        };

        $("narrowPopupTime").onclick = function(event) {
            me.triggerSort(Ci.nsMsgViewSortType.byDate, me.dbView.sortOrder);
            me.delayLoadMessages(loadReason.sort);
        };

        $("narrowPopupSize").onclick = function(event) {
            me.triggerSort(Ci.nsMsgViewSortType.bySize, me.dbView.sortOrder);
            me.delayLoadMessages(loadReason.sort);
        };

        $("narrowPopupUnread").onclick = function(event) {
            me.triggerSort(Ci.nsMsgViewSortType.byUnread, me.dbView.sortOrder);
            me.delayLoadMessages(loadReason.sort);
        };

        $("narrowPopupAttached").onclick = function(event) {
            me.triggerSort(Ci.nsMsgViewSortType.byAttachments, me.dbView.sortOrder);
            me.delayLoadMessages(loadReason.sort);
        };

        $("narrowPopupFlagged").onclick = function(event) {
            me.triggerSort(Ci.nsMsgViewSortType.byFlagged, me.dbView.sortOrder);
            me.delayLoadMessages(loadReason.sort);
        };

        $("narrowPopupAsc").onclick = function(event) {
            me.triggerSort(me.dbView.sortType, Ci.nsMsgViewSortOrder.ascending);
            me.delayLoadMessages(loadReason.sort);
        };

        $("narrowPopupDesc").onclick = function(event) {
            me.triggerSort(me.dbView.sortType, Ci.nsMsgViewSortOrder.descending);
            me.delayLoadMessages(loadReason.sort);
        };
    },

    test1: function() {
    },

    test2: function() {
    }

};

window.addEventListener("load", function() {
    if (/*Services.prefs.getIntPref("mail.messageList.type") == 1*/ true) {  // html模式
        $("htmlThreadList").removeAttribute("hidden");
        htmlThreadList.init();
    } else {
        $("htmlThreadList").setAttribute("hidden", "true");
    }
});
