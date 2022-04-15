//
// Created by ethan on 4/5/22.
//

#include "texture_atlas.h"

namespace hyperion::v2 {

TextureAtlas::TextureAtlas(uint32_t width, uint32_t height,
                           Image::InternalFormat format,
                           Image::FilterMode filter_mode,
                           Image::WrapMode wrap_mode)
    : m_width(width), m_height(height)
{
    m_texture = std::make_unique<Texture2D>(width, height, format, filter_mode, wrap_mode, nullptr);
}

TextureAtlas::TextureAtlas(uint32_t width, uint32_t height)
    : TextureAtlas(width, height,
                   Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA8,
                   Image::FilterMode::TEXTURE_FILTER_LINEAR,
                   Image::WrapMode::TEXTURE_WRAP_CLAMP_TO_BORDER)
{
}

void TextureAtlas::BlitTexture(Engine *engine, Offset &dst_offset, Texture2D *src_texture, Offset &src_offset) {
    Vector4 dst = { (float)dst_offset.x, (float)dst_offset.y,
                    (float)dst_offset.x+(float)dst_offset.width, (float)dst_offset.y+(float)dst_offset.height };
    Vector4 src = { (float)src_offset.x, (float)src_offset.y,
                    (float)src_offset.x+(float)src_offset.width, (float)src_offset.y+(float)src_offset.height };

    m_texture->BlitTexture(engine, dst, src_texture, src);
}

void TextureAtlas::Create(Engine *engine) {
    AssertThrow(m_texture == nullptr);

    m_texture = engine->resources.textures.Add(std::make_unique<Texture2D>(
        m_extent,
        m_format,
        m_filter_mode,
        m_wrap_mode,
        nullptr
    ));

    m_texture.Init(engine);
}





}