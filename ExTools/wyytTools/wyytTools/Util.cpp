#include "Util.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <direct.h>
using namespace std;

bool Util::CopyFile(const TCHAR * srcPath, const TCHAR * destPath)
{
   return ::CopyFile(srcPath,destPath,FALSE);
}

 bool Util::MoveFile(const TCHAR* src, const TCHAR* dest)
 {
    return ::MoveFile(src,dest);
 }

bool Util::DeleteFile(const TCHAR* file)
{
    if(::_waccess(file,0) != -1)
	{
		if (::DeleteFile(file) != TRUE) 
		{
		  return false;
		}
		return true;
    }
  //remove(file);
  return true;   
}


bool Util::IsFileExist(const TCHAR* pFilePath)
{
	return 0 == ::_waccess(pFilePath,0);
}

int Util::MkDir(const TCHAR *dirname )
{
	return ::_wmkdir(dirname);
}

int Util::MkDir_R(const TCHAR *dirname )
{
	int nBufSize = wcslen(dirname) + 1;

	TCHAR* pDir = new TCHAR[nBufSize];
	memset(pDir,0,nBufSize);
	const TCHAR chSep = '/';
#ifndef _LINUX
	const TCHAR chSep2 = '/';
#else
	const TCHAR chSep2 = '\\';
#endif
	int nRet = 0;

	TCHAR* pTmp = pDir;
	for (int i = 0; i < nBufSize; ++i)
	{
		pTmp[i] = dirname[i];
		if (chSep == pTmp[i] ||chSep2 == pTmp[i]|| '\0' == pTmp[i] )
		{
			if (!IsFileExist(pDir))
			{
				nRet = MkDir(pDir);

				if (0 != nRet)
					break;
			} 
		}
	}

	delete[] pDir;

	return nRet;
}

int Util::RmDir(const TCHAR* sDirName, bool bRemoveRootDir)
{
	if (sDirName == NULL)
		return -1;

			//windows
	WIN32_FIND_DATA findData;
	TCHAR sFilePath[MAX_PATH];

	swprintf(sFilePath, L"%s\\*.*", sDirName);

	HANDLE hFile = ::FindFirstFile(sFilePath, &findData);
	if (hFile==NULL || hFile==INVALID_HANDLE_VALUE) 
		return -1;

	do 
	{
		if (wcscmp(findData.cFileName, L".") && wcscmp(findData.cFileName, L".."))
		{
			swprintf(sFilePath, L"%s\\%s", sDirName, findData.cFileName);
			if (findData.dwFileAttributes == FILE_ATTRIBUTE_DIRECTORY)
				RmDir(sFilePath);
			else
				_wunlink(sFilePath);
		}

	} while (::FindNextFile(hFile, &findData));

	::FindClose(hFile);
	int nRet = 0;
	if (bRemoveRootDir)
		nRet = _wrmdir(sDirName);

	return nRet;
}
