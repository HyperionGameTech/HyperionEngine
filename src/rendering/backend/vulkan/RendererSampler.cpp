/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/vulkan/RendererSampler.hpp>
#include <rendering/backend/vulkan/VulkanRenderBackend.hpp>

#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererFeatures.hpp>
#include <rendering/backend/RendererHelpers.hpp>

#include <core/debug/Debug.hpp>

namespace hyperion {

extern IRenderBackend* g_render_backend;

static inline VulkanRenderBackend* GetRenderBackend()
{
    return static_cast<VulkanRenderBackend*>(g_render_backend);
}

VulkanSampler::VulkanSampler(TextureFilterMode min_filter_mode, TextureFilterMode mag_filter_mode, TextureWrapMode wrap_mode)
    : m_handle(VK_NULL_HANDLE)
{
    m_min_filter_mode = min_filter_mode;
    m_mag_filter_mode = mag_filter_mode;
    m_wrap_mode = wrap_mode;
}

VulkanSampler::~VulkanSampler()
{
    AssertThrowMsg(m_handle == VK_NULL_HANDLE, "sampler should have been destroyed");
}

bool VulkanSampler::IsCreated() const
{
    return m_handle != VK_NULL_HANDLE;
}

RendererResult VulkanSampler::Create()
{
    AssertThrow(m_handle == VK_NULL_HANDLE);

    VkSamplerCreateInfo sampler_info { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    sampler_info.magFilter = helpers::ToVkFilter(m_mag_filter_mode);
    sampler_info.minFilter = helpers::ToVkFilter(m_min_filter_mode);
    sampler_info.addressModeU = helpers::ToVkSamplerAddressMode(m_wrap_mode);
    sampler_info.addressModeV = helpers::ToVkSamplerAddressMode(m_wrap_mode);
    sampler_info.addressModeW = helpers::ToVkSamplerAddressMode(m_wrap_mode);

    // if (device->GetFeatures().GetPhysicalDeviceFeatures().samplerAnisotropy) {
    //     sampler_info.anisotropyEnable = VK_TRUE;
    //     sampler_info.maxAnisotropy = 1.0f;//device->GetFeatures().GetPhysicalDeviceProperties().limits.maxSamplerAnisotropy;
    // } else {
    sampler_info.anisotropyEnable = VK_FALSE;
    sampler_info.maxAnisotropy = 1.0f;
    //}

    sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampler_info.unnormalizedCoordinates = VK_FALSE;
    sampler_info.compareEnable = VK_FALSE;
    sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;

    switch (m_min_filter_mode)
    {
    case TFM_NEAREST_MIPMAP:
        sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        break;
    case TFM_LINEAR_MIPMAP:
        sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        break;
    case TFM_MINMAX_MIPMAP:
        sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        break;
    default:
        sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        break;
    }

    sampler_info.mipLodBias = 0.0f;
    sampler_info.minLod = 0.0f;
    sampler_info.maxLod = 12.0f; // 65535.0f;

    VkSamplerReductionModeCreateInfoEXT reduction_info { VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO_EXT };

    if (m_min_filter_mode == TFM_MINMAX_MIPMAP)
    {
        if (!GetRenderBackend()->GetDevice()->GetFeatures().GetSamplerMinMaxProperties().filterMinmaxSingleComponentFormats)
        {
            return HYP_MAKE_ERROR(RendererError, "Device does not support min/max sampler formats");
        }

        reduction_info.reductionMode = VK_SAMPLER_REDUCTION_MODE_MAX;
        sampler_info.pNext = &reduction_info;
    }

    if (vkCreateSampler(GetRenderBackend()->GetDevice()->GetDevice(), &sampler_info, nullptr, &m_handle) != VK_SUCCESS)
    {
        return HYP_MAKE_ERROR(RendererError, "Failed to create sampler!");
    }

    HYPERION_RETURN_OK;
}

RendererResult VulkanSampler::Destroy()
{
    if (m_handle != VK_NULL_HANDLE)
    {
        vkDestroySampler(GetRenderBackend()->GetDevice()->GetDevice(), m_handle, nullptr);
        m_handle = VK_NULL_HANDLE;
    }

    HYPERION_RETURN_OK;
}

} // namespace hyperion
