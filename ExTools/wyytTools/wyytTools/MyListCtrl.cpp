// MyListCtrl.cpp : implementation file
//

#include "stdafx.h"
#include "wyytTools.h"
#include "MyListCtrl.h"


// CMyListCtrl

IMPLEMENT_DYNAMIC(CMyListCtrl, CListCtrl)

CMyListCtrl::CMyListCtrl()
{

}

CMyListCtrl::~CMyListCtrl()
{
}


BEGIN_MESSAGE_MAP(CMyListCtrl, CListCtrl)
	ON_NOTIFY_REFLECT(NM_RCLICK, &CMyListCtrl::OnNMRclick)
	ON_COMMAND(ID_M_DEL_ALL_LOGS, &CMyListCtrl::OnMDelAllLogs)
	ON_COMMAND(ID_M_DEL_OK_LOGS, &CMyListCtrl::OnMDelOkLogs)
	ON_COMMAND(ID_M_DEL_ERROR_LOGS, &CMyListCtrl::OnMDelErrorLogs)
	ON_COMMAND(ID_M_SAVE_LOGS, &CMyListCtrl::OnMSaveLogs)
	ON_COMMAND(ID_M_OPEN_AT_FOLDER, &CMyListCtrl::OnMOpenAtFolder)
END_MESSAGE_MAP()



// CMyListCtrl message handlers



void CMyListCtrl::OnNMRclick(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMITEMACTIVATE pNMItem = (LPNMITEMACTIVATE)pNMHDR;
	CMenu mMenu, *pMenu = NULL;
	mMenu.LoadMenu(IDR_LOG_MENU);
	pMenu = mMenu.GetSubMenu(0);
	CPoint pt;
	GetCursorPos(&pt);
	if ( GetItemCount() <= 0 ) {
		pMenu->EnableMenuItem(ID_M_OPEN_AT_FOLDER, MF_GRAYED);
		pMenu->EnableMenuItem(ID_M_DEL_ALL_LOGS, MF_GRAYED);
		pMenu->EnableMenuItem(ID_M_DEL_OK_LOGS, MF_GRAYED);
		pMenu->EnableMenuItem(ID_M_DEL_ERROR_LOGS, MF_GRAYED);
		pMenu->EnableMenuItem(ID_M_SAVE_LOGS, MF_GRAYED);
	}else{
		if ( pNMItem->iItem < 0 || pNMItem->iItem > GetItemCount()) {
			pMenu->EnableMenuItem(ID_M_OPEN_AT_FOLDER, MF_GRAYED);
		}
	}
	pMenu->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, this);
	*pResult = 0;
}

void CMyListCtrl::OnMDelAllLogs()
{
	DeleteAllItems();
}

void CMyListCtrl::OnMDelOkLogs()
{
	CString strText;
	for( int idx = 0; idx < GetItemCount(); idx++ ) {
		strText = GetItemText(idx, 2);
		if ( strText == _T("成功") ) {
			DeleteItem(idx--);
		}
	}
}

void CMyListCtrl::OnMDelErrorLogs()
{
	CString strText;
	for( int idx = 0; idx < GetItemCount(); idx++ ) {
		strText = GetItemText(idx, 2);
		if ( strText == _T("失败") ) {
			DeleteItem(idx--);
		}
	}
}

void CMyListCtrl::OnMSaveLogs()
{
}

void CMyListCtrl::OnMOpenAtFolder()
{
	CString strPath;
	int idx = GetSelectionMark();
	if ( idx >= 0 ) {
		strPath = GetItemText(idx, 0);
		DEL_ENDER(strPath, '\\');
		strPath = strPath.Left(strPath.ReverseFind('\\'));
		if ( !PathFileExists(strPath) ) {
			MessageBox(_T("文件所在目录已经被删除，无法打开！"), _T("提示"), MB_OK | MB_ICONINFORMATION);
			return;
		}
		HINSTANCE hInst = ShellExecute(NULL, _T("open"), strPath, NULL, NULL, SW_SHOW);
	}
}
