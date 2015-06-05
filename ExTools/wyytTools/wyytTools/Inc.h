
#pragma once
#include <map>

using namespace std;

struct FileSuffix_Struct{
	CString			FileNote;
	CString			FileSuffix;
	short bEnable;
	FileSuffix_Struct()
	{
		bEnable = 0;
	}
	~FileSuffix_Struct() {  }
};
struct FileName_Struct{
	CString			FileNote;
	CString			FileName;
	short bEnable;
	FileName_Struct()
	{
		bEnable = 0;
	}
	~FileName_Struct() {  }
};
#define DEL_ENDER(self, ch) if(self[self.GetLength()-1] == ch) self = self.Left(self.GetLength()-1)

enum eOperType
{
	eOT_Valid, //只过滤合法文件
	eOT_NoValid//只排除不合法文件
};
class CProfileInfo {
public:
	short m_bSavePath;
	CString m_strSourcePath;
	CString m_strTargetPath;
	short m_nOperType;
	CProfileInfo() {
		m_bSavePath = FALSE;
		m_nOperType = eOT_Valid;
	}

};
class CBackDlg;
extern CBackDlg *m_gpMainWnd;
UINT __cdecl ScanAndBackFunc(LPVOID pParam);
void BackStart(vector<FileSuffix_Struct>* pFileSuffixMap,vector<FileName_Struct>* pFileNamesMap, CString strSrcPath, CString strTarPath, int nCurOperType = eOT_Valid );
INT RegReadWriteDword(LPTSTR lpszName, BOOL bRead, INT uValue = 0);
BOOL RegReadWriteStrings(LPTSTR lpszKeyName, CStringArray &arrString, BOOL bRead = TRUE);
BOOL RegReadWriteStrings(LPTSTR lpszName, CString &strValue, BOOL bRead);