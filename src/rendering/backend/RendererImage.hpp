/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BACKEND_RENDERER_IMAGE_HPP
#define HYPERION_BACKEND_RENDERER_IMAGE_HPP

#include <core/Defines.hpp>

#include <core/math/MathUtil.hpp>
#include <core/math/Rect.hpp>

#include <core/utilities/EnumFlags.hpp>

#include <rendering/backend/RendererResult.hpp>
#include <rendering/backend/RendererBuffer.hpp>
#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/Platform.hpp>

#include <Types.hpp>

namespace hyperion::renderer {

namespace platform {

template <PlatformType PLATFORM>
struct ImagePlatformImpl;

template <PlatformType PLATFORM>
class Image
{
public:
    friend struct ImagePlatformImpl<PLATFORM>;

    static constexpr PlatformType platform = PLATFORM;
    
    HYP_API Image(const TextureDesc &texture_desc);

    Image(const Image &other)               = delete;
    Image &operator=(const Image &other)    = delete;
    HYP_API Image(Image &&other) noexcept;
    HYP_API Image &operator=(Image &&other) noexcept;
    HYP_API ~Image();

    HYP_FORCE_INLINE ImagePlatformImpl<PLATFORM> &GetPlatformImpl()
        { return m_platform_impl; }

    HYP_FORCE_INLINE const ImagePlatformImpl<PLATFORM> &GetPlatformImpl() const
        { return m_platform_impl; }

    HYP_FORCE_INLINE const TextureDesc &GetTextureDesc() const
        { return m_texture_desc; }

    HYP_API RendererResult Create();
    HYP_API RendererResult Create(ResourceState initial_state);
    
    HYP_API RendererResult Destroy();

    HYP_API bool IsCreated() const;

    HYP_API ResourceState GetResourceState() const;

    HYP_API ResourceState GetSubResourceState(const ImageSubResource &sub_resource) const;

    HYP_API void SetSubResourceState(const ImageSubResource &sub_resource, ResourceState new_state);

    HYP_API void InsertBarrier(
        CommandBuffer<PLATFORM> *command_buffer,
        ResourceState new_state,
        ShaderModuleType shader_module_type
    );

    HYP_API void InsertBarrier(
        CommandBuffer<PLATFORM> *command_buffer,
        const ImageSubResource &sub_resource,
        ResourceState new_state,
        ShaderModuleType shader_module_type
    );

    HYP_API void InsertSubResourceBarrier(
        CommandBuffer<PLATFORM> *command_buffer,
        const ImageSubResource &sub_resource,
        ResourceState new_state,
        ShaderModuleType shader_module_type
    );

    HYP_API RendererResult Blit(
        CommandBuffer<PLATFORM> *command_buffer,
        const Image *src
    );

    HYP_API RendererResult Blit(
        CommandBuffer<PLATFORM> *command_buffer,
        const Image *src,
        Rect<uint32> src_rect,
        Rect<uint32> dst_rect
    );

    HYP_API RendererResult Blit(
        CommandBuffer<PLATFORM> *command_buffer,
        const Image *src,
        Rect<uint32> src_rect,
        Rect<uint32> dst_rect,
        uint32 src_mip,
        uint32 dst_mip
    );

    HYP_API RendererResult GenerateMipmaps(CommandBuffer<PLATFORM> *command_buffer);

    HYP_API void CopyFromBuffer(
        CommandBuffer<PLATFORM> *command_buffer,
        const GPUBuffer<PLATFORM> *src_buffer
    ) const;

    HYP_API void CopyToBuffer(
        CommandBuffer<PLATFORM> *command_buffer,
        GPUBuffer<PLATFORM> *dst_buffer
    ) const;

    HYP_FORCE_INLINE bool HasMipmaps() const
    {
        return m_texture_desc.HasMipmaps();
    }

    HYP_FORCE_INLINE uint32 NumMipmaps() const
    {
        return m_texture_desc.NumMipmaps();
    }

    /*! \brief Returns the byte-size of the image. Note, it's possible no CPU-side memory exists
        for the image data even if the result is non-zero. To check if any CPU-side bytes exist,
        use HasAssignedImageData(). */
    HYP_FORCE_INLINE uint32 GetByteSize() const
        { return m_texture_desc.GetByteSize(); }

    HYP_FORCE_INLINE uint8 GetBPP() const
        { return m_bpp; }

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

    HYP_FORCE_INLINE ImageType GetType() const
        { return m_texture_desc.type; }
    
private:
    ImagePlatformImpl<PLATFORM>                 m_platform_impl;

    TextureDesc                                 m_texture_desc;

    SizeType                                    m_size;
    SizeType                                    m_bpp; // bytes per pixel
};

} // namespace platform
} // namespace hyperion::renderer

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererImage.hpp>
#else
#error Unsupported rendering backend
#endif

namespace hyperion {
namespace renderer {

using Image = platform::Image<Platform::CURRENT>;

} // namespace renderer

} // namespace hyperion

#endif