#ifndef HYPERION_RENDERER_BACKEND_VULKAN_RENDER_PASS_HPP
#define HYPERION_RENDERER_BACKEND_VULKAN_RENDER_PASS_HPP

#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererImageView.hpp>
#include <rendering/backend/RendererSampler.hpp>
#include <rendering/backend/RendererAttachment.hpp>

#include <Types.hpp>

#include <vulkan/vulkan.h>

namespace hyperion {
namespace renderer {

class CommandBuffer;
class FramebufferObject;

class RenderPass
{
    friend class FramebufferObject;
    friend class GraphicsPipeline;

public:
    enum class Mode
    {
        RENDER_PASS_INLINE = 0,
        RENDER_PASS_SECONDARY_COMMAND_BUFFER = 1
    };

    RenderPass(RenderPassStage stage, Mode mode);
    RenderPass(RenderPassStage stage, Mode mode, UInt num_multiview_layers);
    RenderPass(const RenderPass &other) = delete;
    RenderPass &operator=(const RenderPass &other) = delete;
    ~RenderPass();

    RenderPassStage GetStage() const { return m_stage; }

    bool IsMultiview() const { return m_num_multiview_layers != 0; }
    UInt NumMultiviewLayers() const { return m_num_multiview_layers; }

    void AddAttachmentUsage(AttachmentUsage *attachment_usage)
    {
        attachment_usage->IncRef(HYP_ATTACHMENT_USAGE_INSTANCE);

        m_render_pass_attachment_usages.push_back(attachment_usage);
    }

    bool RemoveAttachmentUsage(const Attachment *attachment)
    {
        const auto it = std::find_if(
            m_render_pass_attachment_usages.begin(),
            m_render_pass_attachment_usages.end(),
            [attachment](const AttachmentUsage *item) {
                return item->GetAttachment() == attachment;
            }
        );

        if (it == m_render_pass_attachment_usages.end()) {
            return false;
        }

        (*it)->DecRef(HYP_ATTACHMENT_USAGE_INSTANCE);

        m_render_pass_attachment_usages.erase(it);

        return true;
    }

    auto &GetAttachmentUsages() { return m_render_pass_attachment_usages; }
    const auto &GetAttachmentUsages() const { return m_render_pass_attachment_usages; }

    VkRenderPass GetHandle() const { return m_handle; }

    Result Create(Device *device);
    Result Destroy(Device *device);

    void Begin(CommandBuffer *cmd, FramebufferObject *framebuffer);
    void End(CommandBuffer *cmd);

private:
    void CreateDependencies();

    void AddDependency(const VkSubpassDependency &dependency)
        { m_dependencies.push_back(dependency); }

    RenderPassStage m_stage;
    Mode m_mode;
    UInt m_num_multiview_layers;

    std::vector<AttachmentUsage *> m_render_pass_attachment_usages;

    std::vector<VkSubpassDependency> m_dependencies;
    std::vector<VkClearValue> m_clear_values;

    VkRenderPass m_handle;
};

} // namespace renderer
} // namespace hyperion

#endif