// WelcomeDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "wyytTools.h"
#include "WelcomeDlg.h"
#include "afxdialogex.h"


// CWelcomeDlg 对话框

IMPLEMENT_DYNAMIC(CWelcomeDlg, CSAPrefsSubDlg)

CWelcomeDlg::CWelcomeDlg(CWnd* pParent /*=NULL*/)
	: CSAPrefsSubDlg(CWelcomeDlg::IDD, pParent)
{

}

CWelcomeDlg::~CWelcomeDlg()
{
}

void CWelcomeDlg::DoDataExchange(CDataExchange* pDX)
{
	CSAPrefsSubDlg::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CWelcomeDlg, CSAPrefsSubDlg)
END_MESSAGE_MAP()


// CWelcomeDlg 消息处理程序
