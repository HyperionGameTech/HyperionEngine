/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERER_BACKEND_VULKAN_RENDER_PASS_HPP
#define HYPERION_RENDERER_BACKEND_VULKAN_RENDER_PASS_HPP

#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererImageView.hpp>
#include <rendering/backend/RendererSampler.hpp>
#include <rendering/backend/RendererAttachment.hpp>

#include <math/Vector4.hpp>
#include <core/lib/DynArray.hpp>

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
class FramebufferObject;

template <>
class RenderPass<Platform::VULKAN>
{
    friend class FramebufferObject<Platform::VULKAN>;
    friend class GraphicsPipeline<Platform::VULKAN>;

public:
    RenderPass(RenderPassStage stage, RenderPassMode mode);
    RenderPass(RenderPassStage stage, RenderPassMode mode, uint num_multiview_layers);
    RenderPass(const RenderPass &other) = delete;
    RenderPass &operator=(const RenderPass &other) = delete;
    ~RenderPass();

    RenderPassStage GetStage() const
        { return m_stage; }

    Vec4f GetClearColor() const
        { return m_clear_color; }

    void SetClearColor(const Vec4f &clear_color)
        { m_clear_color = clear_color; }

    bool IsMultiview() const
        { return m_num_multiview_layers != 0; }

    uint NumMultiviewLayers() const
        { return m_num_multiview_layers; }

    void AddAttachmentUsage(AttachmentUsageRef<Platform::VULKAN> attachment_usage);
    bool RemoveAttachmentUsage(const AttachmentRef<Platform::VULKAN> &attachment);

    auto &GetAttachmentUsages() { return m_render_pass_attachment_usages; }
    const auto &GetAttachmentUsages() const { return m_render_pass_attachment_usages; }

    VkRenderPass GetHandle() const { return m_handle; }

    Result Create(Device<Platform::VULKAN> *device);
    Result Destroy(Device<Platform::VULKAN> *device);

    void Begin(CommandBuffer<Platform::VULKAN> *cmd, FramebufferObject<Platform::VULKAN> *framebuffer);
    void End(CommandBuffer<Platform::VULKAN> *cmd);

private:
    void CreateDependencies();

    void AddDependency(const VkSubpassDependency &dependency)
        { m_dependencies.PushBack(dependency); }

    RenderPassStage                             m_stage;
    RenderPassMode                              m_mode;
    uint                                        m_num_multiview_layers;

    Vec4f                                       m_clear_color;

    Array<AttachmentUsageRef<Platform::VULKAN>> m_render_pass_attachment_usages;

    Array<VkSubpassDependency>                  m_dependencies;
    Array<VkClearValue>                         m_vk_clear_values;

    VkRenderPass                                m_handle;
};

} // namespace platform
} // namespace renderer
} // namespace hyperion

#endif