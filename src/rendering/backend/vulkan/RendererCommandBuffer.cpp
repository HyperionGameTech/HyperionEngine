/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/vulkan/RendererCommandBuffer.hpp>
#include <rendering/backend/vulkan/RendererRenderPass.hpp>
#include <rendering/backend/vulkan/RendererSemaphore.hpp>
#include <rendering/backend/vulkan/VulkanRenderingAPI.hpp>

#include <rendering/backend/RendererComputePipeline.hpp>
#include <rendering/backend/RendererGraphicsPipeline.hpp>
#include <rendering/backend/rt/RendererRaytracingPipeline.hpp>
#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/RendererFeatures.hpp>

#include <Types.hpp>

namespace hyperion {

extern IRenderingAPI *g_rendering_api;

namespace renderer {

static inline VulkanRenderingAPI *GetRenderingAPI()
{
    return static_cast<VulkanRenderingAPI *>(g_rendering_api);
}

VulkanCommandBuffer::VulkanCommandBuffer(CommandBufferType type)
    : m_type(type),
      m_handle(VK_NULL_HANDLE),
      m_command_pool(VK_NULL_HANDLE)
{
}

VulkanCommandBuffer::~VulkanCommandBuffer()
{
    AssertThrowMsg(m_handle == VK_NULL_HANDLE, "command buffer should have been destroyed");
}

bool VulkanCommandBuffer::IsCreated() const
{
    return m_handle != VK_NULL_HANDLE;
}

RendererResult VulkanCommandBuffer::Create(VkCommandPool command_pool)
{
    if (IsCreated()) {
        AssertThrowMsg(m_command_pool == command_pool, "Command buffer already created with a different command pool");

        HYPERION_RETURN_OK;
    }

    m_command_pool = command_pool;

    return Create();
}

RendererResult VulkanCommandBuffer::Create()
{
    AssertThrow(m_command_pool != VK_NULL_HANDLE);

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
    alloc_info.commandPool = m_command_pool;
    alloc_info.commandBufferCount = 1;

    HYPERION_VK_CHECK_MSG(
        vkAllocateCommandBuffers(GetRenderingAPI()->GetDevice()->GetDevice(), &alloc_info, &m_handle),
        "Failed to allocate command buffer"
    );

    HYPERION_RETURN_OK;
}

RendererResult VulkanCommandBuffer::Destroy()
{
    if (m_handle != VK_NULL_HANDLE) {
        AssertThrow(m_command_pool != VK_NULL_HANDLE);

        vkFreeCommandBuffers(GetRenderingAPI()->GetDevice()->GetDevice(), m_command_pool, 1, &m_handle);
        
        m_handle = VK_NULL_HANDLE;
        m_command_pool = VK_NULL_HANDLE;
    }

    HYPERION_RETURN_OK;
}

RendererResult VulkanCommandBuffer::Begin(const VulkanRenderPass *render_pass)
{
    VkCommandBufferInheritanceInfo inheritance_info { VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO };
    inheritance_info.subpass = 0;
    inheritance_info.framebuffer = VK_NULL_HANDLE;

    VkCommandBufferBeginInfo begin_info { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };

    if (m_type == COMMAND_BUFFER_SECONDARY) {
        if (render_pass == nullptr) {
            return HYP_MAKE_ERROR(RendererError, "Render pass not provided for secondary command buffer!");
        }

        inheritance_info.renderPass = render_pass->GetVulkanHandle();

        begin_info.pInheritanceInfo = &inheritance_info;
        begin_info.flags |= VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
    }

    if (!m_handle) {
        return HYP_MAKE_ERROR(RendererError, "Command buffer not created!");
    }

    HYPERION_VK_CHECK_MSG(
        vkBeginCommandBuffer(m_handle, &begin_info),
        "Failed to begin command buffer"
    );

    HYPERION_RETURN_OK;
}

RendererResult VulkanCommandBuffer::End()
{
    HYPERION_VK_CHECK_MSG(
        vkEndCommandBuffer(m_handle),
        "Failed to end command buffer"
    );

    HYPERION_RETURN_OK;
}

RendererResult VulkanCommandBuffer::Reset()
{
    HYPERION_VK_CHECK_MSG(
        vkResetCommandBuffer(m_handle, 0),
        "Failed to reset command buffer"
    );

    HYPERION_RETURN_OK;
}

RendererResult VulkanCommandBuffer::SubmitPrimary(
    VulkanDeviceQueue *queue,
    VulkanFence *fence,
    VulkanSemaphoreChain *semaphore_chain
)
{
    VkSubmitInfo submit_info { VK_STRUCTURE_TYPE_SUBMIT_INFO };

    if (semaphore_chain != nullptr) {
        submit_info.waitSemaphoreCount = uint32(semaphore_chain->GetWaitSemaphoresView().size());
        submit_info.pWaitSemaphores = semaphore_chain->GetWaitSemaphoresView().data();
        submit_info.signalSemaphoreCount = uint32(semaphore_chain->GetSignalSemaphoresView().size());
        submit_info.pSignalSemaphores = semaphore_chain->GetSignalSemaphoresView().data();
        submit_info.pWaitDstStageMask = semaphore_chain->GetWaitSemaphoreStagesView().data();
    } else {
        submit_info.waitSemaphoreCount = 0;
        submit_info.pWaitSemaphores = nullptr;
        submit_info.signalSemaphoreCount = 0;
        submit_info.pSignalSemaphores = nullptr;
        submit_info.pWaitDstStageMask = nullptr;
    }

    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &m_handle;

    AssertThrow(fence != nullptr);
    AssertThrow(fence->GetVulkanHandle() != VK_NULL_HANDLE);

    HYPERION_VK_CHECK_MSG(
        vkQueueSubmit(queue->queue, 1, &submit_info, fence->GetVulkanHandle()),
        "Failed to submit command"
    );

    HYPERION_RETURN_OK;
}

RendererResult VulkanCommandBuffer::SubmitSecondary(VulkanCommandBuffer *primary)
{
    vkCmdExecuteCommands(
        primary->GetVulkanHandle(),
        1,
        &m_handle
    );

    HYPERION_RETURN_OK;
}

void VulkanCommandBuffer::BindVertexBuffer(const GPUBufferBase *buffer)
{
    AssertThrow(buffer != nullptr);
    AssertThrowMsg(buffer->GetBufferType() == GPUBufferType::MESH_VERTEX_BUFFER, "Not a vertex buffer! Got buffer type: %u", uint32(buffer->GetBufferType()));
    
    const VkBuffer vertex_buffers[] = { static_cast<const VulkanGPUBuffer *>(buffer)->GetVulkanHandle() };
    static const VkDeviceSize offsets[] = { 0 };

    vkCmdBindVertexBuffers(m_handle, 0, 1, vertex_buffers, offsets);
}

void VulkanCommandBuffer::BindIndexBuffer(const GPUBufferBase *buffer, DatumType datum_type)
{
    AssertThrow(buffer != nullptr);
    AssertThrowMsg(buffer->GetBufferType() == GPUBufferType::MESH_INDEX_BUFFER, "Not an index buffer! Got buffer type: %u", uint32(buffer->GetBufferType()));
    
    vkCmdBindIndexBuffer(
        m_handle,
        static_cast<const VulkanGPUBuffer *>(buffer)->GetVulkanHandle(),
        0,
        helpers::ToVkIndexType(datum_type)
    );
}

void VulkanCommandBuffer::DrawIndexed(
    uint32 num_indices,
    uint32 num_instances,
    uint32 instance_index
) const
{
    vkCmdDrawIndexed(
        m_handle,
        num_indices,
        num_instances,
        0,
        0,
        instance_index
    );
}

void VulkanCommandBuffer::DrawIndexedIndirect(
    const GPUBufferBase *buffer,
    uint32 buffer_offset
) const
{
    vkCmdDrawIndexedIndirect(
        m_handle,
        static_cast<const VulkanGPUBuffer *>(buffer)->GetVulkanHandle(),
        buffer_offset,
        1,
        uint32(sizeof(IndirectDrawCommand))
    );
}

void VulkanCommandBuffer::DebugMarkerBegin(const char *marker_name) const
{
    if (Features::dyn_functions.vkCmdDebugMarkerBeginEXT) {
        const VkDebugMarkerMarkerInfoEXT marker {
            .sType       = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT,
            .pNext       = nullptr,
            .pMarkerName = marker_name
        };

        Features::dyn_functions.vkCmdDebugMarkerBeginEXT(m_handle, &marker);
    }
}

void VulkanCommandBuffer::DebugMarkerEnd() const
{
    if (Features::dyn_functions.vkCmdDebugMarkerEndEXT) {
        Features::dyn_functions.vkCmdDebugMarkerEndEXT(m_handle);
    }
}

} // namespace renderer
} // namespace hyperion