#pragma once

#include "RxDevice.hpp"

#include <vulkan/vulkan.h>

#include <Core/FxHash.hpp>


enum class RxSamplerFilter : uint8
{
    eNearest,
    eLinear,
};

enum class RxSamplerAddressMode : uint8
{
    eRepeat,
    eClampToBorder,
};

enum class RxSamplerBorderColor : uint8
{
    eIntBlack,
    eFloatBlack,
    eIntWhite,
    eFloatWhite,
    eIntTransparent,
    eFloatTransparent,
};

enum class RxSamplerCompareOp : uint8
{
    eNone,
    eEqual,
    eLess,
    eGreater,
    eLessOrEqual,
    eGreaterOrEqual,
};

struct RxSamplerProps
{
    RxSamplerFilter MinFilter = RxSamplerFilter::eLinear;
    RxSamplerFilter MagFilter = RxSamplerFilter::eLinear;
    RxSamplerFilter MipFilter = RxSamplerFilter::eLinear;

    RxSamplerAddressMode AddressMode = RxSamplerAddressMode::eRepeat;
    RxSamplerBorderColor BorderColor = RxSamplerBorderColor::eIntBlack;
    RxSamplerCompareOp CompareOp = RxSamplerCompareOp::eNone;
};


class RxSampler
{
public:
    RxSampler() = default;
    RxSampler(const RxSamplerProps& props);

    RxSampler(RxSampler&& other);

    void Create(const RxSamplerProps& props);

    void Create();


    void SetCacheId(uint64 prop_id, uint32 cache_id)
    {
        CachePropId = prop_id;
        CacheId = cache_id;
    }

    void InvalidateCacheId()
    {
        CachePropId = FxHashNull64;
        CacheId = UINT32_MAX;
    }

    void Destroy();

    ~RxSampler() { Destroy(); }

public:
    VkSampler Sampler = nullptr;

    /**
     * @brief The index for the sampler in the cache.
     */
    uint32 CacheId = UINT32_MAX;

    /**
     * @brief An identifier for the properties that the sampler was created with. Used by RxSamplerCache.
     */
    FxHash64 CachePropId = FxHashNull64;

private:
    friend struct RxSamplerHandle;
};
