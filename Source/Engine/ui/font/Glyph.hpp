/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <Core/reflection/Handle.hpp>

#include <Core/utilities/Result.hpp>

#include <Core/memory/UniquePtr.hpp>
#include <Core/memory/RefCountedPtr.hpp>

#include <Core/math/Vector2.hpp>

#include <util/img/Bitmap.hpp>

#include <ui/font/FontEngine.hpp>
#include <ui/font/FontFace.hpp>

#include <rendering/Shared.hpp>

namespace Hyperion {

class Texture;

using GlyphBitmap = Bitmap_RGBA8;

struct GlyphImageData
{
    Vec2i dimensions;
    ByteBuffer byteBuffer;

    HYP_API UniquePtr<GlyphBitmap> CreateBitmap() const;
};

class HYP_API Glyph
{
public:
    struct Metrics
    {
        uint16 width = 0;
        uint16 height = 0;
        int16 bearingX = 0;
        int16 bearingY = 0;
        uint32 advance = 0;

        Vec2i imagePosition;
    };

    Glyph(RC<FontFace> face, FontFace::GlyphIndex index, float scale);

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
        return m_glyphImageData;
    }

    void LoadMetrics();
    TResult<UniquePtr<GlyphBitmap>> Rasterize();

    Vec2i GetMax();
    Vec2i GetMin();

private:
    RC<FontFace> m_face;
    FontFace::GlyphIndex m_index;
    float m_scale;

    GlyphImageData m_glyphImageData;
    Metrics m_metrics {};
};

}; // namespace Hyperion
