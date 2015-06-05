// BackInOrValidSetDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "wyytTools.h"
#include "BackInOrValidSetDlg.h"
#include "afxdialogex.h"



// CBackInOrValidSetDlg 对话框

IMPLEMENT_DYNAMIC(CBackInOrValidSetDlg, CDialog)

CBackInOrValidSetDlg::CBackInOrValidSetDlg(CWnd* pParent /*=NULL*/,SetFileType sft, eOperType eOType)
	: CDialog(CBackInOrValidSetDlg::IDD, pParent),m_sft(sft),m_eOType(eOType)
{
	
}

CBackInOrValidSetDlg::~CBackInOrValidSetDlg()
{
}

void CBackInOrValidSetDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LIST_FILESUFFIX, m_listFileSuffix);
}


BEGIN_MESSAGE_MAP(CBackInOrValidSetDlg, CDialog)
	ON_BN_CLICKED(IDC_BUTTON_NEW, &CBackInOrValidSetDlg::OnBnClickedButtonNew)
	ON_NOTIFY(NM_CLICK, IDC_LIST_FILESUFFIX, &CBackInOrValidSetDlg::OnNMClickListFilesuffix)
	ON_BN_CLICKED(IDC_BUTTON_APP, &CBackInOrValidSetDlg::OnBnClickedButtonApp)
	ON_BN_CLICKED(IDC_BUTTON_DEL, &CBackInOrValidSetDlg::OnBnClickedButtonDel)
	ON_WM_CLOSE()
END_MESSAGE_MAP()


// CBackInOrValidSetDlg 消息处理程序


BOOL CBackInOrValidSetDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	CComboBox *comboBox=(CComboBox*)GetDlgItem(IDC_COMBO_ENABLE);
	comboBox->InsertString(0,_T("否"));
	comboBox->InsertString(1, _T("是")); 
	comboBox->SetCurSel(1); //设置选中的项
	// TODO:  在此添加额外的初始化
	m_listFileSuffix.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP);
	ASSERT( (m_listFileSuffix.GetStyle() & LVS_SINGLESEL) == 0);
	m_listFileSuffix.InsertColumn(0, _T("文件说明")/*GetResString(IDS_TITLE)*/, LVCFMT_LEFT, 260);// X: [RUL] - [Remove Useless Localize]
	if ( m_sft == SFT_FileFullName )
	{
		m_listFileSuffix.InsertColumn(1, _T("完整文件名")/*GetResString(IDS_S_DAYS)*/, LVCFMT_LEFT, 80);
		m_listFileSuffix.InsertColumn(2, _T("生效标记")/*GetResString(IDS_S_DAYS)*/, LVCFMT_LEFT, 70);
		LoadFileNamesInfos();
		SetWindowText(L"设置文件全名");
		SetDlgItemText(IDC_STATIC_NAME_FILE,L"文件名：");
	}
	else
	{
		m_listFileSuffix.InsertColumn(1, _T("文件后缀")/*GetResString(IDS_S_DAYS)*/, LVCFMT_LEFT, 80);
		m_listFileSuffix.InsertColumn(2, _T("生效标记")/*GetResString(IDS_S_DAYS)*/, LVCFMT_LEFT, 70);
		LoadFileSufInfos();
		SetWindowText(L"设置文件后缀");
		SetDlgItemText(IDC_STATIC_NAME_FILE,L"文件后缀：");
	}
	


	return TRUE;  // return TRUE unless you set the focus to a control
	// 异常: OCX 属性页应返回 FALSE
}
void CBackInOrValidSetDlg::LoadFileSufInfos()
{
	m_listFileSuffix.DeleteAllItems();
	size_t index=0;
	if ( m_eOType == eOT_Valid )
	{
		for ( vector<FileSuffix_Struct>::iterator it = theApp.m_vecFileSuffix.begin(); it != theApp.m_vecFileSuffix.end(); it++,index++ )
		{
			m_listFileSuffix.InsertItem(index , it->FileNote );
			m_listFileSuffix.SetItemText(index, 1, it->FileSuffix);
			m_listFileSuffix.SetItemText(index, 2, it->bEnable == 1 ? L"是":L"否");
		}
	}
	else
	{
		for ( vector<FileSuffix_Struct>::iterator it = theApp.m_vecFileSuffix_NoValid.begin(); it != theApp.m_vecFileSuffix_NoValid.end(); it++,index++ )
		{
			m_listFileSuffix.InsertItem(index , it->FileNote );
			m_listFileSuffix.SetItemText(index, 1, it->FileSuffix);
			m_listFileSuffix.SetItemText(index, 2, it->bEnable == 1 ? L"是":L"否");
		}
	}
	

	if (m_listFileSuffix.GetItemCount()>0) 
	{
		m_listFileSuffix.SetSelectionMark(0);
		m_listFileSuffix.SetItemState(0, LVIS_SELECTED, LVIS_SELECTED);
	}


}

void CBackInOrValidSetDlg::LoadFileNamesInfos()
{

	m_listFileSuffix.DeleteAllItems();
	size_t index=0;
	for ( vector<FileName_Struct>::iterator it = theApp.m_vecFileNames.begin(); it != theApp.m_vecFileNames.end(); it++,index++ )
	{
		m_listFileSuffix.InsertItem(index , it->FileNote );
		m_listFileSuffix.SetItemText(index, 1, it->FileName);
		m_listFileSuffix.SetItemText(index, 2, it->bEnable == 1 ? L"是":L"否");
	}

	if (m_listFileSuffix.GetItemCount()>0) 
	{
		m_listFileSuffix.SetSelectionMark(0);
		m_listFileSuffix.SetItemState(0, LVIS_SELECTED, LVIS_SELECTED);
	}

}


void CBackInOrValidSetDlg::OnBnClickedButtonNew()
{
	int index;
	if ( m_sft == SFT_FileFullName )
	{
		FileName_Struct newFileName;
		newFileName.FileNote=L"文件说明";
		newFileName.FileName=L"完整文件名";
		newFileName.bEnable = 1;
		theApp.m_vecFileNames.push_back(newFileName);
		index = theApp.m_vecFileNames.size()-1;
		LoadFileNamesInfos();
		m_listFileSuffix.SetSelectionMark(index);
		SetCurFileNameInfo(index);
	}
	else
	{
		FileSuffix_Struct newFileSufx;
		newFileSufx.FileNote=L"所有";
		newFileSufx.FileSuffix=L"*.txt";
		newFileSufx.bEnable = 1;
		if ( m_eOType == eOT_Valid )
		{
			theApp.m_vecFileSuffix.push_back(newFileSufx);
			index = theApp.m_vecFileSuffix.size()-1;
		}
		else
		{
			theApp.m_vecFileSuffix_NoValid.push_back(newFileSufx);
			index = theApp.m_vecFileSuffix_NoValid.size()-1;
		}
		
		LoadFileSufInfos();
		m_listFileSuffix.SetSelectionMark(index);
		SetCurFileSufInfo(index);
	}
	
	
	//index=theApp.scheduler->AddSchedule(newschedule);
	//m_list.InsertItem(index , newschedule->title );
	//m_list.SetSelectionMark(index);
}
void CBackInOrValidSetDlg::AlterCurFileSufInfo(size_t index_cur,FileSuffix_Struct& stFileSux)
{
	size_t index=0;
	if ( m_eOType == eOT_Valid )
	{
		for ( vector<FileSuffix_Struct>::iterator it = theApp.m_vecFileSuffix.begin(); it != theApp.m_vecFileSuffix.end(); it++,index++ )
		{
			if ( index_cur == index )
			{
				it->FileNote = stFileSux.FileNote;
				it->FileSuffix = stFileSux.FileSuffix;
				it->bEnable = stFileSux.bEnable;
				break;
			}
		}
	}
	else
	{
		for ( vector<FileSuffix_Struct>::iterator it = theApp.m_vecFileSuffix_NoValid.begin(); it != theApp.m_vecFileSuffix_NoValid.end(); it++,index++ )
		{
			if ( index_cur == index )
			{
				it->FileNote = stFileSux.FileNote;
				it->FileSuffix = stFileSux.FileSuffix;
				it->bEnable = stFileSux.bEnable;
				break;
			}
		}
	}
	
}
void CBackInOrValidSetDlg::AlterCurFileNameInfo(size_t index_cur,FileName_Struct& stFileName)
{
	size_t index=0;
	for ( vector<FileName_Struct>::iterator it = theApp.m_vecFileNames.begin(); it != theApp.m_vecFileNames.end(); it++,index++ )
	{
		if ( index_cur == index )
		{
			it->FileNote = stFileName.FileNote;
			it->FileName = stFileName.FileName;
			it->bEnable = stFileName.bEnable;
			break;
		}
	}
}

void CBackInOrValidSetDlg::DelCurFileSufInfo(size_t index_cur)
{
	if ( m_eOType == eOT_Valid )
	{
		if (index_cur>=theApp.m_vecFileSuffix.size())
			return;
		size_t index=0;
		for ( vector<FileSuffix_Struct>::iterator it = theApp.m_vecFileSuffix.begin(); it != theApp.m_vecFileSuffix.end(); it++,index++ )
		{
			if ( index_cur == index )
			{
				theApp.m_vecFileSuffix.erase(it);
				break;
			}
		}
	}
	else
	{
		if (index_cur>=theApp.m_vecFileSuffix_NoValid.size())
			return;
		size_t index=0;
		for ( vector<FileSuffix_Struct>::iterator it = theApp.m_vecFileSuffix_NoValid.begin(); it != theApp.m_vecFileSuffix_NoValid.end(); it++,index++ )
		{
			if ( index_cur == index )
			{
				theApp.m_vecFileSuffix_NoValid.erase(it);
				break;
			}
		}
	}
	
}

void CBackInOrValidSetDlg::DelCurFileNameInfo(size_t index_cur)
{
	if (index_cur>=theApp.m_vecFileNames.size())
		return;
	size_t index=0;
	for ( vector<FileName_Struct>::iterator it = theApp.m_vecFileNames.begin(); it != theApp.m_vecFileNames.end(); it++,index++ )
	{
		if ( index_cur == index )
		{
			theApp.m_vecFileNames.erase(it);
			break;
		}
	}
}


void CBackInOrValidSetDlg::SetCurFileNameInfo(size_t index_cur)
{
	size_t index=0;
	for ( vector<FileName_Struct>::iterator it = theApp.m_vecFileNames.begin(); it != theApp.m_vecFileNames.end(); it++,index++ )
	{
		if ( index_cur == index )
		{
			SetDlgItemText(IDC_EDIT_FILE_NOTE,it->FileNote);
			SetDlgItemText(IDC_EDIT_FILE_SUFFIX,it->FileName);
			((CComboBox*)GetDlgItem(IDC_COMBO_ENABLE))->SetCurSel(it->bEnable);
			break;
		}
	}
}
void CBackInOrValidSetDlg::SetCurFileSufInfo(size_t index_cur)
{
	size_t index=0;
	if ( m_eOType == eOT_Valid )
	{
		for ( vector<FileSuffix_Struct>::iterator it = theApp.m_vecFileSuffix.begin(); it != theApp.m_vecFileSuffix.end(); it++,index++ )
		{
			if ( index_cur == index )
			{
				SetDlgItemText(IDC_EDIT_FILE_NOTE,it->FileNote);
				SetDlgItemText(IDC_EDIT_FILE_SUFFIX,it->FileSuffix);
				((CComboBox*)GetDlgItem(IDC_COMBO_ENABLE))->SetCurSel(it->bEnable);
				break;
			}
		}
	}
	else
	{
		for ( vector<FileSuffix_Struct>::iterator it = theApp.m_vecFileSuffix_NoValid.begin(); it != theApp.m_vecFileSuffix_NoValid.end(); it++,index++ )
		{
			if ( index_cur == index )
			{
				SetDlgItemText(IDC_EDIT_FILE_NOTE,it->FileNote);
				SetDlgItemText(IDC_EDIT_FILE_SUFFIX,it->FileSuffix);
				((CComboBox*)GetDlgItem(IDC_COMBO_ENABLE))->SetCurSel(it->bEnable);
				break;
			}
		}
	}
	
}
void CBackInOrValidSetDlg::GetCurFileSufInfo(FileSuffix_Struct& stFileSux)
{
	GetDlgItemText(IDC_EDIT_FILE_NOTE,stFileSux.FileNote);
	GetDlgItemText(IDC_EDIT_FILE_SUFFIX,stFileSux.FileSuffix);
	stFileSux.bEnable=((CComboBox*)GetDlgItem(IDC_COMBO_ENABLE))->GetCurSel();
}
void CBackInOrValidSetDlg::GetCurFileNameInfo(FileName_Struct& stFileName)
{
	GetDlgItemText(IDC_EDIT_FILE_NOTE,stFileName.FileNote);
	GetDlgItemText(IDC_EDIT_FILE_SUFFIX,stFileName.FileName);
	stFileName.bEnable=((CComboBox*)GetDlgItem(IDC_COMBO_ENABLE))->GetCurSel();

}
void CBackInOrValidSetDlg::SetCurFileNameInfo(FileName_Struct& stFileName)
{
	SetDlgItemText(IDC_EDIT_FILE_NOTE,stFileName.FileNote);
	SetDlgItemText(IDC_EDIT_FILE_SUFFIX,stFileName.FileName);
	((CComboBox*)GetDlgItem(IDC_COMBO_ENABLE))->SetCurSel(stFileName.bEnable);
}
void CBackInOrValidSetDlg::SetCurFileSufInfo(FileSuffix_Struct& stFileSux)
{
	SetDlgItemText(IDC_EDIT_FILE_NOTE,stFileSux.FileNote);
	SetDlgItemText(IDC_EDIT_FILE_SUFFIX,stFileSux.FileSuffix);
	((CComboBox*)GetDlgItem(IDC_COMBO_ENABLE))->SetCurSel(stFileSux.bEnable);
}
void CBackInOrValidSetDlg::OnNMClickListFilesuffix(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
	// TODO: 在此添加控件通知处理程序代码
	if (m_listFileSuffix.GetSelectionMark()>-1) 
	{
		if ( m_sft == SFT_FileFullName )
		{
			SetCurFileNameInfo(m_listFileSuffix.GetSelectionMark());
		}
		else
		{
			SetCurFileSufInfo(m_listFileSuffix.GetSelectionMark());
		}
		
	}
	*pResult = 0;
}


void CBackInOrValidSetDlg::OnBnClickedButtonApp()
{
	int index=m_listFileSuffix.GetSelectionMark();

	if (index>-1) 
	{
		if ( m_sft == SFT_FileFullName )
		{
			FileName_Struct stFileSux;
			GetCurFileNameInfo(stFileSux);
			AlterCurFileNameInfo(index,stFileSux);
			m_listFileSuffix.SetItemText(index, 0, stFileSux.FileNote);
			m_listFileSuffix.SetItemText(index, 1, stFileSux.FileName);
			m_listFileSuffix.SetItemText(index, 2, stFileSux.bEnable == 1 ? L"是":L"否");
			
		}
		else
		{
			FileSuffix_Struct stFileSux;
			GetCurFileSufInfo(stFileSux);
			AlterCurFileSufInfo(index,stFileSux);
			m_listFileSuffix.SetItemText(index, 0, stFileSux.FileNote);
			m_listFileSuffix.SetItemText(index, 1, stFileSux.FileSuffix);
			m_listFileSuffix.SetItemText(index, 2, stFileSux.bEnable == 1 ? L"是":L"否");
		}
		
	}
	//LoadFileSufInfos();
}


void CBackInOrValidSetDlg::OnBnClickedButtonDel()
{
	int index=m_listFileSuffix.GetSelectionMark();
	if ( m_sft == SFT_FileFullName )
	{
		if (index!=-1) 
			DelCurFileNameInfo(index);
		LoadFileNamesInfos();
		FileName_Struct stFileSux;
		SetCurFileNameInfo(stFileSux);
	}
	else
	{
		if (index!=-1) 
			DelCurFileSufInfo(index);
		LoadFileSufInfos();
		FileSuffix_Struct stFileSux;
		SetCurFileSufInfo(stFileSux);
	}
	
}


void CBackInOrValidSetDlg::OnClose()
{
	// TODO: 在此添加消息处理程序代码和/或调用默认值
	//下面方法中文写入失败
	CString temp;
	if ( m_sft == SFT_FileFullName )
	{
		CIni ini(thePrefs.GetConfigFile(), _T("Back_NameValids"));
		int nCount = theApp.m_vecFileNames.size();
		ini.WriteInt(_T("Count"), nCount);
		size_t i=0;
		for ( vector<FileName_Struct>::iterator it = theApp.m_vecFileNames.begin(); it != theApp.m_vecFileNames.end(); it++,i++ )
		{
			temp.Format(_T("Back_NameValids#%i"),i);		
			ini.WriteString(_T("FileNote"),it->FileNote,temp); //中文写入失败
			ini.WriteString(_T("FileName"),it->FileName);
			ini.WriteInt(_T("bEnable"),it->bEnable);
		}
	}
	else
	{
		if ( m_eOType == eOT_Valid )
		{
			CIni ini(thePrefs.GetConfigFile(), _T("Back_Valids"));
			int nCount = theApp.m_vecFileSuffix.size();
			ini.WriteInt(_T("Count"), nCount);
			size_t i=0;
			for ( vector<FileSuffix_Struct>::iterator it = theApp.m_vecFileSuffix.begin(); it != theApp.m_vecFileSuffix.end(); it++,i++ )
			{
				temp.Format(_T("Back_Valids#%i"),i);		
				ini.WriteString(_T("FileNote"),it->FileNote,temp); //中文写入失败
				ini.WriteString(_T("FileSuffix"),it->FileSuffix);
				ini.WriteInt(_T("bEnable"),it->bEnable);
			}
		}
		else
		{
			CIni ini(thePrefs.GetConfigFile(), _T("Back_NoValids"));
			int nCount = theApp.m_vecFileSuffix_NoValid.size();
			ini.WriteInt(_T("Count"), nCount);
			size_t i=0;
			for ( vector<FileSuffix_Struct>::iterator it = theApp.m_vecFileSuffix_NoValid.begin(); it != theApp.m_vecFileSuffix_NoValid.end(); it++,i++ )
			{
				temp.Format(_T("Back_NoValids#%i"),i);		
				ini.WriteString(_T("FileNote"),it->FileNote,temp); //中文写入失败
				ini.WriteString(_T("FileSuffix"),it->FileSuffix);
				ini.WriteInt(_T("bEnable"),it->bEnable);
			}
		}
		
	}
	
	//以下注册表方式不太可行
	/*CString strText;
	CStringArray arrString;
	for( int idx = 0; idx < theApp.m_vecFileSuffix.size(); idx++ ) 
	{
	m_TreeSelect.GetLBText(idx, strText);
	arrString.Add(strText);
	}
	RegReadWriteStrings(_T("TreesList"), arrString, FALSE);*/
	CDialog::OnClose();
}
