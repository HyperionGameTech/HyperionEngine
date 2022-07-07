#include "RendererFence.hpp"

namespace hyperion {
namespace renderer {

Fence::Fence(bool create_signaled)
    : m_handle(VK_NULL_HANDLE),
      m_create_signaled(create_signaled)
{
}

Fence::~Fence()
{
    AssertThrowMsg(m_handle == VK_NULL_HANDLE, "fence should have been destroyed");
}

Result Fence::Create(Device *device)
{
    AssertThrow(m_handle == VK_NULL_HANDLE);

    // Create fence to ensure that the command buffer has finished executing
    VkFenceCreateInfo fence_create_info{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fence_create_info.flags = 0;

    if (m_create_signaled) {
        fence_create_info.flags |= VK_FENCE_CREATE_SIGNALED_BIT;
    }
    
    HYPERION_VK_CHECK(vkCreateFence(device->GetDevice(), &fence_create_info, nullptr, &m_handle));

    HYPERION_RETURN_OK;
}

Result Fence::Destroy(Device *device)
{
    AssertThrow(m_handle != VK_NULL_HANDLE);

    vkDestroyFence(device->GetDevice(), m_handle, nullptr);
    m_handle = VK_NULL_HANDLE;

    HYPERION_RETURN_OK;
}

Result Fence::WaitForGpu(Device *device, bool timeout_loop, VkResult *out_result)
{
    AssertThrow(m_handle != VK_NULL_HANDLE);

    VkResult vk_result;

    do {
        vk_result = vkWaitForFences(device->GetDevice(), 1, &m_handle, VK_TRUE, DEFAULT_FENCE_TIMEOUT);
    } while (vk_result == VK_TIMEOUT && timeout_loop);

    if (out_result != nullptr) {
        *out_result = vk_result;
    }

    HYPERION_VK_CHECK(vk_result);

    HYPERION_RETURN_OK;
}

Result Fence::Reset(Device *device)
{
    HYPERION_VK_CHECK(vkResetFences(device->GetDevice(), 1, &m_handle));

    HYPERION_RETURN_OK;
}

} // namespace renderer
} // namespace hyperion