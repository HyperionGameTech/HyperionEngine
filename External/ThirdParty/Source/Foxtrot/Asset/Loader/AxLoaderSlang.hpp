#pragma once

#include "AxLoaderBase.hpp"

#include <Core/FxSizedArray.hpp>
#include <Core/FxTypes.hpp>

class AxLoaderSlang : public AxLoaderBase
{
public:
    using Status = AxLoaderBase::Status;

    AxLoaderSlang() = default;

    Status LoadFromFile(FxTSRef<AxBase> asset, const std::string& path) override;
    Status LoadFromMemory(FxTSRef<AxBase> asset, const uint8* data, uint32 size) override;

    void Destroy(FxTSRef<AxBase>& asset) override;

    ~AxLoaderSlang() override {}

protected:
    void CreateGpuResource(FxTSRef<AxBase>& asset) override;

private:
};
