#include "StdAfx.h"
#include "ShellFileOp.h"

CShellFileOp::CShellFileOp(void)
{
}

CShellFileOp::~CShellFileOp(void)
{
}

INT CShellFileOp::TargetIsFolder(LPCTSTR lpszFName)
{
	INT iResult = -1;
	DWORD dwAttr = GetFileAttributes(lpszFName);
	if ( dwAttr != INVALID_FILE_ATTRIBUTES ) {
		if ( dwAttr & FILE_ATTRIBUTE_DIRECTORY ) {
			iResult = TRUE;
		}else{
			iResult = FALSE;
		}
	}
	return iResult;
}

BOOL CShellFileOp::FolderFileExist(LPCTSTR lpszFName)
{
	BOOL bResult = FALSE;
	DWORD dwAttr = GetFileAttributes(lpszFName);
	if ( dwAttr != INVALID_FILE_ATTRIBUTES ) {
		bResult = TRUE;
	}
	return bResult;
}

LONG CShellFileOp::GetSubFilesNums(LPCTSTR lpszFolderName)
{
	LONG nCount = 0;
	if ( FolderFileExist(lpszFolderName) && TargetIsFolder(lpszFolderName) ) {
		CString strPath(lpszFolderName);
		ADD_ENDER(strPath, '\\');
		CFileFind mFinder;
		BOOL bFind = mFinder.FindFile(strPath + _T("*.*"), 0);
		while ( bFind ){
			bFind = mFinder.FindNextFile();
			if ( mFinder.IsDots() ){
				continue;
			}else if (mFinder.IsDirectory()){
				nCount += GetSubFilesNums(mFinder.GetFilePath());
			}else{
				nCount++;
			}
		}
		mFinder.Close();
	}
	return nCount;
}

LONG CShellFileOp::GetSubFolderNums(LPCTSTR lpszFolderName)
{
	LONG nCount = 0;
	if ( FolderFileExist(lpszFolderName) && TargetIsFolder(lpszFolderName) ) {
		CString strPath(lpszFolderName);
		ADD_ENDER(strPath, '\\');
		CFileFind mFinder;
		BOOL bFind = mFinder.FindFile(strPath + _T("*.*"), 0);
		while ( bFind ){
			bFind = mFinder.FindNextFile();
			if ( mFinder.IsDirectory() ) {
				nCount += GetSubFolderNums(mFinder.GetFilePath());
				nCount++;
			}
		}
		mFinder.Close();
	}
	return nCount;
}

LONG CShellFileOp::GetSubFilesFolderNums(LPCTSTR lpszFolderName)
{
	LONG nCount = 0;
	if ( FolderFileExist(lpszFolderName) && TargetIsFolder(lpszFolderName) ) {
		CString strPath(lpszFolderName);
		ADD_ENDER(strPath, '\\');
		CFileFind mFinder;
		BOOL bFind = mFinder.FindFile(strPath + _T("*.*"), 0);
		while ( bFind ){
			bFind = mFinder.FindNextFile();
			if ( mFinder.IsDots() ) {
				continue;
			}else if ( mFinder.IsDirectory() ) {
				nCount++;
				nCount += GetSubFilesFolderNums(mFinder.GetFilePath());
			}else{
				nCount++;
			}
		}
		mFinder.Close();
	}
	return nCount;
}

BOOL CShellFileOp::RemoveFile(LPCTSTR lpszFileName, BOOL bToRecycle)
{
	if ( !FolderFileExist(lpszFileName) || TargetIsFolder(lpszFileName) ) {
		return FALSE;
	}
	if ( !bToRecycle ) {
		return DeleteFile(lpszFileName);
	}else{
		TCHAR szBuf[MAX_PATH+2] = {0};
		SHFILEOPSTRUCT shFileOp = {0};
		shFileOp.hwnd = AfxGetMainWnd()->GetSafeHwnd();
		shFileOp.wFunc = FO_DELETE;
		shFileOp.fFlags = FOF_ALLOWUNDO | FOF_SILENT | FOF_NOERRORUI | FOF_NOCONFIRMATION;
		_tcscpy_s(szBuf, MAX_PATH, lpszFileName);
		size_t tLen = _tcslen(lpszFileName);
		szBuf[tLen] = 0; szBuf[tLen+1] = 0;
		shFileOp.pFrom = szBuf;
		return !SHFileOperation(&shFileOp);
	}
}

BOOL CShellFileOp::RemoveFolder(LPCTSTR lpszFolderName, BOOL bToRecycle)
{
	if ( !FolderFileExist(lpszFolderName) || !TargetIsFolder(lpszFolderName) ) {
		return FALSE;
	}
	BOOL bRet = FALSE;
	CString strPath(lpszFolderName);
	ADD_ENDER(strPath, '\\');
	CFileFind mFinder;
	BOOL bFind = mFinder.FindFile(strPath + _T("*.*"), 0);
	while ( bFind ){
		bFind = mFinder.FindNextFile();
		if ( mFinder.IsDots() ) {
			continue;
		}else if ( mFinder.IsDirectory() ) {
			RemoveFolder(mFinder.GetFilePath(), bToRecycle);
		}else{
			RemoveFile(mFinder.GetFilePath(), bToRecycle);
		}
	}
	mFinder.Close();
	if ( bToRecycle ) {
		TCHAR szBuf[MAX_PATH+2] = {0};
		SHFILEOPSTRUCT shFileOp = {0};
		shFileOp.hwnd = AfxGetMainWnd()->GetSafeHwnd();
		shFileOp.wFunc = FO_DELETE;
		shFileOp.fFlags = FOF_ALLOWUNDO | FOF_SILENT | FOF_NOERRORUI | FOF_NOCONFIRMATION;
		CString strT(lpszFolderName);
		DEL_ENDER(strT, '\\');
		_tcscpy_s(szBuf, MAX_PATH, strT);
		size_t tLen = strT.GetLength();
		szBuf[tLen] = 0; szBuf[tLen+1] = 0;
		shFileOp.pFrom = szBuf;
		bRet = !SHFileOperation(&shFileOp);
	}else{
		bRet = RemoveDirectory(lpszFolderName);
	}
	return bRet;
}

CString CShellFileOp::GetFileExt(LPCTSTR lpszFileName)
{
	CString strPath(lpszFileName);
	int idx = strPath.ReverseFind('.');
	return strPath.Right(strPath.GetLength()-idx-1);
}

CString CShellFileOp::GetFileParent(LPCTSTR lpszFileName)
{
	int iLast = 0, iCurr = 0;
	CString strParent, strPath(lpszFileName);
	DEL_ENDER(strPath, '\\');
	for ( int idx = 0; idx < strPath.GetLength(); idx++ ) {
		if ( strPath[idx] == '\\' ) {
			iLast = iCurr;
			iCurr = idx;
		}
	}
	strParent = strPath.Mid(iLast+1, iCurr-iLast-1);
	return strParent;
}
CString CShellFileOp::GetFileName(LPCTSTR lpszFileName)
{
	int iCurr = 0;
	CString strFileName, strPath(lpszFileName);
	DEL_ENDER(strPath, '\\');
	for ( int idx = 0; idx < strPath.GetLength(); idx++ ) 
	{
		if ( strPath[idx] == '\\' ) 
		{
			iCurr = idx;
		}
	}
	strFileName = strPath.Mid(iCurr+1,strPath.GetLength());
	return strFileName;
}

INT CShellFileOp::FolderIsEmpty(LPCTSTR lpszFolderName)
{
	if ( !FolderFileExist(lpszFolderName) || !TargetIsFolder(lpszFolderName) ) {
		return -1;
	}
	CString strPath(lpszFolderName);
	ADD_ENDER(strPath, '\\');
	CFileFind mFinder;
	BOOL bFind = mFinder.FindFile(strPath+_T("*.*"), 0);
	while ( bFind ){
		bFind = mFinder.FindNextFile();
		if ( mFinder.IsDots() ) {
			continue;
		}else{
			return FALSE;
		}
	}
	return TRUE;
}