#pragma once
#include "SAPrefsSubDlg.h"


// CWelcomeDlg �Ի���

class CWelcomeDlg : public CSAPrefsSubDlg
{
	DECLARE_DYNAMIC(CWelcomeDlg)

public:
	CWelcomeDlg(CWnd* pParent = NULL);   // ��׼���캯��
	virtual ~CWelcomeDlg();

// �Ի�������
	enum { IDD = IDD_DIALOG_WELCOME };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV ֧��

	DECLARE_MESSAGE_MAP()
};
