/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const EXPORTED_SYMBOLS = ["GlodaAttributeDBDef", "GlodaAccount",
                    "GlodaConversation", "GlodaFolder", "GlodaMessage",
                    "GlodaContact", "GlodaIdentity", "GlodaAttachment"];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource:///modules/mailServices.js");

Cu.import("resource:///modules/gloda/log4moz.js");
const LOG = Log4Moz.repository.getLogger("gloda.datamodel");

Cu.import("resource:///modules/gloda/utils.js");

// Make it lazy.
let gMessenger;
function getMessenger () {
  if (!gMessenger)
    gMessenger = Cc["@mozilla.org/messenger;1"].createInstance(Ci.nsIMessenger);
  return gMessenger;
}

/**
 * @class Represents a gloda attribute definition's DB form.  This class
 *  stores the information in the database relating to this attribute
 *  definition.  Access its attrDef attribute to get at the realy juicy data.
 *  This main interesting thing this class does is serve as the keeper of the
 *  mapping from parameters to attribute ids in the database if this is a
 *  parameterized attribute.
 */
function GlodaAttributeDBDef(aDatastore, aID, aCompoundName, aAttrType,
                           aPluginName, aAttrName) {
  // _datastore is now set on the prototype by GlodaDatastore
  this._id = aID;
  this._compoundName = aCompoundName;
  this._attrType = aAttrType;
  this._pluginName = aPluginName;
  this._attrName = aAttrName;

  this.attrDef = null;

  /** Map parameter values to the underlying database id. */
  this._parameterBindings = {};
}

GlodaAttributeDBDef.prototype = {
  // set by GlodaDatastore
  _datastore: null,
  get id() { return this._id; },
  get attributeName() { return this._attrName; },

  get parameterBindings() { return this._parameterBindings; },

  /**
   * Bind a parameter value to the attribute definition, allowing use of the
   *  attribute-parameter as an attribute.
   *
   * @return
   */
  bindParameter: function gloda_attr_bindParameter(aValue) {
    // people probably shouldn't call us with null, but handle it
    if (aValue == null) {
      return this._id;
    }
    if (aValue in this._parameterBindings) {
      return this._parameterBindings[aValue];
    }
    // no database entry exists if we are here, so we must create it...
    let id = this._datastore._createAttributeDef(this._attrType,
                 this._pluginName, this._attrName, aValue);
    this._parameterBindings[aValue] = id;
    this._datastore.reportBinding(id, this, aValue);
    return id;
  },

  /**
   * Given a list of values, return a list (regardless of plurality) of
   *  database-ready [attribute id,  value] tuples.  This is intended to be used
   *  to directly convert the value of a property on an object that corresponds
   *  to a bound attribute.
   *
   * @param {Array} aInstanceValues An array of instance values regardless of
   *     whether or not the attribute is singular.
   */
  convertValuesToDBAttributes:
      function gloda_attr_convertValuesToDBAttributes(aInstanceValues) {
    let nounDef = this.attrDef.objectNounDef;
    let dbAttributes = [];
    if (nounDef.usesParameter) {
      for each (let [, instanceValue] in Iterator(aInstanceValues)) {
        let [param, dbValue] = nounDef.toParamAndValue(instanceValue);
        dbAttributes.push([this.bindParameter(param), dbValue]);
      }
    }
    else {
      // Not generating any attributes is ok. This basically means the noun is
      // just an informative property on the Gloda Message and has no real
      // indexing purposes.
      if ("toParamAndValue" in nounDef) {
        for each (let [, instanceValue] in Iterator(aInstanceValues)) {
          dbAttributes.push([this._id,
                             nounDef.toParamAndValue(instanceValue)[1]]);
        }
      }
    }
    return dbAttributes;
  },

  toString: function() {
    return this._compoundName;
  }
};

let GlodaHasAttributesMixIn = {
  enumerateAttributes: function gloda_attrix_enumerateAttributes() {
    let nounDef = this.NOUN_DEF;
    for each (let [key, value] in Iterator(this)) {
      let attrDef = nounDef.attribsByBoundName[key];
      // we expect to not have attributes for underscore prefixed values (those
      //  are managed by the instance's logic.  we also want to not explode
      //  should someone crap other values in there, we get both birds with this
      //  one stone.
      if (attrDef === undefined)
        continue;
      if (attrDef.singular) {
        // ignore attributes with null values
        if (value != null)
          yield [attrDef, [value]];
      }
      else {
        // ignore attributes with no values
        if (value.length)
          yield [attrDef, value];
      }
    }
  },

  domContribute: function gloda_attrix_domContribute(aDomNode) {
    let nounDef = this.NOUN_DEF;
    for each (let [attrName, attr] in
        Iterator(nounDef.domExposeAttribsByBoundName)) {
      if (this[attrName])
        aDomNode.setAttribute(attr.domExpose, this[attrName]);
    }
  },
};

function MixIn(aConstructor, aMixIn) {
  let proto = aConstructor.prototype;
  for (let [name, func] in Iterator(aMixIn)) {
    if (name.substring(0, 4) == "get_")
      proto.__defineGetter__(name.substring(4), func);
    else
      proto[name] = func;
  }
}

/**
 * @class A gloda wrapper around nsIMsgIncomingServer.
 */
function GlodaAccount(aIncomingServer) {
  this._incomingServer = aIncomingServer;
}

GlodaAccount.prototype = {
  NOUN_ID: 106,
  get id() { return this._incomingServer.key; },
  get name() { return this._incomingServer.prettyName; },
  get incomingServer() { return this._incomingServer; },
  toString: function gloda_account_toString() {
    return "Account: " + this.id;
  },

  toLocaleString: function gloda_account_toLocaleString() {
    return this.name;
  }
};

/**
 * @class A gloda conversation (thread) exists so that messages can belong.
 */
function GlodaConversation(aDatastore, aID, aSubject, aOldestMessageDate,
                           aNewestMessageDate) {
  // _datastore is now set on the prototype by GlodaDatastore
  this._id = aID;
  this._subject = aSubject;
  this._oldestMessageDate = aOldestMessageDate;
  this._newestMessageDate = aNewestMessageDate;
}

GlodaConversation.prototype = {
  NOUN_ID: 101,
  // set by GlodaDatastore
  _datastore: null,
  get id() { return this._id; },
  get subject() { return this._subject; },
  get oldestMessageDate() { return this._oldestMessageDate; },
  get newestMessageDate() { return this._newestMessageDate; },

  getMessagesCollection: function gloda_conversation_getMessagesCollection(
    aListener, aData) {
    let query = new GlodaMessage.prototype.NOUN_DEF.queryClass();
    query.conversation(this._id).orderBy("date");
    return query.getCollection(aListener, aData);
  },

  toString: function gloda_conversation_toString() {
    return "Conversation:" + this._id;
  },

  toLocaleString: function gloda_conversation_toLocaleString() {
    return this._subject;
  }
};

function GlodaFolder(aDatastore, aID, aURI, aDirtyStatus, aPrettyName,
                     aIndexingPriority) {
  // _datastore is now set by GlodaDatastore
  this._id = aID;
  this._uri = aURI;
  this._dirtyStatus = aDirtyStatus;
  this._prettyName = aPrettyName;
  this._xpcomFolder = null;
  this._account = null;
  this._activeIndexing = false;
  this._activeHeaderRetrievalLastStamp = 0;
  this._indexingPriority = aIndexingPriority;
  this._deleted = false;
  this._compacting = false;
}

GlodaFolder.prototype = {
  NOUN_ID: 100,
  // set by GlodaDatastore
  _datastore: null,

  /** The folder is believed to be up-to-date */
  kFolderClean: 0,
  /** The folder has some un-indexed or dirty messages */
  kFolderDirty: 1,
  /** The folder needs to be entirely re-indexed, regardless of the flags on
   * the messages in the folder. This state will be downgraded to dirty */
  kFolderFilthy: 2,

  _kFolderDirtyStatusMask: 0x7,
  /**
   * The (local) folder has been compacted and all of its message keys are
   *  potentially incorrect.  This is not a possible state for IMAP folders
   *  because their message keys are based on UIDs rather than offsets into
   *  the mbox file.
   */
  _kFolderCompactedFlag: 0x8,

  /** The folder should never be indexed. */
  kIndexingNeverPriority: -1,
  /** The lowest priority assigned to a folder. */
  kIndexingLowestPriority: 0,
  /** The highest priority assigned to a folder. */
  kIndexingHighestPriority: 100,

  /** The indexing priority for a folder if no other priority is assigned. */
  kIndexingDefaultPriority: 20,
  /** Folders marked check new are slightly more important I guess. */
  kIndexingCheckNewPriority: 30,
  /** Favorite folders are more interesting to the user, presumably. */
  kIndexingFavoritePriority: 40,
  /** The indexing priority for inboxes. */
  kIndexingInboxPriority: 50,
  /** The indexing priority for sent mail folders. */
  kIndexingSentMailPriority: 60,

  get id() { return this._id; },
  get uri() { return this._uri; },
  get dirtyStatus() {
    return this._dirtyStatus & this._kFolderDirtyStatusMask;
  },
  /**
   * Mark a folder as dirty if it was clean.  Do nothing if it was already dirty
   *  or filthy.  For use by GlodaMsgIndexer only.  And maybe rkent and his
   *  marvelous extensions.
   */
  _ensureFolderDirty: function gloda_folder__markFolderDirty() {
    if (this.dirtyStatus == this.kFolderClean) {
      this._dirtyStatus = (this.kFolderDirty & this._kFolderDirtyStatusMask) |
                          (this._dirtyStatus & ~this._kFolderDirtyStatusMask);
      this._datastore.updateFolderDirtyStatus(this);
    }
  },
  /**
   * Definitely for use only by GlodaMsgIndexer to downgrade the dirty status of
   *  a folder.
   */
  _downgradeDirtyStatus: function gloda_folder__downgradeDirtyStatus(
                           aNewStatus) {
    if (this.dirtyStatus != aNewStatus) {
      this._dirtyStatus = (aNewStatus & this._kFolderDirtyStatusMask) |
                          (this._dirtyStatus & ~this._kFolderDirtyStatusMask);
      this._datastore.updateFolderDirtyStatus(this);
    }
  },
  /**
   * Indicate whether this folder is currently being compacted.  The
   *  |GlodaMsgIndexer| keeps this in-memory-only value up-to-date.
   */
  get compacting() {
    return this._compacting;
  },
  /**
   * Set whether this folder is currently being compacted.  This is really only
   *  for the |GlodaMsgIndexer| to set.
   */
  set compacting(aCompacting) {
    this._compacting = aCompacting;
  },
  /**
   * Indicate whether this folder was compacted and has not yet been
   *  compaction processed.
   */
  get compacted() {
    return Boolean(this._dirtyStatus & this._kFolderCompactedFlag);
  },
  /**
   * For use only by GlodaMsgIndexer to set/clear the compaction state of this
   *  folder.
   */
  _setCompactedState: function gloda_folder__clearCompactedState(aCompacted) {
    if (this.compacted != aCompacted) {
      if (aCompacted)
        this._dirtyStatus |= this._kFolderCompactedFlag;
      else
        this._dirtyStatus &= ~this._kFolderCompactedFlag;
      this._datastore.updateFolderDirtyStatus(this);
    }
  },

  get name() { return this._prettyName; },
  toString: function gloda_folder_toString() {
    return "Folder:" + this._id;
  },

  toLocaleString: function gloda_folder_toLocaleString() {
    let xpcomFolder = this.getXPCOMFolder(this.kActivityFolderOnlyNoData);
    if (!xpcomFolder)
      return this._prettyName;
    return xpcomFolder.prettiestName +
      " (" + xpcomFolder.rootFolder.prettiestName + ")";
  },

  get indexingPriority() {
    return this._indexingPriority;
  },

  /** We are going to index this folder. */
  kActivityIndexing: 0,
  /** Asking for the folder to perform header retrievals. */
  kActivityHeaderRetrieval: 1,
  /** We only want the folder for its metadata but are not going to open it. */
  kActivityFolderOnlyNoData: 2,


  /** Is this folder known to be actively used for indexing? */
  _activeIndexing: false,
  /** Get our indexing status. */
  get indexing() {
    return this._activeIndexing;
  },
  /**
   * Set our indexing status.  Normally, this will be enabled through passing
   *  an activity type of kActivityIndexing (which will set us), but we will
   *  still need to be explicitly disabled by the indexing code.
   * When disabling indexing, we will call forgetFolderIfUnused to take care of
   *  shutting things down.
   * We are not responsible for committing changes to the message database!
   *  That is on you!
   */
  set indexing(aIndexing) {
    this._activeIndexing = aIndexing;
    if (!aIndexing)
      this.forgetFolderIfUnused();
  },
  /** When was this folder last used for header retrieval purposes? */
  _activeHeaderRetrievalLastStamp: 0,

  /**
   * Retrieve the nsIMsgFolder instance corresponding to this folder, providing
   *  an explanation of why you are requesting it for tracking/cleanup purposes.
   *
   * @param aActivity One of the kActivity* constants.  If you pass
   *     kActivityIndexing, we will set indexing for you, but you will need to
   *     clear it when you are done.
   * @return The nsIMsgFolder if available, null on failure.
   */
  getXPCOMFolder: function gloda_folder_getXPCOMFolder(aActivity) {
    if (!this._xpcomFolder) {
      let rdfService = Cc['@mozilla.org/rdf/rdf-service;1']
                         .getService(Ci.nsIRDFService);
      this._xpcomFolder = rdfService.GetResource(this.uri)
                                    .QueryInterface(Ci.nsIMsgFolder);
    }
    switch (aActivity) {
      case this.kActivityIndexing:
        // mark us as indexing, but don't bother with live tracking.  we do
        //  that independently and only for header retrieval.
        this.indexing = true;
        break;
      case this.kActivityHeaderRetrieval:
        if (this._activeHeaderRetrievalLastStamp === 0)
          this._datastore.markFolderLive(this);
        this._activeHeaderRetrievalLastStamp = Date.now();
        break;
      case this.kActivityFolderOnlyNoData:
        // we don't have to do anything here.
        break;
    }

    return this._xpcomFolder;
  },

  /**
   * Retrieve a GlodaAccount instance corresponding to this folder.
   *
   * @return The GlodaAccount instance.
   */
  getAccount: function gloda_folder_getAccount() {
    if (!this._account) {
      let msgFolder = this.getXPCOMFolder(this.kActivityFolderOnlyNoData);
      this._account = new GlodaAccount(msgFolder.server);
    }
    return this._account;
  },

  /**
   * How many milliseconds must a folder have not had any header retrieval
   *  activity before it's okay to lose the database reference?
   */
  ACCEPTABLY_OLD_THRESHOLD: 10000,

  /**
   * Cleans up our nsIMsgFolder reference if we have one and it's not "in use".
   * In use, from our perspective, means that it is not being used for indexing
   *  and some arbitrary interval of time has elapsed since it was last
   *  retrieved for header retrieval reasons.  The time interval is because if
   *  we have one GlodaMessage requesting a header, there's a high probability
   *  that another message will request a header in the near future.
   * Because setting indexing to false disables us, we are written in an
   *  idempotent fashion.  (It is possible for disabling indexing's call to us
   *  to cause us to return true but for the datastore's timer call to have not
   *  yet triggered.)
   *
   * @returns true if we are cleaned up and can be considered 'dead', false if
   *     we should still be considered alive and this method should be called
   *     again in the future.
   */
  forgetFolderIfUnused: function gloda_folder_forgetFolderIfUnused() {
    // we are not cleaning/cleaned up if we are indexing
    if (this._activeIndexing)
      return false;

    // set a point in the past as the threshold.  the timestamp must be older
    //  than this to be eligible for cleanup.
    let acceptablyOld = Date.now() - this.ACCEPTABLY_OLD_THRESHOLD;
    // we are not cleaning/cleaned up if we have retrieved a header more
    //  recently than the acceptably old threshold.
    if (this._activeHeaderRetrievalLastStamp > acceptablyOld)
      return false;

    if (this._xpcomFolder) {
      // This is the key action we take; the nsIMsgFolder will continue to
      //  exist, but we want it to forget about its database so that it can
      //  be closed and its memory can be reclaimed.
      this._xpcomFolder.msgDatabase = null;
      this._xpcomFolder = null;
      // since the last retrieval time tracks whether we have marked live or
      //  not, this needs to be reset to 0 too.
      this._activeHeaderRetrievalLastStamp = 0;
    }

    return true;
  }
};

/**
 * @class A message representation.
 */
function GlodaMessage(aDatastore, aID, aFolderID, aMessageKey,
                      aConversationID, aConversation, aDate,
                      aHeaderMessageID, aDeleted, aJsonText,
                      aNotability,
                      aSubject, aIndexedBodyText, aAttachmentNames) {
  // _datastore is now set on the prototype by GlodaDatastore
  this._id = aID;
  this._folderID = aFolderID;
  this._messageKey = aMessageKey;
  this._conversationID = aConversationID;
  this._conversation = aConversation;
  this._date = aDate;
  this._headerMessageID = aHeaderMessageID;
  this._jsonText = aJsonText;
  this._notability = aNotability;
  this._subject = aSubject;
  this._indexedBodyText = aIndexedBodyText;
  this._attachmentNames = aAttachmentNames;

  // _jsonText后续会被处理掉，不再可用，因此引入新的属性
  this._jsonAttributes = aJsonText;

  // only set _deleted if we're deleted, otherwise the undefined does our
  //  speaking for us.
  if (aDeleted)
    this._deleted = aDeleted;
}

GlodaMessage.prototype = {
  NOUN_ID: 102,
  // set by GlodaDatastore
  _datastore: null,
  get id() { return this._id; },
  get folderID() { return this._folderID; },
  get messageKey() { return this._messageKey; },
  get conversationID() { return this._conversationID; },
  // conversation is special
  get headerMessageID() { return this._headerMessageID; },
  get jsonAttributes() { return this._jsonAttributes; },
  get notability() { return this._notability; },
  set notability(aNotability) { this._notability = aNotability; },

  get subject() { return this._subject; },
  get indexedBodyText() { return this._indexedBodyText; },
  get attachmentNames() { return this._attachmentNames; },

  get date() { return this._date; },
  set date(aNewDate) { this._date = aNewDate; },

  get folder() {
    // XXX due to a deletion bug it is currently possible to get in a state
    //  where we have an illegal folderID value.  This will result in an
    //  exception.  As a workaround, let's just return null in that case.
    try {
      if (this._folderID != null)
        return this._datastore._mapFolderID(this._folderID);
    }
    catch (ex) {
    }
    return null;
  },
  get folderURI() {
    // XXX just like for folder, handle mapping failures and return null
    try {
      if (this._folderID != null)
        return this._datastore._mapFolderID(this._folderID).uri;
    }
    catch (ex) {
    }
    return null;
  },
  get account() {
    // XXX due to a deletion bug it is currently possible to get in a state
    //  where we have an illegal folderID value.  This will result in an
    //  exception.  As a workaround, let's just return null in that case.
    try {
      if (this._folderID == null)
        return null;
      let folder = this._datastore._mapFolderID(this._folderID);
      return folder.getAccount();
    }
    catch (ex) { }
    return null;
  },
  get conversation() {
    return this._conversation;
  },

  toString: function gloda_message_toString() {
    // uh, this is a tough one...
    return "Message:" + this._id;
  },

  _clone: function gloda_message_clone() {
    return new GlodaMessage(/* datastore */ null, this._id, this._folderID,
      this._messageKey, this._conversationID, this._conversation, this._date,
      this._headerMessageID, this._deleted, this._jsonText, this._notability,
      this._subject, this._indexedBodyText, this._attachmentNames);
  },

  /**
   * Provide a means of propagating changed values on our clone back to
   *  ourselves.  This is required because of an object identity trick gloda
   *  does; when indexing an already existing object, all mutations happen on
   *  a clone of the existing object so that
   */
  _declone: function gloda_message_declone(aOther) {
    if ("_content" in aOther)
      this._content = aOther._content;

    // The _indexedAuthor/_indexedRecipients fields don't get updated on
    //  fulltext update so we don't need to propagate.
    this._indexedBodyText = aOther._indexedBodyText;
    this._attachmentNames = aOther._attachmentNames;
  },

  /**
   * Mark this message as a ghost.  Ghosts are characterized by having no folder
   *  id and no message key.  They also are not deleted or they would be of
   *  absolutely no use to us.
   *
   * These changes are suitable for persistence.
   */
  _ghost: function gloda_message_ghost() {
    this._folderID = null;
    this._messageKey = null;
    if ("_deleted" in this)
      delete this._deleted;
  },

  /**
   * Are we a ghost (which implies not deleted)?  We are not a ghost if we have
   *  a definite folder location (we may not know our message key in the case
   *  of IMAP moves not fully completed) and are not deleted.
   */
  get _isGhost() {
    return this._folderID == null && !this._isDeleted;
  },

  /**
   * If we were dead, un-dead us.
   */
  _ensureNotDeleted: function gloda_message__ensureNotDeleted() {
    if ("_deleted" in this)
      delete this._deleted;
  },

  /**
   * Are we deleted?  This is private because deleted gloda messages are not
   *  visible to non-core-gloda code.
   */
  get _isDeleted() {
    return ("_deleted" in this) && this._deleted;
  },

  /**
   * Trash this message's in-memory representation because it should no longer
   *  be reachable by any code.  The database record is gone, it's not coming
   *  back.
   */
  _objectPurgedMakeYourselfUnpleasant: function gloda_message_nuke() {
    this._id = null;
    this._folderID = null;
    this._messageKey = null;
    this._conversationID = null;
    this._conversation = null;
    this.date = null;
    this._headerMessageID = null;
  },

  /**
   * Return the underlying nsIMsgDBHdr from the folder storage for this, or
   *  null if the message does not exist for one reason or another.  We may log
   *  to our logger in the failure cases.
   *
   * This method no longer caches the result, so if you need to hold onto it,
   *  hold onto it.
   *
   * In the process of retrieving the underlying message header, we may have to
   *  open the message header database associated with the folder.  This may
   *  result in blocking while the load happens, so you may want to try and find
   *  an alternate way to initiate the load before calling us.
   * We provide hinting to the GlodaDatastore via the GlodaFolder so that it
   *  knows when it's a good time for it to go and detach from the database.
   *
   * @returns The nsIMsgDBHdr associated with this message if available, null on
   *     failure.
   */
  get folderMessage() {
    if (this._folderID === null || this._messageKey === null)
      return null;

    // XXX like for folder and folderURI, return null if we can't map the folder
    let glodaFolder;
    try {
      glodaFolder = this._datastore._mapFolderID(this._folderID);
    }
    catch (ex) {
      return null;
    }
    let folder = glodaFolder.getXPCOMFolder(
                   glodaFolder.kActivityHeaderRetrieval);
    if (folder) {
      let folderMessage;
      try {
        folderMessage = folder.GetMessageHeader(this._messageKey);
      }
      catch (ex) {
        folderMessage = null;
      }
      if (folderMessage !== null) {
        // verify the message-id header matches what we expect...
        if (folderMessage.messageId != this._headerMessageID) {
          LOG.info("Message with message key " + this._messageKey +
                   " in folder '" + folder.URI + "' does not match expected " +
                   "header! (" + this._headerMessageID + " expected, got " +
                   folderMessage.messageId + ")");
          folderMessage = null;
        }
      }
      return folderMessage;
    }

    // this only gets logged if things have gone very wrong.  we used to throw
    //  here, but it's unlikely our caller can do anything more meaningful than
    //  treating this as a disappeared message.
    LOG.info("Unable to locate folder message for: " + this._folderID + ":" +
             this._messageKey);
    return null;
  },
  get folderMessageURI() {
    let folderMessage = this.folderMessage;
    if (folderMessage)
      return folderMessage.folder.getUriForMsg(folderMessage);
    else
      return null;
  }
};
MixIn(GlodaMessage, GlodaHasAttributesMixIn);

/**
 * @class Contacts correspond to people (one per person), and may own multiple
 *  identities (e-mail address, IM account, etc.)
 */
function GlodaContact(aDatastore, aID, aDirectoryUUID, aContactUUID, aName,
                      aPopularity, aFrecency, aJsonText) {
  // _datastore set on the prototype by GlodaDatastore
  this._id = aID;
  this._directoryUUID = aDirectoryUUID;
  this._contactUUID = aContactUUID;
  this._name = aName;
  this._popularity = aPopularity;
  this._frecency = aFrecency;
  if (aJsonText)
    this._jsonText = aJsonText;

  this._identities = null;
}

GlodaContact.prototype = {
  NOUN_ID: 103,
  // set by GlodaDatastore
  _datastore: null,

  get id() { return this._id; },
  get directoryUUID() { return this._directoryUUID; },
  get contactUUID() { return this._contactUUID; },
  get name() { return this._name; },
  set name(aName) { this._name = aName; },

  get popularity() { return this._popularity; },
  set popularity(aPopularity) {
    this._popularity = aPopularity;
    this.dirty = true;
  },

  get frecency() { return this._frecency; },
  set frecency(aFrecency) {
    this._frecency = aFrecency;
    this.dirty = true;
  },

  get identities() {
    return this._identities;
  },

  toString: function gloda_contact_toString() {
    return "Contact:" + this._id;
  },

  get accessibleLabel() {
    return "Contact: " + this._name;
  },

  _clone: function gloda_contact_clone() {
    return new GlodaContact(/* datastore */ null, this._id, this._directoryUUID,
      this._contactUUID, this._name, this._popularity, this._frecency);
  },
};
MixIn(GlodaContact, GlodaHasAttributesMixIn);


/**
 * @class A specific means of communication for a contact.
 */
function GlodaIdentity(aDatastore, aID, aContactID, aContact, aKind, aValue,
                       aDescription, aIsRelay) {
  // _datastore set on the prototype by GlodaDatastore
  this._id = aID;
  this._contactID = aContactID;
  this._contact = aContact;
  this._kind = aKind;
  this._value = aValue;
  this._description = aDescription;
  this._isRelay = aIsRelay;
  /// Cached indication of whether there is an address book card for this
  ///  identity.  We keep this up-to-date via address book listener
  ///  notifications in |GlodaABIndexer|.
  this._hasAddressBookCard = undefined;
}

GlodaIdentity.prototype = {
  NOUN_ID: 104,
  // set by GlodaDatastore
  _datastore: null,
  get id() { return this._id; },
  get contactID() { return this._contactID; },
  get contact() { return this._contact; },
  get kind() { return this._kind; },
  get value() { return this._value; },
  get description() { return this._description; },
  get isRelay() { return this._isRelay; },

  get uniqueValue() {
    return this._kind + "@" + this._value;
  },

  toString: function gloda_identity_toString() {
    return "Identity:" + this._kind + ":" + this._value;
  },

  toLocaleString: function gloda_identity_toLocaleString() {
    if (this.contact.name == this.value)
      return this.value;
    return this.contact.name + " : " + this.value;
  },

  get abCard() {
    // for our purposes, the address book only speaks email
    if (this._kind != "email")
      return false;
    let card = GlodaUtils.getCardForEmail(this._value);
    this._hasAddressBookCard = (card != null);
    return card;
  },

  /**
   * Indicates whether we have an address book card for this identity.  This
   *  value is cached once looked-up and kept up-to-date by |GlodaABIndexer|
   *  and its notifications.
   */
  get inAddressBook() {
    if (this._hasAddressBookCard !== undefined)
      return this._hasAddressBookCard;
    return (this.abCard && true) || false;
  },

  pictureURL: function(aSize) {
    if (this.inAddressBook) {
      // XXX should get the photo if we have it.
    }
    return "";
  }
};


/**
 * An attachment, with as much information as we can gather on it
 */
function GlodaAttachment(aGlodaMessage, aName, aContentType, aSize, aPart, aExternalUrl, aIsExternal) {
  // _datastore set on the prototype by GlodaDatastore
  this._glodaMessage = aGlodaMessage;
  this._name = aName;
  this._contentType = aContentType;
  this._size = aSize;
  this._part = aPart;
  this._externalUrl = aExternalUrl;
  this._isExternal = aIsExternal;
}

GlodaAttachment.prototype = {
  NOUN_ID: 105,
  // set by GlodaDatastore
  get name() { return this._name; },
  set name(aName) { this._name = aName; },
  get contentType() { return this._contentType; },
  get size() { return this._size; },
  set size(aSize) { this._size = aSize; },
  get url() {
    if (this.isExternal)
      return this._externalUrl;
    else {
      let uri = this._glodaMessage.folderMessageURI;
      if (!uri)
        throw new Error("The message doesn't exist anymore, unable to rebuild attachment URL");
      let neckoURL = {};
      let msgService = getMessenger().messageServiceFromURI(uri);
      msgService.GetUrlForUri(uri, neckoURL, null);
      let url = neckoURL.value.spec;
      let hasParamAlready = url.match(/\?[a-z]+=[^\/]+$/);
      let sep = hasParamAlready ? "&" : "?";
      return url+sep+"part="+this._part+"&filename="+encodeURIComponent(this._name);
    }
  },
  get isExternal() { return this._isExternal; },

  toString: function gloda_attachment_toString() {
    return "attachment: " + this._name + ":" + this._contentType;
  },

};
