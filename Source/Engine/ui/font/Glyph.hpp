/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

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

HYP_STRUCT()
struct GlyphMetrics
{
    HYP_STRUCT_BODY(GlyphMetrics);

    HYP_FIELD()
    uint16 width = 0;
    
    HYP_FIELD()
    uint16 height = 0;
    
    HYP_FIELD()
    int16 bearingX = 0;
    
    HYP_FIELD()
    int16 bearingY = 0;
    
    HYP_FIELD()
    uint32 advance = 0;
    
    HYP_FIELD()
    Vec2i imagePosition;
};

class Glyph
{
public:
    Glyph(RC<FontFace> face, FontFace::GlyphIndex index, float scale);

    Glyph(const Glyph& other) = default;
    Glyph& operator=(const Glyph& other) = default;
    Glyph(Glyph&& other) noexcept = default;
    Glyph& operator=(Glyph&& other) noexcept = default;

    ~Glyph() = default;

    HYP_FORCE_INLINE const GlyphMetrics& GetMetrics() const
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
    GlyphMetrics m_metrics;
};

}; // namespace Hyperion
