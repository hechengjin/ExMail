#pragma once
#include "ini2.h"

enum SetFileType
	{ 
		SFT_FileSuffix = 0 ,
		SFT_FileFullName
	};
class CPreferences
{
public:
	static	LPCTSTR GetConfigFile();
	static CString	GetIniDirectory(bool bCreate = true);
};

extern CPreferences thePrefs;