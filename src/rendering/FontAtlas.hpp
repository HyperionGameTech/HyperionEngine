//
// Created by emd22 on 24/12/22.
//

#ifndef HYPERION_FONTATLAS_HPP
#define HYPERION_FONTATLAS_HPP

#include <font/Face.hpp>
#include <font/Glyph.hpp>

#include <core/lib/ByteBuffer.hpp>

#include <rendering/Texture.hpp>
#include <rendering/Framebuffer.hpp>

#include <Engine.hpp>

namespace hyperion::v2 {


class FontAtlas {
public:
    static constexpr UInt image_columns = 15;
    static constexpr UInt image_rows = 8;

    FontAtlas(font::Face face)
        : m_face(face)
    {
        m_cell_dimensions = FindMaxDimensions(face);

        m_atlas_dimensions = { m_cell_dimensions.x * image_columns, m_cell_dimensions.y * image_rows };
        m_atlas = CreateObject<Texture>(
            Texture2D(
                m_atlas_dimensions,
                InternalFormat::R8,
                FilterMode::TEXTURE_FILTER_LINEAR,
                WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
                nullptr
            )
        );
        InitObject(m_atlas);
    }

    void Render()
    {
        font::Glyph glyph(m_face, m_face.GetGlyphIndex(L'A'), true);
        glyph.Render();

        DebugLog(LogType::Warn, "Rendering character\n");
        RenderCharacter({0, 0}, m_cell_dimensions, glyph);
        DebugLog(LogType::Warn, "Done Rendering character\n");

        /*const UInt glyphs = '-';
        for (UInt i = '!'; i < glyphs; i++) {
            font::Glyph glyph(m_face, m_face.GetGlyphIndex((font::Face::WChar)i), true);
            glyph.Render();

            /*const int image_index = i - 32;
            const Extent2D cell_index = { (image_index % image_columns), (image_index / image_columns) };
            const Extent2D location = m_cell_dimensions * cell_index;

            RenderCharacter(location, m_cell_dimensions, glyph);
        }*/
    }

    void RenderCharacter(Extent2D location, Extent2D dimensions, font::Glyph &glyph);

    Extent2D FindMaxDimensions(font::Face &face)
    {
        Extent2D highest_dimensions = { 0, 0 };

        const UInt glyphs = '-';
        for (UInt i = '!'; i < glyphs; i++) {
            font::Glyph glyph(face, face.GetGlyphIndex((font::Face::WChar)i), false);
            Extent2D size = glyph.GetMax();

            if (size.width > highest_dimensions.width)
                highest_dimensions.width = size.width;
            if (size.height > highest_dimensions.height)
                highest_dimensions.height = size.height;
        }
        return highest_dimensions;
    }


    const Handle<Texture> &GetAtlas() const
    {
        return m_atlas;
    }

    renderer::Result WriteTexture();
private:
    Handle<Texture> m_atlas;
    font::Face m_face;
    Extent2D m_cell_dimensions;
    Extent2D m_atlas_dimensions;
};



} // namespace hyperion::v2


#endif //HYPERION_FONTATLAS_HPP
