/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <VulkanPch.hpp>

#include <Rendering/Vulkan/VulkanFrame.hpp>
#include <Rendering/Vulkan/VulkanFence.hpp>
#include <Rendering/Vulkan/VulkanCommandBuffer.hpp>
#include <Rendering/Vulkan/VulkanRenderInterface.hpp>
#include <Rendering/Vulkan/VulkanRenderPass.hpp>
#include <Rendering/Vulkan/VulkanSwapchain.hpp>
#include <Rendering/Vulkan/VulkanFeatures.hpp>

#include <Rendering/Device.hpp>
#include <Rendering/RenderTypes.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <Framework/EngineStats.hpp>

#include <VulkanFrame.generated.inl>

namespace Hyperion {

extern VulkanRenderInterface RI;

VulkanFrame::VulkanFrame()
    : FrameBase(0),
      m_queueSubmitFence(nullptr),
      m_frameCompleteValue(0)
{
}

VulkanFrame::VulkanFrame(uint32 frameIndex)
    : FrameBase(frameIndex),
      m_queueSubmitFence(nullptr),
      m_frameCompleteValue(0)
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
    m_queueSubmitFence = nullptr;
}

RendererResult VulkanFrame::Create()
{
    if (IsCreated())
    {
        return {};
    }

    const bool useTimeline = RI.GetDevice()->GetFeatures().SupportsTimelineSemaphores()
        && RI.GetRenderConfig().timelineSemaphores;

    if (useTimeline)
    {
        m_frameCompleteSemaphore = MakeHandle<VulkanSemaphore>(VulkanSemaphoreType::TIMELINE);
        CheckResultOrReturn(m_frameCompleteSemaphore->Create());
    }
    else
    {
        m_queueSubmitFence = new VulkanFence();
        m_queueSubmitFence->Create(/* createSignalled */ true);
    }

    return {};
}

void VulkanFrame::OnFrameStart()
{
    FrameBase::OnFrameStart();

    if (m_queueSubmitFence)
    {
        m_queueSubmitFence->Reset();
    }

    if (m_frameCompleteSemaphore.IsValid())
    {
        m_frameCompleteValue = GetFrameCounter() + 1;
    }

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
}

void VulkanFrame::WriteCommandBuffer(VulkanCommandBuffer* commandBuffer)
{
    AssertOnThread(g_renderThread);

    Array<CommandRecorder*, VulkanTempAllocator> commandRecorders;
    commandRecorders.Reserve(4);

    commandRecorders.PushBack(&preRenderCommands);
    commandRecorders.PushBack(&cr);
    commandRecorders.PushBack(&RI.commandRecorderAllocator.root);
    commandRecorders.PushBack(&postRenderCommands);

    for (CommandRecorder* commandRecorder : commandRecorders)
    {
        commandRecorder->Prepare(this);
    }

    if (OnPresent.AnyBound())
    {
        OnPresent(this);
        OnPresent.RemoveAllDetached();
    }

    for (CommandRecorder* commandRecorder : commandRecorders)
    {
        commandRecorder->Execute(commandBuffer);
        commandRecorder->Reset(/* freeMemory */ false);
    }
}

void VulkanFrame::RecreateFence()
{
    delete m_queueSubmitFence;
    m_queueSubmitFence = nullptr;

    const bool useTimeline = RI.GetDevice()->GetFeatures().SupportsTimelineSemaphores()
        && RI.GetRenderConfig().timelineSemaphores;

    if (useTimeline)
    {
        m_frameCompleteSemaphore = MakeHandle<VulkanSemaphore>(VulkanSemaphoreType::TIMELINE);
        m_frameCompleteSemaphore->Create();
    }
    else
    {
        m_queueSubmitFence = new VulkanFence();
        m_queueSubmitFence->Create(/* createSignalled */ true);
    }
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

            const ResourceState currentState = attachment->GetGpuImage()->GetResourceState();
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
