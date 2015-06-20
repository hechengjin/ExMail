// BackDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "wyytTools.h"
#include "BackDlg.h"
#include "afxdialogex.h"
#include "Util.h"
#include "BackInOrValidSetDlg.h"

using namespace std;	
// CBackDlg 对话框

IMPLEMENT_DYNAMIC(CBackDlg, CSAPrefsSubDlg)

CBackDlg::CBackDlg(CWnd* pParent /*=NULL*/)
	: CSAPrefsSubDlg(CBackDlg::IDD, pParent)
{
	dwPos = 0;
}

CBackDlg::~CBackDlg()
{
}

void CBackDlg::DoDataExchange(CDataExchange* pDX)
{
	CSAPrefsSubDlg::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_CHECK_SAVEPATH, m_SavePathBtn);
	DDX_Control(pDX, IDC_LIST_LOG, m_LogList);
	DDX_Control(pDX, IDC_PROGRESS, m_ProgCtrl);
	DDX_Control(pDX, IDC_DATETIMEPICKER_DATE, m_DTDate);
	DDX_Control(pDX, IDC_DATETIMEPICKER_TIME, m_DTTime);
}


BEGIN_MESSAGE_MAP(CBackDlg, CSAPrefsSubDlg)
	ON_BN_CLICKED(IDC_BUTTON_SETCOPYFILE, &CBackDlg::OnBnClickedButtonSetcopyfile)
	ON_BN_CLICKED(IDC_BUTTON_SOURCE, &CBackDlg::OnBnClickedButtonSource)
	ON_BN_CLICKED(IDC_BUTTON_TARGET, &CBackDlg::OnBnClickedButtonTarget)
	ON_BN_CLICKED(IDC_CHECK_SAVEPATH, &CBackDlg::OnBnClickedCheckSavepath)
	ON_BN_CLICKED(IDC_BUTTON_APP, &CBackDlg::OnBnClickedButtonApp)
	ON_BN_CLICKED(IDC_BUTTON_STARTBACK, &CBackDlg::OnBnClickedButtonStartback)
	ON_WM_TIMER()
	ON_BN_CLICKED(IDC_BUTTON_SETFILENAME, &CBackDlg::OnBnClickedButtonSetfilename)
	ON_BN_CLICKED(IDC_RADIO_VALID, &CBackDlg::OnBnClickedRadioValid)
	ON_BN_CLICKED(IDC_RADIO_NOVALID, &CBackDlg::OnBnClickedRadioNovalid)
	ON_BN_CLICKED(IDC_BUTTON_QUERY, &CBackDlg::OnBnClickedButtonQuery)
	ON_NOTIFY(DTN_DATETIMECHANGE, IDC_DATETIMEPICKER_DATE, &CBackDlg::OnDtnDatetimechangeDatetimepickerDate)
	ON_NOTIFY(DTN_DATETIMECHANGE, IDC_DATETIMEPICKER_TIME, &CBackDlg::OnDtnDatetimechangeDatetimepickerTime)
END_MESSAGE_MAP()


// CBackDlg 消息处理程序



void CBackDlg::OnBnClickedButtonSource()
{
	TCHAR szFolderPath[MAX_PATH] = {0}, szFullPath[MAX_PATH] = {0};
	BROWSEINFO mBroInfo = {0};
	mBroInfo.hwndOwner = m_hWnd;
	mBroInfo.lpszTitle = _T("请选择一文件夹...");
	mBroInfo.pszDisplayName = szFolderPath;
	mBroInfo.ulFlags = BIF_RETURNONLYFSDIRS;
	ITEMIDLIST *pidl = ::SHBrowseForFolder(&mBroInfo);
	if (::SHGetPathFromIDList(pidl, szFullPath))
	{
		SetDlgItemText(IDC_EDIT_SOURCE,szFullPath);
		OnBnClickedButtonApp();
	}
}


void CBackDlg::OnBnClickedButtonTarget()
{
	TCHAR szFolderPath[MAX_PATH] = {0}, szFullPath[MAX_PATH] = {0};
	BROWSEINFO mBroInfo = {0};
	mBroInfo.hwndOwner = m_hWnd;
	mBroInfo.lpszTitle = _T("请选择一文件夹...");
	mBroInfo.pszDisplayName = szFolderPath;
	mBroInfo.ulFlags = BIF_RETURNONLYFSDIRS;
	ITEMIDLIST *pidl = ::SHBrowseForFolder(&mBroInfo);
	if (::SHGetPathFromIDList(pidl, szFullPath))
	{
		SetDlgItemText(IDC_EDIT_TARGET,szFullPath);
		OnBnClickedButtonApp();
	}
}

void CBackDlg::LoadStartTime()
{
	CIni ini(thePrefs.GetConfigFile(), _T("StartTime"));
	m_nStartDate = ini.GetUInt64(_T("Day"),0);
	//CString csStartDate;
	//csStartDate.Format(L"%ld",m_nStartDate);
	//MessageBox(csStartDate);
	CTime startDay(m_nStartDate); 
	if ( m_nStartDate > 0 )
	{
		m_DTDate.SetTime(&startDay);
	}
	


	m_nStartTime = ini.GetUInt64(_T("Time"),0);

	CTime startTime(m_nStartTime); 
	if ( m_nStartDate > 0 )
		m_DTTime.SetTime(&startTime);



}

void CBackDlg::LoadFileNamesFromFile()
{
	CString strName;
	FileName_Struct FSStemp;
	CIni ini(thePrefs.GetConfigFile(), _T("Back_NameValids"));

	size_t max = ini.GetInt(_T("Count"),0);
	size_t count = 0;

	while (count<max) 
	{
		strName.Format(_T("Back_NameValids#%i"),count);
		FSStemp.FileNote = ini.GetString(_T("FileNote"), NULL, strName);
		FSStemp.FileName = ini.GetString(_T("FileName"), NULL, strName);
		FSStemp.bEnable = ini.GetInt(_T("bEnable"), 1, strName);
		theApp.m_vecFileNames.push_back(FSStemp);
		count++;
	}
}
void CBackDlg::LoadFileSufixxFromFile()
{
	CString strName;
	FileSuffix_Struct FSStemp;
	CIni ini(thePrefs.GetConfigFile(), _T("Back_Valids"));

	size_t max = ini.GetInt(_T("Count"),0);
	size_t count = 0;

	while (count<max) 
	{
		strName.Format(_T("Back_Valids#%i"),count);
		FSStemp.FileNote = ini.GetString(_T("FileNote"), NULL, strName);
		FSStemp.FileSuffix = ini.GetString(_T("FileSuffix"), NULL, strName);
		FSStemp.bEnable = ini.GetInt(_T("bEnable"), 1, strName);
		theApp.m_vecFileSuffix.push_back(FSStemp);
		count++;
	}
}

void CBackDlg::LoadFileSufixxFromFile_NoVaild()
{
	CString strName;
	FileSuffix_Struct FSStemp;
	CIni ini(thePrefs.GetConfigFile(), _T("Back_NoValids"));

	size_t max = ini.GetInt(_T("Count"),0);
	size_t count = 0;

	while (count<max) 
	{
		strName.Format(_T("Back_NoValids#%i"),count);
		FSStemp.FileNote = ini.GetString(_T("FileNote"), NULL, strName);
		FSStemp.FileSuffix = ini.GetString(_T("FileSuffix"), NULL, strName);
		FSStemp.bEnable = ini.GetInt(_T("bEnable"), 1, strName);
		theApp.m_vecFileSuffix_NoValid.push_back(FSStemp);
		count++;
	}
}

BOOL CBackDlg::OnInitDialog()
{
	CSAPrefsSubDlg::OnInitDialog();

	// TODO:  在此添加额外的初始化
	//theApp.m_vecFileSuffix[L"*.h"]=L"C/C++头文件";
	//theApp.m_vecFileSuffix[L"*.cpp"]=L"C++源文件";
	LoadFileSufixxFromFile();
	LoadFileSufixxFromFile_NoVaild();
	LoadFileNamesFromFile();
	GetRegProfile();
	m_LogList.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
	m_LogList.InsertColumn(0, _T("名 称"), LVCFMT_LEFT, 460);
	m_LogList.InsertColumn(1, _T("类 型"), LVCFMT_LEFT, 70);
	m_LogList.InsertColumn(2, _T("状 态"), LVCFMT_LEFT, 80);

	
	
	(m_Profile.m_bSavePath == TRUE) ? m_SavePathBtn.SetCheck(BST_CHECKED) : m_SavePathBtn.SetCheck(BST_UNCHECKED);
	if ( m_Profile.m_bSavePath == TRUE )
	{
		SetDlgItemText(IDC_EDIT_SOURCE,m_Profile.m_strSourcePath);
		SetDlgItemText(IDC_EDIT_TARGET,m_Profile.m_strTargetPath);
	}
	if ( m_Profile.m_nOperType == eOT_NoValid )
	{
		((CButton *)GetDlgItem(IDC_RADIO_NOVALID))->SetCheck(TRUE);
		GetDlgItem(IDC_BUTTON_SETFILENAME)->EnableWindow(FALSE);//
	}
	else
	{
		((CButton *)GetDlgItem(IDC_RADIO_VALID))->SetCheck(TRUE);
		GetDlgItem(IDC_BUTTON_SETFILENAME)->EnableWindow(TRUE);//FALSE
	}

	LoadStartTime();
	return TRUE;  // return TRUE unless you set the focus to a control
	// 异常: OCX 属性页应返回 FALSE
}

BOOL CBackDlg::GetRegProfile()//改用从配置文件读取
{
	//m_Profile.m_bSavePath = RegReadWriteDword(_T("Back_SavePath"), TRUE);
	//RegReadWriteStrings(_T("Back_SourcePath"), m_Profile.m_strSourcePath, TRUE);
	//RegReadWriteStrings(_T("Back_TargetPath"), m_Profile.m_strTargetPath, TRUE);
	CIni ini(thePrefs.GetConfigFile(), _T("Back_PathInfo"));

	m_Profile.m_bSavePath = ini.GetInt(_T("Back_bSavePath"),0);
	if ( m_Profile.m_bSavePath)
	{
		m_Profile.m_strSourcePath = ini.GetString(_T("Back_SourcePath"), L"C:\\", L"Back_PathInfo");
		m_Profile.m_strTargetPath = ini.GetString(_T("Back_TargetPath"), L"D:\\", L"Back_PathInfo");
	}
	m_Profile.m_nOperType = ini.GetInt(_T("Back_nOperType"),0);

	return TRUE;
}
void CBackDlg::OnBnClickedCheckSavepath()
{
	OnBnClickedButtonApp();
}


void CBackDlg::OnBnClickedButtonApp()
{
	int nCheck = m_SavePathBtn.GetCheck();
	CIni ini(thePrefs.GetConfigFile(), _T("Back_PathInfo"));
	if ( nCheck == BST_CHECKED ) 
	{
		m_Profile.m_bSavePath = 1;
		GetDlgItemText(IDC_EDIT_SOURCE,m_Profile.m_strSourcePath);
		GetDlgItemText(IDC_EDIT_TARGET,m_Profile.m_strTargetPath);
		ini.WriteInt(_T("Back_bSavePath"), m_Profile.m_bSavePath);	
		ini.WriteString(_T("Back_SourcePath"),m_Profile.m_strSourcePath); //中文写入失败
		ini.WriteString(_T("Back_TargetPath"),m_Profile.m_strTargetPath);

		//RegReadWriteStrings(_T("Back_SourcePath"), m_Profile.m_strSourcePath, FALSE);
		//RegReadWriteStrings(_T("Back_TargetPath"), m_Profile.m_strTargetPath, FALSE);
	}
	else if ( nCheck == BST_UNCHECKED ) 
	{
		m_Profile.m_bSavePath = 0;
		ini.WriteInt(_T("Back_bSavePath"), m_Profile.m_bSavePath);
	}
	if ( ((CButton *)GetDlgItem(IDC_RADIO_VALID))->GetCheck() )//返回1表示选上，0表示没选上
	{
		m_Profile.m_nOperType = eOT_Valid;
		GetDlgItem(IDC_BUTTON_SETFILENAME)->EnableWindow(TRUE);//FALSE
	}
	else
	{
		m_Profile.m_nOperType  = eOT_NoValid;
		GetDlgItem(IDC_BUTTON_SETFILENAME)->EnableWindow(FALSE);//FALSE
	}
	ini.WriteInt(_T("Back_nOperType"), m_Profile.m_nOperType);
}


void CBackDlg::OnBnClickedButtonStartback()
{
	m_curTime = 0;
	m_LogList.DeleteAllItems();
	if ( m_Profile.m_strSourcePath.IsEmpty() || m_Profile.m_strTargetPath.IsEmpty() ) 
	{
		MessageBox(_T("请先选择相应来源或目标目录！"), _T("提示"), MB_OK|MB_ICONINFORMATION);
		return;
	}
	if ( !Util::IsFileExist(m_Profile.m_strTargetPath) )//目标目录不存在，则创建它
	{
		int bRet = Util::MkDir(m_Profile.m_strTargetPath);
		if ( 0 != bRet )
		{
			MessageBox(_T("目标目录创建失败！"), _T("提示"), MB_OK|MB_ICONINFORMATION);
			return;
		}
	}
	if ( !Util::IsFileExist(m_Profile.m_strSourcePath) )//源目录则提示
	{
		MessageBox(_T("源目录不存在！"), _T("提示"), MB_OK|MB_ICONINFORMATION);
		return;
	}
	
	CWinThread *pTread = AfxBeginThread(ScanAndBackFunc, this);
}
BOOL CBackDlg::EnableWindow(UINT uID, BOOL bEnable)
{
	return GetDlgItem(uID)->EnableWindow(bEnable);
}

void CBackDlg::ShowLogMsg(CString strPath, INT bFolder, BOOL bSucceed)
{
	CString strText;
	int nCount = m_LogList.GetItemCount();
	m_LogList.InsertItem(nCount, _T(""));
	m_LogList.SetItemText(nCount, 0, strPath);
	if ( bFolder != -1 ) 
	{
		strText = (bFolder == TRUE) ? _T("文件夹") : _T("文件");
		m_LogList.SetItemText(nCount, 1, strText);
		strText = (bSucceed == TRUE) ? _T("成功") : _T("失败");
		m_LogList.SetItemText(nCount, 2, strText);
	}
}

void CBackDlg::OnTimer(UINT_PTR nIDEvent)
{
	// TODO: 在此添加消息处理程序代码和/或调用默认值
	m_ProgCtrl.SetPos(dwPos);
	CSAPrefsSubDlg::OnTimer(nIDEvent);
}


void CBackDlg::OnBnClickedButtonSetfilename()
{
	CBackInOrValidSetDlg dlg(NULL,SFT_FileFullName);
	dlg.DoModal();
}

void CBackDlg::OnBnClickedButtonSetcopyfile()
{
	// TODO: 在此添加控件通知处理程序代码
	if (m_Profile.m_nOperType == eOT_Valid )
	{
		CBackInOrValidSetDlg dlg(NULL,SFT_FileSuffix,eOT_Valid);
		dlg.DoModal();
	}
	else
	{
		CBackInOrValidSetDlg dlg(NULL,SFT_FileSuffix,eOT_NoValid);
		dlg.DoModal();
	}
	
}


void CBackDlg::OnBnClickedRadioValid()
{
	OnBnClickedButtonApp();
}


void CBackDlg::OnBnClickedRadioNovalid()
{
	OnBnClickedButtonApp();
}


void CBackDlg::OnBnClickedButtonQuery()
{
	CTime startDay=CTime::GetCurrentTime(); //获取系统日期;
	m_DTDate.GetTime(startDay);
	CTime startDate(startDay.GetYear(),startDay.GetMonth(),startDay.GetDay(),0,0,0);
	CString csStartDate;
	csStartDate.Format(L"%d",startDate.GetTime());
	//MessageBox(csStartDate);

	CTime startTime=CTime::GetCurrentTime(); //获取系统日期;
	m_DTTime.GetTime(startTime);
	//CTime startTime2(1970,1,1,8,0,0);  //相对这个时间的秒数
	CTime startDateTime(startDay.GetYear(),startDay.GetMonth(),startDay.GetDay(),startTime.GetHour(),startTime.GetMinute(),startTime.GetSecond());  //相对这个时间的秒数
	CString csStartTime;
	csStartTime.Format(L"%d",startDateTime.GetTime());
	//MessageBox(csStartTime);

	m_curTime = startDateTime.GetTime();

	m_LogList.DeleteAllItems();
	if ( m_Profile.m_strSourcePath.IsEmpty()  ) 
	{
		MessageBox(_T("请先选择相应来源或目标目录！"), _T("提示"), MB_OK|MB_ICONINFORMATION);
		return;
	}
	
	if ( !Util::IsFileExist(m_Profile.m_strSourcePath) )//源目录则提示
	{
		MessageBox(_T("源目录不存在！"), _T("提示"), MB_OK|MB_ICONINFORMATION);
		return;
	}

	CWinThread *pTread = AfxBeginThread(ScanAndBackFunc, this);
	
}


void CBackDlg::OnDtnDatetimechangeDatetimepickerDate(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMDATETIMECHANGE pDTChange = reinterpret_cast<LPNMDATETIMECHANGE>(pNMHDR);
	{
		CTime startDay=CTime::GetCurrentTime(); //获取系统日期;
		m_DTDate.GetTime(startDay);
		CTime startDate(startDay.GetYear(),startDay.GetMonth(),startDay.GetDay(),0,0,0);
		/*CString csStartDate;
		csStartDate.Format(L"%d",startDate.GetTime());
		MessageBox(csStartDate);*/
		CIni ini(thePrefs.GetConfigFile(), _T("StartTime"));
		ini.WriteUInt64(_T("Day"), startDate.GetTime());	

	}
	*pResult = 0;
}


void CBackDlg::OnDtnDatetimechangeDatetimepickerTime(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMDATETIMECHANGE pDTChange = reinterpret_cast<LPNMDATETIMECHANGE>(pNMHDR);
	CTime startTime=CTime::GetCurrentTime(); //获取系统日期;
		m_DTTime.GetTime(startTime);
		//CTime startTime2(startTime.GetYear(),startTime.GetMonth(),startTime.GetDay(),startTime.GetHour(),startTime.GetMinute(),startTime.GetSecond());
		
		/*CString csStartDate;
		csStartDate.Format(L"%d",startDate.GetTime());
		MessageBox(csStartDate);*/
		CIni ini(thePrefs.GetConfigFile(), _T("StartTime"));
		ini.WriteUInt64(_T("Time"), startTime.GetTime());	

	*pResult = 0;
}
