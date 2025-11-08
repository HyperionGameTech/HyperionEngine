/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <asset/font_loaders/FontAtlasLoader.hpp>
#include <asset/Assets.hpp>

#include <ui/font/FontAtlas.hpp>

#include <rendering/Texture.hpp>

#include <core/utilities/Format.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/json/JSON.hpp>

#include <FontAtlasLoader.generated.inl>

namespace hyperion {

using namespace json;

AssetLoadResult FontAtlasLoader::LoadAsset(LoaderState& state) const
{
    Assert(state.assetManager != nullptr);
    JSONValue json;

    const ByteBuffer byteBuffer = state.stream.ReadBytes();

    if (!byteBuffer.Size())
    {
        return HYP_MAKE_ERROR(AssetLoadError, "Empty JSON file", AssetLoadError::ERR_EOF);
    }

    const String jsonString(byteBuffer.ToByteView());

    const auto jsonParseResult = JSON::Parse(jsonString);

    if (!jsonParseResult.ok)
    {
        return HYP_MAKE_ERROR(AssetLoadError, "Failed to parse json: {}", jsonParseResult.message);
    }

    const json::JSONValue jsonValue = jsonParseResult.value;

    FontAtlasTextureSet textureSet;

    if (auto atlasesValue = jsonValue["atlases"]; atlasesValue.IsObject())
    {
        uint32 mainValueKey;

        if (auto mainValue = atlasesValue["main"]; mainValue.IsNumber() || mainValue.IsString())
        {
            mainValueKey = uint32(mainValue.ToNumber());
        }
        else
        {
            return HYP_MAKE_ERROR(AssetLoadError, "Failed to read 'atlases.main' integer");
        }

        if (auto pixelSizesValue = atlasesValue["pixel_sizes"]; pixelSizesValue.IsObject())
        {
            bool mainAtlasFound = false;

            for (const auto& it : pixelSizesValue.AsObject())
            {
                Handle<Texture> bitmapTexture;

                auto bitmapTextureAsset = state.assetManager->Load<Texture>(it.second.ToString());

                if (bitmapTextureAsset.HasValue())
                {
                    bitmapTexture = bitmapTextureAsset->Result();
                }
                else
                {
                    return HYP_MAKE_ERROR(AssetLoadError, "Failed to load bitmap texture: {}", it.second.ToString());
                }

                uint32 keyValue;

                if (!StringUtil::Parse(it.first, &keyValue))
                {
                    return HYP_MAKE_ERROR(AssetLoadError, "Invalid key for font atlas: {} is not able to be parsed as uint32", it.first);
                }

                bool isMainAtlas = keyValue == mainValueKey;

                if (isMainAtlas)
                {
                    if (mainAtlasFound)
                    {
                        HYP_LOG(Assets, Warning, "Multiple elements detected as main atlas");

                        isMainAtlas = false;
                    }
                    else
                    {
                        mainAtlasFound = true;
                    }
                }

                textureSet.AddAtlas(keyValue, std::move(bitmapTexture), isMainAtlas);
            }

            if (!mainAtlasFound)
            {
                return HYP_MAKE_ERROR(AssetLoadError, "Main atlas not found in list of atlases");
            }
        }
        else
        {
            return HYP_MAKE_ERROR(AssetLoadError, "Failed to read 'atlases.pixel_sizes' object");
        }
    }
    else
    {
        return HYP_MAKE_ERROR(AssetLoadError, "Failed to read 'atlases' object");
    }

    Vec2i cellDimensions;

    if (auto cellDimensionsValue = jsonValue["cell_dimensions"])
    {
        cellDimensions.x = int(cellDimensionsValue["width"].ToNumber());
        cellDimensions.y = int(cellDimensionsValue["height"].ToNumber());
    }
    else
    {
        return HYP_MAKE_ERROR(AssetLoadError, "Failed to load cell dimensions");
    }

    FontAtlas::GlyphMetricsBuffer glyphMetrics;

    if (auto glyphMetricsValue = jsonValue["metrics"])
    {
        if (!glyphMetricsValue.IsArray())
        {
            return HYP_MAKE_ERROR(AssetLoadError, "Glyph metrics expected to be an array");
        }

        for (const JSONValue& glyphMetricValue : glyphMetricsValue.AsArray())
        {
            Glyph::Metrics metrics {};

            metrics.width = uint16(glyphMetricValue["width"].ToNumber());
            metrics.height = uint16(glyphMetricValue["height"].ToNumber());
            metrics.bearingX = int16(glyphMetricValue["bearing_x"].ToNumber());
            metrics.bearingY = int16(glyphMetricValue["bearing_y"].ToNumber());
            metrics.advance = uint8(glyphMetricValue["advance"].ToNumber());

            metrics.imagePosition.x = int32(glyphMetricValue["image_position"]["x"].ToNumber());
            metrics.imagePosition.y = int32(glyphMetricValue["image_position"]["y"].ToNumber());

            glyphMetrics.PushBack(metrics);
        }
    }
    else
    {
        return HYP_MAKE_ERROR(AssetLoadError, "Failed to load glyph metrics");
    }

    FontAtlas::SymbolList symbolList;

    if (auto symbolListValue = jsonValue["symbol_list"])
    {
        if (!symbolListValue.IsArray())
        {
            return HYP_MAKE_ERROR(AssetLoadError, "Symbol list expected to be an array");
        }

        for (const JSONValue& symbolValue : symbolListValue.AsArray())
        {
            if (!symbolValue.IsNumber())
            {
                return HYP_MAKE_ERROR(AssetLoadError, "Symbol list expected to be an array of numbers");
            }

            symbolList.PushBack(FontFace::WChar(symbolValue.AsNumber()));
        }

        if (symbolList.Empty())
        {
            return HYP_MAKE_ERROR(AssetLoadError, "No symbols in symbol list");
        }
    }
    else
    {
        return HYP_MAKE_ERROR(AssetLoadError, "Failed to load symbol list");
    }

    Handle<FontAtlas> fontAtlas = CreateObject<FontAtlas>(textureSet, cellDimensions, std::move(glyphMetrics), std::move(symbolList));

    return AssetLoadResult { fontAtlas };
}

} // namespace hyperion
