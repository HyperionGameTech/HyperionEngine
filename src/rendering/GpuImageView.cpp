/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#include <rendering/GpuImageView.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <GpuImageView.generated.inl>

namespace Hyperion {

GpuImageViewBase::GpuImageViewBase(const GpuImageRef& image)
    : m_image(image)
{
    if (image.IsValid())
    {
        const TextureDesc& textureDesc = image->GetTextureDesc();

        m_subResource.baseMipLevel = 0;
        m_subResource.numLevels = textureDesc.NumMips();
        m_subResource.baseArrayLayer = 0;
        m_subResource.numLayers = textureDesc.NumArrayLayers();
    }
}

GpuImageViewBase::GpuImageViewBase(const GpuImageRef& image, const ImageSubResource& subResource)
    : m_image(image),
      m_subResource(subResource)
{
    if (image.IsValid())
    {
        const TextureDesc& textureDesc = image->GetTextureDesc();

        const int descNumMips = int(textureDesc.NumMips());
        const int descNumLayers = int(textureDesc.NumArrayLayers());
        
        m_subResource.baseMipLevel = MathUtil::Max(0, MathUtil::Min(int(m_subResource.baseMipLevel), descNumMips - 1));
        m_subResource.baseArrayLayer = MathUtil::Max(0, MathUtil::Min(int(m_subResource.baseArrayLayer), descNumLayers - 1));
        m_subResource.numLevels = MathUtil::Max(1, MathUtil::Min(int(m_subResource.numLevels), descNumMips - int(m_subResource.baseMipLevel)));
        m_subResource.numLayers = MathUtil::Max(1, MathUtil::Min(int(m_subResource.numLayers), descNumLayers - int(m_subResource.baseArrayLayer)));
    }
}

GpuImageViewBase::~GpuImageViewBase() = default;

} // namespace Hyperion
