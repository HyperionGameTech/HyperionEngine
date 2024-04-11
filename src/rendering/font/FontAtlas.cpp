#include <rendering/font/FontAtlas.hpp>
#include <rendering/backend/RenderCommand.hpp>

#include <Engine.hpp>

namespace hyperion::v2 {

using renderer::Rect;
using renderer::StagingBuffer;

#pragma region Render commands

struct RENDER_COMMAND(RenderFontAtlas) : renderer::RenderCommand
{
    RENDER_COMMAND(RenderFontAtlas)(const Handle<Texture> &atlas, const Handle<Texture> &glyph, const Extent2D location)
        : atlas(atlas),
          glyph(glyph),
          location(location)
    {
    }

    virtual ~RENDER_COMMAND(RenderFontAtlas)() override = default;

    Handle<Texture> atlas;
    Handle<Texture> glyph;

    Extent2D location;
};

#pragma endregion

FontAtlas::FontAtlas(Handle<Texture> atlas, Extent2D cell_dimensions, GlyphMetricsBuffer glyph_metrics, SymbolList symbol_list)
    : m_atlas(std::move(atlas)),
      m_cell_dimensions(cell_dimensions),
      m_glyph_metrics(std::move(glyph_metrics)),
      m_symbol_list(std::move(symbol_list))
{
    AssertThrow(m_symbol_list.Size() != 0);
}

FontAtlas::FontAtlas(RC<FontFace> face)
    : m_face(std::move(face)),
      m_symbol_list(GetDefaultSymbolList())
{
    AssertThrow(m_symbol_list.Size() != 0);

    // Each cell will be the same size at the largest symbol
    m_cell_dimensions = FindMaxDimensions(m_face);
    
    m_atlas = CreateObject<Texture>(
        Texture2D(
            Extent2D { m_cell_dimensions.width * symbol_columns, m_cell_dimensions.height * symbol_rows },
            /* Grayscale 8-bit texture */
            InternalFormat::R8,
            FilterMode::TEXTURE_FILTER_NEAREST,
            WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
            nullptr
        )
    );

    InitObject(m_atlas);
}

FontAtlas::SymbolList FontAtlas::GetDefaultSymbolList()
{
    // first renderable symbol
    static constexpr uint char_range_start = 33; // !

    // highest symbol in the ascii table
    static constexpr uint char_range_end = 126; // ~ + 1

    SymbolList symbol_list;
    symbol_list.Reserve(char_range_end - char_range_start + 1);

    for (uint ch = char_range_start; ch <= char_range_end; ch++) {
        symbol_list.PushBack(ch);
    }

    return symbol_list;
}

void FontAtlas::Render()
{
    AssertThrow(m_face != nullptr);

    if ((m_symbol_list.Size() / symbol_columns) > symbol_rows) {
        DebugLog(LogType::Warn, "Symbol list size is greater than the allocated font atlas!\n");
    }

    m_glyph_metrics.Clear();
    m_glyph_metrics.Resize(m_symbol_list.Size());

    for (uint i = 0; i < m_symbol_list.Size(); i++) {
        const auto &symbol = m_symbol_list[i];

        const uint x_index = i % symbol_columns;
        const uint y_index = i / symbol_columns;

        const Vec2i position {
            int(x_index * m_cell_dimensions.width),
            int(y_index * m_cell_dimensions.height)
        };

        Glyph glyph(m_face, m_face->GetGlyphIndex(symbol), true);
        // Render the glyph into a temporary texture
        glyph.Render();

        m_glyph_metrics[i] = glyph.GetMetrics();
        m_glyph_metrics[i].image_position = position;

        if (y_index > symbol_rows) {
            break;
        }

        // Render our character texture => atlas
        RenderCharacter(position, m_cell_dimensions, glyph);
    }

    if (!Threads::IsOnThread(THREAD_RENDER)) {
        // Sync render commands if not on render thread (render commands are executed inline if on render thread)
        HYP_SYNC_RENDER();
    }
}

void FontAtlas::RenderCharacter(Vec2i location, Extent2D dimensions, Glyph &glyph) const
{
    Handle<Texture> glyph_texture = glyph.GetImageData().CreateTexture();
    InitObject(glyph_texture);

    struct RENDER_COMMAND(FontAtlas_RenderCharacter) : renderer::RenderCommand
    {
        Handle<Texture> atlas;
        Handle<Texture> glyph_texture;
        Vec2i           location;
        Extent2D        cell_dimensions;

        RENDER_COMMAND(FontAtlas_RenderCharacter)(Handle<Texture> atlas, Handle<Texture> glyph_texture, Vec2i location, Extent2D cell_dimensions)
            : atlas(std::move(atlas)),
              glyph_texture(std::move(glyph_texture)),
              location(location),
              cell_dimensions(cell_dimensions)
        {
        }

        virtual ~RENDER_COMMAND(FontAtlas_RenderCharacter)() override
        {
            g_safe_deleter->SafeReleaseHandle(std::move(atlas));
            g_safe_deleter->SafeReleaseHandle(std::move(glyph_texture));
        }

        virtual Result operator()() override
        {
            auto commands = g_engine->GetGPUInstance()->GetSingleTimeCommands();

            const ImageRef &image = glyph_texture->GetImage();

            Rect src_rect {
                0, 0,
                glyph_texture->GetExtent().width,
                glyph_texture->GetExtent().height
            };

            Rect dest_rect {
                uint32(location.x),
                uint32(location.y),
                uint32(location.x + glyph_texture->GetExtent().width),
                uint32(location.y + glyph_texture->GetExtent().height)
            };

            commands.Push([&](CommandBuffer *command_buffer)
            {
                // put src image in state for copying from
                image->GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::COPY_SRC);
                // put dst image in state for copying to
                atlas->GetImage()->GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::COPY_DST);

                //m_buffer->CopyFrom(command_buffer, staging_buffer, sizeof(value));
                atlas->GetImage()->Blit(command_buffer, image, src_rect, dest_rect);

                HYPERION_RETURN_OK;
            });

            return commands.Execute(g_engine->GetGPUInstance()->GetDevice());
        }
    };

    if (Threads::IsOnThread(THREAD_RENDER)) {
        EXEC_RENDER_COMMAND_INLINE(FontAtlas_RenderCharacter, m_atlas, glyph_texture, location, m_cell_dimensions);
    } else {
        PUSH_RENDER_COMMAND(FontAtlas_RenderCharacter, m_atlas, glyph_texture, location, m_cell_dimensions);
    }
}

Extent2D FontAtlas::FindMaxDimensions(const RC<FontFace> &face) const
{
    Extent2D highest_dimensions = { 0, 0 };

    for (const auto &symbol : m_symbol_list) {
        // Create the glyph but only load in the metadata
        Glyph glyph(face, face->GetGlyphIndex(symbol), false);
        // Get the size of each glyph
        Extent2D size = glyph.GetMax();

        if (size.width > highest_dimensions.width) {
            highest_dimensions.width = size.width;
        }

        if (size.height > highest_dimensions.height) {
            highest_dimensions.height = size.height;
        }
    }

    return highest_dimensions;
}

Optional<Glyph::Metrics> FontAtlas::GetGlyphMetrics(FontFace::WChar symbol) const
{
    const auto it = m_symbol_list.Find(symbol);

    if (it == m_symbol_list.End()) {
        return { };
    }

    const auto index = std::distance(m_symbol_list.Begin(), it);
    AssertThrowMsg(index < m_glyph_metrics.Size(), "Index (%u) out of bounds of glyph metrics buffer (%u)", index, m_glyph_metrics.Size());

    return m_glyph_metrics[index];
}

void FontAtlas::WriteToBuffer(ByteBuffer &buffer) const
{
    AssertThrow(m_atlas.IsValid());

    const SizeType buffer_size = m_atlas->GetImage()->GetByteSize();
    buffer.SetSize(buffer_size);

    struct RENDER_COMMAND(FontAtlas_WriteToBuffer) : renderer::RenderCommand
    {
        Handle<Texture> atlas;
        ByteBuffer      &out_buffer;
        SizeType        buffer_size;

        GPUBufferRef    buffer;

        RENDER_COMMAND(FontAtlas_WriteToBuffer)(Handle<Texture> atlas, ByteBuffer &out_buffer, SizeType buffer_size)
            : atlas(std::move(atlas)),
              out_buffer(out_buffer),
              buffer_size(buffer_size)
        {
        }

        virtual ~RENDER_COMMAND(FontAtlas_WriteToBuffer)() override
        {
            SafeRelease(std::move(buffer));
        }

        virtual renderer::Result operator()() override
        {
            renderer::Result result = renderer::Result { };

            buffer = MakeRenderObject<GPUBuffer>(renderer::GPUBufferType::STAGING_BUFFER);
            HYPERION_ASSERT_RESULT(buffer->Create(g_engine->GetGPUDevice(), buffer_size));
            buffer->SetResourceState(renderer::ResourceState::COPY_DST);
            buffer->GetMapping(g_engine->GetGPUDevice());

            if (!result) {
                return result;
            }

            auto commands = g_engine->GetGPUInstance()->GetSingleTimeCommands();

            AssertThrow(atlas);
            AssertThrow(atlas->GetImage());
            AssertThrow(atlas->GetImage()->GetGPUImage());

            commands.Push([&](CommandBuffer *cmd)
            {

                // put src image in state for copying from
                atlas->GetImage()->GetGPUImage()->InsertBarrier(cmd, renderer::ResourceState::COPY_SRC);
                
                // put dst buffer in state for copying to
                buffer->InsertBarrier(cmd, renderer::ResourceState::COPY_DST);

                atlas->GetImage()->CopyToBuffer(cmd, buffer);

                buffer->InsertBarrier(cmd, renderer::ResourceState::COPY_SRC);

                HYPERION_RETURN_OK;
            });

            HYPERION_PASS_ERRORS(commands.Execute(g_engine->GetGPUDevice()), result);
            buffer->Read(g_engine->GetGPUDevice(), buffer_size, out_buffer.Data());

            HYPERION_RETURN_OK;
        }
    };

    if (Threads::IsOnThread(THREAD_RENDER)) {
        EXEC_RENDER_COMMAND_INLINE(FontAtlas_WriteToBuffer, m_atlas, buffer, buffer_size);
    } else {
        PUSH_RENDER_COMMAND(FontAtlas_WriteToBuffer, m_atlas, buffer, buffer_size);
        HYP_SYNC_RENDER();
    }
}

Bitmap<1> FontAtlas::GenerateBitmap() const
{
    ByteBuffer byte_buffer;
    WriteToBuffer(byte_buffer);

    Bitmap<1> bitmap(m_atlas->GetExtent().width, m_atlas->GetExtent().height);
    bitmap.SetPixels(byte_buffer);
    bitmap.FlipVertical();

    return bitmap;
}

json::JSONValue FontAtlas::GenerateMetadataJSON(const String &bitmap_filepath) const
{
    json::JSONObject value;

    value["bitmap_filepath"] = bitmap_filepath;

    value["cell_dimensions"] = json::JSONObject {
        { "width", json::JSONNumber(m_cell_dimensions.width) },
        { "height", json::JSONNumber(m_cell_dimensions.height) }
    };

    json::JSONArray metrics_array;

    for (const Glyph::Metrics &metric : m_glyph_metrics) {
        metrics_array.PushBack(json::JSONObject {
            { "width", json::JSONNumber(metric.metrics.width) },
            { "height", json::JSONNumber(metric.metrics.height) },
            { "bearing_x", json::JSONNumber(metric.metrics.bearing_x) },
            { "bearing_y", json::JSONNumber(metric.metrics.bearing_y) },
            { "advance", json::JSONNumber(metric.metrics.advance) },
            { "image_position", json::JSONObject {
                { "x", json::JSONNumber(metric.image_position.x) },
                { "y", json::JSONNumber(metric.image_position.y) }
            } }
        });
    }
    
    value["metrics"] = std::move(metrics_array);

    json::JSONArray symbol_list_array;

    for (const auto &symbol : m_symbol_list) {
        symbol_list_array.PushBack(json::JSONNumber(symbol));
    }

    value["symbol_list"] = std::move(symbol_list_array);

    return value;
}

}; // namespace hyperion::v2

