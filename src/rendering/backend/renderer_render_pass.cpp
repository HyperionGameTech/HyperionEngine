#include "renderer_render_pass.h"

#include "renderer_command_buffer.h"

namespace hyperion {
namespace renderer {
RenderPass::RenderPass(Stage stage, Mode mode)
    : m_stage(stage),
      m_mode(mode),
      m_render_pass{}
{
}

RenderPass::~RenderPass()
{
    AssertExitMsg(m_render_pass == nullptr, "render pass should have been destroyed");
}


void RenderPass::AddDepthAttachment(std::unique_ptr<AttachmentBase> &&attachment)
{
    AssertThrowMsg(m_depth_attachments.empty(), "May only have one depth attachment in a renderpass!");

    m_depth_attachments.push_back(std::move(attachment));
}

void RenderPass::AddColorAttachment(std::unique_ptr<AttachmentBase> &&attachment)
{
    m_color_attachments.push_back(std::move(attachment));
}

void RenderPass::CreateAttachments()
{
    for (const auto &it : m_attachments) {
        const bool is_depth_attachment = Image::IsDepthTexture(it.second.format);

        std::unique_ptr<AttachmentBase> attachment;

        switch (m_stage) {
        case RENDER_PASS_STAGE_PRESENT:
            if (is_depth_attachment) {
                attachment = std::make_unique<renderer::Attachment
                    <VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL>>
                    (it.first, Image::ToVkFormat(it.second.format));
            } else {
                attachment = std::make_unique<renderer::Attachment
                    <VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR>>
                    (it.first, Image::ToVkFormat(it.second.format));
            }
            break;
        case RENDER_PASS_STAGE_SHADER:
            if (is_depth_attachment) {
                attachment = std::make_unique<renderer::Attachment
                    <VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL>>
                    (it.first, Image::ToVkFormat(it.second.format));
            } else {
                attachment = std::make_unique<renderer::Attachment
                    <VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL>>
                    (it.first, Image::ToVkFormat(it.second.format));
            }
            break;
        default:
            AssertThrowMsg(false, "Unsupported stage type %d", m_stage);
        }

        if (is_depth_attachment) {
            AddDepthAttachment(std::move(attachment));
        } else {
            AddColorAttachment(std::move(attachment));
        }
    }
}
void RenderPass::CreateDependencies()
{
    switch (m_stage) {
    case RENDER_PASS_STAGE_PRESENT:
        AddDependency(VkSubpassDependency{
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
        });

        break;
    case RENDER_PASS_STAGE_SHADER:
        AddDependency(VkSubpassDependency{
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dependencyFlags = 0
        });

        AddDependency(VkSubpassDependency{
            .srcSubpass = 0,
            .dstSubpass = VK_SUBPASS_EXTERNAL,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .dependencyFlags = 0
        });

        break;
    default:
        AssertThrowMsg(0, "Unsupported stage type %d", m_stage);
    }
}

Result RenderPass::Create(Device *device)
{
    CreateAttachments();
    CreateDependencies();

    std::vector<VkAttachmentDescription> attachments;
    attachments.reserve(m_color_attachments.size() + m_depth_attachments.size());

    VkAttachmentReference depth_attachment_ref{};
    std::vector<VkAttachmentReference> color_attachment_refs;

    VkSubpassDescription subpass_description{};
    subpass_description.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass_description.pDepthStencilAttachment = nullptr;

    for (size_t i = 0; i < m_color_attachments.size(); i++) {
        auto &attachment = m_color_attachments[i];

        HYPERION_BUBBLE_ERRORS(attachment->Create(device));

        attachments.push_back(attachment->m_attachment_description);
        color_attachment_refs.push_back(attachment->m_attachment_reference);
        
        m_clear_values.push_back(VkClearValue{.color = {0.0f, 0.0f, 0.0f, 1.0f}});
    }

    /* Should only loop one time due to assertion in AddAttachment.
     * Still, keeping it consistent. */
    for (size_t i = 0; i < m_depth_attachments.size(); i++) {
        const auto &attachment = m_depth_attachments[i];

        HYPERION_BUBBLE_ERRORS(attachment->Create(device));

        attachments.push_back(attachment->m_attachment_description);

        depth_attachment_ref = attachment->m_attachment_reference;
        subpass_description.pDepthStencilAttachment = &depth_attachment_ref;
        
        m_clear_values.push_back(VkClearValue{.depthStencil = {1.0f, 0}});
    }

    subpass_description.colorAttachmentCount = uint32_t(color_attachment_refs.size());
    subpass_description.pColorAttachments    = color_attachment_refs.data();

    // Create the actual renderpass
    VkRenderPassCreateInfo render_pass_info{};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = uint32_t(attachments.size());
    render_pass_info.pAttachments = attachments.data();
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass_description;
    render_pass_info.dependencyCount = uint32_t(m_dependencies.size());
    render_pass_info.pDependencies = m_dependencies.data();

    HYPERION_VK_CHECK(vkCreateRenderPass(device->GetDevice(), &render_pass_info, nullptr, &m_render_pass));

    HYPERION_RETURN_OK;
}

Result RenderPass::Destroy(Device *device)
{
    Result result = Result::OK;

    vkDestroyRenderPass(device->GetDevice(), m_render_pass, nullptr);
    m_render_pass = nullptr;

    return result;
}

void RenderPass::Begin(CommandBuffer *cmd, VkFramebuffer framebuffer, VkExtent2D extent, VkSubpassContents contents)
{
    VkRenderPassBeginInfo render_pass_info{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    render_pass_info.renderPass = m_render_pass;
    render_pass_info.framebuffer = framebuffer;
    render_pass_info.renderArea.offset = {0, 0};
    render_pass_info.renderArea.extent = extent;
    render_pass_info.clearValueCount = uint32_t(m_clear_values.size());
    render_pass_info.pClearValues = m_clear_values.data();

    vkCmdBeginRenderPass(cmd->GetCommandBuffer(), &render_pass_info, contents);
}

void RenderPass::End(CommandBuffer *cmd)
{
    vkCmdEndRenderPass(cmd->GetCommandBuffer());
}

} // namespace renderer
} // namespace hyperion