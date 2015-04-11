/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "DeviceStorageRequestChild.h"
#include "nsDeviceStorage.h"
#include "nsDOMFile.h"
#include "mozilla/dom/ipc/Blob.h"

namespace mozilla {
namespace dom {
namespace devicestorage {

DeviceStorageRequestChild::DeviceStorageRequestChild()
{
  MOZ_COUNT_CTOR(DeviceStorageRequestChild);
}

DeviceStorageRequestChild::DeviceStorageRequestChild(DOMRequest* aRequest,
                                                     DeviceStorageFile* aFile)
  : mRequest(aRequest)
  , mFile(aFile)
{
  MOZ_COUNT_CTOR(DeviceStorageRequestChild);
}

DeviceStorageRequestChild::~DeviceStorageRequestChild() {
  MOZ_COUNT_DTOR(DeviceStorageRequestChild);
}

bool
DeviceStorageRequestChild::Recv__delete__(const DeviceStorageResponseValue& aValue)
{
  switch (aValue.type()) {

    case DeviceStorageResponseValue::TErrorResponse:
    {
      ErrorResponse r = aValue;
      mRequest->FireError(r.error());
      break;
    }

    case DeviceStorageResponseValue::TSuccessResponse:
    {
      jsval result = StringToJsval(mRequest->GetOwner(), mFile->mPath);
      mRequest->FireSuccess(result);
      break;
    }

    case DeviceStorageResponseValue::TBlobResponse:
    {
      BlobResponse r = aValue;
      BlobChild* actor = static_cast<BlobChild*>(r.blobChild());
      nsCOMPtr<nsIDOMBlob> blob = actor->GetBlob();

      jsval result = InterfaceToJsval(mRequest->GetOwner(), blob, &NS_GET_IID(nsIDOMBlob));
      mRequest->FireSuccess(result);
      break;
    }

    case DeviceStorageResponseValue::TStatStorageResponse:
    {
      StatStorageResponse r = aValue;

      nsRefPtr<nsIDOMDeviceStorageStat> domstat = new nsDOMDeviceStorageStat(r.freeBytes(), r.totalBytes(), r.mountState());
      jsval result = InterfaceToJsval(mRequest->GetOwner(), domstat, &NS_GET_IID(nsIDOMDeviceStorageStat));
      mRequest->FireSuccess(result);
      break;
    }

    case DeviceStorageResponseValue::TEnumerationResponse:
    {
      EnumerationResponse r = aValue;
      nsDOMDeviceStorageCursor* cursor = static_cast<nsDOMDeviceStorageCursor*>(mRequest.get());

      uint32_t count = r.paths().Length();
      for (uint32_t i = 0; i < count; i++) {
        nsCOMPtr<nsIFile> f;
        nsresult rv = NS_NewLocalFile(r.paths()[i].fullpath(), false, getter_AddRefs(f));
        if (NS_FAILED(rv)) {
          continue;
        }

        nsRefPtr<DeviceStorageFile> dsf = new DeviceStorageFile(f);
        dsf->SetPath(r.paths()[i].name());
        cursor->mFiles.AppendElement(dsf);
      }

      nsCOMPtr<ContinueCursorEvent> event = new ContinueCursorEvent(cursor);
      NS_DispatchToMainThread(event);
      break;
    }

    default:
    {
      NS_RUNTIMEABORT("not reached");
      break;
    }
  }
  return true;
}


} // namespace devicestorage
} // namespace dom
} // namespace mozilla
