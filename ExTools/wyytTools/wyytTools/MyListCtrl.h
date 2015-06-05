#pragma once

#include "Inc.h"
#include <shlwapi.h>

// CMyListCtrl

class CMyListCtrl : public CListCtrl
{
	DECLARE_DYNAMIC(CMyListCtrl)

public:
	CMyListCtrl();
	virtual ~CMyListCtrl();

protected:
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnNMRclick(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnMDelAllLogs();
	afx_msg void OnMDelOkLogs();
	afx_msg void OnMDelErrorLogs();
	afx_msg void OnMSaveLogs();
	afx_msg void OnMOpenAtFolder();
};


