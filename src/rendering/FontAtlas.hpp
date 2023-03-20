//
// Created by emd22 on 24/12/22.
//

#ifndef HYPERION_FONTATLAS_HPP
#define HYPERION_FONTATLAS_HPP

#include <font/Face.hpp>
#include <font/Glyph.hpp>

#include <core/lib/ByteBuffer.hpp>
#include <core/lib/DynArray.hpp>

#include <rendering/Texture.hpp>
#include <rendering/Framebuffer.hpp>

#include <util/img/Bitmap.hpp>

#include <Engine.hpp>

namespace hyperion::v2 {

class FontAtlas
{
public:

    static constexpr UInt symbol_columns = 20;
    static constexpr UInt symbol_rows = 5;

    static constexpr UInt data_lines_offset = 2;

    using SymbolList = containers::detail::DynArray<Face::WChar, sizeof(Face::WChar)>;
    using GlyphMetricsBuffer = containers::detail::DynArray<Glyph::Metrics, sizeof(Glyph::Metrics)>;

    explicit FontAtlas(Handle<Face> &face)
        : m_face(face)
    {
        // Each cell will be the same size at the largest symbol
        m_cell_dimensions = FindMaxDimensions(face);
        // Data lines to store information about the symbol (overhang, width, height, etc)
        m_cell_dimensions.y += data_lines_offset;

        m_atlas_dimensions = { m_cell_dimensions.x * symbol_columns, m_cell_dimensions.y * symbol_rows };
        m_atlas = CreateObject<Texture>(
            Texture2D(
                m_atlas_dimensions,
                /* Grayscale 8-bit texture */
                InternalFormat::R8,
                FilterMode::TEXTURE_FILTER_LINEAR,
                WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
                nullptr
            )
        );
        InitObject(m_atlas);
    }

    SymbolList GetDefaultSymbolList() const
    {
        // highest symbol in the ascii table
        const UInt end = '~' + 1;
        // first renderable symbol
        const UInt start = '!';

        SymbolList symbol_list;
        symbol_list.Reserve(end - start);

        for (UInt ch = start; ch < end; ch++) {
            symbol_list.PushBack(ch);
        }

        return symbol_list;
    }


    void Render(SymbolList symbol_list = { })
    {
        Threads::AssertOnThread(THREAD_RENDER);

        if (symbol_list.Empty()) {
            symbol_list = GetDefaultSymbolList();
        }

        if ((symbol_list.Size() / symbol_columns) > symbol_rows) {
            DebugLog(LogType::Warn, "Symbol list size is greater than the allocated font atlas!\n");
        }

        m_glyph_metrics.Reserve(symbol_list.Size());

        UInt image_index = 0;
        for (const auto &symbol : symbol_list) {
            Glyph glyph(m_face, m_face->GetGlyphIndex(symbol), true);
            // Render the glyph into a temporary texture
            glyph.Render();

            const UInt x_index = image_index % symbol_columns;
            const UInt y_index = image_index / symbol_columns;

            if (y_index > symbol_rows)
                break;

            const Extent2D position = {
                x_index * m_cell_dimensions.width,

                /* Our cell is offset by data_lines_offset to allow our extra metadata
                 * to be written to the top of each cell. */
                y_index * m_cell_dimensions.height + data_lines_offset
            };

            Glyph::Metrics metrics = glyph.GetMetrics();

            metrics.image_position = {
                x_index * m_cell_dimensions.width,
                y_index * m_cell_dimensions.height,
            };
            m_glyph_metrics.PushBack(metrics);

            image_index++;

            // Render our character texture => atlas
            RenderCharacter(position, m_cell_dimensions, glyph);
        }

        // This is the final size of the fontmap, resize to fit to reduce unneeded memory.
        m_glyph_metrics.Refit();
    }

    void RenderCharacter(Extent2D location, Extent2D dimensions, Glyph &glyph);

    Extent2D FindMaxDimensions(Handle<Face> &face, SymbolList symbol_list = { }) const
    {
        Extent2D highest_dimensions = { 0, 0 };

        if (symbol_list.Empty()) {
            symbol_list = GetDefaultSymbolList();
        }

        for (const auto &symbol : symbol_list) {
            // Create the glyph but only load in the metadata
            Glyph glyph(face, face->GetGlyphIndex(symbol), false);
            // Get the size of each glyph
            Extent2D size = glyph.GetMax();

            if (size.width > highest_dimensions.width)
                highest_dimensions.width = size.width;
            if (size.height > highest_dimensions.height)
                highest_dimensions.height = size.height;
        }
        return highest_dimensions;
    }

    [[nodiscard]] GlyphMetricsBuffer GetGlyphMetrics() const { return m_glyph_metrics; }
    [[nodiscard]] Handle<Texture> &GetAtlas() { return m_atlas; }
    [[nodiscard]] Extent2D GetDimensions() const { return m_atlas_dimensions; }
    [[nodiscard]] Extent2D GetCellDimensions() const { return m_cell_dimensions; }

    renderer::Result WriteToBuffer(RC<ByteBuffer> &buffer);
private:
    Handle<Texture> m_atlas;
    Handle<Face> m_face;
    Extent2D m_cell_dimensions;
    Extent2D m_atlas_dimensions;
    GlyphMetricsBuffer m_glyph_metrics;
};

class FontRenderer
{
public:
    void RenderAtlas(FontAtlas &atlas)
    {
        m_dimensions = atlas.GetDimensions();
        m_bytes = RC<ByteBuffer>::Construct(m_dimensions.width * m_dimensions.height);
        atlas.Render();
        atlas.WriteToBuffer(m_bytes);
    }

    void WriteTexture(FontAtlas &atlas, const String &filename)
    {
        // Create our bitmap
        Bitmap<1> bitmap(m_dimensions.width, m_dimensions.height);
        // Grayscale, so we generate a colour table; ramp from 0 to 255
        auto color_table = bitmap.GenerateColorRamp();
        bitmap.SetColourTable(color_table);

        ByteBuffer bytes(m_dimensions.width * m_dimensions.height);
        WriteGlyphMetrics(bytes, atlas);
        bitmap.SetPixelsFromMemory(m_dimensions.width, m_bytes->Data(), m_dimensions.width * m_dimensions.height);
        bitmap.Write(filename);
    }
private:

    void WriteGlyphMetrics(ByteBuffer &pixels, FontAtlas &atlas)
    {
        const Extent2D cell_dimensions = atlas.GetCellDimensions();
        const FontAtlas::GlyphMetricsBuffer metrics = atlas.GetGlyphMetrics();
        UInt8 *dest_buffer = m_bytes->GetInternalArray().Data();

        for (auto &metric : metrics) {
            SizeType buffer_offset = metric.image_position.y * atlas.GetDimensions().x + metric.image_position.x;

            Int metrics_to_write = sizeof(metric.metrics);
            const UInt cell_width = atlas.GetCellDimensions().x;

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

                Memory::Copy(dest_buffer + buffer_offset, &packed_metrics, (metrics_to_write > cell_width) ? cell_width : metrics_to_write);

                buffer_offset += atlas.GetDimensions().y;
                metrics_to_write -= static_cast<Int>(cell_width);
            } while (metrics_to_write > 0);

            //DebugLog(LogType::RenDebug, "Font char metrics: (w:%d,h:%d), (%d,%d), %d\n", metric.width, metric.height, metric.bearing_x, metric.bearing_y, metric.advance);
        }

    }

    Extent2D m_dimensions;
    RC<ByteBuffer> m_bytes;
};


} // namespace hyperion::v2


#endif //HYPERION_FONTATLAS_HPP
