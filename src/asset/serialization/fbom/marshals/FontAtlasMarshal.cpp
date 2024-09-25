/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/serialization/fbom/FBOM.hpp>
#include <asset/serialization/fbom/FBOMArray.hpp>

#include <rendering/font/FontAtlas.hpp>

#include <core/object/HypClass.hpp>
#include <core/object/HypProperty.hpp>
#include <core/object/HypData.hpp>

#include <core/utilities/Format.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

namespace hyperion::fbom {

#pragma region FBOMMarshaler<FontAtlasTextureSet>

template <>
class FBOMMarshaler<FontAtlasTextureSet> : public FBOMObjectMarshalerBase<FontAtlasTextureSet>
{
public:
    virtual ~FBOMMarshaler() override = default;

    virtual FBOMResult Serialize(const FontAtlasTextureSet &texture_set, FBOMObject &out) const override
    {
        uint32 main_atlas_key = uint32(-1);

        FBOMArray atlas_array { FBOMBaseObjectType() };

        for (const auto &it : texture_set.atlases) {
            if (!it.second.IsValid()) {
                continue;
            }

            if (it.second == texture_set.main_atlas) {
                main_atlas_key = it.first;
            }

            FBOMObject object;
            object.SetProperty("Key", it.first);
            object.SetProperty("Texture", FBOMData::FromObject(FBOMObject::Serialize(*it.second)));

            atlas_array.AddElement(FBOMData::FromObject(std::move(object)));
        }

        out.SetProperty("Atlases", FBOMData::FromArray(std::move(atlas_array)));
        out.SetProperty("MainAtlas", main_atlas_key);
        
        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(const FBOMObject &in, HypData &out) const override
    {
        FontAtlasTextureSet result;

        uint32 main_atlas_key = uint32(-1);

        if (FBOMResult err = in.GetProperty("MainAtlas").ReadUInt32(&main_atlas_key)) {
            return err;
        }

        FBOMArray atlas_array { FBOMUnset() };

        if (FBOMResult err = in.GetProperty("Atlases").ReadArray(atlas_array)) {
            return err;
        }

        bool main_atlas_found = false;

        for (SizeType index = 0; index < atlas_array.Size(); index++) {
            const FBOMData &element = atlas_array.GetElement(index);

            FBOMObject object;

            if (FBOMResult err = element.ReadObject(object)) {
                return err;
            }

            uint32 key;

            if (FBOMResult err = object.GetProperty("Key").ReadUInt32(&key)) {
                return err;
            }

            Handle<Texture> texture;

            FBOMObject texture_object;

            if (FBOMResult err = object.GetProperty("Texture").ReadObject(texture_object)) {
                return err;
            }

            if (texture_object.m_deserialized_object == nullptr || !texture_object.m_deserialized_object->Is<Handle<Texture>>()) {
                return FBOMResult { FBOMResult::FBOM_ERR, "Texture object for font atlas is not a Texture" };
            }

            bool is_main_atlas = key == main_atlas_key;

            if (is_main_atlas) {
                if (main_atlas_found) {
                    HYP_LOG(Serialization, LogLevel::WARNING, "Multiple atlases would be set to main atlas");

                    is_main_atlas = false;
                } else {
                    main_atlas_found = true;
                }
            }

            result.AddAtlas(key, texture_object.m_deserialized_object->Get<Handle<Texture>>(), is_main_atlas);
        }

        out = HypData(result);

        return { FBOMResult::FBOM_OK };
    }
};

HYP_DEFINE_MARSHAL(FontAtlasTextureSet, FBOMMarshaler<FontAtlasTextureSet>);

#pragma endregion FBOMMarshaler<FontAtlasTextureSet>

#pragma region FBOMMarshaler<FontAtlas>

template <>
class FBOMMarshaler<FontAtlas> : public FBOMObjectMarshalerBase<FontAtlas>
{
public:
    virtual ~FBOMMarshaler() override = default;

    virtual FBOMResult Serialize(const FontAtlas &font_atlas, FBOMObject &out) const override
    {
        out.SetProperty("AtlasTextures", FBOMData::FromObject(FBOMObject::Serialize(font_atlas.GetAtlasTextures())));

        FBOMArray symbol_list_array { FBOMUInt32() };

        for (SizeType index = 0; index < font_atlas.GetSymbolList().Size(); index++) {
            symbol_list_array.AddElement(FBOMData::FromUInt32(font_atlas.GetSymbolList()[index]));
        }

        out.SetProperty("SymbolList", FBOMData::FromArray(std::move(symbol_list_array)));
        out.SetProperty("CellDimensions", FBOMData::FromVec2u(font_atlas.GetCellDimensions()));
        
        FBOMType glyph_metrics_struct_type = FBOMStruct::Create<Glyph::Metrics>();
        FBOMArray glyph_metrics_array { glyph_metrics_struct_type };

        for (SizeType index = 0; index < font_atlas.GetGlyphMetrics().Size(); index++) {
            glyph_metrics_array.AddElement(FBOMData::FromStruct(font_atlas.GetGlyphMetrics()[index]));
        }
        
        out.SetProperty("GlyphMetrics", FBOMData::FromArray(std::move(glyph_metrics_array)));
        
        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(const FBOMObject &in, HypData &out) const override
    {
        FBOMObject atlas_textures_object;

        if (FBOMResult err = in.GetProperty("AtlasTextures").ReadObject(atlas_textures_object)) {
            return err;
        }

        if (!atlas_textures_object.m_deserialized_object || !atlas_textures_object.m_deserialized_object->Is<FontAtlasTextureSet>()) {
            return { FBOMResult::FBOM_ERR, "AtlasTextures must be of type FontAtlasTextureSet" };
        }

        const FontAtlasTextureSet &atlas_textures = atlas_textures_object.m_deserialized_object->Get<FontAtlasTextureSet>();

        FBOMType glyph_metrics_struct_type = FBOMStruct::Create<Glyph::Metrics>();
        FBOMArray glyph_metrics_array { FBOMUnset() };

        if (FBOMResult err = in.GetProperty("GlyphMetrics").ReadArray(glyph_metrics_array)) {
            return err;
        }

        if (!glyph_metrics_array.GetElementType().Is(glyph_metrics_struct_type)) {
            return { FBOMResult::FBOM_ERR, "GlyphMetrics struct type mismatch" };
        }

        FontAtlas::GlyphMetricsBuffer glyph_metrics;
        glyph_metrics.Resize(glyph_metrics_array.Size());

        for (SizeType index = 0; index < glyph_metrics_array.Size(); index++) {
            if (FBOMResult err = glyph_metrics_array.GetElement(index).ReadStruct<Glyph::Metrics>(&glyph_metrics[index])) {
                return err;
            }
        }

        FBOMArray symbol_list_array { FBOMUInt32() };

        if (FBOMResult err = in.GetProperty("SymbolList").ReadArray(symbol_list_array)) {
            return err;
        }

        FontAtlas::SymbolList symbol_list;
        symbol_list.Resize(symbol_list_array.Size());

        for (SizeType index = 0; index < symbol_list_array.Size(); index++) {
            if (FBOMResult err = symbol_list_array.GetElement(index).ReadUInt32(&symbol_list[index])) {
                return err;
            }
        }

        Vec2u cell_dimensions;

        if (FBOMResult err = in.GetProperty("CellDimensions").ReadVec2u(&cell_dimensions)) {
            return err;
        }

        RC<FontAtlas> result(new FontAtlas(atlas_textures, cell_dimensions, std::move(glyph_metrics), std::move(symbol_list)));

        out = HypData(std::move(result));

        return { FBOMResult::FBOM_OK };
    }
};

HYP_DEFINE_MARSHAL(FontAtlas, FBOMMarshaler<FontAtlas>);

#pragma endregion FBOMMarshaler<FontAtlas>

} // namespace hyperion::fbom