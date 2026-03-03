/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <VulkanPch.hpp>

#include <rendering/vulkan/VulkanRenderPass.hpp>
#include <rendering/vulkan/VulkanFramebuffer.hpp>
#include <rendering/vulkan/VulkanCommandBuffer.hpp>
#include <rendering/vulkan/VulkanDevice.hpp>
#include <rendering/vulkan/VulkanRenderInterface.hpp>
#include <rendering/vulkan/VulkanFrame.hpp>
#include <rendering/vulkan/VulkanMemory.hpp>
#include <rendering/vulkan/VulkanResult.hpp>
#include <rendering/vulkan/VulkanHelpers.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <VulkanRenderPass.generated.inl>

namespace Hyperion {

extern VulkanRenderInterface* g_renderInterface;

VulkanRenderPass::VulkanRenderPass()
    : VulkanRenderPass(RenderTargetDesc {})
{
}

VulkanRenderPass::VulkanRenderPass(const RenderTargetDesc& renderTargetDesc)
    : m_renderTargetDesc(renderTargetDesc),
      m_handle(VK_NULL_HANDLE),
      m_recordingFramebuffer(nullptr)
{
}

VulkanRenderPass::VulkanRenderPass(VulkanRenderPass&& other) noexcept
    : m_renderTargetDesc(other.m_renderTargetDesc),
      m_dependencies(std::move(other.m_dependencies)),
      m_vkClearValues(std::move(other.m_vkClearValues)),
      m_handle(other.m_handle),
      m_recordingFramebuffer(other.m_recordingFramebuffer)
{
    other.m_handle = VK_NULL_HANDLE;
    other.m_recordingFramebuffer = nullptr;
}

VulkanRenderPass& VulkanRenderPass::operator=(VulkanRenderPass&& other) noexcept
{
    if (m_handle != VK_NULL_HANDLE)
    {
        EnqueueDeletion(FunctionWrapper<Proc<void()>>([handle = m_handle]()
            {
                vkDestroyRenderPass(g_renderInterface->GetDevice()->GetDevice(), handle, nullptr);
            }));

        m_handle = VK_NULL_HANDLE;
    }

    m_renderTargetDesc = other.m_renderTargetDesc;

    m_dependencies = std::move(other.m_dependencies);

    m_vkClearValues = std::move(other.m_vkClearValues);

    m_handle = other.m_handle;
    other.m_handle = VK_NULL_HANDLE;

    m_recordingFramebuffer = other.m_recordingFramebuffer;
    other.m_recordingFramebuffer = nullptr;

    return *this;
}

VulkanRenderPass::~VulkanRenderPass()
{
    if (m_handle != VK_NULL_HANDLE)
    {
        EnqueueDeletion(FunctionWrapper<Proc<void()>>([handle = m_handle]()
            {
                vkDestroyRenderPass(g_renderInterface->GetDevice()->GetDevice(), handle, nullptr);
            }));

        m_handle = VK_NULL_HANDLE;
    }
}

void VulkanRenderPass::CreateDependencies()
{
    m_dependencies.Clear();

    Optional<VkSubpassDependency> loadDependency;
    Optional<VkSubpassDependency> storeDependency;

    switch (m_renderTargetDesc.renderPassMode)
    {
    case RenderPassMode::Present:
        loadDependency = VkSubpassDependency {
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
        };

        break;
    case RenderPassMode::RenderTarget:
    {
        for (uint32 attachmentIdx = 0; attachmentIdx < m_renderTargetDesc.numAttachments; attachmentIdx++)
        {
            const AttachmentDesc& attachmentDesc = m_renderTargetDesc.attachments[attachmentIdx];

            switch (attachmentDesc.loadOp)
            {
            case LoadOperation::CLEAR: // fallthrough
            case LoadOperation::LOAD:
                if (!loadDependency.HasValue())
                {
                    loadDependency = VkSubpassDependency {
                        .srcSubpass = VK_SUBPASS_EXTERNAL,
                        .dstSubpass = 0,
                        .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
                    };
                }

                if (TextureUtils::IsDepthFormat(attachmentDesc.format))
                {
                    loadDependency->dstStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
                    loadDependency->dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

                    loadDependency->srcStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
                    loadDependency->srcAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;

                    if (attachmentDesc.loadOp == LoadOperation::LOAD)
                    {
                        loadDependency->dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
                    }
                }
                else
                {
                    loadDependency->dstStageMask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                    loadDependency->dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

                    loadDependency->srcStageMask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                    loadDependency->srcAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;

                    if (attachmentDesc.loadOp == LoadOperation::LOAD)
                    {
                        loadDependency->dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
                    }
                }
                break;
            default:
                break;
            }

            switch (attachmentDesc.storeOp)
            {
            case StoreOperation::STORE:
                if (!storeDependency.HasValue())
                {
                    storeDependency = VkSubpassDependency {
                        .srcSubpass = 0,
                        .dstSubpass = VK_SUBPASS_EXTERNAL,
                        .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
                    };
                }

                if (TextureUtils::IsDepthFormat(attachmentDesc.format))
                {
                    storeDependency->srcStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
                    storeDependency->srcAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

                    storeDependency->dstStageMask |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT; /// \todo : revisit if compute bit needed if we change SSR to be fragment shader
                    storeDependency->dstAccessMask |= VK_ACCESS_SHADER_READ_BIT;
                }
                else
                {
                    storeDependency->srcStageMask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                    storeDependency->srcAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

                    storeDependency->dstStageMask |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT; /// \todo : revisit if compute bit needed if we change SSR to be fragment shader
                    storeDependency->dstAccessMask |= VK_ACCESS_SHADER_READ_BIT;
                }

                break;
            }
        }

        break;
    }
    default:
        HYP_UNREACHABLE();
    }

    if (loadDependency.HasValue())
    {
        m_dependencies.PushBack(*loadDependency);
    }

    if (storeDependency.HasValue())
    {
        m_dependencies.PushBack(*storeDependency);
    }
}

RendererResult VulkanRenderPass::Create()
{
    CreateDependencies();

    Array<VkAttachmentDescription, VulkanTempAllocator> vkAttachmentDescriptions;
    vkAttachmentDescriptions.Reserve(m_renderTargetDesc.numAttachments);

    VkAttachmentReference depthAttachmentReference {};
    Array<VkAttachmentReference, VulkanTempAllocator> colorAttachmentReferences;

    VkSubpassDescription subpassDescription {};
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.pDepthStencilAttachment = nullptr;

    for (uint32 attachmentIdx = 0; attachmentIdx < m_renderTargetDesc.numAttachments; attachmentIdx++)
    {
        const AttachmentDesc& attachmentDesc = m_renderTargetDesc.attachments[attachmentIdx];

        uint32 attachmentIndex = uint32(vkAttachmentDescriptions.Size());
        vkAttachmentDescriptions.PushBack(ToVkAttachmentDescription(attachmentDesc, m_renderTargetDesc.renderPassMode));

        if (TextureUtils::IsDepthFormat(attachmentDesc.format))
        {
            depthAttachmentReference = ToVkAttachmentReference(attachmentIndex, attachmentDesc);
            subpassDescription.pDepthStencilAttachment = &depthAttachmentReference;

            VkClearValue& clearValue = m_vkClearValues.EmplaceBack();
            clearValue.depthStencil = { 1.0f, 0 };
        }
        else
        {
            colorAttachmentReferences.PushBack(ToVkAttachmentReference(attachmentIndex, attachmentDesc));

            VkClearValue& clearValue = m_vkClearValues.EmplaceBack();
            clearValue.color = {
                .float32 = {
                    attachmentDesc.clearColor[0],
                    attachmentDesc.clearColor[1],
                    attachmentDesc.clearColor[2],
                    attachmentDesc.clearColor[3]
                }
            };
        }
    }

    subpassDescription.colorAttachmentCount = uint32(colorAttachmentReferences.Size());
    subpassDescription.pColorAttachments = colorAttachmentReferences.Data();

    // Create the actual renderpass
    VkRenderPassCreateInfo renderPassInfo { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
    renderPassInfo.attachmentCount = uint32(vkAttachmentDescriptions.Size());
    renderPassInfo.pAttachments = vkAttachmentDescriptions.Data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpassDescription;
    renderPassInfo.dependencyCount = uint32(m_dependencies.Size());
    renderPassInfo.pDependencies = m_dependencies.Data();

    uint32 multiviewViewMask = 0;
    uint32 multiviewCorrelationMask = 0;

    VkRenderPassMultiviewCreateInfo multiviewInfo { VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO };
    multiviewInfo.subpassCount = 1;
    multiviewInfo.pViewMasks = &multiviewViewMask;
    multiviewInfo.pCorrelationMasks = &multiviewCorrelationMask;
    multiviewInfo.correlationMaskCount = 1;

    if (IsMultiview())
    {
        for (uint32 i = 0; i < m_renderTargetDesc.numLayers; i++)
        {
            multiviewViewMask |= 1 << i;
            multiviewCorrelationMask |= 1 << i;
        }

        renderPassInfo.pNext = &multiviewInfo;
    }

    VULKAN_CHECK(vkCreateRenderPass(g_renderInterface->GetDevice()->GetDevice(), &renderPassInfo, nullptr, &m_handle));

    return {};
}

void VulkanRenderPass::Begin(VulkanCommandBuffer* cmd, VulkanFramebuffer* framebuffer)
{
    if (m_recordingFramebuffer)
    {
        return;
    }

    Assert(!cmd->IsInRenderPass());

    Assert(framebuffer != nullptr);
    Assert(m_handle != VK_NULL_HANDLE);

    VkRenderPassBeginInfo renderPassInfo { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    renderPassInfo.renderPass = m_handle;
    renderPassInfo.framebuffer = framebuffer->GetVulkanHandle();
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = VkExtent2D { framebuffer->GetWidth(), framebuffer->GetHeight() };
    renderPassInfo.clearValueCount = uint32(m_vkClearValues.Size());
    renderPassInfo.pClearValues = m_vkClearValues.Data();

    VulkanFrame* currentFrame = g_renderInterface->GetCurrentFrame();
    if (currentFrame != nullptr)
    {
        currentFrame->AddRenderPass(this);
    }

    // transition render targets to initial layout for render passes
    for (uint32 attachmentIndex = 0; attachmentIndex < framebuffer->NumAttachments(); attachmentIndex++)
    {
        VulkanAttachment* attachment = framebuffer->GetAttachment(attachmentIndex);
        AssertDebug(attachment != nullptr);

        const AttachmentDesc& attachmentDesc = attachment->GetAttachmentDesc();

        VulkanGpuImage* image = attachment->GetImage();

        const TextureDesc& textureDesc = image->GetTextureDesc();

        const bool isDepthStencil = textureDesc.IsDepthStencil();
        const bool hasStencil = TextureUtils::HasStencilComponent(textureDesc.format);

        if (isDepthStencil && hasStencil)
        {
            const bool transitionDepth = !attachmentDesc.onlyStencil && image->GetResourceState() != RS_RENDER_TARGET;
            const bool transitionStencil = !attachmentDesc.onlyDepth && image->GetStencilState() != RS_RENDER_TARGET;

            if (transitionDepth ^ transitionStencil)
            {
                if (transitionDepth)
                {
                    image->InsertBarrier(cmd, RS_RENDER_TARGET, ShaderModuleType::Pixel, /* onlyDepth */ true, /* onlyStencil */ false);
                }

                if (transitionStencil)
                {
                    image->InsertBarrier(cmd, RS_RENDER_TARGET, ShaderModuleType::Pixel, /* onlyDepth */ false, /* onlyStencil */ true);
                }
            }
            else if (transitionDepth && transitionStencil)
            {
                image->InsertBarrier(cmd, RS_RENDER_TARGET, ShaderModuleType::Pixel);
            }

            continue;
        }

        if (image->GetResourceState() != RS_RENDER_TARGET)
        {
            image->InsertBarrier(cmd, RS_RENDER_TARGET, ShaderModuleType::Pixel);
        }
    }

    vkCmdBeginRenderPass(cmd->GetVulkanHandle(), &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    m_recordingFramebuffer = framebuffer;
}

void VulkanRenderPass::End(VulkanCommandBuffer* cmd)
{
    if (!m_recordingFramebuffer)
    {
        return;
    }

    vkCmdEndRenderPass(cmd->GetVulkanHandle());

    // handle implicit layout transitions after render pass end
    for (uint32 attachmentIndex = 0; attachmentIndex < m_recordingFramebuffer->NumAttachments(); attachmentIndex++)
    {
        VulkanAttachment* attachment = m_recordingFramebuffer->GetAttachment(attachmentIndex);
        AssertDebug(attachment != nullptr);

        const ResourceState newState = PostRenderResourceStates[uint32(m_renderTargetDesc.renderPassMode)];

        if (attachment->IsDepthAttachment())
        {
            if (attachment->GetAttachmentDesc().onlyStencil)
            {
                attachment->GetImage()->SetStencilState(newState);

                continue;
            }

            if (attachment->GetAttachmentDesc().onlyDepth)
            {
                const ResourceState stencilState = attachment->GetImage()->GetStencilState();

                attachment->GetImage()->SetResourceState(newState);
                attachment->GetImage()->SetStencilState(stencilState);

                continue;
            }
        }

        attachment->GetImage()->SetResourceState(newState);
    }

    m_recordingFramebuffer = nullptr;
}

} // namespace Hyperion
