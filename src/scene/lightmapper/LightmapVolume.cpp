/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/lightmapper/LightmapVolume.hpp>
#include <rendering/Texture.hpp>

#include <rendering/RenderProxy.hpp>

#include <rendering/RenderQueue.hpp>

#include <rendering/RenderCommand.hpp>
#include <rendering/RenderHelpers.hpp>
#include <rendering/RenderBackend.hpp>

#include <rendering/lightmapper/LightmapUVBuilder.hpp>

#include <asset/AssetRegistry.hpp>
#include <asset/Assets.hpp>
#include <asset/TextureAsset.hpp>

#include <core/io/ByteWriter.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/threading/Threads.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>

namespace hyperion {

#pragma region Render commands

struct RENDER_COMMAND(BakeLightmapVolumeTexture)
    : RenderCommand
{
    WeakHandle<LightmapVolume> lightmapVolumeWeak;
    Array<LightmapElement> lightmapElements;
    Array<Handle<Texture>> atlasTextures;

    RENDER_COMMAND(BakeLightmapVolumeTexture)(const WeakHandle<LightmapVolume>& lightmapVolumeWeak, const Array<LightmapElement>& lightmapElements, const Array<Handle<Texture>>& atlasTextures)
        : lightmapVolumeWeak(lightmapVolumeWeak),
          lightmapElements(lightmapElements),
          atlasTextures(atlasTextures)
    {
    }

    virtual ~RENDER_COMMAND(BakeLightmapVolumeTexture)() override
    {
    }

    virtual RendererResult operator()() override
    {
        // Ensure the array of atlas textures are resized to the correct count
        Assert(atlasTextures.Size() == uint32(LTT_MAX));

        Array<Array<Pair<const LightmapElement*, Handle<Texture>>>> elementTextures;
        elementTextures.Resize(uint32(LTT_MAX));

        for (const LightmapElement& element : lightmapElements)
        {
            for (const LightmapElementTextureEntry& entry : element.entries)
            {
                if (!atlasTextures[uint32(entry.type)])
                {
                    HYP_LOG(Lightmap, Warning, "Atlas texture for type {} is not set, skipping element {}", uint32(entry.type), element.index);

                    continue;
                }

                if (!entry.texture.IsValid())
                {
                    continue;
                }

                Assert(entry.texture->IsReady());

                elementTextures[uint32(entry.type)].EmplaceBack(&element, entry.texture);
            }
        }

        UniquePtr<SingleTimeCommands> singleTimeCommands = g_renderBackend->GetSingleTimeCommands();

        singleTimeCommands->Push([&](RenderQueue& renderQueue)
            {
                for (uint32 textureTypeIndex = 0; textureTypeIndex < uint32(LTT_MAX); textureTypeIndex++)
                {
                    if (!atlasTextures[textureTypeIndex])
                    {
                        continue;
                    }

                    const Handle<Texture>& atlasTexture = atlasTextures[textureTypeIndex];

                    renderQueue << InsertBarrier(atlasTexture->GetGpuImage(), RS_COPY_DST);

                    for (const auto& elementTexturePair : elementTextures[textureTypeIndex])
                    {
                        const LightmapElement* element = elementTexturePair.first;
                        const Handle<Texture>& elementTexture = elementTexturePair.second;

                        HYP_LOG(Lightmap, Debug, "Blitting element {} (name: {}) to atlas texture {} (dim: {}), at offset {}, dimensions {}",
                            element->index,
                            elementTexture->GetName(),
                            atlasTexture->GetName(),
                            elementTexture->GetGpuImage()->GetExtent(),
                            element->offsetCoords,
                            element->dimensions);

                        AssertDebug(element->offsetCoords.x < atlasTexture->GetExtent().x);
                        AssertDebug(element->offsetCoords.y < atlasTexture->GetExtent().y);
                        AssertDebug(element->offsetCoords.x + element->dimensions.x <= atlasTexture->GetExtent().x);
                        AssertDebug(element->offsetCoords.y + element->dimensions.y <= atlasTexture->GetExtent().y);

                        renderQueue << InsertBarrier(elementTexture->GetGpuImage(), RS_COPY_SRC);

                        renderQueue << Blit(
                            elementTexture->GetGpuImage(),
                            atlasTexture->GetGpuImage(),
                            Rect<uint32> { 0, 0, elementTexture->GetGpuImage()->GetExtent().x, elementTexture->GetGpuImage()->GetExtent().y },
                            Rect<uint32> { element->offsetCoords.x, element->offsetCoords.y, element->offsetCoords.x + element->dimensions.x, element->offsetCoords.y + element->dimensions.y },
                            0, /* srcMip */
                            0, /* dstMip */
                            0, /* srcFace */
                            0  /* dstFace */
                        );

                        renderQueue << InsertBarrier(elementTexture->GetGpuImage(), RS_SHADER_RESOURCE);
                    }

                    renderQueue << InsertBarrier(atlasTexture->GetGpuImage(), RS_SHADER_RESOURCE);
                }
            });

        Assert(singleTimeCommands->Execute());

        // DEBUGGING: Save each atlas texture to a file for debugging purposes
        HYP_LOG(Lightmap, Info, "Saving atlas textures to disk for debugging");
        for (uint32 textureTypeIndex = 0; textureTypeIndex < uint32(LTT_MAX); textureTypeIndex++)
        {
            if (!atlasTextures[textureTypeIndex])
            {
                continue;
            }

            const Handle<Texture>& atlasTexture = atlasTextures[textureTypeIndex];
            Assert(atlasTexture.IsValid() && atlasTexture->IsReady());

            atlasTexture->EnqueueReadback([atlasTextureWeak = atlasTexture.ToWeak(), textureTypeIndex](ByteBuffer&& byteBuffer) mutable
                {
                    Handle<Texture> atlasTexture = atlasTextureWeak.Lock();
                    if (!atlasTexture)
                    {
                        HYP_LOG(Lightmap, Error, "Atlas texture {} was not alive after GPU image readback!", atlasTextureWeak.Id());

                        return;
                    }

                    if (byteBuffer.Empty())
                    {
                        HYP_LOG(Lightmap, Warning, "Atlas texture {} is empty, skipping save", atlasTexture->GetName());

                        return;
                    }

                    Bitmap<4> bitmap(atlasTexture->GetExtent().x, atlasTexture->GetExtent().y);
                    bitmap.SetPixels(std::move(byteBuffer));

                    const String filename = HYP_FORMAT("lightmap_atlas_texture_{}_{}.bmp",
                        atlasTexture->GetName(),
                        uint32(LightmapTextureType(textureTypeIndex)));

                    HYP_LOG(Lightmap, Info, "Writing atlas texture {} to file {}", atlasTexture->GetName(), filename);

                    FileByteWriter fileByteWriter { filename };
                    bool res = bitmap.Write(&fileByteWriter);

                    if (!res)
                    {
                        HYP_LOG(Lightmap, Error, "Failed to write atlas texture {} to file", atlasTexture->GetName());
                    }
                });
        }

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

LightmapVolume::LightmapVolume()
    : LightmapVolume(BoundingBox::Empty())
{
}

LightmapVolume::LightmapVolume(const BoundingBox& aabb)
    : m_aabb(aabb),
      m_atlasTextures {},
      m_atlas(Vec2u(4096, 4096))
{
}

LightmapVolume::~LightmapVolume()
{
    if (IsInitCalled())
    {
        SetReady(false);
    }
}

bool LightmapVolume::AddElement(const LightmapUVMap& uvMap, LightmapElement& outElement, bool shrinkToFit, float downscaleLimit)
{
    Threads::AssertOnThread(g_gameThread);

    const Vec2u elementDimensions = { uvMap.width, uvMap.height };

    return m_atlas.AddElement(elementDimensions, outElement, shrinkToFit, downscaleLimit);
}

const LightmapElement* LightmapVolume::GetElement(uint32 index) const
{
    Threads::AssertOnThread(g_gameThread);

    if (index >= m_atlas.elements.Size())
    {
        return nullptr;
    }

    return &m_atlas.elements[index];
}

bool LightmapVolume::BuildElementTextures(const LightmapUVMap& uvMap, uint32 index)
{
    Threads::AssertOnThread(g_gameThread);

    if (index >= m_atlas.elements.Size())
    {
        return false;
    }

    LightmapElement& element = m_atlas.elements[index];

    const Vec2u elementDimensions = element.dimensions;

    FixedArray<Bitmap<4, float>, uint32(LTT_MAX)> bitmaps = {
        uvMap.ToBitmapRadiance(),  /* RADIANCE */
        uvMap.ToBitmapIrradiance() /* IRRADIANCE */
    };

    element.entries.Resize(uint32(LTT_MAX));

    static const char* textureTypeNames[uint32(LTT_MAX)] = { "R", "I" };

    for (uint32 i = 0; i < uint32(LTT_MAX); i++)
    {
        element.entries[i].type = LightmapTextureType(i);

        Handle<Texture> texture = CreateObject<Texture>(TextureData {
            TextureDesc {
                TT_TEX2D,
                TF_RGBA32F,
                Vec3u { elementDimensions, 1 },
                TFM_LINEAR,
                TFM_LINEAR,
                TWM_REPEAT },
            ByteBuffer(bitmaps[i].GetUnpackedFloats().ToByteView()) });

        texture->SetName(NAME_FMT("LightmapVolumeTexture_{}_{}_{}", m_uuid, element.index, textureTypeNames[i]));

        element.entries[i].texture = std::move(texture);
    }

    if (IsInitCalled())
    {
        // Initalize textures for the entries in the element
        for (LightmapElementTextureEntry& entry : element.entries)
        {
            if (entry.texture.IsValid())
            {
                InitObject(entry.texture);
            }
        }

        UpdateAtlasTextures();
    }

    return true;
}

void LightmapVolume::Init()
{
    for (LightmapElement& element : m_atlas.elements)
    {
        if (!element.IsValid())
        {
            continue;
        }

        for (LightmapElementTextureEntry& entry : element.entries)
        {
            InitObject(entry.texture);
        }
    }

    UpdateAtlasTextures();

    SetReady(true);
}

void LightmapVolume::UpdateRenderProxy(RenderProxyLightmapVolume* proxy)
{
    proxy->lightmapVolume = WeakHandleFromThis();

    proxy->bufferData.aabbMax = Vec4f(m_aabb.max, 1.0f);
    proxy->bufferData.aabbMin = Vec4f(m_aabb.min, 1.0f);
    proxy->bufferData.textureIndex = ~0u; // @TODO: Set the correct texture index based on the element
}

void LightmapVolume::UpdateAtlasTextures()
{
    HYP_LOG(Lightmap, Debug, "Updating atlas textures for LightmapVolume {}", m_uuid);

    static const char* textureTypeNames[uint32(LTT_MAX)] = { "R", "I" };

    Array<Handle<Texture>> elementTextures;
    elementTextures.Resize(uint32(LTT_MAX));

    for (uint32 i = 0; i < uint32(LTT_MAX); i++)
    {
        LightmapTextureType textureType = LightmapTextureType(i);

        Handle<Texture>& texture = m_atlasTextures[textureType];

        if (!texture.IsValid())
        {
            texture = CreateObject<Texture>(TextureDesc {
                TT_TEX2D,
                TF_RGBA8,
                Vec3u { m_atlas.atlasDimensions, 1 },
                TFM_LINEAR,
                TFM_LINEAR,
                TWM_CLAMP_TO_EDGE });

            texture->SetName(NAME_FMT("LightmapVolumeAtlasTexture_{}_{}_{}", m_uuid, uint32(textureType), textureTypeNames[i]));

            if (Result result = g_assetManager->GetAssetRegistry()->RegisterAsset("$Import/Media/Lightmaps", texture->GetAsset()); result.HasError())
            {
                HYP_LOG(Lightmap, Error, "Failed to register atlas texture '{}' with asset registry: {}", texture->GetName(), result.GetError().GetMessage());
            }
        }

        InitObject(texture);

        elementTextures[i] = texture;
    }

    if (IsInitCalled())
    {
        PUSH_RENDER_COMMAND(
            BakeLightmapVolumeTexture,
            WeakHandleFromThis(),
            m_atlas.elements,
            elementTextures);
    }
}

} // namespace hyperion
