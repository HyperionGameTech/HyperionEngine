/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_TEXTURE_HPP
#define HYPERION_TEXTURE_HPP

#include <core/Base.hpp>
#include <core/Handle.hpp>
#include <core/memory/UniquePtr.hpp>
#include <core/containers/FixedArray.hpp>

#include <streaming/StreamedData.hpp>

#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererImageView.hpp>

#include <scene/VisibilityState.hpp>

#include <math/Vector2.hpp>
#include <math/Vector3.hpp>
#include <math/Vector4.hpp>

#include <Types.hpp>

namespace hyperion {

class HYP_API Texture
    : public BasicObject<STUB_CLASS(Texture)>
{
public:
    static const FixedArray<std::pair<Vector3, Vector3>, 6> cubemap_directions;

    Texture();

    Texture(
        ImageRef image,
        ImageViewRef image_view
    );

    Texture(
        const TextureDesc &texture_desc,
        UniquePtr<StreamedData> &&streamed_data
    );

    Texture(
        Image &&image
    );

    Texture(const Texture &other)                   = delete;
    Texture &operator=(const Texture &other)        = delete;

    Texture(Texture &&other) noexcept;
    Texture &operator=(Texture &&other) noexcept    = delete;

    ~Texture();
    
    HYP_NODISCARD HYP_FORCE_INLINE
    const ImageRef &GetImage() const
        { return m_image; }

    HYP_NODISCARD HYP_FORCE_INLINE
    const ImageViewRef &GetImageView() const
        { return m_image_view; }

    HYP_NODISCARD HYP_FORCE_INLINE
    const TextureDesc &GetTextureDescriptor() const
        { return m_image->GetTextureDescriptor(); }

    HYP_NODISCARD HYP_FORCE_INLINE
    ImageType GetType() const
        { return GetTextureDescriptor().type; }

    HYP_NODISCARD HYP_FORCE_INLINE
    uint32 NumFaces() const
        { return GetTextureDescriptor().num_faces; }

    HYP_NODISCARD HYP_FORCE_INLINE
    bool IsTextureCube() const
        { return m_image->IsTextureCube(); }

    HYP_NODISCARD HYP_FORCE_INLINE
    bool IsPanorama() const
        { return m_image->IsPanorama(); }

    HYP_NODISCARD HYP_FORCE_INLINE
    const Extent3D &GetExtent() const
        { return GetTextureDescriptor().extent; }

    HYP_NODISCARD HYP_FORCE_INLINE
    InternalFormat GetFormat() const
        { return GetTextureDescriptor().format; }

    HYP_NODISCARD HYP_FORCE_INLINE HYP_DEPRECATED
    FilterMode GetFilterMode() const
        { return GetTextureDescriptor().filter_mode_min; }

    HYP_NODISCARD HYP_FORCE_INLINE
    FilterMode GetMinFilterMode() const
        { return GetTextureDescriptor().filter_mode_min; }

    HYP_NODISCARD HYP_FORCE_INLINE
    FilterMode GetMagFilterMode() const
        { return GetTextureDescriptor().filter_mode_mag; }

    HYP_NODISCARD HYP_FORCE_INLINE
    WrapMode GetWrapMode() const
        { return GetTextureDescriptor().wrap_mode; }
    
    void Init();

    void GenerateMipmaps();

    Vec4f Sample(Vec2f uv) const;

protected:
    ImageRef        m_image;
    ImageViewRef    m_image_view;
};

class HYP_API Texture2D : public Texture
{
public:
    Texture2D(
        Extent2D extent,
        InternalFormat format,
        FilterMode filter_mode,
        WrapMode wrap_mode,
        UniquePtr<StreamedData> &&streamed_data
    ) : Texture(
            TextureDesc
            {
                ImageType::TEXTURE_TYPE_2D,
                format,
                Extent3D { extent.width, extent.height, 1 },
                filter_mode,
                filter_mode,
                wrap_mode
            },
            std::move(streamed_data)
        )
    {
    }

    Texture2D(
        Extent2D extent,
        InternalFormat format,
        UniquePtr<StreamedData> &&streamed_data
    ) : Texture(
            TextureDesc
            {
                ImageType::TEXTURE_TYPE_2D,
                format,
                Extent3D { extent.width, extent.height, 1 }
            },
            std::move(streamed_data)
        )
    {
    }
};

} // namespace hyperion

#endif