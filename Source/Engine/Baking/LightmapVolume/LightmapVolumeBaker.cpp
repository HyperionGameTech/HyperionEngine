/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <HyperionPch.hpp>

#include <Baking/LightmapVolume/LightmapVolumeBaker.hpp>
#include <Baking/LightmapVolume/LightmapVolumeBakeJob.hpp>

#include <Baking/Lightmaps/LightmapPathTraceGpu.hpp>

#include <Asset/Assets.hpp>
#include <Asset/AssetRegistry.hpp>

#include <Rendering/Mesh.hpp>
#include <Rendering/Material.hpp>
#include <Rendering/Texture.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <Scene/EntityManager.hpp>
#include <Scene/LightmapVolume.hpp>

#include <Scene/Components/MeshComponent.hpp>
#include <Scene/Components/TransformComponent.hpp>
#include <Scene/Components/BoundingBoxComponent.hpp>
#include <Scene/Components/LightmapElementComponent.hpp>

#include <Core/Threading/TaskSystem.hpp>
#include <Core/Threading/TaskThread.hpp>

#include <Framework/EngineGlobals.hpp>

namespace Hyperion {
namespace Baking {

#pragma region LightmapVolume baking helpers

// Fraction of entityAabb's volume that overlaps volumeAabb, clamped to [0, 1]. Higher = better fit.
static float ComputeLightmapVolumeOverlapWeight(const BoundingBox& entityAabb, const BoundingBox& volumeAabb)
{
    if (!entityAabb.IsValid() || !volumeAabb.IsValid())
    {
        return 0.0f;
    }

    const Vec3f entityExtent = entityAabb.GetExtent();
    const float entityVolume = entityExtent.x * entityExtent.y * entityExtent.z;

    if (entityVolume <= 0.0f)
    {
        return 0.0f;
    }

    const BoundingBox overlap = entityAabb.Intersection(volumeAabb);

    if (!overlap.IsValid())
    {
        return 0.0f;
    }

    const Vec3f overlapExtent = overlap.GetExtent();
    const float overlapVolume = overlapExtent.x * overlapExtent.y * overlapExtent.z;

    return MathUtil::Clamp(overlapVolume / entityVolume, 0.0f, 1.0f);
}

static LightmapShadingType AtlasTextureTypeToShadingType(LightmapVolume::AtlasTextureType type)
{
    switch (type)
    {
    case LightmapVolume::IrradianceTexture:
        return LightmapShadingType::IRRADIANCE;
    case LightmapVolume::BentNormalTexture:
        return LightmapShadingType::BENT_NORMAL;
    default:
        return LightmapShadingType::MAX;
    }
}

static void UpdateAtlasTextures(
    LightmapVolume* lmv,
    uint16 atlasIndex,
    uint32 shadingTypesMask,
    Map<LightmapElementId, FixedArray<typename Baking::BakeData<LightmapVolume>::BitmapType, LightmapVolume::NumAtlasTextureTypes>>&& elementBitmaps)
{
    HYP_LOG(Lightmap, Verbose, "Updating atlas textures for LightmapVolume {}", lmv->Id());

    Assert(atlasIndex < lmv->GetAtlases().Size());

    LightmapVolumeAtlas& atlas = lmv->GetAtlases()[atlasIndex];

    Array<typename Baking::BakeData<LightmapVolume>::BitmapType> atlasBitmaps = {};
    atlasBitmaps.Reserve(LightmapVolume::NumAtlasTextureTypes);
    
    for (uint32 i = 0; i < LightmapVolume::NumAtlasTextureTypes; i++)
    {
        atlasBitmaps.EmplaceBack(atlas.atlasDimensions.x, atlas.atlasDimensions.y);
    }

    for (auto& it : elementBitmaps)
    {
        uint16 elAtlasIndex;
        uint16 elementIndex;
        LightmapElement::GetAtlasAndElementIndex(it.first, elAtlasIndex, elementIndex);

        Assert(elementIndex < atlas.elements.Size());
        const LightmapElement& element = atlas.elements[elementIndex];

        for (uint32 textureTypeIndex = 0; textureTypeIndex < LightmapVolume::NumAtlasTextureTypes; textureTypeIndex++)
        {
            if (!(shadingTypesMask & (1u << uint32(AtlasTextureTypeToShadingType(LightmapVolume::AtlasTextureType(textureTypeIndex))))))
            {
                continue;
            }

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

    // Create atlas textures from the blitted bitmaps, only for the shading types baked this run.
    for (uint32 textureTypeIndex = 0; textureTypeIndex < LightmapVolume::NumAtlasTextureTypes; textureTypeIndex++)
    {
        if (!(shadingTypesMask & (1u << uint32(AtlasTextureTypeToShadingType(LightmapVolume::AtlasTextureType(textureTypeIndex))))))
        {
            continue;
        }

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

        lmv->SetAtlasTexture(atlasIndex, LightmapVolume::AtlasTextureType(textureTypeIndex), atlasTexture);
    }
}

static bool BuildElementTextures(
    LightmapVolume* lmv,
    const BakeData<LightmapVolume>& bakeData,
    LightmapElementId elementId,
    uint32 bakeAtlasIndex,
    uint32 shadingTypesMask)
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

    FixedArray<typename Baking::BakeData<LightmapVolume>::BitmapType, LightmapVolume::NumAtlasTextureTypes> bitmaps;

    if (shadingTypesMask & (1u << uint32(LightmapShadingType::IRRADIANCE)))
    {
        bitmaps[LightmapVolume::IrradianceTexture] = bakeData.ToBitmapIrradiance(bakeAtlasIndex);
    }

    if (shadingTypesMask & (1u << uint32(LightmapShadingType::BENT_NORMAL)))
    {
        bitmaps[LightmapVolume::BentNormalTexture] = bakeData.ToBitmapBentNormal(bakeAtlasIndex);
    }

    FixedArray<typename Baking::BakeData<LightmapVolume>::BitmapType, LightmapVolume::NumAtlasTextureTypes> elementBitmaps;

    for (uint32 i = 0; i < LightmapVolume::NumAtlasTextureTypes; i++)
    {
        if (!(shadingTypesMask & (1u << uint32(AtlasTextureTypeToShadingType(LightmapVolume::AtlasTextureType(i))))))
        {
            continue;
        }

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

    UpdateAtlasTextures(lmv, atlasIndex, shadingTypesMask, { { elementId, std::move(elementBitmaps) } });

    return true;
}

/// @TODO remove when we have a Mesh::Clone()
static Handle<Mesh> CloneMeshForLightmapBake(const Handle<Mesh>& sourceMesh)
{
    Handle<Mesh> clonedMesh = MakeHandle<Mesh>();
    clonedMesh->SetName(NAME_FMT("{}_LightmapBakeClone", sourceMesh->GetName()));

    {
        auto readScope = sourceMesh->GetReadScope();

        const MeshDesc meshDesc = sourceMesh->GetMeshDesc();
        const VertexArrayView vertexData = sourceMesh->GetVertexData(0);
        const Span<const ubyte> indexData = sourceMesh->GetIndexData(0);

        MeshDataView meshData {};
        meshData.vertices[0] = vertexData;
        meshData.indices[0] = ConstByteView(indexData.Data(), indexData.Data() + indexData.Size());

        clonedMesh->SetMeshData(meshDesc, meshData);
    }

    InitObject(clonedMesh);

    clonedMesh->BuildBVH();
    clonedMesh->UploadGpuData();

    return clonedMesh;
}

#pragma endregion LightmapVolume baking helpers

#pragma region Baker<LightmapVolume>

Baker<LightmapVolume>::Baker(BakerConfig&& config, const Handle<LightmapVolume>& volume)
    : BakerBase(std::move(config), volume, MakeStrongRef(volume->GetScene()), volume->GetWorldBounds()),
      m_volume(volume)
{
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

    for (uint32 i = 0; i < uint32(LightmapShadingType::MAX); i++)
    {
        if (!(shadingTypesMask & (1u << i)))
        {
            continue;
        }

        const uint32 maxTexelsPerFrame = MaxTexelsPerFrame();
        AssertDebug(maxTexelsPerFrame > 0);

        const UniquePtr<PathTracer>& pathTracer = m_pathTracers.PushBack(CreatePathTracer(LightmapShadingType(i), maxTexelsPerFrame));

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
    EntityManager& mgr = *m_scene->GetEntityManager();

    m_bakeEntities.Clear();
    m_bakeEntitiesByEntity.Clear();

    const bool onlyOverlappingElements = BakerBase::OnlyOverlappingElements();

    Set<Mesh*> seenMeshes;

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

        // Only claim this entity if we're a better (or equal) fit than whoever currently owns it -
        // prevents a volume's bake from stealing entities that belong to a better-fitting volume.
        const float weight = ComputeLightmapVolumeOverlapWeight(worldAabb, m_aabb);

        if (const LightmapElementComponent* lightmapElementComponent = mgr.TryGetComponent<LightmapElementComponent>(entity))
        {
            if (lightmapElementComponent->NumLightmapVolumeAssignments() > 0)
            {
                const LightmapVolumeId topAssignment = lightmapElementComponent->lightmapVolumeAssignments[0];
                const float topAssignmentWeight = lightmapElementComponent->lightmapVolumeAssignmentWeights[0];

                if (topAssignment != m_volume->GetLightmapVolumeId() && topAssignmentWeight >= weight)
                {
                    continue;
                }
            }
        }

        Handle<Mesh> bakeMesh = meshComponent.mesh;

        // We need to dedupe if we'll be writing to UV1.
        // To do this we maintain a set of visited meshes; then clone the mesh if 
        // it has already been updated to prevent setting incorrectly shared UV1s.
        if (seenMeshes.Contains(bakeMesh.Get()))
        {
            bakeMesh = CloneMeshForLightmapBake(bakeMesh);
        }
        else
        {
            seenMeshes.Add(bakeMesh.Get());
        }

        m_bakeEntities.PushBack(BakeEntity {
            MakeStrongRef(entity),
            bakeMesh,
            meshComponent.material,
            Transform(transformComponent.translation, transformComponent.scale, transformComponent.rotation).GetMatrix(),
            boundingBoxComponent.worldAabb });
    }

    m_bakeData = BakeData<LightmapVolume>(m_bakeEntities.ToSpan(), m_volume);

    m_atlasBuildTask = TaskSystem::GetInstance().Enqueue(
        [buildData = m_bakeData]() mutable -> BakeData<LightmapVolume>
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

void Baker<LightmapVolume>::OnBuildReady()
{
    AssertOnThread(g_simThread);

    m_bakeData = std::move(m_atlasBuildTask).Await();

    AssertDebug(m_bakeData.GetWidth() * m_bakeData.GetHeight() > 0);

    const uint32 shadingTypesMask = GetShadingTypesMask();
    uint32 preserveTextureTypesMask = 0;

    for (uint32 i = 0; i < LightmapVolume::NumAtlasTextureTypes; i++)
    {
        if (!(shadingTypesMask & (1u << uint32(AtlasTextureTypeToShadingType(LightmapVolume::AtlasTextureType(i))))))
        {
            preserveTextureTypesMask |= 1u << i;
        }
    }

    m_volume->RemoveAllElements(preserveTextureTypesMask);

    m_lightmapElementIds.Clear();
    m_lightmapElementIds.Reserve(m_bakeData.GetAtlasCount());

    for (uint32 atlasIndex = 0; atlasIndex < m_bakeData.GetAtlasCount(); atlasIndex++)
    {
        LightmapElement* lightmapElement = nullptr;

        if (!m_volume->AddElement({ m_bakeData.GetWidth(), m_bakeData.GetHeight() }, lightmapElement, /* shrinkToFit */ true, /* downscaleLimit */ 0.1f))
        {
            HYP_LOG(Lightmap, Error, "Failed to add element to volume for atlas {}!", atlasIndex);

            return;
        }

        AssertDebug(lightmapElement != nullptr);
        AssertDebug(lightmapElement->id != InvalidLightmapElementId);

        m_lightmapElementIds.PushBack(lightmapElement->id);
    }

    if (!m_config.onlyGenerateUVs)
    {
        BakerBase::DispatchJobs();
    }
}

void Baker<LightmapVolume>::OnCompleted_Internal()
{
    AssertDebug(!m_lightmapElementIds.Empty());

    m_bakeData.Blur();
    m_bakeData.Dilate();

    const uint32 shadingTypesMask = GetShadingTypesMask();

    for (uint32 atlasIndex = 0; atlasIndex < m_lightmapElementIds.Size(); atlasIndex++)
    {
        if (!BuildElementTextures(m_volume, m_bakeData, m_lightmapElementIds[atlasIndex], atlasIndex, shadingTypesMask))
        {
            HYP_LOG(Lightmap, Error, "Failed to build LightmapElement textures for LightmapVolume, atlas {}, element id: {}",
                atlasIndex, m_lightmapElementIds[atlasIndex]);

            return;
        }
    }

    // Look up all elements once for UV transform
    Array<const LightmapElement*> lightmapElements;
    lightmapElements.Resize(m_lightmapElementIds.Size());

    for (uint32 i = 0; i < m_lightmapElementIds.Size(); i++)
    {
        lightmapElements[i] = m_volume->GetElement(m_lightmapElementIds[i]);
        Assert(lightmapElements[i] != nullptr);
    }

    HYP_LOG(Lightmap, Verbose, "Lightmap baking complete! {} atlas(es)", m_lightmapElementIds.Size());
    
    if (m_lightmapElementIds.Empty())
    {
        // It probably failed
        // Drop out early to prevent crashes due to accessing out of bounds
        return;
    }
    
    // Ensure references to texture assets are saved properly.
    m_volume->MarkDirty();

    // Update meshes
    for (size_t bakeEntityIndex = 0; bakeEntityIndex < m_bakeEntities.Size(); bakeEntityIndex++)
    {
        BakeEntity& bakeEntity = m_bakeEntities[bakeEntityIndex];

        Assert(bakeEntityIndex < m_bakeData.GetMeshData().Size());

        const BakeMeshData& bakeMeshForAtlasCount = m_bakeData.GetMeshData()[bakeEntityIndex];

        // Determine the dominant atlas index for this entity (for the element component assignment,
        // since a single entity can only reference one element/atlas for stencil routing).
        uint32 dominantAtlasIndex = 0;
        {
            Array<uint32> atlasVertexCounts;
            atlasVertexCounts.Resize(m_lightmapElementIds.Size());

            for (int32 vAtlasIndex : bakeMeshForAtlasCount.vertexAtlasIndices)
            {
                if (vAtlasIndex >= 0 && uint32(vAtlasIndex) < atlasVertexCounts.Size())
                {
                    atlasVertexCounts[vAtlasIndex]++;
                }
            }

            for (uint32 i = 1; i < atlasVertexCounts.Size(); i++)
            {
                if (atlasVertexCounts[i] > atlasVertexCounts[dominantAtlasIndex])
                {
                    dominantAtlasIndex = i;
                }
            }

            for (uint32 i = 0; i < m_lightmapElementIds.Size(); i++)
            {
                if (i != dominantAtlasIndex && atlasVertexCounts[i] > 0)
                {
                    HYP_LOG_ONCE(Lightmap, Warning, "Entity {} mesh spans multiple lightmap atlases; rendering may be incorrect for vertices not in the dominant atlas",
                        bakeEntity.entity.IsValid() ? bakeEntity.entity->Id() : ObjIdBase());

                    break;
                }
            }
        }

        const LightmapElementId entityElementId = m_lightmapElementIds[dominantAtlasIndex];

        auto UpdateMeshData = [&]()
        {
            const Handle<Mesh>& mesh = bakeEntity.mesh;
            Assert(mesh.IsValid());

            auto readScope = mesh->GetReadScope();

            BakeMeshData& bakeMesh = m_bakeData.GetMeshData()[bakeEntityIndex];
            Assert(bakeMesh.mesh == mesh);

            const VertexInputLayoutDesc prevInputLayout = mesh->GetMeshDesc().meshAttributes.inputLayout;
            VertexInputLayoutDesc newInputLayout { uint8(prevInputLayout.mask | VT_UV1) };

            const size_t vertexStrideFloats = newInputLayout.VertexSize() / sizeof(float);

            MeshDesc newMeshDesc;
            newMeshDesc.meshAttributes = mesh->GetMeshAttributes();
            newMeshDesc.meshAttributes.inputLayout = newInputLayout;
            newMeshDesc.lods[0].numVertices = uint32(bakeMesh.vertices.Size() / vertexStrideFloats);
            newMeshDesc.lods[0].numIndices = uint32(bakeMesh.indices.Size());

            size_t uv1Offset = 0;
            uv1Offset += (prevInputLayout.mask & VT_Position) ? (sizeof(TVertexPacket<VT_Position>) / sizeof(float)) : 0;
            uv1Offset += (prevInputLayout.mask & VT_Normal) ? (sizeof(TVertexPacket<VT_Normal>) / sizeof(float)) : 0;
            uv1Offset += (prevInputLayout.mask & VT_UV0) ? (sizeof(TVertexPacket<VT_UV0>) / sizeof(float)) : 0;

            AssertDebug(bakeMesh.vertices.Size() % vertexStrideFloats == 0);

            for (size_t i = 0, vertexIndex = 0; i < bakeMesh.vertices.Size(); i += vertexStrideFloats, vertexIndex++)
            {
                float* vertexDataFloat = bakeMesh.vertices.Data() + i;

                TVertexPacket<VT_UV1>* packet = reinterpret_cast<TVertexPacket<VT_UV1>*>(vertexDataFloat + uv1Offset);

                Vec2f uv1 = packet->GetUV1();

                int32 vertexAtlasIndex = (vertexIndex < bakeMesh.vertexAtlasIndices.Size())
                    ? bakeMesh.vertexAtlasIndices[vertexIndex]
                    : int32(dominantAtlasIndex);

                if (vertexAtlasIndex < 0 || uint32(vertexAtlasIndex) >= lightmapElements.Size())
                {
                    vertexAtlasIndex = int32(dominantAtlasIndex);
                }

                const LightmapElement* element = lightmapElements[vertexAtlasIndex];

                // Scale UV1 to atlas section
                uv1 *= element->scale;
                uv1 += Vec2f(element->offsetUV.x, element->offsetUV.y);
                packet->SetUV1(uv1);
            }

            VertexArrayView vertexArrayView {};
            vertexArrayView.floatData = reinterpret_cast<const float*>(bakeMesh.vertices.Data());
            vertexArrayView.layoutDesc = newMeshDesc.meshAttributes.inputLayout;
            vertexArrayView.vertexCount = bakeMesh.vertices.Size() / vertexStrideFloats;

            readScope.Reset();

            MeshDataView meshData {};
            meshData.vertices[0] = vertexArrayView;
            meshData.indices[0] = bakeMesh.indices.ToByteView();

            mesh->SetMeshData(newMeshDesc, meshData);

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
            // @TODO Look for material to use as a base that matches what we need rather than creating it right out of the gate.

            const Handle<Material>& currentMaterial = bakeEntity.material;

            MaterialAttributes newAttributes = currentMaterial->GetAttributes();
            newAttributes.bucket = RenderBucket::Lightmapped;

            Handle<Material> lmMaterial = MakeHandle<Material>(
                NAME_FMT("{}_LM", currentMaterial->GetName()),
                newAttributes,
                currentMaterial->GetParameters(),
                currentMaterial->GetTextures());

            Assert(lmMaterial != nullptr);

            lmMaterial->SetParameters(bakeEntity.material->GetParameters());
            lmMaterial->SetTextures(bakeEntity.material->GetTextures());

            EnqueueDeletion(std::move(bakeEntity.material));

            InitObject(lmMaterial);

            GetCurrentAssetRegistry()->PutAssetsDeep(lmMaterial);

            bakeEntity.material = lmMaterial;

            isNewMaterial = true;
        }
#endif

        auto updateMeshComponent = [entityManagerWeak = MakeWeakRef(m_scene->GetEntityManager()),
                                    entityElementId,
                                    volume = m_volume,
                                    volumeAabb = m_aabb,
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

                // It has changed -- ie cloned.
                if (meshComponent.mesh.Get() != bakeEntity.mesh.Get())
                {
                    meshComponent.mesh = bakeEntity.mesh;

                    entity->MarkDirty();
                }
            }
            else
            {
                HYP_LOG(Lightmap, Warning, "Entity {} does not have a MeshComponent, cannot assign baked material", entity->Id());
            }

            const float weight = ComputeLightmapVolumeOverlapWeight(bakeEntity.aabb, volumeAabb);

            const LightmapVolumeId lightmapVolumeId = volume->GetLightmapVolumeId();
            Assert(lightmapVolumeId != InvalidLightmapVolumeId);

            auto setVolumeAssignment = [lightmapVolumeId, weight](LightmapElementComponent& lightmapElementComponent)
            {
                auto& assignments = lightmapElementComponent.lightmapVolumeAssignments;
                auto& weights = lightmapElementComponent.lightmapVolumeAssignmentWeights;

                uint32 numAssignments = lightmapElementComponent.NumLightmapVolumeAssignments();
                uint32 index = numAssignments;

                for (uint32 i = 0; i < numAssignments; i++)
                {
                    if (assignments[i] == lightmapVolumeId)
                    {
                        index = i;
                        break;
                    }
                }

                if (index == numAssignments)
                {
                    if (numAssignments < MaxLightmapVolumeAssignments)
                    {
                        numAssignments++;
                    }
                    else
                    {
                        // Replaces the lowest prio one
                        index = MaxLightmapVolumeAssignments - 1;
                    }
                }

                FixedArray<Pair<LightmapVolumeId, float>, MaxLightmapVolumeAssignments> kvpArray {};

                for (uint32 i = 0; i < numAssignments; i++)
                {
                    kvpArray[i] = { assignments[i], weights[i] };
                }
                
                if (numAssignments < MaxLightmapVolumeAssignments)
                {
                    std::fill(
                        kvpArray.Begin() + numAssignments,
                        kvpArray.End(),
                        Pair<LightmapVolumeId, float> { InvalidLightmapVolumeId, 0.0f });
                }

                kvpArray[index] = { lightmapVolumeId, weight };

                // Keep ordered
                std::sort(
                    kvpArray.Begin(),
                    kvpArray.Begin() + numAssignments,
                    [](const Pair<LightmapVolumeId, float>& a, const Pair<LightmapVolumeId, float>& b)
                    {
                        return a.second > b.second;
                    });
                
                for (uint32 i = 0; i < MaxLightmapVolumeAssignments; i++)
                {
                    assignments[i] = kvpArray[i].first;
                    weights[i] = kvpArray[i].second;
                }
            };

            if (entityManager->HasComponent<LightmapElementComponent>(entity))
            {
                LightmapElementComponent& lightmapElementComponent = entityManager->GetComponent<LightmapElementComponent>(entity);

                setVolumeAssignment(lightmapElementComponent);

                lightmapElementComponent.lightmapVolume = MakeWeakRef(volume);
                lightmapElementComponent.lightmapElementId = entityElementId;
            }
            else
            {
                LightmapElementComponent lightmapElementComponent;

                setVolumeAssignment(lightmapElementComponent);

                lightmapElementComponent.lightmapVolume = MakeWeakRef(volume);
                lightmapElementComponent.lightmapElementId = entityElementId;

                entityManager->AddComponent<LightmapElementComponent>(entity, std::move(lightmapElementComponent));
            }

            entity->SetNeedsRenderProxyUpdate();
            entity->MarkDirty();
        };

        if (IsOnThread(m_scene->GetEntityManager()->GetOwnerThreadId()))
        {
            updateMeshComponent();
        }
        else
        {
            ThreadBase* thread = GetThreadById(m_scene->GetEntityManager()->GetOwnerThreadId());
            Assert(thread != nullptr);

            thread->GetScheduler().Enqueue(std::move(updateMeshComponent), TaskEnqueueFlags::FIRE_AND_FORGET);
        }
    }
}

#pragma endregion Baker < LightmapVolume>

} // namespace Baking
} // namespace Hyperion
