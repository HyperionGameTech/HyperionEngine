/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <Core/math/Rect.hpp>

#include <Core/utilities/EnumFlags.hpp>

#include <Rendering/RenderResult.hpp>
#include <Rendering/Shared.hpp>
#include <Rendering/RenderTypes.hpp>

#include <Core/Types.hpp>

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

    static Pool* GetAllocator() { return g_rhiPool; }

    virtual ~GpuImageBase() override = default;

#if HYP_DEBUG_MODE
    Name GetDebugName() const
    {
        return m_debugName;
    }

    virtual void SetDebugName(Name name)
    {
        m_debugName = name;
    }
#endif

    HYP_FORCE_INLINE const TextureDesc& GetTextureDesc() const
    {
        return m_textureDesc;
    }

    /*! \brief Gets the current resource state of the image. If this is a depth-stencil image, this is the state of the depth aspect. */
    HYP_FORCE_INLINE ResourceState GetResourceState() const
    {
        return m_resourceState;
    }

    /*! \brief Get the resource state of the stencil aspect of this image. Only valid if the image has a depth-stencil format. */
    HYP_FORCE_INLINE ResourceState GetStencilState() const
    {
        return m_stencilState;
    }

    void SetStencilState(ResourceState newState);
    void SetResourceState(ResourceState newState);

    ResourceState GetSubResourceState(const ImageSubResource& subResource) const;
    void SetSubResourceState(const ImageSubResource& subResource, ResourceState newState);

    /*! \brief Checks if the given ImageSubResource \p subResource makes up the entirety of the image */
    bool IsFullSubResource(const ImageSubResource& subResource) const;

    HYP_FORCE_INLINE bool HasSubResourceStates() const
    {
        return m_subResourceStates.Any();
    }

    HYP_FORCE_INLINE TextureType GetType() const
    {
        return m_textureDesc.type;
    }

    HYP_FORCE_INLINE uint16 NumArrayLayers() const
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

    virtual void InsertBarrier(
        CommandBuffer* commandBuffer,
        ResourceState newState,
        ShaderModuleType shaderModuleType,
        bool onlyDepth = false,
        bool onlyStencil = false) = 0;

    virtual void InsertBarrier(
        CommandBuffer* commandBuffer,
        const ImageSubResource& subResource,
        ResourceState newState,
        ShaderModuleType shaderModuleType,
        bool onlyDepth = false,
        bool onlyStencil = false) = 0;

    virtual void Blit(
        CommandBuffer* commandBuffer,
        const GpuImage* srcImage) = 0;

    virtual void Blit(
        CommandBuffer* commandBuffer,
        const GpuImage* srcImage,
        const Rect<uint32>& srcRect,
        const Rect<uint32>& dstRect) = 0;

    virtual void Blit(
        CommandBuffer* commandBuffer,
        const GpuImage* srcImage,
        const Rect<uint32>& srcRect,
        const Rect<uint32>& dstRect,
        const ImageSubResource& srcSubResource,
        const ImageSubResource& dstSubResource) = 0;

    virtual RendererResult GenerateMipmaps(CommandBuffer* commandBuffer) = 0;

    /*! \brief Inserts a UAV barrier to ensure UAV writes complete before subsequent reads.
     *  Only required on some backends (DX12). Default implementation does nothing.
     *  \param commandBuffer The command buffer to insert the barrier into. */
    virtual void InsertUAVBarrier(CommandBuffer* commandBuffer) {}

    virtual void CopyFromBuffer(
        CommandBuffer* commandBuffer,
        const GpuBuffer* srcBuffer,
        uint32 bufferOffset = 0,
        uint8 dstMipIndex = UINT8_MAX,
        uint16 dstArrayLayer = UINT16_MAX) const = 0;

    virtual void CopyToBuffer(
        CommandBuffer* commandBuffer,
        GpuBuffer* dstBuffer,
        const ImageSubResource& subResource) const = 0;

    virtual void Fill(
        CommandBuffer* commandBuffer,
        float value,
        const ImageSubResource& subResource,
        const Vec3u& offset = Vec3u::Zero(),
        const Vec3u& extent = Vec3u::One()) = 0;

    virtual void CopyFrom(
        CommandBuffer* commandBuffer,
        const GpuImage* srcImage,
        const Vec3u& srcOffset,
        const Vec3u& dstOffset,
        const Vec3u& extent,
        const ImageSubResource& srcSubResource,
        const ImageSubResource& dstSubResource) = 0;

    virtual GpuImageViewRef MakeLayerImageView(uint32 layerIndex) const = 0;

protected:
    explicit GpuImageBase(EnumFlags<GpuImageFlags> flags = GpuImageFlags::NONE)
        : m_resourceState(RS_UNDEFINED),
          m_stencilState(RS_UNDEFINED),
          m_flags(flags)
    {
    }

    explicit GpuImageBase(const TextureDesc& textureDesc, EnumFlags<GpuImageFlags> flags = GpuImageFlags::NONE)
        : m_textureDesc(textureDesc),
          m_resourceState(RS_UNDEFINED),
          m_stencilState(RS_UNDEFINED),
          m_flags(flags)
    {
    }

    TextureDesc m_textureDesc;

    ResourceState m_resourceState;
    ResourceState m_stencilState;

    TMap<uint64, ResourceState, RHIAllocator> m_subResourceStates;

    EnumFlags<GpuImageFlags> m_flags;

#if HYP_DEBUG_MODE
    Name m_debugName;
#endif
};

} // namespace Hyperion

#ifndef INCLUDE_FROM_RHI
#define INCLUDE_FROM_RHI_BASE

#if HYP_VULKAN
#include <Rendering/vulkan/VulkanGpuImage.hpp>
#elif HYP_DX12
#include <Rendering/dx12/DX12GpuImage.hpp>
#endif

#undef INCLUDE_FROM_RHI_BASE
#else
#undef INCLUDE_FROM_RHI
#endif
