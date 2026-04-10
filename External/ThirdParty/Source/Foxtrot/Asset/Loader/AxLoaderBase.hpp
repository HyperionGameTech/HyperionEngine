#pragma once

#include "../AxBase.hpp"

#include <string>

template <typename T>
class FxRef;

class AxLoaderBase
{
public:
    enum class Status
    {
        eNone,
        eSuccess,
        eError,
    };

    AxLoaderBase() {}

    virtual Status LoadFromFile(FxTSRef<AxBase> asset, const std::string& path) = 0;
    virtual Status LoadFromMemory(FxTSRef<AxBase> asset, const uint8* data, uint32 size) = 0;

    virtual void Destroy(FxTSRef<AxBase>& asset) = 0;

    virtual ~AxLoaderBase() {}

    virtual void CreateGpuResource(FxTSRef<AxBase>& asset) = 0;

protected:
    friend class AxManager;
};
