
// wyytTools.h : PROJECT_NAME Ӧ�ó������ͷ�ļ�
//

#pragma once

#ifndef __AFXWIN_H__
	#error "�ڰ������ļ�֮ǰ������stdafx.h�������� PCH �ļ�"
#endif

#include "resource.h"		// ������
#include <vector>
using namespace std;

#include "Inc.h"

// CwyytToolsApp:
// �йش����ʵ�֣������ wyytTools.cpp
//

class CwyytToolsApp : public CWinApp
{
public:
	CwyytToolsApp();

	vector<FileSuffix_Struct> m_vecFileSuffix;
	vector<FileName_Struct> m_vecFileNames;
	
	vector<FileSuffix_Struct> m_vecFileSuffix_NoValid;

// ��д
public:
	virtual BOOL InitInstance();

// ʵ��

	DECLARE_MESSAGE_MAP()
};

extern CwyytToolsApp theApp;
extern CString g_strAppPath;