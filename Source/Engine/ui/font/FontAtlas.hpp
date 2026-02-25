/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/Defines.hpp>

#include <Core/reflection/ObjectBase.hpp>
#include <Core/reflection/Handle.hpp>

#include <Core/memory/ByteBuffer.hpp>
#include <Core/containers/Array.hpp>

#include <Core/utilities/Optional.hpp>
#include <Core/utilities/Result.hpp>

#include <ui/font/FontFace.hpp>
#include <ui/font/Glyph.hpp>

#include <Core/json/JSON.hpp>

#include <util/img/Bitmap.hpp>

namespace Hyperion {

class Texture;
using FontAtlasBitmap = Bitmap_R8;

HYP_STRUCT()
struct HYP_API FontAtlasTextureSet
{
    HYP_STRUCT_BODY(FontAtlasTextureSet);

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

HYP_CLASS()
class FontAtlas : public ObjectBase
{
    HYP_OBJECT_BODY(FontAtlas);

public:
    static constexpr uint32 SymbolColumns = 20;
    static constexpr uint32 SymbolRows = 5;

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

    HYP_API Result RenderAtlasTextures(float mainAtlasScale, float maxScale, float step = 0.1f);

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

    HYP_API JSON::Value GenerateMetadataJSON(const String& outputDirectory) const;

private:
    Vec2i FindMaxDimensions(const RC<FontFace>& face) const;

    RC<FontFace> m_face;

    FontAtlasTextureSet m_atlasTextures;
    Vec2i m_cellDimensions;
    GlyphMetricsBuffer m_glyphMetrics;
    SymbolList m_symbolList;
};

} // namespace Hyperion
