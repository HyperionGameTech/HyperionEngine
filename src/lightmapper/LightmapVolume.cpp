/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <lightmapper/LightmapVolume.hpp>

#include <rendering/Texture.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/RenderQueue.hpp>
#include <rendering/RenderCommand.hpp>
#include <rendering/RenderHelpers.hpp>
#include <rendering/RenderBackend.hpp>
#include <rendering/RenderFrame.hpp>

#include <lightmapper/LightmapData.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <asset/AssetRegistry.hpp>
#include <asset/Assets.hpp>
#include <asset/TextureAsset.hpp>

#include <scene/World.hpp>
#include <scene/Scene.hpp>
#include <scene/EntityManager.hpp>
#include <scene/components/LightmapElementComponent.hpp>

#include <core/io/ByteWriter.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/threading/Threads.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>

#include <LightmapVolume.generated.inl>

namespace hyperion {

constexpr Vec2u DefaultAtlasDimensions = Vec2u(4096, 4096);
constexpr TextureFormat AtlasTextureFormats[LTT_MAX] = {
    TF_R11G11B10F, // Radiance
    TF_R11G11B10F  // Irradiance
};

static Name GenerateElementTextureName(const Uuid& volumeUuid, uint32 elementIndex, LightmapTextureType textureType)
{
    static constexpr const char* TextureTypeNames[uint32(LTT_MAX)] = { "R", "I" };
    return NAME_FMT("LightmapVolumeTexture_{}_{}_{}", volumeUuid, elementIndex, TextureTypeNames[uint32(textureType)]);
}

#pragma region Render commands

struct LightmapVolumeAtlasBlit : RenderCommand
{
    WeakHandle<LightmapVolume> lightmapVolumeWeak;
    Array<LightmapElement> lightmapElements;
    Array<Handle<Texture>> atlasTextures;
    HashMap<LightmapElementId, FixedArray<Handle<Texture>, LTT_MAX>> elementTextures;

    LightmapVolumeAtlasBlit(
        const WeakHandle<LightmapVolume>& lightmapVolumeWeak,
        const Array<LightmapElement>& lightmapElements,
        Array<Handle<Texture>>&& atlasTextures,
        HashMap<LightmapElementId, FixedArray<Handle<Texture>, LTT_MAX>>&& elementTextures)
        : lightmapVolumeWeak(lightmapVolumeWeak),
          lightmapElements(lightmapElements),
          atlasTextures(std::move(atlasTextures)),
          elementTextures(std::move(elementTextures))
    {
    }

    virtual ~LightmapVolumeAtlasBlit() override
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

                renderQueue << InsertBarrier(atlasTexture->GetGpuImage(), RS_COPY_DST);
                renderQueue << InsertBarrier(elementTexture->GetGpuImage(), RS_COPY_SRC);

                renderQueue << Blit(
                    elementTexture->GetGpuImage(),
                    atlasTexture->GetGpuImage(),
                    Rect<uint32> {
                        0, 0,
                        element.dimensions.x, element.dimensions.y },
                    Rect<uint32> {
                        element.offsetCoords.x, element.offsetCoords.y,
                        element.offsetCoords.x + element.dimensions.x, element.offsetCoords.y + element.dimensions.y });

                renderQueue << InsertBarrier(elementTexture->GetGpuImage(), RS_SHADER_RESOURCE);
                renderQueue << InsertBarrier(atlasTexture->GetGpuImage(), RS_SHADER_RESOURCE);
            }
        }

        // Add readback to update TextureData for the lightmap atlas texture
        currentFrame->OnFrameEnd
            .Bind([atlasTextures = atlasTextures](FrameBase* frame)
                {
                    for (uint32 textureTypeIndex = 0; textureTypeIndex < uint32(LTT_MAX); textureTypeIndex++)
                    {
                        if (!atlasTextures[textureTypeIndex])
                        {
                            continue;
                        }

                        const Handle<Texture>& atlasTexture = atlasTextures[textureTypeIndex];
                        Assert(atlasTexture.IsValid() && atlasTexture->IsReady());

                        atlasTexture->EnqueueReadback([atlasTextureWeak = atlasTexture.ToWeak()](ByteBuffer&& byteBuffer)
                            {
                                // update texture data on game thread
                                GetThreadById(g_gameThread)->GetScheduler().Enqueue([atlasTextureWeak, byteBuffer = std::move(byteBuffer)]()
                                    {
                                        Handle<Texture> atlasTexture = atlasTextureWeak.Lock();
                                        if (!atlasTexture)
                                        {
                                            return;
                                        }

                                        Handle<AssetPackage> package;

                                        const Handle<TextureAsset>& prevTextureAsset = atlasTexture->GetAsset();

                                        if (prevTextureAsset)
                                        {
                                            package = prevTextureAsset->GetPackage();

                                            if (package)
                                            {
                                                if (Result removeAssetResult = package->RemoveAssetObject(prevTextureAsset).Await(); removeAssetResult.HasError())
                                                {
                                                    HYP_LOG(Lightmap, Error, "Failed to remove previous texture asset {} from package {}: {}",
                                                        prevTextureAsset->GetName(),
                                                        package->BuildPackagePath(),
                                                        removeAssetResult.GetError().GetMessage());
                                                }
                                            }
                                        }

                                        TextureDesc textureDesc = atlasTexture->GetTextureDesc();
                                        textureDesc.mipOffsets = { 0 };

                                        TextureData textureData { std::move(byteBuffer) };

                                        Handle<TextureAsset> newTextureAsset = CreateObject<TextureAsset>(
                                            atlasTexture->GetName(),
                                            textureDesc,
                                            std::move(textureData));

                                        InitObject(newTextureAsset);

                                        if (package)
                                        {
                                            if (Result result = package->AddAssetObject(newTextureAsset).Await(); result.HasError())
                                            {
                                                HYP_LOG(Lightmap, Error, "Failed to add texture asset '{}' to package {}: {}",
                                                    newTextureAsset->GetName(),
                                                    package->BuildPackagePath(),
                                                    result.GetError().GetMessage());

                                                package.Reset();
                                            }
                                        }

                                        if (!package)
                                        {
                                            if (Result result = g_assetManager->GetAssetRegistry()->RegisterAsset("$Import/Media/Lightmaps", newTextureAsset).Await(); result.HasError())
                                            {
                                                HYP_LOG(Lightmap, Error, "Failed to register atlas texture '{}' with asset registry: {}", newTextureAsset->GetName(), result.GetError().GetMessage());
                                            }
                                        }

                                        atlasTexture->SetAsset(newTextureAsset);
                                    },
                                    TaskEnqueueFlags::FIRE_AND_FORGET);
                            });
                    }
                })
            .Detach();

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

    m_atlases.Reserve(MaxAtlases);
    m_atlases.EmplaceBack(DefaultAtlasDimensions);

    m_radianceAtlasTextures.PushBack(Handle<Texture>::Null());
    m_irradianceAtlasTextures.PushBack(Handle<Texture>::Null());
}

LightmapVolume::~LightmapVolume()
{
    SafeDelete(std::move(m_radianceAtlasTextures));
    SafeDelete(std::move(m_irradianceAtlasTextures));
}

bool LightmapVolume::AddElement(Vec2u dimensions, LightmapElement& outElement, bool shrinkToFit, float downscaleLimit)
{
    HYP_SCOPE;
    AssertOnThread(g_gameThread);

    outElement.id = InvalidLightmapElementId;

    Optional<LightmapVolumeAtlas> tmpAtlas;

    for (uint32 atlasIndex = 0; atlasIndex < MaxAtlases; atlasIndex++)
    {
        LightmapVolumeAtlas* pAtlas = nullptr;
        bool isNewAtlas = false;

        if (atlasIndex >= m_atlases.Size())
        {
            pAtlas = &tmpAtlas.Emplace(DefaultAtlasDimensions);
            isNewAtlas = true;
        }
        else
        {
            pAtlas = &m_atlases[atlasIndex];
        }

        uint32 elementIndex = ~0u;

        if (pAtlas->AddElement(dimensions, outElement, elementIndex, shrinkToFit, downscaleLimit))
        {
            AssertDebug(elementIndex < UINT16_MAX);

            outElement.id = LightmapElementId(uint32((atlasIndex << 16) | elementIndex));

            if (isNewAtlas)
            {
                m_atlases.Resize(MathUtil::Max(m_atlases.Size(), atlasIndex + 1));
                m_radianceAtlasTextures.Resize(m_atlases.Size());
                m_irradianceAtlasTextures.Resize(m_atlases.Size());

                m_atlases[atlasIndex] = std::move(*pAtlas);
            }

            SetNeedsRenderProxyUpdate();

            return true;
        }
    }

    // could not add to any atlas
    return false;
}

const LightmapElement* LightmapVolume::GetElement(LightmapElementId elementId) const
{
    AssertOnThread(g_gameThread);

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

bool LightmapVolume::BuildElementTextures(const LightmapData<LightmapVolume>& lightmapData, LightmapElementId elementId)
{
    HYP_SCOPE;
    AssertOnThread(g_gameThread);

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

    FixedArray<typename LightmapData<LightmapVolume>::BitmapType, uint32(LTT_MAX)> bitmaps = {
        lightmapData.ToBitmapRadiance(),  /* RADIANCE */
        lightmapData.ToBitmapIrradiance() /* IRRADIANCE */
    };

    FixedArray<Handle<Texture>, LTT_MAX> elementTextures;

    static constexpr const char* TextureTypeNames[uint32(LTT_MAX)] = { "R", "I" };

    for (uint32 i = 0; i < uint32(LTT_MAX); i++)
    {
        Optional<typename LightmapData<LightmapVolume>::BitmapType> tempBitmap;

        typename LightmapData<LightmapVolume>::BitmapType* pBitmap = &bitmaps[i];

        if (elementDimensions.x != bitmaps[i].GetWidth() || elementDimensions.y != bitmaps[i].GetHeight())
        {
            typename LightmapData<LightmapVolume>::BitmapType& rescaledBitmap = tempBitmap.Emplace(elementDimensions.x, elementDimensions.y);

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

        Assert(pBitmap->GetByteSize() == texture->GetTextureDesc().GetByteSize(),
            "Bitmap byte size {} does not match texture byte size {}",
            pBitmap->GetByteSize(),
            texture->GetTextureDesc().GetByteSize());

        texture->SetName(GenerateElementTextureName(m_uuid, elementIndex, LightmapTextureType(i)));
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

void LightmapVolume::OnAddedToWorld(World* world)
{
    Entity::OnAddedToWorld(world);

    for (Scene* scene : world->GetScenes())
    {
        for (auto [entity, lightmapElementComponent] : scene->GetEntityManager()->GetEntitySet<LightmapElementComponent>().GetScopedView(DataAccessFlags::ACCESS_RW))
        {
            if (lightmapElementComponent.lightmapVolumeUuid == m_uuid
                && !lightmapElementComponent.lightmapVolume.IsValid())
            {
                lightmapElementComponent.lightmapVolume = MakeWeakRef(this);

                entity->SetNeedsRenderProxyUpdate();
            }
        }
    }
}

void LightmapVolume::OnRemovedFromWorld(World* world)
{
    Entity::OnRemovedFromWorld(world);

    for (Scene* scene : world->GetScenes())
    {
        for (auto [entity, lightmapElementComponent] : scene->GetEntityManager()->GetEntitySet<LightmapElementComponent>().GetScopedView(DataAccessFlags::ACCESS_RW))
        {
            if (lightmapElementComponent.lightmapVolumeUuid == m_uuid
                && lightmapElementComponent.lightmapVolume.IsValid())
            {
                lightmapElementComponent.lightmapVolume.Reset();

                entity->SetNeedsRenderProxyUpdate();
            }
        }
    }
}

void LightmapVolume::UpdateRenderProxy(RenderProxyLightmapVolume* proxy)
{
    proxy->lightmapVolume = WeakHandleFromThis();

    proxy->atlasRadianceTextures.Clear();
    proxy->atlasIrradianceTextures.Resize(m_irradianceAtlasTextures.Size());

    for (uint32 i = 0; i < uint32(m_irradianceAtlasTextures.Size()); i++)
    {
        proxy->atlasIrradianceTextures[i] = m_irradianceAtlasTextures[i].Get();
    }

    proxy->atlasRadianceTextures.Clear();
    proxy->atlasRadianceTextures.Resize(m_radianceAtlasTextures.Size());

    for (uint32 i = 0; i < uint32(m_radianceAtlasTextures.Size()); i++)
    {
        proxy->atlasRadianceTextures[i] = m_radianceAtlasTextures[i].Get();
    }

    proxy->numAtlases = uint32(m_atlases.Size());

    proxy->bufferData.aabbMax = Vec4f(m_entityAabb.max, 1.0f);
    proxy->bufferData.aabbMin = Vec4f(m_entityAabb.min, 1.0f);
    proxy->bufferData.textureIndex = ~0u; // @TODO: Set the correct texture index based on the element
}

void LightmapVolume::UpdateAtlasTextures(
    uint16 atlasIndex,
    HashMap<LightmapElementId, FixedArray<Handle<Texture>, LTT_MAX>>&& elementTextures)
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

    Handle<Texture>& radianceTexture = m_radianceAtlasTextures[atlasIndex];
    if (!radianceTexture)
    {
        radianceTexture = CreateObject<Texture>(
            TextureDesc {
                TT_TEX2D,
                AtlasTextureFormats[LTT_RADIANCE],
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
        irradianceTexture = CreateObject<Texture>(
            TextureDesc {
                TT_TEX2D,
                AtlasTextureFormats[LTT_IRRADIANCE],
                Vec3u { atlas.atlasDimensions, 1 },
                TFM_LINEAR,
                TFM_LINEAR,
                TWM_CLAMP_TO_EDGE });

        irradianceTexture->SetName(NAME_FMT("LightmapVolumeAtlasTexture_{}_I", m_uuid));

        if (Result result = g_assetManager->GetAssetRegistry()->RegisterAsset("$Import/Media/Lightmaps", irradianceTexture->GetAsset()).Await(); result.HasError())
        {
            HYP_LOG(Lightmap, Error, "Failed to register atlas texture '{}' with asset registry: {}", irradianceTexture->GetName(), result.GetError().GetMessage());
        }

        InitObject(irradianceTexture);
    }

    SetNeedsRenderProxyUpdate();

    Array<Handle<Texture>> atlasTextures;
    atlasTextures.Resize(uint32(LTT_MAX));
    atlasTextures[LTT_IRRADIANCE] = irradianceTexture;
    atlasTextures[LTT_RADIANCE] = radianceTexture;

    PUSH_RENDER_COMMAND(
        LightmapVolumeAtlasBlit,
        WeakHandleFromThis(),
        atlas.elements,
        std::move(atlasTextures),
        std::move(elementTextures));
}

} // namespace hyperion
