/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BACKEND_RENDERER_IMAGE_HPP
#define HYPERION_BACKEND_RENDERER_IMAGE_HPP

#include <core/Defines.hpp>

#include <core/math/Rect.hpp>

#include <core/utilities/EnumFlags.hpp>

#include <rendering/backend/RendererResult.hpp>
#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/Platform.hpp>

#include <Types.hpp>

namespace hyperion::renderer {

enum ShaderModuleType : uint32;

class ImageBase : public RenderObject<ImageBase>
{
public:
    virtual ~ImageBase() override = default;

    HYP_FORCE_INLINE const TextureDesc &GetTextureDesc() const
        { return m_texture_desc; }

    HYP_FORCE_INLINE ResourceState GetResourceState() const
        { return m_resource_state; }

    HYP_FORCE_INLINE ImageType GetType() const
        { return m_texture_desc.type; }

    HYP_FORCE_INLINE bool IsTextureCube() const
        { return m_texture_desc.IsTextureCube(); }

    HYP_FORCE_INLINE bool IsPanorama() const
        { return m_texture_desc.IsPanorama(); }

    HYP_FORCE_INLINE bool IsTextureArray() const
        { return m_texture_desc.IsTextureArray(); }

    HYP_FORCE_INLINE bool IsTexture3D() const
        { return m_texture_desc.IsTexture3D(); }

    HYP_FORCE_INLINE bool IsTexture2D() const
        { return m_texture_desc.IsTexture2D(); }

    HYP_FORCE_INLINE uint32 NumLayers() const
        { return m_texture_desc.num_layers; }

    HYP_FORCE_INLINE uint32 NumFaces() const
        { return m_texture_desc.NumFaces(); }

    HYP_FORCE_INLINE FilterMode GetMinFilterMode() const
        { return m_texture_desc.filter_mode_min; }

    HYP_FORCE_INLINE void SetMinFilterMode(FilterMode filter_mode)
        { m_texture_desc.filter_mode_min = filter_mode; }

    HYP_FORCE_INLINE FilterMode GetMagFilterMode() const
        { return m_texture_desc.filter_mode_mag; }

    HYP_FORCE_INLINE void SetMagFilterMode(FilterMode filter_mode)
        { m_texture_desc.filter_mode_mag = filter_mode; }

    HYP_FORCE_INLINE const Vec3u &GetExtent() const
        { return m_texture_desc.extent; }

    HYP_FORCE_INLINE InternalFormat GetTextureFormat() const
        { return m_texture_desc.format; }

    HYP_FORCE_INLINE void SetTextureFormat(InternalFormat format)
        { m_texture_desc.format = format; }

    HYP_FORCE_INLINE bool HasMipmaps() const
        { return m_texture_desc.HasMipmaps(); }

    HYP_FORCE_INLINE uint32 NumMipmaps() const
        { return m_texture_desc.NumMipmaps(); }

    /*! \brief Returns the byte-size of the image. Note, it's possible no CPU-side memory exists
        for the image data even if the result is non-zero. To check if any CPU-side bytes exist,
        use HasAssignedImageData(). */
    HYP_FORCE_INLINE uint32 GetByteSize() const
        { return m_texture_desc.GetByteSize(); }

    HYP_API virtual bool IsCreated() const = 0;

    HYP_API virtual RendererResult Create() = 0;
    HYP_API virtual RendererResult Create(ResourceState initial_state) = 0;
    HYP_API virtual RendererResult Destroy() = 0;

    HYP_API virtual void InsertBarrier(
        CommandBufferBase *command_buffer,
        ResourceState new_state,
        ShaderModuleType shader_module_type
    ) = 0;

    HYP_API virtual void InsertBarrier(
        CommandBufferBase *command_buffer,
        const ImageSubResource &sub_resource,
        ResourceState new_state,
        ShaderModuleType shader_module_type
    ) = 0;

    HYP_API virtual void InsertSubResourceBarrier(
        CommandBufferBase *command_buffer,
        const ImageSubResource &sub_resource,
        ResourceState new_state,
        ShaderModuleType shader_module_type
    ) = 0;

    HYP_API virtual RendererResult Blit(
        CommandBufferBase *command_buffer,
        const ImageBase *src
    ) = 0;

    HYP_API virtual RendererResult Blit(
        CommandBufferBase *command_buffer,
        const ImageBase *src,
        Rect<uint32> src_rect,
        Rect<uint32> dst_rect
    ) = 0;

    HYP_API virtual RendererResult Blit(
        CommandBufferBase *command_buffer,
        const ImageBase *src,
        Rect<uint32> src_rect,
        Rect<uint32> dst_rect,
        uint32 src_mip,
        uint32 dst_mip
    ) = 0;

    HYP_API virtual RendererResult GenerateMipmaps(CommandBufferBase *command_buffer) = 0;

    HYP_API virtual void CopyFromBuffer(
        CommandBufferBase *command_buffer,
        const GPUBufferBase *src_buffer
    ) const = 0;

    HYP_API virtual void CopyToBuffer(
        CommandBufferBase *command_buffer,
        GPUBufferBase *dst_buffer
    ) const = 0;

protected:
    ImageBase()
        : m_resource_state(ResourceState::UNDEFINED)
    {
    }

    ImageBase(const TextureDesc &texture_desc)
        : m_texture_desc(texture_desc),
          m_resource_state(ResourceState::UNDEFINED)
    {
    }

    TextureDesc             m_texture_desc;
    mutable ResourceState   m_resource_state;
};

} // namespace hyperion::renderer

#endif