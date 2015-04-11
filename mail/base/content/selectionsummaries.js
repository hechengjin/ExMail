/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


Components.utils.import("resource://gre/modules/DownloadUtils.jsm");
Components.utils.import("resource://gre/modules/PluralForm.jsm");
Components.utils.import("resource:///modules/gloda/connotent.js");
Components.utils.import("resource:///modules/gloda/mimemsg.js");
Components.utils.import("resource:///modules/templateUtils.js");

let gSelectionSummaryStrings = {
  NConversations: "NConversations",
  numMsgs: "numMsgs",
  countUnread: "countUnread",
  Nmessages: "Nmessages",
  messagesSize: "messagesSize",
  noticeText: "noticeText",
  noSubject: "noSubject",
};
let gSelectionSummaryStringsInitialized = false;

/**
 * loadSelectionSummaryStrings does the routine localization of non-pluralized
 * strings, populating the gSelectionSummaryStrings array based on the current
 * locale.
 */
function loadSelectionSummaryStrings() {
  if (gSelectionSummaryStringsInitialized)
    return;

  gSelectionSummaryStringsInitialized = true;

  // convert strings to those in the string bundle
  let getStr = function(string) document.getElementById("bundle_multimessages").getString(string);
  for (let [name, value] in Iterator(gSelectionSummaryStrings))
    gSelectionSummaryStrings[name] = typeof value == "string" ?
      getStr(value) : value.map(gSelectionSummaryStrings);
}

// Ah, wouldn't it be nice if there was platform code to do the following...

/**
 * the equivalent of jQuery's addClass.  Avoids duplicates, nothing fancy.
 *
 * @param node
 *        any old DOM node
 * @param classname
 *        a string, which will be added as a CSS class
 */
function _mm_addClass(node, classname) {
  let classes = [];
  if (node.hasAttribute('class'))
    classes = node.getAttribute('class').split(' ');

  for each (let [, klass] in Iterator(classes)) {
    if (klass == classname) // already have it
      return;
  }
  classes.push(classname);
  node.setAttribute('class', classes.join(' '));
}

/**
 * the equivalent of jQuery's removeClass.  Doesn't freak if the class name
 * isn't in the class attribute.
 *
 * @param node
 *        any old DOM node
 * @param classname
 *        a string, which will be removed from the class set.
 */
function _mm_removeClass(node, classname) {
  if (! node.hasAttribute('class'))
    return;
  let classes = node.getAttribute('class').split(' ');
  let newclasses = [];
  for each (klass in classes) {
    if (klass != classname)
      newclasses.push(klass);
  }
  node.setAttribute('class', newclasses.join(' '));
}

/**
 * Format the display name for the multi-message/thread summaries. First, try
 * using FormatDisplayName, then fall back to the header's display name or the
 * address.
 *
 * @param aHeaderParser An instance of |nsIMsgHeaderParser|
 * @param aHeaderValue  The raw header value
 * @param aContext      The context of the header field (e.g. "to", "from")
 * @return The formatted display name
 */
function _mm_FormatDisplayName(aHeaderParser, aHeaderValue, aContext)
{
  let addresses = {};
  let fullNames = {};
  let names = {};
  let numAddresses = aHeaderParser.parseHeadersWithArray(aHeaderValue,
    addresses, names, fullNames);

  if (numAddresses > 0) {
    return FormatDisplayName(addresses.value[0], names.value[0], aContext) ||
      names.value[0] || addresses.value[0];
  }
  else {
    // Something strange happened, just return the raw header value.
    return aHeaderValue;
  }
}

/**
 * the MultiMessageSummary class is responsible for populating the message pane
 * with a reasonable summary of a set of messages that span more than one
 * thread.
 *
 * It uses the same multimessage iframe as ThreadSummary, so both it
 * and ThreadSummary should be careful to clean up the other's work
 * before inserting their DOM nodes into the frame.
 *
 * There's a two phase process: build the framework based on what's available
 * from the msgHdr itself, and then spawn an aysnc Gloda query which will
 * fetch the snippets, tags, etc.
 *
 * @param aMessages
 *        Array of message headers.
 * @param [aListener]
 *        An optional listener that implements onLoadStarted and
 *        onLoadCompleted.
 */

function MultiMessageSummary(aMessages, aListener) {
  this._msgHdrs = aMessages;
  this._listener = aListener;

  // Ensure the summary selection strings are loaded.
  loadSelectionSummaryStrings();
}

MultiMessageSummary.prototype = {
  init: function() {
    this._msgTagService = Components.classes["@mozilla.org/messenger/tagservice;1"].
                          getService(Components.interfaces.nsIMsgTagService);
    this._glodaQueries = [];
    this._msgNodes = {};
    if (this._listener)
      this._listener.onLoadStarted();
    this.summarize();
  },

  /**
   * Given a msgHdr, return a list of tag objects. This function
   * just does the messy work of understanding how tags are
   * stored in nsIMsgDBHdrs.  It would be a good candidate for
   * a utility library.
   *
   * @param aMsgHdr: the msgHdr whose tags we want
   * @return a list of tag objects.
   */
  getTagsForMsg: function(aMsgHdr) {
    let keywords = aMsgHdr.getStringProperty("keywords");
    let keywordList = keywords.split(' ');
    let keywordMap = {};
    for (let iKeyword = 0; iKeyword < keywordList.length; iKeyword++) {
      let keyword = keywordList[iKeyword];
      keywordMap[keyword] = true;
    }

    let tagArray = this._msgTagService.getAllTags({});
    let tags = [];
    for (let iTag = 0; iTag < tagArray.length; iTag++) {
      let tag = tagArray[iTag];
      if (tag.key in keywordMap)
        tags.push(tag);
    }
    return tags;
  },

  /**
   * Given a name (as one sees in email headers), strip eventual
   * leading/trailing quotes (both single and double).
   *
   * @param senderName
   *     name which might be quoted
   * @return
   *     name without quotes
   **/
  stripQuotes: function(senderName) {
    if ((senderName[0] == "'" && senderName[senderName.length-1] == "'") ||
        (senderName[0] == '"' && senderName[senderName.length-1] == '"'))
      senderName = senderName.slice(1, -1);
    return senderName;
  },

  summarize: function() {
    const URL = "chrome://messenger/content/multimessageview.xhtml";
    gSummaryFrameManager.loadAndCallback(URL, this._onLoad.bind(this));
  },

  /**
   * Fill in the summary pane describing the selected messages
   **/
  _onLoad: function() {
    let htmlpane = document.getElementById('multimessage');
    // First, we group the messages in threads.
    // count threads
    let threads = {};
    let numThreads = 0;
    let headerParser = Components.classes["@mozilla.org/messenger/headerparser;1"].
                         getService(Components.interfaces.nsIMsgHeaderParser);
    let viewThreadId = function (aMsgHdr) {
      let thread = gDBView.getThreadContainingMsgHdr(aMsgHdr);
      return thread.threadKey;
    };
    for (let [,msgHdr] in Iterator(this._msgHdrs))
    {
      if (! threads[viewThreadId(msgHdr)]) {
        threads[viewThreadId(msgHdr)] = [msgHdr];
        numThreads += 1;
      } else {
        threads[viewThreadId(msgHdr)].push(msgHdr);
      }
    }

    // set the heading based on the number of messages & threads
    let heading = htmlpane.contentDocument.getElementById('heading');
    _mm_addClass(heading, "heading");
    _mm_addClass(heading, "info");

    let messagesTitle =
      PluralForm.get(numThreads, gSelectionSummaryStrings["NConversations"])
                .replace("#1", numThreads);

    heading.textContent = messagesTitle;

    // enable/disable the archive button as appropriate
    let archiveBtn = htmlpane.contentDocument.getElementById('hdrArchiveButton');
    archiveBtn.collapsed = !gFolderDisplay.canArchiveSelectedMessages;

    // clear the messages list
    let messagesElt = htmlpane.contentDocument.getElementById('messagelist');
    while (messagesElt.firstChild)
      messagesElt.removeChild(messagesElt.firstChild);

    const MAX_MESSAGES = 100;
    const SNIPPET_LENGTH = 300;
    let count = 0;
    let maxCountExceeded = false;
    var parser = new DOMParser();

    for (let [thread,msgs] in Iterator(threads)) {
      count += msgs.length;
      if (count > MAX_MESSAGES) {
        maxCountExceeded = true;
        break;
      }
      let countUnread = 0;
      let countStarred = 0;
      let header, countNode;

      // we'll mark the thread unread if any messages in it are unread
      for (let [, msgHdr] in Iterator(msgs)) {
        if (! msgHdr.isRead)
          countUnread += 1;
        if (msgHdr.isFlagged)
          countStarred += 1;
      }

      let numMsgs = msgs.length;
      let msg_classes = "message ";
      if (numMsgs > 1)
        msg_classes += " thread";
      if (countUnread)
        msg_classes += " unread";
      if (countStarred)
        msg_classes += " starred";

      let subject = msgs[0].mime2DecodedSubject || gSelectionSummaryStrings['noSubject'];
      let author = _mm_FormatDisplayName(headerParser, msgs[0].mime2DecodedAuthor, "from");

      let countstring = "";
      if (numMsgs > 1) {
        countstring += "(";
        countstring += PluralForm.get(numMsgs, gSelectionSummaryStrings["numMsgs"]).replace('#1', numMsgs);
        if (countUnread)
          countstring += PluralForm.get(numMsgs, gSelectionSummaryStrings["countUnread"]).replace('#1', countUnread);
        countstring += ")";
      }

      let msgContents = <div class="row">
                          <div class="star"/>
                          <div class="header">
                            <div class="wrappedsubject">
                              <div class="author">{author}</div>
                              <div class="subject link">{subject}</div>
                              <div class="count">{countstring}</div>
                              <div class="tags"></div>
                            </div>
                            <div class="snippet"></div>
                          </div>
                        </div>;

      let msgNode = htmlpane.contentDocument.createElement("div");
      // innerHTML is safe here because all of the data in msgContents is
      // either generated from integers or escaped to be safe.
      msgNode.innerHTML = msgContents.toXMLString();
      _mm_addClass(msgNode, msg_classes);
      messagesElt.appendChild(msgNode);

      let snippetNode = msgNode.getElementsByClassName("snippet")[0];
      let authorNode = msgNode.getElementsByClassName("author")[0];
      try {
        MsgHdrToMimeMessage(msgs[0], null, function(aMsgHdr, aMimeMsg) {
          if (aMimeMsg == null) /* shouldn't happen, but sometimes does? */
            return;

          let [text, meta] = mimeMsgToContentSnippetAndMeta(aMimeMsg,
                                                            aMsgHdr.folder,
                                                            SNIPPET_LENGTH);
          snippetNode.textContent = text;
          if (meta.author)
            authorNode.textContent = meta.author;
        }, false, {saneBodySize: true});
      } catch (e if e.result == Components.results.NS_ERROR_FAILURE) {
        // Offline messages generate exceptions, which is unfortunate.  When
        // that's fixed, this code should adapt. XXX
        snippetNode.textContent = "...";
      }

      // get the subject node.
      let subjectNode = msgNode.getElementsByClassName("subject")[0];
      subjectNode.msgs = msgs;
      subjectNode.addEventListener("click", function() {
        gFolderDisplay.selectMessages(this.msgs);
      }, true);

      let tagsNode = msgNode.getElementsByClassName("tags")[0];
      while (tagsNode.firstChild)
        tagsNode.removeChild(tagsNode.firstChild);
      this._addTagNodes(msgs, tagsNode);
      for (let [, msgHdr] in Iterator(msgs)) {
        this._msgNodes[msgHdr.messageKey + msgHdr.folder.URI] = msgNode;
      }
      messagesElt.appendChild(msgNode);
    }
    this.computeSize(htmlpane);
    this.notifyMaxCountExceeded(htmlpane.contentDocument, this._msgHdrs.length, MAX_MESSAGES);

    this._glodaQueries.push(Gloda.getMessageCollectionForHeaders(this._msgHdrs, this));
    htmlpane.contentDocument.defaultView.adjustHeadingSize();
  },

  /**
   * clear out the tagsnode, and fill in appropriately for the union of
   * tags in the specified messags
   */
  _addTagNodes: function(msgs, tagsNode) {
      // for tags, stars, and read/unread status, we want to map
      // from all messages to one node
      let tags = {};
      for each (let [, msgHdr] in Iterator(msgs)) {
        let msgTags = this.getTagsForMsg(msgHdr);
        for each (let [,tag] in Iterator(msgTags)) {
          if (!(tag.key in tags)) {
            tags[tag.key] = tag;
          }
        }
      }
      for each (let [, tag] in Iterator(tags)) {
        let tagNode = tagsNode.ownerDocument.createElement('span');
        // see tagColors.css
        let colorClass = "blc-" + this._msgTagService.getColorForKey(tag.key).substr(1);
        _mm_addClass(tagNode, "tag " + tag.tag + " " + colorClass);
        tagNode.textContent = tag.tag;
        tagsNode.appendChild(tagNode);
      }
  },

  /**
   * compute the size of the messages in the selection and display it
   * in the element of id "size"
  **/
  computeSize: function(htmlpane) {
    let numThreads = 0;
    let numBytes = 0;

    for (let [,msgHdr] in Iterator(this._msgHdrs))
      numBytes += msgHdr.messageSize; // XXX do something about news?
    let [size, unit] = DownloadUtils.convertByteUnits(numBytes);
    let sizeText = replaceInsert(gSelectionSummaryStrings.messagesSize, 1, size);
    sizeText = replaceInsert(sizeText, 2, unit);
    htmlpane.contentDocument.getElementById('size').textContent = sizeText;
  },

  /** Indicate if we're not summarizing _all_ of the specified messages because
   * that'd just be too much.
  **/
  notifyMaxCountExceeded: function(aContentDocument, aNumMessages, aMaxCount) {
    let notice = aContentDocument.getElementById('notice');
    if (aNumMessages > aMaxCount) {
      let noticeText = gSelectionSummaryStrings.noticeText;
      noticeText = replaceInsert(noticeText, 1, aNumMessages);
      noticeText = replaceInsert(noticeText, 2, aMaxCount);
      notice.textContent = noticeText;
      _mm_removeClass(notice, 'hidden');
    } else {
      _mm_addClass(notice, 'hidden');
    }
  },

  // these are listeners for the gloda collections.
  onItemsAdded: function(aItems) {
  },
  onItemsModified: function(aItems) {
    this.processItems(aItems);
  },
  onItemsRemoved: function(aItems) {
  },

  /**
   * Given a set of items from a gloda collection, process them and update
   * the display accordingly.
   *
   * @param aItems
   *        contents of a gloda collection
  **/
  processItems: function(aItems) {
    let knownMessageNodes = [];

    for (let [,glodaMsg] in Iterator(aItems)) {
      let messageKey = glodaMsg.messageKey;
      let domkey = messageKey + glodaMsg.folder.uri;

      // Unread and starred will get set if any of the messages in a
      // collapsed thread qualify
      // The trick here is that we may get multiple items corresponding to the same
      // thread (and hence DOM node), so we need to detect when we get the first
      // item for a particular DOM node, stash the preexisting status of that DOM
      // node, an only do transitions if the items warrant it.
      let headerNode = this._msgNodes[domkey];
      if (! headerNode.flags) {
        headerNode.flags = {};
        knownMessageNodes.push(headerNode);
      }

      if (! glodaMsg.read)
        headerNode.flags['unread'] = true;
      if (glodaMsg.starred)
        headerNode.flags['starred'] = true;

      // for tags, there's a minor problem in that if _some_ of the items in a
      // thread got modified
      let key = messageKey + glodaMsg.folder.uri;
      let tagsNode = headerNode.getElementsByClassName('tags')[0];
      while (tagsNode.firstChild)
        tagsNode.removeChild(tagsNode.firstChild);
      this._addTagNodes([msg.folderMessage for each ([i,msg] in Iterator(aItems))],
                        tagsNode);
    }

    for ([, headerNode] in Iterator(knownMessageNodes)) {
      if (headerNode.flags['unread'])
        _mm_addClass(headerNode, "unread");
      else
        _mm_removeClass(headerNode, "unread");
      if (headerNode.flags['starred'])
        _mm_addClass(headerNode, "starred");
      else
        _mm_removeClass(headerNode, "starred");
      headerNode.flags = null;
    }
  },

  onQueryCompleted: function(aCollection) {
    /* if we need something that's just available from GlodaMessages,
      this is where we'll get it initially */
    if (this._listener)
      this._listener.onLoadCompleted();
    return;
  }
}


/**
 * the ThreadSummary class is responsible for populating the message pane
 * with a reasonable summary of a set of messages that are are in a single
 * thread.
 *
 * It uses the same multimessage iframe as MultiMessageSummary, so both it
 * and MultiMessageSummary should be careful to clean up the other's work
 * before inserting their DOM nodes into the frame.
 *
 * There's a two phase process: build the framework based on what's available
 * from the msgHdr itself, and then spawn an aysnc Gloda query which will
 * fetch the snippets, tags, etc.
 *
 * @param aMessages
 *        Array of message headers.
 * @param [aListener]
 *        An optional listener that implements onLoadStarted and
 *        onLoadCompleted.
 */

function ThreadSummary(aMessages, aListener)
{
  this._msgHdrs = aMessages;
  this._listener = aListener;

  // Ensure the summary selection strings are loaded.
  loadSelectionSummaryStrings();
}

ThreadSummary.prototype = {
  __proto__: MultiMessageSummary.prototype,

  _onLoad: function() {
    this._msgNodes = {};

    let htmlpane = document.getElementById('multimessage');

    let firstMsgHdr = this._msgHdrs[0];
    let numMessages = this._msgHdrs.length;
    let subject = (firstMsgHdr.mime2DecodedSubject || gSelectionSummaryStrings["noSubject"])
       + " "
       + PluralForm.get(numMessages, gSelectionSummaryStrings["Nmessages"]).replace('#1', numMessages);
    let heading = htmlpane.contentDocument.getElementById('heading');
    heading.setAttribute("class", "heading");
    heading.textContent = subject;

    // enable/disable the archive button as appropriate
    let archiveBtn = htmlpane.contentDocument.getElementById('hdrArchiveButton');
    archiveBtn.collapsed = !gFolderDisplay.canArchiveSelectedMessages;

    let messagesElt = htmlpane.contentDocument.getElementById('messagelist');
    while (messagesElt.firstChild)
      messagesElt.removeChild(messagesElt.firstChild);

    let headerParser = Components.classes["@mozilla.org/messenger/headerparser;1"]
                                    .getService(Components.interfaces.nsIMsgHeaderParser);
    let count = 0;
    const MAX_THREADS = 100;
    const SNIPPET_LENGTH = 300;
    let maxCountExceeded = false;
    for (let i = 0; i < numMessages; ++i) {
      count += 1;
      if (count > MAX_THREADS) {
        maxCountExceeded = true;
        break;
      }
      let msgHdr = this._msgHdrs[i];

      let msg_classes = "message ";
      if (! msgHdr.isRead)
        msg_classes += " unread";
      if (msgHdr.isFlagged)
        msg_classes += " starred";

      let senderName = _mm_FormatDisplayName(headerParser, msgHdr.mime2DecodedAuthor, "from");
      let date = makeFriendlyDateAgo(new Date(msgHdr.date/1000));

      let msgContents = <div class="row">
                          <div class="star"/>
                          <div class="header">
                            <div class="wrappedsender">
                              <div class="sender link">{senderName}</div>
                              <div class="date">{date}</div>
                              <div class="tags"></div>
                            </div>
                            <div class="snippet"></div>
                          </div>
                        </div>;

      let msgNode = htmlpane.contentDocument.createElement("div");
      // innerHTML is safe here because all of the data in msgContents is
      // either generated from integers or escaped to be safe.
      msgNode.innerHTML = msgContents.toXMLString();
      _mm_addClass(msgNode, msg_classes);
      messagesElt.appendChild(msgNode);

      let key = msgHdr.messageKey + msgHdr.folder.URI;
      let snippetNode = msgNode.getElementsByClassName("snippet")[0];
      let senderNode = msgNode.getElementsByClassName("sender")[0];
      try {
        MsgHdrToMimeMessage(msgHdr, null, function(aMsgHdr, aMimeMsg) {
          if (aMimeMsg == null) /* shouldn't happen, but sometimes does? */ {
            return;
          }
          let [text, meta] = mimeMsgToContentSnippetAndMeta(aMimeMsg,
                                                            aMsgHdr.folder,
                                                            SNIPPET_LENGTH);
          snippetNode.textContent = text;
          if (meta.author)
            senderNode.textContent = meta.author;
        }, false, {saneBodySize: true});
      } catch (e if e.result == Components.results.NS_ERROR_FAILURE) {
        // Offline messages generate exceptions, which is unfortunate.  When
        // that's fixed, this code should adapt. XXX
        snippetNode.textContent = "...";
      }
      let tagsNode = msgNode.getElementsByClassName("tags")[0];
      let tags = this.getTagsForMsg(msgHdr);
      for each (let [,tag] in Iterator(tags)) {
        let tagNode = tagsNode.ownerDocument.createElement('span');
        // see tagColors.css
        let colorClass = "blc-" + this._msgTagService.getColorForKey(tag.key).substr(1);
        _mm_addClass(tagNode, "tag " + tag.tag + " " + colorClass);
        tagNode.textContent = tag.tag;
        tagsNode.appendChild(tagNode);
      }

      let sender = msgNode.getElementsByClassName("sender")[0];
      sender.msgHdr = msgHdr;
      sender.addEventListener("click", function(e) {
        // if the msg is the first message in a collapsed thread, we need to
        // uncollapse it.
        let origRowCount = gDBView.rowCount;
        let viewIndex = gDBView.findIndexOfMsgHdr(e.target.msgHdr, true);
        gDBView.selectFolderMsgByKey(this.folder, this.msgKey);
        if (gDBView.rowCount != origRowCount)
          gDBView.selectionChanged();
      }, true);
      sender.folder = msgHdr.folder;
      sender.msgKey = msgHdr.messageKey;

      this._msgNodes[key] = msgNode;

      messagesElt.appendChild(msgNode);
    }
    // stash somewhere so it doesn't get GC'ed
    this._glodaQueries.push(Gloda.getMessageCollectionForHeaders(this._msgHdrs, this));
    this.notifyMaxCountExceeded(htmlpane.contentDocument, numMessages, MAX_THREADS);

    this.computeSize(htmlpane);
    htmlpane.contentDocument.defaultView.adjustHeadingSize();
  }
};

// We use a global to prevent GC of gloda collection (and we reuse it to prevent
// leaks).  Without a global, the GC is aggressive enough that the gloda query
// is gone before it returns.
var gSummary;


/**
 * Given an array of messages which are all in the
 * same thread, summarize them.
 *
 * @param aSelectedMessages
 *        Array of message headers.
 * @param [aListener]
 *        A listener that implements onLoadStarted and onLoadCompleted.
 */
function summarizeThread(aSelectedMessages, aListener)
{
  if (aSelectedMessages.length == 0)
    return;

  try {
    gSummary = new ThreadSummary(aSelectedMessages, aListener);
    gSummary.init();
  } catch (e) {
    dump("Exception in summarizeThread" + e + "\n");
    logException(e);
    Components.utils.reportError(e);
    throw(e);
  }
}

/**
 * Given an array of message URIs, cause the message pane
 * to display a summary of them.
 *
 * @param aSelectedMessages
 *        Array of message headers.
 * @param [aListener]
 *        A listener that implements onLoadStarted and onLoadCompleted.
 */
function summarizeMultipleSelection(aSelectedMessages, aListener)
{
  if (aSelectedMessages.length == 0)
    return;
  try {
    gSummary = new MultiMessageSummary(aSelectedMessages, aListener);
    gSummary.init();
  } catch (e) {
    dump("Exception in summarizeMultipleSelection" + e + "\n");
    Components.utils.reportError(e);
    throw(e);
  }
}


/**
 * Helper function to escape some XML chars, so they display properly in
 * innerHTML.
 *
 * @param s
 *        input text
 * @return The string with <, >, and & replaced by the corresponding entities.
 */
function escapeXMLchars(s)
{
  return s.replace(/[<>&]/g, function(s) {
      switch (s) {
          case "<": return "&lt;";
          case ">": return "&gt;";
          case "&": return "&amp;";
          default: throw Error("Unexpected match");
          }
      }
  );
}
