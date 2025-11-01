/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/vulkan/VulkanRenderPass.hpp>
#include <rendering/vulkan/VulkanFramebuffer.hpp>
#include <rendering/vulkan/VulkanCommandBuffer.hpp>
#include <rendering/vulkan/VulkanDevice.hpp>
#include <rendering/vulkan/VulkanRenderBackend.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <core/containers/HashSet.hpp>

namespace hyperion {

extern IRenderBackend* g_renderBackend;

static inline VulkanRenderBackend* GetRenderBackend()
{
    return static_cast<VulkanRenderBackend*>(g_renderBackend);
}

VulkanRenderPass::VulkanRenderPass(RenderPassStage stage, RenderPassMode mode)
    : m_stage(stage),
      m_mode(mode),
      m_handle(VK_NULL_HANDLE),
      m_numMultiviewLayers(0)
{
}

VulkanRenderPass::VulkanRenderPass(RenderPassStage stage, RenderPassMode mode, uint32 numMultiviewLayers)
    : m_stage(stage),
      m_mode(mode),
      m_handle(VK_NULL_HANDLE),
      m_numMultiviewLayers(numMultiviewLayers)
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
    switch (m_stage)
    {
    case RenderPassStage::PRESENT:
        AddDependency(VkSubpassDependency {
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT });

        break;
    case RenderPassStage::SHADER:
        AddDependency({ .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = VK_ACCESS_MEMORY_READ_BIT,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
                | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT });

        AddDependency({ .srcSubpass = 0,
            .dstSubpass = VK_SUBPASS_EXTERNAL,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
                | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT });

        break;
    default:
        HYP_GFX_ASSERT(0, "Unsupported stage type %d", int(m_stage));
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

RendererResult VulkanRenderPass::Create()
{
    CreateDependencies();

    Array<VkAttachmentDescription> attachmentDescriptions;
    attachmentDescriptions.Reserve(m_renderPassAttachments.Size());

    VkAttachmentReference depthAttachmentReference {};
    Array<VkAttachmentReference> colorAttachmentReferences;

    VkSubpassDescription subpassDescription {};
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.pDepthStencilAttachment = nullptr;

    uint32 nextBinding = 0;
    HashSet<uint32> usedBindings;

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

void VulkanRenderPass::Begin(VulkanCommandBuffer* cmd, VulkanFramebuffer* framebuffer)
{
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

    vkCmdBeginRenderPass(cmd->GetVulkanHandle(), &renderPassInfo, contents);
}

void VulkanRenderPass::End(VulkanCommandBuffer* cmd)
{
    vkCmdEndRenderPass(cmd->GetVulkanHandle());
}

} // namespace hyperion
