
#include "stdafx.h"
#include "Inc.h"
//#include "wyytToolsDlg.h"
#include "BackDlg.h"
#include "ShellFileOp/ShellFileOp.h"
#include "Util.h"
#include "resource.h"
#include "wyytTools.h"

CBackDlg *m_gpMainWnd = NULL;
DWORD dwTotal = 0, dwFiles = 0, dwFolders = 0, dwSucceed = 0, dwFailed = 0;

BOOL IsCopyFile(CString strPath, vector<FileSuffix_Struct>* pFileSuffixMap,vector<FileName_Struct>* pFileNamesMap,int nCurOperType)
{
	
	if ( nCurOperType == eOT_Valid )
	{
		BOOL bCopy = FALSE;
		CString strExt = _T("*.") + CShellFileOp::GetFileExt(strPath);
		for (vector<FileSuffix_Struct>::iterator it = pFileSuffixMap->begin(); it != pFileSuffixMap->end(); it++ )
		{
			if ( it->bEnable == 1 && it->FileSuffix == strExt )
			{
				bCopy = TRUE;
				break;
			}
		}
		if ( !bCopy )
		{
			CString strName = CShellFileOp::GetFileName(strPath);
			for (vector<FileName_Struct>::iterator it = pFileNamesMap->begin(); it != pFileNamesMap->end(); it++ )
			{
				if ( it->bEnable == 1 && it->FileName == strName )
				{
					bCopy = TRUE;
					break;
				}
			}

		}
		return bCopy;
	}
	else
	{
		BOOL bCopy = TRUE;
		CString strExt = _T("*.") + CShellFileOp::GetFileExt(strPath);
		for (vector<FileSuffix_Struct>::iterator it = pFileSuffixMap->begin(); it != pFileSuffixMap->end(); it++ )
		{
			if ( it->bEnable == 1 && it->FileSuffix == strExt )
			{
				bCopy = FALSE;
				break;
			}
		}
		//if ( !bCopy )
		//{
		//	CString strName = CShellFileOp::GetFileName(strPath);
		//	for (vector<FileName_Struct>::iterator it = pFileNamesMap->begin(); it != pFileNamesMap->end(); it++ )
		//	{
		//		if ( it->bEnable == 1 && it->FileName == strName )
		//		{
		//			bCopy = TRUE;
		//			break;
		//		}
		//	}
		//}
		return bCopy;
	}
	
	//map<CString, CString>::iterator it = pFileSuffixMap->find(strExt);
	//if ( it != pFileSuffixMap->end() )
	//{
	//	return TRUE;
	//}
	//else
	//{
	//	return FALSE;
	//}
}

void BackStart(vector<FileSuffix_Struct>* pFileSuffixMap,vector<FileName_Struct>* pFileNamesMap, CString strSrcPath, CString strTarPath/*, BOOL bDelToRecycle*/, int nCurOperType)
{
	BOOL bRet = FALSE;
	CFileFind finder;
	ADD_ENDER(strSrcPath, '\\');
	ADD_ENDER(strTarPath, '\\');
	BOOL bfinder = finder.FindFile(strSrcPath + _T("*.*"), 0);
	while(bfinder)
	{
		bfinder = finder.FindNextFile();
		if (finder.IsDots())
		{
			continue;
		}
		else
		{
			if (finder.IsDirectory())
			{
				CString csCurSrcFullPath = finder.GetFilePath();
				CString csCurTarFullPath = strTarPath + finder.GetFileName();
				m_gpMainWnd->dwPos++;
				dwTotal++;
				dwFolders++;
				//strTarPath += finder.GetFileName();
				//strSrcPath += finder.GetFileName();
				if (!Util::IsFileExist(csCurTarFullPath))
				{
					bRet = Util::MkDir(csCurTarFullPath);
				}
				m_gpMainWnd->ShowLogMsg(csCurTarFullPath, TRUE, 0 == bRet);
				BackStart(pFileSuffixMap, pFileNamesMap,csCurSrcFullPath, csCurTarFullPath,nCurOperType);
			}
			else
			{
				m_gpMainWnd->dwPos++;				
				CString strCurSrcFilePath = finder.GetFilePath();
				CString strCurTarFilePath = strTarPath + finder.GetFileName();
				if ( IsCopyFile(finder.GetFilePath(), pFileSuffixMap, pFileNamesMap,nCurOperType) ) 
				{
					dwTotal++;
					dwFiles++;
					//strTarPath += finder.GetFileName();
					//bRet = CShellFileOp::RemoveFile(finder.GetFilePath(), bDelToRecycle);
					bRet = Util::CopyFile(strCurSrcFilePath,strCurTarFilePath);
					if ( bRet ) 
					{
						dwSucceed++;
					}
					else
					{
						dwFailed++;
					}
					m_gpMainWnd->ShowLogMsg(strCurTarFilePath, FALSE, bRet);
				}
			}
		}
	}
	finder.Close();
	// if the folder is empty, delete it ;
	//m_gpMainWnd->dwPos++;
	//if ( IsDeleteFolder(strPath, pTreeList) && CShellFileOp::FolderIsEmpty(strPath) ) 
	//{
	//	dwTotal++;
	//	dwFolders++;
	//	bRet = CShellFileOp::RemoveFolder(strPath);
	//	if ( bRet == TRUE ) 
	//	{
	//		dwSucceed++;
	//	}
	//	else
	//	{
	//		dwFailed++;
	//	}
	//	m_gpMainWnd->ShowLogMsg(strPath, TRUE, bRet);
	//}
}

UINT __cdecl ScanAndBackFunc(LPVOID pParam)
{
	CBackDlg *m_pBackWnd = (CBackDlg *)pParam;
	ASSERT(m_pBackWnd != NULL);
	dwTotal = 0, dwFiles = 0, dwFolders = 0, dwSucceed = 0, dwFailed = 0;
	m_gpMainWnd = m_pBackWnd;
	m_pBackWnd->EnableWindow(IDC_BUTTON_SOURCE, FALSE);
	m_pBackWnd->EnableWindow(IDC_BUTTON_TARGET, FALSE);
	m_pBackWnd->EnableWindow(IDC_BUTTON_SETCOPYFILE, FALSE);
	CString strSrcPath=m_pBackWnd->m_Profile.m_strSourcePath;
	CString strTarPath=m_pBackWnd->m_Profile.m_strTargetPath;
	LONG nCount = 0;
	nCount += CShellFileOp::GetSubFilesFolderNums(strSrcPath);
	//for the progress ctrl;
	m_pBackWnd->m_ProgCtrl.SetRange32(0, nCount);
	m_pBackWnd->m_ProgCtrl.SetStep(1);
	m_pBackWnd->SetTimer(1, 50, NULL);
	// end;
	//for( int idx = 0; idx < m_pBackWnd->m_PathList.GetCount(); idx++ ) {
	//	m_pBackWnd->m_PathList.GetText(idx, strPath);
	//	CleanStart(m_pBackWnd->GetTreeList(), strPath, RegReadWriteDword(_T("DeleteToRecycle"), TRUE, 0));
	//}
	//全备份　增量备份(根据最新时间比较)/ 是否覆盖(新旧，存在)
	if ( m_pBackWnd->m_Profile.m_nOperType ==  eOT_Valid )
	{
		BackStart(&theApp.m_vecFileSuffix,&theApp.m_vecFileNames,strSrcPath,strTarPath/*, RegReadWriteDword(_T("DeleteToRecycle"), TRUE, 0)*/,eOT_Valid);
	}
	else
	{
		BackStart(&theApp.m_vecFileSuffix_NoValid,&theApp.m_vecFileNames,strSrcPath,strTarPath/*, RegReadWriteDword(_T("DeleteToRecycle"), TRUE, 0)*/,eOT_NoValid);
	}
	
	m_pBackWnd->KillTimer(1);
	m_pBackWnd->m_ProgCtrl.SetPos(0);
	m_pBackWnd->EnableWindow(IDC_BUTTON_SOURCE, TRUE);
	m_pBackWnd->EnableWindow(IDC_BUTTON_TARGET, TRUE);
	m_pBackWnd->EnableWindow(IDC_BUTTON_SETCOPYFILE, TRUE);
	CString strT;
	strT.Format(_T("共复制文件和目录：%ld 个， 文件夹：%ld 个、文件：%ld 个；成功：%ld 个、失败：%ld 个；"), dwTotal, dwFolders, dwFiles, dwSucceed, dwFailed);
	m_pBackWnd->ShowLogMsg(_T(""), -1);
	m_pBackWnd->ShowLogMsg(strT, -1);
	//if ( RegReadWriteDword(_T("EndClose"), TRUE) == TRUE )
	//{
	//	m_pBackWnd->OnCancel();
	//}

	return TRUE;
}

//////////////////////////////// Reg operation /////////////////////////////////////
INT RegReadWriteDword(LPTSTR lpszName, BOOL bRead, INT uValue)
{
	INT iResult = -1;
	HKEY hKey = NULL;
	LONG LRet =  RegCreateKey(HKEY_LOCAL_MACHINE, _T("Software\\Wyyt"), &hKey);
	if ( LRet == ERROR_SUCCESS ){
		if ( bRead ) {
			DWORD dwLen = sizeof(iResult);
			LRet = RegQueryValueEx(hKey, lpszName, NULL, NULL, (LPBYTE)&iResult, &dwLen);
			if ( LRet != ERROR_SUCCESS ){
				iResult = -1;
			}
		}else{
			LRet = RegSetValueEx(hKey, lpszName, 0, REG_DWORD, (LPBYTE)&uValue, sizeof(uValue));
			if ( LRet == ERROR_SUCCESS ){
				iResult = uValue;
			}
		}
	}
	RegCloseKey(hKey);
	return iResult;
}

BOOL RegReadWriteStrings(LPTSTR lpszKeyName, CStringArray &arrString, BOOL bRead)
{
	HKEY hKey = NULL;
	CString strKeyName(lpszKeyName);
	if ( bRead ) {
		strKeyName = _T("Software\\Wyyt\\") + strKeyName;
	}else{
		strKeyName = _T("Software\\Wyyt\\");
	}
	LONG LRet =  RegCreateKey(HKEY_LOCAL_MACHINE, strKeyName, &hKey);
	if ( LRet == ERROR_SUCCESS ){
		if ( bRead ) {
			DWORD dwValues = 0, dwMaxValueNameLen = 0, dwMaxValueLen = 0;
			LRet = RegQueryInfoKey(hKey, NULL, NULL, NULL, NULL, NULL, NULL, &dwValues, NULL, NULL, NULL, NULL);
			dwMaxValueNameLen = 5;
			dwMaxValueLen = MAX_PATH*sizeof(TCHAR);
			TCHAR szValueName[5] = {0};
			TCHAR szValueData[MAX_PATH] = {0};
			for ( DWORD idx = 0; idx < dwValues; idx++ ){
				DWORD dwValueNameLen = dwMaxValueNameLen;
				DWORD dwValueLen = dwMaxValueLen;
				LRet = RegEnumValue(hKey, idx, szValueName, &dwValueNameLen, NULL, NULL, (LPBYTE)szValueData, &dwValueLen);
				if ( LRet == ERROR_SUCCESS ){
					CString strValue(szValueData);
					arrString.Add(strValue);
				}
			}
		}else{
			LRet = SHDeleteKey(hKey, lpszKeyName);
			if ( LRet != ERROR_SUCCESS ) {
				RegCloseKey(hKey);
				return FALSE;
			}
			RegCloseKey(hKey);
			strKeyName.Format(_T("%s"), lpszKeyName);
			strKeyName = _T("Software\\Wyyt\\") + strKeyName;
			LRet =  RegCreateKey(HKEY_LOCAL_MACHINE, strKeyName, &hKey);
			if ( LRet != ERROR_SUCCESS ) {
				return FALSE;
			}
			for( int idx = 0; idx < arrString.GetCount(); idx++ ) {
				CString strName;
				strName.Format(_T("%d"), idx+1);
				TCHAR szValue[MAX_PATH] = {0};
				_tcscpy_s(szValue, MAX_PATH, arrString.GetAt(idx));
				LRet = RegSetValueEx(hKey, strName, 0, REG_SZ, (LPBYTE)szValue, (DWORD)_tcslen(szValue)*sizeof(TCHAR));
				if ( LRet != ERROR_SUCCESS ) {
					RegCloseKey(hKey);
					return FALSE;
				}
			}
		}
		
		RegCloseKey(hKey);
		return TRUE;
	}
	RegCloseKey(hKey);
	return FALSE;
}

BOOL RegReadWriteStrings(LPTSTR lpszName, CString &strValue, BOOL bRead)
{
	HKEY hKey = NULL;
	DWORD dwLen = 0;
	TCHAR szValue[MAX_PATH] = {0};
	LONG LRet =  RegCreateKey(HKEY_LOCAL_MACHINE, _T("Software\\Wyyt"), &hKey);
	if ( LRet == ERROR_SUCCESS ) {
		if ( bRead ) {
			dwLen = (DWORD)sizeof(szValue);
			LRet = RegQueryValueEx(hKey, lpszName, NULL, NULL, (LPBYTE)szValue, &dwLen);
			if ( LRet == ERROR_SUCCESS ){
				strValue.Format(_T("%s"), szValue);
				RegCloseKey(hKey);
				return TRUE;
			}
		}else{
			_tcscpy_s(szValue, MAX_PATH, strValue);
			dwLen = (DWORD)_tcslen(szValue)*sizeof(TCHAR);
			LRet = RegSetValueEx(hKey, lpszName, 0, REG_SZ, (LPBYTE)szValue, dwLen);
			if ( LRet == ERROR_SUCCESS ){
				RegCloseKey(hKey);
				return TRUE;
			}
		}
	}
	RegCloseKey(hKey);
	return FALSE;
}