//
// Created by emd22 on 18/12/22.
//

#ifndef HYP_FONT_FONTGLYPH_HPP
#define HYP_FONT_FONTGLYPH_HPP

#include "FontEngine.hpp"
#include "Face.hpp"

#include <rendering/backend/RendererStructs.hpp>
#include <rendering/Texture.hpp>

namespace hyperion::v2
{

class Glyph
{
public:

    HYP_PACK_BEGIN
    struct PackedMetrics
    {
        UInt16 width;     /* 2 */
        UInt16 height;    /* 4 */
        Int16 bearing_x;  /* 6 */
        Int16 bearing_y;  /* 8 */
        UInt8 advance;    /* 9 */

        UInt8 _reserved0;  /* 10 */
    } HYP_PACK_END;


    struct Metrics
    {
        PackedMetrics metrics;

        Extent2D image_position;

        [[nodiscard]] PackedMetrics GetPackedMetrics() const { return metrics; }
    };

    Glyph(Handle<hyperion::v2::Face> face, hyperion::v2::Face::GlyphIndex index, bool render);
    void Render();

    [[nodiscard]] Metrics GetMetrics() const { return m_metrics; }

    Extent2D GetMax();
    Extent2D GetMin();
    const Handle<Texture> &GetTexture();

private:
    Handle<hyperion::v2::Face> m_face;

    FontEngine::Glyph m_glyph;
    Handle<Texture> m_texture;
    Metrics m_metrics { 0 };
};

}; // namespace hyperion::v2

#endif //HYP_FONT_FONTGLYPH_HPP
