/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>
#include <core/object/Handle.hpp>

#include <core/memory/ByteBuffer.hpp>
#include <core/containers/Array.hpp>
#include <core/utilities/Optional.hpp>
#include <core/utilities/Result.hpp>

#include <rendering/font/FontFace.hpp>
#include <rendering/font/Glyph.hpp>

#include <core/json/JSON.hpp>

#include <util/img/Bitmap.hpp>

namespace hyperion {

class Texture;
using FontAtlasBitmap = Bitmap_RGBA8;

struct HYP_API FontAtlasTextureSet
{
    Handle<Texture> mainAtlas;
    FlatMap<uint32, Handle<Texture>> atlases;

    ~FontAtlasTextureSet();

    HYP_FORCE_INLINE const Handle<Texture>& GetMainAtlas() const
    {
        return mainAtlas;
    }

    Handle<Texture> GetAtlasForPixelSize(uint32 pixelSize) const;

    void AddAtlas(uint32 pixelSize, Handle<Texture> texture, bool isMainAtlas = false);
};

class FontAtlas : public EnableRefCountedPtrFromThis<FontAtlas>
{
public:
    static constexpr uint32 s_symbolColumns = 20;
    static constexpr uint32 s_symbolRows = 5;

    using SymbolList = Array<FontFace::WChar>;
    using GlyphMetricsBuffer = Array<Glyph::Metrics>;

    HYP_API static SymbolList GetDefaultSymbolList();

    FontAtlas() = default;

    HYP_API FontAtlas(
        const FontAtlasTextureSet& atlasTextures,
        Vec2i cellDimensions,
        GlyphMetricsBuffer glyphMetrics,
        SymbolList symbolList = GetDefaultSymbolList());

    explicit HYP_API FontAtlas(RC<FontFace> face);

    FontAtlas(const FontAtlas& other) = delete;
    FontAtlas& operator=(const FontAtlas& other) = delete;
    FontAtlas(FontAtlas&& other) noexcept = delete;
    FontAtlas& operator=(FontAtlas&& other) noexcept = delete;
    ~FontAtlas();

    HYP_API Result RenderAtlasTextures();

    HYP_FORCE_INLINE const GlyphMetricsBuffer& GetGlyphMetrics() const
    {
        return m_glyphMetrics;
    }

    HYP_FORCE_INLINE const FontAtlasTextureSet& GetAtlasTextures() const
    {
        return m_atlasTextures;
    }

    HYP_FORCE_INLINE const Vec2i& GetCellDimensions() const
    {
        return m_cellDimensions;
    }

    HYP_FORCE_INLINE const SymbolList& GetSymbolList() const
    {
        return m_symbolList;
    }

    HYP_API Optional<const Glyph::Metrics&> GetGlyphMetrics(FontFace::WChar symbol) const;

    HYP_API json::JSONValue GenerateMetadataJSON(const String& outputDirectory) const;

private:
    Vec2i FindMaxDimensions(const RC<FontFace>& face) const;

    RC<FontFace> m_face;

    FontAtlasTextureSet m_atlasTextures;
    Vec2i m_cellDimensions;
    GlyphMetricsBuffer m_glyphMetrics;
    SymbolList m_symbolList;
};

} // namespace hyperion
