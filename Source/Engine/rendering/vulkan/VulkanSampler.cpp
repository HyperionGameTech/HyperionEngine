/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <VulkanPch.hpp>

#include <rendering/vulkan/VulkanSampler.hpp>
#include <rendering/vulkan/VulkanDevice.hpp>
#include <rendering/vulkan/VulkanHelpers.hpp>
#include <rendering/vulkan/VulkanFeatures.hpp>
#include <rendering/vulkan/VulkanRenderInterface.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <Core/debug/Debug.hpp>

#include <VulkanSampler.generated.inl>

namespace Hyperion {

extern VulkanRenderInterface RI;

VulkanSampler::VulkanSampler(const SamplerDesc& desc)
    : SamplerBase(desc),
      m_handle(VK_NULL_HANDLE)
{
}

VulkanSampler::~VulkanSampler()
{
    if (m_handle != VK_NULL_HANDLE)
    {
        EnqueueDeletion(FunctionWrapper<Proc<void()>>([handle = m_handle]()
            {
                vkDestroySampler(RI.GetDevice()->GetDevice(), handle, nullptr);
            }));

        m_handle = VK_NULL_HANDLE;
    }
}

bool VulkanSampler::IsCreated() const
{
    return m_handle != VK_NULL_HANDLE;
}

RendererResult VulkanSampler::Create()
{
    Assert(m_handle == VK_NULL_HANDLE);

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

    if (m_compareOp != SamplerCompareOp::None)
    {
        samplerInfo.compareEnable = VK_TRUE;

        switch (m_compareOp)
        {
        case SamplerCompareOp::Less:
            samplerInfo.compareOp = VK_COMPARE_OP_LESS;
            break;
        case SamplerCompareOp::LessEq:
            samplerInfo.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
            break;
        case SamplerCompareOp::Greater:
            samplerInfo.compareOp = VK_COMPARE_OP_GREATER;
            break;
        case SamplerCompareOp::GreaterEq:
            samplerInfo.compareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
            break;
        case SamplerCompareOp::Equal:
            samplerInfo.compareOp = VK_COMPARE_OP_EQUAL;
            break;
        case SamplerCompareOp::NotEqual:
            samplerInfo.compareOp = VK_COMPARE_OP_NOT_EQUAL;
            break;
        case SamplerCompareOp::Always:
            samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
            break;
        case SamplerCompareOp::Never:
            samplerInfo.compareOp = VK_COMPARE_OP_NEVER;
            break;
        }
    }

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
    samplerInfo.maxLod = VK_LOD_CLAMP_NONE;

    VkSamplerReductionModeCreateInfoEXT reductionInfo { VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO_EXT };

    if (m_minFilterMode == TFM_MINMAX_MIPMAP)
    {
        if (!RI.GetDevice()->GetFeatures().GetSamplerMinMaxProperties().filterMinmaxSingleComponentFormats)
        {
            return HYP_MAKE_ERROR(RendererError, "Device does not support min/max sampler formats");
        }

        reductionInfo.reductionMode = VK_SAMPLER_REDUCTION_MODE_MAX;
        samplerInfo.pNext = &reductionInfo;
    }

    if (vkCreateSampler(RI.GetDevice()->GetDevice(), &samplerInfo, nullptr, &m_handle) != VK_SUCCESS)
    {
        return HYP_MAKE_ERROR(RendererError, "Failed to create sampler!");
    }

    return {};
}

#if HYP_DEBUG_MODE

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

    g_vulkanDynamicFunctions->vkSetDebugUtilsObjectNameEXT(RI.GetDevice()->GetDevice(), &objectNameInfo);
}

#endif

} // namespace Hyperion
