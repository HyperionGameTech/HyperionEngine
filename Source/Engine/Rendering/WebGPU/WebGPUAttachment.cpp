/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <WebGPUPch.hpp>

#include <Rendering/WebGPU/WebGPUAttachment.hpp>
#include <Rendering/WebGPU/WebGPUGpuImage.hpp>
#include <Rendering/WebGPU/WebGPUGpuImageView.hpp>
#include <Rendering/WebGPU/WebGPURenderInterface.hpp>

#include <WebGPUAttachment.generated.inl>

namespace Hyperion {

extern WebGPURenderInterface RI;

#pragma region WebGPUAttachment

WebGPUAttachment::WebGPUAttachment(
    const WebGPUGpuImageRef& image,
    const WebGPUGpuImageViewRef& imageView,
    const WebGPUFramebufferWeakRef& framebuffer,
    RenderPassMode renderPassMode,
    const AttachmentDesc& attachmentDesc)
    : AttachmentBase(image, imageView, framebuffer, attachmentDesc),
      m_renderPassMode(renderPassMode)
{
    Assert(m_gpuImage.IsValid());

    if (!m_imageView.IsValid())
    {
        m_imageView = MakeHandle<WebGPUGpuImageView>(m_gpuImage);
    }
}

WebGPUAttachment::WebGPUAttachment(
    const TextureDesc& textureDesc,
    const WebGPUFramebufferWeakRef& framebuffer,
    RenderPassMode renderPassMode,
    const AttachmentDesc& attachmentDesc)
    : AttachmentBase(textureDesc, framebuffer, attachmentDesc),
      m_renderPassMode(renderPassMode)
{
    Assert(m_gpuImage.IsValid());

    if (!m_imageView.IsValid())
    {
        m_imageView = MakeHandle<WebGPUGpuImageView>(m_gpuImage);
    }
}

WebGPUAttachment::~WebGPUAttachment()
{
    m_imageView.Reset();
}

bool WebGPUAttachment::IsCreated() const
{
    return Texture::IsCreated() && m_imageView != nullptr && m_imageView->IsCreated();
}

RendererResult WebGPUAttachment::Create()
{
    Assert(m_gpuImage != nullptr && m_imageView != nullptr);

    if (!m_gpuImage->IsCreated())
    {
        Check(m_gpuImage->Create());
    }

    Check(m_imageView->Create());

    return Texture::Create();
}

#pragma endregion WebGPUAttachment

} // namespace Hyperion
