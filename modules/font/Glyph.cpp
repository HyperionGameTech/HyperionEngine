//
// Created by emd22 on 18/12/22.
//

#include <iostream>

#include "Glyph.hpp"
#include "FontEngine.hpp"
#include "Face.hpp"

#include <util/img/Bitmap.hpp>
#include <Engine.hpp>
#include <rendering/Texture.hpp>

namespace hyperion::v2
{

#include <ft2build.h>
#include FT_FREETYPE_H

Glyph::Glyph(Handle<Face> face, Face::GlyphIndex index, bool render = false)
    : m_face(face)
{
    if (FT_Load_Glyph(face->GetFace(), index, (render) ? FT_LOAD_RENDER : FT_LOAD_DEFAULT)) {
        DebugLog(LogType::Error, "Error loading glyph from font face!\n");
    }
}

void Glyph::Render()
{
    int error = FT_Render_Glyph(m_face->GetFace()->glyph, FT_RENDER_MODE_NORMAL);

    if (error) {
        DebugLog(LogType::Error, "Error rendering glyph '%s'\n", error, FT_Error_String(error));
    }

    FontEngine::Glyph glyph = m_face->GetFace()->glyph;
    FT_Bitmap *ft_bitmap = &glyph->bitmap;

    PackedMetrics packed_metrics =  {
        .bearing_x = static_cast<Int16>(glyph->metrics.horiBearingX / 64),
        .bearing_y = static_cast<Int16>(glyph->metrics.horiBearingY / 64),
        .width  = static_cast<UInt8>(glyph->metrics.width / 64),
        .height = static_cast<UInt8>(glyph->metrics.height / 64),
        .advance = static_cast<UInt8>(glyph->metrics.horiAdvance / 64),
        ._reserved0 = 0,
    };

    m_metrics = {
        .metrics = packed_metrics,

        .image_position = { 0, 0 }
    };

    m_texture = CreateObject<Texture>(
        Texture2D(
            Extent2D(ft_bitmap->width, ft_bitmap->rows),
            InternalFormat::R8,
            FilterMode::TEXTURE_FILTER_LINEAR,
            WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
            ft_bitmap->buffer
        )
    );

    InitObject(m_texture);
}

Extent2D Glyph::GetMax()
{
    auto &box = m_face->GetFace()->glyph->metrics;
    return { (UInt32)box.width/64, (UInt32)box.height/64 };
}

Extent2D Glyph::GetMin()
{
    auto &box = m_face->GetFace()->bbox;
    return { (UInt32)box.xMin, (UInt32)box.yMin };
}

const Handle<Texture> &Glyph::GetTexture()
{
    return m_texture;
}


} // namespace::v2
