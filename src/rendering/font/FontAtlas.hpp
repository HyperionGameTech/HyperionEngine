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

    static constexpr uint data_lines_offset = 2;

    using SymbolList = Array<FontFace::WChar>;
    using GlyphMetricsBuffer = Array<Glyph::Metrics>;

    FontAtlas() = default;

    FontAtlas(Handle<Texture> atlas, Extent2D cell_dimensions, GlyphMetricsBuffer glyph_metrics)
        : m_atlas(std::move(atlas)),
          m_cell_dimensions(cell_dimensions),
          m_glyph_metrics(std::move(glyph_metrics))
    {
    }

    FontAtlas(RC<FontFace> face);

    SymbolList GetDefaultSymbolList() const;

    void Render(Optional<SymbolList> symbol_list = { });

    Extent2D FindMaxDimensions(const RC<FontFace> &face, SymbolList symbol_list = { }) const;

    [[nodiscard]] GlyphMetricsBuffer GetGlyphMetrics() const
        { return m_glyph_metrics; }

    [[nodiscard]] const Handle<Texture> &GetAtlas() const
        { return m_atlas; }

    [[nodiscard]] Extent2D GetDimensions() const
        { return m_atlas.IsValid() ? Extent2D(m_atlas->GetExtent()) : Extent2D { 0, 0 }; }

    [[nodiscard]] Extent2D GetCellDimensions() const
        { return m_cell_dimensions; }

    void WriteToBuffer(ByteBuffer &buffer) const;

private:
    void RenderCharacter(Vec2i location, Extent2D dimensions, Glyph &glyph) const;

    RC<FontFace>            m_face;

    Handle<Texture>     m_atlas;
    Extent2D            m_cell_dimensions;
    GlyphMetricsBuffer  m_glyph_metrics;
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

    void WriteGlyphMetrics(ByteBuffer &pixels, FontAtlas &atlas)
    {
        const Extent2D cell_dimensions = atlas.GetCellDimensions();
        const FontAtlas::GlyphMetricsBuffer metrics = atlas.GetGlyphMetrics();

        ubyte *dest_buffer = m_bytes.GetInternalArray().Data();

        for (auto &metric : metrics) {
            SizeType buffer_offset = metric.image_position.y * atlas.GetDimensions().width + metric.image_position.x;

            int metrics_to_write = int(sizeof(metric.metrics));
            const uint cell_width = atlas.GetCellDimensions().width;

            DebugLog(LogType::RenDebug, "metrics to write: %d\n", metrics_to_write);

            // Check to make sure that we are not going to overwrite glyph data!
            if (metrics_to_write > cell_width * FontAtlas::data_lines_offset) {
                DebugLog(LogType::Warn, "Font glyph metrics data is larger than allocated data lines in image! Skipping writing metrics...\n");
                return;
            }

            // For line wrap, the simplest method is to just loop over until we run out of data.
            // Thus, if we later expand our glyph metadata struct, we can just keep wrapping over
            // to the next available line.
            do {
                auto packed_metrics = metric.GetPackedMetrics();

                Memory::MemCpy(dest_buffer + buffer_offset, &packed_metrics, (metrics_to_write > cell_width) ? cell_width : metrics_to_write);

                buffer_offset += atlas.GetDimensions().height;
                metrics_to_write -= int(cell_width);
            } while (metrics_to_write > 0);

            //DebugLog(LogType::RenDebug, "Font char metrics: (w:%d,h:%d), (%d,%d), %d\n", metric.width, metric.height, metric.bearing_x, metric.bearing_y, metric.advance);
        }
    }

    FontAtlas   m_atlas;

    Extent2D    m_dimensions;
    ByteBuffer  m_bytes;
};


} // namespace hyperion::v2


#endif //HYPERION_FONTATLAS_HPP
