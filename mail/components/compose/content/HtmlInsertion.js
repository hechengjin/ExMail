/* 此代码解决Excel内容粘贴问题。当Excel内容被复制时，剪贴板会出现多种格式，如果粘贴到ThinkMail，
 * 我们只取HTML格式。在HTML格式的内容中，<style>标签里面的内容是被注释掉的（不被注释也没用，
 * nsHTMLDataTransfer.cpp也把它剥离了），如果把这段style插入到文档中，则表格样式都是完整的。
 * 但是我们不能强行把<style>注释打开，并插入到<head>中，因为<style>影响的是全局，再粘贴一段
 * Excel内容，样式会受影响。所以，我的做法是，解析<style>里的样式，把每一条样式塞到受影响的node
 * 的style里去。nsHTMLDataTransfer.cpp负责通知插入的node和剪贴板的内容，此js解析style，并
 * 重新设置nodes的style。当然style的解析做得极其简易，只能适配单class name和单element name。
 *
 * 还有一个问题需要注意，如果打开两个编写窗口，往一个窗口粘贴Excel表格，则两具编写窗口都能收到
 * 通知，并处理传来的元素样式，因为两个窗口都注册了通知。但经测试并没有问题，因为通知过来的元素
 * 对象在另一个窗口的HTML编辑器中是不存在的，虽然它也在处理元素的样式，但并没有生效，生效的是真
 * 正粘贴的那个目标编写窗口。这种每个编写窗口都响应的处理方法不太合理，以后想办法改善。
 */

var styleProcesser = {
    _styleDic: {},      // style映射表。nodeName:style，或className:style

    parse: function(style) {
        this._styleDic = {};

        var curIndex = 0;
        while (true) {
            var leftBracePos = style.indexOf("{", curIndex);
            if (leftBracePos == -1) {
                break;
            }

            var rightBracePos = style.indexOf("}", leftBracePos);
            if (rightBracePos == -1) {
                break;
            }

            var name = style.substring(curIndex, leftBracePos);
            name = name.replace(/(^\s*)|(\s*$)/g, "");  // 去掉左右两端空白字符
            if (name != "") {
                var styleContent = style.substring(leftBracePos + 1, rightBracePos);
                styleContent = styleContent.replace(/[\r\n]*/g, "")     // 清除换行字符
                                           .replace(/:.5pt/g, ":0.5pt"); // 防止Foxmail不认识.5pt，替换成0.5pt
                if (name[0] == ".") {
                    this._styleDic[name] = styleContent;
                } else {
                    this._styleDic[name.toLowerCase()] = styleContent;
                }
            }

            curIndex = rightBracePos + 1;
        }
    },

    match: function(elt) {
        var style = "";

        if (elt.nodeType != 1) {
            return "";
        }

        // 匹配element name
        var lowerNodeName = elt.nodeName.toLowerCase();
        if (lowerNodeName != "table") {
            if (lowerNodeName in this._styleDic) {
                style += this._styleDic[lowerNodeName].replace(/"/g, "'");
            }
        }

        // 匹配class name
        var classArray = elt.className.split(" ");
        for (var i = 0; i < classArray.length; i++) {
            var oneClass = classArray[i];
            if (oneClass == " " || oneClass == "") {
                continue;
            }

            var classWithDot = "." + oneClass;
            if (classWithDot in this._styleDic) {
                style += this._styleDic[classWithDot].replace(/"/g, "'");
            }
        }

        return style;
    },

    // 递归元素，重置style内容
    walkElement: function(elt) {
        var matchStyle = this.match(elt);
        if (matchStyle != "") {
            var existStyle = elt.getAttribute("style");
            elt.setAttribute("style", existStyle + ";" + matchStyle);
        }

        var children = elt.childNodes;
        if (children.length == 0) {     // 递归出口
            return;
        }

        // 如果有子元素，则递归查找样式匹配
        for (var i = 0; i < children.length; i++) {
            this.walkElement(children[i]);
        }
    }
};

var insertionWatcher = {
    observe: function(subject, topic, data) {
        var nodesArray = subject.QueryInterface(Components.interfaces.nsIMutableArray);
        if (nodesArray.length == 0) {
            return;
        }

        if (data == "") {
            return;
        }

        // 从Excel复制的内容都会有这段被注释的<style>，根据这个来正则匹配里面的内容
        var styleReg = /<style>\n<!--([\S\s]*)-->\n<\/style>/i;
        var result = data.match(styleReg);
        if (!result) {
            return;
        }

        styleProcesser.parse(result[1]);    // 解析style标签里面的内容
        for (var i = 0; i < nodesArray.length; i++) {
            var node = nodesArray.queryElementAt(i, Components.interfaces.nsISupports);
            if (node) {
                styleProcesser.walkElement(node);
            }
        }
    }
};

window.addEventListener("load", function() {
    // 来自nsHTMLDataTransfer.cpp的通知
    var observerService = Components.classes["@mozilla.org/observer-service;1"]
        .getService(Components.interfaces.nsIObserverService);
    observerService.addObserver(insertionWatcher, "InsertHTMLContent", false);
}, false);
