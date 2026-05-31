/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <baking/lightmap_volume/LightmapVolumeBaker.hpp>
#include <baking/lightmap_volume/LightmapVolumeBakeJob.hpp>

#include <asset/Assets.hpp>
#include <asset/AssetRegistry.hpp>

#include <rendering/Mesh.hpp>
#include <rendering/MaterialDefinition.hpp>
#include <rendering/MaterialInstance.hpp>
#include <rendering/Texture.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <scene/EntityManager.hpp>
#include <scene/LightmapVolume.hpp>

#include <scene/components/MeshComponent.hpp>
#include <scene/components/TransformComponent.hpp>
#include <scene/components/BoundingBoxComponent.hpp>
#include <scene/components/LightmapElementComponent.hpp>

#include <Core/threading/TaskThread.hpp>

#include <Framework/EngineGlobals.hpp>

namespace Hyperion {
namespace Baking {

static constexpr LightmapElementId InvalidLightmapElementId = LightmapElementId(~0u);

static constexpr const char* LightmapAtlasTextureTypeNames[LightmapVolume::NumAtlasTextureTypes] = { "R", "I" };

#pragma region LightmapVolume baking helpers

static Name GenerateElementTextureName(LightmapVolume* lmv, uint32 elementIndex, LightmapVolume::AtlasTextureType textureType)
{
    return NAME_FMT("LightmapVolumeTexture_{}_{}_{}", lmv->GetName(), elementIndex, LightmapAtlasTextureTypeNames[textureType]);
}

static void UpdateAtlasTextures(
    LightmapVolume* lmv,
    uint16 atlasIndex,
    TMap<LightmapElementId, FixedArray<typename Baking::BakeData<LightmapVolume>::BitmapType, LightmapVolume::NumAtlasTextureTypes>>&& elementBitmaps)
{
    HYP_LOG(Lightmap, Verbose, "Updating atlas textures for LightmapVolume {}", lmv->Id());

    Assert(atlasIndex < lmv->GetAtlases().Size());

    LightmapVolumeAtlas& atlas = lmv->GetAtlases()[atlasIndex];

    FixedArray<typename Baking::BakeData<LightmapVolume>::BitmapType, LightmapVolume::NumAtlasTextureTypes> atlasBitmaps = {
        typename Baking::BakeData<LightmapVolume>::BitmapType(atlas.atlasDimensions.x, atlas.atlasDimensions.y),
        typename Baking::BakeData<LightmapVolume>::BitmapType(atlas.atlasDimensions.x, atlas.atlasDimensions.y)
    };

    for (auto& it : elementBitmaps)
    {
        uint16 elAtlasIndex;
        uint16 elementIndex;
        LightmapElement::GetAtlasAndElementIndex(it.first, elAtlasIndex, elementIndex);

        Assert(elementIndex < atlas.elements.Size());
        const LightmapElement& element = atlas.elements[elementIndex];

        for (uint32 textureTypeIndex = 0; textureTypeIndex < LightmapVolume::NumAtlasTextureTypes; textureTypeIndex++)
        {
            const auto& elementBitmap = it.second[textureTypeIndex];

            Assert(element.offsetCoords.x + element.dimensions.x <= atlasBitmaps[textureTypeIndex].GetWidth());
            Assert(element.offsetCoords.y + element.dimensions.y <= atlasBitmaps[textureTypeIndex].GetHeight());

            Rect<uint32> srcRect { 0, 0, elementBitmap.GetWidth(), elementBitmap.GetHeight() };
            Rect<uint32> dstRect {
                element.offsetCoords.x, element.offsetCoords.y,
                element.offsetCoords.x + element.dimensions.x,
                element.offsetCoords.y + element.dimensions.y
            };

            BitmapUtils::Blit(elementBitmap, atlasBitmaps[textureTypeIndex], srcRect, dstRect);
        }
    }

    // Create atlas textures from the blitted bitmaps
    for (uint32 textureTypeIndex = 0; textureTypeIndex < LightmapVolume::NumAtlasTextureTypes; textureTypeIndex++)
    {
        const auto& atlasBitmap = atlasBitmaps[textureTypeIndex];

        Handle<Texture> atlasTexture = MakeHandle<Texture>(
            TextureDesc {
                TextureType::Texture2D,
                atlasBitmap.GetFormat(),
                Vec3u { atlas.atlasDimensions, 1 },
                TFM_LINEAR,
                TFM_LINEAR,
                TWM_CLAMP_TO_EDGE
            },
            atlasBitmap.ToByteView());

        atlasTexture->SetName(NAME_FMT("LightmapVolumeAtlasTexture_{}_{}", lmv->GetName(), LightmapAtlasTextureTypeNames[textureTypeIndex]));

        GetCurrentAssetRegistry()->PutAsset(atlasTexture);

        CheckResult(atlasTexture->Create());

        lmv->SetAtlasTexture(atlasIndex, LightmapVolume::AtlasTextureType(textureTypeIndex), atlasTexture);
    }

    lmv->SetNeedsRenderProxyUpdate();
}

static bool BuildElementTextures(
    LightmapVolume* lmv,
    const BakeData<LightmapVolume>& bakeData,
    LightmapElementId elementId)
{
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

    FixedArray<typename Baking::BakeData<LightmapVolume>::BitmapType, LightmapVolume::NumAtlasTextureTypes> elementBitmaps;

    for (uint32 i = 0; i < LightmapVolume::NumAtlasTextureTypes; i++)
    {
        typename Baking::BakeData<LightmapVolume>::BitmapType* pBitmap = &bitmaps[i];

        if (elementDimensions.x != pBitmap->GetWidth() || elementDimensions.y != pBitmap->GetHeight())
        {
            elementBitmaps[i] = typename Baking::BakeData<LightmapVolume>::BitmapType(elementDimensions.x, elementDimensions.y);

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

            BitmapUtils::Blit(*pBitmap, elementBitmaps[i], srcRect, dstRect);
        }
        else
        {
            elementBitmaps[i] = std::move(bitmaps[i]);
        }
    }

    UpdateAtlasTextures(lmv, atlasIndex, { { elementId, std::move(elementBitmaps) } });

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
            && meshComponent.material->GetBucket() != RenderBucket::Lightmapped)
        {
            continue;
        }

        const BoundingBox& worldAabb = boundingBoxComponent.worldAabb;

        if (onlyOverlappingElements && !m_aabb.Overlaps(worldAabb))
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

    LightmapElement* lightmapElement = nullptr;
    if (!m_volume->AddElement({ m_bakeData.GetWidth(), m_bakeData.GetHeight() }, lightmapElement, /* shrinkToFit */ true, /* downscaleLimit */ 0.1f))
    {
        HYP_LOG(Lightmap, Error, "Failed to add element to volume!");
        return;
    }

    AssertDebug(lightmapElement != nullptr);

    m_lightmapElementId = lightmapElement->id;
    AssertDebug(m_lightmapElementId != InvalidLightmapElementId);

    if (!m_config.onlyGenerateUVs)
    {
        BakerBase::DispatchJobs();
    }
}

void Baker<LightmapVolume>::OnCompleted_Internal()
{
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

    GetCurrentAssetRegistry()->PutAssetsDeep(m_volume);

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

            auto readScope = mesh->GetReadScope();

            Assert(bakeEntityIndex < m_bakeData.GetMeshData().Size());

            BakeMesh& bakeMesh = m_bakeData.GetMeshData()[bakeEntityIndex];
            Assert(bakeMesh.mesh == mesh);

            const VertexInputLayoutDesc prevInputLayout = mesh->GetMeshDesc().meshAttributes.inputLayout;
            VertexInputLayoutDesc newInputLayout { uint8(prevInputLayout.mask | VT_UV1) };

            const size_t vertexStrideFloats = newInputLayout.VertexSize() / sizeof(float);

            MeshDesc newMeshDesc;
            newMeshDesc.meshAttributes = mesh->GetMeshAttributes();
            newMeshDesc.meshAttributes.inputLayout = newInputLayout;
            newMeshDesc.numVertices = uint32(bakeMesh.vertices.Size() / vertexStrideFloats);
            newMeshDesc.numIndices = uint32(bakeMesh.indices.Size());

            size_t uv1Offset = 0;
            uv1Offset += (prevInputLayout.mask & VT_Position) ? (sizeof(TVertexPacket<VT_Position>) / sizeof(float)) : 0;
            uv1Offset += (prevInputLayout.mask & VT_Normal) ? (sizeof(TVertexPacket<VT_Normal>) / sizeof(float)) : 0;
            uv1Offset += (prevInputLayout.mask & VT_UV0) ? (sizeof(TVertexPacket<VT_UV0>) / sizeof(float)) : 0;

            AssertDebug(bakeMesh.vertices.Size() % vertexStrideFloats == 0);

            for (size_t i = 0; i < bakeMesh.vertices.Size(); i += vertexStrideFloats)
            {
                float* vertexDataFloat = bakeMesh.vertices.Data() + i;

                TVertexPacket<VT_UV1>* packet = reinterpret_cast<TVertexPacket<VT_UV1>*>(vertexDataFloat + uv1Offset);

                // Scale UV1 to atlas section
                Vec2f uv1 = packet->GetUV1();
                uv1 *= lightmapElement->scale;
                uv1 += Vec2f(lightmapElement->offsetUV.x, lightmapElement->offsetUV.y);
                packet->SetUV1(uv1);
            }

            VertexArrayView vertexArrayView {};
            vertexArrayView.floatData = reinterpret_cast<const float*>(bakeMesh.vertices.Data());
            vertexArrayView.layoutDesc = newMeshDesc.meshAttributes.inputLayout;
            vertexArrayView.vertexCount = bakeMesh.vertices.Size() / vertexStrideFloats;

            readScope.Reset();

            mesh->SetMeshData(newMeshDesc, vertexArrayView, bakeMesh.indices.ToByteView());

            // needs reupload!
            if (mesh->isUploaded.Load())
            {
                mesh->UploadGpuData();
            }
        };

        UpdateMeshData();

        // Update material to have the Lightmapped bucket (if it does not already)
        AssertDebug(bakeEntity.material.IsValid());

        bool isNewMaterial = false;
#if 1
        // update material info
        if (bakeEntity.material && bakeEntity.material->GetBucket() != RenderBucket::Lightmapped)
        {
            // @TODO Look for material definition that matches what we need rather than creating it right out of the gate.

            const Handle<MaterialDefinition>& currentMaterialDefinition = bakeEntity.material->GetDefinition();

            MaterialAttributes newAttributes = currentMaterialDefinition->GetAttributes();
            newAttributes.bucket = RenderBucket::Lightmapped;

            Handle<MaterialDefinition> newMaterialDefinition = MakeHandle<MaterialDefinition>(
                NAME_FMT("{}_LM", currentMaterialDefinition->GetName()),
                newAttributes,
                currentMaterialDefinition->GetDefaultParameters(),
                currentMaterialDefinition->GetDefaultTextures());

            Assert(newMaterialDefinition != nullptr);

            InitObject(newMaterialDefinition);

            GetCurrentAssetRegistry()->PutAssetsDeep(newMaterialDefinition);

            Handle<MaterialInstance> newMaterialInstance = newMaterialDefinition->CreateInstance();
            Assert(newMaterialInstance != nullptr);

            newMaterialInstance->SetParameters(bakeEntity.material->GetParameters());
            newMaterialInstance->SetTextures(bakeEntity.material->GetTextures());

            EnqueueDeletion(std::move(bakeEntity.material));

            InitObject(newMaterialInstance);

            GetCurrentAssetRegistry()->PutAssetsDeep(newMaterialInstance);

            bakeEntity.material = newMaterialInstance;

            isNewMaterial = true;
        }
#endif

        auto UpdateMeshComponent = [entityManagerWeak = MakeWeakRef(m_scene->GetEntityManager()),
                                        lightmapElementId = m_lightmapElementId,
                                        volume = m_volume,
                                        bakeEntity = bakeEntity,
                                        material = bakeEntity.material,
                                        isNewMaterial]()
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

                if (isNewMaterial)
                {
                    EnqueueDeletion(std::move(meshComponent.material));

                    meshComponent.material = bakeEntity.material;
                    entity->MarkDirty();
                }
            }
            else
            {
                HYP_LOG(Lightmap, Warning, "Entity {} does not have a MeshComponent, cannot assign baked material", entity->Id());
            }

            if (entityManager->HasComponent<LightmapElementComponent>(entity))
            {
                LightmapElementComponent& lightmapElementComponent = entityManager->GetComponent<LightmapElementComponent>(entity);

                lightmapElementComponent.lightmapVolume = MakeWeakRef(volume);
                lightmapElementComponent.lightmapVolumePath = volume->GetPath();
                lightmapElementComponent.lightmapElementId = lightmapElementId;
            }
            else
            {
                LightmapElementComponent lightmapElementComponent;

                lightmapElementComponent.lightmapVolume = MakeWeakRef(volume);
                lightmapElementComponent.lightmapVolumePath = volume->GetPath();
                lightmapElementComponent.lightmapElementId = lightmapElementId;

                entityManager->AddComponent<LightmapElementComponent>(entity, std::move(lightmapElementComponent));
            }

            entity->SetNeedsRenderProxyUpdate();
            entity->MarkDirty();
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
