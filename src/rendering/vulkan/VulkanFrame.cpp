/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <VulkanPch.hpp>

#include <rendering/vulkan/VulkanFrame.hpp>
#include <rendering/vulkan/VulkanFence.hpp>
#include <rendering/vulkan/VulkanCommandBuffer.hpp>
#include <rendering/vulkan/VulkanRenderBackend.hpp>
#include <rendering/vulkan/VulkanRenderPass.hpp>
#include <rendering/vulkan/VulkanSwapchain.hpp>

#include <rendering/Device.hpp>
#include <rendering/RenderObject.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <VulkanFrame.generated.inl>

namespace hyperion {

extern VulkanRenderBackend* g_renderBackend;

static inline VulkanRenderBackend* GetRenderBackend()
{
    return g_renderBackend;
}

VulkanFrame::VulkanFrame()
    : FrameBase(0)
{
}

VulkanFrame::VulkanFrame(uint32 frameIndex)
    : FrameBase(frameIndex)
{
    FrameBase::m_frameIndex = frameIndex;
}

VulkanFrame::~VulkanFrame()
{
    for (auto& it : m_swapchainSemaphores)
    {
        VulkanSwapchainSemaphores& semaphores = it.second;
        SafeDelete(std::move(semaphores.imageAvailableSemaphore));
        SafeDelete(std::move(semaphores.renderFinishedSemaphore));
    }

    SafeDelete(std::move(m_queueSubmitFence));
}

RendererResult VulkanFrame::Create()
{
    if (IsCreated())
    {
        return {};
    }

    m_queueSubmitFence = CreateObject<VulkanFence>();
    HYP_GFX_CHECK(m_queueSubmitFence->Create());

    return {};
}

RendererResult VulkanFrame::ResetFrameState()
{
    RendererResult result;

    HYPERION_PASS_ERRORS(m_queueSubmitFence->Reset(), result);

#ifdef HYP_DESCRIPTOR_SET_TRACK_FRAME_USAGE
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

    m_usedDescriptorSets.Clear();

    if (OnFrameEnd.AnyBound())
    {
        OnFrameEnd(this);
        OnFrameEnd.RemoveAllDetached();
    }

    return result;
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

    UpdateUsedDescriptorSets();

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
        signalSemaphore = GetRenderFinishedSemaphore(swapchain, /* createIfNotExist */ true);

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
    if (m_queueSubmitFence.IsValid())
    {
        SafeDelete(std::move(m_queueSubmitFence));
    }

    m_queueSubmitFence = CreateObject<VulkanFence>();

    RendererResult res = m_queueSubmitFence->Create();
    Assert(res, "Failed to recreate frame fence: {}", res.GetError().GetMessage());
}

void VulkanFrame::RecreateSemaphores(const VulkanSwapchain* swapchain)
{
    Assert(swapchain != nullptr);

    auto it = m_swapchainSemaphores.Find(swapchain);
    if (it == m_swapchainSemaphores.End())
    {
        VulkanSwapchainSemaphores semaphores;
        semaphores.swapchainWeak = MakeWeakRef(swapchain);

        m_swapchainSemaphores.Insert({ swapchain, std::move(semaphores) });
        it = m_swapchainSemaphores.Find(swapchain);
    }

    VulkanSemaphoreRef& imageAvailableSemaphore = it->second.imageAvailableSemaphore;
    VulkanSemaphoreRef& renderFinishedSemaphore = it->second.renderFinishedSemaphore;

    // reset immediately
    imageAvailableSemaphore.Reset();
    renderFinishedSemaphore.Reset();

    imageAvailableSemaphore = CreateObject<VulkanSemaphore>();
    renderFinishedSemaphore = CreateObject<VulkanSemaphore>();

    RendererResult res = imageAvailableSemaphore->Create();
    Assert(res, "Failed to recreate image available semaphore: {}", res.GetError().GetMessage());

    res = renderFinishedSemaphore->Create();
    Assert(res, "Failed to recreate render finished semaphore: {}", res.GetError().GetMessage());
}

VulkanSemaphore* VulkanFrame::GetImageAvailableSemaphore(const VulkanSwapchain* swapchain, bool createIfNotExist)
{
    auto it = m_swapchainSemaphores.Find(swapchain);
    if (it == m_swapchainSemaphores.End())
    {
        if (!createIfNotExist)
        {
            return nullptr;
        }

        it = m_swapchainSemaphores.Emplace(swapchain).first;
        InitVulkanSwapchainSemaphores(it->second);
    }

    return it->second.imageAvailableSemaphore;
}

VulkanSemaphore* VulkanFrame::GetRenderFinishedSemaphore(const VulkanSwapchain* swapchain, bool createIfNotExist)
{
    auto it = m_swapchainSemaphores.Find(swapchain);
    if (it == m_swapchainSemaphores.End())
    {
        if (!createIfNotExist)
        {
            return nullptr;
        }

        it = m_swapchainSemaphores.Emplace(swapchain).first;
        InitVulkanSwapchainSemaphores(it->second);
    }

    return it->second.renderFinishedSemaphore;
}

void VulkanFrame::InitVulkanSwapchainSemaphores(VulkanSwapchainSemaphores& semaphores)
{
    semaphores.imageAvailableSemaphore = CreateObject<VulkanSemaphore>();
    semaphores.renderFinishedSemaphore = CreateObject<VulkanSemaphore>();

    RendererResult res = semaphores.imageAvailableSemaphore->Create();
    Assert(res, "Failed to create image available semaphore: {}", res.GetError().GetMessage());

    res = semaphores.renderFinishedSemaphore->Create();
    Assert(res, "Failed to create render finished semaphore: {}", res.GetError().GetMessage());
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

    // remove invalid swapchain semaphores
    for (auto it = m_swapchainSemaphores.Begin(); it != m_swapchainSemaphores.End();)
    {
        const VulkanSwapchain* swapchain = it->first;
        AssertDebug(swapchain != nullptr);

        if (swapchain->GetObjectHeader_Internal()->GetRefCountStrong() == 0)
        {
            // swapchain is destroyed, remove semaphores
            SafeDelete(std::move(it->second.imageAvailableSemaphore));
            SafeDelete(std::move(it->second.renderFinishedSemaphore));

            it = m_swapchainSemaphores.Erase(it);
        }
        else
        {
            ++it;
        }
    }
}

} // namespace hyperion
