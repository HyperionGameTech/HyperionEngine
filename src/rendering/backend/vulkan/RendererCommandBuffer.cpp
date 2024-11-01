/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/RendererCommandBuffer.hpp>
#include <rendering/backend/RendererComputePipeline.hpp>
#include <rendering/backend/RendererGraphicsPipeline.hpp>
#include <rendering/backend/RendererDescriptorSet.hpp>
#include <rendering/backend/rt/RendererRaytracingPipeline.hpp>
#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/RendererFeatures.hpp>

#include <Types.hpp>


namespace hyperion {
namespace renderer {
namespace platform {

template <>
CommandBuffer<Platform::VULKAN>::CommandBuffer(CommandBufferType type)
    : m_platform_impl { this, VK_NULL_HANDLE, VK_NULL_HANDLE },
      m_type(type)
{
}

template <>
CommandBuffer<Platform::VULKAN>::~CommandBuffer()
{
    AssertThrowMsg(m_platform_impl.command_buffer == VK_NULL_HANDLE, "command buffer should have been destroyed");
}
template <>
Result CommandBuffer<Platform::VULKAN>::Create(Device<Platform::VULKAN> *device)
{
    AssertThrow(m_platform_impl.command_pool != VK_NULL_HANDLE);

    VkCommandBufferLevel level;

    switch (m_type) {
    case COMMAND_BUFFER_PRIMARY:
        level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        break;
    case COMMAND_BUFFER_SECONDARY:
        level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
        break;
    default:
        AssertThrowMsg(false, "Unsupported command buffer type");
    }

    VkCommandBufferAllocateInfo alloc_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    alloc_info.level = level;
    alloc_info.commandPool = m_platform_impl.command_pool;
    alloc_info.commandBufferCount = 1;

    HYPERION_VK_CHECK_MSG(
        vkAllocateCommandBuffers(device->GetDevice(), &alloc_info, &m_platform_impl.command_buffer),
        "Failed to allocate command buffer"
    );

    HYPERION_RETURN_OK;
}

template <>
Result CommandBuffer<Platform::VULKAN>::Destroy(Device<Platform::VULKAN> *device)
{
    if (m_platform_impl.command_buffer != VK_NULL_HANDLE) {
        AssertThrow(m_platform_impl.command_pool != VK_NULL_HANDLE);

        vkFreeCommandBuffers(device->GetDevice(), m_platform_impl.command_pool, 1, &m_platform_impl.command_buffer);
        
        m_platform_impl.command_buffer = VK_NULL_HANDLE;
        m_platform_impl.command_pool = VK_NULL_HANDLE;
    }

    HYPERION_RETURN_OK;
}

template <>
Result CommandBuffer<Platform::VULKAN>::Begin(Device<Platform::VULKAN> *device, const RenderPass<Platform::VULKAN> *render_pass)
{
    VkCommandBufferInheritanceInfo inheritance_info { VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO };
    inheritance_info.subpass = 0;
    inheritance_info.framebuffer = VK_NULL_HANDLE;

    VkCommandBufferBeginInfo begin_info { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };

    if (m_type == COMMAND_BUFFER_SECONDARY) {
        if (render_pass == nullptr) {
            return Result { Result::RENDERER_ERR, "Render pass not provided for secondary command buffer!" };
        }

        inheritance_info.renderPass = render_pass->GetHandle();

        begin_info.pInheritanceInfo = &inheritance_info;
        begin_info.flags |= VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
    }

    HYPERION_VK_CHECK_MSG(
        vkBeginCommandBuffer(m_platform_impl.command_buffer, &begin_info),
        "Failed to begin command buffer"
    );

    HYPERION_RETURN_OK;
}

template <>
Result CommandBuffer<Platform::VULKAN>::End(Device<Platform::VULKAN> *device)
{
    HYPERION_VK_CHECK_MSG(
        vkEndCommandBuffer(m_platform_impl.command_buffer),
        "Failed to end command buffer"
    );

    HYPERION_RETURN_OK;
}

template <>
Result CommandBuffer<Platform::VULKAN>::Reset(Device<Platform::VULKAN> *device)
{
    HYPERION_VK_CHECK_MSG(
        vkResetCommandBuffer(m_platform_impl.command_buffer, 0),
        "Failed to reset command buffer"
    );

    HYPERION_RETURN_OK;
}

template <>
Result CommandBuffer<Platform::VULKAN>::SubmitPrimary(
    DeviceQueue<Platform::VULKAN> *queue,
    Fence<Platform::VULKAN> *fence,
    SemaphoreChain *semaphore_chain
)
{
    VkSubmitInfo submit_info { VK_STRUCTURE_TYPE_SUBMIT_INFO };

    if (semaphore_chain != nullptr) {
        submit_info.waitSemaphoreCount = uint32(semaphore_chain->m_wait_semaphores_view.size());
        submit_info.pWaitSemaphores = semaphore_chain->m_wait_semaphores_view.data();
        submit_info.signalSemaphoreCount = uint32(semaphore_chain->m_signal_semaphores_view.size());
        submit_info.pSignalSemaphores = semaphore_chain->m_signal_semaphores_view.data();
        submit_info.pWaitDstStageMask = semaphore_chain->m_wait_semaphores_stage_view.data();
    } else {
        submit_info.waitSemaphoreCount = 0;
        submit_info.pWaitSemaphores = nullptr;
        submit_info.signalSemaphoreCount = 0;
        submit_info.pSignalSemaphores = nullptr;
        submit_info.pWaitDstStageMask = nullptr;
    }

    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &m_platform_impl.command_buffer;

    HYPERION_VK_CHECK_MSG(
        vkQueueSubmit(queue->queue, 1, &submit_info, fence->GetPlatformImpl().handle),
        "Failed to submit command"
    );

    HYPERION_RETURN_OK;
}

template <>
Result CommandBuffer<Platform::VULKAN>::SubmitSecondary(CommandBuffer *primary)
{
    vkCmdExecuteCommands(
        primary->m_platform_impl.command_buffer,
        1,
        &m_platform_impl.command_buffer
    );

    HYPERION_RETURN_OK;
}

template <>
void CommandBuffer<Platform::VULKAN>::BindVertexBuffer(const GPUBuffer<Platform::VULKAN> *buffer)
{
    AssertThrow(buffer != nullptr);
    AssertThrowMsg(buffer->GetBufferType() == GPUBufferType::MESH_VERTEX_BUFFER, "Not a vertex buffer! Got buffer type: %u", uint(buffer->GetBufferType()));
    
    const VkBuffer vertex_buffers[] = { buffer->GetPlatformImpl().handle };
    static const VkDeviceSize offsets[] = { 0 };

    vkCmdBindVertexBuffers(m_platform_impl.command_buffer, 0, 1, vertex_buffers, offsets);
}

template <>
void CommandBuffer<Platform::VULKAN>::BindIndexBuffer(const GPUBuffer<Platform::VULKAN> *buffer, DatumType datum_type)
{
    AssertThrow(buffer != nullptr);
    AssertThrowMsg(buffer->GetBufferType() == GPUBufferType::MESH_INDEX_BUFFER, "Not an index buffer! Got buffer type: %u", uint(buffer->GetBufferType()));
    
    vkCmdBindIndexBuffer(
        m_platform_impl.command_buffer,
        buffer->GetPlatformImpl().handle,
        0,
        helpers::ToVkIndexType(datum_type)
    );
}

template <>
void CommandBuffer<Platform::VULKAN>::DrawIndexed(
    uint32 num_indices,
    uint32 num_instances,
    uint32 instance_index
) const
{
    vkCmdDrawIndexed(
        m_platform_impl.command_buffer,
        num_indices,
        num_instances,
        0,
        0,
        instance_index
    );
}

template <>
void CommandBuffer<Platform::VULKAN>::DrawIndexedIndirect(
    const GPUBuffer<Platform::VULKAN> *buffer,
    uint32 buffer_offset
) const
{
    vkCmdDrawIndexedIndirect(
        m_platform_impl.command_buffer,
        buffer->GetPlatformImpl().handle,
        buffer_offset,
        1,
        uint32(sizeof(IndirectDrawCommand))
    );
}

template <>
void CommandBuffer<Platform::VULKAN>::DebugMarkerBegin(const char *marker_name) const
{
    if (Features::s_dynamic_functions.vkCmdDebugMarkerBeginEXT) {
        const VkDebugMarkerMarkerInfoEXT marker {
            .sType       = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT,
            .pNext       = nullptr,
            .pMarkerName = marker_name
        };

        Features::s_dynamic_functions.vkCmdDebugMarkerBeginEXT(m_platform_impl.command_buffer, &marker);
    }
}

template <>
void CommandBuffer<Platform::VULKAN>::DebugMarkerEnd() const
{
    if (Features::s_dynamic_functions.vkCmdDebugMarkerEndEXT) {
        Features::s_dynamic_functions.vkCmdDebugMarkerEndEXT(m_platform_impl.command_buffer);
    }
}

} // namespace platform
} // namespace renderer
} // namespace hyperion