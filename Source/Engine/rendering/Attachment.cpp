/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <RenderingPch.hpp>

#include <rendering/Attachment.hpp>
#include <rendering/GpuImage.hpp>
#include <rendering/RenderInterface.hpp>

#include <engine/EngineGlobals.hpp>

#include <Attachment.generated.inl>

namespace Hyperion {

AttachmentBase::AttachmentBase(
    const GpuImageRef& image,
    const GpuImageViewRef& imageView, // May be null
    const FramebufferWeakRef& framebuffer,
    const AttachmentDesc& attachmentDesc)
    : Texture(),
      m_imageView(imageView),
      m_framebuffer(framebuffer),
      m_attachmentDesc(attachmentDesc)
{
    m_gpuImage = image;

    if (m_gpuImage.IsValid())
    {
        m_textureDesc = m_gpuImage->GetTextureDesc();
    }
    else
    {
        m_textureDesc.type = attachmentDesc.imageType;
        m_textureDesc.format = attachmentDesc.format;
        m_textureDesc.imageUsage |= IU_SAMPLED | IU_ATTACHMENT;
        m_textureDesc.imageUsage &= ~(IU_STORAGE | IU_EXTERNAL);

        m_gpuImage = RI.MakeImage(m_textureDesc);
        Assert(m_gpuImage.IsValid());
    }
}

AttachmentBase::AttachmentBase(
    const TextureDesc& textureDesc,
    const FramebufferWeakRef& framebuffer,
    const AttachmentDesc& attachmentDesc)
    : Texture(textureDesc),
      m_framebuffer(framebuffer),
      m_attachmentDesc(attachmentDesc)
{
    m_textureDesc.imageUsage |= IU_SAMPLED | IU_ATTACHMENT;
    m_textureDesc.imageUsage &= ~(IU_STORAGE | IU_EXTERNAL);

    m_gpuImage = RI.MakeImage(m_textureDesc);
    Assert(m_gpuImage.IsValid());
}

} // namespace Hyperion
