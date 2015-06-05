#ifndef UTILITY_UTIL_H
#define UTILITY_UTIL_H
#include <windows.h>
#include <cstdio>
#include <string>
#include <vector>
#include <ctime>
#include <io.h>
using namespace std;

class Util
{
public:
	
    static bool CopyFile(const TCHAR *srcPath, const TCHAR *destPath);

    static bool MoveFile(const TCHAR* src, const TCHAR* dest);

    static bool DeleteFile(const TCHAR* file);
	
	static bool CreateDirectory(const char* prootpath,const char* psubdirpath,char* strnewdirctory);
	static bool DeleteDirectory(const char* path);
	static bool IsFileExist(const TCHAR* pFilePath);
	static int MkDir(const TCHAR *dirname );
	static int MkDir_R(const TCHAR *dirname);
	static int RmDir(const TCHAR *dirname, bool bRemoveRootDir = true);
	
};
#endif