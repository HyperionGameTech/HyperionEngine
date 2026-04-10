#include "RxSampler.hpp"

#include <Renderer/RxGlobals.hpp>
#include <Renderer/RxRenderBackend.hpp>


static FX_FORCE_INLINE VkSamplerMipmapMode GetVkMipMode(RxSamplerFilter filter)
{
    switch (filter) {
    case RxSamplerFilter::eNearest:
        return VK_SAMPLER_MIPMAP_MODE_NEAREST;
    case RxSamplerFilter::eLinear:
        return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    }

    return VK_SAMPLER_MIPMAP_MODE_LINEAR;
}

static FX_FORCE_INLINE VkFilter GetVkFilter(RxSamplerFilter filter)
{
    switch (filter) {
    case RxSamplerFilter::eNearest:
        return VK_FILTER_NEAREST;
    case RxSamplerFilter::eLinear:
        return VK_FILTER_LINEAR;
    }

    return VK_FILTER_LINEAR;
}

static FX_FORCE_INLINE VkBorderColor GetVkBorderColor(RxSamplerBorderColor color)
{
    switch (color) {
    case RxSamplerBorderColor::eIntBlack:
        return VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    case RxSamplerBorderColor::eFloatBlack:
        return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;

    case RxSamplerBorderColor::eIntWhite:
        return VK_BORDER_COLOR_INT_OPAQUE_WHITE;
    case RxSamplerBorderColor::eFloatWhite:
        return VK_BORDER_COLOR_INT_OPAQUE_WHITE;

    case RxSamplerBorderColor::eIntTransparent:
        return VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;
    case RxSamplerBorderColor::eFloatTransparent:
        return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    }
}

static FX_FORCE_INLINE VkSamplerAddressMode GetVkAddressMode(RxSamplerAddressMode addr_mode)
{
    switch (addr_mode) {
    case RxSamplerAddressMode::eRepeat:
        return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    case RxSamplerAddressMode::eClampToBorder:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    }
}

static FX_FORCE_INLINE VkCompareOp GetVkCompareOp(RxSamplerCompareOp op)
{
    switch (op) {
    case RxSamplerCompareOp::eNone:
        return VK_COMPARE_OP_NEVER;
    case RxSamplerCompareOp::eEqual:
        return VK_COMPARE_OP_EQUAL;
    case RxSamplerCompareOp::eLess:
        return VK_COMPARE_OP_LESS;
    case RxSamplerCompareOp::eGreater:
        return VK_COMPARE_OP_GREATER;
    case RxSamplerCompareOp::eLessOrEqual:
        return VK_COMPARE_OP_LESS_OR_EQUAL;
    case RxSamplerCompareOp::eGreaterOrEqual:
        return VK_COMPARE_OP_GREATER_OR_EQUAL;
    }
}

RxSampler::RxSampler(RxSampler&& other)
{
    Sampler = other.Sampler;
    CacheId = other.CacheId;
    CachePropId = other.CachePropId;

    other.Sampler = nullptr;
    other.InvalidateCacheId();
}

RxSampler::RxSampler(const RxSamplerProps& props) { Create(props); }

void RxSampler::Create(const RxSamplerProps& props)
{
    if (Sampler) {
        FxLogWarning("Sampler has been previously initialized!");
        return;
    }

    VkSamplerAddressMode vk_addr_mode = GetVkAddressMode(props.AddressMode);


    VkSamplerCreateInfo sampler_info {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,

        .magFilter = GetVkFilter(props.MagFilter),
        .minFilter = GetVkFilter(props.MinFilter),

        .mipmapMode = GetVkMipMode(props.MipFilter),

        .addressModeU = vk_addr_mode,
        .addressModeV = vk_addr_mode,
        .addressModeW = vk_addr_mode,

        .mipLodBias = 0.0f,

        .anisotropyEnable = VK_FALSE,
        // .maxAnisotropy = 0,

        .compareEnable = static_cast<VkBool32>(props.CompareOp != RxSamplerCompareOp::eNone),
        .compareOp = GetVkCompareOp(props.CompareOp),

        .minLod = 0.0f,
        .maxLod = 0.0f,

        .borderColor = GetVkBorderColor(props.BorderColor),
        .unnormalizedCoordinates = VK_FALSE,
    };

    VkResult result = vkCreateSampler(gRenderer->GetDevice()->Device, &sampler_info, nullptr, &Sampler);

    if (result != VK_SUCCESS) {
        FxLogError("Error creating texture sampler!");
        Sampler = nullptr;
    }
}

void RxSampler::Create() { Create({}); }

void RxSampler::Destroy()
{
    if (!Sampler) {
        return;
    }


    vkDestroySampler(gRenderer->GetDevice()->Device, Sampler, nullptr);
    Sampler = nullptr;
}
