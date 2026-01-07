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

        const int descNumMips = int(textureDesc.NumMips());
        const int descNumLayers = int(textureDesc.NumArrayLayers());
        
        m_mipIndex = MathUtil::Max(0, MathUtil::Min(int(mipIndex), descNumMips - 1));
        m_layerIndex = MathUtil::Max(0, MathUtil::Min(int(layerIndex), descNumLayers - 1));
        m_numMips = MathUtil::Max(1, MathUtil::Min(int(numMips), descNumMips - int(m_mipIndex)));
        m_numLayers = MathUtil::Max(1, MathUtil::Min(int(numLayers), descNumLayers - int(m_layerIndex)));
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
