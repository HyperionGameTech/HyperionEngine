#include "AxLoaderStb.hpp"

#include <Asset/AxBase.hpp>
#include <Asset/AxImage.hpp>

AxLoaderStb::Status AxLoaderStb::LoadFromFile(FxTSRef<AxBase> asset, const std::string& path)
{
    FxTSRef<AxImage> image(asset);

    const char* c_path = path.c_str();

    const int pixel_size = RxImageFormatUtil::GetSize(ImageFormat);
    FxAssert(pixel_size > 0);

    stbi_info(c_path, &mWidth, &mHeight, &mChannels);

    // image->NumComponents = mChannels;
    image->Size = { uint32(mWidth), uint32(mHeight) };

    uint32 data_size = mWidth * mHeight * pixel_size;
    mDataSize = data_size;

    mImageData = stbi_load(c_path, &mWidth, &mHeight, &mChannels, pixel_size);
    if (mImageData == nullptr) {
        FxLogError("Could not load image file at '{}'", c_path);
        return AxLoaderStb::Status::eError;
    }

    return AxLoaderStb::Status::eSuccess;
}

AxLoaderStb::Status AxLoaderStb::LoadFromMemory(FxTSRef<AxBase> asset, const uint8* data, uint32 size)
{
    FxTSRef<AxImage> image(asset);

    const int pixel_size = RxImageFormatUtil::GetSize(ImageFormat);
    FxAssert(pixel_size > 0);

    if (!stbi_info_from_memory(data, size, &mWidth, &mHeight, &mChannels)) {
        FxLogError("Could not retrieve info from image in memory! (Size={})", size);
        return AxLoaderStb::Status::eError;
    }

    mChannels = pixel_size;

    uint32 data_size = mWidth * mHeight * (pixel_size * sizeof(uint8));
    mDataSize = data_size;

    image->Size = { uint32(mWidth), uint32(mHeight) };

    mImageData = stbi_load_from_memory(data, size, &mWidth, &mHeight, &mChannels, pixel_size);

    if (mImageData == nullptr) {
        FxLogError("Could not load image file from memory!");
        return AxLoaderStb::Status::eError;
    }

    return AxLoaderStb::Status::eSuccess;
}

void AxLoaderStb::CreateGpuResource(FxTSRef<AxBase>& asset)
{
    FxTSRef<AxImage> image(asset);

    FxSizedArray<uint8> data_arr;

    // Since the image data in mImageData is not ours(it is stb's),
    // we need to set this flag to prevent it from attemping to be freed.
    data_arr.DoNotDestroy = true;

    data_arr.pData = mImageData;
    data_arr.Size = mDataSize;
    data_arr.Capacity = mDataSize;

    image->Image.CreateGpuOnly(image->ImageType, image->Size, ImageFormat, data_arr);

    asset = image;

    asset->bIsUploadedToGpu = true;
    asset->bIsUploadedToGpu.notify_all();
}

// void FxLoaderStb::LoadCubemapToLayeredImage(const RxImage&)
// {
//     FxVec2u cubemap_size = cubemap_image.Size;

//     // Here is the type of cubemap we will be looking for here:
//     //
//     // T -> Top, B -> Bottom, L -> Left, R -> Right
//     // FW -> Forward, BW -> Backward
//     //
//     // +-----+-----+-----+-----+
//     // |     |  T  |     |     |
//     // +-----+-----+-----+-----+
//     // |  L  |  FW |  R  |  BW |
//     // +-----+-----+-----+-----+
//     // |     |  B  |     |     |
//     // +-----+-----+-----+-----+
//     //
//     // Note that it is 4 tiles wide and 3 tiles tall.

//     const uint32 tile_width = cubemap_size.X / 4;
//     const uint32 tile_height = cubemap_size.Y / 3;

//     if (tile_width != tile_height) {
//         FxLogWarning("Cubemap tile width != cubemap tile height!");
//     }

//     FxStackArray<VkImageCopy, 6> image_copy_infos;
// }

void AxLoaderStb::Destroy(FxTSRef<AxBase>& asset)
{
    // while (!asset->bIsUploadedToGpu) {
    //     asset->bIsUploadedToGpu.wait(true);
    // }

    stbi_image_free(mImageData);
}
