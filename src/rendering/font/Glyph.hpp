#ifndef HYP_FONT_FONTGLYPH_HPP
#define HYP_FONT_FONTGLYPH_HPP

#include <rendering/font/FontEngine.hpp>
#include <rendering/font/FontFace.hpp>

#include <rendering/backend/RendererStructs.hpp>
#include <rendering/Texture.hpp>

namespace hyperion::v2 {

struct GlyphImageData
{
    Extent2D   dimensions;
    ByteBuffer byte_buffer;

    Handle<Texture> CreateTexture() const;
};

class Glyph
{
public:

    HYP_PACK_BEGIN
    struct PackedMetrics
    {
        uint16  width;     /* 2 */
        uint16  height;    /* 4 */
        int16   bearing_x;  /* 6 */
        int16   bearing_y;  /* 8 */
        uint32   advance;    /* 12 */
    } HYP_PACK_END;

    struct Metrics
    {
        PackedMetrics   metrics;
        Vec2i           image_position;

        [[nodiscard]]
        PackedMetrics GetPackedMetrics() const
            { return metrics; }
    };

    Glyph(RC<FontFace> face, FontFace::GlyphIndex index, bool render);
    void Render();

    [[nodiscard]] Metrics GetMetrics() const
        { return m_metrics; }

    Extent2D GetMax();
    Extent2D GetMin();

    const GlyphImageData &GetImageData() const
        { return m_glyph_image_data; }

private:
    RC<FontFace>                    m_face;

    FontEngine::Glyph           m_glyph;
    GlyphImageData              m_glyph_image_data;
    Metrics                     m_metrics { 0 };
};

}; // namespace hyperion::v2

#endif //HYP_FONT_FONTGLYPH_HPP
