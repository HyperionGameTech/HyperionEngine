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
class DX12Attachment final : public AttachmentBase
{
    HYP_OBJECT_BODY(DX12Attachment);

public:
    DX12Attachment(
        const DX12GpuImageRef& image,
        const DX12GpuImageViewRef& imageView, // May be null
        const DX12FramebufferWeakRef& framebuffer,
        RenderPassMode renderPassMode,
        const AttachmentDesc& attachmentDesc);

    DX12Attachment(
        const TextureDesc& textureDesc,
        const DX12FramebufferWeakRef& framebuffer,
        RenderPassMode renderPassMode,
        const AttachmentDesc& attachmentDesc);

    ~DX12Attachment() override;

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
