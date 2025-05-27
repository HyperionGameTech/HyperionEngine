/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_FONTATLAS_HPP
#define HYPERION_FONTATLAS_HPP

#include <core/Defines.hpp>
#include <core/Handle.hpp>

#include <core/memory/ByteBuffer.hpp>
#include <core/containers/Array.hpp>
#include <core/utilities/Optional.hpp>

#include <rendering/font/FontFace.hpp>
#include <rendering/font/Glyph.hpp>

#include <core/json/JSON.hpp>

#include <util/img/Bitmap.hpp>

namespace hyperion {

class Texture;

struct HYP_API FontAtlasTextureSet
{
    Handle<Texture> main_atlas;
    FlatMap<uint32, Handle<Texture>> atlases;

    ~FontAtlasTextureSet();

    HYP_FORCE_INLINE const Handle<Texture>& GetMainAtlas() const
    {
        return main_atlas;
    }

    Handle<Texture> GetAtlasForPixelSize(uint32 pixel_size) const;

    void AddAtlas(uint32 pixel_size, Handle<Texture> texture, bool is_main_atlas = false);
};

class FontAtlas
{
public:
    static constexpr uint32 s_symbol_columns = 20;
    static constexpr uint32 s_symbol_rows = 5;

    using SymbolList = Array<FontFace::WChar>;
    using GlyphMetricsBuffer = Array<Glyph::Metrics>;

    HYP_API static SymbolList GetDefaultSymbolList();

    FontAtlas() = default;

    HYP_API FontAtlas(const FontAtlasTextureSet& atlas_textures, Vec2i cell_dimensions, GlyphMetricsBuffer glyph_metrics, SymbolList symbol_list = GetDefaultSymbolList());
    HYP_API FontAtlas(RC<FontFace> face);

    FontAtlas(const FontAtlas& other) = delete;
    FontAtlas& operator=(const FontAtlas& other) = delete;
    FontAtlas(FontAtlas&& other) noexcept = default;
    FontAtlas& operator=(FontAtlas&& other) noexcept = default;
    ~FontAtlas() = default;

    HYP_API void Render();

    HYP_FORCE_INLINE const GlyphMetricsBuffer& GetGlyphMetrics() const
    {
        return m_glyph_metrics;
    }

    HYP_FORCE_INLINE const FontAtlasTextureSet& GetAtlasTextures() const
    {
        return m_atlas_textures;
    }

    HYP_FORCE_INLINE const Vec2i& GetCellDimensions() const
    {
        return m_cell_dimensions;
    }

    HYP_FORCE_INLINE const SymbolList& GetSymbolList() const
    {
        return m_symbol_list;
    }

    HYP_API Optional<Glyph::Metrics> GetGlyphMetrics(FontFace::WChar symbol) const;

    HYP_API void WriteToBuffer(uint32 pixel_size, ByteBuffer& buffer) const;
    HYP_API Bitmap<1> GenerateBitmap(uint32 pixel_size) const;
    HYP_API json::JSONValue GenerateMetadataJSON(const String& output_directory) const;

private:
    Vec2i FindMaxDimensions(const RC<FontFace>& face) const;
    void RenderCharacter(const Handle<Texture>& atlas, const Handle<Texture>& glyph_texture, Vec2i location, Vec2i dimensions) const;

    RC<FontFace> m_face;

    FontAtlasTextureSet m_atlas_textures;
    Vec2i m_cell_dimensions;
    GlyphMetricsBuffer m_glyph_metrics;
    SymbolList m_symbol_list;
};

} // namespace hyperion

#endif // HYPERION_FONTATLAS_HPP
