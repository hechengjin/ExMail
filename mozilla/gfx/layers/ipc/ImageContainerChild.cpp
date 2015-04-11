/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "ImageContainerChild.h"
#include "gfxSharedImageSurface.h"
#include "ShadowLayers.h"
#include "mozilla/layers/PLayers.h"
#include "mozilla/layers/SharedImageUtils.h"
#include "ImageContainer.h"
#include "GonkIOSurfaceImage.h"
#include "GrallocImages.h"

namespace mozilla {
namespace layers {

/*
 * - POOL_MAX_SHARED_IMAGES is the maximum number number of shared images to
 * store in the ImageContainerChild's pool.
 *
 * - MAX_ACTIVE_SHARED_IMAGES is the maximum number of active shared images.
 * the number of active shared images for a given ImageContainerChild is equal
 * to the number of shared images allocated minus the number of shared images
 * dealocated by this ImageContainerChild. What can happen is that the compositor
 * hangs for a moment, while the ImageBridgeChild keep sending images. In such a 
 * scenario the compositor is not sending back shared images so the 
 * ImageContinerChild allocates new ones, and if the compositor hangs for too 
 * long, we can run out of shared memory. MAX_ACTIVE_SHARED_IMAGES is there to
 * throttle that. So when the child side wants to allocate a new shared image 
 * but is already at its maximum of active shared images, it just discards the
 * image (which is therefore not allocated and not sent to the compositor).
 * (see ImageToSharedImage)
 *
 * The values for the two constants are arbitrary and should be tweaked if it 
 * happens that we run into shared memory problems.
 */
static const unsigned int POOL_MAX_SHARED_IMAGES = 5;
static const unsigned int MAX_ACTIVE_SHARED_IMAGES = 10;

ImageContainerChild::ImageContainerChild()
: mImageContainerID(0), mActiveImageCount(0),
  mStop(false), mDispatchedDestroy(false) 
{
  MOZ_COUNT_CTOR(ImageContainerChild);
  // the Release corresponding to this AddRef is in 
  // ImageBridgeChild::DeallocPImageContainer
  AddRef();
}

ImageContainerChild::~ImageContainerChild()
{
  MOZ_COUNT_DTOR(ImageContainerChild);
}

void ImageContainerChild::DispatchStop()
{
  GetMessageLoop()->PostTask(FROM_HERE,
                  NewRunnableMethod(this, &ImageContainerChild::StopChildAndParent));
}

void ImageContainerChild::SetIdleNow() 
{
  if (mStop) return;

  SendFlush();
  ClearSharedImagePool();
}

void ImageContainerChild::DispatchSetIdle()
{
  if (mStop) return;

  GetMessageLoop()->PostTask(FROM_HERE, 
                    NewRunnableMethod(this, &ImageContainerChild::SetIdleNow));
}

void ImageContainerChild::StopChildAndParent()
{
  if (mStop) {
    return;
  }
  mStop = true;    

  SendStop(); // IPC message
  DispatchDestroy();
}

void ImageContainerChild::StopChild()
{
  if (mStop) {
    return;
  }
  mStop = true;    

  DispatchDestroy();
}

bool ImageContainerChild::RecvReturnImage(const SharedImage& aImage)
{
  SharedImage* img = new SharedImage(aImage);
  // Remove oldest image from the queue.
  if (mImageQueue.Length() > 0) {
    mImageQueue.RemoveElementAt(0);
  }
  if (!AddSharedImageToPool(img) || mStop) {
    DestroySharedImage(*img);
    delete img;
  }
  return true;
}

void ImageContainerChild::DestroySharedImage(const SharedImage& aImage)
{
  NS_ABORT_IF_FALSE(InImageBridgeChildThread(),
                    "Should be in ImageBridgeChild thread.");

  --mActiveImageCount;
  DeallocSharedImageData(this, aImage);
}

bool ImageContainerChild::CopyDataIntoSharedImage(Image* src, SharedImage* dest)
{
  if ((src->GetFormat() == PLANAR_YCBCR) && 
      (dest->type() == SharedImage::TYUVImage)) {
    PlanarYCbCrImage *YCbCrImage = static_cast<PlanarYCbCrImage*>(src);
    const PlanarYCbCrImage::Data *data = YCbCrImage->GetData();
    NS_ASSERTION(data, "Must be able to retrieve yuv data from image!");
    YUVImage& yuv = dest->get_YUVImage();

    nsRefPtr<gfxSharedImageSurface> surfY =
      gfxSharedImageSurface::Open(yuv.Ydata());
    nsRefPtr<gfxSharedImageSurface> surfU =
      gfxSharedImageSurface::Open(yuv.Udata());
    nsRefPtr<gfxSharedImageSurface> surfV =
      gfxSharedImageSurface::Open(yuv.Vdata());

    for (int i = 0; i < data->mYSize.height; i++) {
      memcpy(surfY->Data() + i * surfY->Stride(),
             data->mYChannel + i * data->mYStride,
             data->mYSize.width);
    }
    for (int i = 0; i < data->mCbCrSize.height; i++) {
      memcpy(surfU->Data() + i * surfU->Stride(),
             data->mCbChannel + i * data->mCbCrStride,
             data->mCbCrSize.width);
      memcpy(surfV->Data() + i * surfV->Stride(),
             data->mCrChannel + i * data->mCbCrStride,
             data->mCbCrSize.width);
    }

    return true;
  }
  return false; // TODO: support more image formats
}

SharedImage* ImageContainerChild::CreateSharedImageFromData(Image* image)
{
  NS_ABORT_IF_FALSE(InImageBridgeChildThread(),
                  "Should be in ImageBridgeChild thread.");
  
  ++mActiveImageCount;

  if (image->GetFormat() == PLANAR_YCBCR ) {
    PlanarYCbCrImage *YCbCrImage = static_cast<PlanarYCbCrImage*>(image);
    const PlanarYCbCrImage::Data *data = YCbCrImage->GetData();
    NS_ASSERTION(data, "Must be able to retrieve yuv data from image!");
    
    nsRefPtr<gfxSharedImageSurface> tempBufferY;
    nsRefPtr<gfxSharedImageSurface> tempBufferU;
    nsRefPtr<gfxSharedImageSurface> tempBufferV;
    
    if (!AllocateSharedBuffer(this, data->mYSize, gfxASurface::CONTENT_ALPHA,
                              getter_AddRefs(tempBufferY)) ||
        !AllocateSharedBuffer(this, data->mCbCrSize, gfxASurface::CONTENT_ALPHA,
                              getter_AddRefs(tempBufferU)) ||
        !AllocateSharedBuffer(this, data->mCbCrSize, gfxASurface::CONTENT_ALPHA,
                              getter_AddRefs(tempBufferV))) {
      NS_RUNTIMEABORT("creating SharedImage failed!");
    }

    for (int i = 0; i < data->mYSize.height; i++) {
      memcpy(tempBufferY->Data() + i * tempBufferY->Stride(),
             data->mYChannel + i * data->mYStride,
             data->mYSize.width);
    }
    for (int i = 0; i < data->mCbCrSize.height; i++) {
      memcpy(tempBufferU->Data() + i * tempBufferU->Stride(),
             data->mCbChannel + i * data->mCbCrStride,
             data->mCbCrSize.width);
      memcpy(tempBufferV->Data() + i * tempBufferV->Stride(),
             data->mCrChannel + i * data->mCbCrStride,
             data->mCbCrSize.width);
    }

    SharedImage* result = new SharedImage( 
              YUVImage(tempBufferY->GetShmem(),
                       tempBufferU->GetShmem(),
                       tempBufferV->GetShmem(),
                       data->GetPictureRect()));
    NS_ABORT_IF_FALSE(result->type() == SharedImage::TYUVImage,
                      "SharedImage type not set correctly");
    return result;
#ifdef MOZ_WIDGET_GONK
  } else if (image->GetFormat() == GONK_IO_SURFACE) {
    GonkIOSurfaceImage* gonkImage = static_cast<GonkIOSurfaceImage*>(image);
    SharedImage* result = new SharedImage(gonkImage->GetSurfaceDescriptor());
    return result;
  } else if (image->GetFormat() == GRALLOC_PLANAR_YCBCR) {
    GrallocPlanarYCbCrImage* GrallocImage = static_cast<GrallocPlanarYCbCrImage*>(image);
    SharedImage* result = new SharedImage(GrallocImage->GetSurfaceDescriptor());
    return result;
#endif
  } else {
    NS_RUNTIMEABORT("TODO: Only YUVImage is supported here right now.");
  }
  return nullptr;
}

bool ImageContainerChild::AddSharedImageToPool(SharedImage* img)
{
  NS_ABORT_IF_FALSE(InImageBridgeChildThread(), 
                    "AddSharedImageToPool must be called in the ImageBridgeChild thread");
  if (mStop) {
    return false;
  }

  if (mSharedImagePool.Length() >= POOL_MAX_SHARED_IMAGES) {
    return false;
  }
  if (img->type() == SharedImage::TYUVImage) {
    mSharedImagePool.AppendElement(img);
    return true;
  }
  return false; // TODO accept more image formats in the pool
}

static bool
SharedImageCompatibleWith(SharedImage* aSharedImage, Image* aImage)
{
  // TODO accept more image formats
  switch (aImage->GetFormat()) {
  case PLANAR_YCBCR: {
    if (aSharedImage->type() != SharedImage::TYUVImage) {
      return false;
    }
    const PlanarYCbCrImage::Data* data =
      static_cast<PlanarYCbCrImage*>(aImage)->GetData();
    const YUVImage& yuv = aSharedImage->get_YUVImage();

    nsRefPtr<gfxSharedImageSurface> surfY =
      gfxSharedImageSurface::Open(yuv.Ydata());
    if (surfY->GetSize() != data->mYSize) {
      return false;
    }

    nsRefPtr<gfxSharedImageSurface> surfU =
      gfxSharedImageSurface::Open(yuv.Udata());
    if (surfU->GetSize() != data->mCbCrSize) {
      return false;
    }
    return true;
  }
  default:
    return false;
  }
}

SharedImage*
ImageContainerChild::GetSharedImageFor(Image* aImage)
{
  while (mSharedImagePool.Length() > 0) {
    // i.e., img = mPool.pop()
    nsAutoPtr<SharedImage> img(mSharedImagePool.LastElement());
    mSharedImagePool.RemoveElementAt(mSharedImagePool.Length() - 1);

    if (SharedImageCompatibleWith(img, aImage)) {
      return img.forget();
    }
    // The cached image is stale, throw it out.
    DeallocSharedImageData(this, *img);
  }
  
  return nullptr;
}

void ImageContainerChild::ClearSharedImagePool()
{
  NS_ABORT_IF_FALSE(InImageBridgeChildThread(),
                    "Should be in ImageBridgeChild thread.");
  for(unsigned int i = 0; i < mSharedImagePool.Length(); ++i) {
    DeallocSharedImageData(this, *mSharedImagePool[i]);
  }
  mSharedImagePool.Clear();
}

class ImageBridgeCopyAndSendTask : public Task
{
public:
  ImageBridgeCopyAndSendTask(ImageContainerChild * child, 
                             ImageContainer * aContainer, 
                             Image * aImage)
  : mChild(child), mImageContainer(aContainer), mImage(aImage) {}

  void Run()
  { 
    SharedImage* img = mChild->ImageToSharedImage(mImage.get());
    if (img) {
      mChild->SendPublishImage(*img);
    }
  }

  ImageContainerChild *mChild;
  nsRefPtr<ImageContainer> mImageContainer;
  nsRefPtr<Image> mImage;
};

SharedImage* ImageContainerChild::ImageToSharedImage(Image* aImage)
{
  if (mStop) {
    return nullptr;
  }
  if (mActiveImageCount > (int)MAX_ACTIVE_SHARED_IMAGES) {
    // Too many active shared images, perhaps the compositor is hanging.
    // Skipping current image
    return nullptr;
  }

  NS_ABORT_IF_FALSE(InImageBridgeChildThread(),
                    "Should be in ImageBridgeChild thread.");
  SharedImage *img = GetSharedImageFor(aImage);
  if (img) {
    CopyDataIntoSharedImage(aImage, img);  
  } else {
    img = CreateSharedImageFromData(aImage);
  }
  // Keep a reference to the image we sent to compositor to maintain a
  // correct reference count.
  mImageQueue.AppendElement(aImage);
  return img;
}

void ImageContainerChild::SendImageAsync(ImageContainer* aContainer,
                                         Image* aImage)
{
  if(!aContainer || !aImage) {
      return;
  }

  if (mStop) {
    return;
  }

  if (InImageBridgeChildThread()) {
    SharedImage *img = ImageToSharedImage(aImage);
    if (img) {
      SendPublishImage(*img);
    }
    delete img;
    return;
  }

  // Sending images and (potentially) allocating shmems must be done 
  // on the ImageBridgeChild thread.
  Task *t = new ImageBridgeCopyAndSendTask(this, aContainer, aImage);   
  GetMessageLoop()->PostTask(FROM_HERE, t);
}

void ImageContainerChild::DestroyNow()
{
  NS_ABORT_IF_FALSE(InImageBridgeChildThread(),
                    "Should be in ImageBridgeChild thread.");
  NS_ABORT_IF_FALSE(mDispatchedDestroy,
                    "Incorrect state in the destruction sequence.");

  ClearSharedImagePool();

  // will decrease the refcount and, in most cases, delete the ImageContainerChild
  Send__delete__(this);
  Release(); // corresponds to the AddRef in DispatchDestroy
}

void ImageContainerChild::DispatchDestroy()
{
  NS_ABORT_IF_FALSE(mStop, "The state should be 'stopped' when destroying");

  if (mDispatchedDestroy) {
    return;
  }
  mDispatchedDestroy = true;
  AddRef(); // corresponds to the Release in DestroyNow
  GetMessageLoop()->PostTask(FROM_HERE, 
                    NewRunnableMethod(this, &ImageContainerChild::DestroyNow));
}

} // namespace
} // namespace

