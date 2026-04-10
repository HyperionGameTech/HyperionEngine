#pragma once

#include "AxLoaderImageBase.hpp"

#include <TurboJPEG/jpeglib.h>

#include <Core/FxSizedArray.hpp>
#include <Core/FxTypes.hpp>
#include <string>

class AxLoaderJpeg : public AxLoaderImageBase
{
public:
    using Status = AxLoaderBase::Status;

    AxLoaderJpeg() = default;

    Status LoadFromFile(FxTSRef<AxBase> asset, const std::string& path) override;
    Status LoadFromMemory(FxTSRef<AxBase> asset, const uint8* data, uint32 size) override;

    void Destroy(FxTSRef<AxBase>& asset) override;

    ~AxLoaderJpeg() override = default;

protected:
    void CreateGpuResource(FxTSRef<AxBase>& asset) override;

private:
    struct jpeg_decompress_struct mJpegInfo;
    FxSizedArray<uint8> mImageData;
};
