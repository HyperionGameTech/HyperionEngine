/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <VulkanPch.hpp>

#include <rendering/vulkan/VulkanFrame.hpp>
#include <rendering/vulkan/VulkanFence.hpp>
#include <rendering/vulkan/VulkanCommandBuffer.hpp>
#include <rendering/vulkan/VulkanRenderInterface.hpp>
#include <rendering/vulkan/VulkanRenderPass.hpp>
#include <rendering/vulkan/VulkanSwapchain.hpp>

#include <rendering/Device.hpp>
#include <rendering/RenderObject.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <VulkanFrame.generated.inl>

namespace Hyperion {

extern VulkanRenderInterface* g_renderInterface;

VulkanFrame::VulkanFrame()
    : FrameBase(0),
      m_queueSubmitFence(nullptr)
{
}

VulkanFrame::VulkanFrame(uint32 frameIndex)
    : FrameBase(frameIndex),
      m_queueSubmitFence(nullptr)
{
}

VulkanFrame::~VulkanFrame()
{
    for (auto& it : m_swapchainData)
    {
        VulkanSwapchainData& data = it.second;
        delete data.imageAvailableSemaphore;
    }

    delete m_queueSubmitFence;
}

RendererResult VulkanFrame::Create()
{
    if (IsCreated())
    {
        return {};
    }

    m_queueSubmitFence = new VulkanFence();
    m_queueSubmitFence->Create(/* createSignalled */ true);

    return {};
}

void VulkanFrame::OnFrameStart()
{
    FrameBase::OnFrameStart();

    m_queueSubmitFence->Reset();

#ifdef DECLARE_SET_TRACK_FRAME_USAGE
    for (VulkanDescriptorSet* descriptorSet : m_usedDescriptorSets)
    {
        auto it = descriptorSet->GetCurrentFrames().FindAs(this);
        if (it != descriptorSet->GetCurrentFrames().End())
        {
            // Remove the current frame from the descriptor set's current frames
            // This is necessary to ensure that the descriptor set is not used in the next frame
            descriptorSet->GetCurrentFrames().Erase(it);
        }
    }
#endif

    if (OnFrameEnd.AnyBound())
    {
        OnFrameEnd(this);
        OnFrameEnd.RemoveAllDetached();
    }
}

RendererResult VulkanFrame::Submit(
    VulkanDeviceQueue* deviceQueue,
    VulkanCommandBuffer* commandBuffer,
    VulkanSwapchain* swapchain)
{
    AssertOnThread(g_renderThread);

    preRenderQueue.Prepare(this);
    renderQueue.Prepare(this);
    postRenderQueue.Prepare(this);

    if (OnPresent.AnyBound())
    {
        OnPresent(this);
        OnPresent.RemoveAllDetached();
    }

    commandBuffer->Begin();
    preRenderQueue.Execute(commandBuffer);
    renderQueue.Execute(commandBuffer);
    postRenderQueue.Execute(commandBuffer);
    commandBuffer->End();

    VulkanSemaphore* waitSemaphore = nullptr;
    VulkanSemaphore* signalSemaphore = nullptr;

    if (swapchain != nullptr)
    {
        waitSemaphore = GetImageAvailableSemaphore(swapchain, /* createIfNotExist */ true);
        signalSemaphore = swapchain->GetCurrentPresentSemaphore();

        AssertDebug(waitSemaphore != nullptr && signalSemaphore != nullptr);
    }

    return commandBuffer->SubmitPrimary(
        deviceQueue,
        m_queueSubmitFence,
        Span<VulkanSemaphore*>(&waitSemaphore, waitSemaphore ? 1 : 0),
        Span<VulkanSemaphore*>(&signalSemaphore, signalSemaphore ? 1 : 0));
}

void VulkanFrame::RecreateFence()
{
    delete m_queueSubmitFence;

    m_queueSubmitFence = new VulkanFence();
    m_queueSubmitFence->Create(/* createSignalled */ true);
}

void VulkanFrame::RecreateSemaphores(const VulkanSwapchain* swapchain)
{
    Assert(swapchain != nullptr);

    auto it = m_swapchainData.Find(swapchain);
    if (it == m_swapchainData.End())
    {
        VulkanSwapchainData swapchainData;
        swapchainData.swapchainWeak = MakeWeakRef(swapchain);

        m_swapchainData.Insert({ swapchain, std::move(swapchainData) });
        it = m_swapchainData.Find(swapchain);
    }

    VulkanSemaphore*& imageAvailableSemaphore = it->second.imageAvailableSemaphore;

    // reset immediately
    delete imageAvailableSemaphore;

    imageAvailableSemaphore = new VulkanSemaphore();

    RendererResult res = imageAvailableSemaphore->Create();
    Assert(res, "Failed to recreate image available semaphore: {}", res.GetError().GetMessage());
}

VulkanSemaphore* VulkanFrame::GetImageAvailableSemaphore(const VulkanSwapchain* swapchain, bool createIfNotExist)
{
    auto it = m_swapchainData.Find(swapchain);
    if (it == m_swapchainData.End())
    {
        if (!createIfNotExist)
        {
            return nullptr;
        }

        it = m_swapchainData.Emplace(swapchain).first;
        InitVulkanSwapchainData(it->second);
    }

    return it->second.imageAvailableSemaphore;
}

void VulkanFrame::InitVulkanSwapchainData(VulkanSwapchainData& swapchainData)
{
    swapchainData.imageAvailableSemaphore = new VulkanSemaphore();

    RendererResult res = swapchainData.imageAvailableSemaphore->Create();
    Assert(res, "Failed to create image available semaphore: {}", res.GetError().GetMessage());
}

void VulkanFrame::ResetTransientStates()
{
#if 0
    for (VulkanRenderPass* renderPass : m_renderPasses)
    {
        for (VulkanAttachment* attachment : renderPass->GetAttachments())
        {
            AssertDebug(attachment != nullptr);

            if (!attachment)
            {
                continue;
            }

            const ResourceState currentState = attachment->GetImage()->GetResourceState();
        }
    }
#endif

    m_renderPasses.Clear();

    // remove invalid swapchain data
    for (auto it = m_swapchainData.Begin(); it != m_swapchainData.End();)
    {
        const VulkanSwapchain* swapchain = it->first;
        AssertDebug(swapchain != nullptr);

        if (swapchain->GetObjectHeader_Internal()->GetRefCountStrong() == 0)
        {
            // swapchain is destroyed, remove semaphores
            delete it->second.imageAvailableSemaphore;
            it->second.imageAvailableSemaphore = nullptr;

            it = m_swapchainData.Erase(it);
        }
        else
        {
            ++it;
        }
    }
}

} // namespace Hyperion
