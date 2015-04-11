/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <windows.h>
#include <shellapi.h>

#include "nsMessengerWinIntegration.h"
#include "nsIMsgAccountManager.h"
#include "nsIMsgMailSession.h"
#include "nsIMsgIncomingServer.h"
#include "nsIMsgIdentity.h"
#include "nsIMsgAccount.h"
#include "nsIMsgFolder.h"
#include "nsIMsgWindow.h"
#include "nsCOMPtr.h"
#include "nsMsgBaseCID.h"
#include "nsMsgFolderFlags.h"
#include "nsDirectoryServiceDefs.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsIDirectoryService.h"
#include "nsIWindowWatcher.h"
#include "nsIWindowMediator.h"
#include "nsIDOMWindow.h"
#include "nsPIDOMWindow.h"
#include "nsIDocShell.h"
#include "nsIBaseWindow.h"
#include "nsIWidget.h"
#include "nsWidgetsCID.h"
#include "MailNewsTypes.h"
#include "nsIMessengerWindowService.h"
#include "prprf.h"
#include "nsIWeakReference.h"
#include "nsIStringBundle.h"
#include "nsIAlertsService.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIProperties.h"
#include "nsISupportsPrimitives.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIWeakReferenceUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsNativeCharsetUtils.h"
#include "nsMsgUtils.h"
#ifdef MOZ_THUNDERBIRD
#include "mozilla/LookAndFeel.h"
#endif
#include "mozilla/Services.h"

#include "nsToolkitCompsCID.h"
#include <stdlib.h>
#define PROFILE_COMMANDLINE_ARG " -profile "

#define NOTIFICATIONCLASSNAME "MailBiffNotificationMessageWindow"
#define UNREADMAILNODEKEY "Software\\Microsoft\\Windows\\CurrentVersion\\UnreadMail\\"
#define SHELL32_DLL L"shell32.dll"
#define DOUBLE_QUOTE "\""
#define MAIL_COMMANDLINE_ARG " -mail"
#define IDI_MAILBIFF 32576
#define UNREAD_UPDATE_INTERVAL	(20 * 1000)	// 20 seconds
#define ALERT_CHROME_URL "chrome://messenger/content/newmailalert.xul"
#define NEW_MAIL_ALERT_ICON "chrome://messenger/skin/icons/new-mail-alert.png"
#define SHOW_ALERT_PREF     "mail.biff.show_alert"
#define SHOW_TRAY_ICON_PREF "mail.biff.show_tray_icon"
#define SHOW_BALLOON_PREF   "mail.biff.show_balloon"

// since we are including windows.h in this file, undefine get user name....
#ifdef GetUserName
#undef GetUserName
#endif

#ifndef NIIF_USER
#define NIIF_USER       0x00000004
#endif

#ifndef NIIF_NOSOUND
#define NIIF_NOSOUND    0x00000010
#endif

#ifndef NIN_BALOONUSERCLICK
#define NIN_BALLOONUSERCLICK (WM_USER + 5)
#endif

using namespace mozilla;

// begin shameless copying from nsNativeAppSupportWin
HWND hwndForDOMWindow( nsISupports *window )
{
  nsCOMPtr<nsPIDOMWindow> win( do_QueryInterface(window) );
  if ( !win )
      return 0;

  nsCOMPtr<nsIBaseWindow> ppBaseWindow =
      do_QueryInterface( win->GetDocShell() );
  if (!ppBaseWindow) return 0;

  nsCOMPtr<nsIWidget> ppWidget;
  ppBaseWindow->GetMainWidget( getter_AddRefs( ppWidget ) );

  return (HWND)( ppWidget->GetNativeData( NS_NATIVE_WIDGET ) );
}

static void activateWindow( nsIDOMWindow *win )
{
  // Try to get native window handle.
  HWND hwnd = hwndForDOMWindow( win );
  if ( hwnd )
  {
    // Restore the window if it is minimized.
    if ( ::IsIconic( hwnd ) )
      ::ShowWindow( hwnd, SW_RESTORE );
    // Use the OS call, if possible.
    ::SetForegroundWindow( hwnd );
  } else // Use internal method.
    win->Focus();
}
// end shameless copying from nsNativeAppWinSupport.cpp

static void openMailWindow(const nsACString& aFolderUri)
{
  nsresult rv;
  nsCOMPtr<nsIMsgMailSession> mailSession ( do_GetService(NS_MSGMAILSESSION_CONTRACTID, &rv));
  if (NS_FAILED(rv))
    return;

  nsCOMPtr<nsIMsgWindow> topMostMsgWindow;
  rv = mailSession->GetTopmostMsgWindow(getter_AddRefs(topMostMsgWindow));
  if (topMostMsgWindow)
  {
    nsCOMPtr<nsIDOMWindow> domWindow;
    topMostMsgWindow->GetDomWindow(getter_AddRefs(domWindow));
    if (domWindow)
    {
      if (!aFolderUri.IsEmpty())
      {
        nsCOMPtr<nsIMsgWindowCommands> windowCommands;
        topMostMsgWindow->GetWindowCommands(getter_AddRefs(windowCommands));
        if (windowCommands)
          windowCommands->SelectFolder(aFolderUri);
      }
      activateWindow(domWindow);
      return;
    }
  }

  {
    // the user doesn't have a mail window open already so open one for them...
    nsCOMPtr<nsIMessengerWindowService> messengerWindowService =
      do_GetService(NS_MESSENGERWINDOWSERVICE_CONTRACTID);
    // if we want to preselect the first account with new mail,
    // here is where we would try to generate a uri to pass in
    // (and add code to the messenger window service to make that work)
    if (messengerWindowService)
      messengerWindowService->OpenMessengerWindowWithUri(
                                "mail:3pane", nsCString(aFolderUri).get(), nsMsgKey_None);
  }
}

static void CALLBACK delayedSingleClick(HWND msgWindow, UINT msg, INT_PTR idEvent, DWORD dwTime)
{
  ::KillTimer(msgWindow, idEvent);

#ifdef MOZ_THUNDERBIRD
  // single clicks on the biff icon should re-open the alert notification
  nsresult rv = NS_OK;
  nsCOMPtr<nsIMessengerOSIntegration> integrationService =
    do_GetService(NS_MESSENGEROSINTEGRATION_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv))
  {
    // we know we are dealing with the windows integration object
    nsMessengerWinIntegration * winIntegrationService = static_cast<nsMessengerWinIntegration*>
                                                                   (static_cast<nsIMessengerOSIntegration*>(integrationService.get()));
    winIntegrationService->ShowNewAlertNotification(true, EmptyString(), EmptyString());
  }
#endif
}

// Window proc.
static LRESULT CALLBACK MessageWindowProc( HWND msgWindow, UINT msg, WPARAM wp, LPARAM lp )
{
  if (msg == WM_USER)
  {
    if (lp == WM_LBUTTONDOWN)
    {
      // the only way to tell a single left click
      // from a double left click is to fire a timer which gets cleared if we end up getting
      // a WM_LBUTTONDBLK event.
      ::SetTimer(msgWindow, 1, GetDoubleClickTime(), (TIMERPROC) delayedSingleClick);
    }
    else if (lp == WM_LBUTTONDBLCLK || lp == NIN_BALLOONUSERCLICK)
    {
      ::KillTimer(msgWindow, 1);
      openMailWindow(EmptyCString());
    }
  }

  return TRUE;
}

static HWND msgWindow;

// Create: Register class and create window.
static nsresult Create()
{
  if (msgWindow)
    return NS_OK;

  WNDCLASS classStruct = { 0,                          // style
                           &MessageWindowProc,         // lpfnWndProc
                           0,                          // cbClsExtra
                           0,                          // cbWndExtra
                           0,                          // hInstance
                           0,                          // hIcon
                           0,                          // hCursor
                           0,                          // hbrBackground
                           0,                          // lpszMenuName
                           NOTIFICATIONCLASSNAME };    // lpszClassName

  // Register the window class.
  NS_ENSURE_TRUE( ::RegisterClass( &classStruct ), NS_ERROR_FAILURE );
  // Create the window.
  NS_ENSURE_TRUE( msgWindow = ::CreateWindow( NOTIFICATIONCLASSNAME,
                                              0,          // title
                                              WS_CAPTION, // style
                                              0,0,0,0,    // x, y, cx, cy
                                              0,          // parent
                                              0,          // menu
                                              0,          // instance
                                              0 ),        // create struct
                  NS_ERROR_FAILURE );
  return NS_OK;
}


nsMessengerWinIntegration::nsMessengerWinIntegration()
{
  mDefaultServerAtom = MsgGetAtom("DefaultServer");
  mTotalUnreadMessagesAtom = MsgGetAtom("TotalUnreadMessages");

  mUnreadTimerActive = false;

  mBiffStateAtom = MsgGetAtom("BiffState");
  mBiffIconVisible = false;
  mSuppressBiffIcon = false;
  mAlertInProgress = false;
  mBiffIconInitialized = false;
  NS_NewISupportsArray(getter_AddRefs(mFoldersWithNewMail));
}

nsMessengerWinIntegration::~nsMessengerWinIntegration()
{
  if (mUnreadCountUpdateTimer) {
    mUnreadCountUpdateTimer->Cancel();
    mUnreadCountUpdateTimer = nullptr;
  }

  // one last attempt, update the registry
  nsresult rv = UpdateRegistryWithCurrent();
  NS_ASSERTION(NS_SUCCEEDED(rv), "failed to update registry on shutdown");
  DestroyBiffIcon();
}

NS_IMPL_ADDREF(nsMessengerWinIntegration)
NS_IMPL_RELEASE(nsMessengerWinIntegration)

NS_INTERFACE_MAP_BEGIN(nsMessengerWinIntegration)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIMessengerOSIntegration)
   NS_INTERFACE_MAP_ENTRY(nsIMessengerOSIntegration)
   NS_INTERFACE_MAP_ENTRY(nsIFolderListener)
   NS_INTERFACE_MAP_ENTRY(nsIObserver)
NS_INTERFACE_MAP_END


nsresult
nsMessengerWinIntegration::ResetCurrent()
{
  mInboxURI.Truncate();
  mEmail.Truncate();

  mCurrentUnreadCount = -1;
  mLastUnreadCountWrittenToRegistry = -1;

  mDefaultAccountMightHaveAnInbox = true;
  return NS_OK;
}

NOTIFYICONDATAW sBiffIconData = { NOTIFYICONDATAW_V2_SIZE,
                                  0,
                                  2,
                                  NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_INFO,
                                  WM_USER,
                                  0,
                                  L"",
                                  0,
                                  0,
                                  L"",
                                  30000,
                                  L"",
                                  NIIF_USER | NIIF_NOSOUND };
// allow for the null terminator
static const uint32_t kMaxTooltipSize = sizeof(sBiffIconData.szTip) /
                                        sizeof(sBiffIconData.szTip[0]) - 1;
static const uint32_t kMaxBalloonSize = sizeof(sBiffIconData.szInfo) /
                                        sizeof(sBiffIconData.szInfo[0]) - 1;
static const uint32_t kMaxBalloonTitle = sizeof(sBiffIconData.szInfoTitle) /
                                         sizeof(sBiffIconData.szInfoTitle[0]) - 1;

void nsMessengerWinIntegration::InitializeBiffStatusIcon()
{
  // initialize our biff status bar icon
  Create();

  sBiffIconData.hWnd = (HWND) msgWindow;
  sBiffIconData.hIcon = ::LoadIcon( ::GetModuleHandle( NULL ), MAKEINTRESOURCE(IDI_MAILBIFF) );

  mBiffIconInitialized = true;
}

nsresult
nsMessengerWinIntegration::Init()
{
  nsresult rv;

  // Get shell32.dll handle
  HMODULE hModule = ::GetModuleHandleW(SHELL32_DLL);

  if (hModule) {
    // SHQueryUserNotificationState is available from Vista
    mSHQueryUserNotificationState = (fnSHQueryUserNotificationState)GetProcAddress(hModule, "SHQueryUserNotificationState");
  }

  nsCOMPtr <nsIMsgAccountManager> accountManager =
    do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  // because we care if the default server changes
  rv = accountManager->AddRootFolderListener(this);
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr<nsIMsgMailSession> mailSession = do_GetService(NS_MSGMAILSESSION_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  // because we care if the unread total count changes
  rv = mailSession->AddFolderListener(this, nsIFolderListener::boolPropertyChanged | nsIFolderListener::intPropertyChanged);
  NS_ENSURE_SUCCESS(rv,rv);

  // get current profile path for the commandliner
  nsCOMPtr<nsIProperties> directoryService =
    do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr<nsIFile> profilePath;
  rv = directoryService->Get(NS_APP_USER_PROFILE_50_DIR,
                             NS_GET_IID(nsIFile),
                             getter_AddRefs(profilePath));
  NS_ENSURE_SUCCESS(rv,rv);

  rv = profilePath->GetPath(mProfilePath);
  NS_ENSURE_SUCCESS(rv, rv);

  // get application path
  WCHAR appPath[_MAX_PATH] = {0};
  ::GetModuleFileNameW(nullptr, appPath, sizeof(appPath));
  mAppName.Assign((PRUnichar *)appPath);

  rv = ResetCurrent();
  NS_ENSURE_SUCCESS(rv,rv);

  return NS_OK;
}

NS_IMETHODIMP
nsMessengerWinIntegration::OnItemPropertyChanged(nsIMsgFolder *, nsIAtom *, char const *, char const *)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMessengerWinIntegration::OnItemUnicharPropertyChanged(nsIMsgFolder *, nsIAtom *, const PRUnichar *, const PRUnichar *)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMessengerWinIntegration::OnItemRemoved(nsIMsgFolder *, nsISupports *)
{
  return NS_OK;
}

nsresult nsMessengerWinIntegration::GetStringBundle(nsIStringBundle **aBundle)
{
  NS_ENSURE_ARG_POINTER(aBundle);
  nsCOMPtr<nsIStringBundleService> bundleService =
    mozilla::services::GetStringBundleService();
  NS_ENSURE_TRUE(bundleService, NS_ERROR_UNEXPECTED);
  nsCOMPtr<nsIStringBundle> bundle;
  bundleService->CreateBundle("chrome://messenger/locale/messenger.properties",
                              getter_AddRefs(bundle));
  NS_IF_ADDREF(*aBundle = bundle);
  return NS_OK;
}

#ifndef MOZ_THUNDERBIRD
nsresult nsMessengerWinIntegration::ShowAlertMessage(const nsString& aAlertTitle,
                                                     const nsString& aAlertText,
                                                     const nsACString& aFolderURI)
{
  nsresult rv;

  // if we are already in the process of showing an alert, don't try to show another....
  if (mAlertInProgress)
    return NS_OK;

  nsCOMPtr<nsIPrefBranch> prefBranch(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  bool showBalloon = false;
  prefBranch->GetBoolPref(SHOW_BALLOON_PREF, &showBalloon);
  sBiffIconData.szInfo[0] = '\0';
  if (showBalloon) {
    ::wcsncpy( sBiffIconData.szInfoTitle, aAlertTitle.get(), kMaxBalloonTitle);
    ::wcsncpy( sBiffIconData.szInfo, aAlertText.get(), kMaxBalloonSize);
  }

  bool showAlert = true;
  prefBranch->GetBoolPref(SHOW_ALERT_PREF, &showAlert);

  if (showAlert)
  {
    nsCOMPtr<nsIAlertsService> alertsService (do_GetService(NS_ALERTSERVICE_CONTRACTID, &rv));
    if (NS_SUCCEEDED(rv))
    {
      rv = alertsService->ShowAlertNotification(NS_LITERAL_STRING(NEW_MAIL_ALERT_ICON), aAlertTitle,
                                                aAlertText, true,
                                                NS_ConvertASCIItoUTF16(aFolderURI), this,
                                                EmptyString());
      mAlertInProgress = true;
    }
  }

  if (!showAlert || NS_FAILED(rv)) // go straight to showing the system tray icon.
    AlertFinished();

  return rv;
}
#else
// Opening Thunderbird's new mail alert notification window
// aUserInitiated --> true if we are opening the alert notification in response to a user action
//                    like clicking on the biff icon
nsresult nsMessengerWinIntegration::ShowNewAlertNotification(bool aUserInitiated, const nsString& aAlertTitle, const nsString& aAlertText)
{
  nsresult rv;

  // if we are already in the process of showing an alert, don't try to show another....
  if (mAlertInProgress)
    return NS_OK;

  nsCOMPtr<nsIPrefBranch> prefBranch(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  bool showBalloon = false;
  prefBranch->GetBoolPref(SHOW_BALLOON_PREF, &showBalloon);
  sBiffIconData.szInfo[0] = '\0';
  if (showBalloon) {
    ::wcsncpy( sBiffIconData.szInfoTitle, aAlertTitle.get(), kMaxBalloonTitle);
    ::wcsncpy( sBiffIconData.szInfo, aAlertText.get(), kMaxBalloonSize);
  }

  bool showAlert = true;

  if (prefBranch)
    prefBranch->GetBoolPref(SHOW_ALERT_PREF, &showAlert);

  // check if we are allowed to show a notification
  if (showAlert && mSHQueryUserNotificationState) {
    MOZ_QUERY_USER_NOTIFICATION_STATE qstate;    
    if (SUCCEEDED(mSHQueryUserNotificationState(&qstate))) {
      if (qstate != QUNS_ACCEPTS_NOTIFICATIONS) {
        showAlert = false;
      }
    }
  }

  if (showAlert)
  {
    nsCOMPtr<nsISupportsArray> argsArray;
    rv = NS_NewISupportsArray(getter_AddRefs(argsArray));
    NS_ENSURE_SUCCESS(rv, rv);

    // pass in the array of folders with unread messages
    nsCOMPtr<nsISupportsInterfacePointer> ifptr = do_CreateInstance(NS_SUPPORTS_INTERFACE_POINTER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    ifptr->SetData(mFoldersWithNewMail);
    ifptr->SetDataIID(&NS_GET_IID(nsISupportsArray));
    rv = argsArray->AppendElement(ifptr);
    NS_ENSURE_SUCCESS(rv, rv);

    // pass in the observer
    ifptr = do_CreateInstance(NS_SUPPORTS_INTERFACE_POINTER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr <nsISupports> supports = do_QueryInterface(static_cast<nsIMessengerOSIntegration*>(this));
    ifptr->SetData(supports);
    ifptr->SetDataIID(&NS_GET_IID(nsIObserver));
    rv = argsArray->AppendElement(ifptr);
    NS_ENSURE_SUCCESS(rv, rv);

    // pass in the animation flag
    nsCOMPtr<nsISupportsPRBool> scriptableUserInitiated (do_CreateInstance(NS_SUPPORTS_PRBOOL_CONTRACTID, &rv));
    NS_ENSURE_SUCCESS(rv, rv);
    scriptableUserInitiated->SetData(aUserInitiated);
    rv = argsArray->AppendElement(scriptableUserInitiated);
    NS_ENSURE_SUCCESS(rv, rv);

    // pass in the alert origin
    nsCOMPtr<nsISupportsPRUint8> scriptableOrigin (do_CreateInstance(NS_SUPPORTS_PRUINT8_CONTRACTID));
    NS_ENSURE_TRUE(scriptableOrigin, NS_ERROR_FAILURE);
    scriptableOrigin->SetData(0);
    int32_t origin = LookAndFeel::GetInt(LookAndFeel::eIntID_AlertNotificationOrigin);
    if (origin && origin >= 0 && origin <= 7)
      scriptableOrigin->SetData(origin);

    rv = argsArray->AppendElement(scriptableOrigin);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService(NS_WINDOWWATCHER_CONTRACTID));
    nsCOMPtr<nsIDOMWindow> newWindow;
    rv = wwatch->OpenWindow(0, ALERT_CHROME_URL, "_blank",
                "chrome,dialog=yes,titlebar=no,popup=yes", argsArray,
                 getter_AddRefs(newWindow));

    mAlertInProgress = true;
  }

  // if the user has turned off the mail alert, or  openWindow generated an error,
  // then go straight to the system tray.
  if (!showAlert || NS_FAILED(rv))
    AlertFinished();

  return rv;
}
#endif

nsresult nsMessengerWinIntegration::AlertFinished()
{
  // okay, we are done showing the alert
  // now put an icon in the system tray, if allowed
  bool showTrayIcon = !mSuppressBiffIcon;
  if (showTrayIcon)
  {
    nsCOMPtr<nsIPrefBranch> prefBranch(do_GetService(NS_PREFSERVICE_CONTRACTID));
    if (prefBranch)
      prefBranch->GetBoolPref(SHOW_TRAY_ICON_PREF, &showTrayIcon);
  }
  if (showTrayIcon || sBiffIconData.szInfo[0])
  {
    GenericShellNotify(NIM_ADD);
    mBiffIconVisible = true;
  }
  mSuppressBiffIcon = false;
  mAlertInProgress = false;
  return NS_OK;
}

nsresult nsMessengerWinIntegration::AlertClicked()
{
#ifdef MOZ_THUNDERBIRD
  nsresult rv;
  nsCOMPtr<nsIMsgMailSession> mailSession = do_GetService(NS_MSGMAILSESSION_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);
  nsCOMPtr<nsIMsgWindow> topMostMsgWindow;
  rv = mailSession->GetTopmostMsgWindow(getter_AddRefs(topMostMsgWindow));
  if (topMostMsgWindow)
  {
    nsCOMPtr<nsIDOMWindow> domWindow;
    topMostMsgWindow->GetDomWindow(getter_AddRefs(domWindow));
    if (domWindow)
    {
      activateWindow(domWindow);
      return NS_OK;
    }
  }
#endif
  // make sure we don't insert the icon in the system tray since the user clicked on the alert.
  mSuppressBiffIcon = true;
  nsCString folderURI;
  GetFirstFolderWithNewMail(folderURI);
  openMailWindow(folderURI);
  return NS_OK;
}

NS_IMETHODIMP
nsMessengerWinIntegration::Observe(nsISupports* aSubject, const char* aTopic, const PRUnichar* aData)
{
  if (strcmp(aTopic, "alertfinished") == 0)
      return AlertFinished();

  if (strcmp(aTopic, "alertclickcallback") == 0)
      return AlertClicked();

  return NS_OK;
}

static void EscapeAmpersands(nsString& aToolTip)
{
  // First, check to see whether we have any ampersands.
  int32_t pos = aToolTip.FindChar('&');
  if (pos == kNotFound)
    return;

  // Next, see if we only have bare ampersands.
  pos = MsgFind(aToolTip, "&&", false, pos);

  // Windows tooltip code removes one ampersand from each run,
  // then collapses pairs of amperands. This means that in the easy case,
  // we need to replace each ampersand with three.
  MsgReplaceSubstring(aToolTip, NS_LITERAL_STRING("&"), NS_LITERAL_STRING("&&&"));
  if (pos == kNotFound)
    return;

  // We inserted too many ampersands. Remove some.
  for (;;) {
    pos = MsgFind(aToolTip, "&&&&&&", false, pos);
    if (pos == kNotFound)
      return;

    aToolTip.Cut(pos, 1);
    pos += 2;
  }
}

void nsMessengerWinIntegration::FillToolTipInfo()
{
  // iterate over all the folders in mFoldersWithNewMail
  nsString accountName;
  nsCString hostName;
  nsString toolTipLine;
  nsAutoString toolTipText;
  nsAutoString animatedAlertText;
  nsCOMPtr<nsIMsgFolder> folder;
  nsCOMPtr<nsIWeakReference> weakReference;
  int32_t numNewMessages = 0;

  uint32_t count = 0;
  mFoldersWithNewMail->Count(&count);

  for (uint32_t index = 0; index < count; index++)
  {
    weakReference = do_QueryElementAt(mFoldersWithNewMail, index);
    folder = do_QueryReferent(weakReference);
    if (folder)
    {
      folder->GetPrettiestName(accountName);

      numNewMessages = 0;
      folder->GetNumNewMessages(true, &numNewMessages);
      nsCOMPtr<nsIStringBundle> bundle;
      GetStringBundle(getter_AddRefs(bundle));
      if (bundle)
      {
        nsAutoString numNewMsgsText;
        numNewMsgsText.AppendInt(numNewMessages);

        const PRUnichar *formatStrings[] =
        {
          numNewMsgsText.get(),
        };

        nsString finalText;
        if (numNewMessages == 1)
          bundle->FormatStringFromName(NS_LITERAL_STRING("biffNotification_message").get(), formatStrings, 1, getter_Copies(finalText));
        else
          bundle->FormatStringFromName(NS_LITERAL_STRING("biffNotification_messages").get(), formatStrings, 1, getter_Copies(finalText));

        // the alert message is special...we actually only want to show the first account with
        // new mail in the alert.
        if (animatedAlertText.IsEmpty()) // if we haven't filled in the animated alert text yet
          animatedAlertText = finalText;

        toolTipLine.Append(accountName);
        toolTipLine.Append(' ');
        toolTipLine.Append(finalText);
        EscapeAmpersands(toolTipLine);

        // only add this new string if it will fit without truncation....
        if (toolTipLine.Length() + toolTipText.Length() <= kMaxTooltipSize)
          toolTipText.Append(toolTipLine);

        // clear out the tooltip line for the next folder
        toolTipLine.Assign('\n');
      } // if we got a bundle
    } // if we got a folder
  } // for each folder

  ::wcsncpy( sBiffIconData.szTip, toolTipText.get(), kMaxTooltipSize);

  if (!mBiffIconVisible)
  {
#ifndef MOZ_THUNDERBIRD
    ShowAlertMessage(accountName, animatedAlertText, EmptyCString());
#else
    ShowNewAlertNotification(false, accountName, animatedAlertText);
#endif
  }
  else
   GenericShellNotify( NIM_MODIFY);
}

// get the first top level folder which we know has new mail, then enumerate over all the subfolders
// looking for the first real folder with new mail. Return the folderURI for that folder.
nsresult nsMessengerWinIntegration::GetFirstFolderWithNewMail(nsACString& aFolderURI)
{
  nsresult rv;
  NS_ENSURE_TRUE(mFoldersWithNewMail, NS_ERROR_FAILURE);

  nsCOMPtr<nsIMsgFolder> folder;
  nsCOMPtr<nsIWeakReference> weakReference;
  int32_t numNewMessages = 0;

  uint32_t count = 0;
  mFoldersWithNewMail->Count(&count);

  if (!count)  // kick out if we don't have any folders with new mail
    return NS_OK;

  weakReference = do_QueryElementAt(mFoldersWithNewMail, 0);
  folder = do_QueryReferent(weakReference);

  if (folder)
  {
    nsCOMPtr<nsIMsgFolder> msgFolder;
    // enumerate over the folders under this root folder till we find one with new mail....
    nsCOMPtr<nsISupportsArray> allFolders;
    NS_NewISupportsArray(getter_AddRefs(allFolders));
    rv = folder->ListDescendents(allFolders);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIEnumerator> enumerator;
    allFolders->Enumerate(getter_AddRefs(enumerator));
    if (enumerator)
    {
      nsCOMPtr<nsISupports> supports;
      nsresult more = enumerator->First();
      while (NS_SUCCEEDED(more))
      {
        rv = enumerator->CurrentItem(getter_AddRefs(supports));
        if (supports)
        {
          msgFolder = do_QueryInterface(supports, &rv);
          if (msgFolder)
          {
            numNewMessages = 0;
            msgFolder->GetNumNewMessages(false, &numNewMessages);
            if (numNewMessages)
              break; // kick out of the while loop
            more = enumerator->Next();
          }
        } // if we have a folder
      }  // if we have more potential folders to enumerate
    }  // if enumerator

    if (msgFolder)
      msgFolder->GetURI(aFolderURI);
  }

  return NS_OK;
}

void nsMessengerWinIntegration::DestroyBiffIcon()
{
  GenericShellNotify(NIM_DELETE);
  // Don't call DestroyIcon().  see http://bugzilla.mozilla.org/show_bug.cgi?id=134745
}

void nsMessengerWinIntegration::GenericShellNotify(DWORD aMessage)
{
  ::Shell_NotifyIconW( aMessage, &sBiffIconData );
}

NS_IMETHODIMP
nsMessengerWinIntegration::OnItemPropertyFlagChanged(nsIMsgDBHdr *item, nsIAtom *property, uint32_t oldFlag, uint32_t newFlag)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMessengerWinIntegration::OnItemAdded(nsIMsgFolder *, nsISupports *)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMessengerWinIntegration::OnItemBoolPropertyChanged(nsIMsgFolder *aItem,
                                                         nsIAtom *aProperty,
                                                         bool aOldValue,
                                                         bool aNewValue)
{
  if (aProperty == mDefaultServerAtom) {
    nsresult rv;

    // this property changes multiple times
    // on account deletion or when the user changes their
    // default account.  ResetCurrent() will set
    // mInboxURI to null, so we use that
    // to prevent us from attempting to remove
    // something from the registry that has already been removed
    if (!mInboxURI.IsEmpty() && !mEmail.IsEmpty()) {
      rv = RemoveCurrentFromRegistry();
      NS_ENSURE_SUCCESS(rv,rv);
    }

    // reset so we'll go get the new default server next time
    rv = ResetCurrent();
    NS_ENSURE_SUCCESS(rv,rv);

    rv = UpdateUnreadCount();
    NS_ENSURE_SUCCESS(rv,rv);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsMessengerWinIntegration::OnItemEvent(nsIMsgFolder *, nsIAtom *)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMessengerWinIntegration::OnItemIntPropertyChanged(nsIMsgFolder *aItem, nsIAtom *aProperty, int32_t aOldValue, int32_t aNewValue)
{
  // if we got new mail show a icon in the system tray
  if (mBiffStateAtom == aProperty && mFoldersWithNewMail)
  {
    nsCOMPtr<nsIWeakReference> weakFolder = do_GetWeakReference(aItem);
    int32_t indexInNewArray = mFoldersWithNewMail->IndexOf(weakFolder);

    if (!mBiffIconInitialized)
      InitializeBiffStatusIcon();

    if (aNewValue == nsIMsgFolder::nsMsgBiffState_NewMail)
    {
      // if the icon is not already visible, only show a system tray icon iff
      // we are performing biff (as opposed to the user getting new mail)
      if (!mBiffIconVisible)
      {
        bool performingBiff = false;
        nsCOMPtr<nsIMsgIncomingServer> server;
        aItem->GetServer(getter_AddRefs(server));
        if (server)
          server->GetPerformingBiff(&performingBiff);
        if (!performingBiff)
          return NS_OK; // kick out right now...
      }
      if (indexInNewArray == -1)
        mFoldersWithNewMail->InsertElementAt(weakFolder, 0);
      // now regenerate the tooltip
      FillToolTipInfo();
    }
    else if (aNewValue == nsIMsgFolder::nsMsgBiffState_NoMail)
    {
      // we are always going to remove the icon whenever we get our first no mail
      // notification.

      // avoid a race condition where we are told to remove the icon before we've actually
      // added it to the system tray. This happens when the user reads a new message before
      // the animated alert has gone away.
      if (mAlertInProgress)
        mSuppressBiffIcon = true;

      if (indexInNewArray != -1)
        mFoldersWithNewMail->RemoveElementAt(indexInNewArray);
      if (mBiffIconVisible)
      {
        mBiffIconVisible = false;
        GenericShellNotify(NIM_DELETE);
      }
    }
  } // if the biff property changed

  if (aProperty == mTotalUnreadMessagesAtom) {
    nsCString itemURI;
    nsresult rv;
    rv = aItem->GetURI(itemURI);
    NS_ENSURE_SUCCESS(rv,rv);

    if (mInboxURI.Equals(itemURI))
      mCurrentUnreadCount = aNewValue;

    // If the timer isn't running yet, then we immediately update the
    // registry and then start a one-shot timer. If the Unread counter
    // has toggled zero / nonzero, we also update immediately.
    // Otherwise, if the timer is running, defer the update. This means
    // that all counter updates that occur within the timer interval are
    // batched into a single registry update, to avoid hitting the
    // registry too frequently. We also do a final update on shutdown,
    // regardless of the timer.
    if (!mUnreadTimerActive ||
         (!mCurrentUnreadCount && mLastUnreadCountWrittenToRegistry) ||
         (mCurrentUnreadCount && mLastUnreadCountWrittenToRegistry < 1)) {
      rv = UpdateUnreadCount();
      NS_ENSURE_SUCCESS(rv,rv);
      // If timer wasn't running, start it.
      if (!mUnreadTimerActive)
        rv = SetupUnreadCountUpdateTimer();
    }
  }
  return NS_OK;
}

void
nsMessengerWinIntegration::OnUnreadCountUpdateTimer(nsITimer *timer, void *osIntegration)
{
  nsMessengerWinIntegration *winIntegration = (nsMessengerWinIntegration*)osIntegration;

  winIntegration->mUnreadTimerActive = false;
  nsresult rv = winIntegration->UpdateUnreadCount();
  NS_ASSERTION(NS_SUCCEEDED(rv), "updating unread count failed");
}

nsresult
nsMessengerWinIntegration::RemoveCurrentFromRegistry()
{
  // If Windows XP, open the registry and get rid of old account registry entries
  // If there is a email prefix, get it and use it to build the registry key.
  // Otherwise, just the email address will be the registry key.
  nsAutoString currentUnreadMailCountKey;
  if (!mEmailPrefix.IsEmpty()) {
    currentUnreadMailCountKey.Assign(mEmailPrefix);
    currentUnreadMailCountKey.Append(NS_ConvertASCIItoUTF16(mEmail));
  }
  else
    CopyASCIItoUTF16(mEmail, currentUnreadMailCountKey);

  WCHAR registryUnreadMailCountKey[_MAX_PATH] = {0};
  // Enumerate through registry entries to delete the key matching
  // currentUnreadMailCountKey
  int index = 0;
  while (SUCCEEDED(SHEnumerateUnreadMailAccountsW(HKEY_CURRENT_USER,
                                                  index,
                                                  registryUnreadMailCountKey,
                                                  sizeof(registryUnreadMailCountKey))))
  {
    if (wcscmp(registryUnreadMailCountKey, currentUnreadMailCountKey.get())==0) {
      nsAutoString deleteKey;
      deleteKey.Assign(NS_LITERAL_STRING(UNREADMAILNODEKEY).get());
      deleteKey.Append(currentUnreadMailCountKey.get());

      if (!deleteKey.IsEmpty()) {
        // delete this key and berak out of the loop
        RegDeleteKey(HKEY_CURRENT_USER,
                     NS_ConvertUTF16toUTF8(deleteKey).get());
        break;
      }
      else {
        index++;
      }
    }
    else {
      index++;
    }
  }
  return NS_OK;
}

nsresult
nsMessengerWinIntegration::UpdateRegistryWithCurrent()
{
  if (mInboxURI.IsEmpty() || mEmail.IsEmpty())
    return NS_OK;

  // only update the registry if the count has changed
  // and if the unread count is valid
  if ((mCurrentUnreadCount < 0) || (mCurrentUnreadCount == mLastUnreadCountWrittenToRegistry))
    return NS_OK;

  // commandliner has to be built in the form of statement
  // which can be open the mailer app to the default user account
  // For given profile 'foo', commandliner will be built as
  // ""<absolute path to application>" -p foo -mail" where absolute
  // path to application is extracted from mAppName
  nsAutoString commandLinerForAppLaunch;
  commandLinerForAppLaunch.Assign(NS_LITERAL_STRING(DOUBLE_QUOTE));
  commandLinerForAppLaunch.Append(mAppName);
  commandLinerForAppLaunch.Append(NS_LITERAL_STRING(DOUBLE_QUOTE));
  commandLinerForAppLaunch.Append(NS_LITERAL_STRING(PROFILE_COMMANDLINE_ARG));
  commandLinerForAppLaunch.Append(NS_LITERAL_STRING(DOUBLE_QUOTE));
  commandLinerForAppLaunch.Append(mProfilePath);
  commandLinerForAppLaunch.Append(NS_LITERAL_STRING(DOUBLE_QUOTE));
  commandLinerForAppLaunch.Append(NS_LITERAL_STRING(MAIL_COMMANDLINE_ARG));

  if (!commandLinerForAppLaunch.IsEmpty())
  {
    nsAutoString pBuffer;

    if (!mEmailPrefix.IsEmpty()) {
      pBuffer.Assign(mEmailPrefix);
      pBuffer.Append(NS_ConvertASCIItoUTF16(mEmail));
    }
    else
      CopyASCIItoUTF16(mEmail, pBuffer);

    // Write the info into the registry
    HRESULT hr = SHSetUnreadMailCountW(pBuffer.get(),
                                       mCurrentUnreadCount,
                                       commandLinerForAppLaunch.get());
  }

  // do this last
  mLastUnreadCountWrittenToRegistry = mCurrentUnreadCount;

  return NS_OK;
}

nsresult
nsMessengerWinIntegration::SetupInbox()
{
  nsresult rv;

  // get default account
  nsCOMPtr <nsIMsgAccountManager> accountManager =
    do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr <nsIMsgAccount> account;
  rv = accountManager->GetDefaultAccount(getter_AddRefs(account));
  if (NS_FAILED(rv)) {
    // this can happen if we launch mail on a new profile
    // we don't have a default account yet
    mDefaultAccountMightHaveAnInbox = false;
    return NS_OK;
  }

  // get incoming server
  nsCOMPtr <nsIMsgIncomingServer> server;
  rv = account->GetIncomingServer(getter_AddRefs(server));
  NS_ENSURE_SUCCESS(rv,rv);
  if (!server)
    return NS_ERROR_FAILURE;

  nsCString type;
  rv = server->GetType(type);
  NS_ENSURE_SUCCESS(rv,rv);

  // we only care about imap and pop3
  if (type.EqualsLiteral("imap") || type.EqualsLiteral("pop3")) {
    // imap and pop3 account should have an Inbox
    mDefaultAccountMightHaveAnInbox = true;

    mEmailPrefix.Truncate();

    // Get user's email address
    nsCOMPtr<nsIMsgIdentity> identity;
    rv = account->GetDefaultIdentity(getter_AddRefs(identity));
    NS_ENSURE_SUCCESS(rv,rv);

    if (!identity)
      return NS_ERROR_FAILURE;

    rv = identity->GetEmail(mEmail);
    NS_ENSURE_SUCCESS(rv,rv);

    nsCOMPtr<nsIMsgFolder> rootMsgFolder;
    rv = server->GetRootMsgFolder(getter_AddRefs(rootMsgFolder));
    NS_ENSURE_SUCCESS(rv,rv);

    if (!rootMsgFolder)
      return NS_ERROR_FAILURE;

    nsCOMPtr <nsIMsgFolder> inboxFolder;
    rootMsgFolder->GetFolderWithFlags(nsMsgFolderFlags::Inbox,
                                      getter_AddRefs(inboxFolder));
    NS_ENSURE_TRUE(inboxFolder, NS_ERROR_FAILURE);

    rv = inboxFolder->GetURI(mInboxURI);
    NS_ENSURE_SUCCESS(rv,rv);

    rv = inboxFolder->GetNumUnread(false, &mCurrentUnreadCount);
    NS_ENSURE_SUCCESS(rv,rv);
  }
  else {
    // the default account is valid, but it's not something
    // that we expect to have an inbox.  (local folders, news accounts)
    // set this flag to avoid calling SetupInbox() every time
    // the timer goes off.
    mDefaultAccountMightHaveAnInbox = false;
  }

  return NS_OK;
}

nsresult
nsMessengerWinIntegration::UpdateUnreadCount()
{
  nsresult rv;

  if (mDefaultAccountMightHaveAnInbox && mInboxURI.IsEmpty()) {
    rv = SetupInbox();
    NS_ENSURE_SUCCESS(rv,rv);
  }

  return UpdateRegistryWithCurrent();
}

nsresult
nsMessengerWinIntegration::SetupUnreadCountUpdateTimer()
{
  mUnreadTimerActive = true;
  if (mUnreadCountUpdateTimer)
    mUnreadCountUpdateTimer->Cancel();
  else
    mUnreadCountUpdateTimer = do_CreateInstance("@mozilla.org/timer;1");

  mUnreadCountUpdateTimer->InitWithFuncCallback(OnUnreadCountUpdateTimer,
    (void *)this, UNREAD_UPDATE_INTERVAL, nsITimer::TYPE_ONE_SHOT);

  return NS_OK;
}
