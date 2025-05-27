/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYP_FONT_FONTGLYPH_HPP
#define HYP_FONT_FONTGLYPH_HPP

#include <core/Handle.hpp>

#include <rendering/font/FontEngine.hpp>
#include <rendering/font/FontFace.hpp>

#include <rendering/backend/RendererStructs.hpp>

namespace hyperion {

class Texture;

struct GlyphImageData
{
    Vec2i dimensions;
    ByteBuffer byte_buffer;

    HYP_API Handle<Texture> CreateTexture() const;
};

class Glyph
{
public:
    struct PackedMetrics
    {
        uint16 width;
        uint16 height;
        int16 bearing_x;
        int16 bearing_y;
        uint32 advance;
    };

    struct Metrics
    {
        PackedMetrics metrics;
        Vec2i image_position;

        HYP_FORCE_INLINE PackedMetrics GetPackedMetrics() const
        {
            return metrics;
        }
    };

    HYP_API Glyph(RC<FontFace> face, FontFace::GlyphIndex index, float scale);

    Glyph(const Glyph& other) = default;
    Glyph& operator=(const Glyph& other) = default;
    Glyph(Glyph&& other) noexcept = default;
    Glyph& operator=(Glyph&& other) noexcept = default;

    ~Glyph() = default;

    HYP_FORCE_INLINE const Metrics& GetMetrics() const
    {
        return m_metrics;
    }

    HYP_FORCE_INLINE const GlyphImageData& GetImageData() const
    {
        return m_glyph_image_data;
    }

    HYP_API void LoadMetrics();
    HYP_API void Render();

    HYP_API Vec2i GetMax();
    HYP_API Vec2i GetMin();

private:
    RC<FontFace> m_face;
    FontFace::GlyphIndex m_index;
    float m_scale;

    FontEngine::Glyph m_glyph;
    GlyphImageData m_glyph_image_data;
    Metrics m_metrics { 0 };
};

}; // namespace hyperion

#endif // HYP_FONT_FONTGLYPH_HPP
