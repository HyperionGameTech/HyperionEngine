/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERER_BACKEND_VULKAN_RENDER_PASS_HPP
#define HYPERION_RENDERER_BACKEND_VULKAN_RENDER_PASS_HPP

#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererImageView.hpp>
#include <rendering/backend/RendererSampler.hpp>
#include <rendering/backend/RendererAttachment.hpp>

#include <core/math/Vector4.hpp>
#include <core/containers/Array.hpp>

#include <Types.hpp>

#include <vulkan/vulkan.h>

namespace hyperion {
namespace renderer {

namespace platform {

template <PlatformType PLATFORM>
class Device;

template <PlatformType PLATFORM>
class CommandBuffer;

template <PlatformType PLATFORM>
class GraphicsPipeline;

template <PlatformType PLATFORM>
class Framebuffer;

template <>
class RenderPass<Platform::VULKAN>
{
    friend class Framebuffer<Platform::VULKAN>;
    friend class GraphicsPipeline<Platform::VULKAN>;

public:
    static constexpr PlatformType platform = Platform::VULKAN;
    
    RenderPass(RenderPassStage stage, RenderPassMode mode);
    RenderPass(RenderPassStage stage, RenderPassMode mode, uint32 num_multiview_layers);
    RenderPass(const RenderPass &other) = delete;
    RenderPass &operator=(const RenderPass &other) = delete;
    ~RenderPass();

    RenderPassStage GetStage() const
        { return m_stage; }

    bool IsMultiview() const
        { return m_num_multiview_layers != 0; }

    uint32 NumMultiviewLayers() const
        { return m_num_multiview_layers; }

    void AddAttachment(AttachmentRef<Platform::VULKAN> attachment);
    bool RemoveAttachment(const AttachmentRef<Platform::VULKAN> &attachment);

    auto &GetAttachments() { return m_render_pass_attachments; }
    const auto &GetAttachments() const { return m_render_pass_attachments; }

    VkRenderPass GetHandle() const { return m_handle; }

    RendererResult Create();
    RendererResult Destroy();

    void Begin(CommandBuffer<Platform::VULKAN> *cmd, Framebuffer<Platform::VULKAN> *framebuffer, uint32 frame_index);
    void End(CommandBuffer<Platform::VULKAN> *cmd);

private:
    void CreateDependencies();

    void AddDependency(const VkSubpassDependency &dependency)
        { m_dependencies.PushBack(dependency); }

    RenderPassStage                         m_stage;
    RenderPassMode                          m_mode;
    uint32                                  m_num_multiview_layers;

    Array<AttachmentRef<Platform::VULKAN>>  m_render_pass_attachments;

    Array<VkSubpassDependency>              m_dependencies;
    Array<VkClearValue>                     m_vk_clear_values;

    VkRenderPass                            m_handle;
};

} // namespace platform
} // namespace renderer
} // namespace hyperion

#endif