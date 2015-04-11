/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef MapiApi_h___
#define MapiApi_h___

#include "nscore.h"
#include "nsStringGlue.h"
#include "nsVoidArray.h"

#include <stdio.h>

#include <windows.h>
#include <mapi.h>
#include <mapix.h>
#include <mapidefs.h>
#include <mapicode.h>
#include <mapitags.h>
#include <mapiutil.h>
// wabutil.h expects mapiutil to define _MAPIUTIL_H but it actually
// defines _MAPIUTIL_H_
#define _MAPIUTIL_H

#ifndef PR_INTERNET_CPID
#define PR_INTERNET_CPID (PROP_TAG(PT_LONG,0x3FDE))
#endif
#ifndef MAPI_NATIVE_BODY
#define MAPI_NATIVE_BODY (0x00010000)
#endif
#ifndef MAPI_NATIVE_BODY_TYPE_RTF
#define MAPI_NATIVE_BODY_TYPE_RTF (0x00000001)
#endif
#ifndef MAPI_NATIVE_BODY_TYPE_HTML
#define MAPI_NATIVE_BODY_TYPE_HTML (0x00000002)
#endif
#ifndef MAPI_NATIVE_BODY_TYPE_PLAINTEXT
#define MAPI_NATIVE_BODY_TYPE_PLAINTEXT (0x00000004)
#endif
#ifndef PR_BODY_HTML_A
#define PR_BODY_HTML_A (PROP_TAG(PT_STRING8,0x1013))
#endif
#ifndef PR_BODY_HTML_W
#define PR_BODY_HTML_W (PROP_TAG(PT_UNICODE,0x1013))
#endif
#ifndef PR_BODY_HTML
#define PR_BODY_HTML (PROP_TAG(PT_TSTRING,0x1013))
#endif

class CMapiFolderList;
class CMsgStore;
class CMapiFolder;

class CMapiContentIter {
public:
  virtual BOOL HandleContentItem(ULONG oType, ULONG cb, LPENTRYID pEntry) = 0;
};

class CMapiHierarchyIter {
public:
  virtual BOOL HandleHierarchyItem(ULONG oType, ULONG cb, LPENTRYID pEntry) = 0;
};

class CMapiApi {
public:
  CMapiApi();
  ~CMapiApi();

  static BOOL    LoadMapi(void);
  static BOOL    LoadMapiEntryPoints(void);
  static void    UnloadMapi(void);

  static HINSTANCE  m_hMapi32;

  static void    MAPIUninitialize(void);
  static HRESULT  MAPIInitialize(LPVOID lpInit);
  static SCODE  MAPIAllocateBuffer(ULONG cbSize, LPVOID FAR * lppBuffer);
  static ULONG  MAPIFreeBuffer(LPVOID lpBuff);
  static HRESULT  MAPILogonEx(ULONG ulUIParam, LPTSTR lpszProfileName, LPTSTR lpszPassword, FLAGS flFlags, LPMAPISESSION FAR * lppSession);
  static HRESULT  OpenStreamOnFile(LPALLOCATEBUFFER lpAllocateBuffer, LPFREEBUFFER lpFreeBuffer, ULONG ulFlags, LPTSTR lpszFileName, LPTSTR lpszPrefix, LPSTREAM FAR * lppStream);
  static void    FreeProws(LPSRowSet prows);


  BOOL  Initialize(void);
  BOOL  LogOn(void);

  void  AddMessageStore(CMsgStore *pStore);
  void  SetCurrentMsgStore(LPMDB lpMdb) { m_lpMdb = lpMdb;}

  // Open any given entry from the current Message Store
  BOOL  OpenEntry(ULONG cbEntry, LPENTRYID pEntryId, LPUNKNOWN *ppOpen);
  static BOOL OpenMdbEntry(LPMDB lpMdb, ULONG cbEntry, LPENTRYID pEntryId, LPUNKNOWN *ppOpen);

  // Fill in the folders list with the hierarchy from the given
  // message store.
  BOOL  GetStoreFolders(ULONG cbEid, LPENTRYID lpEid, CMapiFolderList& folders, int startDepth);
  BOOL  GetStoreAddressFolders(ULONG cbEid, LPENTRYID lpEid, CMapiFolderList& folders);
  BOOL  OpenStore(ULONG cbEid, LPENTRYID lpEid, LPMDB *ppMdb);

  // Iteration
  BOOL  IterateStores(CMapiFolderList& list);
  BOOL  IterateContents(CMapiContentIter *pIter, LPMAPIFOLDER pFolder, ULONG flags = 0);
  BOOL  IterateHierarchy(CMapiHierarchyIter *pIter, LPMAPIFOLDER pFolder, ULONG flags = 0);

  // Properties
  static LPSPropValue  GetMapiProperty(LPMAPIPROP pProp, ULONG tag);
  // If delVal is true, functions will call CMapiApi::MAPIFreeBuffer on pVal.
  static BOOL      GetEntryIdFromProp(LPSPropValue pVal, ULONG& cbEntryId,
                                      LPENTRYID& lpEntryId, BOOL delVal = TRUE);
  static BOOL      GetStringFromProp(LPSPropValue pVal, nsCString& val, BOOL delVal = TRUE);
  static BOOL      GetStringFromProp(LPSPropValue pVal, nsString& val, BOOL delVal = TRUE);
  static LONG      GetLongFromProp(LPSPropValue pVal, BOOL delVal = TRUE);
  static BOOL      GetLargeStringProperty(LPMAPIPROP pProp, ULONG tag, nsCString& val);
  static BOOL      GetLargeStringProperty(LPMAPIPROP pProp, ULONG tag, nsString& val);
  static BOOL      IsLargeProperty(LPSPropValue pVal);
  static ULONG    GetEmailPropertyTag(LPMAPIPROP lpProp, LONG nameID);

  static BOOL GetRTFPropertyDecodedAsUTF16(LPMAPIPROP pProp, nsString& val,
                                           unsigned long& nativeBodyType,
                                           unsigned long codepage = 0);

  // Debugging & reporting stuff
  static void      ListProperties(LPMAPIPROP lpProp, BOOL getValues = TRUE);
  static void      ListPropertyValue(LPSPropValue pVal, nsCString& s);

protected:
  BOOL      HandleHierarchyItem(ULONG oType, ULONG cb, LPENTRYID pEntry);
  BOOL      HandleContentsItem(ULONG oType, ULONG cb, LPENTRYID pEntry);
  void      GetStoreInfo(CMapiFolder *pFolder, long *pSzContents);

  // array of available message stores, cached so that
  // message stores are only opened once, preventing multiple
  // logon's by the user if the store requires a logon.
  CMsgStore *    FindMessageStore(ULONG cbEid, LPENTRYID lpEid);
  void      ClearMessageStores(void);

  static void      CStrToUnicode(const char *pStr, nsString& result);

  // Debugging & reporting stuff
  static void      GetPropTagName(ULONG tag, nsCString& s);
  static void      ReportStringProp(const char *pTag, LPSPropValue pVal);
  static void      ReportUIDProp(const char *pTag, LPSPropValue pVal);
  static void      ReportLongProp(const char *pTag, LPSPropValue pVal);


private:
  static int        m_clients;
  static BOOL        m_initialized;
  static nsVoidArray *  m_pStores;
  static LPMAPISESSION  m_lpSession;
  static LPMDB      m_lpMdb;
  static HRESULT      m_lastError;
  static PRUnichar *    m_pUniBuff;
  static int        m_uniBuffLen;

  static BOOL      GetLargeProperty(LPMAPIPROP pProp, ULONG tag, void** result);
};

class CMapiFolder {
public:
  CMapiFolder();
  CMapiFolder(const CMapiFolder *pCopyFrom);
  CMapiFolder(const PRUnichar *pDisplayName, ULONG cbEid, LPENTRYID lpEid, int depth, LONG oType = MAPI_FOLDER);
  ~CMapiFolder();

  void  SetDoImport(BOOL doIt) { m_doImport = doIt;}
  void  SetObjectType(long oType) { m_objectType = oType;}
  void  SetDisplayName(const PRUnichar *pDisplayName) { m_displayName = pDisplayName;}
  void  SetEntryID(ULONG cbEid, LPENTRYID lpEid);
  void  SetDepth(int depth) { m_depth = depth;}
  void  SetFilePath(const PRUnichar *pFilePath) { m_mailFilePath = pFilePath;}

  BOOL  GetDoImport(void) const { return m_doImport;}
  LONG  GetObjectType(void) const { return m_objectType;}
  void  GetDisplayName(nsString& name) const { name = m_displayName;}
  void  GetFilePath(nsString& path) const { path = m_mailFilePath;}
  BOOL  IsStore(void) const { return m_objectType == MAPI_STORE;}
  BOOL  IsFolder(void) const { return m_objectType == MAPI_FOLDER;}
  int    GetDepth(void) const { return m_depth;}

  LPENTRYID  GetEntryID(ULONG *pCb = NULL) const { if (pCb) *pCb = m_cbEid; return (LPENTRYID) m_lpEid;}
  ULONG    GetCBEntryID(void) const { return m_cbEid;}

private:
  LONG    m_objectType;
  ULONG    m_cbEid;
  BYTE *    m_lpEid;
  nsString  m_displayName;
  int      m_depth;
  nsString  m_mailFilePath;
  BOOL    m_doImport;

};

class CMapiFolderList {
public:
  CMapiFolderList();
  ~CMapiFolderList();

  void      AddItem(CMapiFolder *pFolder);
  CMapiFolder *  GetItem(int index) { if ((index >= 0) && (index < m_array.Count())) return (CMapiFolder *)GetAt(index); else return NULL;}
  void      ClearAll(void);

  // Debugging and reporting
  void      DumpList(void);

  void *      GetAt(int index) { return m_array.ElementAt(index);}
  int        GetSize(void) { return m_array.Count();}

protected:
  void  EnsureUniqueName(CMapiFolder *pFolder);
  void  GenerateFilePath(CMapiFolder *pFolder);
  void  ChangeName(nsString& name);

private:
  nsVoidArray    m_array;
};


class CMsgStore {
public:
  CMsgStore(ULONG cbEid = 0, LPENTRYID lpEid = NULL);
  ~CMsgStore();

  void    SetEntryID(ULONG cbEid, LPENTRYID lpEid);
  BOOL    Open(LPMAPISESSION pSession, LPMDB *ppMdb);

  ULONG    GetCBEntryID(void) { return m_cbEid;}
  LPENTRYID  GetLPEntryID(void) { return (LPENTRYID) m_lpEid;}

private:
  ULONG    m_cbEid;
  BYTE *    m_lpEid;
  LPMDB    m_lpMdb;
};


class CMapiFolderContents {
public:
  CMapiFolderContents(LPMDB lpMdb, ULONG cbEID, LPENTRYID lpEid);
  ~CMapiFolderContents();

  BOOL  GetNext(ULONG *pcbEid, LPENTRYID *ppEid, ULONG *poType, BOOL *pDone);

  ULONG  GetCount(void) { return m_count;}

protected:
  BOOL SetUpIter(void);

private:
  HRESULT      m_lastError;
  BOOL      m_failure;
  LPMDB      m_lpMdb;
  LPMAPIFOLDER  m_lpFolder;
  LPMAPITABLE    m_lpTable;
  ULONG      m_fCbEid;
  BYTE *      m_fLpEid;
  ULONG      m_count;
  ULONG      m_iterCount;
  BYTE *      m_lastLpEid;
  ULONG      m_lastCbEid;
};


#endif /* MapiApi_h__ */
