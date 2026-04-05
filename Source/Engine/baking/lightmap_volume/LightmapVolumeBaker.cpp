/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

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
#include <rendering/CommandRecorder.hpp>

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

static constexpr TextureFormat AtlasTextureFormats[LightmapVolume::NumAtlasTextureTypes] = {
    TextureFormat::RGBA8,//TextureFormat::R11G11B10F, // Radiance
    TextureFormat::R11G11B10F  // Irradiance
};

static constexpr const char* LightmapAtlasTextureTypeNames[LightmapVolume::NumAtlasTextureTypes] = { "R", "I" };

#pragma region Render commands

struct BlitAtlasElements : RenderCommand
{
    Array<LightmapElement> lightmapElements;
    Array<Handle<Texture>> atlasTextures;
    HashMap<LightmapElementId, FixedArray<Handle<Texture>, LightmapVolume::NumAtlasTextureTypes>> elementTextures;

    BlitAtlasElements(
        const Array<LightmapElement>& lightmapElements,
        Array<Handle<Texture>>&& atlasTextures,
        HashMap<LightmapElementId, FixedArray<Handle<Texture>, LightmapVolume::NumAtlasTextureTypes>>&& elementTextures)
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
        Assert(atlasTextures.Size() == LightmapVolume::NumAtlasTextureTypes);

        Frame* currentFrame = g_renderInterface->GetCurrentFrame();
        Assert(currentFrame != nullptr);

        CommandRecorder& cr = currentFrame->postRenderCommands;

        for (uint32 textureTypeIndex = 0; textureTypeIndex < LightmapVolume::NumAtlasTextureTypes; textureTypeIndex++)
        {
            if (!atlasTextures[textureTypeIndex])
            {
                AssertDebug(false, "No atlas texture for lightmap texture type {}", LightmapVolume::AtlasTextureType(textureTypeIndex));

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

                cr << InsertBarrier(atlasTexture->GetGpuImage(), RS_COPY_DST);
                cr << InsertBarrier(elementTexture->GetGpuImage(), RS_COPY_SRC);

                cr << Blit(
                    elementTexture->GetGpuImage(),
                    atlasTexture->GetGpuImage(),
                    Rect<uint32> {
                        0, 0,
                        element.dimensions.x, element.dimensions.y },
                    Rect<uint32> {
                        element.offsetCoords.x, element.offsetCoords.y,
                        element.offsetCoords.x + element.dimensions.x, element.offsetCoords.y + element.dimensions.y });

                cr << InsertBarrier(elementTexture->GetGpuImage(), RS_SHADER_RESOURCE);
                cr << InsertBarrier(atlasTexture->GetGpuImage(), RS_SHADER_RESOURCE);
            }
        }

        // Add readback to update TextureData for the lightmap atlas texture
        currentFrame->OnFrameEnd
            .Bind([atlasTextures = atlasTextures](Frame* frame)
                {
                    for (uint32 textureTypeIndex = 0; textureTypeIndex < LightmapVolume::NumAtlasTextureTypes; textureTypeIndex++)
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

static Name GenerateElementTextureName(LightmapVolume* lmv, uint32 elementIndex, LightmapVolume::AtlasTextureType textureType)
{
    return NAME_FMT("LightmapVolumeTexture_{}_{}_{}", lmv->GetName(), elementIndex, LightmapAtlasTextureTypeNames[textureType]);
}

static void UpdateAtlasTextures(
    LightmapVolume* lmv,
    uint16 atlasIndex,
    HashMap<LightmapElementId, FixedArray<Handle<Texture>, LightmapVolume::NumAtlasTextureTypes>>&& elementTextures)
{
    HYP_LOG(Lightmap, Verbose, "Updating atlas textures for LightmapVolume {}", lmv->Id());

    for (auto& it : elementTextures)
    {
        for (uint32 i = 0; i < LightmapVolume::NumAtlasTextureTypes; i++)
        {
            AssertDebug(it.second[i] != nullptr, "Element texture for type {} is null!", LightmapVolume::AtlasTextureType(i));

            CheckResult(it.second[i]->Create());
        }
    }

    Assert(atlasIndex < lmv->GetAtlases().Size());

    LightmapVolumeAtlas& atlas = lmv->GetAtlases()[atlasIndex];

    Handle<Texture> radianceTexture = lmv->GetAtlasTexture(atlasIndex, LightmapVolume::AtlasTextureType::RadianceTexture);
    if (!radianceTexture)
    {
        radianceTexture = MakeHandle<Texture>(
            TextureDesc {
                TextureType::Texture2D,
                AtlasTextureFormats[LightmapVolume::AtlasTextureType::RadianceTexture],
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

        CheckResult(radianceTexture->Create());

        lmv->SetAtlasTexture(atlasIndex, LightmapVolume::AtlasTextureType::RadianceTexture, radianceTexture);
    }

    Handle<Texture> irradianceTexture = lmv->GetAtlasTexture(atlasIndex, LightmapVolume::AtlasTextureType::IrradianceTexture);
    if (!irradianceTexture)
    {
        irradianceTexture = MakeHandle<Texture>(
            TextureDesc {
                TextureType::Texture2D,
                AtlasTextureFormats[LightmapVolume::AtlasTextureType::IrradianceTexture],
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

        CheckResult(irradianceTexture->Create());

        lmv->SetAtlasTexture(atlasIndex, LightmapVolume::AtlasTextureType::IrradianceTexture, irradianceTexture);
    }

    lmv->SetNeedsRenderProxyUpdate();

    Array<Handle<Texture>> atlasTextures;
    atlasTextures.Resize(LightmapVolume::NumAtlasTextureTypes);
    atlasTextures[LightmapVolume::AtlasTextureType::IrradianceTexture] = irradianceTexture;
    atlasTextures[LightmapVolume::AtlasTextureType::RadianceTexture] = radianceTexture;

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

    FixedArray<typename Baking::BakeData<LightmapVolume>::BitmapType, LightmapVolume::NumAtlasTextureTypes> bitmaps = {
        bakeData.ToBitmapRadiance(),  /* RADIANCE */
        bakeData.ToBitmapIrradiance() /* IRRADIANCE */
    };

    FixedArray<Handle<Texture>, LightmapVolume::NumAtlasTextureTypes> elementTextures;

    for (uint32 i = 0; i < LightmapVolume::NumAtlasTextureTypes; i++)
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

            BitmapUtils::Blit(*pBitmap, rescaledBitmap, srcRect, dstRect);

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

        texture->SetName(GenerateElementTextureName(lmv, elementIndex, LightmapVolume::AtlasTextureType(i)));
        CheckResult(texture->Create());
    }

    UpdateAtlasTextures(lmv, atlasIndex, { { elementId, std::move(elementTextures) } });

    return true;
}

#pragma endregion LightmapVolume baking helpers

#pragma region Baker<LightmapVolume>

Baker<LightmapVolume>::Baker(BakerConfig&& config, const Handle<LightmapVolume>& volume)
    : BakerBase(std::move(config), volume, MakeStrongRef(volume->GetScene()), volume->GetWorldBounds()),
      m_volume(volume),
      m_lightmapElementId(InvalidLightmapElementId)
{
}

UniquePtr<BakeJobBase> Baker<LightmapVolume>::CreateJob(BakeJobParams&& params)
{
    return MakeUnique<BakeJob<LightmapVolume>>(std::move(params), m_volume, &m_bakeData);
}

void Baker<LightmapVolume>::CreateLightmapRenderers()
{
    m_lightmapRenderers.Clear();

    if (!PerformsRayTracing())
    {
        return;
    }

    const uint32 shadingTypesMask = GetShadingTypesMask();

    for (uint32 i = 0; i < uint32(LightmapShadingType::MAX); i++)
    {
        if (!(shadingTypesMask & (1u << i)))
        {
            continue;
        }

        const uint32 maxTexelsPerFrame = MaxTexelsPerFrame();
        AssertDebug(maxTexelsPerFrame > 0);

        UniquePtr<ILightmapRenderer>& lightmapRenderer = m_lightmapRenderers.EmplaceBack();
        lightmapRenderer = CreateRenderer(LightmapShadingType(i), maxTexelsPerFrame);

        if (!lightmapRenderer)
        {
            continue;
        }

        lightmapRenderer->Create();
    }
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

        if (meshComponent.material->GetBucket() != RenderBucket::Opaque
            && meshComponent.material->GetBucket() != RenderBucket::Lightmapped
            && meshComponent.material->GetBucket() != RenderBucket::Translucent)
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
            boundingBoxComponent.worldAabb
        });
    }

    // Build global data
    m_bakeData = BakeData<LightmapVolume>(m_bakeEntities.ToSpan(), m_volume);

    if (Result result = m_bakeData.Build(); result.HasError())
    {
        HYP_LOG(Lightmap, Error, "Failed to build lightmap data: {}", result.GetError().GetMessage());
        return;
    }

    AssertDebug(m_bakeData.GetWidth() * m_bakeData.GetHeight() > 0);

    m_volume->RemoveAllElements();

    LightmapElement lightmapElement;
    if (!m_volume->AddElement({ m_bakeData.GetWidth(), m_bakeData.GetHeight() }, lightmapElement, /* shrinkToFit */ true, /* downscaleLimit */ 0.1f))
    {
        HYP_LOG(Lightmap, Error, "Failed to add element to volume!");
        return;
    }

    m_lightmapElementId = lightmapElement.id;
    AssertDebug(m_lightmapElementId != InvalidLightmapElementId);

    if (!m_config.onlyGenerateUVs)
    {
        BakerBase::DispatchJobs();
    }
}

void Baker<LightmapVolume>::OnCompleted_Internal()
{
    HYP_SCOPE;

    AssertDebug(m_lightmapElementId != InvalidLightmapElementId);

    m_bakeData.Blur();
    m_bakeData.Dilate();

    if (!BuildElementTextures(m_volume, m_bakeData, m_lightmapElementId))
    {
        HYP_LOG(Lightmap, Error, "Failed to build LightmapElement textures for LightmapVolume, element id: {}", m_lightmapElementId);
        return;
    }

    const LightmapElement* lightmapElement = m_volume->GetElement(m_lightmapElementId);
    Assert(lightmapElement != nullptr);

    HYP_LOG(Lightmap, Verbose, "Lightmap baking complete! Building element with id {}, UV offset: {}, Scale: {}", m_lightmapElementId,
        lightmapElement->offsetUV, lightmapElement->scale);

    // Update meshes
    for (size_t bakeEntityIndex = 0; bakeEntityIndex < m_bakeEntities.Size(); bakeEntityIndex++)
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
            newMeshDesc.meshAttributes.inputLayout = { VT_Simple | VT_UV1 };
            newMeshDesc.numVertices = uint32(bakeMesh.vertices.Size());
            newMeshDesc.numIndices = uint32(bakeMesh.indices.Size());

            for (size_t i = 0; i < bakeMesh.vertices.Size(); i++)
            {
                BakeVertex& inVertex = bakeMesh.vertices[i];

                Vec2f uv1 = inVertex.GetUV1();
                //uv1.y = 1.0f - uv1.y; // Invert Y coordinate for lightmaps
                uv1 *= lightmapElement->scale;
                uv1 += Vec2f(lightmapElement->offsetUV.x, lightmapElement->offsetUV.y);
            }

            VertexArrayView vertexArrayView {};
            vertexArrayView.floatData = reinterpret_cast<const float*>(bakeMesh.vertices.Data());
            vertexArrayView.layoutDesc = newMeshDesc.meshAttributes.inputLayout;
            vertexArrayView.vertexCount = bakeMesh.vertices.Size();

            mesh->SetMeshData(newMeshDesc, vertexArrayView, bakeMesh.indices.ToByteView());
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
        
        if (Result result = bakeEntity.material->Register("$Import/Media/Materials"); result.HasError())
        {
            HYP_LOG(Lightmap, Error, "Failed to register material: {}", result.GetError().GetMessage());
        }

        isNewMaterial = true;

        bakeEntity.material->SetBucket(RenderBucket::Lightmapped);

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
