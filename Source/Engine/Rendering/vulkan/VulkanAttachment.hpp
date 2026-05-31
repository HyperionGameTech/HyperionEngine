/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <Rendering/Attachment.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <Rendering/RenderTypes.hpp>

#include <Core/math/MathUtil.hpp>

#include <Core/HashCode.hpp>
#include <Core/Types.hpp>

#include <vulkan/vulkan.h>

namespace Hyperion {

enum class RenderPassMode : uint8;

extern Pool* g_vulkanPool;

HYP_CLASS(NoScriptBindings)
class VulkanAttachment final : public AttachmentBase
{
    HYP_OBJECT_BODY(VulkanAttachment);

public:
    VulkanAttachment(
        const VulkanGpuImageRef& image,
        const VulkanGpuImageViewRef& imageView, // May be null
        const VulkanFramebufferWeakRef& framebuffer,
        RenderPassMode renderPassMode,
        const AttachmentDesc& attachmentDesc);

    VulkanAttachment(
        const TextureDesc& textureDesc,
        const VulkanFramebufferWeakRef& framebuffer,
        RenderPassMode renderPassMode,
        const AttachmentDesc& attachmentDesc);

    ~VulkanAttachment() override;

    HYP_FORCE_INLINE const VkAttachmentReference& GetVulkanHandle() const
    {
        return m_vkAttachmentReference;
    }

    HYP_FORCE_INLINE const VkAttachmentDescription& GetVulkanAttachmentDescription() const
    {
        return m_vkAttachmentDescription;
    }

    HYP_FORCE_INLINE RenderPassMode GetRenderPassMode() const
    {
        return m_renderPassMode;
    }

    bool IsCreated() const override;

    RendererResult Create() override;

private:
    RenderPassMode m_renderPassMode;

    VkAttachmentReference m_vkAttachmentReference;
    VkAttachmentDescription m_vkAttachmentDescription;
};

} // namespace Hyperion
