/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DeviceStorageRequestParent.h"
#include "nsDOMFile.h"
#include "nsIMIMEService.h"
#include "nsCExternalHandlerService.h"
#include "mozilla/unused.h"
#include "mozilla/dom/ipc/Blob.h"
#include "ContentParent.h"
#include "nsProxyRelease.h"

namespace mozilla {
namespace dom {
namespace devicestorage {

DeviceStorageRequestParent::DeviceStorageRequestParent(const DeviceStorageParams& aParams)
{
  MOZ_COUNT_CTOR(DeviceStorageRequestParent);

  switch (aParams.type()) {
    case DeviceStorageParams::TDeviceStorageAddParams:
    {
      DeviceStorageAddParams p = aParams;

      nsCOMPtr<nsIFile> f;
      NS_NewLocalFile(p.fullpath(), false, getter_AddRefs(f));

      nsRefPtr<DeviceStorageFile> dsf = new DeviceStorageFile(f);

      BlobParent* bp = static_cast<BlobParent*>(p.blobParent());
      nsCOMPtr<nsIDOMBlob> blob = bp->GetBlob();

      nsCOMPtr<nsIInputStream> stream;
      blob->GetInternalStream(getter_AddRefs(stream));

      nsRefPtr<CancelableRunnable> r = new WriteFileEvent(this, dsf, stream);

      nsCOMPtr<nsIEventTarget> target = do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
      NS_ASSERTION(target, "Must have stream transport service");
      target->Dispatch(r, NS_DISPATCH_NORMAL);
      break;
    }

    case DeviceStorageParams::TDeviceStorageGetParams:
    {
      DeviceStorageGetParams p = aParams;

      nsCOMPtr<nsIFile> f;
      NS_NewLocalFile(p.fullpath(), false, getter_AddRefs(f));

      nsRefPtr<DeviceStorageFile> dsf = new DeviceStorageFile(f);
      dsf->SetPath(p.name());
      nsRefPtr<CancelableRunnable> r = new ReadFileEvent(this, dsf);

      nsCOMPtr<nsIEventTarget> target = do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
      NS_ASSERTION(target, "Must have stream transport service");
      target->Dispatch(r, NS_DISPATCH_NORMAL);
      break;
    }

    case DeviceStorageParams::TDeviceStorageDeleteParams:
    {
      DeviceStorageDeleteParams p = aParams;

      nsCOMPtr<nsIFile> f;
      NS_NewLocalFile(p.fullpath(), false, getter_AddRefs(f));

      nsRefPtr<DeviceStorageFile> dsf = new DeviceStorageFile(f);
      nsRefPtr<CancelableRunnable> r = new DeleteFileEvent(this, dsf);

      nsCOMPtr<nsIEventTarget> target = do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
      NS_ASSERTION(target, "Must have stream transport service");
      target->Dispatch(r, NS_DISPATCH_NORMAL);
      break;
    }

    case DeviceStorageParams::TDeviceStorageStatParams:
    {
      DeviceStorageStatParams p = aParams;

      nsCOMPtr<nsIFile> f;
      NS_NewLocalFile(p.fullpath(), false, getter_AddRefs(f));

      nsRefPtr<DeviceStorageFile> dsf = new DeviceStorageFile(f);
      nsRefPtr<StatFileEvent> r = new StatFileEvent(this, dsf);

      nsCOMPtr<nsIEventTarget> target = do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
      NS_ASSERTION(target, "Must have stream transport service");
      target->Dispatch(r, NS_DISPATCH_NORMAL);
      break;
    }

    case DeviceStorageParams::TDeviceStorageEnumerationParams:
    {
      DeviceStorageEnumerationParams p = aParams;

      nsCOMPtr<nsIFile> f;
      NS_NewLocalFile(p.fullpath(), false, getter_AddRefs(f));

      nsRefPtr<DeviceStorageFile> dsf = new DeviceStorageFile(f);
      nsRefPtr<CancelableRunnable> r = new EnumerateFileEvent(this, dsf, p.since());

      nsCOMPtr<nsIEventTarget> target = do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
      NS_ASSERTION(target, "Must have stream transport service");
      target->Dispatch(r, NS_DISPATCH_NORMAL);
      break;
    }
    default:
    {
      NS_RUNTIMEABORT("not reached");
      break;
    }
  }
}

DeviceStorageRequestParent::~DeviceStorageRequestParent()
{
  MOZ_COUNT_DTOR(DeviceStorageRequestParent);
}

NS_IMPL_THREADSAFE_ADDREF(DeviceStorageRequestParent);
NS_IMPL_THREADSAFE_RELEASE(DeviceStorageRequestParent);

void
DeviceStorageRequestParent::ActorDestroy(ActorDestroyReason)
{
  int32_t count = mRunnables.Length();
  for (int32_t index = 0; index < count; index++) {
    mRunnables[index]->Cancel();
  }
}

DeviceStorageRequestParent::PostErrorEvent::PostErrorEvent(DeviceStorageRequestParent* aParent,
                                                           const char* aError)
  : CancelableRunnable(aParent)
{
  CopyASCIItoUTF16(aError, mError);
}

DeviceStorageRequestParent::PostErrorEvent::~PostErrorEvent() {}

nsresult
DeviceStorageRequestParent::PostErrorEvent::CancelableRun() {
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  ErrorResponse response(mError);
  unused << mParent->Send__delete__(mParent, response);
  return NS_OK;
}


DeviceStorageRequestParent::PostSuccessEvent::PostSuccessEvent(DeviceStorageRequestParent* aParent)
  : CancelableRunnable(aParent)
{
}

DeviceStorageRequestParent::PostSuccessEvent::~PostSuccessEvent() {}

nsresult
DeviceStorageRequestParent::PostSuccessEvent::CancelableRun() {
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  SuccessResponse response;
  unused <<  mParent->Send__delete__(mParent, response);
  return NS_OK;
}

DeviceStorageRequestParent::PostBlobSuccessEvent::PostBlobSuccessEvent(DeviceStorageRequestParent* aParent,
                                                                       DeviceStorageFile* aFile,
                                                                       uint32_t aLength,
                                                                       nsACString& aMimeType)
  : CancelableRunnable(aParent)
  , mLength(aLength)
  , mFile(aFile)
  , mMimeType(aMimeType)
{
}

DeviceStorageRequestParent::PostBlobSuccessEvent::~PostBlobSuccessEvent() {}

nsresult
DeviceStorageRequestParent::PostBlobSuccessEvent::CancelableRun() {
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsString mime;
  CopyASCIItoUTF16(mMimeType, mime);

  nsCOMPtr<nsIDOMBlob> blob = new nsDOMFileFile(mFile->mPath, mime, mLength, mFile->mFile);

  ContentParent* cp = static_cast<ContentParent*>(mParent->Manager());
  BlobParent* actor = cp->GetOrCreateActorForBlob(blob);

  BlobResponse response;
  response.blobParent() = actor;

  unused <<  mParent->Send__delete__(mParent, response);
  return NS_OK;
}

DeviceStorageRequestParent::PostEnumerationSuccessEvent::PostEnumerationSuccessEvent(DeviceStorageRequestParent* aParent,
                                                                                     InfallibleTArray<DeviceStorageFileValue>& aPaths)
  : CancelableRunnable(aParent)
  , mPaths(aPaths)
{
}

DeviceStorageRequestParent::PostEnumerationSuccessEvent::~PostEnumerationSuccessEvent() {}

nsresult
DeviceStorageRequestParent::PostEnumerationSuccessEvent::CancelableRun() {
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  EnumerationResponse response(mPaths);
  unused <<  mParent->Send__delete__(mParent, response);
  return NS_OK;
}

DeviceStorageRequestParent::WriteFileEvent::WriteFileEvent(DeviceStorageRequestParent* aParent,
                                                           DeviceStorageFile* aFile,
                                                           nsIInputStream* aInputStream)
  : CancelableRunnable(aParent)
  , mFile(aFile)
  , mInputStream(aInputStream)
{
}

DeviceStorageRequestParent::WriteFileEvent::~WriteFileEvent()
{
}

nsresult
DeviceStorageRequestParent::WriteFileEvent::CancelableRun()
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  nsRefPtr<nsRunnable> r;

  if (!mInputStream) {
    r = new PostErrorEvent(mParent, POST_ERROR_EVENT_UNKNOWN);
    NS_DispatchToMainThread(r);
    return NS_OK;
  }

  nsresult rv = mFile->Write(mInputStream);

  if (NS_FAILED(rv)) {
    r = new PostErrorEvent(mParent, POST_ERROR_EVENT_UNKNOWN);
  }
  else {
    r = new PostPathResultEvent(mParent, mFile->mPath);
  }

  NS_DispatchToMainThread(r);
  return NS_OK;
}


DeviceStorageRequestParent::DeleteFileEvent::DeleteFileEvent(DeviceStorageRequestParent* aParent,
                                                             DeviceStorageFile* aFile)
  : CancelableRunnable(aParent)
  , mFile(aFile)
{
}

DeviceStorageRequestParent::DeleteFileEvent::~DeleteFileEvent()
{
}

nsresult
DeviceStorageRequestParent::DeleteFileEvent::CancelableRun()
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  mFile->Remove();

  nsRefPtr<nsRunnable> r;

  bool check = false;
  mFile->mFile->Exists(&check);
  if (check) {
    r = new PostErrorEvent(mParent, POST_ERROR_EVENT_UNKNOWN);
  }
  else {
    r = new PostPathResultEvent(mParent, mFile->mPath);
  }

  NS_DispatchToMainThread(r);
  return NS_OK;
}

DeviceStorageRequestParent::StatFileEvent::StatFileEvent(DeviceStorageRequestParent* aParent,
                                                         DeviceStorageFile* aFile)
  : CancelableRunnable(aParent)
  , mFile(aFile)
{
}

DeviceStorageRequestParent::StatFileEvent::~StatFileEvent()
{
}

nsresult
DeviceStorageRequestParent::StatFileEvent::CancelableRun()
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  nsCOMPtr<nsIRunnable> r;
  uint64_t diskUsage = DeviceStorageFile::DirectoryDiskUsage(mFile->mFile);
  int64_t freeSpace;
  nsresult rv = mFile->mFile->GetDiskSpaceAvailable(&freeSpace);
  if (NS_FAILED(rv)) {
    freeSpace = 0;
  }
  
  r = new PostStatResultEvent(mParent, diskUsage, freeSpace);
  NS_DispatchToMainThread(r);
  return NS_OK;
}

DeviceStorageRequestParent::ReadFileEvent::ReadFileEvent(DeviceStorageRequestParent* aParent,
                                                         DeviceStorageFile* aFile)
  : CancelableRunnable(aParent)
  , mFile(aFile)
{
  nsCOMPtr<nsIMIMEService> mimeService = do_GetService(NS_MIMESERVICE_CONTRACTID);
  if (mimeService) {
    nsresult rv = mimeService->GetTypeFromFile(mFile->mFile, mMimeType);
    if (NS_FAILED(rv)) {
      mMimeType.Truncate();
    }
  }
}

DeviceStorageRequestParent::ReadFileEvent::~ReadFileEvent()
{
}

nsresult
DeviceStorageRequestParent::ReadFileEvent::CancelableRun()
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  nsCOMPtr<nsIRunnable> r;
  bool check = false;
  mFile->mFile->Exists(&check);

  if (!check) {
    r = new PostErrorEvent(mParent, POST_ERROR_EVENT_FILE_DOES_NOT_EXIST);
    NS_DispatchToMainThread(r);
    return NS_OK;
  }

  int64_t fileSize;
  nsresult rv = mFile->mFile->GetFileSize(&fileSize);
  if (NS_FAILED(rv)) {
    r = new PostErrorEvent(mParent, POST_ERROR_EVENT_UNKNOWN);
    NS_DispatchToMainThread(r);
    return NS_OK;
  }

  r = new PostBlobSuccessEvent(mParent, mFile, fileSize, mMimeType);
  NS_DispatchToMainThread(r);
  return NS_OK;
}

DeviceStorageRequestParent::EnumerateFileEvent::EnumerateFileEvent(DeviceStorageRequestParent* aParent,
                                                                   DeviceStorageFile* aFile,
                                                                   uint64_t aSince)
  : CancelableRunnable(aParent)
  , mFile(aFile)
  , mSince(aSince)
{
}

DeviceStorageRequestParent::EnumerateFileEvent::~EnumerateFileEvent()
{
}

nsresult
DeviceStorageRequestParent::EnumerateFileEvent::CancelableRun()
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  nsCOMPtr<nsIRunnable> r;
  bool check = false;
  mFile->mFile->Exists(&check);
  if (!check) {
    r = new PostErrorEvent(mParent, POST_ERROR_EVENT_FILE_DOES_NOT_EXIST);
    NS_DispatchToMainThread(r);
    return NS_OK;
  }

  nsTArray<nsRefPtr<DeviceStorageFile> > files;
  mFile->CollectFiles(files, mSince);

  InfallibleTArray<DeviceStorageFileValue> values;

  uint32_t count = files.Length();
  for (uint32_t i = 0; i < count; i++) {
    nsString fullpath;
    files[i]->mFile->GetPath(fullpath);
    DeviceStorageFileValue dsvf(files[i]->mPath, fullpath);
    values.AppendElement(dsvf);
  }

  r = new PostEnumerationSuccessEvent(mParent, values);
  NS_DispatchToMainThread(r);
  return NS_OK;
}


DeviceStorageRequestParent::PostPathResultEvent::PostPathResultEvent(DeviceStorageRequestParent* aParent,
                                                                     const nsAString& aPath)
  : CancelableRunnable(aParent)
  , mPath(aPath)
{
}

DeviceStorageRequestParent::PostPathResultEvent::~PostPathResultEvent()
{
}

nsresult
DeviceStorageRequestParent::PostPathResultEvent::CancelableRun()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  SuccessResponse response;
  unused <<  mParent->Send__delete__(mParent, response);
  return NS_OK;
}

DeviceStorageRequestParent::PostStatResultEvent::PostStatResultEvent(DeviceStorageRequestParent* aParent,
                                                                     int64_t aFreeBytes,
                                                                     int64_t aTotalBytes)
  : CancelableRunnable(aParent)
  , mFreeBytes(aFreeBytes)
  , mTotalBytes(aTotalBytes)
{
}

DeviceStorageRequestParent::PostStatResultEvent::~PostStatResultEvent()
{
}

nsresult
DeviceStorageRequestParent::PostStatResultEvent::CancelableRun()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsString state;
  state.Assign(NS_LITERAL_STRING("available"));
#ifdef MOZ_WIDGET_GONK
  nsresult rv = GetSDCardStatus(state);
  if (NS_FAILED(rv)) {
    state.Assign(NS_LITERAL_STRING("unavailable"));
  }
#endif

  StatStorageResponse response(mFreeBytes, mTotalBytes, state);
  unused <<  mParent->Send__delete__(mParent, response);
  return NS_OK;
}


} // namespace devicestorage
} // namespace dom
} // namespace mozilla
