/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ContentParent_h
#define mozilla_dom_ContentParent_h

#include "base/waitable_event_watcher.h"

#include "mozilla/dom/PContentParent.h"
#include "mozilla/dom/PMemoryReportRequestParent.h"
#include "mozilla/ipc/GeckoChildProcessHost.h"
#include "mozilla/dom/ipc/Blob.h"
#include "mozilla/Attributes.h"

#include "nsIObserver.h"
#include "nsIThreadInternal.h"
#include "nsNetUtil.h"
#include "nsIPermissionManager.h"
#include "nsIDOMGeoPositionCallback.h"
#include "nsIMemoryReporter.h"
#include "nsCOMArray.h"
#include "nsDataHashtable.h"
#include "nsHashKeys.h"

class mozIApplication;
class nsFrameMessageManager;
class nsIDOMBlob;

namespace mozilla {

namespace ipc {
class OptionalURIParams;
class URIParams;
class TestShellParent;
} // namespace ipc

namespace layers {
class PCompositorParent;
} // namespace layers

namespace dom {

class TabParent;
class PStorageParent;
class ClonedMessageData;

class ContentParent : public PContentParent
                    , public nsIObserver
                    , public nsIThreadObserver
                    , public nsIDOMGeoPositionCallback
{
    typedef mozilla::ipc::GeckoChildProcessHost GeckoChildProcessHost;
    typedef mozilla::ipc::OptionalURIParams OptionalURIParams;
    typedef mozilla::ipc::TestShellParent TestShellParent;
    typedef mozilla::ipc::URIParams URIParams;
    typedef mozilla::layers::PCompositorParent PCompositorParent;
    typedef mozilla::dom::ClonedMessageData ClonedMessageData;

public:
    /**
     * Start up the content-process machinery.  This might include
     * scheduling pre-launch tasks.
     */
    static void StartUp();
    /** Shut down the content-process machinery. */
    static void ShutDown();

    static ContentParent* GetNewOrUsed(bool aForBrowserElement = false);

    /**
     * Get or create a content process for the given app descriptor,
     * which may be null.  This function will assign processes to app
     * or non-app browsers by internal heuristics.
     *
     * Currently apps are given their own process, and browser tabs
     * share processes.
     */
    static TabParent* CreateBrowser(mozIApplication* aApp,
                                    bool aIsBrowserFrame);

    static void GetAll(nsTArray<ContentParent*>& aArray);

    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER
    NS_DECL_NSITHREADOBSERVER
    NS_DECL_NSIDOMGEOPOSITIONCALLBACK

    /** Notify that a tab was destroyed during normal operation. */
    void NotifyTabDestroyed(PBrowserParent* aTab);

    TestShellParent* CreateTestShell();
    bool DestroyTestShell(TestShellParent* aTestShell);
    TestShellParent* GetTestShellSingleton();

    void ReportChildAlreadyBlocked();
    bool RequestRunToCompletion();

    bool IsAlive();
    bool IsForApp();

    void SetChildMemoryReporters(const InfallibleTArray<MemoryReport>& report);

    GeckoChildProcessHost* Process() {
        return mSubprocess;
    }

    bool NeedsPermissionsUpdate() {
        return mSendPermissionUpdates;
    }

    BlobParent* GetOrCreateActorForBlob(nsIDOMBlob* aBlob);

protected:
    void OnChannelConnected(int32 pid);
    virtual void ActorDestroy(ActorDestroyReason why);

private:
    static nsDataHashtable<nsStringHashKey, ContentParent*> *gAppContentParents;
    static nsTArray<ContentParent*>* gNonAppContentParents;
    static nsTArray<ContentParent*>* gPrivateContent;

    static void PreallocateAppProcess();
    static void DelayedPreallocateAppProcess();
    static void ScheduleDelayedPreallocateAppProcess();
    static already_AddRefed<ContentParent> MaybeTakePreallocatedAppProcess();

    // Hide the raw constructor methods since we don't want client code
    // using them.
    using PContentParent::SendPBrowserConstructor;
    using PContentParent::SendPTestShellConstructor;

    ContentParent(const nsAString& aAppManifestURL, bool aIsForBrowser);
    virtual ~ContentParent();

    void Init();

    // Transform a pre-allocated app process into a "real" app
    // process, for the specified manifest URL.
    void SetManifestFromPreallocated(const nsAString& aAppManifestURL);

    /**
     * Mark this ContentParent as dead for the purposes of Get*().
     * This method is idempotent.
     */
    void MarkAsDead();

    /**
     * Exit the subprocess and vamoose.  After this call IsAlive()
     * will return false and this ContentParent will not be returned
     * by the Get*() funtions.  However, the shutdown sequence itself
     * may be asynchronous.
     */
    void ShutDownProcess();

    PCompositorParent* AllocPCompositor(mozilla::ipc::Transport* aTransport,
                                        base::ProcessId aOtherProcess) MOZ_OVERRIDE;

    virtual PBrowserParent* AllocPBrowser(const uint32_t& aChromeFlags,
                                          const bool& aIsBrowserElement,
                                          const AppId& aApp);
    virtual bool DeallocPBrowser(PBrowserParent* frame);

    virtual PDeviceStorageRequestParent* AllocPDeviceStorageRequest(const DeviceStorageParams&);
    virtual bool DeallocPDeviceStorageRequest(PDeviceStorageRequestParent*);

    virtual PBlobParent* AllocPBlob(const BlobConstructorParams& aParams);
    virtual bool DeallocPBlob(PBlobParent*);

    virtual PCrashReporterParent* AllocPCrashReporter(const NativeThreadId& tid,
                                                      const uint32_t& processType);
    virtual bool DeallocPCrashReporter(PCrashReporterParent* crashreporter);
    virtual bool RecvPCrashReporterConstructor(PCrashReporterParent* actor,
                                               const NativeThreadId& tid,
                                               const uint32_t& processType);

    virtual PHalParent* AllocPHal() MOZ_OVERRIDE;
    virtual bool DeallocPHal(PHalParent*) MOZ_OVERRIDE;

    virtual PIndexedDBParent* AllocPIndexedDB();

    virtual bool DeallocPIndexedDB(PIndexedDBParent* aActor);

    virtual bool
    RecvPIndexedDBConstructor(PIndexedDBParent* aActor);

    virtual PMemoryReportRequestParent* AllocPMemoryReportRequest();
    virtual bool DeallocPMemoryReportRequest(PMemoryReportRequestParent* actor);

    virtual PTestShellParent* AllocPTestShell();
    virtual bool DeallocPTestShell(PTestShellParent* shell);

    virtual PAudioParent* AllocPAudio(const int32_t&,
                                     const int32_t&,
                                     const int32_t&);
    virtual bool DeallocPAudio(PAudioParent*);

    virtual PNeckoParent* AllocPNecko();
    virtual bool DeallocPNecko(PNeckoParent* necko);

    virtual PExternalHelperAppParent* AllocPExternalHelperApp(
            const OptionalURIParams& aUri,
            const nsCString& aMimeContentType,
            const nsCString& aContentDisposition,
            const bool& aForceSave,
            const int64_t& aContentLength,
            const OptionalURIParams& aReferrer);
    virtual bool DeallocPExternalHelperApp(PExternalHelperAppParent* aService);

    virtual PSmsParent* AllocPSms();
    virtual bool DeallocPSms(PSmsParent*);

    virtual PStorageParent* AllocPStorage(const StorageConstructData& aData);
    virtual bool DeallocPStorage(PStorageParent* aActor);

    virtual bool RecvReadPrefsArray(InfallibleTArray<PrefSetting>* aPrefs);
    virtual bool RecvReadFontList(InfallibleTArray<FontListEntry>* retValue);

    virtual bool RecvReadPermissions(InfallibleTArray<IPC::Permission>* aPermissions);

    virtual bool RecvSetClipboardText(const nsString& text, const bool& isPrivateData, const int32_t& whichClipboard);
    virtual bool RecvGetClipboardText(const int32_t& whichClipboard, nsString* text);
    virtual bool RecvEmptyClipboard();
    virtual bool RecvClipboardHasText(bool* hasText);

    virtual bool RecvGetSystemColors(const uint32_t& colorsCount, InfallibleTArray<uint32_t>* colors);
    virtual bool RecvGetIconForExtension(const nsCString& aFileExt, const uint32_t& aIconSize, InfallibleTArray<uint8_t>* bits);
    virtual bool RecvGetShowPasswordSetting(bool* showPassword);

    virtual bool RecvStartVisitedQuery(const URIParams& uri);

    virtual bool RecvVisitURI(const URIParams& uri,
                              const OptionalURIParams& referrer,
                              const uint32_t& flags);

    virtual bool RecvSetURITitle(const URIParams& uri,
                                 const nsString& title);
    
    virtual bool RecvShowFilePicker(const int16_t& mode,
                                    const int16_t& selectedType,
                                    const bool& addToRecentDocs,
                                    const nsString& title,
                                    const nsString& defaultFile,
                                    const nsString& defaultExtension,
                                    const InfallibleTArray<nsString>& filters,
                                    const InfallibleTArray<nsString>& filterNames,
                                    InfallibleTArray<nsString>* files,
                                    int16_t* retValue,
                                    nsresult* result);
 
    virtual bool RecvShowAlertNotification(const nsString& aImageUrl, const nsString& aTitle,
                                           const nsString& aText, const bool& aTextClickable,
                                           const nsString& aCookie, const nsString& aName);

    virtual bool RecvLoadURIExternal(const URIParams& uri);

    virtual bool RecvSyncMessage(const nsString& aMsg,
                                 const ClonedMessageData& aData,
                                 InfallibleTArray<nsString>* aRetvals);
    virtual bool RecvAsyncMessage(const nsString& aMsg,
                                  const ClonedMessageData& aData);

    virtual bool RecvAddGeolocationListener();
    virtual bool RecvRemoveGeolocationListener();

    virtual bool RecvConsoleMessage(const nsString& aMessage);
    virtual bool RecvScriptError(const nsString& aMessage,
                                 const nsString& aSourceName,
                                 const nsString& aSourceLine,
                                 const uint32_t& aLineNumber,
                                 const uint32_t& aColNumber,
                                 const uint32_t& aFlags,
                                 const nsCString& aCategory);

    virtual bool RecvPrivateDocShellsExist(const bool& aExist);

    virtual void ProcessingError(Result what) MOZ_OVERRIDE;

    GeckoChildProcessHost* mSubprocess;

    int32_t mGeolocationWatchID;
    int mRunToCompletionDepth;
    bool mShouldCallUnblockChild;

    // This is a cache of all of the memory reporters
    // registered in the child process.  To update this, one
    // can broadcast the topic "child-memory-reporter-request" using
    // the nsIObserverService.
    nsCOMArray<nsIMemoryReporter> mMemoryReporters;

    bool mIsAlive;
    bool mSendPermissionUpdates;

    const nsString mAppManifestURL;
    nsRefPtr<nsFrameMessageManager> mMessageManager;

    friend class CrashReporterParent;
};

} // namespace dom
} // namespace mozilla

#endif
