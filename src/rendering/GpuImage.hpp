/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

#include <core/math/Rect.hpp>

#include <core/utilities/EnumFlags.hpp>

#include <rendering/RenderResult.hpp>
#include <rendering/Shared.hpp>
#include <rendering/RenderObject.hpp>

#include <core/Types.hpp>

namespace Hyperion {

enum class ShaderModuleType : uint8;

HYP_ENUM()
enum class GpuImageFlags : uint32
{
    NONE = 0x0
};

HYP_MAKE_ENUM_FLAGS(GpuImageFlags);

HYP_CLASS(Abstract, NoScriptBindings)
class GpuImageBase : public ObjectBase
{
    HYP_OBJECT_BODY(GpuImageBase);

public:
#ifndef HYP_WINDOWS
    using HANDLE = void*;
#endif

    virtual ~GpuImageBase() override = default;

    Name GetDebugName() const
    {
        return m_debugName;
    }

    virtual void SetDebugName(Name name)
    {
        m_debugName = name;
    }

    HYP_FORCE_INLINE const TextureDesc& GetTextureDesc() const
    {
        return m_textureDesc;
    }

    HYP_FORCE_INLINE ResourceState GetResourceState() const
    {
        return m_resourceState;
    }

    virtual void SetResourceState(ResourceState newState)
    {
        m_resourceState = newState;
    }

    HYP_FORCE_INLINE TextureType GetType() const
    {
        return m_textureDesc.type;
    }

    HYP_FORCE_INLINE uint32 NumLayers() const
    {
        return m_textureDesc.numLayers;
    }

    HYP_FORCE_INLINE uint32 NumArrayLayers() const
    {
        return m_textureDesc.NumArrayLayers();
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

    HYP_FORCE_INLINE bool HasMipMaps() const
    {
        return m_textureDesc.HasMipMaps();
    }

    HYP_FORCE_INLINE uint32 NumMips() const
    {
        return m_textureDesc.NumMips();
    }

    /*! \brief Returns the byte-size of the image, computed using the TextureDesc */
    HYP_FORCE_INLINE uint32 GetByteSize() const
    {
        return m_textureDesc.GetByteSize();
    }

    HYP_FORCE_INLINE EnumFlags<GpuImageFlags> GetFlags() const
    {
        return m_flags;
    }

    virtual bool IsCreated() const = 0;

    /*! \brief Returns true if the underlying GPU image is owned by this object. */
    virtual bool IsOwned() const = 0;

    virtual RendererResult Create() = 0;
    virtual RendererResult Create(ResourceState initialState) = 0;

    virtual RendererResult Resize(const Vec3u& extent) = 0;

    /*! \brief Returns the native handle of the underlying GPU image.
     *   Only valid on images created with IU_EXTERNAL usage flag. */
    virtual HANDLE GetNativeHandle() const = 0;

    virtual void InsertBarrier(
        CommandBuffer* commandBuffer,
        ResourceState newState,
        ShaderModuleType shaderModuleType) = 0;

    virtual void InsertBarrier(
        CommandBuffer* commandBuffer,
        const ImageSubResource& subResource,
        ResourceState newState,
        ShaderModuleType shaderModuleType) = 0;

    virtual RendererResult Blit(
        CommandBuffer* commandBuffer,
        const GpuImage* srcImage) = 0;

    virtual RendererResult Blit(
        CommandBuffer* commandBuffer,
        const GpuImage* srcImage,
        Rect<uint32> srcRect,
        Rect<uint32> dstRect) = 0;

    virtual RendererResult Blit(
        CommandBuffer* commandBuffer,
        const GpuImage* srcImage,
        Rect<uint32> srcRect,
        Rect<uint32> dstRect,
        const ImageSubResource& srcSubResource,
        const ImageSubResource& dstSubResource) = 0;

    virtual RendererResult GenerateMipmaps(CommandBuffer* commandBuffer) = 0;

    virtual void CopyFromBuffer(
        CommandBuffer* commandBuffer,
        const GpuBuffer* srcBuffer,
        uint32 bufferOffset = 0,
        uint8 dstMipIndex = UINT8_MAX,
        uint16 dstArrayLayer = UINT16_MAX) const = 0;

    virtual void CopyToBuffer(
        CommandBuffer* commandBuffer,
        GpuBuffer* dstBuffer) const = 0;

    virtual GpuImageViewRef MakeLayerImageView(uint32 layerIndex) const = 0;

protected:
    explicit GpuImageBase(EnumFlags<GpuImageFlags> flags = GpuImageFlags::NONE)
        : m_resourceState(RS_UNDEFINED),
          m_flags(flags)
    {
    }

    explicit GpuImageBase(const TextureDesc& textureDesc, EnumFlags<GpuImageFlags> flags = GpuImageFlags::NONE)
        : m_textureDesc(textureDesc),
          m_resourceState(RS_UNDEFINED),
          m_flags(flags)
    {
    }

    TextureDesc m_textureDesc;
    mutable ResourceState m_resourceState;
    EnumFlags<GpuImageFlags> m_flags;

    Name m_debugName;
};

} // namespace Hyperion

#ifndef INCLUDE_FROM_RHI
#define INCLUDE_FROM_RHI_BASE

#if HYP_VULKAN
#include <rendering/vulkan/VulkanGpuImage.hpp>
#elif HYP_DX12
#include <rendering/dx12/DX12GpuImage.hpp>
#endif

#undef INCLUDE_FROM_RHI_BASE
#else
#undef INCLUDE_FROM_RHI
#endif
