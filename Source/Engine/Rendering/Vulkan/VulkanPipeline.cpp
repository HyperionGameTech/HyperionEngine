/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <VulkanPch.hpp>

#include <Rendering/Vulkan/VulkanPipeline.hpp>
#include <Rendering/Vulkan/VulkanDevice.hpp>
#include <Rendering/Vulkan/VulkanRenderInterface.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <Core/Memory/Memory.hpp>

#include <Core/Debug/Debug.hpp>

namespace Hyperion {

extern VulkanRenderInterface RI;

#pragma region VulkanPipelineBase

VulkanPipelineBase::VulkanPipelineBase()
    : m_handle(VK_NULL_HANDLE),
      m_layout(VK_NULL_HANDLE)
{
}

VulkanPipelineBase::~VulkanPipelineBase()
{
    // sanity check to ensure validity of handle and layout are the same
    AssertDebug((m_handle == VK_NULL_HANDLE) == (m_layout == VK_NULL_HANDLE));

    if (m_handle != VK_NULL_HANDLE && m_layout != VK_NULL_HANDLE)
    {
        EnqueueDeletion(FunctionWrapper<Proc<void()>>([handle = m_handle, layout = m_layout]()
            {
                vkDestroyPipeline(RI.GetDevice()->GetDevice(), handle, nullptr);
                vkDestroyPipelineLayout(RI.GetDevice()->GetDevice(), layout, nullptr);
            }));

        m_handle = VK_NULL_HANDLE;
        m_layout = VK_NULL_HANDLE;
    }
}

bool VulkanPipelineBase::IsCreated() const
{
    return m_handle != VK_NULL_HANDLE;
}

#if HYP_DEBUG_MODE

void VulkanPipelineBase::SetDebugName(Name name)
{
    if (!IsCreated())
    {
        return;
    }

    const char* strName = name.LookupString();

    if (RI.dynamicFunctions.vkSetDebugUtilsObjectNameEXT)
    {
        VkDebugUtilsObjectNameInfoEXT objectNameInfo { VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
        objectNameInfo.objectType = VK_OBJECT_TYPE_PIPELINE;
        objectNameInfo.objectHandle = (uint64)m_handle;
        objectNameInfo.pObjectName = strName;

        RI.dynamicFunctions.vkSetDebugUtilsObjectNameEXT(RI.GetDevice()->GetDevice(), &objectNameInfo);
    }
}

void VulkanPipelineBase::SetDebugNameLayout(Name name)
{
    if (m_layout == VK_NULL_HANDLE)
    {
        return;
    }

    const char* strName = name.LookupString();

    if (RI.dynamicFunctions.vkSetDebugUtilsObjectNameEXT)
    {
        VkDebugUtilsObjectNameInfoEXT objectNameInfo { VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
        objectNameInfo.objectType = VK_OBJECT_TYPE_PIPELINE_LAYOUT;
        objectNameInfo.objectHandle = (uint64)m_layout;
        objectNameInfo.pObjectName = strName;

        RI.dynamicFunctions.vkSetDebugUtilsObjectNameEXT(RI.GetDevice()->GetDevice(), &objectNameInfo);
    }
}

#endif

#pragma endregion VulkanPipelineBase

} // namespace Hyperion
