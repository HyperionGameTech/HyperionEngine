#include "AxLoaderJpeg.hpp"

#include "Asset/AxBase.hpp"

#include <TurboJPEG/jpeglib.h>

#include <Asset/AxImage.hpp>
#include <Core/FxMemory.hpp>
#include <Core/FxRef.hpp>

static constexpr J_COLOR_SPACE GetJpegColorspaceForFormat(RxImageFormat format)
{
    switch (format) {
    case RxImageFormat::eBGRA8_UNorm:
        return JCS_EXT_BGRA;
    case RxImageFormat::eRGBA8_UNorm:
    case RxImageFormat::eRGBA8_SRGB:
        return JCS_EXT_RGBA;
    default:;
    }

    return JCS_EXT_RGBA;
}


AxLoaderJpeg::Status AxLoaderJpeg::LoadFromFile(FxTSRef<AxBase> asset, const std::string& path)
{
    FxTSRef<AxImage> image(asset);

    const char* c_path = path.c_str();

    FILE* fp = fopen(c_path, "rb");

    if (!fp) {
        FxLogError("Could not find JPEG file at '{:s}'", c_path);
        return AxLoaderJpeg::Status::eError;
    }

    struct jpeg_error_mgr error_mgr;

    mJpegInfo.err = jpeg_std_error(&error_mgr);
    jpeg_create_decompress(&mJpegInfo);

    jpeg_stdio_src(&mJpegInfo, fp);
    jpeg_read_header(&mJpegInfo, true);

    mJpegInfo.out_color_space = GetJpegColorspaceForFormat(ImageFormat);

    jpeg_start_decompress(&mJpegInfo);

    int32 num_jpeg_components = mJpegInfo.output_components;

    printf("Read jpeg, [width=%u, height=%u]\n", mJpegInfo.output_width, mJpegInfo.output_height);
    image->Size = { mJpegInfo.output_width, mJpegInfo.output_height };

    uint32 data_size = mJpegInfo.output_width * mJpegInfo.output_height * num_jpeg_components;
    mImageData.InitSize(data_size);

    const uint32 row_stride = num_jpeg_components * mJpegInfo.output_width;

    uint8* ptr_list[1] = { nullptr };

    while (mJpegInfo.output_scanline < mJpegInfo.output_height) {
        ptr_list[0] = (mImageData.pData + (row_stride * mJpegInfo.output_scanline));
        jpeg_read_scanlines(&mJpegInfo, ptr_list, 1);
    }

    jpeg_finish_decompress(&mJpegInfo);

    fclose(fp);

    return Status::eSuccess;
}

AxLoaderJpeg::Status AxLoaderJpeg::LoadFromMemory(FxTSRef<AxBase> asset, const uint8* data, uint32 size)
{
    FxTSRef<AxImage> image(asset);

    struct jpeg_error_mgr error_mgr;

    mJpegInfo.err = jpeg_std_error(&error_mgr);
    jpeg_create_decompress(&mJpegInfo);

    FxAssert(data != nullptr);

    jpeg_mem_src(&mJpegInfo, data, size);

    jpeg_read_header(&mJpegInfo, true);


    const uint32 num_components = RxImageFormatUtil::GetSize(ImageFormat);

    J_COLOR_SPACE color_space = GetJpegColorspaceForFormat(ImageFormat);

    mJpegInfo.out_color_space = color_space;

    jpeg_start_decompress(&mJpegInfo);

    printf("Read jpeg, [width=%u, height=%u]\n", mJpegInfo.output_width, mJpegInfo.output_height);
    image->Size = { mJpegInfo.output_width, mJpegInfo.output_height };

    uint32 data_size = mJpegInfo.output_width * mJpegInfo.output_height * num_components;
    mImageData.InitSize(data_size);

    const uint32 row_stride = num_components * mJpegInfo.output_width;

    uint8* ptr_list[1] = { nullptr };

    while (mJpegInfo.output_scanline < mJpegInfo.output_height) {
        ptr_list[0] = (mImageData.pData + (row_stride * mJpegInfo.output_scanline));
        jpeg_read_scanlines(&mJpegInfo, ptr_list, 1);
    }

    jpeg_finish_decompress(&mJpegInfo);

    return Status::eSuccess;
}

void AxLoaderJpeg::CreateGpuResource(FxTSRef<AxBase>& asset)
{
    FxTSRef<AxImage> image(asset);

    image->Image.CreateGpuOnly(image->ImageType, image->Size, ImageFormat, mImageData);

    asset->bIsUploadedToGpu = true;
    asset->bIsUploadedToGpu.notify_all();
}

void AxLoaderJpeg::Destroy(FxTSRef<AxBase>& asset)
{
    //    while (!asset->bIsUploadedToGpu) {
    //        asset->bIsUploadedToGpu.wait(true);
    //    }

    jpeg_destroy_decompress(&mJpegInfo);
}
