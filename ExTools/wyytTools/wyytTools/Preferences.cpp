#include "stdafx.h"
#include <io.h>
#include "wyytTools.h"
#include "Preferences.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CPreferences thePrefs;

LPCTSTR CPreferences::GetConfigFile()
{
	return theApp.m_pszProfileName;
}
CString CPreferences::GetIniDirectory( bool bCreate )
{
	CString strRootDir, strIniDir;
	if (bCreate)
	{		
		TCHAR tchBuffer[MAX_PATH];
		::GetModuleFileName(NULL, tchBuffer, _countof(tchBuffer));
		tchBuffer[_countof(tchBuffer) - 1] = _T('\0');
		LPTSTR pszFileName = _tcsrchr(tchBuffer, L'\\') + 1;
		*pszFileName = L'\0';
		strRootDir = tchBuffer;
		
		strIniDir = strRootDir+ _T("ini");
		::CreateDirectory(strIniDir, NULL);
	
	}
	return strIniDir+L"\\";
}