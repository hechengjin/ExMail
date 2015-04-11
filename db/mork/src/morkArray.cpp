/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-  */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nscore.h"

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

#ifndef _MORKARRAY_
#include "morkArray.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

// ````` ````` ````` ````` ````` 
// { ===== begin morkNode interface =====

/*public virtual*/ void
morkArray::CloseMorkNode(morkEnv* ev) // CloseTable() only if open
{
  if ( this->IsOpenNode() )
  {
    this->MarkClosing();
    this->CloseArray(ev);
    this->MarkShut();
  }
}

/*public virtual*/
morkArray::~morkArray() // assert CloseTable() executed earlier
{
  MORK_ASSERT(this->IsShutNode());
  MORK_ASSERT(mArray_Slots==0);
}

/*public non-poly*/
morkArray::morkArray(morkEnv* ev, const morkUsage& inUsage,
    nsIMdbHeap* ioHeap, mork_size inSize, nsIMdbHeap* ioSlotHeap)
: morkNode(ev, inUsage, ioHeap)
, mArray_Slots( 0 )
, mArray_Heap( 0 )
, mArray_Fill( 0 )
, mArray_Size( 0 )
, mArray_Seed( (mork_u4)NS_PTR_TO_INT32(this) ) // "random" integer assignment
{
  if ( ev->Good() )
  {
    if ( ioSlotHeap )
    {
      nsIMdbHeap_SlotStrongHeap(ioSlotHeap, ev, &mArray_Heap);
      if ( ev->Good() )
      {
        if ( inSize < 3 )
          inSize = 3;
        mdb_size byteSize = inSize * sizeof(void*);
        void** block = 0;
        ioSlotHeap->Alloc(ev->AsMdbEnv(), byteSize, (void**) &block);
        if ( block && ev->Good() )
        {
          mArray_Slots = block;
          mArray_Size = inSize;
          MORK_MEMSET(mArray_Slots, 0, byteSize);
          if ( ev->Good() )
            mNode_Derived = morkDerived_kArray;
        }
      }
    }
    else
      ev->NilPointerError();
  }
}

/*public non-poly*/ void
morkArray::CloseArray(morkEnv* ev) // called by CloseMorkNode();
{
  if ( this )
  {
    if ( this->IsNode() )
    {
      if ( mArray_Heap && mArray_Slots )
        mArray_Heap->Free(ev->AsMdbEnv(), mArray_Slots);
        
      mArray_Slots = 0;
      mArray_Size = 0;
      mArray_Fill = 0;
      ++mArray_Seed;
      nsIMdbHeap_SlotStrongHeap((nsIMdbHeap*) 0, ev, &mArray_Heap);
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
morkArray::NonArrayTypeError(morkEnv* ev)
{
  ev->NewError("non morkArray");
}

/*static*/ void
morkArray::IndexBeyondEndError(morkEnv* ev)
{
  ev->NewError("array index beyond end");
}

/*static*/ void
morkArray::NilSlotsAddressError(morkEnv* ev)
{
  ev->NewError("nil mArray_Slots");
}

/*static*/ void
morkArray::FillBeyondSizeError(morkEnv* ev)
{
  ev->NewError("mArray_Fill > mArray_Size");
}

mork_bool
morkArray::Grow(morkEnv* ev, mork_size inNewSize)
// Grow() returns true if capacity becomes >= inNewSize and ev->Good()
{
  if ( ev->Good() && inNewSize > mArray_Size ) // make array larger?
  {
    if ( mArray_Fill <= mArray_Size ) // fill and size fit the invariant?
    {
      if (mArray_Size <= 3)
        inNewSize = mArray_Size + 3;
      else
        inNewSize = mArray_Size  * 2;// + 3;  // try doubling size here - used to grow by 3
        
      mdb_size newByteSize = inNewSize * sizeof(void*);
      void** newBlock = 0;
      mArray_Heap->Alloc(ev->AsMdbEnv(), newByteSize, (void**) &newBlock);
      if ( newBlock && ev->Good() ) // okay new block?
      {
        void** oldSlots = mArray_Slots;
        void** oldEnd = oldSlots + mArray_Fill;
        
        void** newSlots = newBlock;
        void** newEnd = newBlock + inNewSize;
        
        while ( oldSlots < oldEnd )
          *newSlots++ = *oldSlots++;
          
        while ( newSlots < newEnd )
          *newSlots++ = (void*) 0;

        oldSlots = mArray_Slots;
        mArray_Size = inNewSize;
        mArray_Slots = newBlock;
        mArray_Heap->Free(ev->AsMdbEnv(), oldSlots);
      }
    }
    else
      this->FillBeyondSizeError(ev);
  }
  ++mArray_Seed; // always modify seed, since caller intends to add slots
  return ( ev->Good() && mArray_Size >= inNewSize );
}

void*
morkArray::SafeAt(morkEnv* ev, mork_pos inPos)
{
  if ( mArray_Slots )
  {
    if ( inPos >= 0 && inPos < (mork_pos) mArray_Fill )
      return mArray_Slots[ inPos ];
    else
      this->IndexBeyondEndError(ev);
  }
  else
    this->NilSlotsAddressError(ev);
    
  return (void*) 0;
}

void
morkArray::SafeAtPut(morkEnv* ev, mork_pos inPos, void* ioSlot)
{
  if ( mArray_Slots )
  {
    if ( inPos >= 0 && inPos < (mork_pos) mArray_Fill )
    {
      mArray_Slots[ inPos ] = ioSlot;
      ++mArray_Seed;
    }
    else
      this->IndexBeyondEndError(ev);
  }
  else
    this->NilSlotsAddressError(ev);
}

mork_pos
morkArray::AppendSlot(morkEnv* ev, void* ioSlot)
{
  mork_pos outPos = -1;
  if ( mArray_Slots )
  {
    mork_fill fill = mArray_Fill;
    if ( this->Grow(ev, fill+1) )
    {
      outPos = (mork_pos) fill;
      mArray_Slots[ fill ] = ioSlot;
      mArray_Fill = fill + 1;
      // note Grow() increments mArray_Seed
    }
  }
  else
    this->NilSlotsAddressError(ev);
    
  return outPos;
}

void
morkArray::AddSlot(morkEnv* ev, mork_pos inPos, void* ioSlot)
{
  if ( mArray_Slots )
  {
    mork_fill fill = mArray_Fill;
    if ( this->Grow(ev, fill+1) )
    {
      void** slot = mArray_Slots; // the slot vector
      void** end = slot + fill; // one past the last used array slot
      slot += inPos; // the slot to be added

      while ( --end >= slot ) // another slot to move upward?
        end[ 1 ] = *end;

      *slot = ioSlot;
      mArray_Fill = fill + 1;
      // note Grow() increments mArray_Seed
    }
  }
  else
    this->NilSlotsAddressError(ev);
}

void
morkArray::CutSlot(morkEnv* ev, mork_pos inPos)
{
  MORK_USED_1(ev);
  mork_fill fill = mArray_Fill;
  if ( inPos >= 0 && inPos < (mork_pos) fill ) // cutting slot in used array portion?
  {
    void** slot = mArray_Slots; // the slot vector
    void** end = slot + fill; // one past the last used array slot
    slot += inPos; // the slot to be cut
    
    while ( ++slot < end ) // another slot to move downward?
      slot[ -1 ] = *slot;
      
    slot[ -1 ] = 0; // clear the last used slot which is now unused
    
    // note inPos<fill implies fill>0, so fill-1 must be nonnegative:
    mArray_Fill = fill - 1;
    ++mArray_Seed;
  }
}

void
morkArray::CutAllSlots(morkEnv* ev)
{
  if ( mArray_Slots )
  {
    if ( mArray_Fill <= mArray_Size )
    {
      mdb_size oldByteSize = mArray_Fill * sizeof(void*);
      MORK_MEMSET(mArray_Slots, 0, oldByteSize);
    }
    else
      this->FillBeyondSizeError(ev);
  }
  else
    this->NilSlotsAddressError(ev);

  ++mArray_Seed;
  mArray_Fill = 0;
}

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789
