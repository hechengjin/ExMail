/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This file looks to Myk Melez <myk@mozilla.org>'s Mozilla Labs snowl
 * project's (http://hg.mozilla.org/labs/snowl/) modules/datastore.js
 * for inspiration and idioms (and also a name :).
 */

const EXPORTED_SYMBOLS = ["GlodaDatastore"];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource:///modules/IOUtils.js");
Cu.import("resource://gre/modules/Services.jsm");

Cu.import("resource:///modules/gloda/log4moz.js");

Cu.import("resource:///modules/gloda/datamodel.js");
Cu.import("resource:///modules/gloda/databind.js");
Cu.import("resource:///modules/gloda/collection.js");

const MIN_CACHE_SIZE = 8 * 1048576;
const MAX_CACHE_SIZE = 64 * 1048576;
const MEMSIZE_FALLBACK_BYTES = 256 * 1048576;

let PCH_LOG = Log4Moz.repository.getLogger("gloda.ds.pch");

/**
 * Commit async handler; hands off the notification to
 *  |GlodaDatastore._asyncCompleted|.
 */
function PostCommitHandler(aCallbacks) {
  this.callbacks = aCallbacks;
  GlodaDatastore._pendingAsyncStatements++;
}

PostCommitHandler.prototype = {
  handleResult: function gloda_ds_pch_handleResult(aResultSet) {
  },

  handleError: function gloda_ds_pch_handleError(aError) {
    PCH_LOG.error("database error:" + aError);
  },

  handleCompletion: function gloda_ds_pch_handleCompletion(aReason) {
    // just outright bail if we are shutdown
    if (GlodaDatastore.datastoreIsShutdown)
      return;

    if (aReason == Ci.mozIStorageStatementCallback.REASON_FINISHED) {
      for each (let [iCallback, callback] in Iterator(this.callbacks)) {
        try {
          callback();
        }
        catch (ex) {
          PCH_LOG.error("PostCommitHandler callback (" + ex.fileName + ":" +
               ex.lineNumber + ") threw: " + ex);
        }
      }
    }
    try {
      GlodaDatastore._asyncCompleted();
    }
    catch (e) {
      PCH_LOG.error("Exception in handleCompletion:", e);
    }

  }
};

let QFQ_LOG = Log4Moz.repository.getLogger("gloda.ds.qfq");

/**
 * Singleton collection listener used by |QueryFromQueryCallback| to assist in
 *  the loading of referenced noun instances.  Which is to say, messages have
 *  identities (specific e-mail addresses) associated with them via attributes.
 *  And these identities in turn reference / are referenced by contacts (the
 *  notion of a person).
 *
 * This listener is primarily concerned with fixing up the references in each
 *  noun instance to its referenced instances once they have been loaded.  It
 *  also deals with caching so that our identity invariant is maintained: user
 *  code should only ever see one distinct instance of a thing at a time.
 */
let QueryFromQueryResolver = {
  onItemsAdded: function(aIgnoredItems, aCollection, aFake) {
    let originColl = aCollection.dataStack ? aCollection.dataStack.pop()
                                           : aCollection.data;
    //QFQ_LOG.debug("QFQR: originColl: " + originColl);
    if (aCollection.completionShifter)
      aCollection.completionShifter.push(originColl);
    else
      aCollection.completionShifter = [originColl];

    if (!aFake) {
      originColl.deferredCount--;
      originColl.resolvedCount++;
    }

    // bail if we are still pending on some other load completion
    if (originColl.deferredCount > 0) {
      //QFQ_LOG.debug("QFQR: bailing " + originColl._nounDef.name);
      return;
    }

    let referencesByNounID = originColl.masterCollection.referencesByNounID;
    let inverseReferencesByNounID =
      originColl.masterCollection.inverseReferencesByNounID;

    if (originColl.pendingItems) {
      for (let [, item] in Iterator(originColl.pendingItems)) {
        //QFQ_LOG.debug("QFQR: loading deferred " + item.NOUN_ID + ":" + item.id);
        GlodaDatastore.loadNounDeferredDeps(item, referencesByNounID,
            inverseReferencesByNounID);
      }

      // we need to consider the possibility that we are racing a collection very
      //  much like our own.  as such, this means we need to perform cache
      //  unification as our last step.
      GlodaCollectionManager.cacheLoadUnify(originColl._nounDef.id,
        originColl.pendingItems, false);

      // just directly tell the collection about the items.  we know the query
      //  matches (at least until we introduce predicates that we cannot express
      //  in SQL.)
      //QFQ_LOG.debug(" QFQR: about to trigger listener: " + originColl._listener +
      //    "with collection: " + originColl._nounDef.name);
      originColl._onItemsAdded(originColl.pendingItems);
      delete originColl.pendingItems;
      delete originColl._pendingIdMap;
    }
  },
  onItemsModified: function() {
  },
  onItemsRemoved: function() {
  },
  onQueryCompleted: function(aCollection) {
    let originColl = aCollection.completionShifter ?
      aCollection.completionShifter.shift() : aCollection.data;
    //QFQ_LOG.debug(" QFQR about to trigger completion with collection: " +
    //  originColl._nounDef.name);
    if (originColl.deferredCount <= 0) {
      originColl._onQueryCompleted();
    }
  },
};

/**
 * Handles the results from a GlodaDatastore.queryFromQuery call in cooperation
 *  with the |QueryFromQueryResolver| collection listener.  We do a lot of
 *  legwork related to satisfying references to other noun instances on the
 *  noun instances the user directy queried.  Messages reference identities
 *  reference contacts which in turn (implicitly) reference identities again.
 *  We have to spin up those other queries and stitch things together.
 *
 * While the code is generally up to the existing set of tasks it is called to
 *  handle, I would not be surprised for it to fall down if things get more
 *  complex.  Some of the logic here 'evolved' a bit and could benefit from
 *  additional documentation and a fresh go-through.
 */
function QueryFromQueryCallback(aStatement, aNounDef, aCollection) {
  this.statement = aStatement;
  this.nounDef = aNounDef;
  this.collection = aCollection;

  //QFQ_LOG.debug("Creating QFQCallback for noun: " + aNounDef.name);

  // the master collection holds the referencesByNounID
  this.referencesByNounID = {};
  this.masterReferencesByNounID =
    this.collection.masterCollection.referencesByNounID;
  this.inverseReferencesByNounID = {};
  this.masterInverseReferencesByNounID =
    this.collection.masterCollection.inverseReferencesByNounID;
  // we need to contribute our references as we load things; we need this
  //  because of the potential for circular dependencies and our inability to
  //  put things into the caching layer (or collection's _idMap) until we have
  //  fully resolved things.
  if (this.nounDef.id in this.masterReferencesByNounID)
    this.selfReferences = this.masterReferencesByNounID[this.nounDef.id];
  else
    this.selfReferences = this.masterReferencesByNounID[this.nounDef.id] = {};
  if (this.nounDef.parentColumnAttr) {
    if (this.nounDef.id in this.masterInverseReferencesByNounID)
      this.selfInverseReferences =
        this.masterInverseReferencesByNounID[this.nounDef.id];
    else
      this.selfInverseReferences =
        this.masterInverseReferencesByNounID[this.nounDef.id] = {};
  }

  this.needsLoads = false;

  GlodaDatastore._pendingAsyncStatements++;
}

QueryFromQueryCallback.prototype = {
  handleResult: function gloda_ds_qfq_handleResult(aResultSet) {
    try {
      // just outright bail if we are shutdown
      if (GlodaDatastore.datastoreIsShutdown)
        return;

      let pendingItems = this.collection.pendingItems;
      let pendingIdMap = this.collection._pendingIdMap;
      let row;
      let nounDef = this.nounDef;
      let nounID = nounDef.id;
      while ((row = aResultSet.getNextRow())) {
        let item = nounDef.objFromRow.call(nounDef.datastore, row);
        if (this.collection.stashedColumns) {
          let stashed = this.collection.stashedColumns[item.id] = [];
          for (let [,iCol] in
               Iterator(this.collection.query.options.stashColumns)) {
            stashed.push(GlodaDatastore._getVariant(row, iCol));
          }
        }
        // try and replace the item with one from the cache, if we can
        let cachedItem = GlodaCollectionManager.cacheLookupOne(nounID, item.id,
                                                               false);

        // if we already have a copy in the pending id map, skip it
        if (item.id in pendingIdMap)
          continue;

        //QFQ_LOG.debug("loading item " + nounDef.id + ":" + item.id + " existing: " +
        //    this.selfReferences[item.id] + " cached: " + cachedItem);
        if (cachedItem)
          item = cachedItem;
        // we may already have been loaded by this process
        else if (this.selfReferences[item.id] != null)
          item = this.selfReferences[item.id];
        // perform loading logic which may produce reference dependencies
        else
          this.needsLoads =
            GlodaDatastore.loadNounItem(item, this.referencesByNounID,
                                        this.inverseReferencesByNounID) ||
            this.needsLoads;

        // add ourself to the references by our id
        // QFQ_LOG.debug("saving item " + nounDef.id + ":" + item.id + " to self-refs");
        this.selfReferences[item.id] = item;

        // if we're tracking it, add ourselves to our parent's list of children
        //  too
        if (this.selfInverseReferences) {
          let parentID = item[nounDef.parentColumnAttr.idStorageAttributeName];
          let childrenList = this.selfInverseReferences[parentID];
          if (childrenList === undefined)
            childrenList = this.selfInverseReferences[parentID] = [];
          childrenList.push(item);
        }

        pendingItems.push(item);
        pendingIdMap[item.id] = item;
      }
    }
    catch (e) {
      GlodaDatastore._log.error("Exception in handleResult:", e);
    }
  },

  handleError: function gloda_ds_qfq_handleError(aError) {
    GlodaDatastore._log.error("Async queryFromQuery error: " +
      aError.result + ": " + aError.message);
  },

  handleCompletion: function gloda_ds_qfq_handleCompletion(aReason) {
    try {
      try {
        this.statement.finalize();
        this.statement = null;

        // just outright bail if we are shutdown
        if (GlodaDatastore.datastoreIsShutdown)
          return;

        //QFQ_LOG.debug("handleCompletion: " + this.collection._nounDef.name);

        if (this.needsLoads) {
          for each (let [nounID, references] in Iterator(this.referencesByNounID)) {
            if (nounID == this.nounDef.id)
              continue;
            let nounDef = GlodaDatastore._nounIDToDef[nounID];
            //QFQ_LOG.debug("  have references for noun: " + nounDef.name);
            // try and load them out of the cache/existing collections.  items in the
            //  cache will be fully formed, which is nice for us.
            // XXX this mechanism will get dubious when we have multiple paths to a
            //  single noun-type.  For example, a -> b -> c, a-> c; two paths to c
            //  and we're looking at issuing two requests to c, the latter of which
            //  will be a superset of the first one.  This does not currently pose
            //  a problem because we only have a -> b -> c -> b, and sequential
            //  processing means no alarms and no surprises.
            let masterReferences = this.masterReferencesByNounID[nounID];
            if (masterReferences === undefined)
              masterReferences = this.masterReferencesByNounID[nounID] = {};
            let outReferences;
            if (nounDef.parentColumnAttr)
              outReferences = {};
            else
              outReferences = masterReferences;
            let [foundCount, notFoundCount, notFound] =
              GlodaCollectionManager.cacheLookupMany(nounDef.id, references,
                  outReferences);

            if (nounDef.parentColumnAttr) {
              let inverseReferences;
              if (nounDef.id in this.masterInverseReferencesByNounID)
                inverseReferences =
                  this.masterInverseReferencesByNounID[nounDef.id];
              else
                inverseReferences =
                  this.masterInverseReferencesByNounID[nounDef.id] = {};

              for each (let item in outReferences) {
                masterReferences[item.id] = item;
                let parentID = item[nounDef.parentColumnAttr.idStorageAttributeName];
                let childrenList = inverseReferences[parentID];
                if (childrenList === undefined)
                  childrenList = inverseReferences[parentID] = [];
                childrenList.push(item);
              }
            }

            //QFQ_LOG.debug("  found: " + foundCount + " not found: " + notFoundCount);
            if (notFoundCount === 0) {
              this.collection.resolvedCount++;
            }
            else {
              this.collection.deferredCount++;
              let query = new nounDef.queryClass();
              query.id.apply(query, [id for (id in notFound)]);

              this.collection.masterCollection.subCollections[nounDef.id] =
                GlodaDatastore.queryFromQuery(query, QueryFromQueryResolver,
                  this.collection,
                  // we fully expect/allow for there being no such subcollection yet.
                  this.collection.masterCollection.subCollections[nounDef.id],
                  this.collection.masterCollection,
                  {becomeExplicit: true});
            }
          }

          for each (let [nounID, inverseReferences] in
              Iterator(this.inverseReferencesByNounID)) {
            this.collection.deferredCount++;
            let nounDef = GlodaDatastore._nounIDToDef[nounID];

            //QFQ_LOG.debug("Want to load inverse via " + nounDef.parentColumnAttr.boundName);

            let query = new nounDef.queryClass();
            // we want to constrain using the parent column
            let queryConstrainer = query[nounDef.parentColumnAttr.boundName];
            queryConstrainer.apply(query, [pid for (pid in inverseReferences)]);
            this.collection.masterCollection.subCollections[nounDef.id] =
              GlodaDatastore.queryFromQuery(query, QueryFromQueryResolver,
                this.collection,
                // we fully expect/allow for there being no such subcollection yet.
                this.collection.masterCollection.subCollections[nounDef.id],
                this.collection.masterCollection,
                {becomeExplicit: true});
          }
        }
        else {
          this.collection.deferredCount--;
          this.collection.resolvedCount++;
        }

        //QFQ_LOG.debug("  defer: " + this.collection.deferredCount +
        //              " resolved: " + this.collection.resolvedCount);

        // process immediately and kick-up to the master collection...
        if (this.collection.deferredCount <= 0) {
          // this guy will resolve everyone using referencesByNounID and issue the
          //  call to this.collection._onItemsAdded to propagate things to the
          //  next concerned subCollection or the actual listener if this is the
          //  master collection.  (Also, call _onQueryCompleted).
          QueryFromQueryResolver.onItemsAdded(null, {data: this.collection}, true);
          QueryFromQueryResolver.onQueryCompleted({data: this.collection});
        }
      }
      catch (e) {
        Components.utils.reportError(e);
        QFQ_LOG.error("Exception:", e);
      }
    }
    finally {
      GlodaDatastore._asyncCompleted();
    }
  }
};

/**
 * Used by |GlodaDatastore.folderCompactionPassBlockFetch| to accumulate the
 *  results and pass them back in to the compaction process in
 *  |GlodaMsgIndexer._worker_folderCompactionPass|.
 */
function CompactionBlockFetcherHandler(aCallback) {
  this.callback = aCallback;
  this.idsAndMessageKeys = [];
  GlodaDatastore._pendingAsyncStatements++;
}
CompactionBlockFetcherHandler.prototype = {
  handleResult: function gloda_ds_cbfh_handleResult(aResultSet) {
    let row;
    while ((row = aResultSet.getNextRow())) {
      this.idsAndMessageKeys.push([
        row.getInt64(0), // id
        row.getInt64(1), // messageKey
        row.getString(2), // headerMessageID
        ]);
    }
  },
  handleError: function gloda_ds_cbfh_handleError(aError) {
    GlodaDatastore._log.error("CompactionBlockFetcherHandler error: " +
      aError.result + ": " + aError.message);
  },
  handleCompletion: function gloda_ds_cbfh_handleCompletion(aReason) {
    GlodaDatastore._asyncCompleted();
    this.callback(this.idsAndMessageKeys);
  }
};

/**
 * Use this as the callback handler when you have a SQL query that returns a
 *  single row with a single integer column value, like a COUNT() query.
 */
function SingletonResultValueHandler(aCallback) {
  this.callback = aCallback;
  this.result = null;
  GlodaDatastore._pendingAsyncStatements++;
}
SingletonResultValueHandler.prototype = {
  handleResult: function gloda_ds_cbfh_handleResult(aResultSet) {
    let row;
    while ((row = aResultSet.getNextRow())) {
      this.result = row.getInt64(0);
    }
  },
  handleError: function gloda_ds_cbfh_handleError(aError) {
    GlodaDatastore._log.error("SingletonResultValueHandler error: " +
      aError.result + ": " + aError.message);
  },
  handleCompletion: function gloda_ds_cbfh_handleCompletion(aReason) {
    GlodaDatastore._asyncCompleted();
    this.callback(this.result);
  }
};

/**
 * Wrapper that duplicates actions taken on a real statement to an explain
 *  statement.  Currently only fires an explain statement once.
 */
function ExplainedStatementWrapper(aRealStatement, aExplainStatement,
                                   aSQLString, aExplainHandler) {
  this.real = aRealStatement;
  this.explain = aExplainStatement;
  this.sqlString = aSQLString;
  this.explainHandler = aExplainHandler;
  this.done = false;
}
ExplainedStatementWrapper.prototype = {
  bindNullParameter: function(aColIndex) {
    this.real.bindNullParameter(aColIndex);
    if (!this.done)
      this.explain.bindNullParameter(aColIndex);
  },
  bindStringParameter: function(aColIndex, aValue) {
    this.real.bindStringParameter(aColIndex, aValue);
    if (!this.done)
      this.explain.bindStringParameter(aColIndex, aValue);
  },
  bindInt64Parameter: function(aColIndex, aValue) {
    this.real.bindInt64Parameter(aColIndex, aValue);
    if (!this.done)
      this.explain.bindInt64Parameter(aColIndex, aValue);
  },
  bindDoubleParameter: function(aColIndex, aValue) {
    this.real.bindDoubleParameter(aColIndex, aValue);
    if (!this.done)
      this.explain.bindDoubleParameter(aColIndex, aValue);
  },
  executeAsync: function wrapped_executeAsync(aCallback) {
    if (!this.done) {
      this.explainHandler.sqlEnRoute(this.sqlString);
      this.explain.executeAsync(this.explainHandler);
      this.explain.finalize();
      this.done = true;
    }
    return this.real.executeAsync(aCallback);
  },
  finalize: function wrapped_finalize() {
    if (!this.done)
      this.explain.finalize();
    this.real.finalize();
  },
};

/**
 * Writes a single JSON document to the provide file path in a streaming
 *  fashion.  At startup we open an array to place the queries in and at
 *  shutdown we close it.
 */
function ExplainedStatementProcessor(aDumpPath) {
  let observerService = Cc["@mozilla.org/observer-service;1"]
                          .getService(Ci.nsIObserverService);
  observerService.addObserver(this, "quit-application", false);

  this._sqlStack = [];
  this._curOps = [];
  this._objsWritten = 0;

  let filePath = Cc["@mozilla.org/file/local;1"]
                   .createInstance(Ci.nsILocalFile);
  filePath.initWithPath(aDumpPath);

  this._ostream = Cc["@mozilla.org/network/file-output-stream;1"]
                 .createInstance(Ci.nsIFileOutputStream);
  this._ostream.init(filePath, -1, -1, 0);

  let s = '{"queries": [';
  this._ostream.write(s, s.length);
}
ExplainedStatementProcessor.prototype = {
  sqlEnRoute: function esp_sqlEnRoute(aSQLString) {
    this._sqlStack.push(aSQLString);
  },
  handleResult: function esp_handleResult(aResultSet) {
    let row;
    // addr  opcode (s)      p1    p2    p3    p4 (s)   p5   comment (s)
    while ((row = aResultSet.getNextRow())) {
      this._curOps.push([
        row.getInt64(0),  // addr
        row.getString(1), // opcode
        row.getInt64(2),  // p1
        row.getInt64(3),  // p2
        row.getInt64(4),  // p3
        row.getString(5), // p4
        row.getString(6), // p5
        row.getString(7)  // comment
      ]);
    }
  },
  handleError: function esp_handleError(aError) {
    Cu.reportError("Unexpected error in EXPLAIN handler: " + aError);
  },
  handleCompletion: function esp_handleCompletion(aReason) {
    let obj = {
      sql: this._sqlStack.shift(),
      operations: this._curOps,
    };
    let s = (this._objsWritten++ ? ", " : "") + JSON.stringify(obj, null, 2);
    this._ostream.write(s, s.length);

    this._curOps = [];
  },

  observe: function esp_observe(aSubject, aTopic, aData) {
    if (aTopic == "quit-application")
      this.shutdown();
  },

  shutdown: function esp_shutdown() {
    let s = "]}";
    this._ostream.write(s, s.length);
    this._ostream.close();

    let observerService = Cc["@mozilla.org/observer-service;1"]
                            .getService(Ci.nsIObserverService);
    observerService.removeObserver(this, "quit-application");
  }
};

// See the documentation on GlodaDatastore._schemaVersion to understand these:
const DB_SCHEMA_ACCEPT_LEAVE_LOW = 31,
      DB_SCHEMA_ACCEPT_LEAVE_HIGH = 34,
      DB_SCHEMA_ACCEPT_DOWNGRADE_LOW = 35,
      DB_SCHEMA_ACCEPT_DOWNGRADE_HIGH = 39,
      DB_SCHEMA_DOWNGRADE_DELTA = 5;

/**
 * Database abstraction layer.  Contains explicit SQL schemas for our
 *  fundamental representations (core 'nouns', if you will) as well as
 *  specialized functions for then dealing with each type of object.  At the
 *  same time, we are beginning to support extension-provided tables, which
 *  call into question whether we really need our hand-rolled code, or could
 *  simply improve the extension-provided table case to work for most of our
 *  hand-rolled cases.
 * For now, the argument can probably be made that our explicit schemas and code
 *  is readable/intuitive (not magic) and efficient (although generic stuff
 *  could also be made efficient, if slightly evil through use of eval or some
 *  other code generation mechanism.)
 *
 * === Data Model Interaction / Dependencies
 *
 * Dependent on and assumes limited knowledge of the datamodel.js
 *  implementations.  datamodel.js actually has an implicit dependency on
 *  our implementation, reaching back into the datastore via the _datastore
 *  attribute which we pass into every instance we create.
 * We pass a reference to ourself as we create the datamodel.js instances (and
 *  they store it as _datastore) because of a half-implemented attempt to make
 *  it possible to live in a world where we have multiple datastores.  This
 *  would be desirable in the cases where we are dealing with multiple SQLite
 *  databases.  This could be because of per-account global databases or
 *  some other segmentation.  This was abandoned when the importance of
 *  per-account databases was diminished following public discussion, at least
 *  for the short-term, but no attempted was made to excise the feature or
 *  preclude it.  (Merely a recognition that it's too much to try and implement
 *  correct right now, especially because our solution might just be another
 *  (aggregating) layer on top of things, rather than complicating the lower
 *  levels.)
 *
 * === Object Identity / Caching
 *
 * The issue of object identity is handled by integration with the collection.js
 *  provided GlodaCollectionManager.  By "Object Identity", I mean that we only
 *  should ever have one object instance alive at a time that corresponds to
 *  an underlying database row in the database.  Where possible we avoid
 *  performing database look-ups when we can check if the object is already
 *  present in memory; in practice, this means when we are asking for an object
 *  by ID.  When we cannot avoid a database query, we attempt to make sure that
 *  we do not return a duplicate object instance, instead replacing it with the
 *  'live' copy of the object.  (Ideally, we would avoid any redundant
 *  construction costs, but that is not currently the case.)
 * Although you should consult the GlodaCollectionManager for details, the
 *  general idea is that we have 'collections' which represent views of the
 *  database (based on a query) which use a single mechanism for double duty.
 *  The collections are registered with the collection manager via weak
 *  reference.  The first 'duty' is that since the collections may be desired
 *  to be 'live views' of the data, we want them to update as changes occur.
 *  The weak reference allows the collection manager to track the 'live'
 *  collections and update them.  The second 'duty' is the caching/object
 *  identity duty.  In theory, every live item should be referenced by at least
 *  one collection, making it reachable for object identity/caching purposes.
 * There is also an explicit (inclusive) caching layer present to both try and
 *  avoid poor performance from some of the costs of this strategy, as well as
 *  to try and keep track of objects that are being worked with that are not
 *  (yet) tracked by a collection.  Using a size-bounded cache is clearly not
 *  a guarantee of correctness for this, but is suspected will work quite well.
 *  (Well enough to be dangerous because the inevitable failure case will not be
 *  expected.)
 *
 * The current strategy may not be the optimal one, feel free to propose and/or
 *  implement better ones, especially if you have numbers.
 * The current strategy is not fully implemented in this file, but the common
 *  cases are believed to be covered.  (Namely, we fail to purge items from the
 *  cache as they are purged from the database.)
 *
 * === Things That May Not Be Obvious (Gotchas)
 *
 * Although the schema includes "triggers", they are currently not used
 *  and were added when thinking about implementing the feature.  We will
 *  probably implement this feature at some point, which is why they are still
 *  in there.
 *
 * We, and the layers above us, are not sufficiently thorough at cleaning out
 *  data from the database, and may potentially orphan it _as new functionality
 *  is added in the future at layers above us_.  That is, currently we should
 *  not be leaking database rows, but we may in the future.  This is because
 *  we/the layers above us lack a mechanism to track dependencies based on
 *  attributes.  Say a plugin exists that extracts recipes from messages and
 *  relates them via an attribute.  To do so, it must create new recipe rows
 *  in its own table as new recipes are discovered.  No automatic mechanism
 *  will purge recipes as their source messages are purged, nor does any
 *  event-driven mechanism explicitly inform the plugin.  (It could infer
 *  such an event from the indexing/attribute-providing process, or poll the
 *  states of attributes to accomplish this, but that is not desirable.)  This
 *  needs to be addressed, and may be best addressed at layers above
 *  datastore.js.
 * @namespace
 */
var GlodaDatastore = {
  _log: null,

  /* see Gloda's documentation for these constants */
  kSpecialNotAtAll: 0,
  kSpecialColumn: 16,
  kSpecialColumnChildren: 16|1,
  kSpecialColumnParent: 16|2,
  kSpecialString: 32,
  kSpecialFulltext: 64,
  IGNORE_FACET: {},

  kConstraintIdIn: 0,
  kConstraintIn: 1,
  kConstraintRanges: 2,
  kConstraintEquals: 3,
  kConstraintStringLike: 4,
  kConstraintFulltext: 5,

  /* ******************* SCHEMA ******************* */

  /**
   * Schema version policy. IMPORTANT!  We expect the following potential things
   *  to happen in the life of gloda that can impact our schema and the ability
   *  to move between different versions of Thunderbird:
   *
   * - Fundamental changes to the schema so that two versions of Thunderbird
   *    cannot use the same global database.  To wit, Thunderbird N+1 needs to
   *    blow away the database of Thunderbird N and reindex from scratch.
   *    Likewise, Thunderbird N will need to blow away Thunderbird N+1's
   *    database because it can't understand it.  And we can't simply use a
   *    different file because there would be fatal bookkeeping losses.
   *
   * - Bidirectional minor schema changes (rare).
   *    Thunderbird N+1 does something that does not affect Thunderbird N's use
   *    of the database, and a user switching back to Thunderbird N will not be
   *    negatively impacted.  It will also be fine when they go back to N+1 and
   *    N+1 will not be missing any vital data.  The historic example of this is
   *    when we added a missing index that was important for performance.  In
   *    that case, Thunderbird N could have potentially left the schema revision
   *    intact (if there was a safe revision), rather than swapping it on the
   *    downgrade, compelling N+1 to redo the transform on upgrade.
   *
   * - Backwards compatible, upgrade-transition minor schema changes.
   *    Thunderbird N+1 does something that does not require nuking the
   *    database / a full re-index, but does require processing on upgrade from
   *    a version of the database previously used by Thunderbird.  These changes
   *    do not impact N's ability to use the database.  For example, adding a
   *    new indexed attribute that affects a small number of messages could be
   *    handled by issuing a query on upgrade to dirty/index those messages.
   *    However, if the user goes back to N from N+1, when they upgrade to N+1
   *    again, we need to re-index.  In this case N would need to have downgrade
   *    the schema revision.
   *
   * - Backwards incompatible, minor schema changes.
   *    Thunderbird N+1 does something that does not require nuking the database
   *    but will break Thunderbird N's ability to use the database.
   *
   * - Regression fixes.  Sometimes we may land something that screws up
   *    databases, or the platform changes in a way that breaks our code and we
   *    had insufficient unit test coverage and so don't detect it until some
   *    databases have gotten messed up.
   *
   * Accordingly, every version of Thunderbird has a concept of potential schema
   *  versions with associated semantics to prepare for the minor schema upgrade
   *  cases were inter-op is possible.  These ranges and their semantics are:
   * - accepts and leaves intact.  Covers:
   *    - regression fixes that no longer exist with the landing of the upgrade
   *       code as long as users never go back a build in the given channel.
   *    - bidirectional minor schema changes.
   * - accepts but downgrades version to self.  Covers:
   *    - backwards compatible, upgrade-transition minor schema changes.
   * - nuke range (anything beyond a specific revision needs to be nuked):
   *    - backwards incompatible, minor scheme changes
   *    - fundamental changes
   *
   *
   * SO, YOU WANT TO CHANGE THE SCHEMA?
   *
   * Use the ranges below for Thunderbird 11 as a guide, bumping things as little
   *  as possible.  If we start to use up the "accepts and leaves intact" range
   *  without majorly changing things up, re-do the numbering acceptance range
   *  to give us additional runway.
   *
   * Also, if we keep needing non-nuking upgrades, consider adding an additional
   *  table to the database that can tell older versions of Thunderbird what to
   *  do when confronted with a newer database and where it can set flags to tell
   *  the newer Thunderbird what the older Thunderbird got up to.  For example,
   *  it would be much easier if we just tell Thunderbird N what to do when it's
   *  confronted with the database.
   *
   *
   * CURRENT STATE OF THE MIGRATION LOGIC:
   *
   * Thunderbird 11: uses 30 (regression fix from 26)
   * - accepts and leaves intact: 31-34
   * - accepts and downgrades by 5: 35-39
   * - nukes: 40+
   */
  _schemaVersion: 30,
  // what is the schema in the database right now?
  _actualSchemaVersion: 0,
  _schema: {
    tables: {

      // ----- Messages
      folderLocations: {
        columns: [
          ["id", "INTEGER PRIMARY KEY"],
          ["folderURI", "TEXT NOT NULL"],
          ["dirtyStatus", "INTEGER NOT NULL"],
          ["name", "TEXT NOT NULL"],
          ["indexingPriority", "INTEGER NOT NULL"],
        ],

        triggers: {
          delete: "DELETE from messages WHERE folderID = OLD.id",
        },
      },

      conversations: {
        columns: [
          ["id", "INTEGER PRIMARY KEY"],
          ["subject", "TEXT NOT NULL"],
          ["oldestMessageDate", "INTEGER"],
          ["newestMessageDate", "INTEGER"],
        ],

        indices: {
          subject: ['subject'],
          oldestMessageDate: ['oldestMessageDate'],
          newestMessageDate: ['newestMessageDate'],
        },

        fulltextColumns: [
          ["subject", "TEXT"],
        ],

        triggers: {
          delete: "DELETE from messages WHERE conversationID = OLD.id",
        },
      },

      /**
       * A message record correspond to an actual message stored in a folder
       *  somewhere, or is a ghost record indicating a message that we know
       *  should exist, but which we have not seen (and which we may never see).
       *  We represent these ghost messages by storing NULL values in the
       *  folderID and messageKey fields; this may need to change to other
       *  sentinel values if this somehow impacts performance.
       */
      messages: {
        columns: [
          ["id", "INTEGER PRIMARY KEY"],
          ["folderID", "INTEGER"],
          ["messageKey", "INTEGER"],
          // conversationID used to have a REFERENCES but I'm losing it for
          //  presumed performance reasons and it doesn't do anything for us.
          ["conversationID", "INTEGER NOT NULL"],
          ["date", "INTEGER"],
          // we used to have the parentID, but because of the very real
          //  possibility of multiple copies of a message with a given
          //  message-id, the parentID concept is unreliable.
          ["headerMessageID", "TEXT"],
          ["deleted", "INTEGER NOT NULL default 0"],
          ["jsonAttributes", "TEXT"],
          // Notability attempts to capture the static 'interestingness' of a
          //  message as a result of being starred/flagged, labeled, read
          //  multiple times, authored by someone in your address book or that
          //  you converse with a lot, etc.
          ["notability", "INTEGER NOT NULL default 0"],
        ],

        indices: {
          messageLocation: ['folderID', 'messageKey'],
          headerMessageID: ['headerMessageID'],
          conversationID: ['conversationID'],
          date: ['date'],
          deleted: ['deleted'],
        },

        // note: if reordering the columns, you need to change this file's
        //  row-loading logic, msg_search.js's ranking usages and also the
        //  column saturations in nsGlodaRankerFunction
        fulltextColumns: [
          ["body", "TEXT"],
          ["subject", "TEXT"],
          ["attachmentNames", "TEXT"],
          ["author", "TEXT"],
          ["recipients", "TEXT"],
        ],

        triggers: {
          delete: "DELETE FROM messageAttributes WHERE messageID = OLD.id",
        },
      },

      // ----- Attributes
      attributeDefinitions: {
        columns: [
          ["id", "INTEGER PRIMARY KEY"],
          ["attributeType", "INTEGER NOT NULL"],
          ["extensionName", "TEXT NOT NULL"],
          ["name", "TEXT NOT NULL"],
          ["parameter", "BLOB"],
        ],

        triggers: {
          delete: "DELETE FROM messageAttributes WHERE attributeID = OLD.id",
        },
      },

      messageAttributes: {
        columns: [
          // conversationID and messageID used to have REFERENCES back to their
          //  appropriate types.  I removed it when removing attributeID for
          //  better reasons and because the code is not capable of violating
          //  this constraint, so the check is just added cost.  (And we have
          //  unit tests that sanity check my assertions.)
          ["conversationID", "INTEGER NOT NULL"],
          ["messageID", "INTEGER NOT NULL"],
          // This used to be REFERENCES attributeDefinitions(id) but then we
          //  introduced sentinel values and it's hard to justify the effort
          //  to compel injection of the record or the overhead to do the
          //  references checking.
          ["attributeID", "INTEGER NOT NULL"],
          ["value", "NUMERIC"],
        ],

        indices: {
          attribQuery: [
            "attributeID", "value",
            /* covering: */ "conversationID", "messageID"],
          // This is required for deletion of a message's attributes to be
          // performant.  We could optimize this index away if we changed our
          // deletion logic to issue specific attribute deletions based on the
          // information it already has available in the message's JSON blob.
          // The rub there is that if we screwed up we could end up leaking
          // attributes and there is a non-trivial performance overhead to
          // the many requests it would cause (which can also be reduced in
          // the future by changing our SQL dispatch code.)
          messageAttribFastDeletion: [
            "messageID"],
        },
      },

      // ----- Contacts / Identities

      /**
       * Corresponds to a human being and roughly to an address book entry.
       *  Constrast with an identity, which is a specific e-mail address, IRC
       *  nick, etc.  Identities belong to contacts, and this relationship is
       *  expressed on the identityAttributes table.
       */
      contacts: {
        columns: [
          ["id", "INTEGER PRIMARY KEY"],
          ["directoryUUID", "TEXT"],
          ["contactUUID", "TEXT"],
          ["popularity", "INTEGER"],
          ["frecency", "INTEGER"],
          ["name", "TEXT"],
          ["jsonAttributes", "TEXT"],
        ],
        indices: {
          popularity: ["popularity"],
          frecency: ["frecency"],
        },
      },

      contactAttributes: {
        columns: [
          ["contactID", "INTEGER NOT NULL"],
          ["attributeID",
           "INTEGER NOT NULL"],
          ["value", "NUMERIC"]
        ],
        indices: {
          contactAttribQuery: [
            "attributeID", "value",
            /* covering: */ "contactID"],
        }
      },

      /**
       * Identities correspond to specific e-mail addresses, IRC nicks, etc.
       */
      identities: {
        columns: [
          ["id", "INTEGER PRIMARY KEY"],
          ["contactID", "INTEGER NOT NULL"],
          ["kind", "TEXT NOT NULL"], // ex: email, irc, etc.
          ["value", "TEXT NOT NULL"], // ex: e-mail address, irc nick/handle...
          ["description", "NOT NULL"], // what makes this identity different
          // from the others? (ex: home, work, etc.)
          ["relay", "INTEGER NOT NULL"], // is the identity just a relay
          // mechanism? (ex: mailing list, twitter 'bouncer', IRC gateway, etc.)
        ],

        indices: {
          contactQuery: ["contactID"],
          valueQuery: ["kind", "value"]
        }
      },
    },
  },


  /* ******************* LOGIC ******************* */
  /**
   * We only have one connection; this name exists for legacy reasons but helps
   *  track when we are intentionally doing synchronous things during startup.
   *  We do nothing synchronous once our setup has completed.
   */
  syncConnection: null,
  /**
   * We only have one connection and we only do asynchronous things after setup;
   *  this name still exists mainly for legacy reasons.
   */
  asyncConnection: null,

  /**
   * Our "mailnews.database.global.datastore." preferences branch for debug
   * notification handling.  We register as an observer against this.
   */
  _prefBranch: null,

  /**
   * The unique ID assigned to an index when it has been built. This value
   * changes once the index has been rebuilt.
   */
  _datastoreID: null,

  /**
   * Initialize logging, create the database if it doesn't exist, "upgrade" it
   *  if it does and it's not up-to-date, fill our authoritative folder uri/id
   *  mapping.
   */
  _init: function gloda_ds_init(aNounIDToDef) {
    this._log = Log4Moz.repository.getLogger("gloda.datastore");
    this._log.debug("Beginning datastore initialization.");

    this._nounIDToDef = aNounIDToDef;

    let prefService = Cc["@mozilla.org/preferences-service;1"].
                        getService(Ci.nsIPrefService);
    let branch = prefService.getBranch("mailnews.database.global.datastore.");
    this._prefBranch = branch;

    // Not sure the weak reference really makes a difference given that we are a
    // GC root.
    branch.addObserver("", this, false);
    // claim the pref changed so we can centralize our logic there.
    this.observe(null, "nsPref:changed", "explainToPath");

    // Get the path to our global database
    var dirService = Cc["@mozilla.org/file/directory_service;1"].
                     getService(Ci.nsIProperties);
    var dbFile = dirService.get("ProfD", Ci.nsIFile);
    dbFile.append("global-messages-db.sqlite");

    // Get the storage (sqlite) service
    var dbService = Cc["@mozilla.org/storage/service;1"].
                    getService(Ci.mozIStorageService);

    var dbConnection;

    // Report about the size of the database through telemetry (if there's a
    // database, naturally).
    if (dbFile.exists()) {
      try {
        let h = Services.telemetry.getHistogramById("THUNDERBIRD_GLODA_SIZE_MB");
        h.add(dbFile.fileSize/1048576);
      } catch (e) {
        this._log.warn("Couldn't report telemetry", e);
      }
    }

    // Create the file if it does not exist
    if (!dbFile.exists()) {
      this._log.debug("Creating database because it doesn't exist.");
      dbConnection = this._createDB(dbService, dbFile);
    }
    // It does exist, but we (someday) might need to upgrade the schema
    else {
      // (Exceptions may be thrown if the database is corrupt)
      try {
        dbConnection = dbService.openUnsharedDatabase(dbFile);
        let cacheSize = this._determineCachePages(dbConnection);
        // see _createDB...
        dbConnection.executeSimpleSQL("PRAGMA cache_size = "+cacheSize);
        dbConnection.executeSimpleSQL("PRAGMA synchronous = FULL");

        // Register custom tokenizer to index all language text
        var tokenizer = Cc["@mozilla.org/messenger/fts3tokenizer;1"].
                          getService(Ci.nsIFts3Tokenizer);
        tokenizer.registerTokenizer(dbConnection);

        // -- database schema changes
        let dbSchemaVersion = this._actualSchemaVersion =
          dbConnection.schemaVersion;
        // - database from the future!
        if (dbSchemaVersion > this._schemaVersion) {
          if (dbSchemaVersion >= DB_SCHEMA_ACCEPT_LEAVE_LOW &&
              dbSchemaVersion <= DB_SCHEMA_ACCEPT_LEAVE_HIGH) {
            this._log.debug("db from the future in acceptable range; leaving " +
                            "version at: " + dbSchemaVersion);
          }
          else if (dbSchemaVersion >= DB_SCHEMA_ACCEPT_DOWNGRADE_LOW &&
                   dbSchemaVersion <= DB_SCHEMA_ACCEPT_DOWNGRADE_HIGH) {
            let newVersion = dbSchemaVersion - DB_SCHEMA_DOWNGRADE_DELTA;
            this._log.debug("db from the future in downgrade range; setting " +
                            "version to " + newVersion + " down from " +
                            dbSchemaVersion);
            dbConnection.schemaVersion = this._actualSchemaVersion = newVersion;
          }
          // too far from the future, nuke it.
          else {
            dbConnection = this._nukeMigration(dbService, dbFile,
                                               dbConnection);
          }
        }
        // - database from the past!  migrate it, possibly.
        else if (dbSchemaVersion < this._schemaVersion) {
          this._log.debug("Need to migrate database.  (DB version: " +
            this._actualSchemaVersion + " desired version: " +
            this._schemaVersion);
          dbConnection = this._migrate(dbService, dbFile,
                                       dbConnection,
                                       this._actualSchemaVersion,
                                       this._schemaVersion);
          this._log.debug("Migration call completed.");
        }
        // else: this database is juuust right.

        // If we never had a datastore ID, make sure to create one now.
        if (!this._prefBranch.prefHasUserValue("id")) {
          this._datastoreID = this._generateDatastoreID();
          this._prefBranch.setCharPref("id", this._datastoreID);
        } else {
          this._datastoreID = this._prefBranch.getCharPref("id");
        }
      }
      // Handle corrupt databases, other oddities
      catch (ex) {
        if (ex.result == Cr.NS_ERROR_FILE_CORRUPTED) {
          this._log.warn("Database was corrupt, removing the old one.");
          dbFile.remove(false);
          this._log.warn("Removed old database, creating a new one.");
          dbConnection = this._createDB(dbService, dbFile);
        }
        else {
          this._log.error("Unexpected error when trying to open the database:",
                          ex);
          throw ex;
        }
      }
    }

    this.syncConnection = dbConnection;
    this.asyncConnection = dbConnection;

    this._log.debug("Initializing folder mappings.");
    this._getAllFolderMappings();
    // we need to figure out the next id's for all of the tables where we
    //  manage that.
    this._log.debug("Populating managed id counters.");
    this._populateAttributeDefManagedId();
    this._populateConversationManagedId();
    this._populateMessageManagedId();
    this._populateContactManagedId();
    this._populateIdentityManagedId();

    // create the timer we use to periodically drop our references to folders
    //  we no longer need XPCOM references to (or more significantly, their
    //  message databases.)
    this._folderCleanupTimer =
      Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);

    this._log.debug("Completed datastore initialization.");
  },

  observe: function gloda_ds_observe(aSubject, aTopic, aData) {
    if(aTopic != "nsPref:changed")
      return;

    if (aData == "explainToPath") {
      let explainToPath = null;
      try {
        explainToPath = this._prefBranch.getCharPref("explainToPath");
        if (explainToPath.trim() == "")
          explainToPath = null;
      }
      catch (ex) {
        // don't care if the pref is not there.
      }

      // It is conceivable that the name is changing and this isn't a boolean
      // toggle, so always clean out the explain processor.
      if (this._explainProcessor) {
        this._explainProcessor.shutdown();
        this._explainProcessor = null;
      }

      if (explainToPath) {
        this._createAsyncStatement = this._createExplainedAsyncStatement;
        this._explainProcessor = new ExplainedStatementProcessor(
                                       explainToPath);
      }
      else {
        this._createAsyncStatement = this._realCreateAsyncStatement;
      }
    }
  },

  datastoreIsShutdown: false,

  /**
   * Perform datastore shutdown.
   */
  shutdown: function gloda_ds_shutdown() {
    // Clear out any pending transaction by committing it.
    // The indexer has been shutdown by this point; it no longer has any active
    //  indexing logic and it no longer has active event listeners capable of
    //  generating new activity.
    // Semantic consistency of the database is guaranteed by the indexer's
    //  strategy of only yielding control at coherent times.  Although it takes
    //  multiple calls and multiple SQL operations to update the state of our
    //  database representations, the generator does not yield until it has
    //  issued all the database statements required for said update.  As such,
    //  this commit will leave us in a good way (and the commit will happen
    //  because closing the connection will drain the async execution queue.)
    while (this._transactionDepth) {
      this._log.info("Closing pending transaction out for shutdown.");
      // just schedule this function to be run again once the transaction has
      //  been closed out.
      this._commitTransaction();
    }

    this.datastoreIsShutdown = true;

    // shutdown our folder cleanup timer, if active and null it out.
    if (this._folderCleanupActive)
      this._folderCleanupTimer.cancel();
    this._folderCleanupTimer = null;

    this._log.info("Closing db connection");

    // we do not expect exceptions, but it's a good idea to avoid having our
    //  shutdown process explode.
    try {
      this._cleanupAsyncStatements();
      this._cleanupSyncStatements();
    }
    catch (ex) {
      this._log.debug("Unexpected exception during statement cleanup: " + ex);
    }

    // it's conceivable we might get a spurious exception here, but we really
    //  shouldn't get one.  again, we want to ensure shutdown runs to completion
    //  and doesn't break our caller.
    try {
      // This currently causes all pending asynchronous operations to be run to
      //  completion.  this simplifies things from a correctness perspective,
      //  and, honestly, is a lot easier than us tracking all of the async
      //  event tasks so that we can explicitly cancel them.
      // This is a reasonable thing to do because we don't actually ever have
      //  a huge number of statements outstanding.  The indexing process needs
      //  to issue async requests periodically, so the most we have in-flight
      //  from a write perspective is strictly less than the work required to
      //  update the database state for a single message.
      // However, the potential for multiple pending expensive queries does
      //  exist, and it may be advisable to attempt to track and cancel those.
      //  For simplicity we don't currently do this, and I expect this should
      //  not pose a major problem, but those are famous last words.
      // Note: asyncClose does not spin a nested event loop, but the thread
      //  manager shutdown code will spin the async thread's event loop, so it
      //  nets out to be the same.
      this.asyncConnection.asyncClose();
    }
    catch (ex) {
      this._log.debug("Potentially expected exception during connection " +
                      "closure: " + ex);
    }

    this.asyncConnection = null;
    this.syncConnection = null;
  },

  /**
   * Generates and returns a UUID.
   *
   * @return a UUID as a string, ex: "c4dd0159-9287-480f-a648-a4613e147fdb"
   */
  _generateDatastoreID: function gloda_ds_generateDatastoreID() {
    let uuidGen = Cc["@mozilla.org/uuid-generator;1"]
                    .getService(Ci.nsIUUIDGenerator);
    let uuid = uuidGen.generateUUID().toString();
    // We snip off the { and } from each end of the UUID.
    return uuid.substring(1, uuid.length - 2);
  },

  _determineCachePages: function gloda_ds_determineCachePages(aDBConn) {
    try {
      // For the details of the computations, one should read
      //  nsNavHistory::InitDB. We're slightly diverging from them in the sense
      //  that we won't allow gloda to use insane amounts of memory cache, and
      //  we start with 1% instead of 6% like them.
      let pageStmt = aDBConn.createStatement("PRAGMA page_size");
      pageStmt.executeStep();
      let pageSize = pageStmt.row.page_size;
      pageStmt.finalize();
      let cachePermillage = this._prefBranch
                            .getIntPref("cache_to_memory_permillage");
      cachePermillage = Math.min(cachePermillage, 50);
      cachePermillage = Math.max(cachePermillage, 0);
      let physMem = IOUtils.getPhysicalMemorySize();
      if (physMem == 0)
        physMem = MEMSIZE_FALLBACK_BYTES;
      let cacheSize = Math.round(physMem * cachePermillage / 1000);
      cacheSize = Math.max(cacheSize, MIN_CACHE_SIZE);
      cacheSize = Math.min(cacheSize, MAX_CACHE_SIZE);
      let cachePages = Math.round(cacheSize / pageSize);
      return cachePages;
    } catch (ex) {
      this._log.warn("Error determining cache size: " + ex);
      // A little bit lower than on my personal machine, will result in ~40M.
      return 1000;
    }
  },

  /**
   * Create our database; basically a wrapper around _createSchema.
   */
  _createDB: function gloda_ds_createDB(aDBService, aDBFile) {
    var dbConnection = aDBService.openUnsharedDatabase(aDBFile);
    // We now follow the Firefox strategy for places, which mainly consists in
    //  picking a default 32k page size, and then figuring out the amount of
    //  cache accordingly. The default 32k come from mozilla/toolkit/storage,
    //  but let's get it directly from sqlite in case they change it.
    let cachePages = this._determineCachePages(dbConnection);
    // This is a maximum number of pages to be used.  If the database does not
    //  get this large, then the memory does not get used.
    // Do not forget to update the code in _init if you change this value.
    dbConnection.executeSimpleSQL("PRAGMA cache_size = "+cachePages);
    // The mozStorage default is NORMAL which shaves off some fsyncs in the
    //  interest of performance.  Since everything we do after bootstrap is
    //  async, we do not care about the performance, but we really want the
    //  correctness.  Bug reports and support avenues indicate a non-zero number
    //  of corrupt databases.  Note that this may not fix everything; OS X
    //  also supports an F_FULLSYNC flag enabled by PRAGMA fullfsync that we are
    //  not enabling that is much more comprehensive.  We can think about
    //  turning that on after we've seen how this reduces our corruption count.
    dbConnection.executeSimpleSQL("PRAGMA synchronous = FULL");
    // Register custom tokenizer to index all language text
    var tokenizer = Cc["@mozilla.org/messenger/fts3tokenizer;1"].
                      getService(Ci.nsIFts3Tokenizer);
    tokenizer.registerTokenizer(dbConnection);

    // We're creating a new database, so let's generate a new ID for this
    // version of the datastore. This way, indexers can know when the index
    // has been rebuilt in the event that they need to rebuild dependent data.
    this._datastoreID = this._generateDatastoreID();
    this._prefBranch.setCharPref("id", this._datastoreID);

    dbConnection.beginTransaction();
    try {
      this._createSchema(dbConnection);
      dbConnection.commitTransaction();
    }
    catch(ex) {
      dbConnection.rollbackTransaction();
      throw ex;
    }

    return dbConnection;
  },

  _createTableSchema: function gloda_ds_createTableSchema(aDBConnection,
      aTableName, aTableDef) {
    // - Create the table
    this._log.info("Creating table: " + aTableName);
    aDBConnection.createTable(aTableName,
      [(coldef[0] + " " + coldef[1]) for each
       ([i, coldef] in Iterator(aTableDef.columns))].join(", "));

    // - Create the fulltext table if applicable
    if (aTableDef.fulltextColumns) {
      let createFulltextSQL = "CREATE VIRTUAL TABLE " + aTableName + "Text" +
        " USING fts3(tokenize mozporter, " +
        [(coldef[0] + " " + coldef[1]) for each
         ([i, coldef] in Iterator(aTableDef.fulltextColumns))].join(", ") +
        ")";
      this._log.info("Creating fulltext table: " + createFulltextSQL);
      aDBConnection.executeSimpleSQL(createFulltextSQL);
    }

    // - Create its indices
    if (aTableDef.indices) {
      for each (let [indexName, indexColumns] in Iterator(aTableDef.indices)) {
        aDBConnection.executeSimpleSQL(
          "CREATE INDEX " + indexName + " ON " + aTableName +
          "(" + indexColumns.join(", ") + ")");
      }
    }

    // - Create the attributes table if applicable
    if (aTableDef.genericAttributes) {
      aTableDef.genericAttributes = {
        columns: [
          ["nounID", "INTEGER NOT NULL"],
          ["attributeID", "INTEGER NOT NULL"],
          ["value", "NUMERIC"]
        ],
        indices: {}
      };
      aTableDef.genericAttributes.indices[aTableName + "AttribQuery"] =
        ["attributeID", "value", /* covering: */ "nounID"];
      // let's use this very function!  (since we created genericAttributes,
      //  explodey recursion is avoided.)
      this._createTableSchema(aDBConnection, aTableName + "Attributes",
                              aTableDef.genericAttributes);
    }
  },

  /**
   * Create our database schema assuming a newly created database.  This
   *  comes down to creating normal tables, their full-text variants (if
   *  applicable), and their indices.
   */
  _createSchema: function gloda_ds_createSchema(aDBConnection) {
    // -- For each table...
    for each (let [tableName, tableDef] in Iterator(this._schema.tables)) {
      this._createTableSchema(aDBConnection, tableName, tableDef);
    }

    aDBConnection.schemaVersion = this._actualSchemaVersion =
      this._schemaVersion;
  },

  /**
   * Create a table for a noun, replete with data binding.
   */
  createNounTable: function gloda_ds_createTableIfNotExists(aNounDef) {
    // give it a _jsonText attribute if appropriate...
    if (aNounDef.allowsArbitraryAttrs)
      aNounDef.schema.columns.push(['jsonAttributes', 'STRING', '_jsonText']);
    // check if the table exists
    if (!this.asyncConnection.tableExists(aNounDef.tableName)) {
      // it doesn't! create it (and its potentially many variants)
      try {
        this._createTableSchema(this.asyncConnection, aNounDef.tableName,
                                aNounDef.schema);
      }
      catch (ex) {
         this._log.error("Problem creating table " + aNounDef.tableName + " " +
           "because: " + ex + " at " + ex.fileName + ":" + ex.lineNumber);
         return;
      }
    }

    aNounDef._dataBinder = new GlodaDatabind(aNounDef, this);
    aNounDef.datastore = aNounDef._dataBinder;
    aNounDef.objFromRow = aNounDef._dataBinder.objFromRow;
    aNounDef.objInsert = aNounDef._dataBinder.objInsert;
    aNounDef.objUpdate = aNounDef._dataBinder.objUpdate;
    aNounDef.dbAttribAdjuster = aNounDef._dataBinder.adjustAttributes;

    if (aNounDef.schema.genericAttributes) {
      aNounDef.attrTableName = aNounDef.tableName + "Attributes";
      aNounDef.attrIDColumnName = "nounID";
    }
  },

  _nukeMigration: function gloda_ds_nukeMigration(aDBService, aDBFile,
                                                  aDBConnection) {
    aDBConnection.close();
    aDBFile.remove(false);
    this._log.warn("Global database has been purged due to schema change.  " +
                   "old version was " + this._actualSchemaVersion +
                   ", new version is: " + this._schemaVersion);
    return this._createDB(aDBService, aDBFile);
  },

  /**
   * Migrate the database _to the latest version_ from an older version.  We
   *  only keep enough logic around to get us to the recent version.  This code
   *  is not a time machine!  If we need to blow away the database to get to the
   *  most recent version, then that's the sum total of the migration!
   */
  _migrate: function gloda_ds_migrate(aDBService, aDBFile, aDBConnection,
                                      aCurVersion, aNewVersion) {

    // version 12:
    // - notability column added
    // version 13:
    // - we are adding a new fulltext index column. blow away!
    // - note that I screwed up and failed to mark the schema change; apparently
    //   no database will claim to be version 13...
    // version 14ish, still labeled 13?:
    // - new attributes: forwarded, repliedTo, bcc, recipients
    // - altered fromMeTo and fromMeCc to fromMe
    // - altered toMe and ccMe to just be toMe
    // - exposes bcc to cc-related attributes
    // - MIME type DB schema overhaul
    // version 15ish, still labeled 13:
    // - change tokenizer to mozporter to support CJK
    // (We are slip-streaming this so that only people who want to test CJK
    //  have to test it.  We will properly bump the schema revision when the
    //  gloda correctness patch lands.)
    // version 16ish, labeled 14 and now 16
    // - gloda message id's start from 32 now
    // - all kinds of correctness changes (blow away)
    // version 17
    // - more correctness fixes. (blow away)
    // version 18
    // - significant empty set support (blow away)
    // version 19
    // - there was a typo that was resulting in deleted getting set to the
    //  numeric value of the javascript undefined value.  (migrate-able)
    // version 20
    // - tokenizer changes to provide for case/accent-folding. (blow away)
    // version 21
    // - add the messagesAttribFastDeletion index we thought was already covered
    //  by an index we removed a while ago (migrate-able)
    // version 26
    // - bump page size and also cache size (blow away)
    // version 30
    // - recover from bug 732372 that affected TB 11 beta / TB 12 alpha / TB 13
    //    trunk.  The fix is bug 734507.  The revision bump happens
    //    asynchronously. (migrate-able)

    // nuke if prior to 26
    if (aCurVersion < 26)
      return this._nukeMigration(aDBService, aDBFile, aDBConnection);

    // They must be desiring our "a.contact is undefined" fix!
    // This fix runs asynchronously as the first indexing job the indexer ever
    //  performs.  It is scheduled by the enabling of the message indexer and
    //  it is the one that updates the schema version when done.

    // return the same DB connection since we didn't create a new one or do
    //  anything.
    return aDBConnection;
  },

  /**
   * Asynchronously update the schema version; only for use by in-tree callers
   *  who asynchronously perform migration work triggered by their initial
   *  indexing sweep and who have properly updated the schema version in all
   *  the appropriate locations in this file.
   *
   * This is done without doing anything about the current transaction state,
   *  which is desired.
   */
  _updateSchemaVersion: function(newSchemaVersion) {
    this._actualSchemaVersion = newSchemaVersion;
    let stmt = this._createAsyncStatement(
      // we need to concat; pragmas don't like "?1" binds
      "PRAGMA user_version = " + newSchemaVersion, true);
    stmt.executeAsync(this.trackAsync());
    stmt.finalize();
  },

  _outstandingAsyncStatements: [],

  /**
   * Unless debugging, this is just _realCreateAsyncStatement, but in some
   *  debugging modes this is instead the helpful wrapper
   *  _createExplainedAsyncStatement.
   */
  _createAsyncStatement: null,

  _realCreateAsyncStatement: function gloda_ds_createAsyncStatement(aSQLString,
                                                                aWillFinalize) {
    let statement = null;
    try {
      statement = this.asyncConnection.createAsyncStatement(aSQLString);
    }
    catch(ex) {
       throw new Error("error creating async statement " + aSQLString + " - " +
             this.asyncConnection.lastError + ": " +
             this.asyncConnection.lastErrorString + " - " + ex);
    }

    if (!aWillFinalize)
      this._outstandingAsyncStatements.push(statement);

    return statement;
  },

  /**
   * The ExplainedStatementProcessor instance used by
   *  _createExplainedAsyncStatement.  This will be null if
   *  _createExplainedAsyncStatement is not being used as _createAsyncStatement.
   */
  _explainProcessor: null,

  /**
   * Wrapped version of _createAsyncStatement that EXPLAINs the statement.  When
   *  used this decorates _createAsyncStatement, in which case we are found at
   *  that name and the original is at _orig_createAsyncStatement.  This is
   *  controlled by the explainToPath preference (see |_init|).
   */
  _createExplainedAsyncStatement:
      function gloda_ds__createExplainedAsyncStatement(aSQLString,
                                                       aWillFinalize) {
    let realStatement = this._realCreateAsyncStatement(aSQLString,
                                                       aWillFinalize);
    // don't wrap transaction control statements.
    if (aSQLString == "COMMIT" ||
        aSQLString == "BEGIN TRANSACTION" ||
        aSQLString == "ROLLBACK")
      return realStatement;

    let explainSQL = "EXPLAIN " + aSQLString;
    let explainStatement = this._realCreateAsyncStatement(explainSQL);

    return new ExplainedStatementWrapper(realStatement, explainStatement,
                                         aSQLString, this._explainProcessor);
  },

  _cleanupAsyncStatements: function gloda_ds_cleanupAsyncStatements() {
    [stmt.finalize() for each
     ([i, stmt] in Iterator(this._outstandingAsyncStatements))];
  },

  _outstandingSyncStatements: [],

  _createSyncStatement: function gloda_ds_createSyncStatement(aSQLString,
                                                              aWillFinalize) {
    let statement = null;
    try {
      statement = this.syncConnection.createStatement(aSQLString);
    }
    catch(ex) {
       throw new Error("error creating sync statement " + aSQLString + " - " +
             this.syncConnection.lastError + ": " +
             this.syncConnection.lastErrorString + " - " + ex);
    }

    if (!aWillFinalize)
      this._outstandingSyncStatements.push(statement);

    return statement;
  },

  _cleanupSyncStatements: function gloda_ds_cleanupSyncStatements() {
    [stmt.finalize() for each
     ([i, stmt] in Iterator(this._outstandingSyncStatements))];
  },

  /**
   * Perform a synchronous executeStep on the statement, handling any
   *  SQLITE_BUSY fallout that could conceivably happen from a collision on our
   *  read with the async writes.
   * Basically we keep trying until we succeed or run out of tries.
   * We believe this to be a reasonable course of action because we don't
   *  expect this to happen much.
   */
  _syncStep: function gloda_ds_syncStep(aStatement) {
    let tries = 0;
    while (tries < 32000) {
      try {
        return aStatement.executeStep();
      }
      // SQLITE_BUSY becomes NS_ERROR_FAILURE
      catch (e if e.result == 0x80004005) {
        tries++;
        // we really need to delay here, somehow.  unfortunately, we can't
        //  allow event processing to happen, and most of the things we could
        //  do to delay ourselves result in event processing happening.  (Use
        //  of a timer, a synchronous dispatch, etc.)
        // in theory, nsIThreadEventFilter could allow us to stop other events
        //  that aren't our timer from happening, but it seems slightly
        //  dangerous and 'notxpcom' suggests it ain't happening anyways...
        // so, let's just be dumb and hope that the underlying file I/O going
        //  on makes us more likely to yield to the other thread so it can
        //  finish what it is doing...
      }
    }
    this._log.error("Synchronous step gave up after " + tries + " tries.");
  },

  /**
   * Helper to bind based on the actual type of the javascript value.  Note
   *  that we always use int64 because under the hood sqlite just promotes the
   *  normal 'int' call to 'int64' anyways.
   */
  _bindVariant: function gloda_ds_bindBlob(aStatement, aIndex, aVariant) {
    if (aVariant == null) // catch both null and undefined
      aStatement.bindNullParameter(aIndex);
    else if (typeof aVariant == "string")
      aStatement.bindStringParameter(aIndex, aVariant);
    else if (typeof aVariant == "number") {
      // we differentiate for storage representation reasons only.
      if (Math.floor(aVariant) === aVariant)
        aStatement.bindInt64Parameter(aIndex, aVariant);
      else
        aStatement.bindDoubleParameter(aIndex, aVariant);
    }
    else
      throw new Error("Attempt to bind variant with unsupported type: " +
            (typeof aVariant));
  },

  /**
   * Helper that uses the appropriate getter given the data type; should be
   *  mooted once we move to 1.9.2 and can use built-in variant support.
   */
  _getVariant: function gloda_ds_getBlob(aRow, aIndex) {
    let typeOfIndex = aRow.getTypeOfIndex(aIndex);
    if (typeOfIndex == Ci.mozIStorageValueArray.VALUE_TYPE_NULL)
      return null;
    // XPConnect would just end up going through an intermediary double stage
    //  for the int64 case anyways...
    else if (typeOfIndex == Ci.mozIStorageValueArray.VALUE_TYPE_INTEGER ||
             typeOfIndex == Ci.mozIStorageValueArray.VALUE_TYPE_DOUBLE)
      return aRow.getDouble(aIndex);
    else // typeOfIndex == Ci.mozIStorageValueArray.VALUE_TYPE_TEXT
      return aRow.getString(aIndex);
  },

  /** Simple nested transaction support as a performance optimization. */
  _transactionDepth: 0,
  _transactionGood: false,

  /**
   * Self-memoizing BEGIN TRANSACTION statement.
   */
  get _beginTransactionStatement() {
    let statement = this._createAsyncStatement("BEGIN TRANSACTION");
    this.__defineGetter__("_beginTransactionStatement", function() statement);
    return this._beginTransactionStatement;
  },

  /**
   * Self-memoizing COMMIT statement.
   */
  get _commitTransactionStatement() {
    let statement = this._createAsyncStatement("COMMIT");
    this.__defineGetter__("_commitTransactionStatement", function() statement);
    return this._commitTransactionStatement;
  },

  /**
   * Self-memoizing ROLLBACK statement.
   */
  get _rollbackTransactionStatement() {
    let statement = this._createAsyncStatement("ROLLBACK");
    this.__defineGetter__("_rollbackTransactionStatement", function() statement);
    return this._rollbackTransactionStatement;
  },

  _pendingPostCommitCallbacks: null,
  /**
   * Register a callback to be invoked when the current transaction's commit
   *  completes.
   */
  runPostCommit: function gloda_ds_runPostCommit(aCallback) {
    this._pendingPostCommitCallbacks.push(aCallback);
  },

  /**
   * Begin a potentially nested transaction; only the outermost transaction gets
   *  to be an actual transaction, and the failure of any nested transaction
   *  results in a rollback of the entire outer transaction.  If you really
   *  need an atomic transaction
   */
  _beginTransaction: function gloda_ds_beginTransaction() {
    if (this._transactionDepth == 0) {
      this._pendingPostCommitCallbacks = [];
      this._beginTransactionStatement.executeAsync(this.trackAsync());
      this._transactionGood = true;
    }
    this._transactionDepth++;
  },
  /**
   * Commit a potentially nested transaction; if we are the outer-most
   *  transaction and no sub-transaction issues a rollback
   *  (via _rollbackTransaction) then we commit, otherwise we rollback.
   */
  _commitTransaction: function gloda_ds_commitTransaction() {
    this._transactionDepth--;
    if (this._transactionDepth == 0) {
      try {
        if (this._transactionGood)
          this._commitTransactionStatement.executeAsync(
            new PostCommitHandler(this._pendingPostCommitCallbacks));
        else
          this._rollbackTransactionStatement.executeAsync(this.trackAsync());
      }
      catch (ex) {
        this._log.error("Commit problem:", ex);
      }
      this._pendingPostCommitCallbacks = [];
    }
  },
  /**
   * Abort the commit of the potentially nested transaction.  If we are not the
   *  outermost transaction, we set a flag that tells the outermost transaction
   *  that it must roll back.
   */
  _rollbackTransaction: function gloda_ds_rollbackTransaction() {
    this._transactionDepth--;
    this._transactionGood = false;
    if (this._transactionDepth == 0) {
      try {
        this._rollbackTransactionStatement.executeAsync(this.trackAsync());
      }
      catch (ex) {
        this._log.error("Rollback problem:", ex);
      }
    }
  },

  _pendingAsyncStatements: 0,
  /**
   * The function to call, if any, when we hit 0 pending async statements.
   */
  _pendingAsyncCompletedListener: null,
  _asyncCompleted: function () {
    if (--this._pendingAsyncStatements == 0) {
      if (this._pendingAsyncCompletedListener !== null) {
        this._pendingAsyncCompletedListener();
        this._pendingAsyncCompletedListener = null;
      }
    }
  },
  _asyncTrackerListener: {
    handleResult: function () {},
    handleError: function(aError) {
        GlodaDatastore._log.error("got error in _asyncTrackerListener.handleError(): " +
                        aError.result + ": " + aError.message);
    },
    handleCompletion: function () {
      try {
        // the helper method exists because the other classes need to call it too
        GlodaDatastore._asyncCompleted();
      }
      catch (e) {
        this._log.error("Exception in handleCompletion:", e);
      }
    }
  },
  /**
   * Increments _pendingAsyncStatements and returns a listener that will
   *  decrement the value when the statement completes.
   */
  trackAsync: function() {
    this._pendingAsyncStatements++;
    return this._asyncTrackerListener;
  },

  /* ********** Attribute Definitions ********** */
  /** Maps (attribute def) compound names to the GlodaAttributeDBDef objects. */
  _attributeDBDefs: {},
  /** Map attribute ID to the definition and parameter value that produce it. */
  _attributeIDToDBDefAndParam: {},

  /**
   * This attribute id indicates that we are encoding that a non-singular
   *  attribute has an empty set.  The value payload that goes with this should
   *  the attribute id of the attribute we are talking about.
   */
  kEmptySetAttrId: 1,

  /**
   * We maintain the attributeDefinitions next id counter mainly because we can.
   *  Since we mediate the access, there's no real risk to doing so, and it
   *  allows us to keep the writes on the async connection without having to
   *  wait for a completion notification.
   *
   * Start from 32 so we can have a number of sentinel values.
   */
  _nextAttributeId: 32,

  _populateAttributeDefManagedId: function () {
    let stmt = this._createSyncStatement(
      "SELECT MAX(id) FROM attributeDefinitions", true);
    if (stmt.executeStep()) { // no chance of this SQLITE_BUSY on this call
      // 0 gets returned even if there are no messages...
      let highestSeen = stmt.getInt64(0);
      if (highestSeen != 0)
        this._nextAttributeId = highestSeen + 1;
    }
    stmt.finalize();
  },

  get _insertAttributeDefStatement() {
    let statement = this._createAsyncStatement(
      "INSERT INTO attributeDefinitions (id, attributeType, extensionName, \
                                  name, parameter) \
              VALUES (?1, ?2, ?3, ?4, ?5)");
    this.__defineGetter__("_insertAttributeDefStatement", function() statement);
    return this._insertAttributeDefStatement;
  },

  /**
   * Create an attribute definition and return the row ID.  Special/atypical
   *  in that it doesn't directly return a GlodaAttributeDBDef; we leave that up
   *  to the caller since they know much more than actually needs to go in the
   *  database.
   *
   * @return The attribute id allocated to this attribute.
   */
  _createAttributeDef: function gloda_ds_createAttributeDef(aAttrType,
                                    aExtensionName, aAttrName, aParameter) {
    let attributeId = this._nextAttributeId++;

    let iads = this._insertAttributeDefStatement;
    iads.bindInt64Parameter(0, attributeId);
    iads.bindInt64Parameter(1, aAttrType);
    iads.bindStringParameter(2, aExtensionName);
    iads.bindStringParameter(3, aAttrName);
    this._bindVariant(iads, 4, aParameter);

    iads.executeAsync(this.trackAsync());

    return attributeId;
  },

  /**
   * Sync-ly look-up all the attribute definitions, populating our authoritative
   *  _attributeDBDefss and _attributeIDToDBDefAndParam maps.  (In other words,
   *  once this method is called, those maps should always be in sync with the
   *  underlying database.)
   */
  getAllAttributes: function gloda_ds_getAllAttributes() {
    let stmt = this._createSyncStatement(
      "SELECT id, attributeType, extensionName, name, parameter \
         FROM attributeDefinitions", true);

    // map compound name to the attribute
    let attribs = {};
    // map the attribute id to [attribute, parameter] where parameter is null
    //  in cases where parameter is unused.
    let idToAttribAndParam = {};

    this._log.info("loading all attribute defs");

    while (stmt.executeStep()) {  // no chance of this SQLITE_BUSY on this call
      let rowId = stmt.getInt64(0);
      let rowAttributeType = stmt.getInt64(1);
      let rowExtensionName = stmt.getString(2);
      let rowName = stmt.getString(3);
      let rowParameter = this._getVariant(stmt, 4);

      let compoundName = rowExtensionName + ":" + rowName;

      let attrib;
      if (compoundName in attribs) {
        attrib = attribs[compoundName];
      } else {
        attrib = new GlodaAttributeDBDef(this, /* aID */ null,
          compoundName, rowAttributeType, rowExtensionName, rowName);
        attribs[compoundName] = attrib;
      }
      // if the parameter is null, the id goes on the attribute def, otherwise
      //  it is a parameter binding and goes in the binding map.
      if (rowParameter == null) {
        this._log.debug(compoundName + " primary: " + rowId);
        attrib._id = rowId;
        idToAttribAndParam[rowId] = [attrib, null];
      } else {
        this._log.debug(compoundName + " binding: " + rowParameter +
            " = " + rowId);
        attrib._parameterBindings[rowParameter] = rowId;
        idToAttribAndParam[rowId] = [attrib, rowParameter];
      }
    }
    stmt.finalize();

    this._log.info("done loading all attribute defs");

    this._attributeDBDefs = attribs;
    this._attributeIDToDBDefAndParam = idToAttribAndParam;
  },

  /**
   * Helper method for GlodaAttributeDBDef to tell us when their bindParameter
   *  method is called and they have created a new binding (using
   *  GlodaDatastore._createAttributeDef).  In theory, that method could take
   *  an additional argument and obviate the need for this method.
   */
  reportBinding: function gloda_ds_reportBinding(aID, aAttrDef, aParamValue) {
    this._attributeIDToDBDefAndParam[aID] = [aAttrDef, aParamValue];
  },

  /* ********** Folders ********** */
  /** next folder (row) id to issue, populated by _getAllFolderMappings. */
  _nextFolderId: 1,

  get _insertFolderLocationStatement() {
    let statement = this._createAsyncStatement(
      "INSERT INTO folderLocations (id, folderURI, dirtyStatus, name, \
                                    indexingPriority) VALUES \
        (?1, ?2, ?3, ?4, ?5)");
    this.__defineGetter__("_insertFolderLocationStatement",
      function() statement);
    return this._insertFolderLocationStatement;
  },

  /**
   * Authoritative map from folder URI to folder ID.  (Authoritative in the
   *  sense that this map exactly represents the state of the underlying
   *  database.  If it does not, it's a bug in updating the database.)
   */
  _folderByURI: {},
  /** Authoritative map from folder ID to folder URI */
  _folderByID: {},

  /** Intialize our _folderByURI/_folderByID mappings, called by _init(). */
  _getAllFolderMappings: function gloda_ds_getAllFolderMappings() {
    let stmt = this._createSyncStatement(
      "SELECT id, folderURI, dirtyStatus, name, indexingPriority \
        FROM folderLocations", true);

    while (stmt.executeStep()) {  // no chance of this SQLITE_BUSY on this call
      let folderID = stmt.getInt64(0);
      let folderURI = stmt.getString(1);
      let dirtyStatus = stmt.getInt32(2);
      let folderName = stmt.getString(3);
      let indexingPriority = stmt.getInt32(4);

      let folder = new GlodaFolder(this, folderID, folderURI, dirtyStatus,
                                   folderName, indexingPriority);

      this._folderByURI[folderURI] = folder;
      this._folderByID[folderID] = folder;

      if (folderID >= this._nextFolderId)
        this._nextFolderId = folderID + 1;
    }
    stmt.finalize();
  },

  _folderKnown: function gloda_ds_folderKnown(aFolder) {
    let folderURI = aFolder.URI;
    return folderURI in this._folderByURI;
  },

  _folderIdKnown: function gloda_ds_folderIdKnown(aFolderID) {
    return (aFolderID in this._folderByID);
  },

  /**
   * Return the default messaging priority for a folder of this type, based
   * on the folder's flags. If aAllowSpecialFolderIndexing is true, then
   * folders suchs as Trash and Junk will be indexed.
   *
   * @param {nsIMsgFolder} aFolder
   * @param {boolean} aAllowSpecialFolderIndexing
   * @returns {Number}
   */
  getDefaultIndexingPriority: function gloda_ds_getDefaultIndexingPriority(aFolder, aAllowSpecialFolderIndexing) {

    let indexingPriority = GlodaFolder.prototype.kIndexingDefaultPriority;
    // Do not walk into trash/junk folders, unless the user is explicitly
    //  telling us to do so.
    let specialFolderFlags = Ci.nsMsgFolderFlags.Trash | Ci.nsMsgFolderFlags.Junk;
    if (aFolder.isSpecialFolder(specialFolderFlags, true))
      indexingPriority = aAllowSpecialFolderIndexing ?
                           GlodaFolder.prototype.kIndexingDefaultPriority :
                           GlodaFolder.prototype.kIndexingNeverPriority;
    // Queue folders should always be ignored just because messages should not
    //  spend much time in there.
    // We hate newsgroups, and public IMAP folders are similar.
    // Other user IMAP folders should be ignored because it's not this user's
    //  mail.
    else if (aFolder.flags & (Ci.nsMsgFolderFlags.Queue
                              | Ci.nsMsgFolderFlags.Newsgroup
                              // In unit testing at least folders can be
                              // confusingly labeled ImapPublic when they
                              // should not be.  Or at least I don't think they
                              // should be.  So they're legit for now.
                              //| Ci.nsMsgFolderFlags.ImapPublic
                              //| Ci.nsMsgFolderFlags.ImapOtherUser
                             ))
      indexingPriority = GlodaFolder.prototype.kIndexingNeverPriority;
    else if (aFolder.flags & Ci.nsMsgFolderFlags.Inbox)
      indexingPriority = GlodaFolder.prototype.kIndexingInboxPriority;
    else if (aFolder.flags & Ci.nsMsgFolderFlags.SentMail)
      indexingPriority = GlodaFolder.prototype.kIndexingSentMailPriority;
    else if (aFolder.flags & Ci.nsMsgFolderFlags.Favorite)
      indexingPriority = GlodaFolder.prototype.kIndexingFavoritePriority;
    else if (aFolder.flags & Ci.nsMsgFolderFlags.CheckNew)
      indexingPriority = GlodaFolder.prototype.kIndexingCheckNewPriority;

    return indexingPriority;
  },

  /**
   * Map a folder URI to a GlodaFolder instance, creating the mapping if it does
   *  not yet exist.
   *
   * @param aFolder The nsIMsgFolder instance you would like the GlodaFolder
   *     instance for.
   * @returns The existing or newly created GlodaFolder instance.
   */
  _mapFolder: function gloda_ds_mapFolderURI(aFolder) {
    let folderURI = aFolder.URI;
    if (folderURI in this._folderByURI) {
      return this._folderByURI[folderURI];
    }

    let folderID = this._nextFolderId++;

    // if there's an indexingPriority stored on the folder, just use that
    let indexingPriority;
    let stringPrio = aFolder.getStringProperty("indexingPriority");
    if (stringPrio.length)
      indexingPriority = parseInt(stringPrio);
    else
      // otherwise, fall back to the default for folders of this type
      indexingPriority = this.getDefaultIndexingPriority(aFolder);

    // If there are messages in the folder, it is filthy.  If there are no
    //  messages, it can be clean.
    let dirtyStatus = aFolder.getTotalMessages(false) ?
                        GlodaFolder.prototype.kFolderFilthy :
                        GlodaFolder.prototype.kFolderClean;
    let folder = new GlodaFolder(this, folderID, folderURI, dirtyStatus,
                                 aFolder.prettiestName, indexingPriority);

    this._insertFolderLocationStatement.bindInt64Parameter(0, folder.id);
    this._insertFolderLocationStatement.bindStringParameter(1, folder.uri);
    this._insertFolderLocationStatement.bindInt64Parameter(2,
                                                           folder.dirtyStatus);
    this._insertFolderLocationStatement.bindStringParameter(3, folder.name);
    this._insertFolderLocationStatement.bindInt64Parameter(
      4, folder.indexingPriority);
    this._insertFolderLocationStatement.executeAsync(this.trackAsync());

    this._folderByURI[folderURI] = folder;
    this._folderByID[folderID] = folder;
    this._log.debug("!! mapped " + folder.id + " from " + folderURI);
    return folder;
  },

  /**
   * Map an integer gloda folder ID to the corresponding GlodaFolder instance.
   *
   * @param aFolderID The known valid gloda folder ID for which you would like
   *     a GlodaFolder instance.
   * @return The GlodaFolder instance with the given id.  If no such instance
   *     exists, we will throw an exception.
   */
  _mapFolderID: function gloda_ds_mapFolderID(aFolderID) {
    if (aFolderID === null)
      return null;
    if (aFolderID in this._folderByID)
      return this._folderByID[aFolderID];
    throw new Error("Got impossible folder ID: " + aFolderID);
  },

  /**
   * Mark the gloda folder as deleted for any outstanding references to it and
   *  remove it from our tables so we don't hand out any new references.  The
   *  latter is especially important in the case a folder with the same name
   *  is created afterwards; we don't want to confuse the new one with the old
   *  one!
   */
  _killGlodaFolderIntoTombstone:
      function gloda_ds__killGlodaFolderIntoTombstone(aGlodaFolder) {
    aGlodaFolder._deleted = true;
    delete this._folderByURI[aGlodaFolder.uri];
    delete this._folderByID[aGlodaFolder.id];
  },

  get _updateFolderDirtyStatusStatement() {
    let statement = this._createAsyncStatement(
      "UPDATE folderLocations SET dirtyStatus = ?1 \
              WHERE id = ?2");
    this.__defineGetter__("_updateFolderDirtyStatusStatement",
      function() statement);
    return this._updateFolderDirtyStatusStatement;
  },

  updateFolderDirtyStatus: function gloda_ds_updateFolderDirtyStatus(aFolder) {
    let ufds = this._updateFolderDirtyStatusStatement;
    ufds.bindInt64Parameter(1, aFolder.id);
    ufds.bindInt64Parameter(0, aFolder.dirtyStatus);
    ufds.executeAsync(this.trackAsync());
  },

  get _updateFolderIndexingPriorityStatement() {
    let statement = this._createAsyncStatement(
      "UPDATE folderLocations SET indexingPriority = ?1 \
              WHERE id = ?2");
    this.__defineGetter__("_updateFolderIndexingPriorityStatement",
      function() statement);
    return this._updateFolderIndexingPriorityStatement;
  },

  updateFolderIndexingPriority: function gloda_ds_updateFolderIndexingPriority(aFolder) {
    let ufip = this._updateFolderIndexingPriorityStatement;
    ufip.bindInt64Parameter(1, aFolder.id);
    ufip.bindInt64Parameter(0, aFolder.indexingPriority);
    ufip.executeAsync(this.trackAsync());
  },

  get _updateFolderLocationStatement() {
    let statement = this._createAsyncStatement(
      "UPDATE folderLocations SET folderURI = ?1 \
              WHERE id = ?2");
    this.__defineGetter__("_updateFolderLocationStatement",
      function() statement);
    return this._updateFolderLocationStatement;
  },

  /**
   * Non-recursive asynchronous folder renaming based on the URI.
   *
   * @TODO provide a mechanism for recursive folder renames or have a higher
   *     layer deal with it and remove this note.
   */
  renameFolder: function gloda_ds_renameFolder(aOldFolder, aNewURI) {
    let folder = this._mapFolder(aOldFolder); // ensure the folder is mapped
    let oldURI = folder.uri;
    this._folderByURI[aNewURI] = folder;
    folder._uri = aNewURI;
    this._log.info("renaming folder URI " + oldURI + " to " + aNewURI);
    this._updateFolderLocationStatement.bindStringParameter(1, folder.id);
    this._updateFolderLocationStatement.bindStringParameter(0, aNewURI);
    this._updateFolderLocationStatement.executeAsync(this.trackAsync());

    delete this._folderByURI[oldURI];
  },

  get _deleteFolderByIDStatement() {
    let statement = this._createAsyncStatement(
      "DELETE FROM folderLocations WHERE id = ?1");
    this.__defineGetter__("_deleteFolderByIDStatement",
      function() statement);
    return this._deleteFolderByIDStatement;
  },

  deleteFolderByID: function gloda_ds_deleteFolder(aFolderID) {
    let dfbis = this._deleteFolderByIDStatement;
    dfbis.bindInt64Parameter(0, aFolderID);
    dfbis.executeAsync(this.trackAsync());
  },

  /**
   * This timer drives our folder cleanup logic that is in charge of dropping
   *  our folder references and more importantly the folder's msgDatabase
   *  reference, but only if they are no longer in use.
   * This timer is only active when we have one or more live gloda folders (as
   *  tracked by _liveGlodaFolders).  Although we choose our timer interval to
   *  be power-friendly, it doesn't really matter because unless the user or the
   *  indexing process is actively doing things, all of the folders will 'die'
   *  and so we will stop scheduling the timer.
   */
  _folderCleanupTimer: null,

  /**
   * When true, we have a folder cleanup timer event active.
   */
  _folderCleanupActive: false,

  /**
   * Interval at which we call the folder cleanup code, in milliseconds.
   */
  _folderCleanupTimerInterval: 2000,

  /**
   * Maps the id of 'live' GlodaFolders to the instances.  If a GlodaFolder is
   *  in here, it means that it has a reference to its nsIMsgDBFolder which
   *  should have an open nsIMsgDatabase that we will need to close.  This does
   *  not count folders that are being indexed unless they have also been used
   *  for header retrieval.
   */
  _liveGlodaFolders: {},

  /**
   * Mark a GlodaFolder as having a live reference to its nsIMsgFolder with an
   *  implied opened associated message database.  GlodaFolder calls this when
   *  it first acquires its reference.  It is removed from the list of live
   *  folders only when our timer check calls the GlodaFolder's
   *  forgetFolderIfUnused method and that method returns true.
   */
  markFolderLive: function gloda_ds_markFolderLive(aGlodaFolder) {
    this._liveGlodaFolders[aGlodaFolder.id] = aGlodaFolder;
    if (!this._folderCleanupActive) {
      this._folderCleanupTimer.initWithCallback(this._performFolderCleanup,
        this._folderCleanupTimerInterval, Ci.nsITimer.TYPE_REPEATING_SLACK);
      this._folderCleanupActive = true;
    }
  },

  /**
   * Timer-driven folder cleanup logic.  For every live folder tracked in
   *  _liveGlodaFolders, we call their forgetFolderIfUnused method each time
   *  until they return true indicating they have cleaned themselves up.
   * This method is called without a 'this' context!
   */
  _performFolderCleanup: function gloda_ds_performFolderCleanup() {
    // we only need to keep going if there is at least one folder in the table
    //  that is still alive after this pass.
    let keepGoing = false;
    for each (let glodaFolder in GlodaDatastore._liveGlodaFolders) {
      // returns true if it is now 'dead' and doesn't need this heartbeat check
      if (glodaFolder.forgetFolderIfUnused())
        delete GlodaDatastore._liveGlodaFolders[glodaFolder.id];
      else
        keepGoing = true;
    }

    if (!keepGoing) {
      GlodaDatastore._folderCleanupTimer.cancel();
      GlodaDatastore._folderCleanupActive = false;
    }
  },

  /* ********** Conversation ********** */
  /** The next conversation id to allocate.  Initialize at startup. */
  _nextConversationId: 1,

  _populateConversationManagedId: function () {
    let stmt = this._createSyncStatement(
      "SELECT MAX(id) FROM conversations", true);
    if (stmt.executeStep()) { // no chance of this SQLITE_BUSY on this call
      this._nextConversationId = stmt.getInt64(0) + 1;
    }
    stmt.finalize();
  },

  get _insertConversationStatement() {
    let statement = this._createAsyncStatement(
      "INSERT INTO conversations (id, subject, oldestMessageDate, \
                                  newestMessageDate) \
              VALUES (?1, ?2, ?3, ?4)");
    this.__defineGetter__("_insertConversationStatement", function() statement);
    return this._insertConversationStatement;
  },

  get _insertConversationTextStatement() {
    let statement = this._createAsyncStatement(
      "INSERT INTO conversationsText (docid, subject) \
              VALUES (?1, ?2)");
    this.__defineGetter__("_insertConversationTextStatement",
      function() statement);
    return this._insertConversationTextStatement;
  },

  /**
   * Asynchronously create a conversation.
   */
  createConversation: function gloda_ds_createConversation(aSubject,
        aOldestMessageDate, aNewestMessageDate) {

    // create the data row
    let conversationID = this._nextConversationId++;
    let ics = this._insertConversationStatement;
    ics.bindInt64Parameter(0, conversationID);
    ics.bindStringParameter(1, aSubject);
    if (aOldestMessageDate == null)
      ics.bindNullParameter(2);
    else
      ics.bindInt64Parameter(2, aOldestMessageDate);
    if (aNewestMessageDate == null)
      ics.bindNullParameter(3);
    else
      ics.bindInt64Parameter(3, aNewestMessageDate);
    ics.executeAsync(this.trackAsync());

    // create the fulltext row, using the same rowid/docid
    let icts = this._insertConversationTextStatement;
    icts.bindInt64Parameter(0, conversationID);
    icts.bindStringParameter(1, aSubject);
    icts.executeAsync(this.trackAsync());

    // create it
    let conversation = new GlodaConversation(this, conversationID,
                                 aSubject, aOldestMessageDate,
                                 aNewestMessageDate);
    // it's new! let the collection manager know about it.
    GlodaCollectionManager.itemsAdded(conversation.NOUN_ID, [conversation]);
    // return it
    return conversation;
  },

  get _deleteConversationByIDStatement() {
    let statement = this._createAsyncStatement(
      "DELETE FROM conversations WHERE id = ?1");
    this.__defineGetter__("_deleteConversationByIDStatement",
                          function() statement);
    return this._deleteConversationByIDStatement;
  },

  /**
   * Asynchronously delete a conversation given its ID.
   */
  deleteConversationByID: function gloda_ds_deleteConversationByID(
                                      aConversationID) {
    let dcbids = this._deleteConversationByIDStatement;
    dcbids.bindInt64Parameter(0, aConversationID);
    dcbids.executeAsync(this.trackAsync());

    GlodaCollectionManager.itemsDeleted(GlodaConversation.prototype.NOUN_ID,
                                        [aConversationID]);
  },

  _conversationFromRow: function gloda_ds_conversationFromRow(aStmt) {
      let oldestMessageDate, newestMessageDate;
      if (aStmt.getTypeOfIndex(2) == Ci.mozIStorageValueArray.VALUE_TYPE_NULL)
        oldestMessageDate = null;
      else
        oldestMessageDate = aStmt.getInt64(2);
      if (aStmt.getTypeOfIndex(3) == Ci.mozIStorageValueArray.VALUE_TYPE_NULL)
        newestMessageDate = null;
      else
        newestMessageDate = aStmt.getInt64(3);
      return new GlodaConversation(this, aStmt.getInt64(0),
        aStmt.getString(1), oldestMessageDate, newestMessageDate);
  },

  /* ********** Message ********** */
  /**
   * Next message id, managed because of our use of asynchronous inserts.
   * Initialized by _populateMessageManagedId called by _init.
   *
   * Start from 32 to leave us all kinds of magical sentinel values at the
   *  bottom.
   */
  _nextMessageId: 32,

  _populateMessageManagedId: function () {
    let stmt = this._createSyncStatement(
      "SELECT MAX(id) FROM messages", true);
    if (stmt.executeStep()) { // no chance of this SQLITE_BUSY on this call
      // 0 gets returned even if there are no messages...
      let highestSeen = stmt.getInt64(0);
      if (highestSeen != 0)
        this._nextMessageId = highestSeen + 1;
    }
    stmt.finalize();
  },

  get _insertMessageStatement() {
    let statement = this._createAsyncStatement(
      "INSERT INTO messages (id, folderID, messageKey, conversationID, date, \
                             headerMessageID, jsonAttributes, notability) \
              VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8)");
    this.__defineGetter__("_insertMessageStatement", function() statement);
    return this._insertMessageStatement;
  },

  get _insertMessageTextStatement() {
    let statement = this._createAsyncStatement(
      "INSERT INTO messagesText (docid, subject, body, attachmentNames, \
                                 author, recipients) \
              VALUES (?1, ?2, ?3, ?4, ?5, ?6)");
    this.__defineGetter__("_insertMessageTextStatement", function() statement);
    return this._insertMessageTextStatement;
  },

  /**
   * Create a GlodaMessage with the given properties.  Because this is only half
   *  of the process of creating a message (the attributes still need to be
   *  completed), it's on the caller's head to call GlodaCollectionManager's
   *  itemAdded method once the message is fully created.
   *
   * This method uses the async connection, any downstream logic that depends on
   *  this message actually existing in the database must be done using an
   *  async query.
   */
  createMessage: function gloda_ds_createMessage(aFolder, aMessageKey,
                              aConversationID, aDatePRTime, aHeaderMessageID) {
    let folderID;
    if (aFolder != null) {
      folderID = this._mapFolder(aFolder).id;
    }
    else {
      folderID = null;
    }

    let messageID = this._nextMessageId++;

    let message = new GlodaMessage(
      this, messageID, folderID,
      aMessageKey,
      aConversationID, /* conversation */ null,
      aDatePRTime ? new Date(aDatePRTime / 1000) : null,
      aHeaderMessageID,
      /* deleted */ false, /* jsonText */ undefined, /* notability*/ 0);

    // We would love to notify the collection manager about the message at this
    //  point (at least if it's not a ghost), but we can't yet.  We need to wait
    //  until the attributes have been indexed, which means it's out of our
    //  hands.  (Gloda.processMessage does it.)

    return message;
  },

  insertMessage: function gloda_ds_insertMessage(aMessage) {
    let ims = this._insertMessageStatement;
    ims.bindInt64Parameter(0, aMessage.id);
    if (aMessage.folderID == null)
      ims.bindNullParameter(1);
    else
      ims.bindInt64Parameter(1, aMessage.folderID);
    if (aMessage.messageKey == null)
      ims.bindNullParameter(2);
    else
      ims.bindInt64Parameter(2, aMessage.messageKey);
    ims.bindInt64Parameter(3, aMessage.conversationID);
    if (aMessage.date == null)
      ims.bindNullParameter(4);
    else
      ims.bindInt64Parameter(4, aMessage.date * 1000);
    ims.bindStringParameter(5, aMessage.headerMessageID);
    if (aMessage._jsonText)
      ims.bindStringParameter(6, aMessage._jsonText);
    else
      ims.bindNullParameter(6);
    ims.bindInt64Parameter(7, aMessage.notability);

    try {
       ims.executeAsync(this.trackAsync());
    }
    catch(ex) {
       throw new Error("error executing statement... " +
             this.asyncConnection.lastError + ": " +
             this.asyncConnection.lastErrorString + " - " + ex);
    }

    // we create the full-text row for any message that isn't a ghost,
    // whether we have the body or not
    if (aMessage.folderID !== null)
      this._insertMessageText(aMessage);
  },

  /**
   * Inserts a full-text row. This should only be called if you're sure you want
   * to insert a row into the table.
   */
  _insertMessageText: function gloda_ds__insertMessageText(aMessage) {
    if (aMessage._content && aMessage._content.hasContent())
      aMessage._indexedBodyText = aMessage._content.getContentString(true);
    else if (aMessage._bodyLines)
      aMessage._indexedBodyText = aMessage._bodyLines.join("\n");
    else
      aMessage._indexedBodyText = null;

    let imts = this._insertMessageTextStatement;
    imts.bindInt64Parameter(0, aMessage.id);
    imts.bindStringParameter(1, aMessage._subject);
    if (aMessage._indexedBodyText == null)
      imts.bindNullParameter(2);
    else
      imts.bindStringParameter(2, aMessage._indexedBodyText);
    if (aMessage._attachmentNames === null)
      imts.bindNullParameter(3);
    else
      imts.bindStringParameter(3, aMessage._attachmentNames.join("\n"));

    imts.bindStringParameter(4, aMessage._indexAuthor);
    imts.bindStringParameter(5, aMessage._indexRecipients);

    try {
      imts.executeAsync(this.trackAsync());
    }
    catch(ex) {
      throw new Error("error executing fulltext statement... " +
            this.asyncConnection.lastError + ": " +
            this.asyncConnection.lastErrorString + " - " + ex);
    }
  },

  get _updateMessageStatement() {
    let statement = this._createAsyncStatement(
      "UPDATE messages SET folderID = ?1, \
                           messageKey = ?2, \
                           conversationID = ?3, \
                           date = ?4, \
                           headerMessageID = ?5, \
                           jsonAttributes = ?6, \
                           notability = ?7, \
                           deleted = ?8 \
              WHERE id = ?9");
    this.__defineGetter__("_updateMessageStatement", function() statement);
    return this._updateMessageStatement;
  },

  get _updateMessageTextStatement() {
    let statement = this._createAsyncStatement(
      "UPDATE messagesText SET body = ?1, \
                               attachmentNames = ?2 \
              WHERE docid = ?3");

    this.__defineGetter__("_updateMessageTextStatement", function() statement);
    return this._updateMessageTextStatement;
  },

  /**
   * Update the database row associated with the message. If the message is
   * not a ghost and has _isNew defined, messagesText is affected.
   *
   * aMessage._isNew is currently equivalent to the fact that there is no
   * full-text row associated with this message, and we work with this
   * assumption here. Note that if aMessage._isNew is not defined, then
   * we don't do anything.
   */
  updateMessage: function gloda_ds_updateMessage(aMessage) {
    let ums = this._updateMessageStatement;
    ums.bindInt64Parameter(8, aMessage.id);
    if (aMessage.folderID === null)
      ums.bindNullParameter(0);
    else
      ums.bindInt64Parameter(0, aMessage.folderID);
    if (aMessage.messageKey === null)
      ums.bindNullParameter(1);
    else
      ums.bindInt64Parameter(1, aMessage.messageKey);
    ums.bindInt64Parameter(2, aMessage.conversationID);
    if (aMessage.date === null)
      ums.bindNullParameter(3);
    else
      ums.bindInt64Parameter(3, aMessage.date * 1000);
    ums.bindStringParameter(4, aMessage.headerMessageID);
    if (aMessage._jsonText)
      ums.bindStringParameter(5, aMessage._jsonText);
    else
      ums.bindNullParameter(5);
    ums.bindInt64Parameter(6, aMessage.notability);
    ums.bindInt64Parameter(7, aMessage._isDeleted ? 1 : 0);

    ums.executeAsync(this.trackAsync());

    if (aMessage.folderID !== null) {
      if (aMessage._isNew === true)
        this._insertMessageText(aMessage);
      else
        this._updateMessageText(aMessage);
    }
  },

  /**
   * Updates the full-text row associated with this message. This only performs
   * the UPDATE query if the indexed body text has changed, which means that if
   * the body hasn't changed but the attachments have, we don't update.
   */
  _updateMessageText: function gloda_ds__updateMessageText(aMessage) {
    let newIndexedBodyText;
    if (aMessage._content && aMessage._content.hasContent())
      newIndexedBodyText = aMessage._content.getContentString(true);
    else if (aMessage._bodyLines)
      newIndexedBodyText = aMessage._bodyLines.join("\n");
    else
      newIndexedBodyText = null;

    // If the body text matches, don't perform an update
    if (newIndexedBodyText == aMessage._indexedBodyText) {
      this._log.debug("in _updateMessageText, skipping update because body matches");
      return;
    }

    aMessage._indexedBodyText = newIndexedBodyText;
    let umts = this._updateMessageTextStatement;
    umts.bindInt64Parameter(2, aMessage.id);

    if (aMessage._indexedBodyText == null)
      umts.bindNullParameter(0);
    else
      umts.bindStringParameter(0, aMessage._indexedBodyText);

    if (aMessage._attachmentNames == null)
      umts.bindNullParameter(1);
    else
      umts.bindStringParameter(1, aMessage._attachmentNames.join("\n"));

    try {
      umts.executeAsync(this.trackAsync());
    }
    catch(ex) {
      throw new Error("error executing fulltext statement... " +
            this.asyncConnection.lastError + ": " +
            this.asyncConnection.lastErrorString + " - " + ex);
    }
  },

  get _updateMessageLocationStatement() {
    let statement = this._createAsyncStatement(
      "UPDATE messages SET folderID = ?1, messageKey = ?2 WHERE id = ?3");
    this.__defineGetter__("_updateMessageLocationStatement",
                          function() statement);
    return this._updateMessageLocationStatement;
  },

  /**
   * Given a list of gloda message ids, and a list of their new message keys in
   *  the given new folder location, asynchronously update the message's
   *  database locations.  Also, update the in-memory representations.
   */
  updateMessageLocations: function gloda_ds_updateMessageLocations(aMessageIds,
      aNewMessageKeys, aDestFolder, aDoNotNotify) {
    let statement = this._updateMessageLocationStatement;
    let destFolderID = (typeof(aDestFolder) == "number") ? aDestFolder :
                         this._mapFolder(aDestFolder).id;

    // map gloda id to the new message key for in-memory rep transform below
    let cacheLookupMap = {};

    for (let iMsg = 0; iMsg < aMessageIds.length; iMsg++) {
      let id = aMessageIds[iMsg], msgKey = aNewMessageKeys[iMsg];
      statement.bindInt64Parameter(0, destFolderID);
      statement.bindInt64Parameter(1, msgKey);
      statement.bindInt64Parameter(2, id);
      statement.executeAsync(this.trackAsync());

      cacheLookupMap[id] = msgKey;
    }

    // - perform the cache lookup so we can update in-memory representations
    // found in memory items, and converted to list form for notification
    let inMemoryItems = {}, modifiedItems = [];
    GlodaCollectionManager.cacheLookupMany(GlodaMessage.prototype.NOUN_ID,
                                           cacheLookupMap,
                                           inMemoryItems,
                                           /* do not cache */ false);
    for each (let [glodaId, glodaMsg] in Iterator(inMemoryItems)) {
      glodaMsg._folderID = destFolderID;
      glodaMsg._messageKey = cacheLookupMap[glodaId];
      modifiedItems.push(glodaMsg);
    }

    // tell the collection manager about the modified messages so it can update
    //  any existing views...
    if (!aDoNotNotify && modifiedItems.length) {
      GlodaCollectionManager.itemsModified(GlodaMessage.prototype.NOUN_ID,
                                           modifiedItems);
    }
  },

  get _updateMessageKeyStatement() {
    let statement = this._createAsyncStatement(
      "UPDATE messages SET messageKey = ?1 WHERE id = ?2");
    this.__defineGetter__("_updateMessageKeyStatement",
                          function() statement);
    return this._updateMessageKeyStatement;
  },

  /**
   * Update the message keys for the gloda messages with the given id's.  This
   *  is to be used in response to msgKeyChanged notifications and is similar to
   *  `updateMessageLocations` except that we do not update the folder and we
   *  do not perform itemsModified notifications (because message keys are not
   *  intended to be relevant to the gloda message abstraction).
   */
  updateMessageKeys: function(aMessageIds, aNewMessageKeys) {
    let statement = this._updateMessageKeyStatement;

    // map gloda id to the new message key for in-memory rep transform below
    let cacheLookupMap = {};

    for (let iMsg = 0; iMsg < aMessageIds.length; iMsg++) {
      let id = aMessageIds[iMsg], msgKey = aNewMessageKeys[iMsg];
      statement.bindInt64Parameter(0, msgKey);
      statement.bindInt64Parameter(1, id);
      statement.executeAsync(this.trackAsync());

      cacheLookupMap[id] = msgKey;
    }

    // - perform the cache lookup so we can update in-memory representations
    let inMemoryItems = {};
    GlodaCollectionManager.cacheLookupMany(GlodaMessage.prototype.NOUN_ID,
                                           cacheLookupMap,
                                           inMemoryItems,
                                           /* do not cache */ false);
    for each (let [glodaId, glodaMsg] in Iterator(inMemoryItems)) {
      glodaMsg._messageKey = cacheLookupMap[glodaId];
    }
  },

  /**
   * Asynchronously mutate message folder id/message keys for the given
   *  messages, indicating that we are moving them to the target folder, but
   *  don't yet know their target message keys.
   *
   * Updates in-memory representations too.
   */
  updateMessageFoldersByKeyPurging:
      function gloda_ds_updateMessageFoldersByKeyPurging(aGlodaIds,
                                                         aDestFolder) {
    let destFolderID = this._mapFolder(aDestFolder).id;

    let sqlStr = "UPDATE messages SET folderID = ?1, \
                                      messageKey = ?2 \
                   WHERE id IN (" + aGlodaIds.join(", ") + ")";
    let statement = this._createAsyncStatement(sqlStr, true);
    statement.bindInt64Parameter(0, destFolderID);
    statement.bindNullParameter(1);
    statement.executeAsync(this.trackAsync());
    statement.finalize();

    let cached =
      GlodaCollectionManager.cacheLookupManyList(GlodaMessage.prototype.NOUN_ID,
                                                 aGlodaIds);
    for each (let [, glodaMsg] in Iterator(cached)) {
      glodaMsg._folderID = destFolderID;
      glodaMsg._messageKey = null;
    }
  },

  _messageFromRow: function gloda_ds_messageFromRow(aRow) {
    let folderId, messageKey, date, jsonText, subject, indexedBodyText,
        attachmentNames;
    if (aRow.getTypeOfIndex(1) == Ci.mozIStorageValueArray.VALUE_TYPE_NULL)
      folderId = null;
    else
      folderId = aRow.getInt64(1);
    if (aRow.getTypeOfIndex(2) == Ci.mozIStorageValueArray.VALUE_TYPE_NULL)
      messageKey = null;
    else
      messageKey = aRow.getInt64(2);
    if (aRow.getTypeOfIndex(4) == Ci.mozIStorageValueArray.VALUE_TYPE_NULL)
      date = null;
    else
      date = new Date(aRow.getInt64(4) / 1000);
    if (aRow.getTypeOfIndex(7) == Ci.mozIStorageValueArray.VALUE_TYPE_NULL)
      jsonText = undefined;
    else
      jsonText = aRow.getString(7);
    // only queryFromQuery queries will have these columns
    if (aRow.numEntries >= 14) {
      if (aRow.getTypeOfIndex(10) == Ci.mozIStorageValueArray.VALUE_TYPE_NULL)
        subject = undefined;
      else
        subject = aRow.getString(10);
      if (aRow.getTypeOfIndex(9) == Ci.mozIStorageValueArray.VALUE_TYPE_NULL)
        indexedBodyText = undefined;
      else
        indexedBodyText = aRow.getString(9);
      if (aRow.getTypeOfIndex(11) == Ci.mozIStorageValueArray.VALUE_TYPE_NULL)
        attachmentNames = null;
      else {
        attachmentNames = aRow.getString(11);
        if (attachmentNames)
          attachmentNames = attachmentNames.split("\n");
        else
          attachmentNames = null;
      }
      // we ignore 12, author
      // we ignore 13, recipients
    }
    return new GlodaMessage(this, aRow.getInt64(0), folderId, messageKey,
                            aRow.getInt64(3), null, date, aRow.getString(5),
                            aRow.getInt64(6), jsonText, aRow.getInt64(8),
                            subject, indexedBodyText, attachmentNames);
  },

  get _updateMessagesMarkDeletedByFolderID() {
    // When marking deleted clear the folderID and messageKey so that the
    //  indexing process can reuse it without any location constraints.
    let statement = this._createAsyncStatement(
      "UPDATE messages SET folderID = NULL, messageKey = NULL, \
              deleted = 1 WHERE folderID = ?1");
    this.__defineGetter__("_updateMessagesMarkDeletedByFolderID",
      function() statement);
    return this._updateMessagesMarkDeletedByFolderID;
  },

  /**
   * Efficiently mark all the messages in a folder as deleted.  Unfortunately,
   *  we obviously do not know the id's of the messages affected by this which
   *  complicates in-memory updates.  The options are sending out to the SQL
   *  database for a list of the message id's or some form of in-memory
   *  traversal.  I/O costs being what they are, users having a propensity to
   *  have folders with tens of thousands of messages, and the unlikeliness
   *  of all of those messages being gloda-memory-resident, we go with the
   *  in-memory traversal.
   */
  markMessagesDeletedByFolderID:
      function gloda_ds_markMessagesDeletedByFolderID(aFolderID) {
    let statement = this._updateMessagesMarkDeletedByFolderID;
    statement.bindInt64Parameter(0, aFolderID);
    statement.executeAsync(this.trackAsync());

    // Have the collection manager generate itemsRemoved events for any
    //  in-memory messages in that folder.
    GlodaCollectionManager.itemsDeletedByAttribute(
      GlodaMessage.prototype.NOUN_ID,
      function(aMsg) aMsg._folderID == aFolderID);
  },

  /**
   * Mark all the gloda messages as deleted blind-fire.  Check if any of the
   *  messages are known to the collection manager and update them to be deleted
   *  along with the requisite collection notifications.
   */
  markMessagesDeletedByIDs: function gloda_ds_markMessagesDeletedByIDs(
      aMessageIDs) {
    // When marking deleted clear the folderID and messageKey so that the
    //  indexing process can reuse it without any location constraints.
    let sqlString = "UPDATE messages SET folderID = NULL, messageKey = NULL, " +
                     "deleted = 1 WHERE id IN (" +
                     aMessageIDs.join(",") + ")";

    let statement = this._createAsyncStatement(sqlString, true);
    statement.executeAsync(this.trackAsync());
    statement.finalize();

    GlodaCollectionManager.itemsDeleted(GlodaMessage.prototype.NOUN_ID,
                                        aMessageIDs);
  },

  get _countDeletedMessagesStatement() {
    let statement = this._createAsyncStatement(
      "SELECT COUNT(*) FROM messages WHERE deleted = 1");
    this.__defineGetter__("_countDeletedMessagesStatement",
                          function() statement);
    return this._countDeletedMessagesStatement;
  },

  /**
   * Count how many messages are currently marked as deleted in the database.
   */
  countDeletedMessages: function gloda_ds_countDeletedMessages(aCallback) {
    let cms = this._countDeletedMessagesStatement;
    cms.executeAsync(new SingletonResultValueHandler(aCallback));
  },

  get _deleteMessageByIDStatement() {
    let statement = this._createAsyncStatement(
      "DELETE FROM messages WHERE id = ?1");
    this.__defineGetter__("_deleteMessageByIDStatement",
                          function() statement);
    return this._deleteMessageByIDStatement;
  },

  get _deleteMessageTextByIDStatement() {
    let statement = this._createAsyncStatement(
      "DELETE FROM messagesText WHERE docid = ?1");
    this.__defineGetter__("_deleteMessageTextByIDStatement",
                          function() statement);
    return this._deleteMessageTextByIDStatement;
  },

  /**
   * Delete a message and its fulltext from the database.  It is assumed that
   *  the message was already marked as deleted and so is not visible to the
   *  collection manager and so nothing needs to be done about that.
   */
  deleteMessageByID: function gloda_ds_deleteMessageByID(aMessageID) {
    let dmbids = this._deleteMessageByIDStatement;
    dmbids.bindInt64Parameter(0, aMessageID);
    dmbids.executeAsync(this.trackAsync());

    this.deleteMessageTextByID(aMessageID);
  },

  deleteMessageTextByID: function gloda_ds_deleteMessageTextByID(aMessageID) {
    let dmt = this._deleteMessageTextByIDStatement;
    dmt.bindInt64Parameter(0, aMessageID);
    dmt.executeAsync(this.trackAsync());
  },

  get _folderCompactionStatement() {
    let statement = this._createAsyncStatement(
      "SELECT id, messageKey, headerMessageID FROM messages \
        WHERE folderID = ?1 AND \
          messageKey >= ?2 AND +deleted = 0 ORDER BY messageKey LIMIT ?3");
    this.__defineGetter__("_folderCompactionStatement",
                          function() statement);
    return this._folderCompactionStatement;
  },

  folderCompactionPassBlockFetch:
      function gloda_ds_folderCompactionPassBlockFetch(
        aFolderID, aStartingMessageKey, aLimit, aCallback) {
    let fcs = this._folderCompactionStatement;
    fcs.bindInt64Parameter(0, aFolderID);
    fcs.bindInt64Parameter(1, aStartingMessageKey);
    fcs.bindInt64Parameter(2, aLimit);
    fcs.executeAsync(new CompactionBlockFetcherHandler(aCallback));
  },

  /* ********** Message Attributes ********** */
  get _insertMessageAttributeStatement() {
    let statement = this._createAsyncStatement(
      "INSERT INTO messageAttributes (conversationID, messageID, attributeID, \
                             value) \
              VALUES (?1, ?2, ?3, ?4)");
    this.__defineGetter__("_insertMessageAttributeStatement",
      function() statement);
    return this._insertMessageAttributeStatement;
  },

  get _deleteMessageAttributeStatement() {
    let statement = this._createAsyncStatement(
      "DELETE FROM messageAttributes WHERE attributeID = ?1 AND value = ?2 \
         AND conversationID = ?3 AND messageID = ?4");
    this.__defineGetter__("_deleteMessageAttributeStatement",
      function() statement);
    return this._deleteMessageAttributeStatement;
  },

  /**
   * Insert and remove attributes relating to a GlodaMessage.  This is performed
   *  inside a pseudo-transaction (we create one if we aren't in one, using
   *  our _beginTransaction wrapper, but if we are in one, no additional
   *  meaningful semantics are added).
   * No attempt is made to verify uniqueness of inserted attributes, either
   *  against the current database or within the provided list of attributes.
   *  The caller is responsible for ensuring that unwanted duplicates are
   *  avoided.
   *
   * @param aMessage The GlodaMessage the attributes belong to.  This is used
   *     to provide the message id and conversation id.
   * @param aAddDBAttributes A list of attribute tuples to add, where each tuple
   *     contains an attribute ID and a value.  Lest you forget, an attribute ID
   *     corresponds to a row in the attribute definition table.  The attribute
   *     definition table stores the 'parameter' for the attribute, if any.
   *     (Which is to say, our frequent Attribute-Parameter-Value triple has
   *     the Attribute-Parameter part distilled to a single attribute id.)
   * @param aRemoveDBAttributes A list of attribute tuples to remove.
   */
  adjustMessageAttributes: function gloda_ds_adjustMessageAttributes(aMessage,
                                        aAddDBAttributes, aRemoveDBAttributes) {
    let imas = this._insertMessageAttributeStatement;
    let dmas = this._deleteMessageAttributeStatement;
    this._beginTransaction();
    try {
      for (let iAttrib = 0; iAttrib < aAddDBAttributes.length; iAttrib++) {
        let attribValueTuple = aAddDBAttributes[iAttrib];

        imas.bindInt64Parameter(0, aMessage.conversationID);
        imas.bindInt64Parameter(1, aMessage.id);
        imas.bindInt64Parameter(2, attribValueTuple[0]);
        // use 0 instead of null, otherwise the db gets upset.  (and we don't
        //  really care anyways.)
        if (attribValueTuple[1] == null)
          imas.bindInt64Parameter(3, 0);
        else if (Math.floor(attribValueTuple[1]) == attribValueTuple[1])
          imas.bindInt64Parameter(3, attribValueTuple[1]);
        else
          imas.bindDoubleParameter(3, attribValueTuple[1]);
        imas.executeAsync(this.trackAsync());
      }

      for (let iAttrib = 0; iAttrib < aRemoveDBAttributes.length; iAttrib++) {
        let attribValueTuple = aRemoveDBAttributes[iAttrib];

        dmas.bindInt64Parameter(0, attribValueTuple[0]);
        // use 0 instead of null, otherwise the db gets upset.  (and we don't
        //  really care anyways.)
        if (attribValueTuple[1] == null)
          dmas.bindInt64Parameter(1, 0);
        else if (Math.floor(attribValueTuple[1]) == attribValueTuple[1])
          dmas.bindInt64Parameter(1, attribValueTuple[1]);
        else
          dmas.bindDoubleParameter(1, attribValueTuple[1]);
        dmas.bindInt64Parameter(2, aMessage.conversationID);
        dmas.bindInt64Parameter(3, aMessage.id);
        dmas.executeAsync(this.trackAsync());
      }

      this._commitTransaction();
    }
    catch (ex) {
      this._log.error("adjustMessageAttributes:", ex);
      this._rollbackTransaction();
      throw ex;
    }
  },

  get _deleteMessageAttributesByMessageIDStatement() {
    let statement = this._createAsyncStatement(
      "DELETE FROM messageAttributes WHERE messageID = ?1");
    this.__defineGetter__("_deleteMessageAttributesByMessageIDStatement",
      function() statement);
    return this._deleteMessageAttributesByMessageIDStatement;
  },

  /**
   * Clear all the message attributes for a given GlodaMessage.  No changes
   *  are made to the in-memory representation of the message; it is up to the
   *  caller to ensure that it handles things correctly.
   *
   * @param aMessage The GlodaMessage whose database attributes should be
   *     purged.
   */
  clearMessageAttributes: function gloda_ds_clearMessageAttributes(aMessage) {
    if (aMessage.id != null) {
      this._deleteMessageAttributesByMessageIDStatement.bindInt64Parameter(0,
        aMessage.id);
      this._deleteMessageAttributesByMessageIDStatement.executeAsync(
        this.trackAsync());
    }
  },

  _stringSQLQuoter: function(aString) {
    return "'" + aString.replace("'", "''", "g") + "'";
  },
  _numberQuoter: function(aNum) {
    return aNum;
  },

  /* ===== Generic Attribute Support ===== */
  adjustAttributes: function gloda_ds_adjustAttributes(aItem, aAddDBAttributes,
      aRemoveDBAttributes) {
    let nounDef = aItem.NOUN_DEF;
    let dbMeta = nounDef._dbMeta;
    if (dbMeta.insertAttrStatement === undefined) {
      dbMeta.insertAttrStatement = this._createAsyncStatement(
        "INSERT INTO " + nounDef.attrTableName +
        " (" + nounDef.attrIDColumnName + ", attributeID, value) " +
        " VALUES (?1, ?2, ?3)");
      // we always create this at the same time (right here), no need to check
      dbMeta.deleteAttrStatement = this._createAsyncStatement(
        "DELETE FROM " + nounDef.attrTableName + " WHERE " +
        " attributeID = ?1 AND value = ?2 AND " +
        nounDef.attrIDColumnName + " = ?3");
    }

    let ias = dbMeta.insertAttrStatement;
    let das = dbMeta.deleteAttrStatement;
    this._beginTransaction();
    try {
      for (let iAttr = 0; iAttr < aAddDBAttributes.length; iAttr++) {
        let attribValueTuple = aAddDBAttributes[iAttr];

        ias.bindInt64Parameter(0, aItem.id);
        ias.bindInt64Parameter(1, attribValueTuple[0]);
        // use 0 instead of null, otherwise the db gets upset.  (and we don't
        //  really care anyways.)
        if (attribValueTuple[1] == null)
          ias.bindInt64Parameter(2, 0);
        else if (Math.floor(attribValueTuple[1]) == attribValueTuple[1])
          ias.bindInt64Parameter(2, attribValueTuple[1]);
        else
          ias.bindDoubleParameter(2, attribValueTuple[1]);
        ias.executeAsync(this.trackAsync());
      }

      for (let iAttr = 0; iAttr < aRemoveDBAttributes.length; iAttr++) {
        let attribValueTuple = aRemoveDBAttributes[iAttr];

        das.bindInt64Parameter(0, attribValueTuple[0]);
        // use 0 instead of null, otherwise the db gets upset.  (and we don't
        //  really care anyways.)
        if (attribValueTuple[1] == null)
          das.bindInt64Parameter(1, 0);
        else if (Math.floor(attribValueTuple[1]) == attribValueTuple[1])
          das.bindInt64Parameter(1, attribValueTuple[1]);
        else
          das.bindDoubleParameter(1, attribValueTuple[1]);
        das.bindInt64Parameter(2, aItem.id);
        das.executeAsync(this.trackAsync());
      }

      this._commitTransaction();
    }
    catch (ex) {
      this._log.error("adjustAttributes:", ex);
      this._rollbackTransaction();
      throw ex;
    }
  },

  clearAttributes: function gloda_ds_clearAttributes(aItem) {
    let nounDef = aItem.NOUN_DEF;
    let dbMeta = nounMeta._dbMeta;
    if (dbMeta.clearAttrStatement === undefined) {
      dbMeta.clearAttrStatement = this._createAsyncStatement(
        "DELETE FROM " + nounDef.attrTableName + " WHERE " +
        nounDef.attrIDColumnName + " = ?1");
    }

    if (aItem.id != null) {
      dbMeta.clearAttrStatement.bindInt64Parameter(0, aItem.id);
      dbMeta.clearAttrStatement.executeAsync(this.trackAsync());
    }
  },

  /**
   * escapeStringForLIKE is only available on statements, and sometimes we want
   *  to use it before we create our statement, so we create a statement just
   *  for this reason.
   */
  get _escapeLikeStatement() {
    let statement = this._createAsyncStatement("SELECT 0");
    this.__defineGetter__("_escapeLikeStatement", function() statement);
    return this._escapeLikeStatement;
  },

  _convertToDBValuesAndGroupByAttributeID:
    function gloda_ds__convertToDBValuesAndGroupByAttributeID(aAttrDef,
                                                              aValues) {
    let objectNounDef = aAttrDef.objectNounDef;
    if (!objectNounDef.usesParameter) {
      let dbValues = [];
      for (let iValue = 0; iValue < aValues.length; iValue++) {
        let value = aValues[iValue];
        // If the empty set is significant and it's an empty signifier, emit
        //  the appropriate dbvalue.
        if (value == null && aAttrDef.emptySetIsSignificant) {
          yield [this.kEmptySetAttrId, [aAttrDef.id]];
          // Bail if the only value was us; we don't want to add a
          //  value-posessing wildcard into the mix.
          if (aValues.length == 1)
            return;
          continue;
        }
        let dbValue = objectNounDef.toParamAndValue(value)[1];
        if (dbValue != null)
          dbValues.push(dbValue);
      }
      yield [aAttrDef.special ? undefined : aAttrDef.id, dbValues];
      return;
    }

    let curParam, attrID, dbValues;
    let attrDBDef = aAttrDef.dbDef;
    for (let iValue = 0; iValue < aValues.length; iValue++) {
      let value = aValues[iValue];
      // If the empty set is significant and it's an empty signifier, emit
      //  the appropriate dbvalue.
      if (value == null && aAttrDef.emptySetIsSignificant) {
        yield [this.kEmptySetAttrId, [aAttrDef.id]];
        // Bail if the only value was us; we don't want to add a
        //  value-posessing wildcard into the mix.
        if (aValues.length == 1)
          return;
        continue;
      }
      let [dbParam, dbValue] = objectNounDef.toParamAndValue(value);
      if (curParam === undefined) {
        curParam = dbParam;
        attrID = attrDBDef.bindParameter(curParam);
        if (dbValue != null)
          dbValues = [dbValue];
        else
          dbValues = [];
      }
      else if (curParam == dbParam) {
        if (dbValue != null)
          dbValues.push(dbValue);
      }
      else {
        yield [attrID, dbValues];
        curParam = dbParam;
        attrID = attrDBDef.bindParameter(curParam);
        if (dbValue != null)
          dbValues = [dbValue];
        else
          dbValues = [];
      }
    }
    if (dbValues !== undefined)
      yield [attrID, dbValues];
  },

  _convertRangesToDBStringsAndGroupByAttributeID:
    function gloda_ds__convertRangesToDBStringsAndGroupByAttributeID(aAttrDef,
      aValues, aValueColumnName) {
    let objectNounDef = aAttrDef.objectNounDef;
    if (!objectNounDef.usesParameter) {
      let dbStrings = [];
      for (let iValue = 0; iValue < aValues.length; iValue++) {
        let [lowerVal, upperVal] = aValues[iValue];
        // they both can't be null.  that is the law.
        if (lowerVal == null)
          dbStrings.push(aValueColumnName + " <= " +
                         objectNounDef.toParamAndValue(upperVal)[1]);
        else if (upperVal == null)
          dbStrings.push(aValueColumnName + " >= " +
                         objectNounDef.toParamAndValue(lowerVal)[1]);
        else // no one is null!
          dbStrings.push(aValueColumnName + " BETWEEN " +
                         objectNounDef.toParamAndValue(lowerVal)[1] + " AND " +
                         objectNounDef.toParamAndValue(upperVal)[1]);
      }
      yield [aAttrDef.special ? undefined : aAttrDef.id, dbStrings];
      return;
    }

    let curParam, attrID, dbStrings;
    let attrDBDef = aAttrDef.dbDef;
    for (let iValue = 0; iValue < aValues.length; iValue++) {
      let [lowerVal, upperVal] = aValues[iValue];

      let dbString, dbParam, lowerDBVal, upperDBVal;
      // they both can't be null.  that is the law.
      if (lowerVal == null) {
        [dbParam, upperDBVal] = objectNounDef.toParamAndValue(upperVal);
        dbString = aValueColumnName + " <= " + upperDBVal;
      }
      else if (upperVal == null) {
        [dbParam, lowerDBVal] = objectNounDef.toParamAndValue(lowerVal);
        dbString = aValueColumnName + " >= " + lowerDBVal;
      }
      else { // no one is null!
        [dbParam, lowerDBVal] = objectNounDef.toParamAndValue(lowerVal);
        dbString = aValueColumnName + " BETWEEN " + lowerDBVal + " AND " +
                   objectNounDef.toParamAndValue(upperVal)[1];
      }

      if (curParam === undefined) {
        curParam = dbParam;
        attrID = attrDBDef.bindParameter(curParam);
        dbStrings = [dbString];
      }
      else if (curParam === dbParam) {
        dbStrings.push(dbString);
      }
      else {
        yield [attrID, dbStrings];
        curParam = dbParam;
        attrID = attrDBDef.bindParameter(curParam);
        dbStrings = [dbString];
      }
    }
    if (dbStrings !== undefined)
      yield [attrID, dbStrings];
  },

  /**
   * Perform a database query given a GlodaQueryClass instance that specifies
   *  a set of constraints relating to the noun type associated with the query.
   *  A GlodaCollection is returned containing the results of the look-up.
   *  By default the collection is "live", and will mutate (generating events to
   *  its listener) as the state of the database changes.
   * This functionality is made user/extension visible by the Query's
   *  getCollection (asynchronous).
   *
   * @param [aArgs] See |GlodaQuery.getCollection| for info.
   */
  queryFromQuery: function gloda_ds_queryFromQuery(aQuery, aListener,
      aListenerData, aExistingCollection, aMasterCollection, aArgs) {
    // when changing this method, be sure that GlodaQuery's testMatch function
    //  likewise has its changes made.
    let nounDef = aQuery._nounDef;

    let whereClauses = [];
    let unionQueries = [aQuery].concat(aQuery._unions);
    let boundArgs = [];

    // Use the dbQueryValidityConstraintSuffix to provide constraints that
    //  filter items down to those that are valid for the query mechanism to
    //  return.  For example, in the case of messages, deleted or ghost
    //  messages should not be returned by this query layer.  We require
    //  hand-rolled SQL to do that for now.
    let validityConstraintSuffix;
    if (nounDef.dbQueryValidityConstraintSuffix &&
        !aQuery.options.noDbQueryValidityConstraints)
      validityConstraintSuffix = nounDef.dbQueryValidityConstraintSuffix;
    else
      validityConstraintSuffix = "";

    for (let iUnion = 0; iUnion < unionQueries.length; iUnion++) {
      let curQuery = unionQueries[iUnion];
      let selects = [];

      let lastConstraintWasSpecial = false;
      let curConstraintIsSpecial;

      for (let iConstraint = 0; iConstraint < curQuery._constraints.length;
           iConstraint++) {
        let constraint = curQuery._constraints[iConstraint];
        let [constraintType, attrDef] = constraint;
        let constraintValues = constraint.slice(2);

        let tableName, idColumnName, tableColumnName, valueColumnName;
        if (constraintType == this.kConstraintIdIn) {
          // we don't need any of the next cases' setup code, and we especially
          //  would prefer that attrDef isn't accessed since it's null for us.
        }
        else if (attrDef.special) {
          tableName = nounDef.tableName;
          idColumnName = "id"; // canonical id for a table is "id".
          valueColumnName = attrDef.specialColumnName;
          curConstraintIsSpecial = true;
        }
        else {
          tableName = nounDef.attrTableName;
          idColumnName = nounDef.attrIDColumnName;
          valueColumnName = "value";
          curConstraintIsSpecial = false;
        }

        let select = null, test = null, bindArgs = null;
        if (constraintType === this.kConstraintIdIn) {
          // this is somewhat of a trick.  this does mean that this can be the
          //  only constraint.  Namely, our idiom is:
          // SELECT * FROM blah WHERE id IN (a INTERSECT b INTERSECT c)
          //  but if we only have 'a', then that becomes "...IN (a)", and if
          //  'a' is not a select but a list of id's... tricky, no?
          select = constraintValues.join(",");
        }
        // @testpoint gloda.datastore.sqlgen.kConstraintIn
        else if (constraintType === this.kConstraintIn) {
          let clauses = [];
          for each (let [attrID, values] in
              this._convertToDBValuesAndGroupByAttributeID(attrDef,
                                                           constraintValues)) {
            let clausePart;
            if (attrID !== undefined)
              clausePart = "(attributeID = " + attrID +
                (values.length ? " AND " : "");
            else
              clausePart = "(";
            if (values.length) {
              // strings need to be escaped, we would use ? binding, except
              //  that gets mad if we have too many strings... so we use our
              //  own escaping logic.  correctly escaping is easy, but it still
              //  feels wrong to do it. (just double the quote character...)
              if (attrDef.special == this.kSpecialString)
                clausePart += valueColumnName + " IN (" +
                  [("'" + v.replace("'", "''", "g") + "'") for each
                   ([, v] in Iterator(values))].join(",") + "))";
              else
                clausePart += valueColumnName + " IN (" + values.join(",") +
                              "))";
            }
            else
              clausePart += ")";
            clauses.push(clausePart);
          }
          test = clauses.join(" OR ");
        }
        // @testpoint gloda.datastore.sqlgen.kConstraintRanges
        else if (constraintType === this.kConstraintRanges) {
          let clauses = [];
          for each (let [attrID, dbStrings] in
              this._convertRangesToDBStringsAndGroupByAttributeID(attrDef,
                              constraintValues, valueColumnName)) {
            if (attrID !== undefined)
              clauses.push("(attributeID = " + attrID +
                           " AND (" + dbStrings.join(" OR ") + "))");
            else
              clauses.push("(" + dbStrings.join(" OR ") + ")");
          }
          test = clauses.join(" OR ");
        }
        // @testpoint gloda.datastore.sqlgen.kConstraintEquals
        else if (constraintType === this.kConstraintEquals) {
          let clauses = [];
          for each (let [attrID, values] in
              this._convertToDBValuesAndGroupByAttributeID(attrDef,
                                                           constraintValues)) {
            if (attrID !== undefined)
              clauses.push("(attributeID = " + attrID +
                  " AND (" + [valueColumnName + " = ?" for each
                  (value in values)].join(" OR ") + "))");
            else
              clauses.push("(" + [valueColumnName + " = ?" for each
                  (value in values)].join(" OR ") + ")");
            boundArgs.push.apply(boundArgs, values);
          }
          test = clauses.join(" OR ");
        }
        // @testpoint gloda.datastore.sqlgen.kConstraintStringLike
        else if (constraintType === this.kConstraintStringLike) {
          let likePayload = '';
          for each (let [iValuePart, valuePart] in Iterator(constraintValues)) {
            if (typeof valuePart == "string")
              likePayload += this._escapeLikeStatement.escapeStringForLIKE(
                valuePart, "/");
            else
              likePayload += "%";
          }
          test = valueColumnName + " LIKE ? ESCAPE '/'";
          boundArgs.push(likePayload);
        }
        // @testpoint gloda.datastore.sqlgen.kConstraintFulltext
        else if (constraintType === this.kConstraintFulltext) {
          let matchStr = constraintValues[0];
          select = "SELECT docid FROM " + nounDef.tableName + "Text" +
            " WHERE " + attrDef.specialColumnName + " MATCH ?";
          boundArgs.push(matchStr);
        }

        if (curConstraintIsSpecial && lastConstraintWasSpecial && test) {
          selects[selects.length-1] += " AND " + test;
        }
        else if (select)
          selects.push(select);
        else if (test) {
          select = "SELECT " + idColumnName + " FROM " + tableName + " WHERE " +
              test;
          selects.push(select);
        }
        else
          this._log.warn("Unable to translate constraint of type " +
            constraintType + " on attribute bound as " + nounDef.name);

        lastConstraintWasSpecial = curConstraintIsSpecial;
      }

      if (selects.length)
        whereClauses.push("id IN (" + selects.join(" INTERSECT ") + ")" +
                          validityConstraintSuffix);
    }

    let sqlString = "SELECT * FROM " + nounDef.tableName;
    if (!aQuery.options.noMagic) {
      if (aQuery.options.noDbQueryValidityConstraints &&
          nounDef.dbQueryJoinMagicWithNoValidityConstraints)
        sqlString += nounDef.dbQueryJoinMagicWithNoValidityConstraints;
      else if (nounDef.dbQueryJoinMagic)
        sqlString += nounDef.dbQueryJoinMagic;
    }

    if (whereClauses.length)
      sqlString += " WHERE (" + whereClauses.join(") OR (") + ")";

    if (aQuery.options.explicitSQL)
      sqlString = aQuery.options.explicitSQL;

    if (aQuery.options.outerWrapColumns)
      sqlString = "SELECT *, " + aQuery.options.outerWrapColumns.join(", ") +
                  " FROM (" + sqlString + ")";

    if (aQuery._order.length) {
      let orderClauses = [];
      for (let [, colName] in Iterator(aQuery._order)) {
         if (colName[0] == "-")
           orderClauses.push(colName.substring(1) + " DESC");
         else
           orderClauses.push(colName + " ASC");
      }
      sqlString += " ORDER BY " + orderClauses.join(", ");
    }

    if (aQuery._limit) {
      if (!("limitClauseAlreadyIncluded" in aQuery.options))
        sqlString += " LIMIT ?";
      boundArgs.push(aQuery._limit);
    }

    this._log.debug("QUERY FROM QUERY: " + sqlString + " ARGS: " + boundArgs);

    // if we want to become explicit, replace the query (which has already
    //  provided our actual SQL query) with an explicit query.  This will be
    //  what gets attached to the collection in the event we create a new
    //  collection.  If we are reusing one, we assume that the explicitness,
    //  if desired, already happened.
    // (we do not need to pass an argument to the explicitQueryClass constructor
    //  because it will be passed in to the collection's constructor, which will
    //  ensure that the collection attribute gets set.)
    if (aArgs && ("becomeExplicit" in aArgs) && aArgs.becomeExplicit)
      aQuery = new nounDef.explicitQueryClass();
    else if (aArgs && ("becomeNull" in aArgs) && aArgs.becomeNull)
      aQuery = new nounDef.nullQueryClass();

    return this._queryFromSQLString(sqlString, boundArgs, nounDef, aQuery,
        aListener, aListenerData, aExistingCollection, aMasterCollection);
  },

  _queryFromSQLString: function gloda_ds__queryFromSQLString(aSqlString,
      aBoundArgs, aNounDef, aQuery, aListener, aListenerData,
      aExistingCollection, aMasterCollection) {
    let statement = this._createAsyncStatement(aSqlString, true);
    for (let [iBinding, bindingValue] in Iterator(aBoundArgs)) {
      this._bindVariant(statement, iBinding, bindingValue);
    }

    let collection;
    if (aExistingCollection)
      collection = aExistingCollection;
    else {
      collection = new GlodaCollection(aNounDef, [], aQuery, aListener,
                                       aMasterCollection);
      GlodaCollectionManager.registerCollection(collection);
      // we don't want to overwrite the existing listener or its data, but this
      //  does raise the question about what should happen if we get passed in
      //  a different listener and/or data.
      if (aListenerData !== undefined)
        collection.data = aListenerData;
    }
    if (aListenerData) {
      if (collection.dataStack)
        collection.dataStack.push(aListenerData);
      else
        collection.dataStack = [aListenerData];
    }

    statement.executeAsync(new QueryFromQueryCallback(statement, aNounDef,
      collection));
    statement.finalize();
    return collection;
  },

  /**
   *
   *
   */
  loadNounItem: function gloda_ds_loadNounItem(aItem, aReferencesByNounID,
      aInverseReferencesByNounID) {
    let attribIDToDBDefAndParam = this._attributeIDToDBDefAndParam;

    let hadDeps = aItem._deps != null;
    let deps = aItem._deps || {};
    let hasDeps = false;

    //this._log.debug("  hadDeps: " + hadDeps + " deps: " +
    //    Log4Moz.enumerateProperties(deps).join(","));

    for each (let [, attrib] in Iterator(aItem.NOUN_DEF.specialLoadAttribs)) {
      let objectNounDef = attrib.objectNounDef;

      if (attrib.special === this.kSpecialColumnChildren) {
        let invReferences = aInverseReferencesByNounID[objectNounDef.id];
        if (invReferences === undefined)
          invReferences = aInverseReferencesByNounID[objectNounDef.id] = {};
        // only contribute if it's not already pending or there
        if (!(attrib.id in deps) && aItem[attrib.storageAttributeName] == null){
          //this._log.debug("   Adding inv ref for: " + aItem.id);
          if (!(aItem.id in invReferences))
            invReferences[aItem.id] = null;
          deps[attrib.id] = null;
          hasDeps = true;
        }
      }
      else if (attrib.special === this.kSpecialColumnParent) {
        let references = aReferencesByNounID[objectNounDef.id];
        if (references === undefined)
          references = aReferencesByNounID[objectNounDef.id] = {};
        // nothing to contribute if it's already there
        if (!(attrib.id in deps) &&
            aItem[attrib.valueStorageAttributeName] == null) {
          let parentID = aItem[attrib.idStorageAttributeName];
          if (!(parentID in references))
            references[parentID] = null;
          //this._log.debug("   Adding parent ref for: " +
          //  aItem[attrib.idStorageAttributeName]);
          deps[attrib.id] = null;
          hasDeps = true;
        }
        else {
          this._log.debug("  paranoia value storage: " + aItem[attrib.valueStorageAttributeName]);
        }
      }
    }

    // bail here if arbitrary values are not allowed, there just is no
    //  encoded json, or we already had dependencies for this guy, implying
    //  the json pass has already been performed
    if (!aItem.NOUN_DEF.allowsArbitraryAttrs || !aItem._jsonText || hadDeps) {
      if (hasDeps)
        aItem._deps = deps;
      return hasDeps;
    }

    //this._log.debug(" load json: " + aItem._jsonText);
    let jsonDict = JSON.parse(aItem._jsonText);
    delete aItem._jsonText;

    // Iterate over the attributes on the item
    for each (let [attribId, jsonValue] in Iterator(jsonDict)) {
      // It is technically impossible for attribute ids to go away at this
      //  point in time.  This would require someone to monkey around with
      //  our schema.  But we will introduce this functionality one day, so
      //  prepare for it now.
      if (!(attribId in attribIDToDBDefAndParam))
        continue;
      // find the attribute definition that corresponds to this key
      let dbAttrib = attribIDToDBDefAndParam[attribId][0];

      let attrib = dbAttrib.attrDef;
      // The attribute definition will fail to exist if no one defines the
      //  attribute anymore.  This can happen for many reasons: an extension
      //  was uninstalled, an extension was changed and no longer defines the
      //  attribute, or patches are being applied/unapplied.  Ignore this
      //  attribute if missing.
      if (attrib == null)
        continue;
      let objectNounDef = attrib.objectNounDef;

      // If it has a tableName member but no fromJSON, then it's a persistent
      //  object that needs to be loaded, which also means we need to hold it in
      //  a collection owned by our collection.
      // (If it has a fromJSON method, then it's a special case like
      //  MimeTypeNoun where it is authoritatively backed by a table but caches
      //  everything into memory.  There is no case where fromJSON would be
      //  implemented but we should still be doing database lookups.)
      if (objectNounDef.tableName && !objectNounDef.fromJSON) {
        let references = aReferencesByNounID[objectNounDef.id];
        if (references === undefined)
          references = aReferencesByNounID[objectNounDef.id] = {};

        if (attrib.singular) {
          if (!(jsonValue in references))
            references[jsonValue] = null;
        }
        else {
          for each (let [, anID] in Iterator(jsonValue)) {
            if (!(anID in references))
            references[anID] = null;
          }
        }

        deps[attribId] = jsonValue;
        hasDeps = true;
      }
      /* if it has custom contribution logic, use it */
      else if (objectNounDef.contributeObjDependencies) {
        if (objectNounDef.contributeObjDependencies(jsonValue,
                             aReferencesByNounID, aInverseReferencesByNounID)) {
          deps[attribId] = jsonValue;
          hasDeps = true;
        }
        else // just propagate the value, it's some form of simple sentinel
          aItem[attrib.boundName] = jsonValue;
      }
      // otherwise, the value just needs to be de-persisted, or...
      else if (objectNounDef.fromJSON) {
        if (attrib.singular) {
          // For consistency with the non-singular case, we don't assign the
          //  attribute if undefined is returned.
          let deserialized = objectNounDef.fromJSON(jsonValue, aItem);
          if (deserialized !== undefined)
            aItem[attrib.boundName] = deserialized;
        }
        else {
          // Convert all the entries in the list filtering out any undefined
          //  values. (TagNoun will do this if the tag is now dead.)
          let outList = [];
          for each (let [, val] in Iterator(jsonValue)) {
            let deserialized = objectNounDef.fromJSON(val, aItem);
            if (deserialized !== undefined)
              outList.push(deserialized);
          }
          // Note: It's possible if we filtered things out that this is an empty
          //  list.  This is acceptable because this is somewhat of an unusual
          //  case and I don't think we want to further complicate our
          //  semantics.
          aItem[attrib.boundName] = outList;
        }
      }
      // it's fine as is
      else
        aItem[attrib.boundName] = jsonValue;
    }

    if (hasDeps)
      aItem._deps = deps;
    return hasDeps;
  },

  loadNounDeferredDeps: function gloda_ds_loadNounDeferredDeps(aItem,
      aReferencesByNounID, aInverseReferencesByNounID) {
    if (aItem._deps === undefined)
      return;

    //this._log.debug("  loading deferred, deps: " +
    //    Log4Moz.enumerateProperties(aItem._deps).join(","));


    let attribIDToDBDefAndParam = this._attributeIDToDBDefAndParam;

    for (let [attribId, jsonValue] in Iterator(aItem._deps)) {
      let dbAttrib = attribIDToDBDefAndParam[attribId][0];
      let attrib = dbAttrib.attrDef;

      let objectNounDef = attrib.objectNounDef;
      let references = aReferencesByNounID[objectNounDef.id];
      if (attrib.special) {
        if (attrib.special === this.kSpecialColumnChildren) {
          let inverseReferences = aInverseReferencesByNounID[objectNounDef.id];
          //this._log.info("inverse assignment: " + objectNounDef.id +
          //    " of " + aItem.id)
          aItem[attrib.storageAttributeName] = inverseReferences[aItem.id];
        }
        else if (attrib.special === this.kSpecialColumnParent) {
          //this._log.info("parent column load: " + objectNounDef.id +
          //    " storage value: " + aItem[attrib.idStorageAttributeName]);
          aItem[attrib.valueStorageAttributeName] =
            references[aItem[attrib.idStorageAttributeName]];
        }
      }
      else if (objectNounDef.tableName) {
        //this._log.info("trying to load: " + objectNounDef.id + " refs: " +
        //    jsonValue + ": " + Log4Moz.enumerateProperties(jsonValue).join(","));
        if (attrib.singular)
          aItem[attrib.boundName] = references[jsonValue];
        else
          aItem[attrib.boundName] = [references[val] for each
                                     ([, val] in Iterator(jsonValue))];
      }
      else if (objectNounDef.contributeObjDependencies) {
        aItem[attrib.boundName] =
          objectNounDef.resolveObjDependencies(jsonValue, aReferencesByNounID,
            aInverseReferencesByNounID);
      }
      // there is no other case
    }

    delete aItem._deps;
  },

  /* ********** Contact ********** */
  _nextContactId: 1,

  _populateContactManagedId: function () {
    let stmt = this._createSyncStatement("SELECT MAX(id) FROM contacts", true);
    if (stmt.executeStep()) {  // no chance of this SQLITE_BUSY on this call
      this._nextContactId = stmt.getInt64(0) + 1;
    }
    stmt.finalize();
  },

  get _insertContactStatement() {
    let statement = this._createAsyncStatement(
      "INSERT INTO contacts (id, directoryUUID, contactUUID, name, popularity,\
                             frecency, jsonAttributes) \
              VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7)");
    this.__defineGetter__("_insertContactStatement", function() statement);
    return this._insertContactStatement;
  },

  createContact: function gloda_ds_createContact(aDirectoryUUID, aContactUUID,
      aName, aPopularity, aFrecency) {
    let contactID = this._nextContactId++;
    let contact = new GlodaContact(this, contactID,
                                   aDirectoryUUID, aContactUUID, aName,
                                   aPopularity, aFrecency);
    return contact;
  },

  insertContact: function gloda_ds_insertContact(aContact) {
    let ics = this._insertContactStatement;
    ics.bindInt64Parameter(0, aContact.id);
    if (aContact.directoryUUID == null)
      ics.bindNullParameter(1);
    else
      ics.bindStringParameter(1, aContact.directoryUUID);
    if (aContact.contactUUID == null)
      ics.bindNullParameter(2);
    else
      ics.bindStringParameter(2, aContact.contactUUID);
    ics.bindStringParameter(3, aContact.name);
    ics.bindInt64Parameter(4, aContact.popularity);
    ics.bindInt64Parameter(5, aContact.frecency);
    if (aContact._jsonText)
      ics.bindStringParameter(6, aContact._jsonText);
    else
      ics.bindNullParameter(6);

    ics.executeAsync(this.trackAsync());

    return aContact;
  },

  get _updateContactStatement() {
    let statement = this._createAsyncStatement(
      "UPDATE contacts SET directoryUUID = ?1, \
                           contactUUID = ?2, \
                           name = ?3, \
                           popularity = ?4, \
                           frecency = ?5, \
                           jsonAttributes = ?6 \
                       WHERE id = ?7");
    this.__defineGetter__("_updateContactStatement", function() statement);
    return this._updateContactStatement;
  },

  updateContact: function gloda_ds_updateContact(aContact) {
    let ucs = this._updateContactStatement;
    ucs.bindInt64Parameter(6, aContact.id);
    ucs.bindStringParameter(0, aContact.directoryUUID);
    ucs.bindStringParameter(1, aContact.contactUUID);
    ucs.bindStringParameter(2, aContact.name);
    ucs.bindInt64Parameter(3, aContact.popularity);
    ucs.bindInt64Parameter(4, aContact.frecency);
    if (aContact._jsonText)
      ucs.bindStringParameter(5, aContact._jsonText);
    else
      ucs.bindNullParameter(5);

    ucs.executeAsync(this.trackAsync());
  },

  _contactFromRow: function gloda_ds_contactFromRow(aRow) {
    let directoryUUID, contactUUID, jsonText;
    if (aRow.getTypeOfIndex(1) == Ci.mozIStorageValueArray.VALUE_TYPE_NULL)
      directoryUUID = null;
    else
      directoryUUID = aRow.getString(1);
    if (aRow.getTypeOfIndex(2) == Ci.mozIStorageValueArray.VALUE_TYPE_NULL)
      contactUUID = null;
    else
      contactUUID = aRow.getString(2);
    if (aRow.getTypeOfIndex(6) == Ci.mozIStorageValueArray.VALUE_TYPE_NULL)
      jsonText = undefined;
    else
      jsonText = aRow.getString(6);

    return new GlodaContact(this, aRow.getInt64(0), directoryUUID,
                            contactUUID, aRow.getString(5),
                            aRow.getInt64(3), aRow.getInt64(4), jsonText);
  },

  get _selectContactByIDStatement() {
    let statement = this._createSyncStatement(
      "SELECT * FROM contacts WHERE id = ?1");
    this.__defineGetter__("_selectContactByIDStatement",
      function() statement);
    return this._selectContactByIDStatement;
  },

  /**
   * Synchronous contact lookup currently only for use by gloda's creation
   *  of the concept of "me".  It is okay for it to be doing synchronous work
   *  because it is part of the startup process before any user code could
   *  have gotten a reference to Gloda, but no one else should do this.
   */
  getContactByID: function gloda_ds_getContactByID(aContactID) {
    let contact = GlodaCollectionManager.cacheLookupOne(
      GlodaContact.prototype.NOUN_ID, aContactID);

    if (contact === null) {
      let scbi = this._selectContactByIDStatement;
      scbi.bindInt64Parameter(0, aContactID);
      if (this._syncStep(scbi)) {
        contact = this._contactFromRow(scbi);
        GlodaCollectionManager.itemLoaded(contact);
      }
      scbi.reset();
    }

    return contact;
  },

  /* ********** Identity ********** */
  /** next identity id, managed for async use reasons. */
  _nextIdentityId: 1,
  _populateIdentityManagedId: function () {
    let stmt = this._createSyncStatement(
      "SELECT MAX(id) FROM identities", true);
    if (stmt.executeStep()) { // no chance of this SQLITE_BUSY on this call
      this._nextIdentityId = stmt.getInt64(0) + 1;
    }
    stmt.finalize();
  },

  get _insertIdentityStatement() {
    let statement = this._createAsyncStatement(
      "INSERT INTO identities (id, contactID, kind, value, description, relay) \
              VALUES (?1, ?2, ?3, ?4, ?5, ?6)");
    this.__defineGetter__("_insertIdentityStatement", function() statement);
    return this._insertIdentityStatement;
  },

  createIdentity: function gloda_ds_createIdentity(aContactID, aContact, aKind,
                                                   aValue, aDescription,
                                                   aIsRelay) {
    let identityID = this._nextIdentityId++;
    let iis = this._insertIdentityStatement;
    iis.bindInt64Parameter(0, identityID);
    iis.bindInt64Parameter(1, aContactID);
    iis.bindStringParameter(2, aKind);
    iis.bindStringParameter(3, aValue);
    iis.bindStringParameter(4, aDescription);
    iis.bindInt64Parameter(5, aIsRelay ? 1 : 0);
    iis.executeAsync(this.trackAsync());

    let identity = new GlodaIdentity(this, identityID,
                                     aContactID, aContact, aKind, aValue,
                                     aDescription, aIsRelay);
    GlodaCollectionManager.itemsAdded(identity.NOUN_ID, [identity]);
    return identity;
  },

  _identityFromRow: function gloda_ds_identityFromRow(aRow) {
    return new GlodaIdentity(this, aRow.getInt64(0), aRow.getInt64(1), null,
                             aRow.getString(2), aRow.getString(3),
                             aRow.getString(4),
                             aRow.getInt32(5) ? true : false);
  },

  get _selectIdentityByKindValueStatement() {
    let statement = this._createSyncStatement(
      "SELECT * FROM identities WHERE kind = ?1 AND value = ?2");
    this.__defineGetter__("_selectIdentityByKindValueStatement",
      function() statement);
    return this._selectIdentityByKindValueStatement;
  },

  /**
   * Synchronous lookup of an identity by kind and value, only for use by
   *  the legacy gloda core code that creates a concept of "me".
   *  Ex: (email, foo@bar.com)
   */
  getIdentity: function gloda_ds_getIdentity(aKind, aValue) {
    let identity = GlodaCollectionManager.cacheLookupOneByUniqueValue(
      GlodaIdentity.prototype.NOUN_ID, aKind + "@" + aValue);

    let ibkv = this._selectIdentityByKindValueStatement;
    ibkv.bindStringParameter(0, aKind);
    ibkv.bindStringParameter(1, aValue);
    if (this._syncStep(ibkv)) {
      identity = this._identityFromRow(ibkv);
      GlodaCollectionManager.itemLoaded(identity);
    }
    ibkv.reset();

    return identity;
  },
};
GlodaAttributeDBDef.prototype._datastore = GlodaDatastore;
GlodaConversation.prototype._datastore = GlodaDatastore;
GlodaFolder.prototype._datastore = GlodaDatastore;
GlodaMessage.prototype._datastore = GlodaDatastore;
GlodaContact.prototype._datastore = GlodaDatastore;
GlodaIdentity.prototype._datastore = GlodaDatastore;
