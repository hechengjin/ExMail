/*
 * Copyright (C) 2012 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "base/basictypes.h"
#include "nsDebug.h"
#include "GonkCameraHwMgr.h"
#include "GonkNativeWindow.h"

#define DOM_CAMERA_LOG_LEVEL        3
#include "CameraCommon.h"

using namespace mozilla;
using namespace mozilla::layers;
using namespace android;

/**
 * See bug 783682.  Most camera implementations, despite claiming they
 * support 'yuv420p' as a preview format, actually ignore this setting
 * and return 'yuv420sp' data anyway.  We have come across a new implementation
 * that, while reporting that 'yuv420p' is supported *and* has been accepted,
 * still returns the frame data in 'yuv420sp' anyway.  So for now, since
 * everyone seems to return this format, we just force it.
 */
#define FORCE_PREVIEW_FORMAT_YUV420SP   1

#if GIHM_TIMING_RECEIVEFRAME
#define INCLUDE_TIME_H                  1
#endif
#if GIHM_TIMING_OVERALL
#define INCLUDE_TIME_H                  1
#endif

#if INCLUDE_TIME_H
#include <time.h>

static __inline void timespecSubtract(struct timespec* a, struct timespec* b)
{
  // a = b - a
  if (b->tv_nsec < a->tv_nsec) {
    b->tv_nsec += 1000000000;
    b->tv_sec -= 1;
  }
  a->tv_nsec = b->tv_nsec - a->tv_nsec;
  a->tv_sec = b->tv_sec - a->tv_sec;
}
#endif

GonkCameraHardware::GonkCameraHardware(GonkCamera* aTarget, uint32_t aCamera)
  : mCamera(aCamera)
  , mFps(30)
#if !FORCE_PREVIEW_FORMAT_YUV420SP
  , mPreviewFormat(PREVIEW_FORMAT_UNKNOWN)
#else
  , mPreviewFormat(PREVIEW_FORMAT_YUV420SP)
#endif
  , mClosing(false)
  , mMonitor("GonkCameraHardware.Monitor")
  , mNumFrames(0)
  , mTarget(aTarget)
  , mInitialized(false)
{
  DOM_CAMERA_LOGI( "%s: this = %p (aTarget = %p)\n", __func__, (void*)this, (void*)aTarget );
  init();
}

void
GonkCameraHardware::OnNewFrame()
{
  if (mClosing) {
    return;
  }
  GonkNativeWindow* window = static_cast<GonkNativeWindow*>(mWindow.get());
  nsRefPtr<GraphicBufferLocked> buffer = window->getCurrentBuffer();
  ReceiveFrame(mTarget, buffer);
}

// Android data callback
void
GonkCameraHardware::DataCallback(int32_t aMsgType, const sp<IMemory> &aDataPtr, camera_frame_metadata_t* aMetadata, void* aUser)
{
  GonkCameraHardware* hw = GetHardware((uint32_t)aUser);
  if (!hw) {
    DOM_CAMERA_LOGW("%s:aUser = %d resolved to no camera hw\n", __func__, (uint32_t)aUser);
    return;
  }
  if (hw->mClosing) {
    return;
  }

  GonkCamera* camera = hw->mTarget;
  if (camera) {
    switch (aMsgType) {
      case CAMERA_MSG_PREVIEW_FRAME:
        // Do nothing
        break;

      case CAMERA_MSG_COMPRESSED_IMAGE:
        ReceiveImage(camera, (uint8_t*)aDataPtr->pointer(), aDataPtr->size());
        break;

      default:
        DOM_CAMERA_LOGE("Unhandled data callback event %d\n", aMsgType);
        break;
    }
  } else {
    DOM_CAMERA_LOGW("%s: hw = %p (camera = NULL)\n", __func__, hw);
  }
}

// Android notify callback
void
GonkCameraHardware::NotifyCallback(int32_t aMsgType, int32_t ext1, int32_t ext2, void* aUser)
{
  bool bSuccess;
  GonkCameraHardware* hw = GetHardware((uint32_t)aUser);
  if (!hw) {
    DOM_CAMERA_LOGW("%s:aUser = %d resolved to no camera hw\n", __func__, (uint32_t)aUser);
    return;
  }
  if (hw->mClosing) {
    return;
  }

  GonkCamera* camera = hw->mTarget;
  if (!camera) {
    return;
  }

  switch (aMsgType) {
    case CAMERA_MSG_FOCUS:
      if (ext1) {
        DOM_CAMERA_LOGI("Autofocus complete");
        bSuccess = true;
      } else {
        DOM_CAMERA_LOGW("Autofocus failed");
        bSuccess = false;
      }
      AutoFocusComplete(camera, bSuccess);
      break;

    case CAMERA_MSG_SHUTTER:
      DOM_CAMERA_LOGW("Shutter event not handled yet\n");
      break;

    default:
      DOM_CAMERA_LOGE("Unhandled notify callback event %d\n", aMsgType);
      break;
  }
}

void
GonkCameraHardware::init()
{
  DOM_CAMERA_LOGI("%s: this = %p\n", __func__, (void* )this);

  if (hw_get_module(CAMERA_HARDWARE_MODULE_ID, (const hw_module_t**)&mModule) < 0) {
    return;
  }
  char cameraDeviceName[4];
  snprintf(cameraDeviceName, sizeof(cameraDeviceName), "%d", mCamera);
  mHardware = new CameraHardwareInterface(cameraDeviceName);
  if (mHardware->initialize(&mModule->common) != OK) {
    mHardware.clear();
    return;
  }

  mWindow = new GonkNativeWindow(this);

  if (sHwHandle == 0) {
    sHwHandle = 1;  // don't use 0
  }
  mHardware->setCallbacks(GonkCameraHardware::NotifyCallback, GonkCameraHardware::DataCallback, NULL, (void*)sHwHandle);

  // initialize the local camera parameter database
  mParams = mHardware->getParameters();

  mHardware->setPreviewWindow(mWindow);

  mInitialized = true;
}

GonkCameraHardware::~GonkCameraHardware()
{
  DOM_CAMERA_LOGI( "%s:%d : this = %p\n", __func__, __LINE__, (void*)this );
  sHw = nullptr;
}

GonkCameraHardware* GonkCameraHardware::sHw         = nullptr;
uint32_t            GonkCameraHardware::sHwHandle   = 0;

void
GonkCameraHardware::ReleaseHandle(uint32_t aHwHandle)
{
  GonkCameraHardware* hw = GetHardware(aHwHandle);
  DOM_CAMERA_LOGI("%s: aHwHandle = %d, hw = %p (sHwHandle = %d)\n", __func__, aHwHandle, (void*)hw, sHwHandle);
  if (!hw) {
    return;
  }

  DOM_CAMERA_LOGI("%s: before: sHwHandle = %d\n", __func__, sHwHandle);
  sHwHandle += 1; // invalidate old handles before deleting
  hw->mClosing = true;
  hw->mHardware->disableMsgType(CAMERA_MSG_ALL_MSGS);
  hw->mHardware->stopPreview();
  hw->mHardware->release();
  GonkNativeWindow* window = static_cast<GonkNativeWindow*>(hw->mWindow.get());
  window->abandon();
  DOM_CAMERA_LOGI("%s: after: sHwHandle = %d\n", __func__, sHwHandle);
  delete hw;     // destroy the camera hardware instance
}

uint32_t
GonkCameraHardware::GetHandle(GonkCamera* aTarget, uint32_t aCamera)
{
  ReleaseHandle(sHwHandle);

  sHw = new GonkCameraHardware(aTarget, aCamera);

  if (sHw->IsInitialized()) {
    return sHwHandle;
  }

  DOM_CAMERA_LOGE("failed to initialize camera hardware\n");
  delete sHw;
  sHw = nullptr;
  return 0;
}

uint32_t
GonkCameraHardware::GetFps(uint32_t aHwHandle)
{
  GonkCameraHardware* hw = GetHardware(aHwHandle);
  if (!hw) {
    return 0;
  }

  return hw->mFps;
}

void
GonkCameraHardware::GetPreviewSize(uint32_t aHwHandle, uint32_t* aWidth, uint32_t* aHeight)
{
  GonkCameraHardware* hw = GetHardware(aHwHandle);
  if (hw) {
    *aWidth = hw->mWidth;
    *aHeight = hw->mHeight;
  } else {
    *aWidth = 0;
    *aHeight = 0;
  }
}

void
GonkCameraHardware::SetPreviewSize(uint32_t aWidth, uint32_t aHeight)
{
  Vector<Size> previewSizes;
  uint32_t bestWidth = aWidth;
  uint32_t bestHeight = aHeight;
  uint32_t minSizeDelta = PR_UINT32_MAX;
  uint32_t delta;
  Size size;

  mParams.getSupportedPreviewSizes(previewSizes);

  if (!aWidth && !aHeight) {
    // no size specified, take the first supported size
    size = previewSizes[0];
    bestWidth = size.width;
    bestHeight = size.height;
  } else if (aWidth && aHeight) {
    // both height and width specified, find the supported size closest to requested size
    for (uint32_t i = 0; i < previewSizes.size(); i++) {
      Size size = previewSizes[i];
      uint32_t delta = abs((long int)(size.width * size.height - aWidth * aHeight));
      if (delta < minSizeDelta) {
        minSizeDelta = delta;
        bestWidth = size.width;
        bestHeight = size.height;
      }
    }
  } else if (!aWidth) {
    // width not specified, find closest height match
    for (uint32_t i = 0; i < previewSizes.size(); i++) {
      size = previewSizes[i];
      delta = abs((long int)(size.height - aHeight));
      if (delta < minSizeDelta) {
        minSizeDelta = delta;
        bestWidth = size.width;
        bestHeight = size.height;
      }
    }
  } else if (!aHeight) {
    // height not specified, find closest width match
    for (uint32_t i = 0; i < previewSizes.size(); i++) {
      size = previewSizes[i];
      delta = abs((long int)(size.width - aWidth));
      if (delta < minSizeDelta) {
        minSizeDelta = delta;
        bestWidth = size.width;
        bestHeight = size.height;
      }
    }
  }

  mWidth = bestWidth;
  mHeight = bestHeight;
  mParams.setPreviewSize(mWidth, mHeight);
}

void
GonkCameraHardware::SetPreviewSize(uint32_t aHwHandle, uint32_t aWidth, uint32_t aHeight)
{
  GonkCameraHardware* hw = GetHardware(aHwHandle);
  if (hw) {
    hw->SetPreviewSize(aWidth, aHeight);
  }
}

int
GonkCameraHardware::AutoFocus(uint32_t aHwHandle)
{
  DOM_CAMERA_LOGI("%s: aHwHandle = %d\n", __func__, aHwHandle);
  GonkCameraHardware* hw = GetHardware(aHwHandle);
  if (!hw) {
    return DEAD_OBJECT;
  }

  hw->mHardware->enableMsgType(CAMERA_MSG_FOCUS);
  return hw->mHardware->autoFocus();
}

void
GonkCameraHardware::CancelAutoFocus(uint32_t aHwHandle)
{
  DOM_CAMERA_LOGI("%s: aHwHandle = %d\n", __func__, aHwHandle);
  GonkCameraHardware* hw = GetHardware(aHwHandle);
  if (hw) {
    hw->mHardware->cancelAutoFocus();
  }
}

int
GonkCameraHardware::TakePicture(uint32_t aHwHandle)
{
  GonkCameraHardware* hw = GetHardware(aHwHandle);
  if (!hw) {
    return DEAD_OBJECT;
  }

  hw->mHardware->enableMsgType(CAMERA_MSG_COMPRESSED_IMAGE);
  return hw->mHardware->takePicture();
}

void
GonkCameraHardware::CancelTakePicture(uint32_t aHwHandle)
{
  GonkCameraHardware* hw = GetHardware(aHwHandle);
  if (hw) {
    hw->mHardware->cancelPicture();
  }
}

int
GonkCameraHardware::PushParameters(uint32_t aHwHandle, const CameraParameters& aParams)
{
  GonkCameraHardware* hw = GetHardware(aHwHandle);
  if (!hw) {
    return DEAD_OBJECT;
  }

  return hw->mHardware->setParameters(aParams);
}

void
GonkCameraHardware::PullParameters(uint32_t aHwHandle, CameraParameters& aParams)
{
  GonkCameraHardware* hw = GetHardware(aHwHandle);
  if (hw) {
    aParams = hw->mHardware->getParameters();
  }
}

int
GonkCameraHardware::StartPreview()
{
  const char* format;

#if !FORCE_PREVIEW_FORMAT_YUV420SP
  DOM_CAMERA_LOGI("Preview formats: %s\n", mParams.get(mParams.KEY_SUPPORTED_PREVIEW_FORMATS));

  // try to set preferred image format and frame rate
  const char* const PREVIEW_FORMAT = "yuv420p";
  const char* const BAD_PREVIEW_FORMAT = "yuv420sp";
  mParams.setPreviewFormat(PREVIEW_FORMAT);
  mParams.setPreviewFrameRate(mFps);
  mHardware->setParameters(mParams);

  // check that our settings stuck
  mParams = mHardware->getParameters();
  format = mParams.getPreviewFormat();
  if (strcmp(format, PREVIEW_FORMAT) == 0) {
    mPreviewFormat = PREVIEW_FORMAT_YUV420P;  /* \o/ */
  } else if (strcmp(format, BAD_PREVIEW_FORMAT) == 0) {
    mPreviewFormat = PREVIEW_FORMAT_YUV420SP;
    DOM_CAMERA_LOGA("Camera ignored our request for '%s' preview, will have to convert (from %d)\n", PREVIEW_FORMAT, mPreviewFormat);
  } else {
    mPreviewFormat = PREVIEW_FORMAT_UNKNOWN;
    DOM_CAMERA_LOGE("Camera ignored our request for '%s' preview, returned UNSUPPORTED format '%s'\n", PREVIEW_FORMAT, format);
  }
#else
  mParams.setPreviewFormat("yuv420sp");
  mParams.setPreviewFrameRate(mFps);
  mHardware->setParameters(mParams);
#endif

  // Check the frame rate and log if the camera ignored our setting
  uint32_t fps = mParams.getPreviewFrameRate();
  if (fps != mFps) {
    DOM_CAMERA_LOGA("We asked for %d fps but camera returned %d fps, using it", mFps, fps);
    mFps = fps;
  }

  return mHardware->startPreview();
}

int
GonkCameraHardware::StartPreview(uint32_t aHwHandle)
{
  GonkCameraHardware* hw = GetHardware(aHwHandle);
  DOM_CAMERA_LOGI("%s:%d : aHwHandle = %d, hw = %p\n", __func__, __LINE__, aHwHandle, hw);
  if (!hw) {
    return DEAD_OBJECT;
  }

  return hw->StartPreview();
}

void
GonkCameraHardware::StopPreview(uint32_t aHwHandle)
{
  GonkCameraHardware* hw = GetHardware(aHwHandle);
  if (hw) {
    hw->mHardware->stopPreview();
  }
}

uint32_t
GonkCameraHardware::GetPreviewFormat(uint32_t aHwHandle)
{
  GonkCameraHardware* hw = GetHardware(aHwHandle);
  if (!hw) {
    return PREVIEW_FORMAT_UNKNOWN;
  }

  return hw->mPreviewFormat;
}
