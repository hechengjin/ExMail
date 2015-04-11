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

#ifndef _MORKMAP_
#include "morkMap.h"
#endif

#ifndef _MORKSPACE_
#include "morkSpace.h"
#endif

#ifndef _MORKMAP_
#include "morkMap.h"
#endif

#ifndef _MORKENV_
#include "morkEnv.h"
#endif

#ifndef _MORKSTORE_
#include "morkStore.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

// ````` ````` ````` ````` ````` 
// { ===== begin morkNode interface =====

/*public virtual*/ void
morkSpace::CloseMorkNode(morkEnv* ev) // CloseSpace() only if open
{
  if ( this->IsOpenNode() )
  {
    this->MarkClosing();
    this->CloseSpace(ev);
    this->MarkShut();
  }
}

/*public virtual*/
morkSpace::~morkSpace() // assert CloseSpace() executed earlier
{
  MORK_ASSERT(SpaceScope()==0);
  MORK_ASSERT(mSpace_Store==0);
  MORK_ASSERT(this->IsShutNode());
}    

/*public non-poly*/
//morkSpace::morkSpace(morkEnv* ev, const morkUsage& inUsage,
//  nsIMdbHeap* ioNodeHeap, const morkMapForm& inForm,
//  nsIMdbHeap* ioSlotHeap)
//: morkNode(ev, inUsage, ioNodeHeap)
//, mSpace_Map(ev, morkUsage::kMember, (nsIMdbHeap*) 0, ioSlotHeap)
//{
//  ev->StubMethodOnlyError();
//}

/*public non-poly*/
morkSpace::morkSpace(morkEnv* ev,
  const morkUsage& inUsage, mork_scope inScope, morkStore* ioStore,
  nsIMdbHeap* ioHeap, nsIMdbHeap* ioSlotHeap)
: morkBead(ev, inUsage, ioHeap, inScope)
, mSpace_Store( 0 )
, mSpace_DoAutoIDs( morkBool_kFalse )
, mSpace_HaveDoneAutoIDs( morkBool_kFalse )
, mSpace_CanDirty( morkBool_kFalse ) // only when store can be dirtied
{
  if ( ev->Good() )
  {
    if ( ioStore && ioSlotHeap )
    {
      morkStore::SlotWeakStore(ioStore, ev, &mSpace_Store);

      mSpace_CanDirty = ioStore->mStore_CanDirty;
      if ( mSpace_CanDirty ) // this new space dirties the store?
        this->MaybeDirtyStoreAndSpace();
        
      if ( ev->Good() )
        mNode_Derived = morkDerived_kSpace;
    }
    else
      ev->NilPointerError();
  }
}

/*public non-poly*/ void
morkSpace::CloseSpace(morkEnv* ev) // called by CloseMorkNode();
{
  if ( this )
  {
    if ( this->IsNode() )
    {
      morkStore::SlotWeakStore((morkStore*) 0, ev, &mSpace_Store);
      mBead_Color = 0; // this->CloseBead();
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

/*static*/ void 
morkSpace::NonAsciiSpaceScopeName(morkEnv* ev)
{
  ev->NewError("SpaceScope() > 0x7F");
}

/*static*/ void 
morkSpace::NilSpaceStoreError(morkEnv* ev)
{
  ev->NewError("nil mSpace_Store");
}

morkPool* morkSpace::GetSpaceStorePool() const
{
  return &mSpace_Store->mStore_Pool;
}

mork_bool morkSpace::MaybeDirtyStoreAndSpace()
{
  morkStore* store = mSpace_Store;
  if ( store && store->mStore_CanDirty )
  {
    store->SetStoreDirty();
    mSpace_CanDirty = morkBool_kTrue;
  }
  
  if ( mSpace_CanDirty )
  {
    this->SetSpaceDirty();
    return morkBool_kTrue;
  }
  
  return morkBool_kFalse;
}


//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789
