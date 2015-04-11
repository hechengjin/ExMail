/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-  */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _MORKROWMAP_
#define _MORKROWMAP_ 1

#ifndef _MORK_
#include "mork.h"
#endif

#ifndef _MORKNODE_
#include "morkNode.h"
#endif

#ifndef _MORKMAP_
#include "morkMap.h"
#endif

#ifndef _MORKPROBEMAP_
#include "morkProbeMap.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#define morkDerived_kRowMap  /*i*/ 0x724D /* ascii 'rM' */

/*| morkRowMap: maps a set of morkRow by contained Oid
|*/
class morkRowMap : public morkMap { // for mapping row IDs to rows

// { ===== begin morkNode interface =====
public: // morkNode virtual methods
  virtual void CloseMorkNode(morkEnv* ev); // CloseRowMap() only if open
  virtual ~morkRowMap(); // assert that CloseRowMap() executed earlier
  
public: // morkMap construction & destruction
  morkRowMap(morkEnv* ev, const morkUsage& inUsage,
    nsIMdbHeap* ioHeap, nsIMdbHeap* ioSlotHeap, mork_size inSlots);
  void CloseRowMap(morkEnv* ev); // called by CloseMorkNode();

public: // dynamic type identification
  mork_bool IsRowMap() const
  { return IsNode() && mNode_Derived == morkDerived_kRowMap; }
// } ===== end morkNode methods =====

// { ===== begin morkMap poly interface =====
  virtual mork_bool // note: equal(a,b) implies hash(a) == hash(b)
  Equal(morkEnv* ev, const void* inKeyA, const void* inKeyB) const;
  // implemented using morkRow::EqualRow()

  virtual mork_u4 // note: equal(a,b) implies hash(a) == hash(b)
  Hash(morkEnv* ev, const void* inKey) const;
  // implemented using morkRow::HashRow()
// } ===== end morkMap poly interface =====

public: // other map methods

  mork_bool AddRow(morkEnv* ev, morkRow* ioRow);
  // AddRow() returns ev->Good()

  morkRow*  CutOid(morkEnv* ev, const mdbOid* inOid);
  // CutRid() returns the row removed equal to inRid, if there was one

  morkRow*  CutRow(morkEnv* ev, const morkRow* ioRow);
  // CutRow() returns the row removed equal to ioRow, if there was one
  
  morkRow*  GetOid(morkEnv* ev, const mdbOid* inOid);
  // GetOid() returns the row equal to inRid, or else nil
  
  morkRow*  GetRow(morkEnv* ev, const morkRow* ioRow);
  // GetRow() returns the row equal to ioRow, or else nil
  
  // note the rows are owned elsewhere, usuall by morkRowSpace

public: // typesafe refcounting inlines calling inherited morkNode methods
  static void SlotWeakRowMap(morkRowMap* me,
    morkEnv* ev, morkRowMap** ioSlot)
  { morkNode::SlotWeakNode((morkNode*) me, ev, (morkNode**) ioSlot); }
  
  static void SlotStrongRowMap(morkRowMap* me,
    morkEnv* ev, morkRowMap** ioSlot)
  { morkNode::SlotStrongNode((morkNode*) me, ev, (morkNode**) ioSlot); }
};

class morkRowMapIter: public morkMapIter{ // typesafe wrapper class

public:
  morkRowMapIter(morkEnv* ev, morkRowMap* ioMap)
  : morkMapIter(ev, ioMap) { }
 
  morkRowMapIter( ) : morkMapIter()  { }
  void InitRowMapIter(morkEnv* ev, morkRowMap* ioMap)
  { this->InitMapIter(ev, ioMap); }
   
  mork_change* FirstRow(morkEnv* ev, morkRow** outRowPtr)
  { return this->First(ev, outRowPtr, /*val*/ (void*) 0); }
  
  mork_change* NextRow(morkEnv* ev, morkRow** outRowPtr)
  { return this->Next(ev, outRowPtr, /*val*/ (void*) 0); }
  
  mork_change* HereRow(morkEnv* ev, morkRow** outRowPtr)
  { return this->Here(ev, outRowPtr, /*val*/ (void*) 0); }
  
  mork_change* CutHereRow(morkEnv* ev, morkRow** outRowPtr)
  { return this->CutHere(ev, outRowPtr, /*val*/ (void*) 0); }
};


//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#define morkDerived_kRowProbeMap  /*i*/ 0x726D /* ascii 'rm' */

/*| morkRowProbeMap: maps a set of morkRow by contained Oid
|*/
class morkRowProbeMap : public morkProbeMap { // for mapping row IDs to rows

// { ===== begin morkNode interface =====
public: // morkNode virtual methods
  virtual void CloseMorkNode(morkEnv* ev); // CloseRowProbeMap() only if open
  virtual ~morkRowProbeMap(); // assert CloseRowProbeMap() executed earlier
  
public: // morkMap construction & destruction
  morkRowProbeMap(morkEnv* ev, const morkUsage& inUsage,
    nsIMdbHeap* ioHeap, nsIMdbHeap* ioSlotHeap, mork_size inSlots);
  void CloseRowProbeMap(morkEnv* ev); // called by CloseMorkNode();

public: // dynamic type identification
  mork_bool IsRowMap() const
  { return IsNode() && mNode_Derived == morkDerived_kRowMap; }
// } ===== end morkNode methods =====

  // { ===== begin morkProbeMap methods =====
  virtual mork_test // hit(a,b) implies hash(a) == hash(b)
  MapTest(morkEnv* ev, const void* inMapKey, const void* inAppKey) const;

  virtual mork_u4 // hit(a,b) implies hash(a) == hash(b)
  MapHash(morkEnv* ev, const void* inAppKey) const;

  virtual mork_u4 ProbeMapHashMapKey(morkEnv* ev, const void* inMapKey) const;

  // virtual mork_bool ProbeMapIsKeyNil(morkEnv* ev, void* ioMapKey);

  // virtual void ProbeMapClearKey(morkEnv* ev, // put 'nil' alls keys inside map
  //   void* ioMapKey, mork_count inKeyCount); // array of keys inside map

  // virtual void ProbeMapPushIn(morkEnv* ev, // move (key,val) into the map
  //   const void* inAppKey, const void* inAppVal, // (key,val) outside map
  //   void* outMapKey, void* outMapVal);      // (key,val) inside map

  // virtual void ProbeMapPullOut(morkEnv* ev, // move (key,val) out from the map
  //   const void* inMapKey, const void* inMapVal, // (key,val) inside map
  //   void* outAppKey, void* outAppVal) const;    // (key,val) outside map
  // } ===== end morkProbeMap methods =====

public: // other map methods

  mork_bool AddRow(morkEnv* ev, morkRow* ioRow);
  // AddRow() returns ev->Good()

  morkRow*  CutOid(morkEnv* ev, const mdbOid* inOid);
  // CutRid() returns the row removed equal to inRid, if there was one

  morkRow*  CutRow(morkEnv* ev, const morkRow* ioRow);
  // CutRow() returns the row removed equal to ioRow, if there was one
  
  morkRow*  GetOid(morkEnv* ev, const mdbOid* inOid);
  // GetOid() returns the row equal to inRid, or else nil
  
  morkRow*  GetRow(morkEnv* ev, const morkRow* ioRow);
  // GetRow() returns the row equal to ioRow, or else nil
  
  // note the rows are owned elsewhere, usuall by morkRowSpace

public: // typesafe refcounting inlines calling inherited morkNode methods
  static void SlotWeakRowProbeMap(morkRowProbeMap* me,
    morkEnv* ev, morkRowProbeMap** ioSlot)
  { morkNode::SlotWeakNode((morkNode*) me, ev, (morkNode**) ioSlot); }
  
  static void SlotStrongRowProbeMap(morkRowProbeMap* me,
    morkEnv* ev, morkRowProbeMap** ioSlot)
  { morkNode::SlotStrongNode((morkNode*) me, ev, (morkNode**) ioSlot); }
};

class morkRowProbeMapIter: public morkProbeMapIter{ // typesafe wrapper class

public:
  morkRowProbeMapIter(morkEnv* ev, morkRowProbeMap* ioMap)
  : morkProbeMapIter(ev, ioMap) { }
 
  morkRowProbeMapIter( ) : morkProbeMapIter()  { }
  void InitRowMapIter(morkEnv* ev, morkRowProbeMap* ioMap)
  { this->InitMapIter(ev, ioMap); }
   
  mork_change* FirstRow(morkEnv* ev, morkRow** outRowPtr)
  { return this->First(ev, outRowPtr, /*val*/ (void*) 0); }
  
  mork_change* NextRow(morkEnv* ev, morkRow** outRowPtr)
  { return this->Next(ev, outRowPtr, /*val*/ (void*) 0); }
  
  mork_change* HereRow(morkEnv* ev, morkRow** outRowPtr)
  { return this->Here(ev, outRowPtr, /*val*/ (void*) 0); }
  
  mork_change* CutHereRow(morkEnv* ev, morkRow** outRowPtr)
  { return this->CutHere(ev, outRowPtr, /*val*/ (void*) 0); }
};

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#endif /* _MORKROWMAP_ */
