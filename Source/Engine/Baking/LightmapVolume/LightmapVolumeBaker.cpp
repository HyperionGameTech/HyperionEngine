/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <HyperionPch.hpp>

#include <Baking/LightmapVolume/LightmapVolumeBaker.hpp>
#include <Baking/LightmapVolume/LightmapVolumeBakeJob.hpp>

#include <Baking/PathTracer/PathTracer.hpp>

#include <Asset/Assets.hpp>
#include <Asset/AssetRegistry.hpp>

#include <Rendering/Mesh.hpp>
#include <Rendering/Material.hpp>
#include <Rendering/Texture.hpp>

#include <Rendering/Util/MeshLodGenerator.hpp>
#include <Rendering/Util/DeletionQueue.hpp>

#include <Scene/World.hpp>
#include <Scene/EntityManager.hpp>
#include <Scene/LightmapVolume.hpp>

#include <Scene/Systems/LightmapSystem.hpp>

#include <Scene/Swatch.hpp>

#include <Baking/BakeEpoch.hpp>
#include <Baking/BakeLayer.hpp>

#include <Scene/Components/MeshComponent.hpp>
#include <Scene/Components/TransformComponent.hpp>
#include <Scene/Components/BoundingBoxComponent.hpp>
#include <Scene/Components/LightmapElementComponent.hpp>
#include <Scene/Components/TerrainPatchComponent.hpp>

#include <Core/Containers/Set.hpp>

#include <Core/Threading/TaskSystem.hpp>
#include <Core/Threading/TaskThread.hpp>

#include <Framework/EngineGlobals.hpp>

namespace Hyperion {
namespace Baking {

#pragma region LightmapVolume baking helpers

// How much of entityAabb lies inside volumeAabb, in [0, 1]. Higher = better fit.
static float ComputeLightmapVolumeOverlapWeight(const BoundingBox& entityAabb, const BoundingBox& volumeAabb)
{
    if (!entityAabb.IsValid() || !volumeAabb.IsValid() || !volumeAabb.Overlaps(entityAabb))
    {
        return 0.0f;
    }

    const BoundingBox overlap = entityAabb.Intersection(volumeAabb);

    if (!overlap.IsValid())
    {
        return 0.0f;
    }

    const Vec3f entityExtent = entityAabb.GetExtent();
    const Vec3f overlapExtent = overlap.GetExtent();

    // flat entities (a floor plane) have no extent on some axis; overlapping on it is all that matters there
    auto axisWeight = [](float entityAxisExtent, float overlapAxisExtent) -> float
    {
        return entityAxisExtent > 1e-6f ? MathUtil::Clamp(overlapAxisExtent / entityAxisExtent, 0.0f, 1.0f) : 1.0f;
    };

    return axisWeight(entityExtent.x, overlapExtent.x)
        * axisWeight(entityExtent.y, overlapExtent.y)
        * axisWeight(entityExtent.z, overlapExtent.z);
}

using LightmapColorBitmap = BakeData<LightmapVolume>::ColorBitmap;
using LightmapBentNormalBitmap = BakeData<LightmapVolume>::BentNormalBitmap;

static void UpdateAtlasTextures(
    LightmapVolume* lmv,
    const BakeData<LightmapVolume>& bakeData,
    uint16 atlasIndex,
    uint32 shadingTypesMask,
    Name swatchName)
{
    AssertOnThread(g_simThread);

    HYP_LOG(Lightmap, Verbose, "Updating atlas textures for LightmapVolume {} on swatch '{}'", lmv->Id(), swatchName);

    Assert(atlasIndex < lmv->GetAtlases().Size());

    const Vec2u atlasDimensions = lmv->GetAtlases()[atlasIndex].atlasDimensions;
    Assert(atlasDimensions.x == bakeData.GetWidth() && atlasDimensions.y == bakeData.GetHeight());

    // bakes rasterize straight into atlas space, so the whole page is uploaded as is
    if (shadingTypesMask & (1u << uint32(PathTraceType::Lightmap)))
    {
        const LightmapColorBitmap irradiance = bakeData.ToBitmapIrradiance(atlasIndex);

        Handle<Texture> atlasTexture = MakeHandle<Texture>(
            TextureDesc {
                TextureType::Texture2D,
                irradiance.GetFormat(),
                Vec3u { atlasDimensions, 1 },
                TextureFilterMode::Linear,
                TextureFilterMode::Linear,
                TextureWrapMode::ClampToEdge
            },
            irradiance.ToByteView());

        lmv->SetAtlasTextureForSwatch(atlasIndex, LightmapVolume::IrradianceTexture, atlasTexture, swatchName);
    }

    if (shadingTypesMask & (1u << uint32(PathTraceType::BentNormals)))
    {
        const LightmapBentNormalBitmap bentNormal = bakeData.ToBitmapBentNormal(atlasIndex);

        Handle<Texture> atlasTexture = MakeHandle<Texture>(
            TextureDesc {
                TextureType::Texture2D,
                bentNormal.GetFormat(),
                Vec3u { atlasDimensions, 1 },
                TextureFilterMode::Linear,
                TextureFilterMode::Linear,
                TextureWrapMode::ClampToEdge
            },
            bentNormal.ToByteView());

        lmv->SetAtlasTextureForSwatch(atlasIndex, LightmapVolume::BentNormalTexture, atlasTexture, swatchName);
    }
}

template <class Func>
static void RunOnEntityManagerThread(EntityManager& entityManager, Func&& func)
{
    if (IsOnThread(entityManager.GetOwnerThreadId()))
    {
        func();

        return;
    }

    ThreadBase* thread = GetThreadById(entityManager.GetOwnerThreadId());
    Assert(thread != nullptr);

    thread->GetScheduler().Enqueue(std::forward<Func>(func), TaskEnqueueFlags::FIRE_AND_FORGET);
}

static Handle<Material> CreateLightmappedMaterial(const Handle<Material>& sourceMaterial)
{
    MaterialAttributes newAttributes = sourceMaterial->GetAttributes();
    newAttributes.bucket = RenderBucket::Lightmapped;

    Handle<Material> lightmappedMaterial = MakeHandle<Material>(
        NAME_FMT("{}_LM", sourceMaterial->GetName()),
        newAttributes,
        sourceMaterial->GetParameters(),
        sourceMaterial->GetTextures());

    Assert(lightmappedMaterial != nullptr);

    lightmappedMaterial->SetParameters(sourceMaterial->GetParameters());
    lightmappedMaterial->SetTextures(sourceMaterial->GetTextures());

    InitObject(lightmappedMaterial);

    GetCurrentAssetRegistry()->PutAssetsDeep(lightmappedMaterial);

    return lightmappedMaterial;
}

#pragma endregion LightmapVolume baking helpers

#pragma region Baker<LightmapVolume>

Baker<LightmapVolume>::Baker(BakerConfig&& config, BakeLayer& bakeLayer, const Handle<LightmapVolume>& volume)
    : BakerBase(std::move(config), bakeLayer, volume, MakeStrongRef(volume->GetScene()), volume->GetWorldBounds()),
      m_volume(volume)
{
}

Name Baker<LightmapVolume>::GetBakeLayerName() const
{
    return m_bakeLayer ? m_bakeLayer->name : g_defaultSwatchName;
}

BoundingBox Baker<LightmapVolume>::GetTraceBounds() const
{
    if (!m_aabb.IsValid())
    {
        return m_aabb;
    }

    const Vec3f margin = m_aabb.GetExtent();

    return BoundingBox(m_aabb.min - margin, m_aabb.max + margin);
}

UniquePtr<BakeJobBase> Baker<LightmapVolume>::CreateJob(BakeJobParams&& params)
{
    return MakeUnique<BakeJob<LightmapVolume>>(std::move(params), m_volume, &m_bakeData);
}

void Baker<LightmapVolume>::CreateLightmapRenderers()
{
    m_pathTracers.Clear();

    if (!PerformsRayTracing())
    {
        return;
    }

    const uint32 shadingTypesMask = GetShadingTypesMask();

    for (uint32 i = 0; i < uint32(PathTraceType::Max); i++)
    {
        if (!(shadingTypesMask & (1u << i)))
        {
            continue;
        }

        const uint32 maxTexelsPerFrame = MaxTexelsPerFrame();
        AssertDebug(maxTexelsPerFrame > 0);

        const UniquePtr<PathTracer>& pathTracer = m_pathTracers.PushBack(CreatePathTracer(PathTraceType(i), maxTexelsPerFrame));

        if (!pathTracer)
        {
            continue;
        }

        pathTracer->Create();
    }
}

void Baker<LightmapVolume>::Initialize_Internal()
{
    // no-op
}

void Baker<LightmapVolume>::Build()
{
    AssertOnThread(g_simThread);

    EntityManager& mgr = *m_scene->GetEntityManager();

    m_bakeEntities.Clear();
    m_bakeEntitiesByEntity.Clear();
    m_releasedEntities.Clear();

    m_buildFailed = false;

    const LightmapVolumeId volumeId = m_volume->GetLightmapVolumeId();
    Assert(volumeId != InvalidLightmapVolumeId);

    LightmapSystem* lightmapSystem = m_scene->GetWorld() ? m_scene->GetWorld()->GetSystem<LightmapSystem>() : nullptr;

    Set<Entity*> claimedEntities;

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

        // translucent geometry still blocks light in the trace, it just can't be shaded from a lightmap
        if (meshComponent.material->GetBucket() != RenderBucket::Opaque
            && meshComponent.material->GetBucket() != RenderBucket::Lightmapped)
        {
            continue;
        }

        if (meshComponent.mesh->GetMeshAttributes().topology != Topology::Triangles)
        {
            continue;
        }

        if (meshComponent.mesh->GetMeshAttributes().inputLayout.mask & (VT_Tree | VT_Foliage))
        {
            continue;
        }

        // one rect can't hold the lighting of every instance
        if (meshComponent.numInstances != 0 || meshComponent.enableAutoInstancing)
        {
            continue;
        }

        //terrain cannot be lightmapped as the meshes are dynamically built based on height data
        if (mgr.HasComponent<TerrainPatchComponent>(entity))
        {
            continue;
        }

        const BoundingBox& worldAabb = boundingBoxComponent.worldAabb;

        const float weight = ComputeLightmapVolumeOverlapWeight(worldAabb, m_aabb);

        if (weight <= 0.0f)
        {
            continue;
        }

        // an entity has one rect in one atlas, so it belongs to whichever volume covers it best; ties stay with the current owner
        if (const LightmapElementComponent* lightmapElementComponent = mgr.TryGetComponent<LightmapElementComponent>(entity))
        {
            if (lightmapElementComponent->lightmapVolumeId != volumeId
                && lightmapSystem != nullptr
                && lightmapSystem->IsIdForAliveLightmapVolume(lightmapElementComponent->lightmapVolumeId)
                && lightmapElementComponent->lightmapVolumeWeight >= weight)
            {
                continue;
            }
        }

        claimedEntities.Insert(entity);

        m_bakeEntities.PushBack(BakeEntity {
            MakeStrongRef(entity),
            meshComponent.mesh,
            meshComponent.material,
            Transform(transformComponent.translation, transformComponent.scale, transformComponent.rotation).GetMatrix(),
            worldAabb,
            weight });
    }

    for (auto [entity, lightmapElementComponent] : mgr.GetEntitySet<LightmapElementComponent>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
    {
        if (lightmapElementComponent.lightmapVolumeId == volumeId && !claimedEntities.Contains(entity))
        {
            m_releasedEntities.PushBack(MakeStrongRef(entity));
        }
    }

    if (m_bakeEntities.Empty())
    {
        HYP_LOG(Lightmap, Warning, "No entities to bake for LightmapVolume {}", m_volume->GetName());
    }

    m_bakeData = BakeData<LightmapVolume>(m_bakeEntities.ToSpan(), m_volume->GetTexelsPerUnit());

    if (TryReuseExistingPacking())
    {
        HYP_LOG(Lightmap, Info, "Rebaking LightmapVolume {} onto its existing packing", m_volume->GetName());
    }
    else
    {
        HYP_LOG(Lightmap, Info, "Repacking LightmapVolume {}; every layer's lightmaps for it will need rebaking", m_volume->GetName());
    }

    m_atlasBuildTask = TaskSystem::GetInstance().Enqueue(
        [buildData = std::move(m_bakeData)]() mutable -> BakeData<LightmapVolume>
        {
            Result result = buildData.Build();

            if (result.HasError())
            {
                HYP_LOG(Lightmap, Error, "Failed to build lightmap data: {}", result.GetError().GetMessage());

                return {};
            }

            return std::move(buildData);
        },
        TaskThreadPoolName::THREAD_POOL_BACKGROUND);

    m_state = BakerState::Building;
}

bool Baker<LightmapVolume>::TryReuseExistingPacking()
{
    if (m_bakeEntities.Empty() || m_volume->NumUsedAtlases() == 0)
    {
        return false;
    }

    if (m_volume->GetPackedTexelsPerUnit() != m_volume->GetTexelsPerUnit())
    {
        return false;
    }

    // UV1 is about to change for at least one mesh, so its rects are stale
    if (m_bakeData.AnyMeshNeedsUnwrap())
    {
        return false;
    }

    EntityManager& mgr = *m_scene->GetEntityManager();

    const LightmapVolumeId volumeId = m_volume->GetLightmapVolumeId();

    Array<BakeData<LightmapVolume>::EntityRect, BakerAllocator> entityRects;
    entityRects.Resize(m_bakeEntities.Size());

    Set<uint32> usedElementIds;

    for (size_t entityIndex = 0; entityIndex < m_bakeEntities.Size(); entityIndex++)
    {
        const BakeEntity& bakeEntity = m_bakeEntities[entityIndex];

        const LightmapElementComponent* lightmapElementComponent = mgr.TryGetComponent<LightmapElementComponent>(bakeEntity.entity.Get());

        // new to this volume
        if (!lightmapElementComponent || lightmapElementComponent->lightmapVolumeId != volumeId)
        {
            return false;
        }

        const uint64 meshLightmapUVHash = bakeEntity.mesh->GetLightmapUVDataHash();

        // mesh was swapped since the rect was packed
        if (meshLightmapUVHash == 0 || lightmapElementComponent->meshLightmapUVHash != meshLightmapUVHash)
        {
            return false;
        }

        const LightmapElement* element = m_volume->GetElement(lightmapElementComponent->lightmapElementId);

        if (!element || !element->IsValid())
        {
            return false;
        }

        // a duplicated entity shares its original's rect until it gets one of its own
        if (usedElementIds.Contains(uint32(element->id)))
        {
            return false;
        }

        usedElementIds.Insert(uint32(element->id));

        BakeData<LightmapVolume>::EntityRect& entityRect = entityRects[entityIndex];
        entityRect.valid = true;
        entityRect.atlasIndex = element->GetAtlasIndex();
        entityRect.elementIndex = element->GetElementIndex();
        entityRect.offsetCoords = element->offsetCoords;
        entityRect.dimensions = element->dimensions;
        entityRect.offsetUV = element->offsetUV;
        entityRect.scale = element->scale;
    }

    m_bakeData.UseExistingPacking(*m_volume, std::move(entityRects));

    return true;
}

void Baker<LightmapVolume>::OnBuildReady()
{
    AssertOnThread(g_simThread);

    m_bakeData = std::move(m_atlasBuildTask).Await();

    if (!m_bakeData.IsBuilt())
    {
        // OnCompleted_Internal() sorts out what's left
        m_buildFailed = true;

        return;
    }

    if (!m_config.onlyGenerateUVs)
    {
        BakerBase::DispatchJobs();
    }
}

void Baker<LightmapVolume>::WriteBackUnwrappedMeshes()
{
    AssertOnThread(g_simThread);

    // UV1 doesn't depend on the volume, so the new unwrap goes straight onto the shared mesh asset
    for (const BakeData<LightmapVolume>::MeshSnapshot& meshSnapshot : m_bakeData.GetMeshSnapshots())
    {
        if (!meshSnapshot.valid || !meshSnapshot.unwrapped)
        {
            continue;
        }

        const Handle<Mesh>& mesh = meshSnapshot.mesh;
        Assert(mesh.IsValid());

        const size_t vertexSize = meshSnapshot.layout.VertexSize();

        MeshDesc newMeshDesc;
        newMeshDesc.meshAttributes = mesh->GetMeshAttributes();
        newMeshDesc.meshAttributes.inputLayout = meshSnapshot.layout;
        newMeshDesc.meshAttributes.indexBufferElemType = GpuElemType::UnsignedInt;
        newMeshDesc.lods[0] = mesh->GetMeshDesc().lods[0];
        newMeshDesc.lods[0].numVertices = uint32(meshSnapshot.vertices.ByteSize() / vertexSize);
        newMeshDesc.lods[0].numIndices = uint32(meshSnapshot.indices.Size());

        VertexArrayView vertexArrayView {};
        vertexArrayView.floatData = meshSnapshot.vertices.Data();
        vertexArrayView.layoutDesc = meshSnapshot.layout;
        vertexArrayView.vertexCount = newMeshDesc.lods[0].numVertices;

        MeshDataView meshData {};
        meshData.vertices[0] = vertexArrayView;
        meshData.indices[0] = meshSnapshot.indices.ToByteView();

        const bool hadLods = mesh->GetMeshDesc().GetNumLods() > 1;

        // Handles write scope on its own
        mesh->SetMeshData(newMeshDesc, meshData);

        uint64 lightmapUvDataHash = 0;

        {
            auto readScope = mesh->GetReadScope();

            lightmapUvDataHash = mesh->ComputeLod0DataHash();
        }

        mesh->SetLightmapUVDataHash(lightmapUvDataHash);

        // LOD 0 has a new vertex set, so the old LODs point at vertices that no longer exist - rebuild them with the new UV1
        if (hadLods)
        {
            TResult<MeshLodGenerationResult> lodResult = MeshLodGenerator::Generate(mesh.Get(), mesh->GetLodGenerationSettings());

            if (lodResult.HasError())
            {
                HYP_LOG(Lightmap, Warning, "Failed to rebuild LODs for mesh '{}' after generating lightmap UVs: {}",
                    mesh->GetName(), lodResult.GetError().GetMessage());
            }
            else
            {
                (void)MeshLodGenerator::Apply(mesh.Get(), lodResult.GetValue());
            }
        }

        GetCurrentAssetRegistry()->PutAssetUnique(mesh);

        Result saveResult = mesh->Save();

        if (saveResult.HasError())
        {
            HYP_LOG(Lightmap, Error, "Failed to save mesh '{}' after generating lightmap UVs: {}",
                mesh->GetName(),
                saveResult.GetError().GetMessage());
        }

        // needs reupload!
        if (mesh->isUploaded.Load())
        {
            mesh->UploadGpuData();
        }
    }
}

void Baker<LightmapVolume>::ReleaseEntity(const Handle<Entity>& entity)
{
    const Handle<EntityManager>& entityManager = m_scene->GetEntityManager();

    if (!entity.IsValid() || entity->GetEntityManager() != entityManager.Get())
    {
        return;
    }

    RunOnEntityManagerThread(*entityManager, [entityManagerWeak = MakeWeakRef(entityManager), entity, volumeId = m_volume->GetLightmapVolumeId()]()
        {
            Handle<EntityManager> entityManager = entityManagerWeak.Lock();

            if (!entityManager || entity->GetEntityManager() != entityManager.Get() || !entityManager->HasEntity(entity->Id()))
            {
                return;
            }

            const LightmapElementComponent* lightmapElementComponent = entityManager->TryGetComponent<LightmapElementComponent>(entity.Get());

            // taken over by another volume in the meantime
            if (!lightmapElementComponent || lightmapElementComponent->lightmapVolumeId != volumeId)
            {
                return;
            }

            entityManager->RemoveComponent<LightmapElementComponent>(entity.Get());

            entity->SetNeedsRenderProxyUpdate();
            entity->MarkDirty();
        });
}

void Baker<LightmapVolume>::OnCompleted_Internal()
{
    AssertOnThread(g_simThread);

    LightmapSystem* lightmapSystem = m_scene->GetWorld() ? m_scene->GetWorld()->GetSystem<LightmapSystem>() : nullptr;

    if (m_buildFailed)
    {
        if (!m_bakeEntities.Empty())
        {
            HYP_LOG(Lightmap, Error, "Lightmap bake for LightmapVolume {} failed; keeping its previous lightmaps", m_volume->GetName());

            return;
        }

        // nothing left inside the volume to light
        m_volume->RemoveAllElements();

        for (const Handle<Entity>& entity : m_releasedEntities)
        {
            ReleaseEntity(entity);
        }

        if (lightmapSystem != nullptr)
        {
            lightmapSystem->AssignStencilValues();
        }

        return;
    }

    m_bakeData.Blur();
    m_bakeData.Dilate();

    const bool repacked = !m_bakeData.IsReusingExistingPacking();

    if (repacked)
    {
        // every layer's textures were baked for the old packing
        m_volume->RemoveAllElements();
        m_volume->SetPacking(std::move(m_bakeData.GetPackedAtlases()), m_volume->GetTexelsPerUnit());
    }

    const uint32 shadingTypesMask = GetShadingTypesMask();
    const Name bakeLayerName = GetBakeLayerName();

    for (uint32 atlasIndex = 0; atlasIndex < m_bakeData.GetAtlasCount(); atlasIndex++)
    {
        UpdateAtlasTextures(m_volume, m_bakeData, uint16(atlasIndex), shadingTypesMask, bakeLayerName);
    }

    // Ensure references to texture assets are saved properly.
    m_volume->MarkDirty();

    WriteBackUnwrappedMeshes();

    HYP_LOG(Lightmap, Verbose, "Lightmap baking complete! {} atlas(es)", m_bakeData.GetAtlasCount());

    const Handle<EntityManager>& entityManager = m_scene->GetEntityManager();
    const LightmapVolumeId volumeId = m_volume->GetLightmapVolumeId();

    BoundingBox coverageBounds = repacked ? BoundingBox::Empty() : m_volume->GetCoverageBounds();

    // one lightmapped variant per source material, so entities sharing a material keep sharing it
    Map<ObjId<Material>, Handle<Material>> lightmappedMaterials;

    const Span<const BakeData<LightmapVolume>::EntityRect> entityRects = m_bakeData.GetEntityRects();

    for (size_t bakeEntityIndex = 0; bakeEntityIndex < m_bakeEntities.Size(); bakeEntityIndex++)
    {
        const BakeEntity& bakeEntity = m_bakeEntities[bakeEntityIndex];

        // entity may have been deleted or moved to another scene mid-bake
        if (!bakeEntity.entity.IsValid() || bakeEntity.entity->GetEntityManager() != entityManager.Get())
        {
            HYP_LOG(Lightmap, Warning, "Skipping baked entity {}: removed from the scene during the bake",
                bakeEntity.entity.IsValid() ? bakeEntity.entity->Id() : ObjIdBase());

            continue;
        }

        const BakeData<LightmapVolume>::EntityRect& entityRect = entityRects[bakeEntityIndex];

        if (!entityRect.valid)
        {
            // couldn't be unwrapped or packed; whatever it had from this volume no longer matches
            ReleaseEntity(bakeEntity.entity);

            continue;
        }

        coverageBounds = coverageBounds.IsValid() ? coverageBounds.Union(bakeEntity.aabb) : bakeEntity.aabb;

        Handle<Material> lightmappedMaterial = bakeEntity.material;

        if (bakeEntity.material->GetBucket() != RenderBucket::Lightmapped)
        {
            auto lightmappedMaterialIt = lightmappedMaterials.Find(bakeEntity.material->Id());

            if (lightmappedMaterialIt != lightmappedMaterials.End())
            {
                lightmappedMaterial = lightmappedMaterialIt->second;
            }
            else
            {
                lightmappedMaterial = CreateLightmappedMaterial(bakeEntity.material);

                lightmappedMaterials.Set(bakeEntity.material->Id(), lightmappedMaterial);
            }
        }

        const LightmapElementId elementId = LightmapElement::MakeId(entityRect.atlasIndex, entityRect.elementIndex);
        const uint64 meshLightmapUVHash = bakeEntity.mesh->GetLightmapUVDataHash();

        RunOnEntityManagerThread(*entityManager, [entityManagerWeak = MakeWeakRef(entityManager),
                                                     volume = m_volume,
                                                     entity = bakeEntity.entity,
                                                     sourceMaterial = bakeEntity.material,
                                                     lightmappedMaterial,
                                                     elementId,
                                                     volumeId,
                                                     volumeWeight = bakeEntity.volumeWeight,
                                                     meshLightmapUVHash]()
            {
                Handle<EntityManager> entityManager = entityManagerWeak.Lock();

                if (!entityManager)
                {
                    return;
                }

                // could have been deleted or moved to another scene between the bake finishing and this running
                if (!entity.IsValid() || entity->GetEntityManager() != entityManager.Get() || !entityManager->HasEntity(entity->Id()))
                {
                    return;
                }

                if (MeshComponent* meshComponent = entityManager->TryGetComponent<MeshComponent>(entity.Get()))
                {
                    // leave it alone if someone assigned a different material mid-bake
                    if (lightmappedMaterial != sourceMaterial && meshComponent->material == sourceMaterial)
                    {
                        EnqueueDeletion(std::move(meshComponent->material));

                        meshComponent->material = lightmappedMaterial;

                        entity->MarkDirty();
                    }
                }
                else
                {
                    HYP_LOG(Lightmap, Warning, "Entity {} does not have a MeshComponent, cannot assign baked material", entity->Id());
                }

                if (!entityManager->HasComponent<LightmapElementComponent>(entity))
                {
                    entityManager->AddComponent<LightmapElementComponent>(entity, LightmapElementComponent {});
                }

                LightmapElementComponent& lightmapElementComponent = entityManager->GetComponent<LightmapElementComponent>(entity);
                lightmapElementComponent.lightmapElementId = elementId;
                lightmapElementComponent.lightmapVolumeId = volumeId;
                lightmapElementComponent.lightmapVolumeWeight = volumeWeight;
                lightmapElementComponent.meshLightmapUVHash = meshLightmapUVHash;
                lightmapElementComponent.lightmapVolume = MakeWeakRef(volume);

                entity->SetNeedsRenderProxyUpdate();
                entity->MarkDirty();
            });
    }

    for (const Handle<Entity>& entity : m_releasedEntities)
    {
        ReleaseEntity(entity);
    }

    m_volume->SetCoverageBounds(coverageBounds);

    // the atlas count and lighting bounds may have changed, which can shift stencil values for this and overlapping volumes
    if (lightmapSystem != nullptr)
    {
        lightmapSystem->AssignStencilValues();
    }
}

#pragma endregion Baker < LightmapVolume>

} // namespace Baking
} // namespace Hyperion
