// WelcomeDlg.cpp : ʵ���ļ�
//

#include "stdafx.h"
#include "wyytTools.h"
#include "WelcomeDlg.h"
#include "afxdialogex.h"


// CWelcomeDlg �Ի���

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


// CWelcomeDlg ��Ϣ�������
