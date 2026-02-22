/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <baking/lightmap_volume/LightmapVolumeBaker.hpp>
#include <baking/lightmap_volume/LightmapVolumeBakeJob.hpp>

#include <asset/Assets.hpp>
#include <asset/AssetRegistry.hpp>

#include <rendering/Mesh.hpp>
#include <rendering/Material.hpp>
#include <rendering/Texture.hpp>
#include <rendering/RenderCommand.hpp>
#include <rendering/RenderInterface.hpp>
#include <rendering/Frame.hpp>
#include <rendering/RenderQueue.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <scene/EntityManager.hpp>
#include <scene/LightmapVolume.hpp>

#include <scene/components/MeshComponent.hpp>
#include <scene/components/TransformComponent.hpp>
#include <scene/components/BoundingBoxComponent.hpp>
#include <scene/components/LightmapElementComponent.hpp>

#include <Core/threading/TaskThread.hpp>

#include <engine/EngineGlobals.hpp>

namespace Hyperion {
namespace Baking {

static constexpr LightmapElementId InvalidLightmapElementId = LightmapElementId(~0u);

static constexpr TextureFormat AtlasTextureFormats[LTT_MAX] = {
    TextureFormat::R11G11B10F, // Radiance
    TextureFormat::R11G11B10F  // Irradiance
};

#pragma region Render commands

struct BlitAtlasElements : RenderCommand
{
    Array<LightmapElement> lightmapElements;
    Array<Handle<Texture>> atlasTextures;
    HashMap<LightmapElementId, FixedArray<Handle<Texture>, LTT_MAX>> elementTextures;

    BlitAtlasElements(
        const Array<LightmapElement>& lightmapElements,
        Array<Handle<Texture>>&& atlasTextures,
        HashMap<LightmapElementId, FixedArray<Handle<Texture>, LTT_MAX>>&& elementTextures)
        : lightmapElements(lightmapElements),
          atlasTextures(std::move(atlasTextures)),
          elementTextures(std::move(elementTextures))
    {
    }

    virtual ~BlitAtlasElements() override
    {
        for (auto& it : elementTextures)
        {
            EnqueueDeletion(std::move(it.second));
        }

        EnqueueDeletion(std::move(atlasTextures));
    }

    virtual RendererResult operator()() override
    {
        AssertDebug(!elementTextures.Empty());

        // Ensure the array of atlas textures are resized to the correct count
        Assert(atlasTextures.Size() == uint32(LTT_MAX));

        Frame* currentFrame = g_renderInterface->GetCurrentFrame();
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

                /// \todo : Add assertion that atlasIndex == our current atlas index

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
            .Bind([atlasTextures = atlasTextures](Frame* frame)
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
                                Handle<Texture> atlasTexture = atlasTextureWeak.Lock();
                                if (!atlasTexture)
                                {
                                    return;
                                }

                                auto writeScope = atlasTexture->GetWriteScope();

                                TextureDesc textureDesc = atlasTexture->GetTextureDesc();
                                textureDesc.mipOffsets = { 0 };

                                atlasTexture->SetImageData(byteBuffer.ToByteView());
                            });
                    }
                })
            .Detach();

        return {};
    }
};

#pragma endregion Render commands

#pragma region LightmapVolume baking helpers

static Name GenerateElementTextureName(LightmapVolume* lmv, uint32 elementIndex, LightmapTextureType textureType)
{
    static constexpr const char* TextureTypeNames[uint32(LTT_MAX)] = { "R", "I" };
    return NAME_FMT("LightmapVolumeTexture_{}_{}_{}", lmv->GetName(), elementIndex, TextureTypeNames[uint32(textureType)]);
}

static void UpdateAtlasTextures(
    LightmapVolume* lmv,
    uint16 atlasIndex,
    HashMap<LightmapElementId, FixedArray<Handle<Texture>, LTT_MAX>>&& elementTextures)
{
    HYP_LOG(Lightmap, Debug, "Updating atlas textures for LightmapVolume {}", lmv->Id());

    for (auto& it : elementTextures)
    {
        for (uint32 i = 0; i < uint32(LTT_MAX); i++)
        {
            AssertDebug(it.second[i] != nullptr, "Element texture for type {} is null!", uint32(LightmapTextureType(i)));

            InitObject(it.second[i]);
        }
    }

    Assert(atlasIndex < lmv->GetAtlases().Size());

    LightmapVolumeAtlas& atlas = lmv->GetAtlases()[atlasIndex];

    Handle<Texture> radianceTexture = lmv->GetAtlasTexture(atlasIndex, LightmapTextureType::LTT_RADIANCE);
    if (!radianceTexture)
    {
        radianceTexture = MakeHandle<Texture>(
            TextureDesc {
                TextureType::Texture2D,
                AtlasTextureFormats[LTT_RADIANCE],
                Vec3u { atlas.atlasDimensions, 1 },
                TFM_LINEAR,
                TFM_LINEAR,
                TWM_CLAMP_TO_EDGE
            });

        radianceTexture->SetName(NAME_FMT("LightmapVolumeAtlasTexture_{}_R", lmv->GetName()));

        if (Result result = g_assetManager->GetAssetRegistry()->RegisterAsset("$Import/Media/Lightmaps", radianceTexture, AddAssetConflictMode::ReplaceExisting); result.HasError())
        {
            HYP_LOG(Lightmap, Error, "Failed to register atlas texture '{}' with asset registry: {}", radianceTexture->GetName(), result.GetError().GetMessage());
        }

        InitObject(radianceTexture);

        lmv->SetAtlasTexture(atlasIndex, LightmapTextureType::LTT_RADIANCE, radianceTexture);
    }

    Handle<Texture> irradianceTexture = lmv->GetAtlasTexture(atlasIndex, LightmapTextureType::LTT_IRRADIANCE);
    if (!irradianceTexture)
    {
        irradianceTexture = MakeHandle<Texture>(
            TextureDesc {
                TextureType::Texture2D,
                AtlasTextureFormats[LTT_IRRADIANCE],
                Vec3u { atlas.atlasDimensions, 1 },
                TFM_LINEAR,
                TFM_LINEAR,
                TWM_CLAMP_TO_EDGE
            });

        irradianceTexture->SetName(NAME_FMT("LightmapVolumeAtlasTexture_{}_I", lmv->GetName()));

        if (Result result = g_assetManager->GetAssetRegistry()->RegisterAsset("$Import/Media/Lightmaps", irradianceTexture, AddAssetConflictMode::ReplaceExisting); result.HasError())
        {
            HYP_LOG(Lightmap, Error, "Failed to register atlas texture '{}' with asset registry: {}", irradianceTexture->GetName(), result.GetError().GetMessage());
        }

        InitObject(irradianceTexture);

        lmv->SetAtlasTexture(atlasIndex, LightmapTextureType::LTT_IRRADIANCE, irradianceTexture);
    }

    lmv->SetNeedsRenderProxyUpdate();

    Array<Handle<Texture>> atlasTextures;
    atlasTextures.Resize(uint32(LTT_MAX));
    atlasTextures[LTT_IRRADIANCE] = irradianceTexture;
    atlasTextures[LTT_RADIANCE] = radianceTexture;

    PUSH_RENDER_COMMAND(
        BlitAtlasElements,
        atlas.elements,
        std::move(atlasTextures),
        std::move(elementTextures));
}

static bool BuildElementTextures(
    LightmapVolume* lmv,
    const BakeData<LightmapVolume>& bakeData,
    LightmapElementId elementId)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    uint16 atlasIndex;
    uint16 elementIndex;
    LightmapElement::GetAtlasAndElementIndex(elementId, atlasIndex, elementIndex);

    if (atlasIndex >= lmv->GetAtlases().Size())
    {
        return false;
    }

    if (elementIndex >= lmv->GetAtlases()[atlasIndex].elements.Size())
    {
        return false;
    }

    LightmapElement& element = lmv->GetAtlases()[atlasIndex].elements[elementIndex];

    const Vec2u elementDimensions = element.dimensions;

    FixedArray<typename Baking::BakeData<LightmapVolume>::BitmapType, uint32(LTT_MAX)> bitmaps = {
        bakeData.ToBitmapRadiance(),  /* RADIANCE */
        bakeData.ToBitmapIrradiance() /* IRRADIANCE */
    };

    FixedArray<Handle<Texture>, LTT_MAX> elementTextures;

    static constexpr const char* TextureTypeNames[uint32(LTT_MAX)] = { "R", "I" };

    for (uint32 i = 0; i < uint32(LTT_MAX); i++)
    {
        Optional<typename Baking::BakeData<LightmapVolume>::BitmapType> tempBitmap;

        typename Baking::BakeData<LightmapVolume>::BitmapType* pBitmap = &bitmaps[i];

        if (elementDimensions.x != bitmaps[i].GetWidth() || elementDimensions.y != bitmaps[i].GetHeight())
        {
            typename Baking::BakeData<LightmapVolume>::BitmapType& rescaledBitmap = tempBitmap.Emplace(elementDimensions.x, elementDimensions.y);

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

        texture = MakeHandle<Texture>(
            TextureDesc {
                TextureType::Texture2D,
                pBitmap->GetFormat(),
                Vec3u { elementDimensions, 1 },
                TFM_LINEAR,
                TFM_LINEAR,
                TWM_CLAMP_TO_EDGE
            },
            pBitmap->ToByteView());

        Assert(pBitmap->GetByteSize() == texture->GetTextureDesc().GetByteSize(),
            "Bitmap byte size {} does not match texture byte size {}",
            pBitmap->GetByteSize(),
            texture->GetTextureDesc().GetByteSize());

        texture->SetName(GenerateElementTextureName(lmv, elementIndex, LightmapTextureType(i)));
        InitObject(texture);
    }

    UpdateAtlasTextures(lmv, atlasIndex, { { elementId, std::move(elementTextures) } });

    return true;
}

#pragma endregion LightmapVolume baking helpers

#pragma region Baker<LightmapVolume>

Baker<LightmapVolume>::Baker(LightmapperConfig&& config, const Handle<LightmapVolume>& volume)
    : BakerBase(std::move(config), volume, MakeStrongRef(volume->GetScene()), volume->GetWorldBounds()),
      m_volume(volume),
      m_lightmapElementId(InvalidLightmapElementId)
{
}

UniquePtr<BakeJobBase> Baker<LightmapVolume>::CreateJob(BakeJobParams&& params)
{
    return MakeUnique<BakeJob<LightmapVolume>>(std::move(params), m_volume, &m_bakeData);
}

void Baker<LightmapVolume>::Initialize_Internal()
{
    // no-op
}

void Baker<LightmapVolume>::Build()
{
    HYP_SCOPE;

    EntityManager& mgr = *m_scene->GetEntityManager();

    m_bakeEntities.Clear();
    m_bakeEntitiesByEntity.Clear();

    const bool onlyOverlappingElements = BakerBase::OnlyOverlappingElements();

    for (auto [entity, meshComponent, transformComponent, boundingBoxComponent, _] : mgr.GetEntitySet<MeshComponent, TransformComponent, BoundingBoxComponent, TagComponent<EntityTag::MobStatic>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
    {
        if (entity->InstanceClass() != Entity::StaticClass())
        {
            continue;
        }

        if (!meshComponent.mesh || !meshComponent.material)
        {
            continue;
        }

        if (meshComponent.material->GetBucket() != RB_OPAQUE
            && meshComponent.material->GetBucket() != RB_LIGHTMAP
            && meshComponent.material->GetBucket() != RB_TRANSLUCENT)
        {
            continue;
        }

        const BoundingBox& worldAabb = boundingBoxComponent.worldAabb;

        if (!onlyOverlappingElements && !m_aabb.Overlaps(worldAabb))
        {
            continue;
        }

        m_bakeEntities.PushBack(BakeEntity {
            MakeStrongRef(entity),
            meshComponent.mesh,
            meshComponent.material,
            Transform(transformComponent.translation, transformComponent.scale, transformComponent.rotation).GetMatrix(),
            boundingBoxComponent.worldAabb });
    }

    // Build global data
    m_bakeData = BakeData<LightmapVolume>(m_bakeEntities.ToSpan(), m_volume);

    if (Result result = m_bakeData.Build(); result.HasError())
    {
        HYP_LOG(Lightmap, Error, "Failed to build lightmap data: {}", result.GetError().GetMessage());
        return;
    }

    LightmapElement lightmapElement;
    if (!m_volume->AddElement({ m_bakeData.GetWidth(), m_bakeData.GetHeight() }, lightmapElement, /* shrinkToFit */ true, /* downscaleLimit */ 0.1f))
    {
        HYP_LOG(Lightmap, Error, "Failed to add element to volume!");
        return;
    }

    m_lightmapElementId = lightmapElement.id;
    AssertDebug(m_lightmapElementId != InvalidLightmapElementId);

    BakerBase::DispatchJobs();
}

void Baker<LightmapVolume>::OnCompleted_Internal()
{
    HYP_SCOPE;

    AssertDebug(m_lightmapElementId != InvalidLightmapElementId);

    if (!BuildElementTextures(m_volume, m_bakeData, m_lightmapElementId))
    {
        HYP_LOG(Lightmap, Error, "Failed to build LightmapElement textures for LightmapVolume, element id: {}", m_lightmapElementId);
        return;
    }

    const LightmapElement* lightmapElement = m_volume->GetElement(m_lightmapElementId);
    Assert(lightmapElement != nullptr);

    HYP_LOG(Lightmap, Debug, "Lightmap baking complete! Building element with id {}, UV offset: {}, Scale: {}", m_lightmapElementId,
        lightmapElement->offsetUv, lightmapElement->scale);

    // Update meshes
    for (SizeType bakeEntityIndex = 0; bakeEntityIndex < m_bakeEntities.Size(); bakeEntityIndex++)
    {
        BakeEntity& bakeEntity = m_bakeEntities[bakeEntityIndex];

        auto UpdateMeshData = [&]()
        {
            const Handle<Mesh>& mesh = bakeEntity.mesh;
            Assert(mesh.IsValid());

            Assert(bakeEntityIndex < m_bakeData.GetMeshData().Size());

            BakeMesh& bakeMesh = m_bakeData.GetMeshData()[bakeEntityIndex];
            Assert(bakeMesh.mesh == mesh);

            MeshDesc newMeshDesc;
            newMeshDesc.meshAttributes = mesh->GetMeshAttributes();
            newMeshDesc.numVertices = uint32(bakeMesh.vertices.Size());
            newMeshDesc.numIndices = uint32(bakeMesh.indices.Size());

            for (SizeType i = 0; i < bakeMesh.vertices.Size(); i++)
            {
                Vertex& vertex = bakeMesh.vertices[i];

                Vec2f& lightmapUv = vertex.texcoord1;
                lightmapUv.y = 1.0f - lightmapUv.y; // Invert Y coordinate for lightmaps
                lightmapUv *= lightmapElement->scale;
                lightmapUv += Vec2f(lightmapElement->offsetUv.x, lightmapElement->offsetUv.y);
            }

            mesh->SetMeshData(newMeshDesc, bakeMesh.vertices.ToSpan(), bakeMesh.indices.ToByteView());
        };

        UpdateMeshData();

        bool isNewMaterial = false;

        if (bakeEntity.material)
        {
            Handle<Material> clonedMaterial = bakeEntity.material->Clone();
            EnqueueDeletion(std::move(bakeEntity.material));

            bakeEntity.material = clonedMaterial;
        }
        else
        {
            bakeEntity.material = MakeHandle<Material>();
        }

        isNewMaterial = true;

        bakeEntity.material->SetBucket(RB_LIGHTMAP);

        auto UpdateMeshComponent = [entityManagerWeak = MakeWeakRef(m_scene->GetEntityManager()),
                                        lightmapElementId = m_lightmapElementId,
                                        volume = m_volume,
                                        bakeEntity = bakeEntity,
                                        newMaterial = (isNewMaterial ? bakeEntity.material : Handle<Material>::empty)]()
        {
            Handle<EntityManager> entityManager = entityManagerWeak.Lock();

            if (!entityManager)
            {
                return;
            }

            const Handle<Entity>& entity = bakeEntity.entity;

            if (entityManager->HasComponent<MeshComponent>(entity))
            {
                MeshComponent& meshComponent = entityManager->GetComponent<MeshComponent>(entity);

                if (newMaterial.IsValid())
                {
                    InitObject(newMaterial);

                    EnqueueDeletion(std::move(meshComponent.material));

                    meshComponent.material = std::move(newMaterial);
                }
            }
            else
            {
                Assert(newMaterial.IsValid());
                InitObject(newMaterial);

                MeshComponent meshComponent {};
                meshComponent.mesh = bakeEntity.mesh;
                meshComponent.material = newMaterial;

                entityManager->AddComponent<MeshComponent>(entity, std::move(meshComponent));
            }

            if (entityManager->HasComponent<LightmapElementComponent>(entity))
            {
                LightmapElementComponent& lightmapElementComponent = entityManager->GetComponent<LightmapElementComponent>(entity);

                lightmapElementComponent.lightmapVolume = volume.ToWeak();
                lightmapElementComponent.lightmapElementId = lightmapElementId;
            }
            else
            {
                LightmapElementComponent lightmapElementComponent;

                lightmapElementComponent.lightmapVolume = volume.ToWeak();
                lightmapElementComponent.lightmapElementId = lightmapElementId;

                entityManager->AddComponent<LightmapElementComponent>(entity, std::move(lightmapElementComponent));
            }

            entity->SetNeedsRenderProxyUpdate();
        };

        if (IsOnThread(m_scene->GetEntityManager()->GetOwnerThreadId()))
        {
            UpdateMeshComponent();
        }
        else
        {
            ThreadBase* thread = GetThreadById(m_scene->GetEntityManager()->GetOwnerThreadId());
            Assert(thread != nullptr);

            thread->GetScheduler().Enqueue(std::move(UpdateMeshComponent), TaskEnqueueFlags::FIRE_AND_FORGET);
        }
    }
}

#pragma endregion Baker<LightmapVolume>

} // namespace Baking
} // namespace Hyperion
