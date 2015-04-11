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

#ifndef _MORKSTORE_
#include "morkStore.h"
#endif

#ifndef _MORKPOOL_
#include "morkPool.h"
#endif

#ifndef _MORKENV_
#include "morkEnv.h"
#endif

#ifndef _MORKCELL_
#include "morkCell.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

void
morkCell::SetYarn(morkEnv* ev, const mdbYarn* inYarn, morkStore* ioStore)
{
  morkAtom* atom = ioStore->YarnToAtom(ev, inYarn, true /* create */);
  if ( atom )
    this->SetAtom(ev, atom, ioStore->StorePool()); // refcounts atom
}

void
morkCell::GetYarn(morkEnv* ev, mdbYarn* outYarn) const
{
  MORK_USED_1(ev);
  mCell_Atom->GetYarn(outYarn);
}

void
morkCell::AliasYarn(morkEnv* ev, mdbYarn* outYarn) const
{
  MORK_USED_1(ev);
  mCell_Atom->AliasYarn(outYarn);
}
  
  
void
morkCell::SetCellClean()
{
  mork_column col = this->GetColumn();
  this->SetColumnAndChange(col, morkChange_kNil);
}
  
void
morkCell::SetCellDirty()
{
  mork_column col = this->GetColumn();
  this->SetColumnAndChange(col, morkChange_kAdd);
}

void
morkCell::SetAtom(morkEnv* ev, morkAtom* ioAtom, morkPool* ioPool)
  // SetAtom() "acquires" the new ioAtom if non-nil, by calling AddCellUse()
  // to increase the refcount, and puts ioAtom into mCell_Atom.  If the old
  // atom in mCell_Atom is non-nil, then it is "released" first by a call to
  // CutCellUse(), and if the use count then becomes zero, then the old atom
  // is deallocated by returning it to the pool ioPool.  (And this is
  // why ioPool is a parameter to this method.)  Note that ioAtom can be nil
  // to cause the cell to refer to nothing, and the old atom in mCell_Atom
  // can also be nil, and all the atom refcounting is handled correctly.
  //
  // Note that if ioAtom was just created, it typically has a zero use count
  // before calling SetAtom().  But use count is one higher after SetAtom().
{
  morkAtom* oldAtom = mCell_Atom;
  if ( oldAtom != ioAtom ) // ioAtom is not already installed in this cell?
  {
    if ( oldAtom )
    {
      mCell_Atom = 0;
      if ( oldAtom->CutCellUse(ev) == 0 )
      {
      // this was zapping atoms still in use - comment out until davidmc
      // can figure out a better fix.
//        if ( ioPool )
//        {
//          if ( oldAtom->IsBook() )
//            ((morkBookAtom*) oldAtom)->CutBookAtomFromSpace(ev);
            
//          ioPool->ZapAtom(ev, oldAtom);
//        }
//        else
//          ev->NilPointerError();
      }
    }
    if ( ioAtom )
      ioAtom->AddCellUse(ev);
      
    mCell_Atom = ioAtom;
  }
}

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789
