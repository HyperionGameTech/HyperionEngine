/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <UIPch.hpp>

#include <ui/font/FontAtlas.hpp>

#include <rendering/RenderInterface.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/CommandRecorder.hpp>
#include <rendering/RenderCommand.hpp>
#include <rendering/RenderHelpers.hpp>
#include <rendering/Texture.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <asset/Assets.hpp>
#include <asset/AssetRegistry.hpp>

#include <Core/utilities/DeferredScope.hpp>

#include <Core/io/ByteWriter.hpp>

#include <FontAtlas.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Font);

#pragma region FontAtlasTextureSet

FontAtlasTextureSet::~FontAtlasTextureSet()
{
    for (auto& atlas : atlases)
    {
        EnqueueDeletion(std::move(atlas.second));
    }
}

void FontAtlasTextureSet::AddAtlas(uint32 pixelSize, Handle<Texture> texture, bool isMainAtlas)
{
    if (isMainAtlas)
    {
        AssertDebug(!mainAtlas.IsValid(), "Main atlas already set");
    }

    if (!texture.IsValid())
    {
        return;
    }

    atlases[pixelSize] = texture;

    if (isMainAtlas)
    {
        mainAtlas = texture;
    }
}

Handle<Texture> FontAtlasTextureSet::GetAtlasForPixelSize(uint32 pixelSize) const
{
    auto it = atlases.Find(pixelSize);

    if (it != atlases.End())
    {
        return it->second;
    }

    it = atlases.UpperBound(pixelSize);

    if (it != atlases.End())
    {
        return it->second;
    }

    return Handle<Texture> {};
}

#pragma endregion FontAtlasTextureSet

#pragma region FontAtlas

FontAtlas::FontAtlas(
    const FontAtlasTextureSet& atlasTextures,
    Vec2i cellDimensions,
    GlyphMetricsBuffer glyphMetrics,
    SymbolList symbolList)
    : m_atlasTextures(std::move(atlasTextures)),
      m_cellDimensions(cellDimensions),
      m_glyphMetrics(std::move(glyphMetrics)),
      m_symbolList(std::move(symbolList))
{
    Assert(m_symbolList.Size() != 0);

    for (auto& it : m_atlasTextures.atlases)
    {
        if (!it.second.IsValid())
        {
            continue;
        }

        CheckResult(it.second->Create());
    }
}

FontAtlas::FontAtlas(RC<FontFace> face)
    : m_face(std::move(face)),
      m_symbolList(GetDefaultSymbolList())
{
    Assert(m_symbolList.Size() != 0);

    // Each cell will be the same size at the largest symbol
    m_cellDimensions = FindMaxDimensions(m_face);
}

FontAtlas::~FontAtlas() = default;

FontAtlas::SymbolList FontAtlas::GetDefaultSymbolList()
{
    // first renderable symbol
    static constexpr uint32 CharRangeStart = 33; // !

    // highest symbol in the ascii table
    static constexpr uint32 CharRangeEnd = 126; // ~ + 1

    SymbolList symbolList;
    symbolList.Reserve(CharRangeEnd - CharRangeStart + 1);

    for (uint32 ch = CharRangeStart; ch <= CharRangeEnd; ch++)
    {
        symbolList.PushBack(ch);
    }

    return symbolList;
}

Result FontAtlas::RenderAtlasTextures(float mainAtlasScale, float maxScale, float step)
{
    Assert(m_face != nullptr);

    if ((m_symbolList.Size() / SymbolColumns) > SymbolRows)
    {
        HYP_LOG(Font, Warning, "Symbol list size is greater than the allocated font atlas!");
    }

    m_glyphMetrics.Clear();
    m_glyphMetrics.Resize(m_symbolList.Size());

    const auto RenderGlyphs = [&](float scale, bool isMainAtlas) -> Result
    {
        const Vec2i scaledExtent {
            MathUtil::Ceil<float, int>(float(m_cellDimensions.x) * scale),
            MathUtil::Ceil<float, int>(float(m_cellDimensions.y) * scale)
        };

        HYP_LOG(Font, Verbose, "Rendering font atlas for pixel size {}", scaledExtent.y);

        UniquePtr<FontAtlasBitmap> atlasBitmap = MakeUnique<FontAtlasBitmap>(uint32(scaledExtent.x * SymbolColumns), uint32(scaledExtent.y * SymbolRows));

        for (size_t i = 0; i < m_symbolList.Size(); i++)
        {
            const FontFace::WChar symbol = m_symbolList[i];

            const Vec2i index { int(i % SymbolColumns), int(i / SymbolColumns) };
            const Vec2i offset = index * scaledExtent;

            if (index.y > SymbolRows)
            {
                break;
            }

            Glyph glyph { m_face, m_face->GetGlyphIndex(symbol), scale };
            glyph.LoadMetrics();

            if (isMainAtlas)
            {
                m_glyphMetrics[i] = glyph.GetMetrics();
                m_glyphMetrics[i].imagePosition = offset;
            }

            TResult<UniquePtr<GlyphBitmap>> glyphRasterizeResult = glyph.Rasterize();

            if (glyphRasterizeResult.HasError())
            {
                HYP_LOG(Font, Error, "Failed to rasterize glyph for symbol '{}': {}", symbol, glyphRasterizeResult.GetError().GetMessage());

                return glyphRasterizeResult.GetError();
            }

            UniquePtr<GlyphBitmap> glyphBitmap = std::move(glyphRasterizeResult.GetValue());
            AssertDebug(glyphBitmap != nullptr);

            Rect<uint32> srcRect {
                0, 0,
                uint32(scaledExtent.x),
                uint32(scaledExtent.y)
            };

            Rect<uint32> dstRect {
                uint32(offset.x),
                uint32(offset.y),
                uint32(offset.x + scaledExtent.x),
                uint32(offset.y + scaledExtent.y)
            };

            BitmapUtils::Blit(*glyphBitmap, *atlasBitmap, srcRect, dstRect);
        }

        atlasBitmap->FlipVertical();

        const TextureDesc atlasTextureDesc {
            TextureType::Texture2D,
            TextureFormat::R8,
            Vec3u { atlasBitmap->GetWidth(), atlasBitmap->GetHeight(), 1 },
            TFM_NEAREST,
            TFM_NEAREST,
            TWM_CLAMP_TO_EDGE
        };

        ByteBuffer imageData = atlasBitmap->GetUnpackedBytes(1);

        Handle<Texture> atlasTexture = MakeHandle<Texture>(atlasTextureDesc, imageData.ToByteView());
        atlasTexture->SetName(NAME_FMT("FontAtlas_{}", scale));
        CheckResult(atlasTexture->Create());

        // Add initial atlas
        m_atlasTextures.AddAtlas(scaledExtent.y, std::move(atlasTexture), isMainAtlas);

        return {};
    };

    // main
    if (Result result = RenderGlyphs(mainAtlasScale, true); result.HasError())
    {
        return result.GetError();
    }

    // different scales
    for (float i = mainAtlasScale + step; i <= maxScale; i += step)
    {
        if (auto result = RenderGlyphs(i, false); result.HasError())
        {
            return result.GetError();
        }
    }

    return {};
}

Vec2i FontAtlas::FindMaxDimensions(const RC<FontFace>& face) const
{
    Vec2i highestDimensions = { 0, 0 };

    for (const auto& symbol : m_symbolList)
    {
        // Create the glyph but only load in the metadata
        Glyph glyph(face, face->GetGlyphIndex(symbol), 1.0f);
        glyph.LoadMetrics();

        // Get the size of each glyph
        Vec2i size = glyph.GetMax();

        if (size.x > highestDimensions.x)
        {
            highestDimensions.x = size.x;
        }

        if (size.y > highestDimensions.y)
        {
            highestDimensions.y = size.y;
        }
    }

    return highestDimensions;
}

Optional<const Glyph::Metrics&> FontAtlas::GetGlyphMetrics(FontFace::WChar symbol) const
{
    const auto it = m_symbolList.Find(symbol);

    if (it == m_symbolList.End())
    {
        return {};
    }

    const size_t index = std::distance(m_symbolList.Begin(), it);
    Assert(index < m_glyphMetrics.Size(), "Index {} out of bounds of glyph metrics buffer, size: {}", index, m_glyphMetrics.Size());

    return m_glyphMetrics[index];
}

JSON::Value FontAtlas::GenerateMetadataJSON(const String& outputDirectory) const
{
    JSON::Object value;

    JSON::Object atlasesValue;
    JSON::Object pixelSizesValue;

    uint32 mainAtlasKey = uint32(-1);

    for (const auto& it : m_atlasTextures.atlases)
    {
        if (!it.second.IsValid())
        {
            continue;
        }

        if (m_atlasTextures.mainAtlas == it.second)
        {
            mainAtlasKey = it.first;
        }

        const String keyString = String::ToString(it.first);

        pixelSizesValue[keyString] = FilePath(outputDirectory) / ("atlas_" + keyString + ".bmp");
    }

    atlasesValue["pixel_sizes"] = std::move(pixelSizesValue);
    atlasesValue["main"] = JSON::Number(mainAtlasKey);

    value["atlases"] = std::move(atlasesValue);

    value["cell_dimensions"] = JSON::Object {
        { "width", JSON::Number(m_cellDimensions.x) },
        { "height", JSON::Number(m_cellDimensions.y) }
    };

    JSON::JArray metricsArray;

    for (const Glyph::Metrics& metric : m_glyphMetrics)
    {
        metricsArray.PushBack(JSON::Object {
            { "width", JSON::Number(metric.width) },
            { "height", JSON::Number(metric.height) },
            { "bearing_x", JSON::Number(metric.bearingX) },
            { "bearing_y", JSON::Number(metric.bearingY) },
            { "advance", JSON::Number(metric.advance) },
            { "image_position", JSON::Object { { "x", JSON::Number(metric.imagePosition.x) }, { "y", JSON::Number(metric.imagePosition.y) } } } });
    }

    value["metrics"] = std::move(metricsArray);

    JSON::JArray symbolListArray;

    for (const auto& symbol : m_symbolList)
    {
        symbolListArray.PushBack(JSON::Number(symbol));
    }

    value["symbol_list"] = std::move(symbolListArray);

    return value;
}

#pragma endregion FontAtlas

}; // namespace Hyperion
