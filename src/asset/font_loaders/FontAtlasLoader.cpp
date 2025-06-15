/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/font_loaders/FontAtlasLoader.hpp>

#include <rendering/font/FontAtlas.hpp>

#include <scene/Texture.hpp>

#include <core/utilities/Format.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/json/JSON.hpp>

namespace hyperion {

using namespace json;

AssetLoadResult FontAtlasLoader::LoadAsset(LoaderState& state) const
{
    AssertThrow(state.asset_manager != nullptr);
    JSONValue json;

    const ByteBuffer byte_buffer = state.stream.ReadBytes();

    if (!byte_buffer.Size())
    {
        return HYP_MAKE_ERROR(AssetLoadError, "Empty JSON file", AssetLoadError::ERR_EOF);
    }

    const String json_string(byte_buffer.ToByteView());

    const auto json_parse_result = JSON::Parse(json_string);

    if (!json_parse_result.ok)
    {
        return HYP_MAKE_ERROR(AssetLoadError, "Failed to parse json: {}", json_parse_result.message);
    }

    const json::JSONValue json_value = json_parse_result.value;

    FontAtlasTextureSet texture_set;

    if (auto atlases_value = json_value["atlases"]; atlases_value.IsObject())
    {
        uint32 main_value_key;

        if (auto main_value = atlases_value["main"]; main_value.IsNumber() || main_value.IsString())
        {
            main_value_key = uint32(main_value.ToNumber());
        }
        else
        {
            return HYP_MAKE_ERROR(AssetLoadError, "Failed to read 'atlases.main' integer");
        }

        if (auto pixel_sizes_value = atlases_value["pixel_sizes"]; pixel_sizes_value.IsObject())
        {
            bool main_atlas_found = false;

            for (const auto& it : pixel_sizes_value.AsObject())
            {
                Handle<Texture> bitmap_texture;

                auto bitmap_texture_asset = state.asset_manager->Load<Texture>(it.second.ToString());

                if (bitmap_texture_asset.HasValue())
                {
                    bitmap_texture = bitmap_texture_asset->Result();
                }
                else
                {
                    return HYP_MAKE_ERROR(AssetLoadError, "Failed to load bitmap texture: {}", it.second.ToString());
                }

                uint32 key_value;

                if (!StringUtil::Parse(it.first, &key_value))
                {
                    return HYP_MAKE_ERROR(AssetLoadError, "Invalid key for font atlas: {} is not able to be parsed as uint32", it.first);
                }

                bool is_main_atlas = key_value == main_value_key;

                if (is_main_atlas)
                {
                    if (main_atlas_found)
                    {
                        HYP_LOG(Assets, Warning, "Multiple elements detected as main atlas");

                        is_main_atlas = false;
                    }
                    else
                    {
                        main_atlas_found = true;
                    }
                }

                texture_set.AddAtlas(key_value, std::move(bitmap_texture), is_main_atlas);
            }

            if (!main_atlas_found)
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

    Vec2i cell_dimensions;

    if (auto cell_dimensions_value = json_value["cell_dimensions"])
    {
        cell_dimensions.x = int(cell_dimensions_value["width"].ToNumber());
        cell_dimensions.y = int(cell_dimensions_value["height"].ToNumber());
    }
    else
    {
        return HYP_MAKE_ERROR(AssetLoadError, "Failed to load cell dimensions");
    }

    FontAtlas::GlyphMetricsBuffer glyph_metrics;

    if (auto glyph_metrics_value = json_value["metrics"])
    {
        if (!glyph_metrics_value.IsArray())
        {
            return HYP_MAKE_ERROR(AssetLoadError, "Glyph metrics expected to be an array");
        }

        for (const JSONValue& glyph_metric_value : glyph_metrics_value.AsArray())
        {
            Glyph::Metrics metrics {};

            metrics.metrics.width = uint16(glyph_metric_value["width"].ToNumber());
            metrics.metrics.height = uint16(glyph_metric_value["height"].ToNumber());
            metrics.metrics.bearing_x = int16(glyph_metric_value["bearing_x"].ToNumber());
            metrics.metrics.bearing_y = int16(glyph_metric_value["bearing_y"].ToNumber());
            metrics.metrics.advance = uint8(glyph_metric_value["advance"].ToNumber());

            metrics.image_position.x = int32(glyph_metric_value["image_position"]["x"].ToNumber());
            metrics.image_position.y = int32(glyph_metric_value["image_position"]["y"].ToNumber());

            glyph_metrics.PushBack(metrics);
        }
    }
    else
    {
        return HYP_MAKE_ERROR(AssetLoadError, "Failed to load glyph metrics");
    }

    FontAtlas::SymbolList symbol_list;

    if (auto symbol_list_value = json_value["symbol_list"])
    {
        if (!symbol_list_value.IsArray())
        {
            return HYP_MAKE_ERROR(AssetLoadError, "Symbol list expected to be an array");
        }

        for (const JSONValue& symbol_value : symbol_list_value.AsArray())
        {
            if (!symbol_value.IsNumber())
            {
                return HYP_MAKE_ERROR(AssetLoadError, "Symbol list expected to be an array of numbers");
            }

            symbol_list.PushBack(FontFace::WChar(symbol_value.AsNumber()));
        }

        if (symbol_list.Empty())
        {
            return HYP_MAKE_ERROR(AssetLoadError, "No symbols in symbol list");
        }
    }
    else
    {
        return HYP_MAKE_ERROR(AssetLoadError, "Failed to load symbol list");
    }

    RC<FontAtlas> font_atlas = MakeRefCountedPtr<FontAtlas>(texture_set, cell_dimensions, std::move(glyph_metrics), std::move(symbol_list));

    return AssetLoadResult { font_atlas };
}

} // namespace hyperion
