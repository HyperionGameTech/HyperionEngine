/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <lightmapper/Lightmapper.hpp>
#include <lightmapper/LightmapJob.hpp>
#include <lightmapper/LightmapPathTraceCpu.hpp>
#include <lightmapper/LightmapPathTraceGpu.hpp>
#include <lightmapper/LightmapAccelerationStructure.hpp>
#include <lightmapper/LightmapVolume.hpp>

#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/RenderHelpers.hpp>
#include <rendering/RenderCollection.hpp>
#include <rendering/RenderBackend.hpp>
#include <rendering/RenderObject.hpp>
#include <rendering/RenderConfig.hpp>
#include <rendering/RenderDevice.hpp>
#include <rendering/RenderFrame.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/Material.hpp>
#include <rendering/Texture.hpp>
#include <rendering/Renderer.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <asset/TextureAsset.hpp>

#include <scene/BVH.hpp>
#include <scene/World.hpp>
#include <scene/Light.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/EnvGrid.hpp>
#include <scene/View.hpp>

#include <scene/util/VoxelOctree.hpp>

#include <scene/camera/Camera.hpp>
#include <scene/camera/OrthoCamera.hpp>

#include <scene/EntityManager.hpp>
#include <scene/components/MeshComponent.hpp>
#include <scene/components/TransformComponent.hpp>
#include <scene/components/BoundingBoxComponent.hpp>
#include <scene/components/LightmapElementComponent.hpp>

#include <asset/Assets.hpp>
#include <asset/AssetRegistry.hpp>

#include <core/reflection/Class.hpp>

#include <core/threading/TaskSystem.hpp>
#include <core/threading/TaskThread.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/utilities/Time.hpp>
#include <core/utilities/DeferredScope.hpp>
#include <core/utilities/Float16.hpp>

#include <core/math/Triangle.hpp>

#include <util/MeshBuilder.hpp>

#include <system/AppContext.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>

#include <Lightmapper.generated.inl>

namespace hyperion {

static constexpr uint32 TileSize = 32;

#pragma region LightmapperConfig

void LightmapperConfig::PostLoadCallback()
{
    if (traceMode == LightmapTraceMode::GPU_PATH_TRACING)
    {
        if (!g_renderBackend->GetRenderConfig().raytracing)
        {
            traceMode = LightmapTraceMode::CPU_PATH_TRACING;

            HYP_LOG(Lightmap, Warning, "GPU path tracing is not supported on this device. Falling back to CPU path tracing.");
        }
    }
}

#pragma endregion LightmapperConfig

#pragma region LightmapperBase

LightmapperBase::LightmapperBase(LightmapperConfig&& config, const Handle<Scene>& scene, const BoundingBox& aabb)
    : m_config(std::move(config)),
      m_scene(scene),
      m_aabb(aabb),
      m_threadPool(nullptr),
      m_numJobs { 0 }
{
}

LightmapperBase::~LightmapperBase()
{
    m_queue.Clear();

    if (m_view != nullptr)
    {
        m_scene->GetWorld()->RemoveView(m_view);

        SafeDelete(std::move(m_view));
    }

    if (m_threadPool)
    {
        if (m_threadPool->IsRunning())
        {
            m_threadPool->Stop();
        }

        delete m_threadPool;
        m_threadPool = nullptr;
    }
}

bool LightmapperBase::IsComplete() const
{
    return m_numJobs.Get(MemoryOrder::ACQUIRE) == 0;
}

void LightmapperBase::Initialize()
{
    HYP_LOG(Lightmap, Info, "Initializing lightmapper: {}", m_config.ToString());

    Handle<Camera> camera = CreateObject<Camera>();
    camera->SetName(NAME_FMT("{}_Camera", InstanceClass()->GetName()));
    camera->AddCameraController(CreateObject<OrthoCameraController>());
    InitObject(camera);

    // dummy output target
    ViewOutputTargetDesc outputTargetDesc {
        .extent = Vec2u::One(),
        .attachments = { { TF_R8 } }
    };

    ViewDesc viewDesc {
        .flags = ViewFlags::COLLECT_STATIC_ENTITIES
            | ViewFlags::NO_FRUSTUM_CULLING
            | ViewFlags::SKIP_ENV_GRIDS | ViewFlags::SKIP_LIGHTMAP_VOLUMES | ViewFlags::SKIP_PARTICLE_VOLUMES
            | ViewFlags::RAYTRACING
            | ViewFlags::NO_DRAW_CALLS
            | ViewFlags::NOT_MULTI_BUFFERED,
        .viewport = Viewport { .extent = Vec2u::One(), .position = Vec2i::Zero() },
        .outputTargetDesc = outputTargetDesc,
        .scenes = { m_scene },
        .camera = camera
    };

    m_view = CreateObject<View>(viewDesc);
    InitObject(m_view);

    m_view->UpdateViewport();
    m_view->UpdateVisibility();
    m_view->CollectSync();

    /// If cpu path tracing, set up thread pool and stuff
    if (m_config.traceMode == LightmapTraceMode::CPU_PATH_TRACING)
    {
        BuildResourceCache();
        BuildAccelerationStructures();

        m_threadPool = new LightmapThreadPool();
        m_threadPool->Start();
    }

    Initialize_Internal();

    Build();
}

LightmapJobParams LightmapperBase::CreateLightmapJobParams(SizeType startIndex, SizeType endIndex)
{
    Array<UniquePtr<ILightmapRenderer>> lightmapRenderers;

    const uint32 shadingTypesMask = GetShadingTypesMask();

    for (uint32 i = 0; i < uint32(LightmapShadingType::MAX); i++)
    {
        if (!(shadingTypesMask & (1u << i)))
        {
            continue;
        }

        switch (LightmapShadingType(i))
        {
        case LightmapShadingType::RADIANCE:
            if (!m_config.radiance)
            {
                continue;
            }

            break;
        case LightmapShadingType::IRRADIANCE:
            if (!m_config.irradiance)
            {
                continue;
            }

            break;
        case LightmapShadingType::FULL: // fallthrough
        default:
            break;
        }

        UniquePtr<ILightmapRenderer>& lightmapRenderer = lightmapRenderers.EmplaceBack();
        lightmapRenderer = CreateRenderer(LightmapShadingType(i), TileSize * TileSize * m_config.numSamples);

        if (!lightmapRenderer)
        {
            continue;
        }

        lightmapRenderer->Create();
    }

    Assert(lightmapRenderers.Any());

    LightmapJobParams jobParams {
        &m_config,
        m_scene,
        m_view,
        m_subElements.ToSpan().Slice(startIndex, endIndex - startIndex),
        &m_subElementsByEntity,
        std::move(lightmapRenderers)
    };

    return jobParams;
}

UniquePtr<ILightmapRenderer> LightmapperBase::CreateRenderer(LightmapShadingType shadingType, uint32 maxRaysPerFrame)
{
    switch (m_config.traceMode)
    {
    case LightmapTraceMode::GPU_PATH_TRACING:
        return MakeUnique<LightmapRenderer_GpuPathTracing>(this, m_scene, shadingType, maxRaysPerFrame);
    case LightmapTraceMode::CPU_PATH_TRACING:
        return MakeUnique<LightmapRenderer_CpuPathTracing>(this, m_accelerationStructure.Get(), m_threadPool, m_scene, shadingType);
    default:
        HYP_UNREACHABLE();
    }
}

void LightmapperBase::BuildAccelerationStructures()
{
    Assert(m_accelerationStructure == nullptr);
    m_accelerationStructure = MakeUnique<LightmapTopLevelAccelerationStructure>();

    if (m_subElements.Empty())
    {
        return;
    }

    for (LightmapSubElement& subElement : m_subElements)
    {
        const Handle<MeshAsset>& meshAsset = subElement.mesh->GetAsset();

        if (!meshAsset)
        {
            HYP_LOG(Lightmap, Error, "Mesh asset is invalid for entity {} in lightmapper", subElement.entity.Id());
            continue;
        }

        ResourceHandle resourceHandle(*meshAsset->GetResource());

        const MeshData& meshData = *meshAsset->GetMeshData();

        BVHNode bvhNode;

        if (!meshData.BuildBVH(bvhNode, /* maxDepth */ 3))
        {
            HYP_LOG(Lightmap, Error, "Failed to build BVH for mesh on entity {} in lightmapper", subElement.entity.Id());

            continue;
        }

        Array<Vertex> vertices = meshData.vertexData;

        Array<uint32> indices;
        indices.Resize(meshData.indexData.Size() / sizeof(uint32));
        Memory::MemCpy(indices.Data(), meshData.indexData.Data(), meshData.indexData.Size());

        m_accelerationStructure->Add(
            &subElement,
            std::move(bvhNode),
            std::move(vertices),
            std::move(indices));
    }
}

/// Build cache to keep scene meshes, textures etc. in memory while we perform CPU path tracing
void LightmapperBase::BuildResourceCache()
{
    HYP_NAMED_SCOPE("Building lightmapper resource cache");

    HYP_LOG(Lightmap, Info, "Building lightmapper resource cache");

    Mutex mtx;

    TaskBatch taskBatch;

    auto callback = [&](LightmapSubElement& subElement, uint32, uint32) -> void
    {
        Array<CachedResource> localResources;

        if (subElement.mesh.IsValid())
        {
            Assert(subElement.mesh->GetAsset().IsValid());

            localResources.EmplaceBack(subElement.mesh->GetAsset(), *subElement.mesh->GetAsset()->GetResource());
        }

        if (subElement.material.IsValid())
        {
            for (const Handle<Texture>& texture : subElement.material->GetTextures())
            {
                if (texture.IsValid())
                {
                    Assert(texture->GetAsset().IsValid());

                    localResources.EmplaceBack(texture->GetAsset(), *texture->GetAsset()->GetResource());
                }
            }
        }

        if (localResources.Any())
        {
            Mutex::Guard guard(mtx);

            for (CachedResource& cachedResource : localResources)
            {
                m_resourceCache.Set(std::move(cachedResource));
            }
        }
    };

    TaskSystem::GetInstance().ParallelForEach_Batch(
        taskBatch,
        (m_subElements.Size() + 255) / 256,
        m_subElements, callback);

    TaskSystem::GetInstance().EnqueueBatch(&taskBatch);

    while (!taskBatch.IsCompleted())
    {
        ThreadSleep(1000);

        Mutex::Guard guard(mtx);

        HYP_LOG(Lightmap, Debug, "Waiting for lightmapper resource cache to finish building... ({} resources discovered)", m_resourceCache.Size());
    }
}

void LightmapperBase::Build()
{
    HYP_SCOPE;
    const uint32 idealTrianglesPerJob = m_config.idealTrianglesPerJob;

    Assert(m_numJobs.Get(MemoryOrder::ACQUIRE) == 0, "Cannot initialize lightmap renderer -- jobs currently running!");

    // Build jobs
    HYP_LOG(Lightmap, Info, "Building graph for lightmapper");

    EntityManager& mgr = *m_scene->GetEntityManager();

    m_subElements.Clear();
    m_subElementsByEntity.Clear();

    const bool onlyOverlappingElements = OnlyOverlappingElements();

    for (auto [entity, meshComponent, transformComponent, boundingBoxComponent, _] : mgr.GetEntitySet<MeshComponent, TransformComponent, BoundingBoxComponent, TagComponent<EntityTag::STATIC>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
    {
        if (entity->InstanceClass() != Entity::StaticClass())
        {
            // skip non-Entity instances (we only want Entities with MeshComponent)
            continue;
        }

        if (!meshComponent.mesh || !meshComponent.material)
        {
            continue;
        }

        // Only process opaque and translucent materials
        if (meshComponent.material->GetBucket() != RB_OPAQUE && meshComponent.material->GetBucket() != RB_TRANSLUCENT)
        {
            continue;
        }

        const BoundingBox& worldAabb = boundingBoxComponent.worldAabb;

        if (!onlyOverlappingElements && !m_aabb.Overlaps(worldAabb))
        {
            continue; // must be inside volume to be considered
        }

        m_subElements.PushBack(LightmapSubElement {
            MakeStrongRef(entity),
            meshComponent.mesh,
            meshComponent.material,
            transformComponent.transform,
            boundingBoxComponent.worldAabb });
    }

    Build_Internal();

    uint32 numTriangles = 0;
    SizeType startIndex = 0;

    if (ShouldSplitIntoJobs())
    {
        for (SizeType index = 0; index < m_subElements.Size(); index++)
        {
            LightmapSubElement& subElement = m_subElements[index];

            m_subElementsByEntity.Set(subElement.entity, &subElement);

            if (idealTrianglesPerJob != 0 && numTriangles != 0 && numTriangles + subElement.mesh->NumIndices() / 3 > idealTrianglesPerJob)
            {
                UniquePtr<LightmapJobBase> job = CreateJob(CreateLightmapJobParams(startIndex, index + 1));
                Assert(job != nullptr);

                startIndex = index + 1;

                AddJob(std::move(job));

                numTriangles = 0;
            }

            numTriangles += subElement.mesh->NumIndices() / 3;
        }
    }

    if (startIndex < m_subElements.Size() - 1)
    {
        UniquePtr<LightmapJobBase> job = CreateJob(CreateLightmapJobParams(startIndex, m_subElements.Size()));
        Assert(job != nullptr);

        AddJob(std::move(job));
    }
}

void LightmapperBase::Update(float delta)
{
    HYP_SCOPE;

    uint32 numJobs = m_numJobs.Get(MemoryOrder::ACQUIRE);

//    m_view->UpdateViewport();
//    m_view->UpdateVisibility();
//    m_view->CollectSync();

    Mutex::Guard guard(m_queueMutex);

    for (auto it = m_queue.Begin(); it != m_queue.End() && numRaysProcessed < IdealRaysPerFrame;)
    {
        LightmapJobBase* job = it->Get();

        if (job->IsRunning())
        {
            numRaysProcessed += job->Process(MathUtil::Max(0, int64(IdealRaysPerFrame) - int64(numRaysProcessed)));
        }
        else if (numRaysProcessed + (job->GetTexelIndices().Size() * m_config.numSamples) <= IdealRaysPerFrame)
        {
            job->Start();

            AssertDebug(job->IsRunning());

            numRaysProcessed += job->Process(MathUtil::Max(0, int64(IdealRaysPerFrame) - int64(numRaysProcessed)));
        }

        if (job->IsCompleted())
        {
            HandleCompletedJob(job);

            it = m_queue.Erase(it);

            continue;
        }

        ++it;
    }

    HYP_LOG(Lightmap, Debug, "Processing {} primary rays this frame", numRaysProcessed);
}

void LightmapperBase::HandleCompletedJob(LightmapJobBase* job)
{
    HYP_SCOPE;
    AssertOnThread(g_gameThread);

    HYP_DEFER({
        m_numJobs.Decrement(1, MemoryOrder::RELEASE);
    });

    if (job->GetResult().HasError())
    {
        HYP_LOG(Lightmap, Error, "Lightmap job {} failed with error: {}", job->GetUUID(), job->GetResult().GetError().GetMessage());

        return;
    }

    HandleCompletedJob_Internal(job);

    HYP_LOG(Lightmap, Debug, "Tracing completed for lightmapping job {} ({} subelements)", job->GetUUID(), job->GetSubElements().Size());
}

#pragma endregion LightmapperBase

#pragma region Lightmapper < LightmapVolume>

Lightmapper<LightmapVolume>::Lightmapper(LightmapperConfig&& config, const Handle<LightmapVolume>& volume)
    : LightmapperBase(std::move(config), MakeStrongRef(volume->GetScene()), volume->GetWorldAABB()),
      m_volume(volume),
      m_lightmapElementId(InvalidLightmapElementId)
{
}

void Lightmapper<LightmapVolume>::Initialize_Internal()
{
    // no-op
}

void Lightmapper<LightmapVolume>::Build()
{
    HYP_SCOPE;

    EntityManager& mgr = *m_scene->GetEntityManager();

    m_subElements.Clear();
    m_subElementsByEntity.Clear();

    const bool onlyOverlappingElements = OnlyOverlappingElements();

    for (auto [entity, meshComponent, transformComponent, boundingBoxComponent, _] : mgr.GetEntitySet<MeshComponent, TransformComponent, BoundingBoxComponent, TagComponent<EntityTag::STATIC>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
    {
        if (entity->InstanceClass() != Entity::StaticClass())
        {
            continue;
        }

        if (!meshComponent.mesh || !meshComponent.material)
        {
            continue;
        }

        if (meshComponent.material->GetBucket() != RB_OPAQUE && meshComponent.material->GetBucket() != RB_TRANSLUCENT)
        {
            continue;
        }

        const BoundingBox& worldAabb = boundingBoxComponent.worldAabb;

        if (!onlyOverlappingElements && !m_aabb.Overlaps(worldAabb))
        {
            continue;
        }

        m_subElements.PushBack(LightmapSubElement {
            MakeStrongRef(entity),
            meshComponent.mesh,
            meshComponent.material,
            transformComponent.transform,
            boundingBoxComponent.worldAabb });
    }

    // Build global data
    m_lightmapData = LightmapData<LightmapVolume>(m_subElements.ToSpan(), m_volume);

    if (Result result = m_lightmapData.Build(); result.HasError())
    {
        HYP_LOG(Lightmap, Error, "Failed to build lightmap data: {}", result.GetError().GetMessage());
        return;
    }

    LightmapElement lightmapElement;
    if (!m_volume->AddElement({ m_lightmapData.width, m_lightmapData.height }, lightmapElement, /* shrinkToFit */ true, /* downscaleLimit */ 0.1f))
    {
        HYP_LOG(Lightmap, Error, "Failed to add element to volume!");
        return;
    }

    m_lightmapElementId = lightmapElement.id;
    AssertDebug(m_lightmapElementId != InvalidLightmapElementId);

    const uint32 width = m_lightmapData.width;
    const uint32 height = m_lightmapData.height;

    HashMap<Vec2i, Array<uint32>> tileBuckets;

    for (uint32 i = 0; i < m_lightmapData.texels.Size(); i++)
    {
        const LightmapTexel& texel = m_lightmapData.texels[i];

        if (!texel.ray.meshId.IsValid())
        {
            continue;
        }

        // calculate tile coord based on texel index
        const uint32 x = i % width;
        const uint32 y = i / width;

        const Vec2i tileCoord { int(x / TileSize), int(y / TileSize) };
        tileBuckets[tileCoord].PushBack(i);
    }

    HYP_LOG(Lightmap, Info, "Dispatching {} tile jobs for {} valid texels", tileBuckets.Size(), m_lightmapData.texels.Size());

    for (auto& it : tileBuckets)
    {
        UniquePtr<LightmapJob<LightmapVolume>> job = MakeUnique<LightmapJob<LightmapVolume>>(
            CreateLightmapJobParams(0, m_subElements.Size()),
            m_volume,
            &m_lightmapData);

        job->SetTexelIndices(std::move(it.second));
        AddJob(std::move(job));
    }
}

void Lightmapper<LightmapVolume>::HandleCompletedJob_Internal(LightmapJobBase* job)
{
    HYP_SCOPE;

    if (m_numJobs.Get(MemoryOrder::ACQUIRE) == 1)
    {
        AssertDebug(m_lightmapElementId != InvalidLightmapElementId);

        if (!m_volume->BuildElementTextures(m_lightmapData, m_lightmapElementId))
        {
            HYP_LOG(Lightmap, Error, "Failed to build LightmapElement textures for LightmapVolume, element id: {}", m_lightmapElementId);
            return;
        }

        const LightmapElement* lightmapElement = m_volume->GetElement(m_lightmapElementId);
        Assert(lightmapElement != nullptr);

        HYP_LOG(Lightmap, Debug, "Lightmap baking complete! Building element with id {}, UV offset: {}, Scale: {}", m_lightmapElementId,
            lightmapElement->offsetUv, lightmapElement->scale);

        // Update meshes
        for (SizeType subElementIndex = 0; subElementIndex < m_subElements.Size(); subElementIndex++)
        {
            LightmapSubElement& subElement = m_subElements[subElementIndex];

            auto updateMeshData = [&]()
            {
                const Handle<Mesh>& mesh = subElement.mesh;
                Assert(mesh.IsValid());

                Assert(subElementIndex < m_lightmapData.GetMeshData().Size());

                const LightmapMeshData& lightmapMeshData = m_lightmapData.GetMeshData()[subElementIndex];
                Assert(lightmapMeshData.mesh == mesh);

                MeshDesc newMeshDesc;
                newMeshDesc.meshAttributes = mesh->GetMeshAttributes();
                newMeshDesc.numVertices = uint32(lightmapMeshData.vertices.Size());
                newMeshDesc.numIndices = uint32(lightmapMeshData.indices.Size());

                MeshData newMeshData;
                newMeshData.vertexData = lightmapMeshData.vertices;
                newMeshData.indexData = ByteBuffer(lightmapMeshData.indices.ToByteView());

                for (SizeType i = 0; i < newMeshData.vertexData.Size(); i++)
                {
                    Vec2f& lightmapUv = newMeshData.vertexData[i].texcoord1;
                    lightmapUv.y = 1.0f - lightmapUv.y; // Invert Y coordinate for lightmaps
                    lightmapUv *= lightmapElement->scale;
                    lightmapUv += Vec2f(lightmapElement->offsetUv.x, lightmapElement->offsetUv.y);
                }

                mesh->SetMeshData(newMeshDesc, newMeshData);
            };

            updateMeshData();

            bool isNewMaterial = false;

            if (subElement.material)
            {
                Handle<Material> clonedMaterial = subElement.material->Clone();
                SafeDelete(std::move(subElement.material));

                subElement.material = clonedMaterial;
            }
            else
            {
                subElement.material = CreateObject<Material>();
            }

            isNewMaterial = true;

            subElement.material->SetBucket(RB_LIGHTMAP);

            subElement.material->SetTexture(MaterialTextureKey::IRRADIANCE_MAP, m_volume->GetAtlasTexture(lightmapElement->GetAtlasIndex(), LTT_IRRADIANCE));
            subElement.material->SetTexture(MaterialTextureKey::RADIANCE_MAP, m_volume->GetAtlasTexture(lightmapElement->GetAtlasIndex(), LTT_RADIANCE));

            auto updateMeshComponent = [entityManagerWeak = MakeWeakRef(m_scene->GetEntityManager()), lightmapElementId = m_lightmapElementId, volume = m_volume, subElement = subElement, newMaterial = (isNewMaterial ? subElement.material : Handle<Material>::empty)]()
            {
                Handle<EntityManager> entityManager = entityManagerWeak.Lock();

                if (!entityManager)
                {
                    return;
                }

                const Handle<Entity>& entity = subElement.entity;

                if (entityManager->HasComponent<MeshComponent>(entity))
                {
                    MeshComponent& meshComponent = entityManager->GetComponent<MeshComponent>(entity);

                    if (newMaterial.IsValid())
                    {
                        InitObject(newMaterial);

                        SafeDelete(std::move(meshComponent.material));

                        meshComponent.material = std::move(newMaterial);
                    }
                }
                else
                {
                    Assert(newMaterial.IsValid());
                    InitObject(newMaterial);

                    MeshComponent meshComponent {};
                    meshComponent.mesh = subElement.mesh;
                    meshComponent.material = newMaterial;

                    entityManager->AddComponent<MeshComponent>(entity, std::move(meshComponent));
                }

                if (entityManager->HasComponent<LightmapElementComponent>(entity))
                {
                    LightmapElementComponent& lightmapElementComponent = entityManager->GetComponent<LightmapElementComponent>(entity);

                    lightmapElementComponent.lightmapVolume = volume.ToWeak();
                    lightmapElementComponent.lightmapElementId = lightmapElementId;
                    lightmapElementComponent.lightmapVolumeUuid = volume->GetUUID();
                }
                else
                {
                    LightmapElementComponent lightmapElementComponent;

                    lightmapElementComponent.lightmapVolume = volume.ToWeak();
                    lightmapElementComponent.lightmapElementId = lightmapElementId;
                    lightmapElementComponent.lightmapVolumeUuid = volume->GetUUID();

                    entityManager->AddComponent<LightmapElementComponent>(entity, std::move(lightmapElementComponent));
                }

            entity->SetNeedsRenderProxyUpdate();
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
}

#pragma endregion Lightmapper < LightmapVolume>

#pragma region Lightmapper < EnvProbe>

Lightmapper<EnvProbe>::Lightmapper(LightmapperConfig&& config, const Handle<EnvProbe>& envProbe)
    : LightmapperBase(std::move(config), MakeStrongRef(envProbe->GetScene()), envProbe->GetAABB()),
      m_envProbe(envProbe)
{
}

void Lightmapper<EnvProbe>::Initialize_Internal()
{
    Assert(m_envProbe != nullptr);
}

void Lightmapper<EnvProbe>::HandleCompletedJob_Internal(LightmapJobBase* job)
{
    HYP_SCOPE;

    LightmapJob<EnvProbe>* jobCasted = static_cast<LightmapJob<EnvProbe>*>(job);

    const LightmapData<EnvProbe>& lightmapData = jobCasted->GetLightmapData();

    if (!lightmapData.IsBuilt())
    {
        HYP_LOG(Lightmap, Warning, "Lightmap data for EnvProbe {} is not built, skipping texture creation", m_envProbe->Id());
        return;
    }

    const Vec2u dimensions = m_envProbe->GetDimensions();

    // Convert lightmap data to bitmaps (6 faces stacked vertically)
    LightmapData<EnvProbe>::BitmapType bitmap = lightmapData.ToBitmap();

    TextureDesc textureDesc {
        TT_CUBEMAP,
        bitmap.GetFormat(),
        Vec3u { dimensions.x, dimensions.y, 1 },
        TFM_LINEAR_MIPMAP,
        TFM_LINEAR,
        TWM_CLAMP_TO_EDGE
    };

    TextureData textureData {
        ByteBuffer(bitmap.ToByteView())
    };

    Texture::GenerateMipmaps(textureDesc, textureData);

    Handle<Texture> cubemap = CreateObject<Texture>(textureDesc, std::move(textureData));

    cubemap->SetName(NAME_FMT("EnvProbe_{}_Baked", m_envProbe->GetUUID()));

    if (Result result = g_assetManager->GetAssetRegistry()->RegisterAsset("$Import/Media/Lightmaps", cubemap->GetAsset()).Await(); result.HasError())
    {
        HYP_LOG(Lightmap, Error, "Failed to register radiance texture '{}' with asset registry: {}", cubemap->GetName(), result.GetError().GetMessage());
    }

    InitObject(cubemap);

    // Set the baked texture on the EnvProbe
    m_envProbe->SetBakedTexture(cubemap);
    m_envProbe->SetIsBaked(true);

    HYP_LOG(Lightmap, Info, "EnvProbe {} lightmap baking complete! Radiance and irradiance textures created.", m_envProbe->Id());
}

} // namespace hyperion
