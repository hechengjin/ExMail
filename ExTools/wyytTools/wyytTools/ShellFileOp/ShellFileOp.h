#pragma once
#include "afx.h"

#define ADD_HEADER(self, ch) if(self[0] != ch) self = ch + self    
#define DEL_HEADER(self, ch) if(self[0] == ch) self.Delete(0)
#define ADD_ENDER(self, ch) if(self[self.GetLength()-1] != ch) self += ch
#define DEL_ENDER(self, ch) if(self[self.GetLength()-1] == ch) self = self.Left(self.GetLength()-1)

class CShellFileOp :
	public CStdioFile
{
public:
	CShellFileOp(void);
	~CShellFileOp(void);
public:
	static INT TargetIsFolder(LPCTSTR lpszFName);
	static BOOL FolderFileExist(LPCTSTR lpszFName);
	static BOOL RemoveFile(LPCTSTR lpszFileName, BOOL bToRecycle = TRUE);
	static BOOL RemoveFolder(LPCTSTR lpszFolderName, BOOL bToRecycle = FALSE);
	static LONG GetSubFilesNums(LPCTSTR lpszFolderName);
	static LONG GetSubFolderNums(LPCTSTR lpszFolderName);
	static LONG GetSubFilesFolderNums(LPCTSTR lpszFolderName);
	static CString GetFileExt(LPCTSTR lpszFileName);
	static CString GetFileParent(LPCTSTR lpszFileName);
	static CString GetFileName(LPCTSTR lpszFileName);
	static INT FolderIsEmpty(LPCTSTR lpszFolderName);
};
