/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This file tests our indexing prowess.  This includes both our ability to
 *  properly be triggered by events taking place in thunderbird as well as our
 *  ability to correctly extract/index the right data.
 * In general, if these tests pass, things are probably working quite well.
 *
 * This test has local, IMAP online, IMAP offline, and IMAP online-become-offline
 *  variants.  See the text_index_messages_*.js files.
 *
 * Things we don't test that you think we might test:
 * - Full-text search.  Happens in query testing.
 */

load("resources/glodaTestHelper.js");

Components.utils.import("resource:///modules/MailUtils.js");
Components.utils.import("resource://gre/modules/NetUtil.jsm");

// Whether we can expect fulltext results
var expectFulltextResults = true;

/**
 * Should we force our folders offline after we have indexed them once.  We do
 * this in the online_to_offline test variant.
 */
var goOffline = false;

/* ===== Indexing Basics ===== */

/**
 * Index a message, wait for a commit, make sure the header gets the property
 *  set correctly.  Then modify the message, verify the dirty property shows
 *  up, flush again, and make sure the dirty property goes clean again.
 */
function test_pending_commit_tracker_flushes_correctly() {
  let [folder, msgSet] = make_folder_with_sets([{count: 1}]);
  yield wait_for_message_injection();
  yield wait_for_gloda_indexer(msgSet, {augment: true});

  // before the flush, there should be no gloda-id property
  let msgHdr = msgSet.getMsgHdr(0);
  // get it as a string to make sure it's empty rather than possessing a value
  do_check_eq(msgHdr.getStringProperty("gloda-id"), "");

  yield wait_for_gloda_db_flush();

  // after the flush there should be a gloda-id property and it should
  //  equal the gloda id
  let gmsg = msgSet.glodaMessages[0];
  do_check_eq(msgHdr.getUint32Property("gloda-id"), gmsg.id);

  // make sure no dirty property was written...
  do_check_eq(msgHdr.getStringProperty("gloda-dirty"), "");

  // modify the message
  msgSet.setRead(true);
  yield wait_for_gloda_indexer(msgSet);

  // now there should be a dirty property and it should be 1...
  do_check_eq(msgHdr.getUint32Property("gloda-dirty"),
              GlodaMsgIndexer.kMessageDirty);

  // flush
  yield wait_for_gloda_db_flush();

  // now dirty should be 0 and the gloda id should still be the same
  do_check_eq(msgHdr.getUint32Property("gloda-dirty"),
              GlodaMsgIndexer.kMessageClean);
  do_check_eq(msgHdr.getUint32Property("gloda-id"), gmsg.id);
}

/**
 * Make sure that PendingCommitTracker causes a msgdb commit to occur so that
 *  if the nsIMsgFolder's msgDatabase attribute has already been nulled
 *  (which is normally how we force a msgdb commit), that the changes to the
 *  header actually hit the disk.
 */
function test_pending_commit_causes_msgdb_commit() {
  // new message, index it
  let [folder, msgSet] = make_folder_with_sets([{count: 1}]);
  yield wait_for_message_injection();
  yield wait_for_gloda_indexer(msgSet, {augment: true});

  // force the msgDatabase closed; the sqlite commit will not yet have occurred
  get_real_injection_folder(folder).msgDatabase = null;
  // make the commit happen, this causes the header to get set.
  yield wait_for_gloda_db_flush();
  // Force a GC.  this will kill off the header and the database, losing data
  //  if we are not protecting it.
  Components.utils.forceGC();

  // now retrieve the header and make sure it has the gloda id set!
  let msgHdr = msgSet.getMsgHdr(0);
  do_check_eq(msgHdr.getUint32Property("gloda-id"), msgSet.glodaMessages[0].id);
}

/**
 * Give the indexing sweep a workout.
 *
 * This includes:
 * - Basic indexing sweep across never-before-indexed folders.
 * - Indexing sweep across folders with just some changes.
 * - Filthy pass.
 */
function test_indexing_sweep() {
  // -- Never-before-indexed folders
  mark_sub_test_start("never before indexed folders");
  // turn off event-driven indexing
  configure_gloda_indexing({event: false});

  let [folderA, setA1, setA2] = make_folder_with_sets([{count: 3},
                                                       {count: 2}]);
  yield wait_for_message_injection();
  let [folderB, setB1, setB2] = make_folder_with_sets([{count: 3},
                                                       {count: 2}]);
  yield wait_for_message_injection();
  let [folderC, setC1, setC2] = make_folder_with_sets([{count: 3},
                                                       {count: 2}]);
  yield wait_for_message_injection();

  // Make sure that event-driven job gets nuked out of existence
  GlodaIndexer.purgeJobsUsingFilter(function() true);

  // turn on event-driven indexing again; this will trigger a sweep.
  configure_gloda_indexing({event: true});
  GlodaMsgIndexer.indexingSweepNeeded = true;
  yield wait_for_gloda_indexer([setA1, setA2, setB1, setB2, setC1, setC2]);


  // -- Folders with some changes, pending commits
  mark_sub_test_start("folders with some changes, pending commits");
  // indexing off
  configure_gloda_indexing({event: false});

  setA1.setRead(true);
  setB2.setRead(true);

  // indexing on, killing all outstanding jobs, trigger sweep
  GlodaIndexer.purgeJobsUsingFilter(function() true);
  configure_gloda_indexing({event: true});
  GlodaMsgIndexer.indexingSweepNeeded = true;

  yield wait_for_gloda_indexer([setA1, setB2]);


  // -- Folders with some changes, no pending commits
  mark_sub_test_start("folders with some changes, no pending commits");
  // force a commit to clear out our pending commits
  yield wait_for_gloda_db_flush();
  // indexing off
  configure_gloda_indexing({event: false});

  setA2.setRead(true);
  setB1.setRead(true);

  // indexing on, killing all outstanding jobs, trigger sweep
  GlodaIndexer.purgeJobsUsingFilter(function() true);
  configure_gloda_indexing({event: true});
  GlodaMsgIndexer.indexingSweepNeeded = true;

  yield wait_for_gloda_indexer([setA2, setB1]);


  // -- Filthy foldering indexing
  // Just mark the folder filthy and make sure that we reindex everyone.
  // IMPORTANT!  The trick of marking the folder filthy only works because
  //  we flushed/committed the database above; the PendingCommitTracker
  //  is not aware of bogus filthy-marking of folders.
  // We leave the verification of the implementation details to
  //  test_index_sweep_folder.js.
  mark_sub_test_start("filthy folder indexing");
  let glodaFolderC = Gloda.getFolderForFolder(
                       get_real_injection_folder(folderC));
  glodaFolderC._dirtyStatus = glodaFolderC.kFolderFilthy;
  mark_action("actual", "marked gloda folder dirty", [glodaFolderC]);
  GlodaMsgIndexer.indexingSweepNeeded = true;
  yield wait_for_gloda_indexer([setC1, setC2]);

  // -- Forced folder indexing.
  var callbackInvoked = false;
  mark_sub_test_start("forced folder indexing");
  GlodaMsgIndexer.indexFolder(get_real_injection_folder(folderA), {
    force: true,
    callback: function() {
      callbackInvoked = true;
    }});
  yield wait_for_gloda_indexer([setA1, setA2]);
  do_check_true(callbackInvoked);
}


/**
 * We used to screw up and downgrade filthy folders to dirty if we saw an event
 *  happen in the folder before we got to the folder; this tests that we no
 *  longer do that.
 */
function test_event_driven_indexing_does_not_mess_with_filthy_folders() {
  // add a folder with a message.
  let [folder, msgSet] = make_folder_with_sets([{count: 1}]);
  yield wait_for_message_injection();
  yield wait_for_gloda_indexer([msgSet]);

  // fake marking the folder filthy.
  let glodaFolder = Gloda.getFolderForFolder(get_real_injection_folder(folder));
  glodaFolder._dirtyStatus = glodaFolder.kFolderFilthy;

  // generate an event in the folder
  msgSet.setRead(true);
  // make sure the indexer did not do anything and the folder is still filthy.
  yield wait_for_gloda_indexer([]);
  do_check_eq(glodaFolder._dirtyStatus, glodaFolder.kFolderFilthy);
  // also, the message should not have actually gotten marked dirty
  do_check_eq(msgSet.getMsgHdr(0).getUint32Property("gloda-dirty"), 0);

  // let's make the message un-read again for consistency with the gloda state
  msgSet.setRead(false);
  // make the folder dirty and let an indexing sweep take care of this so we
  //  don't get extra events in subsequent tests.
  glodaFolder._dirtyStatus = glodaFolder.kFolderDirty;
  GlodaMsgIndexer.indexingSweepNeeded = true;
  // (the message won't get indexed though)
  yield wait_for_gloda_indexer([]);
}

function test_indexing_never_priority() {

  // add a folder with a bunch of messages
  let [folder, msgSet] = make_folder_with_sets([{count: 1}]);
  yield wait_for_message_injection();

  // index it, and augment the msgSet with the glodaMessages array
  // for later use by sqlExpectCount
  yield wait_for_gloda_indexer([msgSet], {augment: true});

  // explicitly tell gloda to never index this folder
  let XPCOMFolder = get_real_injection_folder(folder);
  let glodaFolder = Gloda.getFolderForFolder(XPCOMFolder);
  GlodaMsgIndexer.setFolderIndexingPriority(XPCOMFolder,
                                            glodaFolder.kIndexingNeverPriority);

  // verify that the setter and getter do the right thing
  do_check_eq(glodaFolder.indexingPriority, glodaFolder.kIndexingNeverPriority);

  // check that existing message is marked as deleted
  yield wait_for_gloda_indexer([], {deleted: [msgSet]});

  // make sure the deletion hit the database
  yield sqlExpectCount(1,
    "SELECT COUNT(*) from folderLocations WHERE id = ? AND indexingPriority = ?",
     glodaFolder.id, glodaFolder.kIndexingNeverPriority);

  // add another message
  make_new_sets_in_folder(folder, [{count: 1}]);
  yield wait_for_message_injection();

  // make sure that indexing returns nothing
  GlodaMsgIndexer.indexingSweepNeeded = true;
  yield wait_for_gloda_indexer([]);
}

function test_setting_indexing_priority_never_while_indexing() {

  if (!message_injection_is_local())
    return;

  // Configure the gloda indexer to hang while streaming the message.
  configure_gloda_indexing({hangWhile: "streaming"});

  // create a folder with a message inside.
  let [folder, msgSet] = make_folder_with_sets([{count: 1}]);
  yield wait_for_message_injection();

  yield wait_for_indexing_hang();

  // explicitly tell gloda to never index this folder
  let XPCOMFolder = get_real_injection_folder(folder);
  let glodaFolder = Gloda.getFolderForFolder(XPCOMFolder);
  GlodaMsgIndexer.setFolderIndexingPriority(XPCOMFolder,
                                            glodaFolder.kIndexingNeverPriority);

  // reset indexing to not hang
  configure_gloda_indexing({});

  // sorta get the event chain going again...
  resume_from_simulated_hang(true);

  // Because the folder was dirty it should actually end up getting indexed,
  //  so in the end the message will get indexed.  Also, make sure a cleanup
  //  was observed.
  yield wait_for_gloda_indexer([], {cleanedUp: 1});
}

/* ===== Threading / Conversation Grouping ===== */

var gSynMessages = [];
function allMessageInSameConversation(aSynthMessage, aGlodaMessage, aConvID) {
  if (aConvID === undefined)
    return aGlodaMessage.conversationID;
  do_check_eq(aConvID, aGlodaMessage.conversationID);
  // Cheat and stash the synthetic message (we need them for one of the IMAP
  // tests)
  gSynMessages.push(aSynthMessage);
  return aConvID;
}

/**
 * Test our conversation/threading logic in the straight-forward direct
 *  reply case, the missing intermediary case, and the siblings with missing
 *  parent case.  We also test all permutations of receipt of those messages.
 * (Also tests that we index new messages.)
 */
function test_threading() {
  mark_sub_test_start("direct reply");
  yield indexAndPermuteMessages(scenarios.directReply,
                                allMessageInSameConversation);
  mark_sub_test_start("missing intermediary");
  yield indexAndPermuteMessages(scenarios.missingIntermediary,
                                allMessageInSameConversation);
  mark_sub_test_start("siblings missing parent");
  yield indexAndPermuteMessages(scenarios.siblingsMissingParent,
                                allMessageInSameConversation);
}

/**
 * Test the bit that says "if we're fulltext-indexing the message and we
 *  discover it didn't have any attachments, clear the attachment bit from the
 *  message header".
 */
function test_attachment_flag() {
  // create a synthetic message with an attachment that won't normally be listed
  // in the attachment pane (Content-Disposition: inline, no filename, and
  // displayable inline)
  let smsg = msgGen.makeMessage({
    name: 'test message with part 1.2 attachment',
    attachments: [{ body: 'attachment',
                    filename: '',
                    format: '' }],
  });
  // save it off for test_attributes_fundamental_from_disk
  let msgSet = new SyntheticMessageSet([smsg]);
  let folder = fundamentalFolderHandle = make_empty_folder();
  yield add_sets_to_folders(folder, [msgSet]);

  // if we need to go offline, let the indexing pass run, then force us offline
  if (goOffline) {
    yield wait_for_gloda_indexer(msgSet);
    yield make_folder_and_contents_offline(folder);
    // now the next indexer wait will wait for the next indexing pass...
  }

  yield wait_for_gloda_indexer(msgSet,
                               {verifier: verify_attachment_flag});

}

function verify_attachment_flag(smsg, gmsg) {
  // -- attachments. We won't have these if we don't have fulltext results
  if (expectFulltextResults) {
    do_check_eq(gmsg.attachmentNames.length, 0);
    do_check_eq(gmsg.attachmentInfos.length, 0);
    do_check_false(gmsg.folderMessage.flags & Ci.nsMsgMessageFlags.Attachment);
  }
}
/* ===== Fundamental Attributes (per fundattr.js) ===== */

/**
 * Save the synthetic message created in test_attributes_fundamental for the
 *  benefit of test_attributes_fundamental_from_disk.
 */
var fundamentalSyntheticMessage;
var fundamentalFolderHandle;
/**
 * We're saving this one so that we can move the message later and verify that
 * the attributes are consistent.
 */
var fundamentalMsgSet;
var fundamentalGlodaMsgAttachmentUrls;
/**
 * Save the resulting gloda message id corresponding to the
 *  fundamentalSyntheticMessage so we can use it to query the message from disk.
 */
var fundamentalGlodaMessageId;

/**
 * Test that we extract the 'fundamental attributes' of a message properly
 *  'Fundamental' in this case is talking about the attributes defined/extracted
 *  by gloda's fundattr.js and perhaps the core message indexing logic itself
 *  (which show up as kSpecial* attributes in fundattr.js anyways.)
 */
function test_attributes_fundamental() {
  // create a synthetic message with attachment
  let smsg = msgGen.makeMessage({
    name: 'test message',
    bodyPart: new SyntheticPartMultiMixed([
      new SyntheticPartLeaf({body: 'I like cheese!'}),
      msgGen.makeMessage({ body: { body: 'I like wine!' }}), // that's one attachment
    ]),
    attachments: [
      {filename: 'bob.txt', body: 'I like bread!'} // and that's another one
    ],
  });
  // save it off for test_attributes_fundamental_from_disk
  fundamentalSyntheticMessage = smsg;
  let msgSet = new SyntheticMessageSet([smsg]);
  fundamentalMsgSet = msgSet;
  let folder = fundamentalFolderHandle = make_empty_folder();
  yield add_sets_to_folders(folder, [msgSet]);

  // if we need to go offline, let the indexing pass run, then force us offline
  if (goOffline) {
    yield wait_for_gloda_indexer(msgSet);
    yield make_folder_and_contents_offline(folder);
    // now the next indexer wait will wait for the next indexing pass...
  }

  yield wait_for_gloda_indexer(msgSet, { verifier: verify_attributes_fundamental });

}

function verify_attributes_fundamental(smsg, gmsg) {
  // save off the message id for test_attributes_fundamental_from_disk
  fundamentalGlodaMessageId = gmsg.id;
  fundamentalGlodaMsgAttachmentUrls = [att.url for each (att in gmsg.attachmentInfos)];

  do_check_eq(gmsg.folderURI,
              get_real_injection_folder(fundamentalFolderHandle).URI);

  // -- subject
  do_check_eq(smsg.subject, gmsg.conversation.subject);
  do_check_eq(smsg.subject, gmsg.subject);

  // -- contact/identity information
  // - from
  // check the e-mail address
  do_check_eq(gmsg.from.kind, "email");
  do_check_eq(smsg.fromAddress, gmsg.from.value);
  // check the name
  do_check_eq(smsg.fromName, gmsg.from.contact.name);

  // - to
  do_check_eq(smsg.toAddress, gmsg.to[0].value);
  do_check_eq(smsg.toName, gmsg.to[0].contact.name);

  // date
  do_check_eq(smsg.date.valueOf(), gmsg.date.valueOf());

  // -- message ID
  do_check_eq(smsg.messageId, gmsg.headerMessageID);

  // -- attachments. We won't have these if we don't have fulltext results
  if (expectFulltextResults) {
    do_check_eq(gmsg.attachmentTypes.length, 1);
    do_check_eq(gmsg.attachmentTypes[0], "text/plain");
    do_check_eq(gmsg.attachmentNames.length, 1);
    do_check_eq(gmsg.attachmentNames[0], "bob.txt");

    let expectedInfos = [
      // the name for that one is generated randomly
      { contentType: "message/rfc822" },
      { name: "bob.txt", contentType: "text/plain" },
    ];
    let expectedSize = 14;
    do_check_eq(gmsg.attachmentInfos.length, 2);
    for each (let [i, attInfos] in Iterator(gmsg.attachmentInfos)) {
      for (let k in expectedInfos[i])
        do_check_eq(attInfos[k], expectedInfos[i][k]);
      // because it's unreliable and depends on the platform
      do_check_true(Math.abs(attInfos.size - expectedSize) <= 2);
      // check that the attachment URLs are correct
      let channel = NetUtil.newChannel(attInfos.url);
      try {
        // will throw if the URL is invalid
        channel.open();
      } catch (e) {
        do_throw(new Error("Invalid attachment URL"));
      }
    }
  }
  else {
    // Make sure we don't actually get attachments!
    do_check_eq(gmsg.attachmentTypes, null);
    do_check_eq(gmsg.attachmentNames, null);
  }
}

/**
 * We now move the message into another folder, wait for it to be indexed,
 * and make sure the magic url getter for GlodaAttachment returns a proper
 * URL.
 */
function test_moved_message_attributes() {
  if (!expectFulltextResults)
    return;

  // Don't ask me why, let destFolder = make_empty_folder would result in a
  // random error when running test_index_messages_imap_offline.js ...
  let [destFolder, ignoreSet] = make_folder_with_sets([{count: 2}]);
  fundamentalFolderHandle = destFolder;
  yield wait_for_message_injection();
  yield wait_for_gloda_indexer([ignoreSet]);

  // this is a fast move (third parameter set to true)
  yield async_move_messages(fundamentalMsgSet, destFolder, true);

  yield wait_for_gloda_indexer(fundamentalMsgSet, {
    verifier: function (newSynMsg, newGlodaMsg) {
      // verify we still have the same number of attachments
      do_check_eq(fundamentalGlodaMsgAttachmentUrls.length,
        newGlodaMsg.attachmentInfos.length);
      for each (let [i, attInfos] in Iterator(newGlodaMsg.attachmentInfos)) {
        // verify the url has changed
        do_check_neq(fundamentalGlodaMsgAttachmentUrls[i], attInfos.url);
        // and verify that the new url is still valid
        let channel = NetUtil.newChannel(attInfos.url);
        try {
          channel.open();
        } catch (e) {
          do_throw(new Error("Invalid attachment URL"));
        }
      }
    },
    fullyIndexed: 0,
  });
}

/**
 * We want to make sure that all of the fundamental properties also are there
 *  when we load them from disk.  Nuke our cache, query the message back up.
 *  We previously used getMessagesByMessageID to get the message back, but he
 *  does not perform a full load-out like a query does, so we need to use our
 *  query mechanism for this.
 */
function test_attributes_fundamental_from_disk() {
  nukeGlodaCachesAndCollections();

  let query = Gloda.newQuery(Gloda.NOUN_MESSAGE).id(fundamentalGlodaMessageId);
  queryExpect(query, [fundamentalSyntheticMessage],
              verify_attributes_fundamental_from_disk,
              function (smsg) { return smsg.messageId; } );
  return false;
}

/**
 * We are just a wrapper around verify_attributes_fundamental, adapting the
 *  return callback from getMessagesByMessageID.
 *
 * @param aGlodaMessageLists This should be [[theGlodaMessage]].
 */
function verify_attributes_fundamental_from_disk(aGlodaMessage) {
  // return the message id for test_attributes_fundamental_from_disk's benefit
  verify_attributes_fundamental(fundamentalSyntheticMessage,
                                aGlodaMessage);
  return aGlodaMessage.headerMessageID;
}

/* ===== Explicit Attributes (per explattr.js) ===== */

/**
 * Test the attributes defined by explattr.js.
 */
function test_attributes_explicit() {
  let [folder, msgSet] = make_folder_with_sets([{count: 1}]);
  yield wait_for_message_injection();
  yield wait_for_gloda_indexer(msgSet, {augment: true});
  let gmsg = msgSet.glodaMessages[0];

  // -- Star
  mark_sub_test_start("Star");
  msgSet.setStarred(true);
  yield wait_for_gloda_indexer(msgSet);
  do_check_eq(gmsg.starred, true);

  msgSet.setStarred(false);
  yield wait_for_gloda_indexer(msgSet);
  do_check_eq(gmsg.starred, false);

  // -- Read / Unread
  mark_sub_test_start("Read/Unread");
  msgSet.setRead(true);
  yield wait_for_gloda_indexer(msgSet);
  do_check_eq(gmsg.read, true);

  msgSet.setRead(false);
  yield wait_for_gloda_indexer(msgSet);
  do_check_eq(gmsg.read, false);

  // -- Tags
  mark_sub_test_start("Tags");
  // note that the tag service does not guarantee stable nsIMsgTag references,
  //  nor does noun_tag go too far out of its way to provide stability.
  //  However, it is stable as long as we don't spook it by bringing new tags
  //  into the equation.
  let tagOne = TagNoun.getTag("$label1");
  let tagTwo = TagNoun.getTag("$label2");

  msgSet.addTag(tagOne.key);
  yield wait_for_gloda_indexer(msgSet);
  do_check_neq(gmsg.tags.indexOf(tagOne), -1);

  msgSet.addTag(tagTwo.key);
  yield wait_for_gloda_indexer(msgSet);
  do_check_neq(gmsg.tags.indexOf(tagOne), -1);
  do_check_neq(gmsg.tags.indexOf(tagTwo), -1);

  msgSet.removeTag(tagOne.key);
  yield wait_for_gloda_indexer(msgSet);
  do_check_eq(gmsg.tags.indexOf(tagOne), -1);
  do_check_neq(gmsg.tags.indexOf(tagTwo), -1);

  msgSet.removeTag(tagTwo.key);
  yield wait_for_gloda_indexer(msgSet);
  do_check_eq(gmsg.tags.indexOf(tagOne), -1);
  do_check_eq(gmsg.tags.indexOf(tagTwo), -1);

  // -- Replied To

  // -- Forwarded
}


/**
 * Test non-query-able attributes
 */
function test_attributes_cant_query() {
  let [folder, msgSet] = make_folder_with_sets([{count: 1}]);
  yield wait_for_message_injection();
  yield wait_for_gloda_indexer(msgSet, {augment: true});
  let gmsg = msgSet.glodaMessages[0];

  // -- Star
  mark_sub_test_start("Star");
  msgSet.setStarred(true);
  yield wait_for_gloda_indexer(msgSet);
  do_check_eq(gmsg.starred, true);

  msgSet.setStarred(false);
  yield wait_for_gloda_indexer(msgSet);
  do_check_eq(gmsg.starred, false);

  // -- Read / Unread
  mark_sub_test_start("Read/Unread");
  msgSet.setRead(true);
  yield wait_for_gloda_indexer(msgSet);
  do_check_eq(gmsg.read, true);

  msgSet.setRead(false);
  yield wait_for_gloda_indexer(msgSet);
  do_check_eq(gmsg.read, false);

  let readDbAttr = Gloda.getAttrDef(Gloda.BUILT_IN, "read");
  let readId = readDbAttr.id;

  yield sqlExpectCount(0, "SELECT COUNT(*) FROM messageAttributes WHERE attributeID = ?1",
                       readId);

  // -- Replied To

  // -- Forwarded
}

/**
 * Have the participants be in our addressbook prior to indexing so that we can
 *  verify that the hand-off to the addressbook indexer does not cause breakage.
 */
function test_people_in_addressbook() {
  var senderPair = msgGen.makeNameAndAddress(),
      recipPair = msgGen.makeNameAndAddress();
  
  // - add both people to the address book
  makeABCardForAddressPair(senderPair);
  makeABCardForAddressPair(recipPair);

  let [folder, msgSet] = make_folder_with_sets([
    { count: 1, to: [recipPair], from: senderPair }]);
  yield wait_for_message_injection();
  yield wait_for_gloda_indexer(msgSet, {augment: true});
  let gmsg = msgSet.glodaMessages[0],
      senderIdentity = gmsg.from,
      recipIdentity = gmsg.to[0];

  do_check_neq(senderIdentity.contact, null);
  do_check_true(senderIdentity.inAddressBook);

  do_check_neq(recipIdentity.contact, null);
  do_check_true(recipIdentity.inAddressBook);
}

/* ===== fulltexts Indexing ===== */

/**
 * Make sure that we are using the saneBodySize flag.  This is basically the
 *  test_sane_bodies test from test_mime_emitter but we pull the indexedBodyText
 *  off the message to check and also make sure that the text contents slice
 *  off the end rather than the beginning.
 */
function test_streamed_bodies_are_size_capped() {
  if (!expectFulltextResults)
    return;

  let hugeString =
    "qqqqxxxx qqqqxxx qqqqxxx qqqqxxx qqqqxxx qqqqxxx qqqqxxx \r\n";
  const powahsOfTwo = 10;
  for (let i = 0; i < powahsOfTwo; i++) {
    hugeString = hugeString + hugeString;
  }
  let bodyString = "aabb" + hugeString + "xxyy";

  let synMsg = gMessageGenerator.makeMessage(
    {body: {body: bodyString, contentType: "text/plain"}});
  let msgSet = new SyntheticMessageSet([synMsg]);
  let folder = make_empty_folder();
  yield add_sets_to_folder(folder, [msgSet]);

  if (goOffline) {
    yield wait_for_gloda_indexer(msgSet);
    yield make_folder_and_contents_offline(folder);
  }

  yield wait_for_gloda_indexer(msgSet, {augment: true});
  let gmsg = msgSet.glodaMessages[0];
  do_check_eq(gmsg.indexedBodyText.indexOf("aabb"), 0);
  do_check_eq(gmsg.indexedBodyText.indexOf("xxyy"), -1);

  if (gmsg.indexedBodyText.length > (20 * 1024 + 58 + 10))
    do_throw("indexed body text is too big! (" + gmsg.indexedBodyText.length +
             ")");
}


/* ===== Message Deletion ===== */
/**
 * Test actually deleting a message on a per-message basis (not just nuking the
 *  folder like emptying the trash does.)
 *
 * Logic situations:
 * - Non-last message in a conversation, twin.
 * - Non-last message in a conversation, not a twin.
 * - Last message in a conversation
 */
function test_message_deletion() {
  mark_sub_test_start("non-last message in conv, twin");
  // create and index two messages in a conversation
  let [folder, convSet] = make_folder_with_sets([{count: 2, msgsPerThread: 2}]);
  yield wait_for_message_injection();
  yield wait_for_gloda_indexer([convSet], {augment: true});

  // Twin the first message in a different folder owing to our reliance on
  //  message-id's in the SyntheticMessageSet logic.  (This is also why we broke
  //  up the indexing waits too.)
  let twinFolder = make_empty_folder();
  let twinSet = new SyntheticMessageSet([convSet.synMessages[0]]);
  yield add_sets_to_folder(twinFolder, [twinSet]);
  yield wait_for_gloda_indexer([twinSet], {augment: true});

  // Split the conv set into two helper sets...
  let firstSet = convSet.slice(0, 1); // the twinned first message in the thread
  let secondSet = convSet.slice(1, 2); // the un-twinned second thread message

  // make sure we can find the message (paranoia)
  let firstQuery = Gloda.newQuery(Gloda.NOUN_MESSAGE);
  firstQuery.id(firstSet.glodaMessages[0].id);
  let firstColl = queryExpect(firstQuery, firstSet);
  yield false; // queryExpect is async but returns a value...

  // delete it (not trash! delete!)
  yield async_delete_messages(firstSet);
  // which should result in an apparent deletion
  yield wait_for_gloda_indexer([], {deleted: firstSet});
  // and our collection from that query should now be empty
  do_check_eq(firstColl.items.length, 0);

  // make sure it no longer shows up in a standard query
  firstColl = queryExpect(firstQuery, []);
  yield false; // queryExpect is async

  // make sure it shows up in a privileged query
  let privQuery = Gloda.newQuery(Gloda.NOUN_MESSAGE, {
                      noDbQueryValidityConstraints: true,
                    });
  let firstGlodaId = firstSet.glodaMessages[0].id;
  privQuery.id(firstGlodaId);
  queryExpect(privQuery, firstSet);
  yield false; // queryExpect is async

  // force a deletion pass
  GlodaMsgIndexer.indexingSweepNeeded = true;
  yield wait_for_gloda_indexer([]);

  // Make sure it no longer shows up in a privileged query; since it has a twin
  //  we don't need to leave it as a ghost.
  queryExpect(privQuery, []);
  yield false; // queryExpect is async

  // make sure the messagesText entry got blown away
  yield sqlExpectCount(0, "SELECT COUNT(*) FROM messagesText WHERE docid = ?1",
                       firstGlodaId);

  // make sure the conversation still exists...
  let conv = twinSet.glodaMessages[0].conversation;
  let convQuery = Gloda.newQuery(Gloda.NOUN_CONVERSATION);
  convQuery.id(conv.id);
  let convColl = queryExpect(convQuery, [conv]);
  yield false; // queryExpect is async


  // -- non-last message, no longer a twin => ghost
  mark_sub_test_start("non-last message in conv, no longer a twin");

  // make sure nuking the twin didn't somehow kill them both
  let twinQuery = Gloda.newQuery(Gloda.NOUN_MESSAGE);
  // (let's search on the message-id now that there is no ambiguity.)
  twinQuery.headerMessageID(twinSet.synMessages[0].messageId);
  let twinColl = queryExpect(twinQuery, twinSet);
  yield false; // queryExpect is async

  // delete the twin
  yield async_delete_messages(twinSet);
  // which should result in an apparent deletion
  yield wait_for_gloda_indexer([], {deleted: twinSet});
  // it should disappear from the collection
  do_check_eq(twinColl.items.length, 0);

  // no longer show up in the standard query
  twinColl = queryExpect(twinQuery, []);
  yield false; // queryExpect is async

  // still show up in a privileged query
  privQuery = Gloda.newQuery(Gloda.NOUN_MESSAGE, {
                               noDbQueryValidityConstraints: true,
                             });
  privQuery.headerMessageID(twinSet.synMessages[0].messageId);
  queryExpect(privQuery, twinSet);
  yield false; // queryExpect is async

  // force a deletion pass
  GlodaMsgIndexer.indexingSweepNeeded = true;
  yield wait_for_gloda_indexer([]);

  // The message should be marked as a ghost now that the deletion pass.
  // Ghosts have no fulltext rows, so check for that.
  yield sqlExpectCount(0, "SELECT COUNT(*) FROM messagesText WHERE docid = ?1",
                       twinSet.glodaMessages[0].id);

  // it still should show up in the privileged query; it's a ghost!
  let privColl = queryExpect(privQuery, twinSet);
  yield false; // queryExpect is async
  // make sure it looks like a ghost.
  let twinGhost = privColl.items[0];
  do_check_eq(twinGhost._folderID, null);
  do_check_eq(twinGhost._messageKey, null);

  // make sure the conversation still exists...
  queryExpect(convQuery, [conv]);
  yield false; // queryExpect is async


  // -- non-last message, not a twin
  // This should blow away the message, the ghosts, and the conversation.
  mark_sub_test_start("last message in conv");

  // second message should still be around
  let secondQuery = Gloda.newQuery(Gloda.NOUN_MESSAGE);
  secondQuery.headerMessageID(secondSet.synMessages[0].messageId);
  let secondColl = queryExpect(secondQuery, secondSet);
  yield false; // queryExpect is async

  // delete it and make sure it gets marked deleted appropriately
  yield async_delete_messages(secondSet);
  yield wait_for_gloda_indexer([], {deleted: secondSet});
  do_check_eq(secondColl.items.length, 0);

  // still show up in a privileged query
  privQuery = Gloda.newQuery(Gloda.NOUN_MESSAGE, {
                               noDbQueryValidityConstraints: true,
                             });
  privQuery.headerMessageID(secondSet.synMessages[0].messageId);
  queryExpect(privQuery, secondSet);
  yield false; // queryExpect is async

  // force a deletion pass
  GlodaMsgIndexer.indexingSweepNeeded = true;
  yield wait_for_gloda_indexer([]);

  // it should no longer show up in a privileged query; we killed the ghosts
  queryExpect(privQuery, []);
  yield false; // queryExpect is async

  // - the conversation should have disappeared too
  // (we have no listener to watch for it to have disappeared from convQuery but
  //  this is basically how glodaTestHelper does its thing anyways.)
  do_check_eq(convColl.items.length, 0);

  // make sure the query fails to find it too
  queryExpect(convQuery, []);
  yield false; // queryExpect is async


  // -- identity culling verification
  mark_sub_test_start("identity culling verification");
  // The identities associated with that message should no longer exist, nor
  //  should their contacts.

}

function test_moving_to_trash_marks_deletion() {
  // create and index two messages in a conversation
  let [folder, msgSet] = make_folder_with_sets([{count: 2, msgsPerThread: 2}]);
  yield wait_for_message_injection();
  yield wait_for_gloda_indexer([msgSet], {augment: true});

  let convId = msgSet.glodaMessages[0].conversation.id;
  let firstGlodaId = msgSet.glodaMessages[0].id;
  let secondGlodaId = msgSet.glodaMessages[1].id;

  // move them to the trash.
  yield async_trash_messages(msgSet);

  // we do not index the trash folder so this should actually make them appear
  //  deleted to an unprivileged query.
  let msgQuery = Gloda.newQuery(Gloda.NOUN_MESSAGE);
  msgQuery.id(firstGlodaId, secondGlodaId);
  queryExpect(msgQuery, []);
  yield false; // queryExpect is async

  // they will appear deleted after the events
  yield wait_for_gloda_indexer([], {deleted: msgSet});

  // force a sweep
  GlodaMsgIndexer.indexingSweepNeeded = true;
  // there should be no apparent change as the result of this pass
  // (well, the conversation will die, but we can't see that.)
  yield wait_for_gloda_indexer([]);

  // the conversation should be gone
  let convQuery = Gloda.newQuery(Gloda.NOUN_CONVERSATION);
  convQuery.id(convId);
  queryExpect(convQuery, []);
  yield false; // queryExpect is async

  // the messages should be entirely gone
  let msgPrivQuery = Gloda.newQuery(Gloda.NOUN_MESSAGE, {
                                      noDbQueryValidityConstraints: true,
                                    });
  msgPrivQuery.id(firstGlodaId, secondGlodaId);
  queryExpect(msgPrivQuery, []);
  yield false; // queryExpect is async
}

/**
 * Deletion that occurs because a folder got deleted.
 *  There is no hand-holding involving the headers that were in the folder.
 */
function test_folder_nuking_message_deletion() {
  // create and index two messages in a conversation
  let [folder, msgSet] = make_folder_with_sets([{count: 2, msgsPerThread: 2}]);
  yield wait_for_message_injection();
  yield wait_for_gloda_indexer([msgSet], {augment: true});

  let convId = msgSet.glodaMessages[0].conversation.id;
  let firstGlodaId = msgSet.glodaMessages[0].id;
  let secondGlodaId = msgSet.glodaMessages[1].id;

  // Delete the folder
  yield async_delete_folder(folder);
  // That does generate the deletion events if the messages were in-memory,
  //  which these are.
  yield wait_for_gloda_indexer([], {deleted: msgSet});

  // this should have caused us to mark all the messages as deleted; the
  //  messages should no longer show up in an unprivileged query
  let msgQuery = Gloda.newQuery(Gloda.NOUN_MESSAGE);
  msgQuery.id(firstGlodaId, secondGlodaId);
  queryExpect(msgQuery, []);
  yield false; // queryExpect is async

  // force a sweep
  GlodaMsgIndexer.indexingSweepNeeded = true;
  // there should be no apparent change as the result of this pass
  // (well, the conversation will die, but we can't see that.)
  yield wait_for_gloda_indexer([]);

  // the conversation should be gone
  let convQuery = Gloda.newQuery(Gloda.NOUN_CONVERSATION);
  convQuery.id(convId);
  queryExpect(convQuery, []);
  yield false; // queryExpect is async

  // the messages should be entirely gone
  let msgPrivQuery = Gloda.newQuery(Gloda.NOUN_MESSAGE, {
                                      noDbQueryValidityConstraints: true,
                                    });
  msgPrivQuery.id(firstGlodaId, secondGlodaId);
  queryExpect(msgPrivQuery, []);
  yield false; // queryExpect is async
}

/* ===== Folder Move/Rename/Copy (Single and Nested) ===== */

function get_nsIMsgFolder(aFolder) {
  if (!(aFolder instanceof Ci.nsIMsgFolder))
    return MailUtils.getFolderForURI(aFolder);
  else
    return aFolder;
}

function get_testFolder(aFolder) {
  if ((typeof aFolder) != "string")
    return aFolder.URI;
  else
    return aFolder;
}

function test_folder_deletion_nested() {
  // add a folder with a bunch of messages
  let [folder1, msgSet1] = make_folder_with_sets([{count: 1}]);
  yield wait_for_message_injection();

  let [folder2, msgSet2] = make_folder_with_sets([{count: 1}]);
  yield wait_for_message_injection();

  // index these folders, and augment the msgSet with the glodaMessages array
  // for later use by sqlExpectCount
  yield wait_for_gloda_indexer([msgSet1, msgSet2], { augment: true });
  // the move has to be performed after the indexing, because otherwise, on
  // IMAP, the moved message header are different entities and it's not msgSet2
  // that ends up indexed, but the fresh headers
  yield move_folder(folder2, folder1);

  // add a trash folder, and move folder1 into it
  let trash = make_empty_folder(null, [Ci.nsMsgFolderFlags.Trash]);
  yield move_folder(folder1, trash);

  let descendentFolders = Cc["@mozilla.org/supports-array;1"]
                          .createInstance(Ci.nsISupportsArray);
  get_nsIMsgFolder(trash).ListDescendents(descendentFolders);
  let folders = [folder for (folder in fixIterator(descendentFolders, Ci.nsIMsgFolder))];
  do_check_eq(folders.length, 2);
  let [newFolder1, newFolder2] = folders;

  let glodaFolder1 = Gloda.getFolderForFolder(newFolder1);
  let glodaFolder2 = Gloda.getFolderForFolder(newFolder2);

  // verify that Gloda properly marked this folder as not to be indexed anymore 
  do_check_eq(glodaFolder1.indexingPriority, glodaFolder1.kIndexingNeverPriority);

  // check that existing message is marked as deleted
  yield wait_for_gloda_indexer([], {deleted: [msgSet1, msgSet2]});

  // make sure the deletion hit the database
  yield sqlExpectCount(1,
    "SELECT COUNT(*) from folderLocations WHERE id = ? AND indexingPriority = ?",
     glodaFolder1.id, glodaFolder1.kIndexingNeverPriority);
  yield sqlExpectCount(1,
    "SELECT COUNT(*) from folderLocations WHERE id = ? AND indexingPriority = ?",
     glodaFolder2.id, glodaFolder2.kIndexingNeverPriority);

  if (_messageInjectionSetup.injectionConfig.mode == "local") {
    // add another message
    make_new_sets_in_folder(newFolder1, [{count: 1}]);
    yield wait_for_message_injection();
    make_new_sets_in_folder(newFolder2, [{count: 1}]);
    yield wait_for_message_injection();

    // make sure that indexing returns nothing
    GlodaMsgIndexer.indexingSweepNeeded = true;
    yield wait_for_gloda_indexer([]);
  }
}

/* ===== IMAP Nuances ===== */

/**
 * Verify that for IMAP folders we still see an index a message that is added
 *  as read.
 */
function test_imap_add_unread_to_folder() {
  if (message_injection_is_local())
    return;

  let [srcFolder, msgSet] = make_folder_with_sets([{count: 1, read: true}]);
  yield wait_for_message_injection();
  yield wait_for_gloda_indexer(msgSet);
}

/* ===== Message Moving ===== */

/**
 * Moving a message between folders should result in us knowing that the message
 *  is in the target location.
 */
function test_message_moving() {
  // - inject and insert
  // source folder with the message we care about
  let [srcFolder, msgSet] = make_folder_with_sets([{count: 1}]);
  yield wait_for_message_injection();
  // dest folder with some messages in it to test some wacky local folder moving
  //  logic.  (Local moves try and update the correspondence immediately.)
  let [destFolder, ignoreSet] = make_folder_with_sets([{count: 2}]);
  yield wait_for_message_injection();

  // (we want the gloda message mapping...)
  yield wait_for_gloda_indexer([msgSet, ignoreSet], {augment: true});
  let gmsg = msgSet.glodaMessages[0];
  // save off the message key so we can make sure it changes.
  let oldMessageKey = msgSet.getMsgHdr(0).messageKey;

  // - fastpath (offline) move it to a new folder
  mark_sub_test_start("initial move");
  yield async_move_messages(msgSet, destFolder, true);

  // - make sure gloda sees it in the new folder
  // Since we are doing offline IMAP moves, the fast-path should be taken and
  //  so we should receive an itemsModified notification without a call to
  //  Gloda.grokNounItem.
  yield wait_for_gloda_indexer(msgSet, {fullyIndexed: 0});

  do_check_eq(gmsg.folderURI,
              get_real_injection_folder(destFolder).URI);

  // - make sure the message key is correct!
  do_check_eq(gmsg.messageKey, msgSet.getMsgHdr(0).messageKey);
  // (sanity check that the messageKey actually changed for the message...)
  do_check_neq(gmsg.messageKey, oldMessageKey);

  // - make sure the indexer's _keyChangedBatchInfo dict is empty
  for each (let [evilKey, evilValue] in
              Iterator(GlodaMsgIndexer._keyChangedBatchInfo)) {
    mark_failure(["GlodaMsgIndexer._keyChangedBatchInfo should be empty but",
                  "has key:",  evilKey, "and value:", evilValue, "."]);
  }

  // - slowpath (IMAP online) move it back to its origin folder
  mark_sub_test_start("move it back");
  yield async_move_messages(msgSet, srcFolder, false);
  // In the IMAP case we will end up reindexing the message because we will
  //  not be able to fast-path, but the local case will still be fast-pathed.
  yield wait_for_gloda_indexer(msgSet,
                               {fullyIndexed: message_injection_is_local() ?
                                                0 : 1});
  do_check_eq(gmsg.folderURI,
              get_real_injection_folder(srcFolder).URI);
  do_check_eq(gmsg.messageKey, msgSet.getMsgHdr(0).messageKey);
}

/**
 * Moving a gloda-indexed message out of a filthy folder should result in the
 *  destination message not having a gloda-id.
 */

/* ===== Message Copying ===== */


/* ===== Sweep Complications ==== */

/**
 * Make sure that a message indexed by event-driven indexing does not
 *  get reindexed by sweep indexing that follows.
 */
function test_sweep_indexing_does_not_reindex_event_indexed() {
  let [folder, msgSet] = make_folder_with_sets([{count: 1}]);
  yield wait_for_message_injection();

  // wait for the event sweep to complete
  yield wait_for_gloda_indexer([msgSet]);

  // force a sweep of the folder
  GlodaMsgIndexer.indexFolder(get_real_injection_folder(folder));
  yield wait_for_gloda_indexer([]);
}

/**
 * Verify that moving apparently gloda-indexed messages from a filthy folder or
 *  one that simply should not be gloda indexed does not result in the target
 *  messages having the gloda-id property on them.  To avoid messing with too
 *  many invariants we do the 'folder should not be gloda indexed' case.
 * Uh, and of course, the message should still get indexed once we clear the
 *  filthy gloda-id off of it given that it is moving from a folder that is not
 *  indexed to one that is indexed.
 */
function test_filthy_moves_slash_move_from_unindexed_to_indexed() {
  // - inject
  // the source folder needs a flag so we don't index it
  let srcFolder = make_empty_folder(null, [Ci.nsMsgFolderFlags.Junk]);
  // the destination folder has to be something we want to index though;
  let destFolder = make_empty_folder();
  let [msgSet] = make_new_sets_in_folder(srcFolder, [{count: 1}]);
  yield wait_for_message_injection();

  // - mark with a bogus gloda-id
  msgSet.getMsgHdr(0).setUint32Property("gloda-id", 9999);

  // - disable event driven indexing so we don't get interference from indexing
  configure_gloda_indexing({event: false});

  // - move
  yield async_move_messages(msgSet, destFolder);

  // - verify the target has no gloda-id!
  mark_action("actual", "checking", [msgSet.getMsgHdr(0)]);
  do_check_eq(msgSet.getMsgHdr(0).getUint32Property("gloda-id"), 0);

  // - re-enable indexing and let the indexer run
  // (we don't want to affect other tests)
  configure_gloda_indexing({});
  yield wait_for_gloda_indexer([msgSet]);
}

var tests = [
  test_pending_commit_tracker_flushes_correctly,
  test_pending_commit_causes_msgdb_commit,
  test_indexing_sweep,
  test_event_driven_indexing_does_not_mess_with_filthy_folders,

  test_threading,
  test_attachment_flag,
  test_attributes_fundamental,
  test_moved_message_attributes,
  test_attributes_fundamental_from_disk,
  test_attributes_explicit,
  test_attributes_cant_query,

  test_people_in_addressbook,

  test_streamed_bodies_are_size_capped,

  test_imap_add_unread_to_folder,
  test_message_moving,

  test_message_deletion,
  test_moving_to_trash_marks_deletion,
  test_folder_nuking_message_deletion,

  test_sweep_indexing_does_not_reindex_event_indexed,

  test_filthy_moves_slash_move_from_unindexed_to_indexed,

  test_indexing_never_priority,
  test_setting_indexing_priority_never_while_indexing,

  test_folder_deletion_nested,
];
