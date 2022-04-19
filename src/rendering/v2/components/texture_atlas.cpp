//
// Created by ethan on 4/5/22.
//

#include "texture_atlas.h"
#include "../engine.h"

namespace hyperion::v2 {

TextureAtlas::TextureAtlas(Extent2D extent,
                           Image::InternalFormat format,
                           Image::FilterMode filter_mode,
                           Image::WrapMode wrap_mode)
    : EngineComponentBase(),
      m_extent(extent),
      m_format(format),
      m_filter_mode(filter_mode),
      m_wrap_mode(wrap_mode)
{
}

TextureAtlas::TextureAtlas(Extent2D extent)
    : TextureAtlas(extent,
                   Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA8,
                   Image::FilterMode::TEXTURE_FILTER_LINEAR,
                   Image::WrapMode::TEXTURE_WRAP_CLAMP_TO_BORDER)
{
}

void TextureAtlas::BlitTexture(Engine *engine, Offset &dst_offset, Ref<Texture> &&src_texture, Offset &src_offset) {
    Image::Rect dst = { (uint32_t)dst_offset.x, (uint32_t)dst_offset.y,
                    (uint32_t)dst_offset.x+(uint32_t)dst_offset.width, (uint32_t)dst_offset.y+(uint32_t)dst_offset.height };
    Image::Rect src = { (uint32_t)src_offset.x, (uint32_t)src_offset.y,
                    (uint32_t)src_offset.x+(uint32_t)src_offset.width, (uint32_t)src_offset.y+(uint32_t)src_offset.height };

    m_texture->BlitTexture(engine, dst, std::move(src_texture), src);
}

void TextureAtlas::Init(Engine *engine) {
    AssertThrow(m_texture == nullptr);

    if (IsInit())
        return;

    EngineComponentBase::Init();

    m_texture = engine->resources.textures.Add(std::make_unique<Texture2D>(
        m_extent,
        m_format,
        m_filter_mode,
        m_wrap_mode,
        nullptr
    ));

    m_texture.Init();
}

TextureAtlas::~TextureAtlas() {
    Teardown();
}

}