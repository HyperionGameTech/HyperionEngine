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

namespace Hyperion {

struct AttachmentDesc;
enum class RenderPassMode : uint8;

HYP_CLASS(NoScriptBindings)
class WebGPUAttachment final : public AttachmentBase
{
    HYP_OBJECT_BODY(WebGPUAttachment);

public:
    WebGPUAttachment(
        const WebGPUGpuImageRef& image,
        const WebGPUGpuImageViewRef& imageView, // May be null
        const WebGPUFramebufferWeakRef& framebuffer,
        RenderPassMode renderPassMode,
        const AttachmentDesc& attachmentDesc);

    WebGPUAttachment(
        const TextureDesc& textureDesc,
        const WebGPUFramebufferWeakRef& framebuffer,
        RenderPassMode renderPassMode,
        const AttachmentDesc& attachmentDesc);

    ~WebGPUAttachment() override;

    HYP_FORCE_INLINE RenderPassMode GetRenderPassMode() const
    {
        return m_renderPassMode;
    }

    bool IsCreated() const override;

    RendererResult Create() override;

private:
    RenderPassMode m_renderPassMode;
};

} // namespace Hyperion
