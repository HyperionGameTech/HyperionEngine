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
        FilterMode::TEXTURE_FILTER_LINEAR,
        WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
        std::move(streamed_texture_data)
    ));

    InitObject(texture);

    return texture;
}

// Glyph

Glyph::Glyph(RC<FontFace> face, FontFace::GlyphIndex index, bool render = false)
    : m_face(std::move(face))
{
#ifdef HYP_FREETYPE
    if (FT_Load_Glyph(m_face->GetFace(), index, (render) ? FT_LOAD_RENDER : FT_LOAD_DEFAULT)) {
        DebugLog(LogType::Error, "Error loading glyph from font face!\n");
    }
#endif
}

void Glyph::Render()
{
    AssertThrow(m_face != nullptr);

    FontEngine::Glyph glyph { };
    PackedMetrics packed_metrics { };

#ifdef HYP_FREETYPE
    int error = FT_Render_Glyph(m_face->GetFace()->glyph, FT_RENDER_MODE_NORMAL);

    if (error) {
        DebugLog(LogType::Error, "Error rendering glyph '%s': %u\n", FT_Error_String(error), error);
    }

    glyph = m_face->GetFace()->glyph;
    FT_Bitmap *ft_bitmap = &glyph->bitmap;

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
    ByteBuffer byte_buffer(ft_bitmap->width * ft_bitmap->rows, ft_bitmap->buffer);

    m_glyph_image_data = GlyphImageData {
        { ft_bitmap->width, ft_bitmap->rows },
        std::move(byte_buffer)
    };
#endif
}

Extent2D Glyph::GetMax()
{
    AssertThrow(m_face != nullptr);

#ifdef HYP_FREETYPE
    auto &box = m_face->GetFace()->glyph->metrics;
    return { (uint32)box.width/64, (uint32)box.height/64 };
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
