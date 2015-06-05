#pragma once
#include "SAPrefsSubDlg.h"


// CWelcomeDlg 对话框

class CWelcomeDlg : public CSAPrefsSubDlg
{
	DECLARE_DYNAMIC(CWelcomeDlg)

public:
	CWelcomeDlg(CWnd* pParent = NULL);   // 标准构造函数
	virtual ~CWelcomeDlg();

// 对话框数据
	enum { IDD = IDD_DIALOG_WELCOME };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
};
