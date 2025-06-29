/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/vulkan/RendererRenderPass.hpp>
#include <rendering/backend/vulkan/RendererFramebuffer.hpp>
#include <rendering/backend/vulkan/RendererCommandBuffer.hpp>
#include <rendering/backend/vulkan/VulkanRenderBackend.hpp>

#include <core/containers/HashSet.hpp>

namespace hyperion {

extern IRenderBackend* g_render_backend;

static inline VulkanRenderBackend* GetRenderBackend()
{
    return static_cast<VulkanRenderBackend*>(g_render_backend);
}

VulkanRenderPass::VulkanRenderPass(RenderPassStage stage, RenderPassMode mode)
    : m_stage(stage),
      m_mode(mode),
      m_handle(VK_NULL_HANDLE),
      m_num_multiview_layers(0)
{
}

VulkanRenderPass::VulkanRenderPass(RenderPassStage stage, RenderPassMode mode, uint32 num_multiview_layers)
    : m_stage(stage),
      m_mode(mode),
      m_handle(VK_NULL_HANDLE),
      m_num_multiview_layers(num_multiview_layers)
{
}

VulkanRenderPass::~VulkanRenderPass()
{
    AssertThrowMsg(m_handle == VK_NULL_HANDLE, "handle should have been destroyed");
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
        AssertThrowMsg(0, "Unsupported stage type %d", m_stage);
    }
}

void VulkanRenderPass::AddAttachment(VulkanAttachmentRef attachment)
{
    m_render_pass_attachments.PushBack(std::move(attachment));
}

bool VulkanRenderPass::RemoveAttachment(const VulkanAttachment* attachment)
{
    const auto it = m_render_pass_attachments.FindAs(attachment);

    if (it == m_render_pass_attachments.End())
    {
        return false;
    }

    SafeRelease(std::move(*it));

    m_render_pass_attachments.Erase(it);

    return true;
}

RendererResult VulkanRenderPass::Create()
{
    CreateDependencies();

    Array<VkAttachmentDescription> attachment_descriptions;
    attachment_descriptions.Reserve(m_render_pass_attachments.Size());

    VkAttachmentReference depth_attachment_reference {};
    Array<VkAttachmentReference> color_attachment_references;

    VkSubpassDescription subpass_description {};
    subpass_description.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass_description.pDepthStencilAttachment = nullptr;

    uint32 next_binding = 0;
    HashSet<uint32> used_bindings;

    for (const VulkanAttachmentRef& attachment : m_render_pass_attachments)
    {
        if (!attachment->HasBinding())
        { // no binding has manually been set so we make one
            attachment->SetBinding(next_binding);
        }

        if (used_bindings.Contains(attachment->GetBinding()))
        {
            return HYP_MAKE_ERROR(RendererError, "Render pass attachment binding cannot be reused");
        }

        used_bindings.Insert(attachment->GetBinding());

        next_binding = attachment->GetBinding() + 1;

        attachment_descriptions.PushBack(attachment->GetVulkanAttachmentDescription());

        if (attachment->IsDepthAttachment())
        {
            depth_attachment_reference = attachment->GetVulkanHandle();
            subpass_description.pDepthStencilAttachment = &depth_attachment_reference;

            m_vk_clear_values.PushBack(VkClearValue {
                .depthStencil = { 1.0f, 0 } });
        }
        else
        {
            color_attachment_references.PushBack(attachment->GetVulkanHandle());

            m_vk_clear_values.PushBack(VkClearValue {
                .color = {
                    .float32 = {
                        attachment->GetClearColor().x,
                        attachment->GetClearColor().y,
                        attachment->GetClearColor().z,
                        attachment->GetClearColor().w } } });
        }
    }

    subpass_description.colorAttachmentCount = uint32(color_attachment_references.Size());
    subpass_description.pColorAttachments = color_attachment_references.Data();

    // Create the actual renderpass
    VkRenderPassCreateInfo render_pass_info { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
    render_pass_info.attachmentCount = uint32(attachment_descriptions.Size());
    render_pass_info.pAttachments = attachment_descriptions.Data();
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass_description;
    render_pass_info.dependencyCount = uint32(m_dependencies.Size());
    render_pass_info.pDependencies = m_dependencies.Data();

    uint32 multiview_view_mask = 0;
    uint32 multiview_correlation_mask = 0;

    VkRenderPassMultiviewCreateInfo multiview_info { VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO };
    multiview_info.subpassCount = 1;
    multiview_info.pViewMasks = &multiview_view_mask;
    multiview_info.pCorrelationMasks = &multiview_correlation_mask;
    multiview_info.correlationMaskCount = 1;

    if (IsMultiview())
    {
        for (uint32 i = 0; i < m_num_multiview_layers; i++)
        {
            multiview_view_mask |= 1 << i;
            multiview_correlation_mask |= 1 << i;
        }

        render_pass_info.pNext = &multiview_info;
    }

    HYPERION_VK_CHECK(vkCreateRenderPass(GetRenderBackend()->GetDevice()->GetDevice(), &render_pass_info, nullptr, &m_handle));

    HYPERION_RETURN_OK;
}

RendererResult VulkanRenderPass::Destroy()
{
    RendererResult result;

    vkDestroyRenderPass(GetRenderBackend()->GetDevice()->GetDevice(), m_handle, nullptr);
    m_handle = VK_NULL_HANDLE;

    SafeRelease(std::move(m_render_pass_attachments));

    return result;
}

void VulkanRenderPass::Begin(VulkanCommandBuffer* cmd, VulkanFramebuffer* framebuffer, uint32 frame_index)
{
    AssertThrow(framebuffer != nullptr);

    VkRenderPassBeginInfo render_pass_info { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    render_pass_info.renderPass = m_handle;
    render_pass_info.framebuffer = framebuffer->GetVulkanHandles()[frame_index];
    render_pass_info.renderArea.offset = { 0, 0 };
    render_pass_info.renderArea.extent = VkExtent2D { framebuffer->GetWidth(), framebuffer->GetHeight() };
    render_pass_info.clearValueCount = uint32(m_vk_clear_values.Size());
    render_pass_info.pClearValues = m_vk_clear_values.Data();

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

    vkCmdBeginRenderPass(cmd->GetVulkanHandle(), &render_pass_info, contents);
}

void VulkanRenderPass::End(VulkanCommandBuffer* cmd)
{
    vkCmdEndRenderPass(cmd->GetVulkanHandle());
}

} // namespace hyperion