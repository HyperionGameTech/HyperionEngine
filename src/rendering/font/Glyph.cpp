/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/font/Glyph.hpp>
#include <rendering/font/FontEngine.hpp>
#include <rendering/font/FontFace.hpp>

#include <rendering/RenderTexture.hpp>

#include <core/logging/Logger.hpp>

#ifdef HYP_FREETYPE

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_BITMAP_H

#endif

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Font);

static constexpr InternalFormat g_glyph_texture_format = InternalFormat::RGBA8;

#pragma region GlyphImageData

Handle<Texture> GlyphImageData::CreateTexture() const
{
    RC<StreamedTextureData> streamed_texture_data = MakeRefCountedPtr<StreamedTextureData>(TextureData {
        TextureDesc {
            ImageType::TEXTURE_TYPE_2D,
            g_glyph_texture_format,
            Vec3u { uint32(dimensions.x), uint32(dimensions.y), 1 }
        },
        byte_buffer
    });

    return CreateObject<Texture>(std::move(streamed_texture_data));
}

#pragma endregion GlyphImageData

#pragma region Glyph

Glyph::Glyph(RC<FontFace> face, FontFace::GlyphIndex index, float scale)
    : m_face(std::move(face)),
      m_index(index),
      m_scale(scale)
{
}

void Glyph::LoadMetrics()
{
    AssertThrow(m_face != nullptr);

#ifdef HYP_FREETYPE
    if (FT_Set_Pixel_Sizes(m_face->GetFace(), 0, MathUtil::Floor<float, uint32>(64.0f * m_scale))) {
        HYP_LOG(Font, Error, "Error setting pixel size for font face!");

        return;
    }

    if (FT_Load_Glyph(m_face->GetFace(), m_index, FT_LOAD_DEFAULT | FT_LOAD_COLOR | FT_PIXEL_MODE_GRAY)) {
        HYP_LOG(Font, Error, "Error loading glyph from font face!");

        return;
    }
#endif
}

void Glyph::Render()
{
    AssertThrow(m_face != nullptr);

    FontEngine::Glyph glyph { };
    PackedMetrics packed_metrics { };

#ifdef HYP_FREETYPE
    if (FT_Set_Pixel_Sizes(m_face->GetFace(), 0, MathUtil::Floor<float, uint32>(64.0f * m_scale))) {
        HYP_LOG(Font, Error, "Error setting pixel size for font face!");

        return;
    }

    if (FT_Load_Glyph(m_face->GetFace(), m_index, FT_LOAD_DEFAULT | FT_LOAD_COLOR | FT_LOAD_RENDER | FT_PIXEL_MODE_GRAY)) {
        HYP_LOG(Font, Error, "Error loading glyph from font face!");

        return;
    }

    glyph = m_face->GetFace()->glyph;
    FT_Bitmap &ft_bitmap = glyph->bitmap;

    AssertThrow(glyph->format == FT_GLYPH_FORMAT_BITMAP);

    packed_metrics = {
        .width      = uint16(glyph->bitmap.width),
        .height     = uint16(glyph->bitmap.rows),
        .bearing_x  = int16(glyph->bitmap_left),
        .bearing_y  = int16(glyph->bitmap_top),
        .advance    = uint32(glyph->advance.x)
    };
#endif

    m_metrics = {
        .metrics        = packed_metrics,
        .image_position = { 0, 0 }
    };

#ifdef HYP_FREETYPE
    const Vec2i dimensions = GetMax();

    ByteBuffer byte_buffer;
    byte_buffer.SetSize(dimensions.Volume() * renderer::NumComponents(g_glyph_texture_format) * renderer::NumBytes(g_glyph_texture_format));
    
    if (ft_bitmap.buffer != nullptr) {
        for (uint32 row = 0; row < ft_bitmap.rows; ++row) {
            for (uint32 col = 0; col < ft_bitmap.width; ++col) {
                ubyte *dst_offset = byte_buffer.Data() + (row * dimensions.x + col) * renderer::NumComponents(g_glyph_texture_format) * renderer::NumBytes(g_glyph_texture_format);

                // Copy the grayscale value into the RGBA channels
                for (uint32 component_index = 0; component_index < renderer::NumComponents(g_glyph_texture_format); component_index++) {
                    for (uint32 byte_index = 0; byte_index < renderer::NumBytes(g_glyph_texture_format); byte_index++) {
                        dst_offset[component_index * renderer::NumBytes(g_glyph_texture_format) + byte_index] = ft_bitmap.buffer[row * ft_bitmap.pitch + col];
                    }
                }
                // byte_buffer.Data()[row * dimensions.x + col] = ft_bitmap.buffer[row * ft_bitmap.pitch + col];
            }
        }
    }

    m_glyph_image_data = GlyphImageData {
        dimensions,
        std::move(byte_buffer)
    };
#endif
}

Vec2i Glyph::GetMax()
{
    AssertThrow(m_face != nullptr);

#ifdef HYP_FREETYPE
    return MathUtil::Max({ MathUtil::Abs(m_face->GetFace()->glyph->bitmap.pitch), int(m_face->GetFace()->glyph->bitmap.rows) }, Vec2i::One());
#else
    return { 0, 0 };
#endif
}

Vec2i Glyph::GetMin()
{
    AssertThrow(m_face != nullptr);

#ifdef HYP_FREETYPE
    auto &box = m_face->GetFace()->bbox;
    return { int(box.xMin), int(box.yMin) };
#else
    return { 0, 0 };
#endif
}

#pragma endregion Glyph

} // namespace
