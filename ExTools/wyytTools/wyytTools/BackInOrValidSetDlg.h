#pragma once
#include "afxcmn.h"
#include "Preferences.h"


// CBackInOrValidSetDlg 对话框

class CBackInOrValidSetDlg : public CDialog
{
	DECLARE_DYNAMIC(CBackInOrValidSetDlg)

public:
	CBackInOrValidSetDlg(CWnd* pParent = NULL,SetFileType sft=SFT_FileSuffix, eOperType eOType = eOT_NoValid);   // 标准构造函数
	virtual ~CBackInOrValidSetDlg();

// 对话框数据
	enum { IDD = IDD_DIALOG_BACK_VALID };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
	CListCtrl m_listFileSuffix;
	virtual BOOL OnInitDialog();
	afx_msg void OnBnClickedButtonNew();

	void LoadFileSufInfos();
	void LoadFileNamesInfos();
	void SetCurFileSufInfo(size_t index_cur);
	void SetCurFileNameInfo(size_t index_cur);
	void DelCurFileSufInfo(size_t index_cur);
	void DelCurFileNameInfo(size_t index_cur);
	void AlterCurFileSufInfo(size_t index_cur,FileSuffix_Struct& stFileSux);
	void AlterCurFileNameInfo(size_t index_cur,FileName_Struct& stFileName);
	void GetCurFileSufInfo(FileSuffix_Struct& stFileSux);
	void GetCurFileNameInfo(FileName_Struct& stFileName);
	void SetCurFileSufInfo(FileSuffix_Struct& stFileSux);
	void SetCurFileNameInfo(FileName_Struct& stFileName);
	afx_msg void OnNMClickListFilesuffix(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnBnClickedButtonApp();
	afx_msg void OnBnClickedButtonDel();
	afx_msg void OnClose();

private:
	SetFileType m_sft;
	eOperType m_eOType;
};
