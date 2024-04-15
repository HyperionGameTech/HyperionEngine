/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/font_loaders/FontAtlasLoader.hpp>
#include <rendering/font/FontAtlas.hpp>

#include <util/json/JSON.hpp>

#include <Engine.hpp>

namespace hyperion::v2 {

using namespace json;

LoadedAsset FontAtlasLoader::LoadAsset(LoaderState &state) const
{
    AssertThrow(state.asset_manager != nullptr);
    JSONValue json;

    ByteBuffer byte_buffer = state.stream.ReadBytes();

    if (!byte_buffer.Size()) {
        return { { LoaderResult::Status::ERR_EOF } };
    }

    const String json_string(byte_buffer);

    const auto json_parse_result = JSON::Parse(json_string);

    if (!json_parse_result.ok) {
        return { { LoaderResult::Status::ERR, json_parse_result.message } };
    }

    const json::JSONValue json_value = json_parse_result.value;

    Handle<Texture> bitmap_texture;

    if (auto bitmap_filepath_value = json_value["bitmap_filepath"]) {
        bitmap_texture = state.asset_manager ->Load<Texture>(bitmap_filepath_value.AsString());

        if (!bitmap_texture.IsValid()) {
            return { { LoaderResult::Status::ERR, "Failed to load bitmap texture" } };
        }

        InitObject(bitmap_texture);
    }

    Extent2D cell_dimensions;

    if (auto cell_dimensions_value = json_value["cell_dimensions"]) {
        cell_dimensions.width = uint32(cell_dimensions_value["width"].ToNumber());
        cell_dimensions.height = uint32(cell_dimensions_value["height"].ToNumber());
    } else {
        return { { LoaderResult::Status::ERR, "Failed to load cell dimensions" } };
    }

    FontAtlas::GlyphMetricsBuffer glyph_metrics;

    if (auto glyph_metrics_value = json_value["metrics"]) {
        if (!glyph_metrics_value.IsArray()) {
            return { { LoaderResult::Status::ERR, "Glyph metrics expected to be an array" } };
        }

        for (const JSONValue &glyph_metric_value : glyph_metrics_value.AsArray()) {
            Glyph::Metrics metrics { };

            metrics.metrics.width = uint16(glyph_metric_value["width"].ToNumber());
            metrics.metrics.height = uint16(glyph_metric_value["height"].ToNumber());
            metrics.metrics.bearing_x = int16(glyph_metric_value["bearing_x"].ToNumber());
            metrics.metrics.bearing_y = int16(glyph_metric_value["bearing_y"].ToNumber());
            metrics.metrics.advance = uint8(glyph_metric_value["advance"].ToNumber());

            metrics.image_position.x = int32(glyph_metric_value["image_position"]["x"].ToNumber());
            metrics.image_position.y = int32(glyph_metric_value["image_position"]["y"].ToNumber());

            glyph_metrics.PushBack(metrics);
        }
    } else {
        return { { LoaderResult::Status::ERR, "Failed to load glyph metrics" } };
    }

    FontAtlas::SymbolList symbol_list;

    if (auto symbol_list_value = json_value["symbol_list"]) {
        if (!symbol_list_value.IsArray()) {
            return { { LoaderResult::Status::ERR, "Symbol list expected to be an array" } };
        }

        for (const JSONValue &symbol_value : symbol_list_value.AsArray()) {
            if (!symbol_value.IsNumber()) {
                return { { LoaderResult::Status::ERR, "Symbol list expected to be an array of numbers" } };
            }

            symbol_list.PushBack(FontFace::WChar(symbol_value.AsNumber()));
        }
    } else {
        return { { LoaderResult::Status::ERR, "Failed to load symbol list" } };
    }

    // @TODO: Fix
    UniquePtr<FontAtlas> font_atlas;

    // UniquePtr<FontAtlas> font_atlas(new FontAtlas(std::move(bitmap_texture), cell_dimensions, std::move(glyph_metrics)));

    return { { LoaderResult::Status::OK }, font_atlas.Cast<void>() };
}

} // namespace hyperion::v2
