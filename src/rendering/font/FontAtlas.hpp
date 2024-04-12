#ifndef HYPERION_FONTATLAS_HPP
#define HYPERION_FONTATLAS_HPP

#include <rendering/font/FontFace.hpp>
#include <rendering/font/Glyph.hpp>
#include <rendering/Texture.hpp>
#include <rendering/Framebuffer.hpp>

#include <core/lib/ByteBuffer.hpp>
#include <core/lib/DynArray.hpp>
#include <core/lib/Optional.hpp>

#include <util/json/JSON.hpp>

#include <util/img/Bitmap.hpp>
#include <util/fs/FsUtil.hpp>

#include <util/Defines.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

struct HYP_API FontAtlasTextureSet
{
    Handle<Texture>                 main_atlas;
    FlatMap<uint, Handle<Texture>>  atlases;

    ~FontAtlasTextureSet();

    [[nodiscard]]
    HYP_FORCE_INLINE
    const Handle<Texture> &GetMainAtlas() const
        { return main_atlas; }

    void AddAtlas(uint pixel_size, Handle<Texture> texture, bool is_main_atlas = false);
    Handle<Texture> GetAtlasForPixelSize(uint pixel_size) const;
};

class FontAtlas
{
public:
    static constexpr uint symbol_columns = 20;
    static constexpr uint symbol_rows = 5;

    using SymbolList = Array<FontFace::WChar>;
    using GlyphMetricsBuffer = Array<Glyph::Metrics>;

    HYP_API static SymbolList GetDefaultSymbolList();

    FontAtlas() = default;

    HYP_API FontAtlas(RC<FontAtlasTextureSet> atlases, Extent2D cell_dimensions, GlyphMetricsBuffer glyph_metrics, SymbolList symbol_list = GetDefaultSymbolList());
    HYP_API FontAtlas(RC<FontFace> face);

    FontAtlas(const FontAtlas &other)                   = delete;
    FontAtlas &operator=(const FontAtlas &other)        = delete;
    FontAtlas(FontAtlas &&other) noexcept               = default;
    FontAtlas &operator=(FontAtlas &&other) noexcept    = default;
    ~FontAtlas()                                        = default;

    HYP_API void Render();

    [[nodiscard]]
    const GlyphMetricsBuffer &GetGlyphMetrics() const
        { return m_glyph_metrics; }

    [[nodiscard]]
    const RC<FontAtlasTextureSet> &GetAtlases() const
        { return m_atlases; }

    [[nodiscard]]
    Extent2D GetCellDimensions() const
        { return m_cell_dimensions; }

    [[nodiscard]]
    const SymbolList &GetSymbolList() const
        { return m_symbol_list; }

    HYP_API Optional<Glyph::Metrics> GetGlyphMetrics(FontFace::WChar symbol) const;

    HYP_API void WriteToBuffer(uint pixel_size, ByteBuffer &buffer) const;
    HYP_API Bitmap<1> GenerateBitmap(uint pixel_size) const;
    HYP_API json::JSONValue GenerateMetadataJSON(const String &bitmap_filepath) const;

private:
    Extent2D FindMaxDimensions(const RC<FontFace> &face) const;
    void RenderCharacter(Handle<Texture> &atlas, Vec2i location, Extent2D dimensions, Glyph &glyph) const;

    RC<FontFace>            m_face;

    RC<FontAtlasTextureSet> m_atlases;
    Extent2D                m_cell_dimensions;
    GlyphMetricsBuffer      m_glyph_metrics;
    SymbolList              m_symbol_list;
};

} // namespace hyperion::v2


#endif //HYPERION_FONTATLAS_HPP
