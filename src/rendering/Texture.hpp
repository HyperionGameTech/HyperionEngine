#ifndef HYPERION_V2_TEXTURE_H
#define HYPERION_V2_TEXTURE_H

#include "Base.hpp"

#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererImageView.hpp>
#include <rendering/backend/RendererSampler.hpp>

#include <memory>
#include <map>

namespace hyperion::v2 {

using renderer::Image;
using renderer::TextureImage;
using renderer::StorageImage;
using renderer::ImageView;
using renderer::Sampler;
using renderer::Extent2D;
using renderer::Extent3D;
using renderer::CommandBuffer;

class Texture
    : public EngineComponentBase<STUB_CLASS(Texture)>,
      public RenderResource
{
public:
    Texture(
        Extent3D extent,
        Image::InternalFormat format,
        Image::Type type,
        Image::FilterMode filter_mode,
        Image::WrapMode wrap_mode,
        const unsigned char *bytes
    );

    Texture(
        Image &&image,
        Image::FilterMode filter_mode,
        Image::WrapMode wrap_mode
    );

    Texture(const Texture &other) = delete;
    Texture &operator=(const Texture &other) = delete;
    ~Texture();
    
    Image &GetImage() { return m_image; }
    const Image &GetImage() const { return m_image; }

    ImageView &GetImageView() { return m_image_view; }
    const ImageView &GetImageView() const { return m_image_view; }

    Sampler &GetSampler() { return m_sampler; }
    const Sampler &GetSampler() const { return m_sampler; }

    Image::Type GetType() const { return m_image.GetType(); }

    UInt NumFaces() const { return m_image.NumFaces(); }

    bool IsTextureCube() const { return m_image.IsTextureCube(); }
    bool IsPanorama() const { return m_image.IsPanorama(); }

    const Extent3D &GetExtent() const { return m_image.GetExtent(); }

    Image::InternalFormat GetFormat() const { return m_image.GetTextureFormat(); }
    Image::FilterMode GetFilterMode() const { return m_sampler.GetFilterMode(); }
    Image::WrapMode GetWrapMode() const { return m_sampler.GetWrapMode(); }
    
    void Init(Engine *engine);

protected:
    Image m_image;
    ImageView m_image_view;
    Sampler m_sampler;
};

class Texture2D : public Texture
{
public:
    Texture2D(
        Extent2D extent,
        Image::InternalFormat format,
        Image::FilterMode filter_mode,
        Image::WrapMode wrap_mode,
        const unsigned char *bytes
    ) : Texture(
        Extent3D(extent),
        format,
        Image::Type::TEXTURE_TYPE_2D,
        filter_mode,
        wrap_mode,
        bytes
    )
    {
    }
};

class Texture3D : public Texture
{
public:
    Texture3D(
        Extent3D extent,
        Image::InternalFormat format,
        Image::FilterMode filter_mode,
        Image::WrapMode wrap_mode,
        const unsigned char *bytes
    ) : Texture(
        extent,
        format,
        Image::Type::TEXTURE_TYPE_3D,
        filter_mode,
        wrap_mode,
        bytes
    )
    {
    }
};

class TextureCube : public Texture
{
public:
    TextureCube(
        Extent2D extent,
        Image::InternalFormat format,
        Image::FilterMode filter_mode,
        Image::WrapMode wrap_mode,
        const unsigned char *bytes
    ) : Texture(
            Extent3D(extent),
            format,
            Image::Type::TEXTURE_TYPE_CUBEMAP,
            filter_mode,
            wrap_mode,
            bytes
        )
    {
    }

    TextureCube(std::unique_ptr<Texture> &&other) noexcept
        : Texture(
              other
                ? (other->IsTextureCube()
                    ? other->GetImage().GetExtent()
                    : (other->IsPanorama()
                        ? Extent3D(other->GetImage().GetExtent().width / 4, other->GetImage().GetExtent().height / 3, 1)
                        : (other->GetImage().GetExtent() / Extent3D(6, 6, 1))))
                : Extent3D { 1, 1, 1 },
              other ? other->GetFormat() : Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA8,
              Image::Type::TEXTURE_TYPE_CUBEMAP,
              other ? other->GetFilterMode() : Image::FilterMode::TEXTURE_FILTER_NEAREST,
              other ? other->GetWrapMode() : Image::WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
              nullptr
          )
    {
        if (other && other->GetImage().HasAssignedImageData()) {
            m_image.EnsureCapacity(m_image.GetByteSize());

            if (other->IsPanorama()) {
                // create a cubemap from a panorama image
                //         [top]
                // [right][back][left][front]
                //       [bottom]

#if 0 // todo

                const auto width_per_face = other->GetImage().GetExtent().width / 4;
                const auto height_per_face = width_per_face;

                const auto face_size = width_per_face * height_per_face * Image::NumComponents(other->GetFormat());

                AssertThrow(face_size * 6 < other->GetImage().GetByteSize());

                // TODO: Project spherical onto sphere

                for (UInt x = 0; x < width_per_face * Image::NumComponents(other->GetFormat(); x++) {
                    for (UInt y = 0; y < height_per_face * Image::NumComponents(other->GetFormat(); y++) {
                        
                    }
                }
                SizeType src_offset = 0,
                    dst_offset = 0;

                // skip firstt in row
                src_offset += face_size;

                // copy top image
                m_image.CopyImageData(other->GetImage().GetBytes() + src_offset, face_size, dst_offset);
                dst_offset += face_size;
                src_offset += face_size;

                // skip next, move to row
                src_offset += face_size;
                src_offset += face_size;

                for (UInt i = 0; i < 4; i++) {
                    // copy 4 faces in row
                    m_image.CopyImageData(other->GetImage().GetBytes() + src_offset, face_size, dst_offset);
                    dst_offset += face_size;
                    src_offset += face_size;
                }

                // skip first in row
                src_offset += face_size;

                m_image.CopyImageData(other->GetImage().GetBytes() + src_offset, face_size, dst_offset);
#endif
            } else {
                m_image.CopyImageData(other->GetImage().GetBytes(), other->GetImage().GetByteSize(), 0);
            }
        }

        other.reset();
    }

    TextureCube(
        std::array<std::unique_ptr<Texture>, 6> &&texture_faces
    ) : Texture(
            texture_faces[0] ? texture_faces[0]->GetExtent() : Extent3D { },
            texture_faces[0] ? texture_faces[0]->GetFormat() : Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA8,
            Image::Type::TEXTURE_TYPE_CUBEMAP,
            texture_faces[0] ? texture_faces[0]->GetFilterMode() : Image::FilterMode::TEXTURE_FILTER_NEAREST,
            texture_faces[0] ?  texture_faces[0]->GetWrapMode() : Image::WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
            nullptr
        )
    {
        m_image.EnsureCapacity(m_image.GetByteSize());

        SizeType offset = 0,
            face_size = 0;

        for (auto &texture : texture_faces) {
            if (texture != nullptr) {
                face_size = texture->GetExtent().Size() * Image::NumComponents(texture->GetFormat());

                m_image.CopyImageData(texture->GetImage().GetBytes(), face_size, offset);
            }

            texture.reset();

            offset += face_size;
        }
    }
};

} // namespace hyperion::v2

#endif