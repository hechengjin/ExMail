This directory (mailnews/test/data) contains various support files for the
testing of mailnews.

Not all files will be documented here, but this will be a place for some
documentation.

signons-mailnews1.8.txt
-----------------------

This is a passwords file from gecko 1.8/gecko 1.9/early 1.9.1
(TB 1.5/2.0/pre 3.0 beta 1, SM 1.0/1.5/early 2.0). It is used as a test input
for several test_*Password.js files to check that we load usernames and
passwords correctly from the legacy file.

abLists1.mab
------------

An address book with 5 cards and 3 lists. The cards only have the email and
prefer mail format set:

test1@invalid.com  unknown
test2@invalid.com  unknown
test3@invalid.com  unknown
test4@invalid.com  plain text
test5@invalid.com  html

There are 3 lists, TestList1, TestList2 and TestList3. They have the following
cards:

TestList1:

test1@invalid.com
test2@invalid.com
test3@invalid.com

TestList2:

test4@invalid.com

TestList3:

test5@invalid.com


abLists2.mab
------------

The same as abLists1.mab, but with com.invalid instead of invalid.com, and ListTestX instead of TestListX.
