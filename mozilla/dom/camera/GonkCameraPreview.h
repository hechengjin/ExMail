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

#ifndef DOM_CAMERA_GONKCAMERAPREVIEW_H
#define DOM_CAMERA_GONKCAMERAPREVIEW_H

#include "CameraPreview.h"

#define DOM_CAMERA_LOG_LEVEL  3
#include "CameraCommon.h"

namespace mozilla {
namespace layers {
class GraphicBufferLocked;
} // namespace layers
} // namespace mozilla

namespace mozilla {

class GonkCameraPreview : public CameraPreview
{
public:
  GonkCameraPreview(nsIThread* aCameraThread, uint32_t aHwHandle, uint32_t aWidth, uint32_t aHeight)
    : CameraPreview(aCameraThread, aWidth, aHeight)
    , mHwHandle(aHwHandle)
    , mDiscardedFrameCount(0)
    , mFormat(GonkCameraHardware::PREVIEW_FORMAT_UNKNOWN)
  { }

  void ReceiveFrame(layers::GraphicBufferLocked* aBuffer);

  nsresult StartImpl();
  nsresult StopImpl();

protected:
  ~GonkCameraPreview()
  {
    Stop();
  }

  uint32_t mHwHandle;
  uint32_t mDiscardedFrameCount;
  uint32_t mFormat;

private:
  GonkCameraPreview(const GonkCameraPreview&) MOZ_DELETE;
  GonkCameraPreview& operator=(const GonkCameraPreview&) MOZ_DELETE;
};

} // namespace mozilla

#endif // DOM_CAMERA_GONKCAMERAPREVIEW_H
