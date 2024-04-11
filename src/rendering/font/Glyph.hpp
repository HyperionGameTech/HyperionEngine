#ifndef HYP_FONT_FONTGLYPH_HPP
#define HYP_FONT_FONTGLYPH_HPP

#include <rendering/font/FontEngine.hpp>
#include <rendering/font/FontFace.hpp>

#include <rendering/backend/RendererStructs.hpp>
#include <rendering/Texture.hpp>

namespace hyperion::v2 {

struct GlyphImageData
{
    Extent2D    dimensions;
    ByteBuffer  byte_buffer;

    HYP_API Handle<Texture> CreateTexture() const;
};

class Glyph
{
public:
    struct PackedMetrics
    {
        uint16  width;
        uint16  height;
        int16   bearing_x;
        int16   bearing_y;
        uint32  advance;
    };

    struct Metrics
    {
        PackedMetrics   metrics;
        Vec2i           image_position;

        [[nodiscard]]
        PackedMetrics GetPackedMetrics() const
            { return metrics; }
    };

    HYP_API Glyph(RC<FontFace> face, FontFace::GlyphIndex index, bool render = false);

    Glyph(const Glyph &other)                   = default;
    Glyph &operator=(const Glyph &other)        = default;
    Glyph(Glyph &&other) noexcept               = default;
    Glyph &operator=(Glyph &&other) noexcept    = default;

    ~Glyph()                                    = default;

    [[nodiscard]]
    HYP_FORCE_INLINE
    Metrics GetMetrics() const
        { return m_metrics; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    const GlyphImageData &GetImageData() const
        { return m_glyph_image_data; }

    HYP_API void Render();

    HYP_API Extent2D GetMax();
    HYP_API Extent2D GetMin();

private:
    RC<FontFace>        m_face;

    FontEngine::Glyph   m_glyph;
    GlyphImageData      m_glyph_image_data;
    Metrics             m_metrics { 0 };
};

}; // namespace hyperion::v2

#endif //HYP_FONT_FONTGLYPH_HPP
