/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-  */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _MDB_
#include "mdb.h"
#endif

#ifndef _MORK_
#include "mork.h"
#endif

#ifndef _MORKNODE_
#include "morkNode.h"
#endif

#ifndef _MORKENV_
#include "morkEnv.h"
#endif

#ifndef _MORKMAP_
#include "morkMap.h"
#endif

#ifndef _MORKROWMAP_
#include "morkRowMap.h"
#endif

#ifndef _MORKROW_
#include "morkRow.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

// ````` ````` ````` ````` ````` 
// { ===== begin morkNode interface =====

/*public virtual*/ void
morkRowMap::CloseMorkNode(morkEnv* ev) // CloseRowMap() only if open
{
  if ( this->IsOpenNode() )
  {
    this->MarkClosing();
    this->CloseRowMap(ev);
    this->MarkShut();
  }
}

/*public virtual*/
morkRowMap::~morkRowMap() // assert CloseRowMap() executed earlier
{
  MORK_ASSERT(this->IsShutNode());
}

/*public non-poly*/
morkRowMap::morkRowMap(morkEnv* ev, const morkUsage& inUsage,
  nsIMdbHeap* ioHeap, nsIMdbHeap* ioSlotHeap, mork_size inSlots)
: morkMap(ev, inUsage,  ioHeap,
  /*inKeySize*/ sizeof(morkRow*), /*inValSize*/ 0,
  inSlots, ioSlotHeap, /*inHoldChanges*/ morkBool_kFalse)
{
  if ( ev->Good() )
    mNode_Derived = morkDerived_kRowMap;
}

/*public non-poly*/ void
morkRowMap::CloseRowMap(morkEnv* ev) // called by CloseMorkNode();
{
  if ( this )
  {
    if ( this->IsNode() )
    {
      this->CloseMap(ev);
      this->MarkShut();
    }
    else
      this->NonNodeError(ev);
  }
  else
    ev->NilPointerError();
}

// } ===== end morkNode methods =====
// ````` ````` ````` ````` ````` 


// { ===== begin morkMap poly interface =====
/*virtual*/ mork_bool // 
morkRowMap::Equal(morkEnv* ev, const void* inKeyA,
  const void* inKeyB) const
{
  MORK_USED_1(ev);
  return (*(const morkRow**) inKeyA)->EqualRow(*(const morkRow**) inKeyB);
}

/*virtual*/ mork_u4 // 
morkRowMap::Hash(morkEnv* ev, const void* inKey) const
{
  MORK_USED_1(ev);
  return (*(const morkRow**) inKey)->HashRow();
}
// } ===== end morkMap poly interface =====


mork_bool
morkRowMap::AddRow(morkEnv* ev, morkRow* ioRow)
{
  if ( ev->Good() )
  {
    this->Put(ev, &ioRow, /*val*/ (void*) 0, 
      /*key*/ (void*) 0, /*val*/ (void*) 0, (mork_change**) 0);
  }
  return ev->Good();
}

morkRow*
morkRowMap::CutOid(morkEnv* ev, const mdbOid* inOid)
{
  morkRow row(inOid);
  morkRow* key = &row;
  morkRow* oldKey = 0;
  this->Cut(ev, &key, &oldKey, /*val*/ (void*) 0,
    (mork_change**) 0);
    
  return oldKey;
}

morkRow*
morkRowMap::CutRow(morkEnv* ev, const morkRow* ioRow)
{
  morkRow* oldKey = 0;
  this->Cut(ev, &ioRow, &oldKey, /*val*/ (void*) 0,
    (mork_change**) 0);
    
  return oldKey;
}

morkRow*
morkRowMap::GetOid(morkEnv* ev, const mdbOid* inOid)
{
  morkRow row(inOid);
  morkRow* key = &row;
  morkRow* oldKey = 0;
  this->Get(ev, &key, &oldKey, /*val*/ (void*) 0, (mork_change**) 0);
  
  return oldKey;
}

morkRow*
morkRowMap::GetRow(morkEnv* ev, const morkRow* ioRow)
{
  morkRow* oldKey = 0;
  this->Get(ev, &ioRow, &oldKey, /*val*/ (void*) 0, (mork_change**) 0);
  
  return oldKey;
}



//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

// ````` ````` ````` ````` ````` 
// { ===== begin morkNode interface =====

/*public virtual*/ void
morkRowProbeMap::CloseMorkNode(morkEnv* ev) // CloseRowProbeMap() only if open
{
  if ( this->IsOpenNode() )
  {
    this->MarkClosing();
    this->CloseRowProbeMap(ev);
    this->MarkShut();
  }
}

/*public virtual*/
morkRowProbeMap::~morkRowProbeMap() // assert CloseRowProbeMap() executed earlier
{
  MORK_ASSERT(this->IsShutNode());
}

/*public non-poly*/
morkRowProbeMap::morkRowProbeMap(morkEnv* ev, const morkUsage& inUsage,
  nsIMdbHeap* ioHeap, nsIMdbHeap* ioSlotHeap, mork_size inSlots)
: morkProbeMap(ev, inUsage,  ioHeap,
  /*inKeySize*/ sizeof(morkRow*), /*inValSize*/ 0,
  ioSlotHeap, inSlots, 
  /*inHoldChanges*/ morkBool_kTrue)
{
  if ( ev->Good() )
    mNode_Derived = morkDerived_kRowProbeMap;
}

/*public non-poly*/ void
morkRowProbeMap::CloseRowProbeMap(morkEnv* ev) // called by CloseMorkNode();
{
  if ( this )
  {
    if ( this->IsNode() )
    {
      this->CloseProbeMap(ev);
      this->MarkShut();
    }
    else
      this->NonNodeError(ev);
  }
  else
    ev->NilPointerError();
}

// } ===== end morkNode methods =====
// ````` ````` ````` ````` ````` 

/*virtual*/ mork_test // hit(a,b) implies hash(a) == hash(b)
morkRowProbeMap::MapTest(morkEnv* ev, const void* inMapKey,
  const void* inAppKey) const
{
  MORK_USED_1(ev);
  const morkRow* key = *(const morkRow**) inMapKey;
  if ( key )
  {
    mork_bool hit = key->EqualRow(*(const morkRow**) inAppKey);
    return ( hit ) ? morkTest_kHit : morkTest_kMiss;
  }
  else
    return morkTest_kVoid;
}

/*virtual*/ mork_u4 // hit(a,b) implies hash(a) == hash(b)
morkRowProbeMap::MapHash(morkEnv* ev, const void* inAppKey) const
{
  const morkRow* key = *(const morkRow**) inAppKey;
  if ( key )
    return key->HashRow();
  else
  {
    ev->NilPointerWarning();
    return 0;
  }
}

/*virtual*/ mork_u4 
morkRowProbeMap::ProbeMapHashMapKey(morkEnv* ev,
  const void* inMapKey) const
{
  const morkRow* key = *(const morkRow**) inMapKey;
  if ( key )
    return key->HashRow();
  else
  {
    ev->NilPointerWarning();
    return 0;
  }
}

mork_bool
morkRowProbeMap::AddRow(morkEnv* ev, morkRow* ioRow)
{
  if ( ev->Good() )
  {
    this->MapAtPut(ev, &ioRow, /*val*/ (void*) 0, 
      /*key*/ (void*) 0, /*val*/ (void*) 0);
  }
  return ev->Good();
}

morkRow*
morkRowProbeMap::CutOid(morkEnv* ev, const mdbOid* inOid)
{
  MORK_USED_1(inOid);
  morkProbeMap::ProbeMapCutError(ev);
    
  return 0;
}

morkRow*
morkRowProbeMap::CutRow(morkEnv* ev, const morkRow* ioRow)
{
  MORK_USED_1(ioRow);
  morkProbeMap::ProbeMapCutError(ev);
    
  return 0;
}

morkRow*
morkRowProbeMap::GetOid(morkEnv* ev, const mdbOid* inOid)
{
  morkRow row(inOid);
  morkRow* key = &row;
  morkRow* oldKey = 0;
  this->MapAt(ev, &key, &oldKey, /*val*/ (void*) 0);
  
  return oldKey;
}

morkRow*
morkRowProbeMap::GetRow(morkEnv* ev, const morkRow* ioRow)
{
  morkRow* oldKey = 0;
  this->MapAt(ev, &ioRow, &oldKey, /*val*/ (void*) 0);
  
  return oldKey;
}


//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789
