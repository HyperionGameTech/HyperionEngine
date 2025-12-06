/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <rendering/vulkan/VulkanCommandBuffer.hpp>
#include <rendering/vulkan/VulkanRenderPass.hpp>
#include <rendering/vulkan/VulkanSemaphore.hpp>
#include <rendering/vulkan/VulkanDevice.hpp>
#include <rendering/vulkan/VulkanRenderBackend.hpp>
#include <rendering/vulkan/VulkanResult.hpp>
#include <rendering/vulkan/VulkanStructs.hpp>
#include <rendering/vulkan/VulkanHelpers.hpp>
#include <rendering/vulkan/VulkanFeatures.hpp>

#include <rendering/RenderComputePipeline.hpp>
#include <rendering/RenderGraphicsPipeline.hpp>
#include <rendering/raytracing/RenderRaytracingPipeline.hpp>

#include <core/logging/Logger.hpp>

#include <VulkanCommandBuffer.generated.inl>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(RenderingBackend);

extern VulkanRenderBackend* g_renderBackend;

static inline VulkanRenderBackend* GetRenderBackend()
{
    return g_renderBackend;
}

VulkanCommandBuffer::VulkanCommandBuffer(VkCommandBufferLevel type)
    : m_type(type),
      m_handle(VK_NULL_HANDLE),
      m_commandPool(VK_NULL_HANDLE),
      m_isInRenderPass(false)
{
}

VulkanCommandBuffer::~VulkanCommandBuffer()
{
    if (m_handle != VK_NULL_HANDLE)
    {
        HYP_GFX_ASSERT(m_commandPool != VK_NULL_HANDLE);

        vkFreeCommandBuffers(GetRenderBackend()->GetDevice()->GetDevice(), m_commandPool, 1, &m_handle);

        m_handle = VK_NULL_HANDLE;
        m_commandPool = VK_NULL_HANDLE;
    }
}

bool VulkanCommandBuffer::IsCreated() const
{
    return m_handle != VK_NULL_HANDLE;
}

RendererResult VulkanCommandBuffer::Create(VkCommandPool commandPool)
{
    if (IsCreated())
    {
        HYP_GFX_ASSERT(m_commandPool == commandPool, "Command buffer already created with a different command pool");

        HYPERION_RETURN_OK;
    }

    m_commandPool = commandPool;

    return Create();
}

RendererResult VulkanCommandBuffer::Create()
{
    HYP_GFX_ASSERT(m_commandPool != VK_NULL_HANDLE);

    VkCommandBufferAllocateInfo allocInfo { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    allocInfo.level = m_type;
    allocInfo.commandPool = m_commandPool;
    allocInfo.commandBufferCount = 1;

    VULKAN_CHECK_MSG(
        vkAllocateCommandBuffers(GetRenderBackend()->GetDevice()->GetDevice(), &allocInfo, &m_handle),
        "Failed to allocate command buffer");

    return {};
}

RendererResult VulkanCommandBuffer::Begin(const VulkanRenderPass* renderPass)
{
    m_boundDescriptorSets.Clear();
    ResetStencilState();

    VkCommandBufferInheritanceInfo inheritanceInfo { VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO };
    inheritanceInfo.subpass = 0;
    inheritanceInfo.framebuffer = VK_NULL_HANDLE;

    VkCommandBufferBeginInfo beginInfo { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };

    if (m_type == VK_COMMAND_BUFFER_LEVEL_SECONDARY)
    {
        if (renderPass == nullptr)
        {
            return HYP_MAKE_ERROR(RendererError, "Render pass not provided for secondary command buffer!");
        }

        inheritanceInfo.renderPass = renderPass->GetVulkanHandle();

        beginInfo.pInheritanceInfo = &inheritanceInfo;
        beginInfo.flags |= VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
    }

    if (!m_handle)
    {
        return HYP_MAKE_ERROR(RendererError, "Command buffer not created!");
    }

    VULKAN_CHECK_MSG(
        vkBeginCommandBuffer(m_handle, &beginInfo),
        "Failed to begin command buffer");

    return {};
}

RendererResult VulkanCommandBuffer::End()
{
    m_boundDescriptorSets.Clear();
    ResetStencilState();

    VULKAN_CHECK_MSG(
        vkEndCommandBuffer(m_handle),
        "Failed to end command buffer");

    return {};
}

RendererResult VulkanCommandBuffer::Reset()
{
    m_boundDescriptorSets.Clear();
    ResetStencilState();

    VULKAN_CHECK_MSG(
        vkResetCommandBuffer(m_handle, 0),
        "Failed to reset command buffer");

    return {};
}

RendererResult VulkanCommandBuffer::SubmitPrimary(
    VulkanDeviceQueue* queue,
    VulkanFence* fence,
    Span<VulkanSemaphore*> waitSemaphores,
    Span<VulkanSemaphore*> signalSemaphores)
{
    AssertOnThread(g_renderThread);

    m_boundDescriptorSets.Clear();
    ResetStencilState();

    VkSemaphore* signalSemaphoresVk = signalSemaphores.Size() > 0 ? (VkSemaphore*)StackAlloc(sizeof(VkSemaphore) * signalSemaphores.Size()) : nullptr;

    for (uint32 i = 0; i < uint32(signalSemaphores.Size()); i++)
    {
        signalSemaphoresVk[i] = signalSemaphores[i]->GetVulkanHandle();
    }

    VkSemaphore* waitSemaphoresVk = waitSemaphores.Size() > 0 ? (VkSemaphore*)StackAlloc(sizeof(VkSemaphore) * waitSemaphores.Size()) : nullptr;
    VkPipelineStageFlags* waitStages = (VkPipelineStageFlags*)StackAlloc(sizeof(VkPipelineStageFlags) * waitSemaphores.Size());

    for (uint32 i = 0; i < uint32(waitSemaphores.Size()); i++)
    {
        waitSemaphoresVk[i] = waitSemaphores[i]->GetVulkanHandle();
        waitStages[i] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    }

    VkSubmitInfo submitInfo { VK_STRUCTURE_TYPE_SUBMIT_INFO };

    if (waitSemaphores.Size() > 0)
    {
        submitInfo.waitSemaphoreCount = uint32(waitSemaphores.Size());
        submitInfo.pWaitSemaphores = waitSemaphoresVk;
        submitInfo.pWaitDstStageMask = waitStages;
    }
    else
    {
        submitInfo.waitSemaphoreCount = 0;
        submitInfo.pWaitSemaphores = nullptr;
        submitInfo.pWaitDstStageMask = nullptr;
    }

    if (signalSemaphores.Size() > 0)
    {
        submitInfo.signalSemaphoreCount = uint32(signalSemaphores.Size());
        submitInfo.pSignalSemaphores = signalSemaphoresVk;
    }
    else
    {
        submitInfo.signalSemaphoreCount = 0;
        submitInfo.pSignalSemaphores = nullptr;
    }

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_handle;

    VULKAN_CHECK(vkQueueSubmit(queue->queue, 1, &submitInfo, fence->GetVulkanHandle()));

    return {};
}

RendererResult VulkanCommandBuffer::SubmitSecondary(VulkanCommandBuffer* primary)
{
    m_boundDescriptorSets.Clear();
    ResetStencilState();

    vkCmdExecuteCommands(
        primary->GetVulkanHandle(),
        1,
        &m_handle);

    HYPERION_RETURN_OK;
}

void VulkanCommandBuffer::BindVertexBuffer(const VulkanGpuBuffer* buffer)
{
    static constexpr VkDeviceSize BindingOffsets[] = { 0 };

    HYP_GFX_ASSERT(buffer != nullptr);
    HYP_GFX_ASSERT(buffer->GetBufferType() == GpuBufferType::MESH_VERTEX_BUFFER, "Not a vertex buffer! Got buffer type: %u", uint32(buffer->GetBufferType()));

    const VkBuffer vertexBuffers[] = { static_cast<const VulkanGpuBuffer*>(buffer)->GetVulkanHandle() };

    vkCmdBindVertexBuffers(m_handle, 0, 1, vertexBuffers, BindingOffsets);
}

void VulkanCommandBuffer::BindIndexBuffer(const VulkanGpuBuffer* buffer, GpuElemType elemType)
{
    HYP_GFX_ASSERT(buffer != nullptr);
    HYP_GFX_ASSERT(buffer->GetBufferType() == GpuBufferType::MESH_INDEX_BUFFER, "Not an index buffer! Got buffer type: %u", uint32(buffer->GetBufferType()));

    vkCmdBindIndexBuffer(
        m_handle,
        static_cast<const VulkanGpuBuffer*>(buffer)->GetVulkanHandle(),
        0,
        ToVkIndexType(elemType));
}

void VulkanCommandBuffer::DrawIndexed(
    uint32 numIndices,
    uint32 numInstances,
    uint32 instanceIndex) const
{
    vkCmdDrawIndexed(
        m_handle,
        numIndices,
        numInstances,
        0,
        0,
        instanceIndex);
}

void VulkanCommandBuffer::DrawIndexedIndirect(
    const VulkanGpuBuffer* buffer,
    uint32 bufferOffset) const
{
    vkCmdDrawIndexedIndirect(
        m_handle,
        static_cast<const VulkanGpuBuffer*>(buffer)->GetVulkanHandle(),
        bufferOffset,
        1,
        uint32(sizeof(VkDrawIndexedIndirectCommand)));
}

void VulkanCommandBuffer::DebugMarkerBegin(const char* markerName) const
{
#ifdef HYP_DEBUG_MODE
    if (g_vulkanDynamicFunctions->vkCmdDebugMarkerBeginEXT)
    {
        const VkDebugMarkerMarkerInfoEXT marker {
            .sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT,
            .pNext = nullptr,
            .pMarkerName = markerName
        };

        g_vulkanDynamicFunctions->vkCmdDebugMarkerBeginEXT(m_handle, &marker);
    }
#endif
}

void VulkanCommandBuffer::DebugMarkerEnd() const
{
#ifdef HYP_DEBUG_MODE
    if (g_vulkanDynamicFunctions->vkCmdDebugMarkerEndEXT)
    {
        g_vulkanDynamicFunctions->vkCmdDebugMarkerEndEXT(m_handle);
    }
#endif
}

} // namespace hyperion
