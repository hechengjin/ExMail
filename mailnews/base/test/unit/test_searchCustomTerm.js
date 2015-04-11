/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Testing of custom search features.
 *
 */
load("../../../resources/searchTestUtils.js");

const copyService = Cc["@mozilla.org/messenger/messagecopyservice;1"]
                      .getService(Ci.nsIMsgCopyService);

const kCustomId = "xpcomtest@mozilla.org#test";
var gHdr;

var Tests =
[
  { setValue: "iamgood",
    testValue: "iamnotgood",
    op: Ci.nsMsgSearchOp.Is,
    count: 0 },
  { setValue: "iamgood",
    testValue: "iamgood",
    op: Ci.nsMsgSearchOp.Is,
    count: 1 }
]

// nsIMsgSearchCustomTerm object
var customTerm =
{
  id: kCustomId,
  name: "term name",
  getEnabled: function(scope, op)
    {
      return scope == Ci.nsMsgSearchScope.offlineMail &&
             op == Ci.nsMsgSearchOp::Is
    },
  getAvailable: function(scope, op)
    {
      return scope == Ci.nsMsgSearchScope.offlineMail &&
             op == Ci.nsMsgSearchOp::Is
    },
  getAvailableOperators: function(scope, length)
    {
       length.value = 1;
       return [Ci.nsMsgSearchOp.Is];
    },
  match: function(msgHdr, searchValue, searchOp)
    {
      switch (searchOp)
      {
        case Ci.nsMsgSearchOp.Is:
          if (msgHdr.getProperty("theTestProperty") == searchValue)
            return true;
      }
      return false;
    }
};

function run_test()
{
  loadLocalMailAccount();
  let filterService = Cc["@mozilla.org/messenger/services/filters;1"]
                        .getService(Ci.nsIMsgFilterService);
  filterService.addCustomTerm(customTerm);

  var copyListener = 
  {
    OnStartCopy: function() {},
    OnProgress: function(aProgress, aProgressMax) {},
    SetMessageKey: function(aKey) { gHdr = gLocalInboxFolder.GetMessageHeader(aKey);},
    SetMessageId: function(aMessageId) {},
    OnStopCopy: function(aStatus) { doTest();}
  };

  // Get a message into the local filestore.
  // function testSearch() continues the testing after the copy.
  let bugmail1 = do_get_file("../../../data/bugmail1");
  do_test_pending();

  copyService.CopyFileMessage(bugmail1, gLocalInboxFolder, null, false, 0,
                              "", copyListener, null);
}

var testObject;

function doTest()
{
  let test = Tests.shift();
  if (test)
  {
    gHdr.setStringProperty("theTestProperty", test.setValue);
    testObject = new TestSearch(gLocalInboxFolder,
                         test.testValue,
                         Ci.nsMsgSearchAttrib.Custom,
                         test.op,
                         test.count,
                         doTest,
                         kCustomId);
  }
  else
  {
    testObject = null;
    gHdr = null;
    do_test_finished();
  }
}

