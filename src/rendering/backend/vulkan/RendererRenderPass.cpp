#include <rendering/backend/RendererRenderPass.hpp>
#include <rendering/backend/RendererFramebuffer.hpp>
#include <rendering/backend/RendererCommandBuffer.hpp>

namespace hyperion {
namespace renderer {
namespace platform {

RenderPass<Platform::VULKAN>::RenderPass(RenderPassStage stage, RenderPassMode mode)
    : m_stage(stage),
      m_mode(mode),
      m_handle(VK_NULL_HANDLE),
      m_num_multiview_layers(0)
{
}

RenderPass<Platform::VULKAN>::RenderPass(RenderPassStage stage, RenderPassMode mode, UInt num_multiview_layers)
    : m_stage(stage),
      m_mode(mode),
      m_handle(VK_NULL_HANDLE),
      m_num_multiview_layers(num_multiview_layers)
{
}

RenderPass<Platform::VULKAN>::~RenderPass()
{
    AssertThrowMsg(m_handle == VK_NULL_HANDLE, "handle should have been destroyed");
}

void RenderPass<Platform::VULKAN>::CreateDependencies()
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
		    .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		    .srcAccessMask = VK_ACCESS_MEMORY_READ_BIT,
		    .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
                           | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		    .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
        });

        AddDependency({
            .srcSubpass = 0,
		    .dstSubpass = VK_SUBPASS_EXTERNAL,
		    .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		    .dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
		    .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
                           | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		    .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
		    .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
        });

        break;
    default:
        AssertThrowMsg(0, "Unsupported stage type %d", m_stage);
    }
}

Result RenderPass<Platform::VULKAN>::Create(Device<Platform::VULKAN> *device)
{
    CreateDependencies();

    Array<VkAttachmentDescription> attachment_descriptions;
    attachment_descriptions.Reserve(m_render_pass_attachment_usages.Size());

    VkAttachmentReference depth_attachment_usage { };
    Array<VkAttachmentReference> color_attachment_usages;

    VkSubpassDescription subpass_description { };
    subpass_description.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass_description.pDepthStencilAttachment = nullptr;

    UInt next_binding = 0;

    for (auto *attachment_usage : m_render_pass_attachment_usages) {
        if (!attachment_usage->HasBinding()) { // no binding has manually been set so we make one
            attachment_usage->SetBinding(next_binding);
        }

        next_binding = attachment_usage->GetBinding() + 1;

        attachment_descriptions.PushBack(attachment_usage->GetAttachmentDescription());

        if (attachment_usage->IsDepthAttachment()) {
            depth_attachment_usage = attachment_usage->GetHandle();
            subpass_description.pDepthStencilAttachment = &depth_attachment_usage;

            m_clear_values.PushBack(VkClearValue {
                .depthStencil = { 1.0f, 0 }
            });
        } else {
            color_attachment_usages.PushBack(attachment_usage->GetHandle());

            m_clear_values.PushBack(VkClearValue {
                .color = {
                    .float32 = { 0.0f, 0.0f, 0.0f, 0.0f }
                }
            });
        }
    }

    subpass_description.colorAttachmentCount    = UInt32(color_attachment_usages.Size());
    subpass_description.pColorAttachments       = color_attachment_usages.Data();

    // Create the actual renderpass
    VkRenderPassCreateInfo render_pass_info { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
    render_pass_info.attachmentCount    = UInt32(attachment_descriptions.Size());
    render_pass_info.pAttachments       = attachment_descriptions.Data();
    render_pass_info.subpassCount       = 1;
    render_pass_info.pSubpasses         = &subpass_description;
    render_pass_info.dependencyCount    = UInt32(m_dependencies.Size());
    render_pass_info.pDependencies      = m_dependencies.Data();

    UInt32 multiview_view_mask = 0;
    UInt32 multiview_correlation_mask = 0;

    VkRenderPassMultiviewCreateInfo multiview_info { VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO };
    multiview_info.subpassCount         = 1;
    multiview_info.pViewMasks           = &multiview_view_mask;
    multiview_info.pCorrelationMasks    = &multiview_correlation_mask;
    multiview_info.correlationMaskCount = 1;

    if (IsMultiview()) {
        for (UInt i = 0; i < m_num_multiview_layers; i++) {
            multiview_view_mask |= 1 << i;
            multiview_correlation_mask |= 1 << i;
        }
        
        render_pass_info.pNext = &multiview_info;
    }

    HYPERION_VK_CHECK(vkCreateRenderPass(device->GetDevice(), &render_pass_info, nullptr, &m_handle));

    HYPERION_RETURN_OK;
}

Result RenderPass<Platform::VULKAN>::Destroy(Device<Platform::VULKAN> *device)
{
    auto result = Result::OK;

    vkDestroyRenderPass(device->GetDevice(), m_handle, nullptr);
    m_handle = nullptr;

    for (const auto *attachment_usage : m_render_pass_attachment_usages) {
        attachment_usage->DecRef(HYP_ATTACHMENT_USAGE_INSTANCE);
    }

    m_render_pass_attachment_usages.Clear();

    return result;
}

void RenderPass<Platform::VULKAN>::Begin(CommandBuffer<Platform::VULKAN> *cmd, FramebufferObject<Platform::VULKAN> *framebuffer)
{
    AssertThrow(framebuffer != nullptr && framebuffer->GetHandle() != nullptr);

    VkRenderPassBeginInfo render_pass_info { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    render_pass_info.renderPass         = m_handle;
    render_pass_info.framebuffer        = framebuffer->GetHandle();
    render_pass_info.renderArea.offset  = { 0, 0 };
    render_pass_info.renderArea.extent  = VkExtent2D { framebuffer->GetWidth(), framebuffer->GetHeight() };
    render_pass_info.clearValueCount    = UInt32(m_clear_values.Size());
    render_pass_info.pClearValues       = m_clear_values.Data();

    VkSubpassContents contents = VK_SUBPASS_CONTENTS_INLINE;

    switch (m_mode) {
    case RENDER_PASS_INLINE:
        contents = VK_SUBPASS_CONTENTS_INLINE;
        break;
    case RENDER_PASS_SECONDARY_COMMAND_BUFFER:
        contents = VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS;
        break;
    }

    vkCmdBeginRenderPass(cmd->GetCommandBuffer(), &render_pass_info, contents);
}

void RenderPass<Platform::VULKAN>::End(CommandBuffer<Platform::VULKAN> *cmd)
{
    vkCmdEndRenderPass(cmd->GetCommandBuffer());
}

} // namespace platform
} // namespace renderer
} // namespace hyperion