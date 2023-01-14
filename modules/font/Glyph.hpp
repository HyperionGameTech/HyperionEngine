//
// Created by emd22 on 18/12/22.
//

#ifndef HYP_FONT_FONTGLYPH_HPP
#define HYP_FONT_FONTGLYPH_HPP

#include "FontEngine.hpp"
#include "Face.hpp"

#include <rendering/backend/RendererStructs.hpp>
#include <rendering/Texture.hpp>

namespace hyperion::v2::font
{

class Glyph
{
public:
    Glyph(Face &face, Face::GlyphIndex index, bool render);
    void Render();
    Extent2D GetMax();
    Extent2D GetMin();
    const Handle<Texture> &GetTexture();

private:
    Face m_face;

    FontEngine::Glyph m_glyph;
    Handle<Texture> m_texture;
};

}; // namespace hyperion::v2::font

#endif //HYP_FONT_FONTGLYPH_HPP
