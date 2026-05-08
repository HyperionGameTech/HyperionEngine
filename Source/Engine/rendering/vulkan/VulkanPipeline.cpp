/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <VulkanPch.hpp>

#include <rendering/vulkan/VulkanPipeline.hpp>
#include <rendering/vulkan/VulkanDevice.hpp>
#include <rendering/vulkan/VulkanRenderInterface.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <Core/memory/Memory.hpp>

#include <Core/debug/Debug.hpp>

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

void VulkanPipelineBase::SetPushConstants(const void* data, size_t size)
{
    Assert(size <= 128, "Push constant data size exceeds 128 bytes");

    m_pushConstants = PushConstantData(data, size);
}

#if HYP_DEBUG_MODE

void VulkanPipelineBase::SetDebugName(Name name)
{
    if (!IsCreated())
    {
        return;
    }

    const char* strName = name.LookupString();

    VkDebugUtilsObjectNameInfoEXT objectNameInfo { VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
    objectNameInfo.objectType = VK_OBJECT_TYPE_PIPELINE;
    objectNameInfo.objectHandle = (uint64)m_handle;
    objectNameInfo.pObjectName = strName;

    g_vulkanDynamicFunctions->vkSetDebugUtilsObjectNameEXT(RI.GetDevice()->GetDevice(), &objectNameInfo);
}

#endif

#pragma endregion VulkanPipelineBase

} // namespace Hyperion
