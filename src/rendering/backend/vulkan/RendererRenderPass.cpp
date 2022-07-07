#include "RendererRenderPass.hpp"
#include "RendererFramebuffer.hpp"
#include "RendererCommandBuffer.hpp"

namespace hyperion {
namespace renderer {

RenderPass::RenderPass(RenderPassStage stage, Mode mode)
    : m_stage(stage),
      m_mode(mode),
      m_handle(VK_NULL_HANDLE),
      m_num_multiview_layers(0)
{
}

RenderPass::RenderPass(RenderPassStage stage, Mode mode, uint32_t num_multiview_layers)
    : m_stage(stage),
      m_mode(mode),
      m_handle(VK_NULL_HANDLE),
      m_num_multiview_layers(num_multiview_layers)
{
}

RenderPass::~RenderPass()
{
    AssertThrowMsg(m_handle == VK_NULL_HANDLE, "handle should have been destroyed");
}

void RenderPass::CreateDependencies()
{
    switch (m_stage) {
    case RenderPassStage::PRESENT:
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
    case RenderPassStage::SHADER:
        AddDependency({
            .srcSubpass = VK_SUBPASS_EXTERNAL,
		    .dstSubpass = 0,
		    .srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
		    .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                          | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
                          | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		    .srcAccessMask = VK_ACCESS_MEMORY_READ_BIT,
		    .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
                           | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                           | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT
                           | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		    .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
        });

        AddDependency({
            .srcSubpass = 0,
		    .dstSubpass = VK_SUBPASS_EXTERNAL,
		    .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                          | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
                          | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		    .dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
		    .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
                           | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                           | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT
                           | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		    .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
		    .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
        });

        break;
    default:
        AssertThrowMsg(0, "Unsupported stage type %d", m_stage);
    }
}

Result RenderPass::Create(Device *device)
{
    CreateDependencies();

    std::vector<VkAttachmentDescription> attachment_descriptions;
    attachment_descriptions.reserve(m_render_pass_attachment_refs.size());

    VkAttachmentReference depth_attachment_ref{};
    std::vector<VkAttachmentReference> color_attachment_refs;

    VkSubpassDescription subpass_description{};
    subpass_description.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass_description.pDepthStencilAttachment = nullptr;
    
    uint32_t next_binding = 0;

    for (auto *attachment_ref : m_render_pass_attachment_refs) {
        if (!attachment_ref->HasBinding()) { // no binding has manually been set so we make one
            attachment_ref->SetBinding(next_binding);
        }

        next_binding = attachment_ref->GetBinding() + 1;

        attachment_descriptions.push_back(attachment_ref->GetAttachmentDescription());

        if (attachment_ref->IsDepthAttachment()) {
            depth_attachment_ref = attachment_ref->GetHandle();
            subpass_description.pDepthStencilAttachment = &depth_attachment_ref;

            m_clear_values.push_back(VkClearValue {
                .depthStencil = { 1.0f, 0 }
            });
        } else {
            color_attachment_refs.push_back(attachment_ref->GetHandle());

            m_clear_values.push_back(VkClearValue {
                .color = {
                    .float32 = { 0.0f, 0.0f, 0.0f, 1.0f }
                }
            });
        }
    }

    subpass_description.colorAttachmentCount = static_cast<uint32_t>(color_attachment_refs.size());
    subpass_description.pColorAttachments    = color_attachment_refs.data();

    // Create the actual renderpass
    VkRenderPassCreateInfo render_pass_info{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    render_pass_info.attachmentCount = static_cast<uint32_t>(attachment_descriptions.size());
    render_pass_info.pAttachments    = attachment_descriptions.data();
    render_pass_info.subpassCount    = 1;
    render_pass_info.pSubpasses      = &subpass_description;
    render_pass_info.dependencyCount = static_cast<uint32_t>(m_dependencies.size());
    render_pass_info.pDependencies   = m_dependencies.data();

    uint32_t multiview_view_mask        = 0;
    uint32_t multiview_correlation_mask = 0;

    VkRenderPassMultiviewCreateInfo multiview_info{VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO};
    multiview_info.subpassCount         = 1;
    multiview_info.pViewMasks           = &multiview_view_mask;
    multiview_info.pCorrelationMasks    = &multiview_correlation_mask;
    multiview_info.correlationMaskCount = 1;

    if (IsMultiview()) {
        for (uint32_t i = 0; i < m_num_multiview_layers; i++) {
            multiview_view_mask        |= 1 << i;
            multiview_correlation_mask |= 1 << i;
        }
        DebugLog(LogType::RenDebug, "Multiview layer masks:\n\tVIEW MASK: 0x%02X\n\tCORRELATION MASK 0x%02X\n", multiview_view_mask, multiview_correlation_mask);

        render_pass_info.pNext = &multiview_info;
    }

    HYPERION_VK_CHECK(vkCreateRenderPass(device->GetDevice(), &render_pass_info, nullptr, &m_handle));

    HYPERION_RETURN_OK;
}

Result RenderPass::Destroy(Device *device)
{
    auto result = Result::OK;

    vkDestroyRenderPass(device->GetDevice(), m_handle, nullptr);
    m_handle = nullptr;

    for (const auto *attachment_ref : m_render_pass_attachment_refs) {
        attachment_ref->DecRef(HYP_ATTACHMENT_REF_INSTANCE);
    }

    m_render_pass_attachment_refs.clear();

    return result;
}

void RenderPass::Begin(CommandBuffer *cmd, FramebufferObject *framebuffer)
{
    AssertThrow(framebuffer != nullptr && framebuffer->GetHandle() != nullptr);

    VkRenderPassBeginInfo render_pass_info{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    render_pass_info.renderPass          = m_handle;
    render_pass_info.framebuffer         = framebuffer->GetHandle();
    render_pass_info.renderArea.offset   = {0, 0};
    render_pass_info.renderArea.extent   = VkExtent2D{framebuffer->GetWidth(), framebuffer->GetHeight()};
    render_pass_info.clearValueCount     = static_cast<uint32_t>(m_clear_values.size());
    render_pass_info.pClearValues        = m_clear_values.data();

    VkSubpassContents contents = VK_SUBPASS_CONTENTS_INLINE;

    switch (m_mode) {
    case Mode::RENDER_PASS_INLINE:
        contents = VK_SUBPASS_CONTENTS_INLINE;
        break;
    case Mode::RENDER_PASS_SECONDARY_COMMAND_BUFFER:
        contents = VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS;
        break;
    }

    vkCmdBeginRenderPass(cmd->GetCommandBuffer(), &render_pass_info, contents);
}

void RenderPass::End(CommandBuffer *cmd)
{
    vkCmdEndRenderPass(cmd->GetCommandBuffer());
}

} // namespace renderer
} // namespace hyperion