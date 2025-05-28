/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/font/FontAtlas.hpp>
#include <rendering/RenderTexture.hpp>
#include <rendering/SafeDeleter.hpp>

#include <rendering/rhi/RHICommandList.hpp>

#include <rendering/backend/RenderCommand.hpp>
#include <rendering/backend/RendererHelpers.hpp>

#include <scene/Texture.hpp>

#include <core/logging/Logger.hpp>

#include <Engine.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Font);

using renderer::GPUBufferType;

#pragma region Render commands

struct RENDER_COMMAND(FontAtlas_RenderCharacter)
    : renderer::RenderCommand
{
    Handle<Texture> atlas_texture;
    Handle<Texture> glyph_texture;
    Vec2i location;
    Vec2i cell_dimensions;

    RENDER_COMMAND(FontAtlas_RenderCharacter)(const Handle<Texture>& atlas_texture, const Handle<Texture>& glyph_texture, Vec2i location, Vec2i cell_dimensions)
        : atlas_texture(atlas_texture),
          glyph_texture(glyph_texture),
          location(location),
          cell_dimensions(cell_dimensions)
    {
        atlas_texture->GetRenderResource().IncRef();
        glyph_texture->GetRenderResource().IncRef();
    }

    virtual ~RENDER_COMMAND(FontAtlas_RenderCharacter)() override
    {
        atlas_texture->GetRenderResource().DecRef();
        glyph_texture->GetRenderResource().DecRef();
    }

    virtual RendererResult operator()() override
    {
        renderer::SingleTimeCommands commands;

        const ImageRef& image = glyph_texture->GetRenderResource().GetImage();
        AssertThrow(image.IsValid());

        const Vec3u& extent = image->GetExtent();

        Rect<uint32> src_rect {
            0, 0,
            extent.x,
            extent.y
        };

        Rect<uint32> dest_rect {
            uint32(location.x),
            uint32(location.y),
            uint32(location.x + extent.x),
            uint32(location.y + extent.y)
        };

        AssertThrowMsg(cell_dimensions.x >= extent.x, "Cell width (%u) is less than glyph width (%u)", cell_dimensions.x, extent.x);
        AssertThrowMsg(cell_dimensions.y >= extent.y, "Cell height (%u) is less than glyph height (%u)", cell_dimensions.y, extent.y);

        commands.Push([&](RHICommandList& cmd)
            {
                // put src image in state for copying from
                cmd.Add<InsertBarrier>(image, renderer::ResourceState::COPY_SRC);

                // put dst image in state for copying to
                cmd.Add<InsertBarrier>(atlas_texture->GetRenderResource().GetImage(), renderer::ResourceState::COPY_DST);

                cmd.Add<BlitRect>(
                    image,
                    atlas_texture->GetRenderResource().GetImage(),
                    src_rect,
                    dest_rect);
            });

        return commands.Execute();
    }
};

#pragma endregion Render commands

#pragma region FontAtlasTextureSet

FontAtlasTextureSet::~FontAtlasTextureSet()
{
    for (auto& atlas : atlases)
    {
        g_safe_deleter->SafeRelease(std::move(atlas.second));
    }
}

void FontAtlasTextureSet::AddAtlas(uint32 pixel_size, Handle<Texture> texture, bool is_main_atlas)
{
    if (is_main_atlas)
    {
        AssertThrowMsg(!main_atlas.IsValid(), "Main atlas already set");
    }

    if (!texture.IsValid())
    {
        return;
    }

    atlases[pixel_size] = texture;

    if (is_main_atlas)
    {
        main_atlas = texture;
    }
}

Handle<Texture> FontAtlasTextureSet::GetAtlasForPixelSize(uint32 pixel_size) const
{
    auto it = atlases.Find(pixel_size);

    if (it != atlases.End())
    {
        return it->second;
    }

    it = atlases.UpperBound(pixel_size);

    if (it != atlases.End())
    {
        return it->second;
    }

    return Handle<Texture> {};
}

#pragma endregion FontAtlasTextureSet

#pragma region FontAtlas

FontAtlas::FontAtlas(const FontAtlasTextureSet& atlas_textures, Vec2i cell_dimensions, GlyphMetricsBuffer glyph_metrics, SymbolList symbol_list)
    : m_atlas_textures(std::move(atlas_textures)),
      m_cell_dimensions(cell_dimensions),
      m_glyph_metrics(std::move(glyph_metrics)),
      m_symbol_list(std::move(symbol_list))
{
    AssertThrow(m_symbol_list.Size() != 0);

    for (auto& it : m_atlas_textures.atlases)
    {
        if (!it.second.IsValid())
        {
            continue;
        }

        InitObject(it.second);
    }
}

FontAtlas::FontAtlas(RC<FontFace> face)
    : m_face(std::move(face)),
      m_symbol_list(GetDefaultSymbolList())
{
    AssertThrow(m_symbol_list.Size() != 0);

    // Each cell will be the same size at the largest symbol
    m_cell_dimensions = FindMaxDimensions(m_face);
}

FontAtlas::SymbolList FontAtlas::GetDefaultSymbolList()
{
    // first renderable symbol
    static constexpr uint32 char_range_start = 33; // !

    // highest symbol in the ascii table
    static constexpr uint32 char_range_end = 126; // ~ + 1

    SymbolList symbol_list;
    symbol_list.Reserve(char_range_end - char_range_start + 1);

    for (uint32 ch = char_range_start; ch <= char_range_end; ch++)
    {
        symbol_list.PushBack(ch);
    }

    return symbol_list;
}

void FontAtlas::Render()
{
    AssertThrow(m_face != nullptr);

    if ((m_symbol_list.Size() / s_symbol_columns) > s_symbol_rows)
    {
        HYP_LOG(Font, Warning, "Symbol list size is greater than the allocated font atlas!");
    }

    m_glyph_metrics.Clear();
    m_glyph_metrics.Resize(m_symbol_list.Size());

    const auto render_glyphs = [&](float scale, bool is_main_atlas)
    {
        const Vec2i scaled_extent {
            MathUtil::Ceil<float, int>(float(m_cell_dimensions.x) * scale),
            MathUtil::Ceil<float, int>(float(m_cell_dimensions.y) * scale)
        };

        HYP_LOG(Font, Info, "Rendering font atlas for pixel size {}", scaled_extent.y);

        Handle<Texture> texture = CreateObject<Texture>(TextureDesc {
            ImageType::TEXTURE_TYPE_2D,
            InternalFormat::RGBA8,
            Vec3u { uint32(scaled_extent.x * s_symbol_columns), uint32(scaled_extent.y * s_symbol_rows), 1 },
            FilterMode::TEXTURE_FILTER_NEAREST,
            FilterMode::TEXTURE_FILTER_NEAREST,
            WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE });

        InitObject(texture);

        for (SizeType i = 0; i < m_symbol_list.Size(); i++)
        {
            const auto& symbol = m_symbol_list[i];

            const Vec2i index { int(i % s_symbol_columns), int(i / s_symbol_columns) };
            const Vec2i position = index * scaled_extent;

            Glyph glyph(m_face, m_face->GetGlyphIndex(symbol), scale);
            // Render the glyph into a temporary texture
            glyph.Render();

            Handle<Texture> glyph_texture = glyph.GetImageData().CreateTexture();
            AssertThrow(InitObject(glyph_texture));

            if (is_main_atlas)
            {
                m_glyph_metrics[i] = glyph.GetMetrics();
                m_glyph_metrics[i].image_position = position;
            }

            if (index.y > s_symbol_rows)
            {
                break;
            }

            // Render our character texture => atlas texture
            RenderCharacter(texture, glyph_texture, position, scaled_extent);
        }

        // Readback the texture to streamed texture data
        texture->Readback();

        // Add initial atlas
        m_atlas_textures.AddAtlas(scaled_extent.y, std::move(texture), is_main_atlas);
    };

    render_glyphs(1.0f, true);

    for (float i = 1.1f; i <= 3.0f; i += 0.1f)
    {
        render_glyphs(i, false);
    }

    if (!Threads::IsOnThread(g_render_thread))
    {
        // Sync render commands if not on render thread (render commands are executed inline if on render thread)
        HYP_SYNC_RENDER();
    }
}

void FontAtlas::RenderCharacter(const Handle<Texture>& atlas_texture, const Handle<Texture>& glyph_texture, Vec2i location, Vec2i dimensions) const
{
    PUSH_RENDER_COMMAND(FontAtlas_RenderCharacter, atlas_texture, glyph_texture, location, dimensions);
}

Vec2i FontAtlas::FindMaxDimensions(const RC<FontFace>& face) const
{
    Vec2i highest_dimensions = { 0, 0 };

    for (const auto& symbol : m_symbol_list)
    {
        // Create the glyph but only load in the metadata
        Glyph glyph(face, face->GetGlyphIndex(symbol), 1.0f);
        glyph.LoadMetrics();

        // Get the size of each glyph
        Vec2i size = glyph.GetMax();

        if (size.x > highest_dimensions.x)
        {
            highest_dimensions.x = size.x;
        }

        if (size.y > highest_dimensions.y)
        {
            highest_dimensions.y = size.y;
        }
    }

    return highest_dimensions;
}

Optional<Glyph::Metrics> FontAtlas::GetGlyphMetrics(FontFace::WChar symbol) const
{
    const auto it = m_symbol_list.Find(symbol);

    if (it == m_symbol_list.End())
    {
        return {};
    }

    const auto index = std::distance(m_symbol_list.Begin(), it);
    AssertThrowMsg(index < m_glyph_metrics.Size(), "Index (%u) out of bounds of glyph metrics buffer (%u)", index, m_glyph_metrics.Size());

    return m_glyph_metrics[index];
}

void FontAtlas::WriteToBuffer(uint32 pixel_size, ByteBuffer& buffer) const
{
    Handle<Texture> atlas = m_atlas_textures.GetAtlasForPixelSize(pixel_size);

    if (!atlas.IsValid())
    {
        HYP_LOG(Font, Error, "Failed to get atlas for pixel size {}", pixel_size);

        return;
    }

    const uint32 buffer_size = atlas->GetTextureDesc().GetByteSize();
    buffer.SetSize(buffer_size);

    struct RENDER_COMMAND(FontAtlas_WriteToBuffer)
        : renderer::RenderCommand
    {
        Handle<Texture> atlas;
        ByteBuffer& out_buffer;
        uint32 buffer_size;

        GPUBufferRef buffer;

        RENDER_COMMAND(FontAtlas_WriteToBuffer)(Handle<Texture> atlas, ByteBuffer& out_buffer, uint32 buffer_size)
            : atlas(std::move(atlas)),
              out_buffer(out_buffer),
              buffer_size(buffer_size)
        {
        }

        virtual ~RENDER_COMMAND(FontAtlas_WriteToBuffer)() override
        {
            g_safe_deleter->SafeRelease(std::move(atlas));

            SafeRelease(std::move(buffer));
        }

        virtual RendererResult operator()() override
        {
            RendererResult result;

            buffer = g_rendering_api->MakeGPUBuffer(GPUBufferType::STAGING_BUFFER, buffer_size);
            HYPERION_ASSERT_RESULT(buffer->Create());

            if (!result)
            {
                return result;
            }

            renderer::SingleTimeCommands commands;

            AssertThrow(atlas);
            AssertThrow(atlas->IsReady());

            commands.Push([&](RHICommandList& cmd)
                {
                    // put src image in state for copying from
                    cmd.Add<InsertBarrier>(atlas->GetRenderResource().GetImage(), renderer::ResourceState::COPY_SRC);

                    // put dst buffer in state for copying to
                    cmd.Add<InsertBarrier>(buffer, renderer::ResourceState::COPY_DST);

                    cmd.Add<CopyImageToBuffer>(atlas->GetRenderResource().GetImage(), buffer);

                    cmd.Add<InsertBarrier>(buffer, renderer::ResourceState::COPY_SRC);
                });

            HYPERION_PASS_ERRORS(commands.Execute(), result);

            buffer->Read(buffer_size, out_buffer.Data());

            HYPERION_RETURN_OK;
        }
    };

    PUSH_RENDER_COMMAND(FontAtlas_WriteToBuffer, atlas, buffer, buffer_size);
    HYP_SYNC_RENDER();
}

Bitmap<1> FontAtlas::GenerateBitmap(uint32 pixel_size) const
{
    Handle<Texture> atlas = m_atlas_textures.GetAtlasForPixelSize(pixel_size);

    if (!atlas.IsValid())
    {
        HYP_LOG(Font, Error, "Failed to get atlas for pixel size {}", pixel_size);

        return {};
    }

    ByteBuffer byte_buffer;
    WriteToBuffer(pixel_size, byte_buffer);

    Bitmap<1> bitmap(atlas->GetExtent().x, atlas->GetExtent().y);
    bitmap.SetPixels(byte_buffer);
    bitmap.FlipVertical();

    return bitmap;
}

json::JSONValue FontAtlas::GenerateMetadataJSON(const String& output_directory) const
{
    json::JSONObject value;

    json::JSONObject atlases_value;
    json::JSONObject pixel_sizes_value;

    uint32 main_atlas_key = uint32(-1);

    for (const auto& it : m_atlas_textures.atlases)
    {
        if (!it.second.IsValid())
        {
            continue;
        }

        if (m_atlas_textures.main_atlas == it.second)
        {
            main_atlas_key = it.first;
        }

        const String key_string = String::ToString(it.first);

        pixel_sizes_value[key_string] = FilePath(output_directory) / ("atlas_" + key_string + ".bmp");
    }

    atlases_value["pixel_sizes"] = std::move(pixel_sizes_value);
    atlases_value["main"] = json::JSONNumber(main_atlas_key);

    value["atlases"] = std::move(atlases_value);

    value["cell_dimensions"] = json::JSONObject {
        { "width", json::JSONNumber(m_cell_dimensions.x) },
        { "height", json::JSONNumber(m_cell_dimensions.y) }
    };

    json::JSONArray metrics_array;

    for (const Glyph::Metrics& metric : m_glyph_metrics)
    {
        metrics_array.PushBack(json::JSONObject {
            { "width", json::JSONNumber(metric.metrics.width) },
            { "height", json::JSONNumber(metric.metrics.height) },
            { "bearing_x", json::JSONNumber(metric.metrics.bearing_x) },
            { "bearing_y", json::JSONNumber(metric.metrics.bearing_y) },
            { "advance", json::JSONNumber(metric.metrics.advance) },
            { "image_position", json::JSONObject { { "x", json::JSONNumber(metric.image_position.x) }, { "y", json::JSONNumber(metric.image_position.y) } } } });
    }

    value["metrics"] = std::move(metrics_array);

    json::JSONArray symbol_list_array;

    for (const auto& symbol : m_symbol_list)
    {
        symbol_list_array.PushBack(json::JSONNumber(symbol));
    }

    value["symbol_list"] = std::move(symbol_list_array);

    return value;
}

#pragma endregion FontAtlas

}; // namespace hyperion
