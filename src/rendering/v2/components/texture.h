#ifndef HYPERION_V2_TEXTURE_H
#define HYPERION_V2_TEXTURE_H

#include "base.h"

#include <rendering/backend/renderer_image.h>
#include <rendering/backend/renderer_image_view.h>
#include <rendering/backend/renderer_sampler.h>

#include <memory>
#include <map>

namespace hyperion::v2 {

using renderer::Image;
using renderer::TextureImage;
using renderer::ImageView;
using renderer::Sampler;
using renderer::Extent2D;
using renderer::Extent3D;

class Texture : public EngineComponent<TextureImage> {
public:
    Texture(
        Extent3D extent,
        Image::InternalFormat format,
        Image::Type type,
        Image::FilterMode filter_mode,
        Image::WrapMode wrap_mode,
        const unsigned char *bytes);

    Texture(const Texture &other) = delete;
    Texture &operator=(const Texture &other) = delete;
    ~Texture();

    inline ImageView *GetImageView() const { return m_image_view.get(); }
    inline Sampler *GetSampler() const { return m_sampler.get(); }

    void BlitTexture(Engine *engine, Vector4 dst_rect, Texture *src, Vector4 src_rect);
    void Init(Engine *engine);

private:
    std::unique_ptr<ImageView> m_image_view;
    std::unique_ptr<Sampler> m_sampler;
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
    ) {}
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
    ) {}
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
    ) {}
};

} // namespace hyperion::v2

#endif