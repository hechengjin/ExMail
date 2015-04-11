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

#ifndef _MORKROWOBJECT_
#include "morkRowObject.h"
#endif

#ifndef _MORKENV_
#include "morkEnv.h"
#endif

#ifndef _MORKSTORE_
#include "morkStore.h"
#endif

#ifndef _MORKROWCELLCURSOR_
#include "morkRowCellCursor.h"
#endif

#ifndef _MORKCELLOBJECT_
#include "morkCellObject.h"
#endif

#ifndef _MORKROW_
#include "morkRow.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

// ````` ````` ````` ````` ````` 
// { ===== begin morkNode interface =====

/*public virtual*/ void
morkRowObject::CloseMorkNode(morkEnv* ev) // CloseRowObject() only if open
{
  if ( this->IsOpenNode() )
  {
    this->MarkClosing();
    this->CloseRowObject(ev);
    this->MarkShut();
  }
}

/*public virtual*/
morkRowObject::~morkRowObject() // assert CloseRowObject() executed earlier
{
  CloseMorkNode(mMorkEnv);
  MORK_ASSERT(this->IsShutNode());
}

/*public non-poly*/
morkRowObject::morkRowObject(morkEnv* ev,
  const morkUsage& inUsage, nsIMdbHeap* ioHeap,
     morkRow* ioRow, morkStore* ioStore)
: morkObject(ev, inUsage, ioHeap, morkColor_kNone, (morkHandle*) 0)
, mRowObject_Row( 0 )
, mRowObject_Store( 0 )
{
  if ( ev->Good() )
  {
    if ( ioRow && ioStore )
    {
      mRowObject_Row = ioRow;
      mRowObject_Store = ioStore; // morkRowObjects don't ref-cnt the owning store.
      
      if ( ev->Good() )
        mNode_Derived = morkDerived_kRowObject;
    }
    else
      ev->NilPointerError();
  }
}

NS_IMPL_ISUPPORTS_INHERITED1(morkRowObject, morkObject, nsIMdbRow)
// { ===== begin nsIMdbCollection methods =====

// { ----- begin attribute methods -----
NS_IMETHODIMP
morkRowObject::GetSeed(nsIMdbEnv* mev,
  mdb_seed* outSeed)
{
  mdb_err outErr = NS_OK;
  morkEnv* ev = morkEnv::FromMdbEnv(mev);
  if ( ev )
  {
    *outSeed = (mdb_seed) mRowObject_Row->mRow_Seed;
    outErr = ev->AsErr();
  }
  return outErr;
}
NS_IMETHODIMP
morkRowObject::GetCount(nsIMdbEnv* mev,
  mdb_count* outCount)
{
  mdb_err outErr = NS_OK;
  morkEnv* ev = morkEnv::FromMdbEnv(mev);
  if ( ev )
  {
    *outCount = (mdb_count) mRowObject_Row->mRow_Length;
    outErr = ev->AsErr();
  }
  return outErr;
}

NS_IMETHODIMP
morkRowObject::GetPort(nsIMdbEnv* mev,
  nsIMdbPort** acqPort)
{
  mdb_err outErr = NS_OK;
  nsIMdbPort* outPort = 0;
  morkEnv* ev = morkEnv::FromMdbEnv(mev);
  if ( ev )
  {
    morkRowSpace* rowSpace = mRowObject_Row->mRow_Space;
    if ( rowSpace && rowSpace->mSpace_Store )
    {
      morkStore* store = mRowObject_Row->GetRowSpaceStore(ev);
      if ( store )
        outPort = store->AcquireStoreHandle(ev);
    }
    else
      ev->NilPointerError();
      
    outErr = ev->AsErr();
  }
  if ( acqPort )
    *acqPort = outPort;
    
  return outErr;
}
// } ----- end attribute methods -----

// { ----- begin cursor methods -----
NS_IMETHODIMP
morkRowObject::GetCursor( // make a cursor starting iter at inMemberPos
  nsIMdbEnv* mev, // context
  mdb_pos inMemberPos, // zero-based ordinal pos of member in collection
  nsIMdbCursor** acqCursor)
{
  return this->GetRowCellCursor(mev, inMemberPos,
    (nsIMdbRowCellCursor**) acqCursor);
}
// } ----- end cursor methods -----

// { ----- begin ID methods -----
NS_IMETHODIMP
morkRowObject::GetOid(nsIMdbEnv* mev,
  mdbOid* outOid)
{
  *outOid = mRowObject_Row->mRow_Oid;
  morkEnv* ev = morkEnv::FromMdbEnv(mev);
  return (ev) ? ev->AsErr() : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
morkRowObject::BecomeContent(nsIMdbEnv* mev,
  const mdbOid* inOid)
{
  NS_ASSERTION(false, "not implemented");
  return NS_ERROR_NOT_IMPLEMENTED;
  // remember row->MaybeDirtySpaceStoreAndRow();
}
// } ----- end ID methods -----

// { ----- begin activity dropping methods -----
NS_IMETHODIMP
morkRowObject::DropActivity( // tell collection usage no longer expected
  nsIMdbEnv* mev)
{
  NS_ASSERTION(false, "not implemented");
  return NS_ERROR_NOT_IMPLEMENTED;
}
// } ----- end activity dropping methods -----

// } ===== end nsIMdbCollection methods =====

// { ===== begin nsIMdbRow methods =====

// { ----- begin cursor methods -----
NS_IMETHODIMP
morkRowObject::GetRowCellCursor( // make a cursor starting iteration at inCellPos
  nsIMdbEnv* mev, // context
  mdb_pos inPos, // zero-based ordinal position of cell in row
  nsIMdbRowCellCursor** acqCursor)
{
  mdb_err outErr = NS_OK;
  morkEnv* ev = morkEnv::FromMdbEnv(mev);
  nsIMdbRowCellCursor* outCursor = 0;
  if ( ev )
  {
    morkRowCellCursor* cursor = mRowObject_Row->NewRowCellCursor(ev, inPos);
    if ( cursor )
    {
      if ( ev->Good() )
      {
        cursor->mCursor_Seed = (mork_seed) inPos;
        outCursor = cursor;
        NS_ADDREF(cursor);
      }
    }
    outErr = ev->AsErr();
  }
  if ( acqCursor )
    *acqCursor = outCursor;
  return outErr;
}
// } ----- end cursor methods -----

// { ----- begin column methods -----
NS_IMETHODIMP
morkRowObject::AddColumn( // make sure a particular column is inside row
  nsIMdbEnv* mev, // context
  mdb_column inColumn, // column to add
  const mdbYarn* inYarn)
{
  mdb_err outErr = NS_ERROR_FAILURE;
  morkEnv* ev = morkEnv::FromMdbEnv(mev);
  if ( ev )
  {
    if ( mRowObject_Store && mRowObject_Row)
      mRowObject_Row->AddColumn(ev, inColumn, inYarn, mRowObject_Store);
      
    outErr = ev->AsErr();
  }
  return outErr;
}

NS_IMETHODIMP
morkRowObject::CutColumn( // make sure a column is absent from the row
  nsIMdbEnv* mev, // context
  mdb_column inColumn)
{
  mdb_err outErr = NS_ERROR_FAILURE;
  morkEnv* ev = morkEnv::FromMdbEnv(mev);
  if ( ev )
  {
    mRowObject_Row->CutColumn(ev, inColumn);
    outErr = ev->AsErr();
  }
  return outErr;
}

NS_IMETHODIMP
morkRowObject::CutAllColumns( // remove all columns from the row
  nsIMdbEnv* mev)
{
  mdb_err outErr = NS_OK;
  morkEnv* ev = morkEnv::FromMdbEnv(mev);
  if ( ev )
  {
    mRowObject_Row->CutAllColumns(ev);
    outErr = ev->AsErr();
  }
  return outErr;
}
// } ----- end column methods -----

// { ----- begin cell methods -----
NS_IMETHODIMP
morkRowObject::NewCell( // get cell for specified column, or add new one
  nsIMdbEnv* mev, // context
  mdb_column inColumn, // column to add
  nsIMdbCell** acqCell)
{
  mdb_err outErr = NS_OK;
  morkEnv* ev = morkEnv::FromMdbEnv(mev);
  if ( ev )
  {
    GetCell(mev, inColumn, acqCell);
    if ( !*acqCell )
    {
      if ( mRowObject_Store )
      {
        mdbYarn yarn; // to pass empty yarn into morkRowObject::AddColumn()
        yarn.mYarn_Buf = 0;
        yarn.mYarn_Fill = 0;
        yarn.mYarn_Size = 0;
        yarn.mYarn_More = 0;
        yarn.mYarn_Form = 0;
        yarn.mYarn_Grow = 0;
        AddColumn(ev, inColumn, &yarn);
        GetCell(mev, inColumn, acqCell);
      }
    }
      
    outErr = ev->AsErr();
  }
  return outErr;
}
  
NS_IMETHODIMP
morkRowObject::AddCell( // copy a cell from another row to this row
  nsIMdbEnv* mev, // context
  const nsIMdbCell* inCell)
{
  mdb_err outErr = NS_OK;
  morkEnv* ev = morkEnv::FromMdbEnv(mev);
  if ( ev )
  {
    morkCell* cell = 0;
    morkCellObject* cellObj = (morkCellObject*) inCell;
    if ( cellObj->CanUseCell(mev, morkBool_kFalse, &outErr, &cell) )
    {

      morkRow* cellRow = cellObj->mCellObject_Row;
      if ( cellRow )
      {
        if ( mRowObject_Row != cellRow )
        {
          morkStore* store = mRowObject_Row->GetRowSpaceStore(ev);
          morkStore* cellStore = cellRow->GetRowSpaceStore(ev);
          if ( store && cellStore )
          {
            mork_column col = cell->GetColumn();
            morkAtom* atom = cell->mCell_Atom;
            mdbYarn yarn;
            atom->AliasYarn(&yarn); // works even when atom is nil
            
            if ( store != cellStore )
              col = store->CopyToken(ev, col, cellStore);
            if ( ev->Good() )
              AddColumn(ev, col, &yarn);
          }
          else
            ev->NilPointerError();
        }
      }
      else
        ev->NilPointerError();
    }

    outErr = ev->AsErr();
  }
  return outErr;
}
  
NS_IMETHODIMP
morkRowObject::GetCell( // find a cell in this row
  nsIMdbEnv* mev, // context
  mdb_column inColumn, // column to find
  nsIMdbCell** acqCell)
{
  mdb_err outErr = NS_OK;
  nsIMdbCell* outCell = 0;
  morkEnv* ev = morkEnv::FromMdbEnv(mev);

  if ( ev )
  {
    if ( inColumn )
    {
      mork_pos pos = 0;
      morkCell* cell = mRowObject_Row->GetCell(ev, inColumn, &pos);
      if ( cell )
      {
        outCell = mRowObject_Row->AcquireCellHandle(ev, cell, inColumn, pos);
      }
    }
    else
      mRowObject_Row->ZeroColumnError(ev);
      
    outErr = ev->AsErr();
  }
  if ( acqCell )
    *acqCell = outCell;
  return outErr;
}
  
NS_IMETHODIMP
morkRowObject::EmptyAllCells( // make all cells in row empty of content
  nsIMdbEnv* mev)
{
  mdb_err outErr = NS_OK;
  morkEnv* ev = morkEnv::FromMdbEnv(mev);
  if ( ev )
  {
    EmptyAllCells(ev);
    outErr = ev->AsErr();
  }
  return outErr;
}
// } ----- end cell methods -----

// { ----- begin row methods -----
NS_IMETHODIMP
morkRowObject::AddRow( // add all cells in another row to this one
  nsIMdbEnv* mev, // context
  nsIMdbRow* ioSourceRow)
{
  mdb_err outErr = NS_OK;
  morkEnv* ev = morkEnv::FromMdbEnv(mev);
  if ( ev )
  {
    morkRow* unsafeSource = (morkRow*) ioSourceRow; // unsafe cast
//    if ( unsafeSource->CanUseRow(mev, morkBool_kFalse, &outErr, &source) )
    {
      mRowObject_Row->AddRow(ev, unsafeSource);
    }
    outErr = ev->AsErr();
  }
  return outErr;
}
  
NS_IMETHODIMP
morkRowObject::SetRow( // make exact duplicate of another row
  nsIMdbEnv* mev, // context
  nsIMdbRow* ioSourceRow)
{
  mdb_err outErr = NS_OK;
  morkEnv* ev = morkEnv::FromMdbEnv(mev);
  if ( ev )
  {
    morkRowObject *sourceObject = (morkRowObject *) ioSourceRow; // unsafe cast
    morkRow* unsafeSource = sourceObject->mRowObject_Row;
//    if ( unsafeSource->CanUseRow(mev, morkBool_kFalse, &outErr, &source) )
    {
      mRowObject_Row->SetRow(ev, unsafeSource);
    }
    outErr = ev->AsErr();
  }
  return outErr;
}
// } ----- end row methods -----

// { ----- begin blob methods -----
NS_IMETHODIMP
morkRowObject::SetCellYarn( // synonym for AddColumn()
  nsIMdbEnv* mev, // context
  mdb_column inColumn, // column to add
  const mdbYarn* inYarn)
{
  mdb_err outErr = NS_OK;
  morkEnv* ev = morkEnv::FromMdbEnv(mev);
  if ( ev )
  {
    if ( mRowObject_Store )
      AddColumn(ev, inColumn, inYarn);
      
    outErr = ev->AsErr();
  }
  return outErr;
}
NS_IMETHODIMP
morkRowObject::GetCellYarn(
  nsIMdbEnv* mev, // context
  mdb_column inColumn, // column to read 
  mdbYarn* outYarn)  // writes some yarn slots 
// copy content into the yarn buffer, and update mYarn_Fill and mYarn_Form
{
  mdb_err outErr = NS_OK;
  morkEnv* ev = morkEnv::FromMdbEnv(mev);
  if ( ev )
  {
    if ( mRowObject_Store && mRowObject_Row)
    {
      morkAtom* atom = mRowObject_Row->GetColumnAtom(ev, inColumn);
      atom->GetYarn(outYarn);
      // note nil atom works and sets yarn correctly
    }
      
    outErr = ev->AsErr();
  }
  return outErr;
}

NS_IMETHODIMP
morkRowObject::AliasCellYarn(
  nsIMdbEnv* mev, // context
    mdb_column inColumn, // column to alias
    mdbYarn* outYarn) // writes ALL yarn slots
{
  mdb_err outErr = NS_OK;
  morkEnv* ev = morkEnv::FromMdbEnv(mev);
  if ( ev )
  {
    if ( mRowObject_Store && mRowObject_Row)
    {
      morkAtom* atom = mRowObject_Row->GetColumnAtom(ev, inColumn);
      atom->AliasYarn(outYarn);
      // note nil atom works and sets yarn correctly
    }
    outErr = ev->AsErr();
  }
  return outErr;
}

NS_IMETHODIMP
morkRowObject::NextCellYarn(nsIMdbEnv* mev, // iterative version of GetCellYarn()
  mdb_column* ioColumn, // next column to read
  mdbYarn* outYarn)  // writes some yarn slots 
// copy content into the yarn buffer, and update mYarn_Fill and mYarn_Form
//
// The ioColumn argument is an inout parameter which initially contains the
// last column accessed and returns the next column corresponding to the
// content read into the yarn.  Callers should start with a zero column
// value to say 'no previous column', which causes the first column to be
// read.  Then the value returned in ioColumn is perfect for the next call
// to NextCellYarn(), since it will then be the previous column accessed.
// Callers need only examine the column token returned to see which cell
// in the row is being read into the yarn.  When no more columns remain,
// and the iteration has ended, ioColumn will return a zero token again.
// So iterating over cells starts and ends with a zero column token.
{
  mdb_err outErr = NS_OK;
  morkEnv* ev = morkEnv::FromMdbEnv(mev);
  if ( ev )
  {
    if ( mRowObject_Store && mRowObject_Row)
      mRowObject_Row->NextColumn(ev, ioColumn, outYarn);
      
    outErr = ev->AsErr();
  }
  return outErr;
}

NS_IMETHODIMP
morkRowObject::SeekCellYarn( // resembles nsIMdbRowCellCursor::SeekCell()
  nsIMdbEnv* mev, // context
  mdb_pos inPos, // position of cell in row sequence
  mdb_column* outColumn, // column for this particular cell
  mdbYarn* outYarn) // writes some yarn slots
// copy content into the yarn buffer, and update mYarn_Fill and mYarn_Form
// Callers can pass nil for outYarn to indicate no interest in content, so
// only the outColumn value is returned.  NOTE to subclasses: you must be
// able to ignore outYarn when the pointer is nil; please do not crash.

{
  mdb_err outErr = NS_OK;
  morkEnv* ev = morkEnv::FromMdbEnv(mev);
  if ( ev )
  {
    if ( mRowObject_Store && mRowObject_Row)
      mRowObject_Row->SeekColumn(ev, inPos, outColumn, outYarn);
      
    outErr = ev->AsErr();
  }
  return outErr;
}

// } ----- end blob methods -----


// } ===== end nsIMdbRow methods =====



/*public non-poly*/ void
morkRowObject::CloseRowObject(morkEnv* ev) // called by CloseMorkNode();
{
  if ( this )
  {
    if ( this->IsNode() )
    {
      morkRow* row = mRowObject_Row;
      mRowObject_Row = 0;
      this->CloseObject(ev);
      this->MarkShut();

      if ( row )
      {
        MORK_ASSERT(row->mRow_Object == this);
        if ( row->mRow_Object == this )
        {
          row->mRow_Object = 0; // just nil this slot -- cut ref down below
          
          mRowObject_Store = 0; // morkRowObjects don't ref-cnt the owning store.
            
          this->CutWeakRef(ev->AsMdbEnv()); // do last, because it might self destroy
        }
      }
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
morkRowObject::NonRowObjectTypeError(morkEnv* ev)
{
  ev->NewError("non morkRowObject");
}

/*static*/ void
morkRowObject::NilRowError(morkEnv* ev)
{
  ev->NewError("nil mRowObject_Row");
}

/*static*/ void
morkRowObject::NilStoreError(morkEnv* ev)
{
  ev->NewError("nil mRowObject_Store");
}

/*static*/ void
morkRowObject::RowObjectRowNotSelfError(morkEnv* ev)
{
  ev->NewError("mRowObject_Row->mRow_Object != self");
}


nsIMdbRow*
morkRowObject::AcquireRowHandle(morkEnv* ev) // mObject_Handle
{
  AddRef();
  return this;
}


//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789
