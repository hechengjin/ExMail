
// wyytToolsDlg.h : ͷ�ļ�
//

#pragma once

#include "SAPrefsStatic.h"
#include "SAPrefsSubDlg.h"
#include "SAPrefsDialog.h"
#include "WelcomeDlg.h"
#include "BackDlg.h"
using namespace std;
// CwyytToolsDlg �Ի���
class CwyytToolsDlg : public CSAPrefsDialog
{
// ����
public:
	CwyytToolsDlg(CWnd* pParent = NULL);	// ��׼���캯��
	~CwyytToolsDlg();
// �Ի�������
	enum { IDD = IDD_WYYTTOOLS_DIALOG };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV ֧��


// ʵ��
protected:
	HICON m_hIcon;

	// ���ɵ���Ϣӳ�亯��
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()

protected:
	CWelcomeDlg *m_pWelcomeDlgPage;
	CBackDlg *m_pBackDlgPage;
};
