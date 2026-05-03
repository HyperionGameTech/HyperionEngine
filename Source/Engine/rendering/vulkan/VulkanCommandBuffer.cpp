/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <VulkanPch.hpp>

#include <rendering/vulkan/VulkanCommandBuffer.hpp>
#include <rendering/vulkan/VulkanRenderPass.hpp>
#include <rendering/vulkan/VulkanSemaphore.hpp>
#include <rendering/vulkan/VulkanDevice.hpp>
#include <rendering/vulkan/VulkanRenderInterface.hpp>
#include <rendering/vulkan/VulkanResult.hpp>
#include <rendering/vulkan/VulkanStructs.hpp>
#include <rendering/vulkan/VulkanHelpers.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <VulkanCommandBuffer.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(RenderingBackend);

extern VulkanRenderInterface RI;

VulkanCommandBuffer::VulkanCommandBuffer()
    : m_handle(VK_NULL_HANDLE),
      m_commandPool(VK_NULL_HANDLE),
      m_isRecording(false),
      m_renderPass(nullptr),
      m_boundGraphicsPipeline(nullptr),
      m_boundComputePipeline(nullptr),
      m_boundRayTracingPipeline(nullptr)
{
}

VulkanCommandBuffer::VulkanCommandBuffer(VulkanCommandBuffer&& other) noexcept
    : m_handle(other.m_handle),
      m_commandPool(other.m_commandPool),
      m_boundDescriptorSets(std::move(other.m_boundDescriptorSets)),
      m_isRecording(other.m_isRecording),
      m_renderPass(other.m_renderPass),
      m_boundGraphicsPipeline(other.m_boundGraphicsPipeline),
      m_boundComputePipeline(other.m_boundComputePipeline),
      m_boundRayTracingPipeline(other.m_boundRayTracingPipeline)
{
    other.m_handle = VK_NULL_HANDLE;
    other.m_commandPool = VK_NULL_HANDLE;
    other.m_isRecording = false;
    other.m_renderPass = nullptr;
    other.m_boundGraphicsPipeline = nullptr;
    other.m_boundComputePipeline = nullptr;
    other.m_boundRayTracingPipeline = nullptr;
}

VulkanCommandBuffer& VulkanCommandBuffer::operator=(VulkanCommandBuffer&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    if (m_handle != VK_NULL_HANDLE)
    {
        Assert(m_commandPool != VK_NULL_HANDLE);

        EnqueueDeletion(FunctionWrapper<Proc<void()>>([commandPool = m_commandPool, handle = m_handle]() -> void
            {
                vkFreeCommandBuffers(RI.GetDevice()->GetDevice(), commandPool, 1, &handle);
            }));
    }

    m_handle = other.m_handle;
    m_commandPool = other.m_commandPool;
    m_boundDescriptorSets = std::move(other.m_boundDescriptorSets);
    m_isRecording = other.m_isRecording;
    m_renderPass = other.m_renderPass;
    m_boundGraphicsPipeline = other.m_boundGraphicsPipeline;
    m_boundComputePipeline = other.m_boundComputePipeline;
    m_boundRayTracingPipeline = other.m_boundRayTracingPipeline;

    other.m_handle = VK_NULL_HANDLE;
    other.m_commandPool = VK_NULL_HANDLE;
    other.m_isRecording = false;
    other.m_renderPass = nullptr;
    other.m_boundGraphicsPipeline = nullptr;
    other.m_boundComputePipeline = nullptr;
    other.m_boundRayTracingPipeline = nullptr;

    return *this;
}

VulkanCommandBuffer::~VulkanCommandBuffer()
{
    if (m_handle != VK_NULL_HANDLE)
    {
        Assert(m_commandPool != VK_NULL_HANDLE);

        EnqueueDeletion(FunctionWrapper<Proc<void()>>([commandPool = m_commandPool, handle = m_handle]() -> void
            {
                vkFreeCommandBuffers(RI.GetDevice()->GetDevice(), commandPool, 1, &handle);
            }));

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
        Assert(m_commandPool == commandPool, "Command buffer already created with a different command pool");

        return {};
    }

    m_commandPool = commandPool;

    return Create();
}

RendererResult VulkanCommandBuffer::Create()
{
    Assert(m_commandPool != VK_NULL_HANDLE);

    VkCommandBufferAllocateInfo allocInfo { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = m_commandPool;
    allocInfo.commandBufferCount = 1;

    VULKAN_CHECK_MSG(
        vkAllocateCommandBuffers(RI.GetDevice()->GetDevice(), &allocInfo, &m_handle),
        "Failed to allocate command buffer");

    return {};
}

void VulkanCommandBuffer::Begin()
{
    Assert(!m_isRecording, "Command buffer is already recording!");
    Assert(m_handle != VK_NULL_HANDLE, "Command buffer must be created before it can be begun!");

    m_boundDescriptorSets.Clear();
    m_renderPass = nullptr;
    m_boundGraphicsPipeline = nullptr;
    m_boundComputePipeline = nullptr;
    m_boundRayTracingPipeline = nullptr;

    VkCommandBufferInheritanceInfo inheritanceInfo { VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO };
    inheritanceInfo.subpass = 0;
    inheritanceInfo.framebuffer = VK_NULL_HANDLE;

    VkCommandBufferBeginInfo beginInfo { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    Assert(vkBeginCommandBuffer(m_handle, &beginInfo) == VK_SUCCESS, "Failed to begin command buffer");

    m_isRecording = true;
}

void VulkanCommandBuffer::End()
{
    Assert(m_isRecording, "Command buffer is not recording!");

    Assert(vkEndCommandBuffer(m_handle) == VK_SUCCESS, "Failed to end command buffer");

    m_isRecording = false;
}

void VulkanCommandBuffer::Reset()
{
    Assert(!m_isRecording, "Cannot reset command buffer while it is in recording state!");

    m_boundDescriptorSets.Clear();
    m_renderPass = nullptr;
    m_boundGraphicsPipeline = nullptr;
    m_boundComputePipeline = nullptr;
    m_boundRayTracingPipeline = nullptr;

    Assert(vkResetCommandBuffer(m_handle, 0), "Failed to reset command buffer");
}

RendererResult VulkanCommandBuffer::Submit(
    VulkanDeviceQueue* queue,
    VulkanFence* fence,
    Span<VulkanSemaphore*> waitSemaphores,
    Span<VulkanSemaphore*> signalSemaphores)
{
    AssertOnThread(g_renderThread);

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

    if (fence != nullptr)
    {
        Assert(!fence->isSubmitted);
        fence->isSubmitted = true;
    }

    VULKAN_CHECK(vkQueueSubmit(queue->queue, 1, &submitInfo, fence ? fence->GetVulkanHandle() : VK_NULL_HANDLE));

    return {};
}

void VulkanCommandBuffer::BindVertexBuffer(const VulkanGpuBuffer* buffer)
{
    static constexpr VkDeviceSize BindingOffsets[] = { 0 };

    AssertDebug(buffer != nullptr);
    AssertDebug(buffer->GetBufferType() == GpuBufferType::VertexBuffer, "Not a vertex buffer! Got buffer type: %u", uint32(buffer->GetBufferType()));

    const VkBuffer vertexBuffers[] = { buffer->GetVulkanHandle() };

    vkCmdBindVertexBuffers(m_handle, 0, 1, vertexBuffers, BindingOffsets);
}

void VulkanCommandBuffer::BindIndexBuffer(const VulkanGpuBuffer* buffer, GpuElemType elemType)
{
    AssertDebug(buffer != nullptr);
    AssertDebug(buffer->GetBufferType() == GpuBufferType::IndexBuffer, "Not an index buffer! Got buffer type: %u", uint32(buffer->GetBufferType()));

    vkCmdBindIndexBuffer(
        m_handle,
        buffer->GetVulkanHandle(),
        0,
        ToVkIndexType(elemType));
}

void VulkanCommandBuffer::DrawIndexed(
    uint32 numIndices,
    uint32 numInstances,
    uint32 instanceIndex) const
{
    AssertDebug(m_renderPass && m_boundGraphicsPipeline);

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
    AssertDebug(m_renderPass && m_boundGraphicsPipeline);

    vkCmdDrawIndexedIndirect(
        m_handle,
        static_cast<const VulkanGpuBuffer*>(buffer)->GetVulkanHandle(),
        bufferOffset,
        1,
        uint32(sizeof(VkDrawIndexedIndirectCommand)));
}

void VulkanCommandBuffer::DebugMarkerBegin(const char* markerName) const
{
#if HYP_DEBUG_MODE
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
#if HYP_DEBUG_MODE
    if (g_vulkanDynamicFunctions->vkCmdDebugMarkerEndEXT)
    {
        g_vulkanDynamicFunctions->vkCmdDebugMarkerEndEXT(m_handle);
    }
#endif
}

} // namespace Hyperion
