/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#include <rendering/GpuImageView.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <GpuImageView.generated.inl>

namespace Hyperion {

GpuImageViewBase::GpuImageViewBase(const GpuImageRef& image)
    : m_image(image),
      m_mipIndex(0),
      m_numMips(0),
      m_layerIndex(0),
      m_numLayers(0)
{
    if (image.IsValid())
    {
        const TextureDesc& textureDesc = image->GetTextureDesc();

        m_numMips = textureDesc.NumMips();
        m_numLayers = textureDesc.NumArrayLayers();
    }
}

GpuImageViewBase::GpuImageViewBase(
    const GpuImageRef& image,
    uint32 mipIndex,
    uint32 numMips,
    uint32 layerIndex,
    uint32 numLayers)
    : m_image(image),
      m_mipIndex(mipIndex),
      m_numMips(numMips),
      m_layerIndex(layerIndex),
      m_numLayers(numLayers)
{
    if (image.IsValid())
    {
        const TextureDesc& textureDesc = image->GetTextureDesc();

        const uint32 maxMipLevel = textureDesc.NumMips() - 1;
        const uint32 maxArrayLayer = textureDesc.NumArrayLayers() - 1;

        m_numMips = MathUtil::Min(numMips, maxMipLevel + 1);
        m_numLayers = MathUtil::Min(numLayers, maxArrayLayer + 1);
        
        m_mipIndex = MathUtil::Min(mipIndex, maxMipLevel);
        m_layerIndex = MathUtil::Min(layerIndex, maxArrayLayer);
    }
}

GpuImageViewBase::~GpuImageViewBase()
{
    if (m_image.IsValid())
    {
        SafeDelete(std::move(m_image));
    }
}

} // namespace Hyperion
