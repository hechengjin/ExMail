#pragma once
#include "SAPrefsSubDlg.h"
#include "afxwin.h"
#include "Inc.h"
#include "afxcmn.h"
#include "MyListCtrl.h"
#include "resource.h"

// CBackDlg 对话框

class CBackDlg : public CSAPrefsSubDlg
{
	DECLARE_DYNAMIC(CBackDlg)

public:
	CBackDlg(CWnd* pParent = NULL);   // 标准构造函数
	virtual ~CBackDlg();

// 对话框数据
	enum { IDD = IDD_DIALOG_BACK };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedButtonSetcopyfile();
	afx_msg void OnBnClickedButtonSource();
	afx_msg void OnBnClickedButtonTarget();
	CButton m_SavePathBtn;
	virtual BOOL OnInitDialog();
	afx_msg void OnBnClickedCheckSavepath();

	//config info ;
	BOOL GetRegProfile();
	// progress ctrl;
	DWORD dwPos;

	afx_msg void OnBnClickedButtonApp();
	CProfileInfo  m_Profile;
	CMyListCtrl m_LogList;
	afx_msg void OnBnClickedButtonStartback();

	void ShowLogMsg(CString strPath, BOOL bFolder, BOOL bSucceed = TRUE);
	CProgressCtrl m_ProgCtrl;
	afx_msg void OnTimer(UINT_PTR nIDEvent);

	void LoadFileSufixxFromFile();
	void LoadFileSufixxFromFile_NoVaild();
	void LoadFileNamesFromFile();
	BOOL EnableWindow(UINT uID, BOOL bEnable = TRUE);

	afx_msg void OnBnClickedButtonSetfilename();
	afx_msg void OnBnClickedRadioValid();
	afx_msg void OnBnClickedRadioNovalid();
};
