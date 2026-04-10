#include "AxImage.hpp"

#include <Core/FxPagedArray.hpp>
#include <Core/FxRef.hpp>


static FxPagedArray<FxTSRef<AxImage>> mEmptyImagesPerFormat;

FxPagedArray<FxTSRef<AxImage>>& AxImage::GetEmptyImagesArray() { return mEmptyImagesPerFormat; }

AxImage::AxImage(const AxImage& other) { (*this) = other; }

void AxImage::MarkAndSignalLoaded()
{
    IsFinishedNotifier.SignalDataWritten();

    bIsUploadedToGpu = true;
    bIsUploadedToGpu.notify_all();

    mIsLoaded.store(true);
}

AxImage& AxImage::operator=(const AxImage& other)
{
    Image = other.Image;
    ImageType = other.ImageType;
    Size = other.Size;

    return *this;
}


void AxImage::Destroy()
{
    if (!bIsUploadedToGpu.load()) {
        return;
    }

    Image.DecRef();

    bIsUploadedToGpu = false;
}
