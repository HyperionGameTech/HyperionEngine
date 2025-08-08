/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

#include <core/math/Rect.hpp>

#include <core/utilities/EnumFlags.hpp>

#include <rendering/RenderResult.hpp>
#include <rendering/Shared.hpp>
#include <rendering/RenderObject.hpp>

#include <core/Types.hpp>

namespace hyperion {

enum ShaderModuleType : uint32;

class GpuImageBase : public RenderObject<GpuImageBase>
{
public:
    virtual ~GpuImageBase() override = default;

    HYP_FORCE_INLINE const TextureDesc& GetTextureDesc() const
    {
        return m_textureDesc;
    }

    HYP_FORCE_INLINE ResourceState GetResourceState() const
    {
        return m_resourceState;
    }

    HYP_FORCE_INLINE TextureType GetType() const
    {
        return m_textureDesc.type;
    }

    HYP_FORCE_INLINE uint32 NumLayers() const
    {
        return m_textureDesc.numLayers;
    }

    HYP_FORCE_INLINE uint32 NumFaces() const
    {
        return m_textureDesc.NumFaces();
    }

    HYP_FORCE_INLINE TextureFilterMode GetMinFilterMode() const
    {
        return m_textureDesc.filterModeMin;
    }

    HYP_FORCE_INLINE void SetMinFilterMode(TextureFilterMode filterMode)
    {
        m_textureDesc.filterModeMin = filterMode;
    }

    HYP_FORCE_INLINE TextureFilterMode GetMagFilterMode() const
    {
        return m_textureDesc.filterModeMag;
    }

    HYP_FORCE_INLINE void SetMagFilterMode(TextureFilterMode filterMode)
    {
        m_textureDesc.filterModeMag = filterMode;
    }

    HYP_FORCE_INLINE const Vec3u& GetExtent() const
    {
        return m_textureDesc.extent;
    }

    HYP_FORCE_INLINE TextureFormat GetTextureFormat() const
    {
        return m_textureDesc.format;
    }

    HYP_FORCE_INLINE void SetTextureFormat(TextureFormat format)
    {
        m_textureDesc.format = format;
    }

    HYP_FORCE_INLINE bool HasMipmaps() const
    {
        return m_textureDesc.HasMipmaps();
    }

    HYP_FORCE_INLINE uint32 NumMipmaps() const
    {
        return m_textureDesc.NumMipmaps();
    }

    /*! \brief Returns the byte-size of the image, computed using the TextureDesc */
    HYP_FORCE_INLINE uint32 GetByteSize() const
    {
        return m_textureDesc.GetByteSize();
    }

    virtual bool IsCreated() const = 0;

    /*! \brief Returns true if the underlying GPU image is owned by this object. */
    virtual bool IsOwned() const = 0;

    virtual RendererResult Create() = 0;
    virtual RendererResult Create(ResourceState initialState) = 0;
    virtual RendererResult Destroy() = 0;

    virtual RendererResult Resize(const Vec3u& extent) = 0;

    virtual void InsertBarrier(
        CommandBufferBase* commandBuffer,
        ResourceState newState,
        ShaderModuleType shaderModuleType) = 0;

    virtual void InsertBarrier(
        CommandBufferBase* commandBuffer,
        const ImageSubResource& subResource,
        ResourceState newState,
        ShaderModuleType shaderModuleType) = 0;

    virtual RendererResult Blit(
        CommandBufferBase* commandBuffer,
        const GpuImageBase* src) = 0;

    virtual RendererResult Blit(
        CommandBufferBase* commandBuffer,
        const GpuImageBase* src,
        uint32 srcMip,
        uint32 dstMip,
        uint32 srcFace,
        uint32 dstFace) = 0;

    virtual RendererResult Blit(
        CommandBufferBase* commandBuffer,
        const GpuImageBase* src,
        Rect<uint32> srcRect,
        Rect<uint32> dstRect) = 0;

    virtual RendererResult Blit(
        CommandBufferBase* commandBuffer,
        const GpuImageBase* src,
        Rect<uint32> srcRect,
        Rect<uint32> dstRect,
        uint32 srcMip,
        uint32 dstMip,
        uint32 srcFace,
        uint32 dstFace) = 0;

    virtual RendererResult GenerateMipmaps(CommandBufferBase* commandBuffer) = 0;

    virtual void CopyFromBuffer(
        CommandBufferBase* commandBuffer,
        const GpuBufferBase* srcBuffer) const = 0;

    virtual void CopyToBuffer(
        CommandBufferBase* commandBuffer,
        GpuBufferBase* dstBuffer) const = 0;

    virtual GpuImageViewRef MakeLayerImageView(uint32 layerIndex) const = 0;

protected:
    GpuImageBase()
        : m_resourceState(RS_UNDEFINED)
    {
    }

    GpuImageBase(const TextureDesc& textureDesc)
        : m_textureDesc(textureDesc),
          m_resourceState(RS_UNDEFINED)
    {
    }

    TextureDesc m_textureDesc;
    mutable ResourceState m_resourceState;
};

} // namespace hyperion
