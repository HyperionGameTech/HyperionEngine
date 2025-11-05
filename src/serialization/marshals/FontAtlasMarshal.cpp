/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <core/serialization/fbom/FBOM.hpp>
#include <core/serialization/fbom/FBOMArray.hpp>
#include <core/serialization/fbom/FBOMMarshaler.hpp>

#include <core/reflection/Class.hpp>
#include <core/reflection/Property.hpp>
#include <core/utilities/Format.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <rendering/font/FontAtlas.hpp>

#include <rendering/Texture.hpp>

namespace hyperion::serialization {

class FontAtlasTextureSetMarshal : public FBOMObjectMarshalerBase<FontAtlasTextureSet>
{
public:
    virtual FBOMResult Serialize(const FontAtlasTextureSet& textureSet, FBOMObject& out) const override
    {
        uint32 mainAtlasKey = uint32(-1);

        FBOMArray atlasArray;

        for (const auto& it : textureSet.atlases)
        {
            if (!it.second.IsValid())
            {
                continue;
            }

            if (it.second == textureSet.mainAtlas)
            {
                mainAtlasKey = it.first;
            }

            FBOMObject object;
            object.SetProperty("Key", it.first);
            object.SetProperty("Texture", FBOMData::FromObject(FBOMObject::Serialize(*it.second)));

            atlasArray.AddElement(FBOMData::FromObject(std::move(object)));
        }

        out.SetProperty("Atlases", FBOMData::FromArray(std::move(atlasArray)));
        out.SetProperty("MainAtlas", mainAtlasKey);

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(FBOMLoadContext& context, const FBOMObject& in, HypData& out) const override
    {
        FontAtlasTextureSet result;

        uint32 mainAtlasKey = uint32(-1);

        if (FBOMResult err = in.GetProperty("MainAtlas").ReadUInt32(&mainAtlasKey))
        {
            return err;
        }

        FBOMArray atlasArray;

        if (FBOMResult err = in.GetProperty("Atlases").ReadArray(context, atlasArray))
        {
            return err;
        }

        bool mainAtlasFound = false;

        for (SizeType index = 0; index < atlasArray.Size(); index++)
        {
            const FBOMData& element = atlasArray.GetElement(index);

            FBOMObject object;

            if (FBOMResult err = element.ReadObject(context, object))
            {
                return err;
            }

            uint32 key;

            if (FBOMResult err = object.GetProperty("Key").ReadUInt32(&key))
            {
                return err;
            }

            Handle<Texture> texture;

            FBOMObject textureObject;

            if (FBOMResult err = object.GetProperty("Texture").ReadObject(context, textureObject))
            {
                return err;
            }

            if (textureObject.m_deserializedObject == nullptr || !textureObject.m_deserializedObject->Is<Handle<Texture>>())
            {
                return FBOMResult { FBOMResult::FBOM_ERR, "Texture object for font atlas is not a Texture" };
            }

            bool isMainAtlas = key == mainAtlasKey;

            if (isMainAtlas)
            {
                if (mainAtlasFound)
                {
                    HYP_LOG(Serialization, Warning, "Multiple atlases would be set to main atlas");

                    isMainAtlas = false;
                }
                else
                {
                    mainAtlasFound = true;
                }
            }

            result.AddAtlas(key, textureObject.m_deserializedObject->Get<Handle<Texture>>(), isMainAtlas);
        }

        out = HypData(result);

        return { FBOMResult::FBOM_OK };
    }
};

HYP_DEFINE_MARSHAL(FontAtlasTextureSet, FontAtlasTextureSetMarshal);

class FontAtlasMarshal : public FBOMObjectMarshalerBase<FontAtlas>
{
public:
    virtual FBOMResult Serialize(const FontAtlas& fontAtlas, FBOMObject& out) const override
    {
        out.SetProperty("AtlasTextures", FBOMData::FromObject(FBOMObject::Serialize(fontAtlas.GetAtlasTextures())));

        FBOMArray symbolListArray;

        for (SizeType index = 0; index < fontAtlas.GetSymbolList().Size(); index++)
        {
            symbolListArray.AddElement(FBOMData::FromUInt32(fontAtlas.GetSymbolList()[index]));
        }

        out.SetProperty("SymbolList", FBOMData::FromArray(std::move(symbolListArray)));
        out.SetProperty("CellDimensions", FBOMData::FromVec2u(Vec2u(fontAtlas.GetCellDimensions())));

        FBOMArray glyphMetricsArray;

        for (SizeType index = 0; index < fontAtlas.GetGlyphMetrics().Size(); index++)
        {
            glyphMetricsArray.AddElement(FBOMData::FromStruct(fontAtlas.GetGlyphMetrics()[index]));
        }

        out.SetProperty("GlyphMetrics", FBOMData::FromArray(std::move(glyphMetricsArray)));

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(FBOMLoadContext& context, const FBOMObject& in, HypData& out) const override
    {
        FBOMObject atlasTexturesObject;

        if (FBOMResult err = in.GetProperty("AtlasTextures").ReadObject(context, atlasTexturesObject))
        {
            return err;
        }

        if (!atlasTexturesObject.m_deserializedObject || !atlasTexturesObject.m_deserializedObject->Is<FontAtlasTextureSet>())
        {
            return { FBOMResult::FBOM_ERR, "AtlasTextures must be of type FontAtlasTextureSet" };
        }

        const FontAtlasTextureSet& atlasTextures = atlasTexturesObject.m_deserializedObject->Get<FontAtlasTextureSet>();

        FBOMArray glyphMetricsArray;

        if (FBOMResult err = in.GetProperty("GlyphMetrics").ReadArray(context, glyphMetricsArray))
        {
            return err;
        }

        FontAtlas::GlyphMetricsBuffer glyphMetrics;
        glyphMetrics.Resize(glyphMetricsArray.Size());

        for (SizeType index = 0; index < glyphMetricsArray.Size(); index++)
        {
            if (FBOMResult err = glyphMetricsArray.GetElement(index).ReadStruct<Glyph::Metrics>(&glyphMetrics[index]))
            {
                return err;
            }
        }

        FBOMArray symbolListArray;

        if (FBOMResult err = in.GetProperty("SymbolList").ReadArray(context, symbolListArray))
        {
            return err;
        }

        FontAtlas::SymbolList symbolList;
        symbolList.Resize(symbolListArray.Size());

        for (SizeType index = 0; index < symbolListArray.Size(); index++)
        {
            if (FBOMResult err = symbolListArray.GetElement(index).ReadUInt32(&symbolList[index]))
            {
                return err;
            }
        }

        Vec2u cellDimensions;

        if (FBOMResult err = in.GetProperty("CellDimensions").ReadVec2u(&cellDimensions))
        {
            return err;
        }

        Handle<FontAtlas> result = CreateObject<FontAtlas>(atlasTextures, Vec2i(cellDimensions), std::move(glyphMetrics), std::move(symbolList));

        out = HypData(std::move(result));

        return { FBOMResult::FBOM_OK };
    }
};

HYP_DEFINE_MARSHAL(FontAtlas, FontAtlasMarshal);

} // namespace hyperion::serialization
