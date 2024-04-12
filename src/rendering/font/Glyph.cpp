#include <rendering/font/Glyph.hpp>
#include <rendering/font/FontEngine.hpp>
#include <rendering/font/FontFace.hpp>

#include <rendering/Texture.hpp>

#include <util/img/Bitmap.hpp>

#include <Engine.hpp>

#ifdef HYP_FREETYPE

#include <ft2build.h>
#include FT_FREETYPE_H

#endif

namespace hyperion::v2 {

// GlyphImageData

Handle<Texture> GlyphImageData::CreateTexture() const
{
    AssertThrow(byte_buffer.Size() == dimensions.Size());

    UniquePtr<StreamedData> streamed_texture_data(new MemoryStreamedData(byte_buffer));

    Handle<Texture> texture = CreateObject<Texture>(Texture2D(
        dimensions,
        InternalFormat::R8,
        FilterMode::TEXTURE_FILTER_NEAREST,
        WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
        std::move(streamed_texture_data)
    ));

    InitObject(texture);

    return texture;
}

// Glyph

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
        DebugLog(LogType::Error, "Error setting pixel size for font face!\n");

        return;
    }

    if (FT_Load_Glyph(m_face->GetFace(), m_index, FT_LOAD_DEFAULT | FT_LOAD_COLOR)) {
        DebugLog(LogType::Error, "Error loading glyph from font face!\n");

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
    // int error = FT_Render_Glyph(m_face->GetFace()->glyph, FT_RENDER_MODE_NORMAL);

    // if (error) {
    //     DebugLog(LogType::Error, "Error rendering glyph '%s': %u\n", FT_Error_String(error), error);
    // }

    if (FT_Set_Pixel_Sizes(m_face->GetFace(), 0, MathUtil::Floor<float, uint32>(64.0f * m_scale))) {
        DebugLog(LogType::Error, "Error setting pixel size for font face!\n");

        return;
    }

    if (FT_Load_Glyph(m_face->GetFace(), m_index, FT_LOAD_DEFAULT | FT_LOAD_COLOR | FT_LOAD_RENDER)) {
        DebugLog(LogType::Error, "Error loading glyph from font face!\n");

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
    Extent2D dimensions = GetMax();
    dimensions.width = MathUtil::Max(dimensions.width, 1u);
    dimensions.height = MathUtil::Max(dimensions.height, 1u);

    ByteBuffer byte_buffer;
    byte_buffer.SetSize(dimensions.Size());

    if (ft_bitmap.buffer != nullptr) {
        byte_buffer.Write(ft_bitmap.width * ft_bitmap.rows, 0, ft_bitmap.buffer);
    }

    m_glyph_image_data = GlyphImageData {
        dimensions,
        std::move(byte_buffer)
    };
#endif
}

Extent2D Glyph::GetMax()
{
    AssertThrow(m_face != nullptr);

#ifdef HYP_FREETYPE

    return { m_face->GetFace()->glyph->bitmap.width, m_face->GetFace()->glyph->bitmap.rows };
    // auto &box = m_face->GetFace()->glyph->metrics;
    // return { (uint32)box.width/64, (uint32)box.height/64 };
#else
    return { 0, 0 };
#endif
}

Extent2D Glyph::GetMin()
{
    AssertThrow(m_face != nullptr);

#ifdef HYP_FREETYPE
    auto &box = m_face->GetFace()->bbox;
    return { (uint32)box.xMin, (uint32)box.yMin };
#else
    return { 0, 0 };
#endif
}

} // namespace::v2
