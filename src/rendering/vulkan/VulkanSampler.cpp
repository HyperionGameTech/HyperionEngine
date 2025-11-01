/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/vulkan/VulkanSampler.hpp>
#include <rendering/vulkan/VulkanDevice.hpp>
#include <rendering/vulkan/VulkanHelpers.hpp>
#include <rendering/vulkan/VulkanFeatures.hpp>
#include <rendering/vulkan/VulkanRenderBackend.hpp>

#include <core/debug/Debug.hpp>

namespace hyperion {

extern IRenderBackend* g_renderBackend;

static inline VulkanRenderBackend* GetRenderBackend()
{
    return static_cast<VulkanRenderBackend*>(g_renderBackend);
}

VulkanSampler::VulkanSampler(TextureFilterMode minFilterMode, TextureFilterMode magFilterMode, TextureWrapMode wrapMode)
    : m_handle(VK_NULL_HANDLE)
{
    m_minFilterMode = minFilterMode;
    m_magFilterMode = magFilterMode;
    m_wrapMode = wrapMode;
}

VulkanSampler::~VulkanSampler()
{
    if (m_handle != VK_NULL_HANDLE)
    {
        vkDestroySampler(GetRenderBackend()->GetDevice()->GetDevice(), m_handle, nullptr);
        m_handle = VK_NULL_HANDLE;
    }
}

bool VulkanSampler::IsCreated() const
{
    return m_handle != VK_NULL_HANDLE;
}

RendererResult VulkanSampler::Create()
{
    HYP_GFX_ASSERT(m_handle == VK_NULL_HANDLE);

    VkSamplerCreateInfo samplerInfo { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    samplerInfo.magFilter = ToVkFilter(m_magFilterMode);
    samplerInfo.minFilter = ToVkFilter(m_minFilterMode);
    samplerInfo.addressModeU = ToVkSamplerAddressMode(m_wrapMode);
    samplerInfo.addressModeV = ToVkSamplerAddressMode(m_wrapMode);
    samplerInfo.addressModeW = ToVkSamplerAddressMode(m_wrapMode);

    // if (device->GetFeatures().GetPhysicalDeviceFeatures().samplerAnisotropy) {
    //     samplerInfo.anisotropyEnable = VK_TRUE;
    //     samplerInfo.maxAnisotropy = 1.0f;//device->GetFeatures().GetPhysicalDeviceProperties().limits.maxSamplerAnisotropy;
    // } else {
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    //}

    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

    switch (m_minFilterMode)
    {
    case TFM_NEAREST_MIPMAP:
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        break;
    case TFM_LINEAR_MIPMAP:
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        break;
    case TFM_MINMAX_MIPMAP:
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        break;
    default:
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        break;
    }

    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 12.0f; // 65535.0f;

    VkSamplerReductionModeCreateInfoEXT reductionInfo { VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO_EXT };

    if (m_minFilterMode == TFM_MINMAX_MIPMAP)
    {
        if (!GetRenderBackend()->GetDevice()->GetFeatures().GetSamplerMinMaxProperties().filterMinmaxSingleComponentFormats)
        {
            return HYP_MAKE_ERROR(RendererError, "Device does not support min/max sampler formats");
        }

        reductionInfo.reductionMode = VK_SAMPLER_REDUCTION_MODE_MAX;
        samplerInfo.pNext = &reductionInfo;
    }

    if (vkCreateSampler(GetRenderBackend()->GetDevice()->GetDevice(), &samplerInfo, nullptr, &m_handle) != VK_SUCCESS)
    {
        return HYP_MAKE_ERROR(RendererError, "Failed to create sampler!");
    }

    HYPERION_RETURN_OK;
}

#ifdef HYP_DEBUG_MODE

void VulkanSampler::SetDebugName(Name name)
{
    SamplerBase::SetDebugName(name);

    if (!IsCreated())
    {
        return;
    }

    const char* strName = name.LookupString();

    VkDebugUtilsObjectNameInfoEXT objectNameInfo { VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
    objectNameInfo.objectType = VK_OBJECT_TYPE_SAMPLER;
    objectNameInfo.objectHandle = (uint64)m_handle;
    objectNameInfo.pObjectName = strName;

    g_vulkanDynamicFunctions->vkSetDebugUtilsObjectNameEXT(GetRenderBackend()->GetDevice()->GetDevice(), &objectNameInfo);
}

#endif

} // namespace hyperion
