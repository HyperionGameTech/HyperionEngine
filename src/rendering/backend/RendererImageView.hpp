/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BACKEND_RENDERER_IMAGE_VIEW_HPP
#define HYPERION_BACKEND_RENDERER_IMAGE_VIEW_HPP

#include <rendering/backend/Platform.hpp>
#include <rendering/backend/RendererResult.hpp>
#include <core/Defines.hpp>

namespace hyperion {
namespace renderer {
namespace platform {

template <PlatformType PLATFORM>
class Device;

template <PlatformType PLATFORM>
class Image;

template <PlatformType PLATFORM>
struct ImageViewPlatformImpl;

template <PlatformType PLATFORM>
class ImageView
{
public:
    friend struct ImageViewPlatformImpl<PLATFORM>;
    
    static constexpr PlatformType platform = PLATFORM;

    HYP_API ImageView();
    ImageView(const ImageView &other)               = delete;
    ImageView &operator=(const ImageView &other)    = delete;
    HYP_API ImageView(ImageView &&other) noexcept;
    HYP_API ImageView &operator=(ImageView &&other) noexcept;
    HYP_API ~ImageView();
    
    HYP_FORCE_INLINE ImageViewPlatformImpl<PLATFORM> &GetPlatformImpl()
        { return m_platform_impl; }
    
    HYP_FORCE_INLINE const ImageViewPlatformImpl<PLATFORM> &GetPlatformImpl() const
        { return m_platform_impl; }
    
    HYP_FORCE_INLINE uint32 NumFaces() const
        { return m_num_faces; }

    HYP_API bool IsCreated() const;

    HYP_API RendererResult Create(
        const Image<PLATFORM> *image,
        uint32 mipmap_layer,
        uint32 num_mipmaps,
        uint32 face_layer,
        uint32 num_faces
    );

    HYP_API RendererResult Create(
        const Image<PLATFORM> *image
    );

    HYP_API RendererResult Destroy();

private:
    ImageViewPlatformImpl<PLATFORM> m_platform_impl;

    uint32                          m_num_faces;
};

} // namespace platform
} // namespace renderer
} // namespace hyperion

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererImageView.hpp>
#else
#error Unsupported rendering backend
#endif

namespace hyperion {
namespace renderer {

using ImageView = platform::ImageView<Platform::CURRENT>;

} // namespace renderer
} // namespace hyperion

#endif