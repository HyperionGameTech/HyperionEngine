/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_TEXTURE_HPP
#define HYPERION_TEXTURE_HPP

#include <core/Base.hpp>
#include <core/Handle.hpp>

#include <core/memory/UniquePtr.hpp>

#include <core/containers/FixedArray.hpp>

#include <core/utilities/Pair.hpp>

#include <core/object/HypObject.hpp>

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

HYP_CLASS()
class HYP_API Texture : public BasicObject<Texture>
{
    HYP_OBJECT_BODY(Texture);

    HYP_PROPERTY(ID, &Texture::GetID);

public:
    static const FixedArray<Pair<Vec3f, Vec3f>, 6> cubemap_directions;

    Texture();

    Texture(
        ImageRef image,
        ImageViewRef image_view
    );

    Texture(const TextureDesc &texture_desc);
    Texture(const RC<StreamedTextureData> &streamed_data);
    Texture(RC<StreamedTextureData> &&streamed_data);

    Texture(
        Image &&image
    );

    Texture(const Texture &other)                   = delete;
    Texture &operator=(const Texture &other)        = delete;

    Texture(Texture &&other) noexcept               = delete;
    Texture &operator=(Texture &&other) noexcept    = delete;

    ~Texture();
    
    HYP_FORCE_INLINE const ImageRef &GetImage() const
        { return m_image; }

    HYP_FORCE_INLINE const ImageViewRef &GetImageView() const
        { return m_image_view; }

    HYP_FORCE_INLINE const TextureDesc &GetTextureDesc() const
        { return m_image->GetTextureDesc(); }

    HYP_FORCE_INLINE ImageType GetType() const
        { return GetTextureDesc().type; }

    HYP_FORCE_INLINE uint32 NumFaces() const
        { return GetTextureDesc().num_faces; }

    HYP_FORCE_INLINE bool IsTextureCube() const
        { return m_image->IsTextureCube(); }

    HYP_FORCE_INLINE bool IsPanorama() const
        { return m_image->IsPanorama(); }

    HYP_FORCE_INLINE const Vec3u &GetExtent() const
        { return GetTextureDesc().extent; }

    HYP_FORCE_INLINE InternalFormat GetFormat() const
        { return GetTextureDesc().format; }

    HYP_FORCE_INLINE HYP_DEPRECATED FilterMode GetFilterMode() const
        { return GetTextureDesc().filter_mode_min; }

    HYP_FORCE_INLINE FilterMode GetMinFilterMode() const
        { return GetTextureDesc().filter_mode_min; }

    HYP_FORCE_INLINE FilterMode GetMagFilterMode() const
        { return GetTextureDesc().filter_mode_mag; }

    HYP_FORCE_INLINE WrapMode GetWrapMode() const
        { return GetTextureDesc().wrap_mode; }
    
    void Init();

    void GenerateMipmaps();

    /*! \brief Copies the texture data to the CPU. Waits (blocking) for the render thread to execute the task.
     *  \note While this method is usable from any thread, it is not thread-safe as it modifies the streamed texture data. of the image.
     *  Ensure that the image is not being used in other threads before calling this method.
     * 
     *  The texture data will be copied to the CPU and the image will have its StreamedTextureData recreated. */
    void Readback() const;

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
        WrapMode wrap_mode
    ) : Texture(
            TextureDesc
            {
                ImageType::TEXTURE_TYPE_2D,
                format,
                Vec3u { extent.width, extent.height, 1 },
                filter_mode,
                filter_mode,
                wrap_mode
            }
        )
    {
    }

    Texture2D(
        Extent2D extent,
        InternalFormat format
    ) : Texture(
            TextureDesc
            {
                ImageType::TEXTURE_TYPE_2D,
                format,
                Vec3u { extent.width, extent.height, 1 }
            }
        )
    {
    }
};

} // namespace hyperion

#endif