/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <rendering/vulkan/VulkanRenderPass.hpp>
#include <rendering/vulkan/VulkanFramebuffer.hpp>
#include <rendering/vulkan/VulkanCommandBuffer.hpp>
#include <rendering/vulkan/VulkanDevice.hpp>
#include <rendering/vulkan/VulkanRenderBackend.hpp>
#include <rendering/vulkan/VulkanMemory.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <VulkanRenderPass.generated.inl>

namespace hyperion {

extern IRenderBackend* g_renderBackend;

static inline VulkanRenderBackend* GetRenderBackend()
{
    return static_cast<VulkanRenderBackend*>(g_renderBackend);
}

VulkanRenderPass::VulkanRenderPass(RenderTargetType renderTargetType, RenderPassMode mode)
    : m_renderTargetType(renderTargetType),
      m_mode(mode),
      m_handle(VK_NULL_HANDLE),
      m_numMultiviewLayers(0),
      m_isRecording(false)
{
}

VulkanRenderPass::VulkanRenderPass(RenderTargetType renderTargetType, RenderPassMode mode, uint32 numMultiviewLayers)
    : m_renderTargetType(renderTargetType),
      m_mode(mode),
      m_handle(VK_NULL_HANDLE),
      m_numMultiviewLayers(numMultiviewLayers),
      m_isRecording(false)
{
}

VulkanRenderPass::~VulkanRenderPass()
{
    vkDestroyRenderPass(GetRenderBackend()->GetDevice()->GetDevice(), m_handle, nullptr);
    m_handle = VK_NULL_HANDLE;

    SafeDelete(std::move(m_renderPassAttachments));
}

void VulkanRenderPass::CreateDependencies()
{
    m_dependencies.Clear();

    Optional<VkSubpassDependency> loadDependency;
    Optional<VkSubpassDependency> storeDependency;

    switch (m_renderTargetType)
    {
    case RTT_PRESENT:
        loadDependency = VkSubpassDependency {
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
        };

        break;
    case RTT_SHADER_RESOURCE: // fallthrough
    case RTT_RENDER_TARGET:
    {
        for (const VulkanAttachmentRef& attachment : m_renderPassAttachments)
        {
            switch (attachment->GetLoadOperation())
            {
            case LoadOperation::LOAD:
                if (!loadDependency.HasValue())
                {
                    loadDependency = VkSubpassDependency {
                        .srcSubpass = VK_SUBPASS_EXTERNAL,
                        .dstSubpass = 0,
                        .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
                    };
                }

                if (attachment->IsDepthAttachment())
                {
                    loadDependency->srcStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
                    loadDependency->srcAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

                    loadDependency->dstStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
                    loadDependency->dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                }
                else
                {
                    loadDependency->srcStageMask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                    loadDependency->srcAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

                    loadDependency->dstStageMask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                    loadDependency->dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                }

                break;
            default:
                break;
            }

            switch (attachment->GetStoreOperation())
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

                if (attachment->IsDepthAttachment())
                {
                    storeDependency->srcStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
                    storeDependency->srcAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

                    storeDependency->dstStageMask |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT; // @TODO: revisit if compute bit needed if we change SSR to be fragment shader
                    storeDependency->dstAccessMask |= VK_ACCESS_SHADER_READ_BIT;
                }
                else
                {
                    storeDependency->srcStageMask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                    storeDependency->srcAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

                    storeDependency->dstStageMask |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT; // @TODO: revisit if compute bit needed if we change SSR to be fragment shader
                    storeDependency->dstAccessMask |= VK_ACCESS_SHADER_READ_BIT;
                }

                break;
            }
        }

        break;
    }
    default:
        HYP_FAIL("Unsupported RenderTargetType value {}", int(m_renderTargetType));
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

void VulkanRenderPass::AddAttachment(VulkanAttachmentRef attachment)
{
    m_renderPassAttachments.PushBack(std::move(attachment));
}

bool VulkanRenderPass::RemoveAttachment(const VulkanAttachment* attachment)
{
    const auto it = m_renderPassAttachments.FindAs(attachment);

    if (it == m_renderPassAttachments.End())
    {
        return false;
    }

    SafeDelete(std::move(*it));

    m_renderPassAttachments.Erase(it);

    return true;
}

HYP_DISABLE_OPTIMIZATION;
RendererResult VulkanRenderPass::Create()
{
    CreateDependencies();

    Array<VkAttachmentDescription, VulkanAllocator> attachmentDescriptions;
    attachmentDescriptions.Reserve(m_renderPassAttachments.Size());

    VkAttachmentReference depthAttachmentReference {};
    Array<VkAttachmentReference, VulkanAllocator> colorAttachmentReferences;

    VkSubpassDescription subpassDescription {};
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.pDepthStencilAttachment = nullptr;

    uint32 nextBinding = 0;
    HashSet<uint32, &KeyBy_Identity<uint32>, NodeAllocator<VulkanAllocator>> usedBindings;

    for (const VulkanAttachmentRef& attachment : m_renderPassAttachments)
    {
        if (!attachment->HasBinding())
        { // no binding has manually been set so we make one
            attachment->SetBinding(nextBinding);
        }

        if (usedBindings.Contains(attachment->GetBinding()))
        {
            return HYP_MAKE_ERROR(RendererError, "Render pass attachment binding cannot be reused");
        }

        usedBindings.Insert(attachment->GetBinding());

        nextBinding = attachment->GetBinding() + 1;

        attachmentDescriptions.PushBack(attachment->GetVulkanAttachmentDescription());
        
        if (attachmentDescriptions.Back().finalLayout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            && attachmentDescriptions.Back().finalLayout != VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
        {
            HYP_BREAKPOINT;
        }

        if (attachment->IsDepthAttachment())
        {
            depthAttachmentReference = attachment->GetVulkanHandle();
            subpassDescription.pDepthStencilAttachment = &depthAttachmentReference;

            m_vkClearValues.PushBack(VkClearValue {
                .depthStencil = { 1.0f, 0 } });
        }
        else
        {
            colorAttachmentReferences.PushBack(attachment->GetVulkanHandle());

            m_vkClearValues.PushBack(VkClearValue {
                .color = {
                    .float32 = {
                        attachment->GetClearColor().x,
                        attachment->GetClearColor().y,
                        attachment->GetClearColor().z,
                        attachment->GetClearColor().w } } });
        }
    }

    subpassDescription.colorAttachmentCount = uint32(colorAttachmentReferences.Size());
    subpassDescription.pColorAttachments = colorAttachmentReferences.Data();

    // Create the actual renderpass
    VkRenderPassCreateInfo renderPassInfo { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
    renderPassInfo.attachmentCount = uint32(attachmentDescriptions.Size());
    renderPassInfo.pAttachments = attachmentDescriptions.Data();
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
        for (uint32 i = 0; i < m_numMultiviewLayers; i++)
        {
            multiviewViewMask |= 1 << i;
            multiviewCorrelationMask |= 1 << i;
        }

        renderPassInfo.pNext = &multiviewInfo;
    }

    VULKAN_CHECK(vkCreateRenderPass(GetRenderBackend()->GetDevice()->GetDevice(), &renderPassInfo, nullptr, &m_handle));

    HYPERION_RETURN_OK;
}
HYP_ENABLE_OPTIMIZATION;

void VulkanRenderPass::Begin(VulkanCommandBuffer* cmd, VulkanFramebuffer* framebuffer)
{
    if (m_isRecording)
    {
        return;
    }

    HYP_GFX_ASSERT(framebuffer != nullptr);

    VkRenderPassBeginInfo renderPassInfo { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    renderPassInfo.renderPass = m_handle;
    renderPassInfo.framebuffer = framebuffer->GetVulkanHandle();
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = VkExtent2D { framebuffer->GetWidth(), framebuffer->GetHeight() };
    renderPassInfo.clearValueCount = uint32(m_vkClearValues.Size());
    renderPassInfo.pClearValues = m_vkClearValues.Data();

    VkSubpassContents contents = VK_SUBPASS_CONTENTS_INLINE;

    switch (m_mode)
    {
    case RENDER_PASS_INLINE:
        contents = VK_SUBPASS_CONTENTS_INLINE;
        break;
    case RENDER_PASS_SECONDARY_COMMAND_BUFFER:
        contents = VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS;
        break;
    }

#ifdef HYP_DEBUG_MODE
    for (VulkanAttachment* attachment : m_renderPassAttachments)
    {
        const ResourceState expectedResourceState = attachment->IsDepthAttachment()
            ? PreRenderResourceStatesDepth[int(attachment->GetLoadOperation() == LoadOperation::LOAD)]
            : PreRenderResourceStates[int(attachment->GetLoadOperation() == LoadOperation::LOAD)];

        // don't bother checking undefined as that just means the driver can do whatever it wants with it
        if (expectedResourceState != RS_UNDEFINED)
        {
            Assert(attachment->GetImage()->GetResourceState() == expectedResourceState,
                "Expected render target attachment to be in resource state {} but got {}",
                EnumToString(expectedResourceState),
                EnumToString(attachment->GetImage()->GetResourceState()));
        }

        HYP_LOG_TEMP("Attachment {} resource state = {}", attachment->GetImage()->GetDebugName(), EnumToString(attachment->GetImage()->GetResourceState()));
    }
#endif

    vkCmdBeginRenderPass(cmd->GetVulkanHandle(), &renderPassInfo, contents);

    m_isRecording = true;
}

void VulkanRenderPass::End(VulkanCommandBuffer* cmd)
{
    if (!m_isRecording)
    {
        return;
    }

    vkCmdEndRenderPass(cmd->GetVulkanHandle());

    for (VulkanAttachment* attachment : m_renderPassAttachments)
    {
        attachment->GetImage()->SetResourceState(attachment->IsDepthAttachment()
                ? PostRenderResourceStatesDepth[m_renderTargetType]
                : PostRenderResourceStates[m_renderTargetType]);
    }

    m_isRecording = false;
}

} // namespace hyperion
