/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <VulkanPch.hpp>

#include <Rendering/Vulkan/VulkanAsyncCompute.hpp>
#include <Rendering/Vulkan/VulkanFrame.hpp>
#include <Rendering/Vulkan/VulkanDevice.hpp>
#include <Rendering/Vulkan/VulkanCommandBuffer.hpp>
#include <Rendering/Vulkan/VulkanComputePipeline.hpp>
#include <Rendering/Vulkan/VulkanDescriptorSet.hpp>
#include <Rendering/Vulkan/VulkanGpuBuffer.hpp>
#include <Rendering/Vulkan/VulkanRenderInterface.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

namespace Hyperion {

extern VulkanRenderInterface RI;

VulkanAsyncCompute::VulkanAsyncCompute()
    : m_deviceQueue(nullptr),
      m_isSupported(false),
      m_isSubmitted(false)
{
    m_commandBuffer = new VulkanCommandBuffer();
    m_fence = new VulkanFence();
}

VulkanAsyncCompute::~VulkanAsyncCompute()
{
    if (m_fence != nullptr)
    {
        if (m_isSubmitted && !CheckStatus())
        {
            m_fence->Wait();
        }

        m_fence->Release();
        m_fence = nullptr;

        m_commandBuffer->Release();
        m_commandBuffer = nullptr;
    }
}

bool VulkanAsyncCompute::CheckStatus()
{
    Assert(m_isSupported && m_fence != nullptr);

    if (!m_isSubmitted)
    {
        return true;
    }

    VkResult result = vkGetFenceStatus(RI.GetDevice()->GetDevice(), m_fence->GetVulkanHandle());

    if (result == VK_NOT_READY)
    {
        return false;
    }

    Assert(result == VK_SUCCESS);

    if (result == VK_SUCCESS)
    {
        m_isSubmitted = false;

        return true;
    }

    return false;
}

void VulkanAsyncCompute::Submit()
{
    Assert(CheckStatus());

    m_fence->Wait(true);
    m_fence->Reset();

    m_commandBuffer->Begin();
    cr.Execute(m_commandBuffer);
    m_commandBuffer->End();

    CheckResult(m_commandBuffer->Submit(m_deviceQueue, m_fence, nullptr, nullptr));

    m_isSubmitted = true;
}

void VulkanAsyncCompute::Create()
{
    HYP_SCOPE;

    Assert(RI.GetDevice()->GetQueueFamilyIndices().IsComplete());

    m_deviceQueue = RI.GetDevice()->GetComputeQueue();

    m_isSupported = RI.GetDevice()->GetQueueFamilyIndices().computeFamily.HasValue();

    if (!m_isSupported)
    {
        HYP_LOG(RenderingBackend, Warning, "Dedicated compute queue not supported, using graphics queue for compute operations");

        m_deviceQueue = RI.GetDevice()->GetGraphicsQueue();
    }

    CheckResult(m_commandBuffer->Create(m_deviceQueue->commandPools[0]));
    m_fence->Create(/* createSignalled */ true);
}

} // namespace Hyperion
