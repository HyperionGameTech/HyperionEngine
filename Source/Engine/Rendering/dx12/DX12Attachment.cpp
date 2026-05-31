/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <DX12Pch.hpp>

#include <Rendering/dx12/DX12Attachment.hpp>
#include <Rendering/dx12/DX12GpuImage.hpp>
#include <Rendering/dx12/DX12GpuImageView.hpp>
#include <Rendering/dx12/DX12RenderInterface.hpp>

#include <Rendering/util/DeletionQueue.hpp>

#include <DX12Attachment.generated.inl>

namespace Hyperion {

extern DX12RenderInterface RI;

#pragma region DX12Attachment

DX12Attachment::DX12Attachment(
    const DX12GpuImageRef& image,
    const DX12GpuImageViewRef& imageView,
    const DX12FramebufferWeakRef& framebuffer,
    RenderPassMode renderPassMode,
    const AttachmentDesc& attachmentDesc)
    : AttachmentBase(image, imageView, framebuffer, attachmentDesc),
      m_renderPassMode(renderPassMode)
{
    Assert(m_gpuImage.IsValid());

    if (!m_imageView.IsValid())
    {
        m_imageView = MakeHandle<DX12GpuImageView>(m_gpuImage);
    }
}

DX12Attachment::DX12Attachment(
    const TextureDesc& textureDesc,
    const DX12FramebufferWeakRef& framebuffer,
    RenderPassMode renderPassMode,
    const AttachmentDesc& attachmentDesc)
    : AttachmentBase(textureDesc, framebuffer, attachmentDesc),
      m_renderPassMode(renderPassMode)
{
    Assert(m_gpuImage.IsValid());

    if (!m_imageView.IsValid())
    {
        m_imageView = MakeHandle<DX12GpuImageView>(m_gpuImage);
    }
}

DX12Attachment::~DX12Attachment()
{
    m_imageView.Reset();
}

bool DX12Attachment::IsCreated() const
{
    return Texture::IsCreated() && m_imageView != nullptr && m_imageView->IsCreated();
}

RendererResult DX12Attachment::Create()
{
    Assert(m_gpuImage != nullptr && m_imageView != nullptr);

    if (!m_gpuImage->IsCreated())
    {
        CheckResult(m_gpuImage->Create());
    }

    CheckResult(m_imageView->Create());

    return Texture::Create();
}

#pragma endregion DX12Attachment

} // namespace Hyperion
