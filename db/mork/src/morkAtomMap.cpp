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

#ifndef _MORKATOMMAP_
#include "morkAtomMap.h"
#endif

#ifndef _MORKATOM_
#include "morkAtom.h"
#endif

#ifndef _MORKINTMAP_
#include "morkIntMap.h"
#endif

#ifndef _MORKROW_
#include "morkRow.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

// ````` ````` ````` ````` ````` 
// { ===== begin morkNode interface =====

/*public virtual*/ void
morkAtomAidMap::CloseMorkNode(morkEnv* ev) // CloseAtomAidMap() only if open
{
  if ( this->IsOpenNode() )
  {
    this->MarkClosing();
    this->CloseAtomAidMap(ev);
    this->MarkShut();
  }
}

/*public virtual*/
morkAtomAidMap::~morkAtomAidMap() // assert CloseAtomAidMap() executed earlier
{
  MORK_ASSERT(this->IsShutNode());
}


/*public non-poly*/
morkAtomAidMap::morkAtomAidMap(morkEnv* ev, const morkUsage& inUsage,
    nsIMdbHeap* ioHeap, nsIMdbHeap* ioSlotHeap)
#ifdef MORK_ENABLE_PROBE_MAPS
: morkProbeMap(ev, inUsage,  ioHeap,
  /*inKeySize*/ sizeof(morkBookAtom*), /*inValSize*/ 0,
  ioSlotHeap, morkAtomAidMap_kStartSlotCount, 
  /*inZeroIsClearKey*/ morkBool_kTrue)
#else /*MORK_ENABLE_PROBE_MAPS*/
: morkMap(ev, inUsage,  ioHeap,
  /*inKeySize*/ sizeof(morkBookAtom*), /*inValSize*/ 0,
  morkAtomAidMap_kStartSlotCount, ioSlotHeap,
  /*inHoldChanges*/ morkBool_kFalse)
#endif /*MORK_ENABLE_PROBE_MAPS*/
{
  if ( ev->Good() )
    mNode_Derived = morkDerived_kAtomAidMap;
}

/*public non-poly*/ void
morkAtomAidMap::CloseAtomAidMap(morkEnv* ev) // called by CloseMorkNode();
{
  if ( this )
  {
    if ( this->IsNode() )
    {
#ifdef MORK_ENABLE_PROBE_MAPS
      this->CloseProbeMap(ev);
#else /*MORK_ENABLE_PROBE_MAPS*/
      this->CloseMap(ev);
#endif /*MORK_ENABLE_PROBE_MAPS*/
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

#ifdef MORK_ENABLE_PROBE_MAPS

  /*virtual*/ mork_test // hit(a,b) implies hash(a) == hash(b)
  morkAtomAidMap::MapTest(morkEnv* ev, const void* inMapKey,
    const void* inAppKey) const
  {
    MORK_USED_1(ev);
    const morkBookAtom* key = *(const morkBookAtom**) inMapKey;
    if ( key )
    {
      mork_bool hit = key->EqualAid(*(const morkBookAtom**) inAppKey);
      return ( hit ) ? morkTest_kHit : morkTest_kMiss;
    }
    else
      return morkTest_kVoid;
  }

  /*virtual*/ mork_u4 // hit(a,b) implies hash(a) == hash(b)
  morkAtomAidMap::MapHash(morkEnv* ev, const void* inAppKey) const
  {
    const morkBookAtom* key = *(const morkBookAtom**) inAppKey;
    if ( key )
      return key->HashAid();
    else
    {
      ev->NilPointerWarning();
      return 0;
    }
  }

  /*virtual*/ mork_u4 
  morkAtomAidMap::ProbeMapHashMapKey(morkEnv* ev,
    const void* inMapKey) const
  {
    const morkBookAtom* key = *(const morkBookAtom**) inMapKey;
    if ( key )
      return key->HashAid();
    else
    {
      ev->NilPointerWarning();
      return 0;
    }
  }
#else /*MORK_ENABLE_PROBE_MAPS*/
  // { ===== begin morkMap poly interface =====
  /*virtual*/ mork_bool // 
  morkAtomAidMap::Equal(morkEnv* ev, const void* inKeyA,
    const void* inKeyB) const
  {
    MORK_USED_1(ev);
    return (*(const morkBookAtom**) inKeyA)->EqualAid(
      *(const morkBookAtom**) inKeyB);
  }

  /*virtual*/ mork_u4 // 
  morkAtomAidMap::Hash(morkEnv* ev, const void* inKey) const
  {
    MORK_USED_1(ev);
    return (*(const morkBookAtom**) inKey)->HashAid();
  }
  // } ===== end morkMap poly interface =====
#endif /*MORK_ENABLE_PROBE_MAPS*/


mork_bool
morkAtomAidMap::AddAtom(morkEnv* ev, morkBookAtom* ioAtom)
{
  if ( ev->Good() )
  {
#ifdef MORK_ENABLE_PROBE_MAPS
    this->MapAtPut(ev, &ioAtom, /*val*/ (void*) 0, 
      /*key*/ (void*) 0, /*val*/ (void*) 0);
#else /*MORK_ENABLE_PROBE_MAPS*/
    this->Put(ev, &ioAtom, /*val*/ (void*) 0, 
      /*key*/ (void*) 0, /*val*/ (void*) 0, (mork_change**) 0);
#endif /*MORK_ENABLE_PROBE_MAPS*/
  }
  return ev->Good();
}

morkBookAtom*
morkAtomAidMap::CutAtom(morkEnv* ev, const morkBookAtom* inAtom)
{
  morkBookAtom* oldKey = 0;
  
#ifdef MORK_ENABLE_PROBE_MAPS
  MORK_USED_1(inAtom);
  morkProbeMap::ProbeMapCutError(ev);
#else /*MORK_ENABLE_PROBE_MAPS*/
  this->Cut(ev, &inAtom, &oldKey, /*val*/ (void*) 0,
    (mork_change**) 0);
#endif /*MORK_ENABLE_PROBE_MAPS*/
    
  return oldKey;
}

morkBookAtom*
morkAtomAidMap::GetAtom(morkEnv* ev, const morkBookAtom* inAtom)
{
  morkBookAtom* key = 0; // old val in the map

#ifdef MORK_ENABLE_PROBE_MAPS
  this->MapAt(ev, &inAtom, &key, /*val*/ (void*) 0);
#else /*MORK_ENABLE_PROBE_MAPS*/
  this->Get(ev, &inAtom, &key, /*val*/ (void*) 0, (mork_change**) 0);
#endif /*MORK_ENABLE_PROBE_MAPS*/
  
  return key;
}

morkBookAtom*
morkAtomAidMap::GetAid(morkEnv* ev, mork_aid inAid)
{
  morkWeeBookAtom weeAtom(inAid);
  morkBookAtom* key = &weeAtom; // we need a pointer
  morkBookAtom* oldKey = 0; // old key in the map

#ifdef MORK_ENABLE_PROBE_MAPS
  this->MapAt(ev, &key, &oldKey, /*val*/ (void*) 0);
#else /*MORK_ENABLE_PROBE_MAPS*/
  this->Get(ev, &key, &oldKey, /*val*/ (void*) 0, (mork_change**) 0);
#endif /*MORK_ENABLE_PROBE_MAPS*/
  
  return oldKey;
}

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789


//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

// ````` ````` ````` ````` ````` 
// { ===== begin morkNode interface =====

/*public virtual*/ void
morkAtomBodyMap::CloseMorkNode(morkEnv* ev) // CloseAtomBodyMap() only if open
{
  if ( this->IsOpenNode() )
  {
    this->MarkClosing();
    this->CloseAtomBodyMap(ev);
    this->MarkShut();
  }
}

/*public virtual*/
morkAtomBodyMap::~morkAtomBodyMap() // assert CloseAtomBodyMap() executed earlier
{
  MORK_ASSERT(this->IsShutNode());
}


/*public non-poly*/
morkAtomBodyMap::morkAtomBodyMap(morkEnv* ev, const morkUsage& inUsage,
    nsIMdbHeap* ioHeap, nsIMdbHeap* ioSlotHeap)
#ifdef MORK_ENABLE_PROBE_MAPS
: morkProbeMap(ev, inUsage,  ioHeap,
  /*inKeySize*/ sizeof(morkBookAtom*), /*inValSize*/ 0,
  ioSlotHeap, morkAtomBodyMap_kStartSlotCount, 
  /*inZeroIsClearKey*/ morkBool_kTrue)
#else /*MORK_ENABLE_PROBE_MAPS*/
: morkMap(ev, inUsage,  ioHeap,
  /*inKeySize*/ sizeof(morkBookAtom*), /*inValSize*/ 0,
  morkAtomBodyMap_kStartSlotCount, ioSlotHeap,
  /*inHoldChanges*/ morkBool_kFalse)
#endif /*MORK_ENABLE_PROBE_MAPS*/
{
  if ( ev->Good() )
    mNode_Derived = morkDerived_kAtomBodyMap;
}

/*public non-poly*/ void
morkAtomBodyMap::CloseAtomBodyMap(morkEnv* ev) // called by CloseMorkNode();
{
  if ( this )
  {
    if ( this->IsNode() )
    {
#ifdef MORK_ENABLE_PROBE_MAPS
      this->CloseProbeMap(ev);
#else /*MORK_ENABLE_PROBE_MAPS*/
      this->CloseMap(ev);
#endif /*MORK_ENABLE_PROBE_MAPS*/
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
#ifdef MORK_ENABLE_PROBE_MAPS

  /*virtual*/ mork_test // hit(a,b) implies hash(a) == hash(b)
  morkAtomBodyMap::MapTest(morkEnv* ev, const void* inMapKey,
    const void* inAppKey) const
  {
    const morkBookAtom* key = *(const morkBookAtom**) inMapKey;
    if ( key )
    {
      return ( key->EqualFormAndBody(ev, *(const morkBookAtom**) inAppKey) ) ?
        morkTest_kHit : morkTest_kMiss;
    }
    else
      return morkTest_kVoid;
  }

  /*virtual*/ mork_u4 // hit(a,b) implies hash(a) == hash(b)
  morkAtomBodyMap::MapHash(morkEnv* ev, const void* inAppKey) const
  {
    const morkBookAtom* key = *(const morkBookAtom**) inAppKey;
    if ( key )
      return key->HashFormAndBody(ev);
    else
      return 0;
  }

  /*virtual*/ mork_u4 
  morkAtomBodyMap::ProbeMapHashMapKey(morkEnv* ev, const void* inMapKey) const
  {
    const morkBookAtom* key = *(const morkBookAtom**) inMapKey;
    if ( key )
      return key->HashFormAndBody(ev);
    else
      return 0;
  }
#else /*MORK_ENABLE_PROBE_MAPS*/
  // { ===== begin morkMap poly interface =====
  /*virtual*/ mork_bool // 
  morkAtomBodyMap::Equal(morkEnv* ev, const void* inKeyA,
    const void* inKeyB) const
  {
    return (*(const morkBookAtom**) inKeyA)->EqualFormAndBody(ev,
      *(const morkBookAtom**) inKeyB);
  }

  /*virtual*/ mork_u4 // 
  morkAtomBodyMap::Hash(morkEnv* ev, const void* inKey) const
  {
    return (*(const morkBookAtom**) inKey)->HashFormAndBody(ev);
  }
  // } ===== end morkMap poly interface =====
#endif /*MORK_ENABLE_PROBE_MAPS*/


mork_bool
morkAtomBodyMap::AddAtom(morkEnv* ev, morkBookAtom* ioAtom)
{
  if ( ev->Good() )
  {
#ifdef MORK_ENABLE_PROBE_MAPS
    this->MapAtPut(ev, &ioAtom, /*val*/ (void*) 0, 
      /*key*/ (void*) 0, /*val*/ (void*) 0);
#else /*MORK_ENABLE_PROBE_MAPS*/
    this->Put(ev, &ioAtom, /*val*/ (void*) 0, 
      /*key*/ (void*) 0, /*val*/ (void*) 0, (mork_change**) 0);
#endif /*MORK_ENABLE_PROBE_MAPS*/
  }
  return ev->Good();
}

morkBookAtom*
morkAtomBodyMap::CutAtom(morkEnv* ev, const morkBookAtom* inAtom)
{
  morkBookAtom* oldKey = 0;

#ifdef MORK_ENABLE_PROBE_MAPS
  MORK_USED_1(inAtom);
  morkProbeMap::ProbeMapCutError(ev);
#else /*MORK_ENABLE_PROBE_MAPS*/
  this->Cut(ev, &inAtom, &oldKey, /*val*/ (void*) 0,
    (mork_change**) 0);
#endif /*MORK_ENABLE_PROBE_MAPS*/
    
  return oldKey;
}

morkBookAtom*
morkAtomBodyMap::GetAtom(morkEnv* ev, const morkBookAtom* inAtom)
{
  morkBookAtom* key = 0; // old val in the map
#ifdef MORK_ENABLE_PROBE_MAPS
  this->MapAt(ev, &inAtom, &key, /*val*/ (void*) 0);
#else /*MORK_ENABLE_PROBE_MAPS*/
  this->Get(ev, &inAtom, &key, /*val*/ (void*) 0, (mork_change**) 0);
#endif /*MORK_ENABLE_PROBE_MAPS*/
  
  return key;
}

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

morkAtomRowMap::~morkAtomRowMap()
{
}

// I changed to sizeof(mork_ip) from sizeof(mork_aid) to fix a crash on
// 64 bit machines.  I am not sure it was the right way to fix the problem,
// but it does stop the crash.  Perhaps we should be using the
// morkPointerMap instead?
morkAtomRowMap::morkAtomRowMap(morkEnv* ev, const morkUsage& inUsage,
  nsIMdbHeap* ioHeap, nsIMdbHeap* ioSlotHeap, mork_column inIndexColumn)
  : morkIntMap(ev, inUsage, sizeof(mork_ip), ioHeap, ioSlotHeap,
    /*inHoldChanges*/ morkBool_kFalse)
, mAtomRowMap_IndexColumn( inIndexColumn )
{
  if ( ev->Good() )
    mNode_Derived = morkDerived_kAtomRowMap;
}

void morkAtomRowMap::AddRow(morkEnv* ev, morkRow* ioRow)
// add ioRow only if it contains a cell in mAtomRowMap_IndexColumn. 
{
  mork_aid aid = ioRow->GetCellAtomAid(ev, mAtomRowMap_IndexColumn);
  if ( aid )
    this->AddAid(ev, aid, ioRow);
}

void morkAtomRowMap::CutRow(morkEnv* ev, morkRow* ioRow)
// cut ioRow only if it contains a cell in mAtomRowMap_IndexColumn. 
{
  mork_aid aid = ioRow->GetCellAtomAid(ev, mAtomRowMap_IndexColumn);
  if ( aid )
    this->CutAid(ev, aid);
}

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789
