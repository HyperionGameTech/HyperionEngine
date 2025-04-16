/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/RendererSampler.hpp>
#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererFeatures.hpp>
#include <rendering/backend/RendererHelpers.hpp>

#include <rendering/backend/vulkan/VulkanRenderingAPI.hpp>

#include <core/debug/Debug.hpp>

namespace hyperion {

extern IRenderingAPI *g_rendering_api;

namespace renderer {
namespace platform {

static inline VulkanRenderingAPI *GetRenderingAPI()
{
    return static_cast<VulkanRenderingAPI *>(g_rendering_api);
}

template <>
Sampler<Platform::VULKAN>::Sampler(FilterMode min_filter_mode, FilterMode mag_filter_mode, WrapMode wrap_mode)
    : m_platform_impl { this, VK_NULL_HANDLE },
      m_min_filter_mode(min_filter_mode),
      m_mag_filter_mode(mag_filter_mode),
      m_wrap_mode(wrap_mode)
{
}

template <>
Sampler<Platform::VULKAN>::Sampler(Sampler<Platform::VULKAN> &&other) noexcept
    : m_platform_impl { this, other.m_platform_impl.handle },
      m_min_filter_mode(other.m_min_filter_mode),
      m_mag_filter_mode(other.m_mag_filter_mode),
      m_wrap_mode(other.m_wrap_mode)
{
    other.m_platform_impl.handle = VK_NULL_HANDLE;
}

template <>
Sampler<Platform::VULKAN> &Sampler<Platform::VULKAN>::operator=(Sampler<Platform::VULKAN> &&other) noexcept
{
    AssertThrowMsg(m_platform_impl.handle == VK_NULL_HANDLE, "sampler should have been destroyed");

    m_platform_impl.handle = other.m_platform_impl.handle;
    m_min_filter_mode = other.m_min_filter_mode;
    m_mag_filter_mode = other.m_mag_filter_mode;
    m_wrap_mode = other.m_wrap_mode;

    other.m_platform_impl.handle = VK_NULL_HANDLE;

    return *this;
}

template <>
Sampler<Platform::VULKAN>::~Sampler()
{
    AssertThrowMsg(m_platform_impl.handle == VK_NULL_HANDLE, "sampler should have been destroyed");
}

template <>
RendererResult Sampler<Platform::VULKAN>::Create()
{
    AssertThrow(m_platform_impl.handle == VK_NULL_HANDLE);

    VkSamplerCreateInfo sampler_info{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sampler_info.magFilter = helpers::ToVkFilter(m_mag_filter_mode);
    sampler_info.minFilter = helpers::ToVkFilter(m_min_filter_mode);

    sampler_info.addressModeU = helpers::ToVkSamplerAddressMode(m_wrap_mode);
    sampler_info.addressModeV = helpers::ToVkSamplerAddressMode(m_wrap_mode);
    sampler_info.addressModeW = helpers::ToVkSamplerAddressMode(m_wrap_mode);

    //if (device->GetFeatures().GetPhysicalDeviceFeatures().samplerAnisotropy) {
    //    sampler_info.anisotropyEnable = VK_TRUE;
    //    sampler_info.maxAnisotropy = 1.0f;//device->GetFeatures().GetPhysicalDeviceProperties().limits.maxSamplerAnisotropy;
    //} else {
        sampler_info.anisotropyEnable = VK_FALSE;
        sampler_info.maxAnisotropy = 1.0f;
    //}

    sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampler_info.unnormalizedCoordinates = VK_FALSE;
    sampler_info.compareEnable = VK_FALSE;
    sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
    
    switch (m_min_filter_mode) {
    case FilterMode::TEXTURE_FILTER_NEAREST_MIPMAP:
        sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        break;
    case FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP:
        sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        break;
    case FilterMode::TEXTURE_FILTER_MINMAX_MIPMAP:
        sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        break;
    default:
        sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        break;
    }

    sampler_info.mipLodBias = 0.0f;
    sampler_info.minLod = 0.0f;
    sampler_info.maxLod = 12.0f;//65535.0f;

    VkSamplerReductionModeCreateInfoEXT reduction_info { VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO_EXT };

    if (m_min_filter_mode == FilterMode::TEXTURE_FILTER_MINMAX_MIPMAP) {
        if (!GetRenderingAPI()->GetDevice()->GetFeatures().GetSamplerMinMaxProperties().filterMinmaxSingleComponentFormats) {
            return HYP_MAKE_ERROR(RendererError, "Device does not support min/max sampler formats");
        }

        reduction_info.reductionMode = VK_SAMPLER_REDUCTION_MODE_MAX;
        sampler_info.pNext = &reduction_info;
    }

    if (vkCreateSampler(GetRenderingAPI()->GetDevice()->GetDevice(), &sampler_info, nullptr, &m_platform_impl.handle) != VK_SUCCESS) {
        return HYP_MAKE_ERROR(RendererError, "Failed to create sampler!");
    }

    HYPERION_RETURN_OK;
}

template <>
RendererResult Sampler<Platform::VULKAN>::Destroy()
{
    if (m_platform_impl.handle != VK_NULL_HANDLE) {
        vkDestroySampler(GetRenderingAPI()->GetDevice()->GetDevice(), m_platform_impl.handle, nullptr);
        m_platform_impl.handle = VK_NULL_HANDLE;
    }

    HYPERION_RETURN_OK;
}

} // namespace platform
} // namespace renderer
} // namespace hyperion
