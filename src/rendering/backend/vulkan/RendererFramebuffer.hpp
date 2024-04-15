/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERER_BACKEND_VULKAN_FBO_HPP
#define HYPERION_RENDERER_BACKEND_VULKAN_FBO_HPP

#include <rendering/backend/RendererAttachment.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererImageView.hpp>
#include <rendering/backend/RendererSampler.hpp>
#include <rendering/backend/RendererRenderPass.hpp>

#include <Types.hpp>

namespace hyperion {
namespace renderer {
namespace platform {

template <>
class FramebufferObject<Platform::VULKAN>
{
public:
    HYP_API FramebufferObject(Extent2D extent);
    HYP_API FramebufferObject(Extent3D extent);
    FramebufferObject(const FramebufferObject &other)               = delete;
    FramebufferObject &operator=(const FramebufferObject &other)    = delete;
    HYP_API ~FramebufferObject();

    VkFramebuffer GetHandle() const
        { return m_handle; }

    HYP_API Result Create(Device<Platform::VULKAN> *device, RenderPass<Platform::VULKAN> *render_pass);
    HYP_API Result Destroy(Device<Platform::VULKAN> *device);

    HYP_API void AddAttachmentUsage(AttachmentUsageRef<Platform::VULKAN> attachment_usage);
    HYP_API bool RemoveAttachmentUsage(const AttachmentRef<Platform::VULKAN> &attachment);

    const Array<AttachmentUsageRef<Platform::VULKAN>> &GetAttachmentUsages() const
        { return m_attachment_usages; }

    uint GetWidth() const
        { return m_extent.width; }

    uint GetHeight() const
        { return m_extent.height; }

private:
    Extent3D                                    m_extent;

    Array<AttachmentUsageRef<Platform::VULKAN>> m_attachment_usages;

    VkFramebuffer                               m_handle;
};

} // namespace platform
} // namespace renderer
} // namespace hyperion

#endif // RENDERER_FBO_H
