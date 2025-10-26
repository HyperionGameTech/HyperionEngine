/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/lightmapper/LightmapVolume.hpp>
#include <rendering/Texture.hpp>

#include <rendering/RenderProxy.hpp>
#include <rendering/RenderQueue.hpp>
#include <rendering/RenderCommand.hpp>
#include <rendering/RenderHelpers.hpp>
#include <rendering/RenderBackend.hpp>
#include <rendering/RenderFrame.hpp>

#include <rendering/lightmapper/LightmapUVBuilder.hpp>

#include <asset/AssetRegistry.hpp>
#include <asset/Assets.hpp>
#include <asset/TextureAsset.hpp>

#include <core/io/ByteWriter.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/threading/Threads.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>

namespace hyperion {

#pragma region Render commands

struct RENDER_COMMAND(BakeLightmapAtlasTexture)
    : RenderCommand
{
    WeakHandle<LightmapVolume> lightmapVolumeWeak;
    Array<LightmapElement> lightmapElements;
    Array<Handle<Texture>> atlasTextures;
    HashMap<LightmapElement::Id, FixedArray<Handle<Texture>, LTT_MAX>> elementTextures;

    RENDER_COMMAND(BakeLightmapAtlasTexture)(
        const WeakHandle<LightmapVolume>& lightmapVolumeWeak,
        const Array<LightmapElement>& lightmapElements,
        Array<Handle<Texture>>&& atlasTextures,
        HashMap<LightmapElement::Id, FixedArray<Handle<Texture>, LTT_MAX>>&& elementTextures)
        : lightmapVolumeWeak(lightmapVolumeWeak),
          lightmapElements(lightmapElements),
          atlasTextures(std::move(atlasTextures)),
          elementTextures(std::move(elementTextures))
    {
    }

    virtual ~RENDER_COMMAND(BakeLightmapAtlasTexture)() override
    {
        SafeDelete(std::move(atlasTextures));

        for (auto& it : elementTextures)
        {
            SafeDelete(std::move(it.second));
        }
    }

    virtual RendererResult operator()() override
    {
        AssertDebug(!elementTextures.Empty());

        // Ensure the array of atlas textures are resized to the correct count
        Assert(atlasTextures.Size() == uint32(LTT_MAX));

        FrameBase* currentFrame = g_renderBackend->GetCurrentFrame();
        Assert(currentFrame != nullptr);

        RenderQueue& renderQueue = currentFrame->postRenderQueue;

        for (uint32 textureTypeIndex = 0; textureTypeIndex < uint32(LTT_MAX); textureTypeIndex++)
        {
            if (!atlasTextures[textureTypeIndex])
            {
                AssertDebug(false, "No atlas texture for lightmap texture type {}", uint32(LightmapTextureType(textureTypeIndex)));

                continue;
            }

            const Handle<Texture>& atlasTexture = atlasTextures[textureTypeIndex];
            Assert(atlasTexture != nullptr && atlasTexture->GetGpuImage()->IsCreated());

            for (auto& elementTexturesIt : elementTextures)
            {
                uint16 atlasIndex;
                uint16 elementIndex;
                LightmapElement::GetAtlasAndElementIndex(elementTexturesIt.first, atlasIndex, elementIndex);

                // @TODO: Add assertion that atlasIndex == our current atlas index

                Assert(elementIndex < lightmapElements.Size());

                const LightmapElement& element = lightmapElements[elementIndex];
                const Handle<Texture>& elementTexture = elementTexturesIt.second[textureTypeIndex];

                if (!elementTexture)
                {
                    AssertDebug(false, "Missing element texture!");
                    continue;
                }

                Assert(element.offsetCoords.x < atlasTexture->GetExtent().x);
                Assert(element.offsetCoords.y < atlasTexture->GetExtent().y);
                Assert(element.offsetCoords.x + element.dimensions.x <= atlasTexture->GetExtent().x);
                Assert(element.offsetCoords.y + element.dimensions.y <= atlasTexture->GetExtent().y);

                // Have to do the blit on frame end, as our element texture may not be init'd yet!
                //currentFrame->OnFrameEnd.Bind([atlasTexture = atlasTexture, elementTexture = elementTexture, element = element](FrameBase* frame) mutable
                //    {
                //            UniquePtr<SingleTimeCommands> singleTimeCommands = g_renderBackend->GetSingleTimeCommands();

                //            singleTimeCommands->Push([&](RenderQueue& renderQueue) -> RendererResult
                //                {
                //                    // if this happens we have some ordering issue and the element texture is not ready for copy
                //                    AssertDebug(elementTexture->GetGpuImage()->GetResourceState() != RS_UNDEFINED);

                                    renderQueue << InsertBarrier(atlasTexture->GetGpuImage(), RS_COPY_DST);
                                    renderQueue << InsertBarrier(elementTexture->GetGpuImage(), RS_COPY_SRC);

                                    renderQueue << Blit(
                                        elementTexture->GetGpuImage(),
                                        atlasTexture->GetGpuImage(),
                                        Rect<uint32> {
                                            0, 0,
                                            element.dimensions.x, element.dimensions.y
                                        },
                                        Rect<uint32> {
                                            element.offsetCoords.x, element.offsetCoords.y,
                                            element.offsetCoords.x + element.dimensions.x, element.offsetCoords.y + element.dimensions.y
                                        });

                                    renderQueue << InsertBarrier(elementTexture->GetGpuImage(), RS_SHADER_RESOURCE);
                                    renderQueue << InsertBarrier(atlasTexture->GetGpuImage(), RS_SHADER_RESOURCE);

                                   /* return {};
                                });

                            Assert(singleTimeCommands->Execute());

                            SafeDelete(std::move(atlasTexture));
                            SafeDelete(std::move(elementTexture));
                    }).Detach();*/
            }
        }

#if 0
        // DEBUGGING: Save each atlas texture to a file for debugging purposes
        currentFrame->OnFrameEnd
            .Bind([atlasTextures = atlasTextures](FrameBase* frame)
                                    {
                                        HYP_LOG(Lightmap, Debug, "Saving atlas textures to disk for debugging");

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

                                                    LightmapAtlasBitmap bitmap(atlasTexture->GetExtent().x, atlasTexture->GetExtent().y);
                                                    bitmap.SetPixels(std::move(byteBuffer));

                                                    const String filename = HYP_FORMAT("lightmap_atlas_texture_{}_{}.bmp",
                                                        atlasTexture->GetName(),
                                                        uint32(LightmapTextureType(textureTypeIndex)));

                                                    HYP_LOG(Lightmap, Debug, "Writing atlas texture {} to file {}", atlasTexture->GetName(), filename);

                                                    FileByteWriter fileByteWriter { filename };
                                                    bool res = bitmap.Write(&fileByteWriter);

                                                    if (!res)
                                                    {
                                                        HYP_LOG(Lightmap, Error, "Failed to write atlas texture {} to file", atlasTexture->GetName());
                                                    }
                                                });
                                        }
                                    })
            .Detach();
#endif

        return {};
    }
};

#pragma endregion Render commands

LightmapVolume::LightmapVolume()
    : LightmapVolume(BoundingBox::Empty())
{
}

LightmapVolume::LightmapVolume(const BoundingBox& aabb)
    : Entity()
{
    m_entityAabb = aabb;

    m_atlases.Reserve(s_maxAtlases);
    m_atlases.EmplaceBack(Vec2u(4096, 4096));

    m_radianceAtlasTextures.PushBack(Handle<Texture>::Null());
    m_irradianceAtlasTextures.PushBack(Handle<Texture>::Null());
}

LightmapVolume::~LightmapVolume()
{
    SafeDelete(std::move(m_radianceAtlasTextures));
    SafeDelete(std::move(m_irradianceAtlasTextures));
}

bool LightmapVolume::AddElement(const LightmapUVMap& uvMap, LightmapElement& outElement, bool shrinkToFit, float downscaleLimit)
{
    Threads::AssertOnThread(g_gameThread);

    outElement.id = ~0u;

    const Vec2u elementDimensions = { uvMap.width, uvMap.height };

    LinkedList<LightmapVolumeAtlas> tmpAtlas;

    for (uint32 atlasIndex = 0; atlasIndex < s_maxAtlases; atlasIndex++)
    {
        LightmapVolumeAtlas* pAtlas = nullptr;
        bool isNewAtlas = false;

        if (atlasIndex >= m_atlases.Size())
        {
            pAtlas = &tmpAtlas.EmplaceBack(Vec2u(4096, 4096));
            isNewAtlas = true;
        }
        else
        {
            pAtlas = &m_atlases[atlasIndex];
        }

        uint32 elementIndex = ~0u;

        if (pAtlas->AddElement(elementDimensions, outElement, elementIndex, shrinkToFit, downscaleLimit))
        {
            AssertDebug(elementIndex < UINT16_MAX);

            outElement.id = uint32((atlasIndex << 16) | elementIndex);

            if (isNewAtlas)
            {
                m_atlases.Resize(MathUtil::Max(m_atlases.Size(), atlasIndex + 1));
                m_radianceAtlasTextures.Resize(m_atlases.Size());
                m_irradianceAtlasTextures.Resize(m_atlases.Size());

                m_atlases[atlasIndex] = std::move(*pAtlas);
            }

            return true;
        }
    }

    // could not add to any atlas
    return false;
}

const LightmapElement* LightmapVolume::GetElement(LightmapElement::Id elementId) const
{
    Threads::AssertOnThread(g_gameThread);

    uint16 atlasIndex;
    uint16 elementIndex;
    LightmapElement::GetAtlasAndElementIndex(elementId, atlasIndex, elementIndex);

    if (atlasIndex >= m_atlases.Size())
    {
        return nullptr;
    }

    if (elementIndex >= m_atlases[atlasIndex].elements.Size())
    {
        return nullptr;
    }

    return &m_atlases[atlasIndex].elements[elementIndex];
}

bool LightmapVolume::BuildElementTextures(const LightmapUVMap& uvMap, LightmapElement::Id elementId)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread);

    AssertReady();

    uint16 atlasIndex;
    uint16 elementIndex;
    LightmapElement::GetAtlasAndElementIndex(elementId, atlasIndex, elementIndex);

    if (atlasIndex >= m_atlases.Size())
    {
        return false;
    }

    if (elementIndex >= m_atlases[atlasIndex].elements.Size())
    {
        return false;
    }

    LightmapElement& element = m_atlases[atlasIndex].elements[elementIndex];

    const Vec2u elementDimensions = element.dimensions;

    FixedArray<LightmapAtlasBitmap, uint32(LTT_MAX)> bitmaps = {
        uvMap.ToBitmapRadiance(),  /* RADIANCE */
        uvMap.ToBitmapIrradiance() /* IRRADIANCE */
    };

    FixedArray<Handle<Texture>, LTT_MAX> elementTextures;

    static constexpr const char* TextureTypeNames[uint32(LTT_MAX)] = { "R", "I" };

    for (uint32 i = 0; i < uint32(LTT_MAX); i++)
    {
        LinkedList<LightmapAtlasBitmap> tempBitmap;

        LightmapAtlasBitmap* pBitmap = &bitmaps[i];

        if (elementDimensions.x != bitmaps[i].GetWidth() || elementDimensions.y != bitmaps[i].GetHeight())
        {
            LightmapAtlasBitmap& rescaledBitmap = tempBitmap.EmplaceBack(elementDimensions.x, elementDimensions.y);

            Rect<uint32> srcRect {
                0, 0,
                pBitmap->GetWidth(),
                pBitmap->GetHeight()
            };

            Rect<uint32> dstRect {
                0, 0,
                elementDimensions.x,
                elementDimensions.y
            };

            rescaledBitmap.Blit(*pBitmap, srcRect, dstRect);

            pBitmap = &rescaledBitmap;
        }

        Handle<Texture>& texture = elementTextures[i];

        texture = CreateObject<Texture>(
            TextureDesc {
                TT_TEX2D,
                pBitmap->GetFormat(),
                Vec3u { elementDimensions, 1 },
                TFM_LINEAR,
                TFM_LINEAR,
                TWM_CLAMP_TO_EDGE },
            TextureData { ByteBuffer(pBitmap->ToByteView()) });

        Assert(pBitmap->GetByteSize() == texture->GetTextureDesc().GetByteSize());

        texture->SetName(NAME_FMT("LightmapVolumeTexture_{}_{}_{}", m_uuid, elementIndex, TextureTypeNames[i]));
        InitObject(texture);
    }

    UpdateAtlasTextures(atlasIndex, { { elementId, std::move(elementTextures) } });

    return true;
}

void LightmapVolume::Init()
{
    Entity::Init();

    SetReady(true);
}

void LightmapVolume::UpdateRenderProxy(RenderProxyLightmapVolume* proxy)
{
    proxy->lightmapVolume = WeakHandleFromThis();

    proxy->bufferData.aabbMax = Vec4f(m_entityAabb.max, 1.0f);
    proxy->bufferData.aabbMin = Vec4f(m_entityAabb.min, 1.0f);
    proxy->bufferData.textureIndex = ~0u; // @TODO: Set the correct texture index based on the element
}

void LightmapVolume::UpdateAtlasTextures(
    uint16 atlasIndex,
    HashMap<LightmapElement::Id, FixedArray<Handle<Texture>, LTT_MAX>>&& elementTextures)
{
    HYP_SCOPE;
    AssertReady();

    HYP_LOG(Lightmap, Debug, "Updating atlas textures for LightmapVolume {}", m_uuid);

    for (auto& it : elementTextures)
    {
        for (uint32 i = 0; i < uint32(LTT_MAX); i++)
        {
            AssertDebug(it.second[i] != nullptr, "Element texture for type {} is null!", uint32(LightmapTextureType(i)));

            InitObject(it.second[i]);
        }
    }

    Assert(atlasIndex < m_atlases.Size());

    LightmapVolumeAtlas& atlas = m_atlases[atlasIndex];
    
    // Calculate the size of the atlas texture in bytes
    constexpr TextureFormat AtlasTextureFormat = TF_RGBA16F;
    constexpr uint32 BytesPerPixel = BytesPerComponent(AtlasTextureFormat) * NumComponents(AtlasTextureFormat);

    const SizeType atlasWidth = atlas.atlasDimensions.x;
    const SizeType atlasHeight = atlas.atlasDimensions.y;
    const SizeType atlasDataSize = atlasWidth * atlasHeight * BytesPerPixel;
        
    Handle<Texture>& radianceTexture = m_radianceAtlasTextures[atlasIndex];
    if (!radianceTexture)
    {
        radianceTexture = CreateObject<Texture>(
            TextureDesc {
                TT_TEX2D,
                AtlasTextureFormat,
                Vec3u { atlas.atlasDimensions, 1 },
                TFM_LINEAR,
                TFM_LINEAR,
                TWM_CLAMP_TO_EDGE });

        radianceTexture->SetName(NAME_FMT("LightmapVolumeAtlasTexture_{}_R", m_uuid));

        if (Result result = g_assetManager->GetAssetRegistry()->RegisterAsset("$Import/Media/Lightmaps", radianceTexture->GetAsset()).Await(); result.HasError())
        {
            HYP_LOG(Lightmap, Error, "Failed to register atlas texture '{}' with asset registry: {}", radianceTexture->GetName(), result.GetError().GetMessage());
        }

        InitObject(radianceTexture);
    }

    Handle<Texture>& irradianceTexture = m_irradianceAtlasTextures[atlasIndex];
    if (!irradianceTexture)
    {
        // fill with some test color for debugging
        ByteBuffer atlasData;
        atlasData.SetSize(atlasDataSize);

        for (SizeType y = 0; y < atlasHeight; y++)
        {
            for (SizeType x = 0; x < atlasWidth; x++)
            {
                const SizeType pixelIndex = (y * atlasWidth + x) * BytesPerPixel;
                Float16* pixelPtr = reinterpret_cast<Float16*>(atlasData.Data() + pixelIndex);
                // test pattern
                pixelPtr[0] = Float16(0.0f); // R
                pixelPtr[1] = Float16(0.0f); // G
                pixelPtr[2] = Float16(1.0f); // B
                pixelPtr[3] = Float16(1.0f); // A
            }
        }

        irradianceTexture = CreateObject<Texture>(
            TextureDesc {
                TT_TEX2D,
                AtlasTextureFormat,
                Vec3u { atlas.atlasDimensions, 1 },
                TFM_LINEAR,
                TFM_LINEAR,
                TWM_CLAMP_TO_EDGE },
            TextureData { std::move(atlasData) });

        irradianceTexture->SetName(NAME_FMT("LightmapVolumeAtlasTexture_{}_I", m_uuid));

        /*if (Result result = g_assetManager->GetAssetRegistry()->RegisterAsset("$Import/Media/Lightmaps", irradianceTexture->GetAsset()).Await(); result.HasError())
        {
            HYP_LOG(Lightmap, Error, "Failed to register atlas texture '{}' with asset registry: {}", irradianceTexture->GetName(), result.GetError().GetMessage());
        }*/

        InitObject(irradianceTexture);
    }

    /*Array<FixedArray<Handle<Texture>, LTT_MAX>> elementTextures;
    elementTextures.Resize(atlas.elements.Size());

    for (SizeType elementIndex = 0; elementIndex < atlas.elements.Size(); elementIndex++)
    {
        LightmapElement& element = atlas.elements[elementIndex];
        FixedArray<Handle<Texture>, LTT_MAX>& currentElementTextures = elementTextures[elementIndex];

        for (SizeType entryIndex = 0; entryIndex < element.entries.Size(); entryIndex++)
        {
            LightmapElementTextureEntry& entry = element.entries[entryIndex];
            AssertDebug(entry.type < LTT_MAX);

            currentElementTextures[entry.type] = entry.texture;
        }
    }*/

    Array<Handle<Texture>> atlasTextures;
    atlasTextures.Resize(uint32(LTT_MAX));
    atlasTextures[LTT_IRRADIANCE] = irradianceTexture;
    atlasTextures[LTT_RADIANCE] = radianceTexture;

    PUSH_RENDER_COMMAND(
        BakeLightmapAtlasTexture,
        WeakHandleFromThis(),
        atlas.elements,
        std::move(atlasTextures),
        std::move(elementTextures));
}

} // namespace hyperion
