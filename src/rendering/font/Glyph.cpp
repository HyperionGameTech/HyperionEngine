/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/font/Glyph.hpp>
#include <rendering/font/FontEngine.hpp>
#include <rendering/font/FontFace.hpp>

#include <rendering/Texture.hpp>

#include <core/logging/Logger.hpp>

#include <util/img/Bitmap.hpp>

#ifdef HYP_FREETYPE

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_BITMAP_H

#endif

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Font);

#pragma region GlyphImageData

Handle<Texture> GlyphImageData::CreateTexture() const
{
    AssertThrow(byte_buffer.Size() == dimensions.x * dimensions.y);

    RC<StreamedTextureData> streamed_texture_data = MakeRefCountedPtr<StreamedTextureData>(TextureData {
        TextureDesc {
            ImageType::TEXTURE_TYPE_2D,
            InternalFormat::R8,
            Vec3u { uint32(dimensions.x), uint32(dimensions.y), 1 }
        },
        byte_buffer
    });

    Handle<Texture> texture = CreateObject<Texture>(std::move(streamed_texture_data));
    InitObject(texture);

    return texture;
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
        HYP_LOG(Font, LogLevel::ERR, "Error setting pixel size for font face!");

        return;
    }

    if (FT_Load_Glyph(m_face->GetFace(), m_index, FT_LOAD_DEFAULT | FT_LOAD_COLOR | FT_PIXEL_MODE_GRAY)) {
        HYP_LOG(Font, LogLevel::ERR, "Error loading glyph from font face!");

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
        HYP_LOG(Font, LogLevel::ERR, "Error setting pixel size for font face!");

        return;
    }

    if (FT_Load_Glyph(m_face->GetFace(), m_index, FT_LOAD_DEFAULT | FT_LOAD_COLOR | FT_LOAD_RENDER | FT_PIXEL_MODE_GRAY)) {
        HYP_LOG(Font, LogLevel::ERR, "Error loading glyph from font face!");

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
    byte_buffer.SetSize(dimensions.x * dimensions.y);

    if (ft_bitmap.buffer != nullptr) {
        // byte_buffer.Write(uint32(MathUtil::Abs(ft_bitmap.pitch)) * ft_bitmap.rows, 0, ft_bitmap.buffer);

        for (uint32 row = 0; row < ft_bitmap.rows; ++row) {
            for (uint32 col = 0; col < ft_bitmap.width; ++col) {
                byte_buffer.Data()[row * dimensions.x + col] = ft_bitmap.buffer[row * ft_bitmap.pitch + col];
            }
        }
    }

    Bitmap<1> bm(dimensions.x, dimensions.y);
    bm.SetPixels(byte_buffer);
    bm.Write(String("glyph_") + String::ToString(m_index) + ".bmp");

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
