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

#include <Engine.hpp>

namespace hyperion::v2 {

class FontAtlas
{
public:
    static constexpr uint symbol_columns = 20;
    static constexpr uint symbol_rows = 5;

    using SymbolList = Array<FontFace::WChar>;
    using GlyphMetricsBuffer = Array<Glyph::Metrics>;

    static SymbolList GetDefaultSymbolList();

    FontAtlas() = default;

    FontAtlas(Handle<Texture> atlas, Extent2D cell_dimensions, GlyphMetricsBuffer glyph_metrics, SymbolList symbol_list = GetDefaultSymbolList());

    FontAtlas(RC<FontFace> face);

    void Render();

    [[nodiscard]]
    const GlyphMetricsBuffer &GetGlyphMetrics() const
        { return m_glyph_metrics; }

    [[nodiscard]]
    const Handle<Texture> &GetTexture() const
        { return m_atlas; }

    [[nodiscard]]
    Extent2D GetDimensions() const
        { return m_atlas.IsValid() ? Extent2D(m_atlas->GetExtent()) : Extent2D { 0, 0 }; }

    [[nodiscard]]
    Extent2D GetCellDimensions() const
        { return m_cell_dimensions; }

    [[nodiscard]]
    const SymbolList &GetSymbolList() const
        { return m_symbol_list; }

    Optional<Glyph::Metrics> GetGlyphMetrics(FontFace::WChar symbol) const;

    void WriteToBuffer(ByteBuffer &buffer) const;

private:
    Extent2D FindMaxDimensions(const RC<FontFace> &face) const;
    void RenderCharacter(Vec2i location, Extent2D dimensions, Glyph &glyph) const;

    RC<FontFace>        m_face;

    Handle<Texture>     m_atlas;
    Extent2D            m_cell_dimensions;
    GlyphMetricsBuffer  m_glyph_metrics;
    SymbolList          m_symbol_list;
};

class FontRenderer
{
public:
    FontRenderer(FontAtlas &atlas)
        : m_atlas(atlas)
    {
        m_dimensions = atlas.GetDimensions();
        m_bytes.SetSize(m_dimensions.width * m_dimensions.height);
    }

    void Render()
    {
        m_atlas.Render();
        m_atlas.WriteToBuffer(m_bytes);
    }

    Bitmap<1> GenerateBitmap() const;
    json::JSONValue GenerateMetadataJSON(const String &bitmap_filepath) const;

    // void WriteTexture(const FilePath &path)
    // {
    //     // Create our bitmap
    //     Bitmap<1> bitmap(m_dimensions.width, m_dimensions.height);
    //     // // Grayscale, so we generate a colour table; ramp from 0 to 255
    //     // auto color_table = bitmap.GenerateColorRamp();
    //     // bitmap.SetColorTable(color_table);

    //     // ByteBuffer bytes(m_dimensions.width * m_dimensions.height);
    //     // WriteGlyphMetrics(bytes, atlas);
    //     bitmap.SetPixels(m_dimensions.width, m_bytes.Data(), m_dimensions.width * m_dimensions.height);
    //     bitmap.Write(path);
    // }
private:

    FontAtlas   m_atlas;

    Extent2D    m_dimensions;
    ByteBuffer  m_bytes;
};


} // namespace hyperion::v2


#endif //HYPERION_FONTATLAS_HPP
