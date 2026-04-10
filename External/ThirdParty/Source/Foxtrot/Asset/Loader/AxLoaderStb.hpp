#pragma once

#include "AxLoaderImageBase.hpp"

#include <ThirdParty/stb_image.h>

#include <Core/FxSizedArray.hpp>
#include <Core/FxTypes.hpp>
#include <string>

class AxLoaderStb : public AxLoaderImageBase
{
public:
    using Status = AxLoaderBase::Status;

    AxLoaderStb() = default;

    Status LoadFromFile(FxTSRef<AxBase> asset, const std::string& path) override;
    Status LoadFromMemory(FxTSRef<AxBase> asset, const uint8* data, uint32 size) override;

    void Destroy(FxTSRef<AxBase>& asset) override;

    ~AxLoaderStb() override = default;

protected:
    void CreateGpuResource(FxTSRef<AxBase>& asset) override;

private:
    void LoadCubemapToLayeredImage();

private:
    int mWidth;
    int mHeight;
    int mChannels;

    uint32 mDataSize = 0;
    uint8* mImageData = nullptr;
};
