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
      public RenderObject
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
    
    Image &GetImage()                          { return m_image; }
    const Image &GetImage() const              { return m_image; }
    ImageView &GetImageView()                  { return m_image_view; }
    const ImageView &GetImageView() const      { return m_image_view; }
    Sampler &GetSampler()                      { return m_sampler; }
    const Sampler &GetSampler() const          { return m_sampler; }
    Image::Type GetType() const                { return m_image.GetType(); }
    uint32_t NumFaces() const                  { return m_image.NumFaces(); }
    bool IsTextureCube() const                 { return m_image.IsTextureCube(); }
    const Extent3D &GetExtent() const          { return m_image.GetExtent(); }
    Image::InternalFormat GetFormat() const    { return m_image.GetTextureFormat(); }
    Image::FilterMode GetFilterMode() const    { return m_sampler.GetFilterMode(); }
    Image::WrapMode GetWrapMode() const        { return m_sampler.GetWrapMode(); }
    
    void Init(Engine *engine);

protected:
    Image        m_image;
    ImageView    m_image_view;
    Sampler      m_sampler;
};

class Texture2D : public Texture {
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

class Texture3D : public Texture {
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

class TextureCube : public Texture {
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

    TextureCube(
        std::array<std::unique_ptr<Texture>, 6> &&texture_faces
    ) : Texture(
        texture_faces[0] ? texture_faces[0]->GetExtent() : Extent3D{},
        texture_faces[0] ? texture_faces[0]->GetFormat() : Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA8,
        Image::Type::TEXTURE_TYPE_CUBEMAP,
        texture_faces[0] ? texture_faces[0]->GetFilterMode() : Image::FilterMode::TEXTURE_FILTER_NEAREST,
        texture_faces[0] ?  texture_faces[0]->GetWrapMode()  : Image::WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
        nullptr
    )
    {
        if (m_image.GetBytes() != nullptr) {
            size_t offset = 0,
                   face_size = 0;

            for (auto &texture : texture_faces) {
                if (texture != nullptr) {
                    face_size = texture->GetExtent().Size() * Image::NumComponents(texture->GetFormat());

                    m_image.CopyImageData(texture->GetImage().GetBytes(), face_size, offset);
                }

                offset += face_size;
            }
        }
    }
};

} // namespace hyperion::v2

#endif