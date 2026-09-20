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

static PathTraceType AtlasTextureTypeToShadingType(LightmapVolume::AtlasTextureType type)
{
    switch (type)
    {
    case LightmapVolume::IrradianceTexture:
        return PathTraceType::Lightmap;
    case LightmapVolume::BentNormalTexture:
        return PathTraceType::BentNormals;
    default:
        return PathTraceType::Max;
    }
}

using LightmapColorBitmap = BakeData<LightmapVolume>::ColorBitmap;
using LightmapBentNormalBitmap = BakeData<LightmapVolume>::BentNormalBitmap;

struct LightmapElementBitmaps
{
    UniquePtr<LightmapColorBitmap, BakerAllocator> irradiance;
    UniquePtr<LightmapBentNormalBitmap, BakerAllocator> bentNormal;
};

static void UpdateAtlasTextures(
    LightmapVolume* lmv,
    uint16 atlasIndex,
    Name swatchName,
    const LightmapElementBitmaps& atlasBitmaps)
{
    HYP_LOG(Lightmap, Verbose, "Updating atlas textures for LightmapVolume {} on swatch '{}'", lmv->Id(), swatchName);

    Assert(atlasIndex < lmv->GetAtlases().Size());

    const Vec2u atlasDimensions = lmv->GetAtlases()[atlasIndex].atlasDimensions;

    if (auto* irradiance = atlasBitmaps.irradiance.Get())
    {
        Handle<Texture> atlasTexture = MakeHandle<Texture>(
            TextureDesc {
                TextureType::Texture2D,
                irradiance->GetFormat(),
                Vec3u { atlasDimensions, 1 },
                TextureFilterMode::Linear,
                TextureFilterMode::Linear,
                TextureWrapMode::ClampToEdge
            },
            irradiance->ToByteView());

        lmv->SetAtlasTextureForSwatch(atlasIndex, LightmapVolume::IrradianceTexture, atlasTexture, swatchName);
    }

    if (auto* bentNormal = atlasBitmaps.bentNormal.Get())
    {
        Handle<Texture> atlasTexture = MakeHandle<Texture>(
            TextureDesc {
                TextureType::Texture2D,
                bentNormal->GetFormat(),
                Vec3u { atlasDimensions, 1 },
                TextureFilterMode::Linear,
                TextureFilterMode::Linear,
                TextureWrapMode::ClampToEdge
            },
            bentNormal->ToByteView());

        lmv->SetAtlasTextureForSwatch(atlasIndex, LightmapVolume::BentNormalTexture, atlasTexture, swatchName);
    }
}

// Resizes \c bitmap down/up to \c targetDimensions via a blit if it doesn't already match, moving
// it through unchanged otherwise.
template <class BitmapType>
static BitmapType ResizeBitmapToElement(BitmapType&& bitmap, Vec2u targetDimensions)
{
    if (bitmap.GetWidth() == targetDimensions.x && bitmap.GetHeight() == targetDimensions.y)
    {
        return std::move(bitmap);
    }

    BitmapType resized(targetDimensions.x, targetDimensions.y);

    Rect<uint32> srcRect { 0, 0, bitmap.GetWidth(), bitmap.GetHeight() };
    Rect<uint32> dstRect { 0, 0, targetDimensions.x, targetDimensions.y };

    BitmapUtils::Blit(bitmap, resized, srcRect, dstRect);

    return resized;
}

// Places a baked page into its slot in the atlas. When the packing is being reused the bake already
// rasterized straight into atlas space, so the bitmap passes through untouched.
template <class BitmapType>
static BitmapType BuildAtlasBitmap(BitmapType&& bakedBitmap, const LightmapElement& element, Vec2u atlasDimensions, bool reuseExistingPacking)
{
    if (reuseExistingPacking)
    {
        return std::move(bakedBitmap);
    }

    BitmapType elementBitmap = ResizeBitmapToElement(std::move(bakedBitmap), element.dimensions);

    Assert(element.offsetCoords.x + element.dimensions.x <= atlasDimensions.x);
    Assert(element.offsetCoords.y + element.dimensions.y <= atlasDimensions.y);

    BitmapType atlasBitmap(atlasDimensions.x, atlasDimensions.y);

    Rect<uint32> srcRect { 0, 0, elementBitmap.GetWidth(), elementBitmap.GetHeight() };
    Rect<uint32> dstRect {
        element.offsetCoords.x, element.offsetCoords.y,
        element.offsetCoords.x + element.dimensions.x,
        element.offsetCoords.y + element.dimensions.y
    };

    BitmapUtils::Blit(elementBitmap, atlasBitmap, srcRect, dstRect);

    return atlasBitmap;
}

static bool BuildElementTextures(
    LightmapVolume* lmv,
    const BakeData<LightmapVolume>& bakeData,
    LightmapElementId elementId,
    uint32 bakeAtlasIndex,
    uint32 shadingTypesMask,
    Name swatchName)
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

    const LightmapVolumeAtlas& atlas = lmv->GetAtlases()[atlasIndex];
    const LightmapElement& element = atlas.elements[elementIndex];

    const bool reuseExistingPacking = bakeData.IsReusingExistingPacking();

    LightmapElementBitmaps atlasBitmaps;

    if (shadingTypesMask & (1u << uint32(PathTraceType::Lightmap)))
    {
        atlasBitmaps.irradiance = MakeUniqueWithAllocator<LightmapColorBitmap, BakerAllocator>(
            BuildAtlasBitmap(bakeData.ToBitmapIrradiance(bakeAtlasIndex), element, atlas.atlasDimensions, reuseExistingPacking));
    }

    if (shadingTypesMask & (1u << uint32(PathTraceType::BentNormals)))
    {
        atlasBitmaps.bentNormal = MakeUniqueWithAllocator<LightmapBentNormalBitmap, BakerAllocator>(
            BuildAtlasBitmap(bakeData.ToBitmapBentNormal(bakeAtlasIndex), element, atlas.atlasDimensions, reuseExistingPacking));
    }

    UpdateAtlasTextures(lmv, atlasIndex, swatchName, atlasBitmaps);

    return true;
}

static Handle<Mesh> CloneMeshForLightmapBake(const Handle<Mesh>& sourceMesh)
{
    Handle<Mesh> clonedMesh = sourceMesh->Clone();

    // Will be made unique on PutAssetUnique().
    clonedMesh->SetName(sourceMesh->GetName());

    if (!clonedMesh->GetBVH().IsValid())
    {
        BVHNode bvh;
        clonedMesh->BuildBVH(bvh);

        auto writeScope = clonedMesh->GetWriteScope();
        clonedMesh->SetBVH(std::move(bvh));
    }

    GetCurrentAssetRegistry()->PutAssetUnique(clonedMesh);

    InitObject(clonedMesh);

    clonedMesh->UploadGpuData();

    return clonedMesh;
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

bool Baker<LightmapVolume>::ComputeShouldReuseExistingPacking()
{
    Scene* scene = m_volume->GetScene();
    Assert(scene && m_bakeLayer);

    if (!scene || !m_bakeLayer)
    {
        HYP_LOG(Lightmap, Error, "Cannot compute packing hash: no scene or bake layer");

        return false;
    }

    m_packingEntryHash = BakeEpoch::ComputePackingHash(*m_volume, *m_bakeLayer);

    const uint64 cachedPackingHash = m_volume->GetPackingHash();

    if (cachedPackingHash != m_packingEntryHash)
    {
        HYP_LOG(Lightmap, Info, "Packing hash mismatch: cached={}, computed={}. UV1s will not be reused between LMV textures", cachedPackingHash, m_packingEntryHash);

        return false;
    }

    return true;
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
    EntityManager& mgr = *m_scene->GetEntityManager();

    m_bakeEntities.Clear();
    m_bakeEntitiesByEntity.Clear();

    const bool onlyOverlappingElements = BakerBase::OnlyOverlappingElements();

    m_reuseExistingPacking = ComputeShouldReuseExistingPacking();

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
            && meshComponent.material->GetBucket() != RenderBucket::Lightmapped
            && meshComponent.material->GetBucket() != RenderBucket::Translucent)
        {
            continue;
        }

        if (meshComponent.mesh->GetMeshAttributes().inputLayout.mask & (VT_Tree | VT_Foliage))
        {
             continue;
        }
        
        //terrain cannot be lightmapped as the meshes are dynamically built based on height data
        if (mgr.HasComponent<TerrainPatchComponent>(entity))
        {
            continue;
        }

        const BoundingBox& worldAabb = boundingBoxComponent.worldAabb;

        if (onlyOverlappingElements && !m_aabb.Overlaps(worldAabb))
        {
            continue;
        }

        // Only claim this entity if we're a better or equal fit than whoever currently owns it,
        // if whoever owns it is a valid lightmap volume (not removed from scene).
        const float weight = ComputeLightmapVolumeOverlapWeight(worldAabb, m_aabb);

        if (const LightmapElementComponent* lightmapElementComponent = mgr.TryGetComponent<LightmapElementComponent>(entity))
        {
            if (lightmapElementComponent->NumLightmapVolumeAssignments() > 0)
            {
                const LightmapVolumeId topAssignment = lightmapElementComponent->lightmapVolumeAssignments[0];

                if (topAssignment != m_volume->GetLightmapVolumeId())
                {
                    LightmapSystem* lightmapSystem = m_scene->GetWorld()->GetSystem<LightmapSystem>();

                    if (lightmapSystem != nullptr && lightmapSystem->IsIdForAliveLightmapVolume(topAssignment))
                    {
                        const float topAssignmentWeight = lightmapElementComponent->lightmapVolumeAssignmentWeights[0];

                        if (topAssignmentWeight >= weight)
                        {
                            // SKIP! We're not as much of a fit for them as we thought we were.
                            continue;
                        }
                    }
                }
            }
        }

        Handle<Mesh> bakeMesh = meshComponent.mesh;

        uint32 lightmapAtlasIndex = 0;

        if (m_reuseExistingPacking)
        {
            // Reusing the packing means reusing UV1 as it stands, so shared meshes stay shared.
            const LightmapElementComponent* lightmapElementComponent = mgr.TryGetComponent<LightmapElementComponent>(entity);

            if (!lightmapElementComponent || !m_volume->GetElement(lightmapElementComponent->lightmapElementId))
            {
                HYP_LOG(Lightmap, Warning, "Entity {} has no lightmap element in this volume; run a bake on the Default swatch first",
                    entity->GetName());

                continue;
            }

            uint16 atlasIndex;
            uint16 elementIndex;
            LightmapElement::GetAtlasAndElementIndex(lightmapElementComponent->lightmapElementId, atlasIndex, elementIndex);

            lightmapAtlasIndex = atlasIndex;
        }
        else if (seenMeshes.Contains(bakeMesh.Get()))
        {
            // UV1 is about to be rewritten, so a mesh used by more than one entity has to be split first.
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
            boundingBoxComponent.worldAabb,
            lightmapAtlasIndex
        });
    }

    if (m_bakeEntities.Empty())
    {
        HYP_LOG(Lightmap, Warning, "No entities to bake for LightmapVolume {}", m_volume->GetName());
    }

    m_bakeData = BakeData<LightmapVolume>(m_bakeEntities.ToSpan(), m_volume, m_reuseExistingPacking);

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

    m_lightmapElementIds.Clear();

    if (ShouldReuseExistingPacking())
    {
        // The packing and the meshes' UV1 are shared by every swatch, so only the textures get rebuilt.
        for (uint32 atlasIndex = 0; atlasIndex < uint32(m_volume->GetAtlases().Size()); atlasIndex++)
        {
            const LightmapVolumeAtlas& atlas = m_volume->GetAtlases()[atlasIndex];

            if (atlas.elements.Empty())
            {
                HYP_LOG(Lightmap, Error, "LightmapVolume atlas {} has no elements to rebake onto", atlasIndex);

                return;
            }

            m_lightmapElementIds.PushBack(atlas.elements[0].id);
        }

        if (m_lightmapElementIds.Empty())
        {
            HYP_LOG(Lightmap, Error, "LightmapVolume {} has no packing to rebake onto",
                m_volume->GetName());

            return;
        }
    }
    else
    {
        m_volume->RemoveAllElements(preserveTextureTypesMask);

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
    }

    m_volume->SetPackingHash(m_packingEntryHash);

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

    const Name bakeLayerName = GetBakeLayerName();

    for (uint32 atlasIndex = 0; atlasIndex < m_lightmapElementIds.Size(); atlasIndex++)
    {
        if (!BuildElementTextures(m_volume, m_bakeData, m_lightmapElementIds[atlasIndex], atlasIndex, shadingTypesMask, bakeLayerName))
        {
            HYP_LOG(Lightmap, Error, "Failed to build LightmapElement textures for LightmapVolume, atlas {}, element id: {}",
                atlasIndex, m_lightmapElementIds[atlasIndex]);

            return;
        }
    }

    // Look up all elements once for UV transform
    Array<const LightmapElement*, BakerAllocator> lightmapElements;
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

    if (ShouldReuseExistingPacking())
    {
        // No need to create UV1s.
        return;
    }

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

        auto updateMeshData = [&]()
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

            const bool hadLods = mesh->GetMeshDesc().GetNumLods() > 1;

            // Handles write scope on its own
            mesh->SetMeshData(newMeshDesc, meshData);

            // The bake replaces LOD 0 with a new vertex set, so any LODs it had reference vertices that no
            // longer exist - rebuild them so they carry the new lightmap UVs too.
            if (hadLods)
            {
                TResult<MeshLodGenerationResult> lodResult = MeshLodGenerator::Generate(mesh.Get(), mesh->GetLodGenerationSettings());

                if (lodResult.HasError())
                {
                    HYP_LOG(Lightmap, Warning, "Failed to rebuild LODs for mesh '{}' after baking: {}",
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
                HYP_LOG(Lightmap, Error, "Failed to save mesh '{}' after setting new mesh data: {}",
                        mesh->GetName(),
                        saveResult.GetError().GetMessage());

                return;
            }

            // needs reupload!
            if (mesh->isUploaded.Load())
            {
                mesh->UploadGpuData();
            }
        };

        updateMeshData();

        // Update material to have the Lightmapped bucket (if it does not already)
        AssertDebug(bakeEntity.material.IsValid());

        bool isNewMaterial = false;

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
