/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <baking/Baker.hpp>
#include <baking/BakeJob.hpp>

#include <baking/lightmaps/LightmapAccelerationStructure.hpp>
#include <baking/lightmaps/LightmapPathTraceCpu.hpp>
#include <baking/lightmaps/LightmapPathTraceGpu.hpp>

#include <rendering/RenderInterface.hpp>
#include <rendering/RenderHelpers.hpp>
#include <rendering/RenderCollection.hpp>
#include <rendering/RenderBackend.hpp>
#include <rendering/RenderObject.hpp>
#include <rendering/RenderConfig.hpp>
#include <rendering/Device.hpp>
#include <rendering/Frame.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/Material.hpp>
#include <rendering/Texture.hpp>
#include <rendering/RendererBase.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <asset/TextureAsset.hpp>

#include <scene/BVH.hpp>
#include <scene/World.hpp>
#include <scene/Light.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/EnvGrid.hpp>
#include <scene/FogVolume.hpp>
#include <scene/View.hpp>
#include <scene/LightmapVolume.hpp>

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

#include <core/threading/TaskSystem.hpp>
#include <core/threading/TaskThread.hpp>

#include <core/utilities/Time.hpp>
#include <core/utilities/DeferredScope.hpp>
#include <core/utilities/Float16.hpp>

#include <core/math/Triangle.hpp>

#include <util/MeshBuilder.hpp>

#include <system/AppContext.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>

#include <Baker.generated.inl>

namespace Hyperion {

namespace Baking {

static constexpr uint32 TileSize = 32;
static constexpr uint32 IdealTexelsPerFrame = 1000000;
static constexpr double IdealGpuMemUsageMB = 1024 * 3;
static constexpr uint32 MaxConcurrentJobs = ~0u;

// for LightmapVolume gpu trace job
static inline double GetEstimatedGPUMemUsageForJob(const LightmapperConfig& config)
{
    double usage = 0.0;

    constexpr double RaysBufferSizeMB = (TileSize * TileSize * sizeof(Vec4f) * 2) / 1024.0 / 1024.0;
    constexpr double HitsBufferSizeMB = (TileSize * TileSize * sizeof(LightmapHit)) / 1024.0 / 1024.0;

    usage += RaysBufferSizeMB * config.numSamples * NumFramesInFlight;
    usage += HitsBufferSizeMB;

    return usage;
}

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

#pragma region BakerBase

BakerBase::BakerBase(LightmapperConfig&& config, ObjectBase* source, const Handle<Scene>& scene, const BoundingBox& aabb)
    : m_config(std::move(config)),
      m_source(source),
      m_scene(scene),
      m_aabb(aabb),
      m_threadPool(nullptr),
      m_numJobs(0),
      m_initialNumJobs(0),
      m_updateTimer { 1.0 } // every second
{
    AssertDebug(m_source != nullptr);
}

BakerBase::~BakerBase()
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

uint32 BakerBase::NumTexelSamples() const
{
    return m_config.numSamples;
}

uint32 BakerBase::MaxTexelsPerFrame() const
{
    if (ShouldSplitIntoJobs())
    {
        return TileSize * TileSize * NumTexelSamples();
    }
    else
    {
        const Vec2u dimensions = GetBakeData().dimensions.GetXY();
        AssertDebug(dimensions.Volume() > 0);

        return dimensions.Volume() * NumTexelSamples();
    }
}

bool BakerBase::IsComplete() const
{
    return m_numJobs == 0;
}

void BakerBase::Initialize()
{
    HYP_LOG(Lightmap, Info, "Initializing lightmapper: {}", m_config.ToString());

    if (PerformsRayTracing())
    {
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
                | ViewFlags::SKIP_ENV_GRIDS
                | ViewFlags::SKIP_LIGHTMAP_VOLUMES | ViewFlags::SKIP_PARTICLE_VOLUMES | ViewFlags::SKIP_FOG_VOLUMES
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
    }

    Initialize_Internal();

    Build();

    if (PerformsRayTracing())
    {
        /// If cpu path tracing, set up thread pool and stuff
        if (m_config.traceMode == LightmapTraceMode::CPU_PATH_TRACING)
        {
            BuildResourceCache();
            BuildAccelerationStructures();

            m_threadPool = new LightmapThreadPool();
            m_threadPool->Start();
        }

        CreateLightmapRenderers();
    }
}

BakeJobParams BakerBase::CreateLightmapJobParams(SizeType startIndex, SizeType endIndex)
{
    BakeJobParams jobParams {
        &m_config,
        m_scene,
        m_view,
        m_subElements.ToSpan().Slice(startIndex, endIndex - startIndex),
        &m_subElementsByEntity,
        &m_lightmapRenderers
    };

    return jobParams;
}

UniquePtr<ILightmapRenderer> BakerBase::CreateRenderer(LightmapShadingType shadingType, uint32 maxTexelsPerFrame)
{
    if (!PerformsRayTracing())
    {
        return nullptr;
    }

    switch (m_config.traceMode)
    {
    case LightmapTraceMode::GPU_PATH_TRACING:
        return MakeUnique<LightmapRenderer_GpuPathTracing>(this, m_scene, shadingType, maxTexelsPerFrame);
    case LightmapTraceMode::CPU_PATH_TRACING:
        return MakeUnique<LightmapRenderer_CpuPathTracing>(this, m_accelerationStructure.Get(), m_threadPool, m_scene, shadingType);
    default:
        HYP_UNREACHABLE();
    }
}

void BakerBase::CreateLightmapRenderers()
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

void BakerBase::BuildAccelerationStructures()
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
void BakerBase::BuildResourceCache()
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
        uint32(m_subElements.Size() + 255) / 256,
        m_subElements, callback);

    TaskSystem::GetInstance().EnqueueBatch(&taskBatch);

    while (!taskBatch.IsCompleted())
    {
        ThreadSleep(1000);

        Mutex::Guard guard(mtx);

        HYP_LOG(Lightmap, Debug, "Waiting for lightmapper resource cache to finish building... ({} resources discovered)", m_resourceCache.Size());
    }
}

void BakerBase::Build()
{
    HYP_SCOPE;
    const uint32 idealTrianglesPerJob = m_config.idealTrianglesPerJob;

    Assert(m_numJobs == 0, "Cannot initialize lightmap renderer -- jobs currently running!");

    // Build jobs
    HYP_LOG(Lightmap, Info, "Building graph for lightmapper");

    EntityManager& mgr = *m_scene->GetEntityManager();

    m_subElements.Clear();
    m_subElementsByEntity.Clear();

    const bool onlyOverlappingElements = OnlyOverlappingElements();

    for (auto [entity, meshComponent, transformComponent, boundingBoxComponent, _] : mgr.GetEntitySet<MeshComponent, TransformComponent, BoundingBoxComponent, TagComponent<EntityTag::MobStatic>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
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
            Transform(transformComponent.translation, transformComponent.scale, transformComponent.rotation).GetMatrix(),
            boundingBoxComponent.worldAabb });
    }

    // set pointers in map after pushing, so that the addresses are stable
    for (SizeType index = 0; index < m_subElements.Size(); index++)
    {
        LightmapSubElement& subElement = m_subElements[index];

        m_subElementsByEntity.Set(subElement.entity, &subElement);
    }

    if (Result buildInternalResult = Build_Internal(); buildInternalResult.HasError())
    {
        HYP_LOG(Lightmap, Error, "Baker build failed: {}", buildInternalResult.GetError().GetMessage());
        return;
    }

    DispatchJobs();
}

void BakerBase::DispatchJobs()
{
    if (ShouldSplitIntoJobs())
    {
        const BakeDataBase& bakeData = GetBakeData();

        const Vec3u dimensions = bakeData.dimensions;
        Assert(dimensions.Volume() > 0);

        const bool performsRayTracing = PerformsRayTracing();

        HashMap<Vec2i, Array<uint32>> tileBuckets;

        AssertDebug(bakeData.texels.Any());

        for (uint32 i = 0; i < bakeData.texels.Size(); i++)
        {
            const LightmapTexel& texel = bakeData.texels[i];

            if (performsRayTracing && !texel.pRay)
            {
                continue;
            }

            // calculate tile coord based on texel index
            const uint32 x = i % dimensions.x;
            const uint32 y = i / dimensions.y;

            const Vec2i tileCoord { int(x / TileSize), int(y / TileSize) };
            tileBuckets[tileCoord].PushBack(i);
        }

        HYP_LOG(Lightmap, Info, "Dispatching {} tile jobs for {} valid texels", tileBuckets.Size(), bakeData.texels.Size());

        for (auto& it : tileBuckets)
        {
            UniquePtr<BakeJobBase> job = CreateJob(CreateLightmapJobParams(0, m_subElements.Size()));
            Assert(job != nullptr);

            job->SetTexelIndices(std::move(it.second));
            AddJob(std::move(job));
        }
    }
    else
    {
        UniquePtr<BakeJobBase> job = CreateJob(CreateLightmapJobParams(0, m_subElements.Size()));
        Assert(job != nullptr);

        // all texels
        Array<uint32> allTexelIndices;
        allTexelIndices.Resize(GetBakeData().texels.Size());

        for (uint32 i = 0; i < allTexelIndices.Size(); i++)
        {
            allTexelIndices[i] = i;
        }

        job->SetTexelIndices(std::move(allTexelIndices));

        AddJob(std::move(job));
    }

    m_initialNumJobs = m_numJobs;
}

void BakerBase::Update(float delta)
{
    HYP_SCOPE;

    uint32 numTexelsProcessed = 0;
    uint32 numRunningJobs = 0;
    uint32 numProcessedJobs = 0;

    double gpuMemUsagePerJobMB = GetEstimatedGPUMemUsageForJob(m_config);
    AssertDebug(gpuMemUsagePerJobMB <= IdealGpuMemUsageMB);

    double currentGpuMemUsageMB = 0.0;

    Mutex::Guard guard(m_queueMutex);

    if (m_config.traceMode == LightmapTraceMode::GPU_PATH_TRACING)
    {
        // tally up estimated gpu mem usage
        for (auto it = m_queue.Begin(); it != m_queue.End(); ++it)
        {
            BakeJobBase* job = it->Get();

            if (job->IsRunning() || job->IsCompleted())
            {
                currentGpuMemUsageMB += gpuMemUsagePerJobMB;
                numRunningJobs++;
            }
        }
    }

    for (auto it = m_queue.Begin(); it != m_queue.End();)
    {
        BakeJobBase* job = it->Get();

        if (job->IsRunning())
        {
            numTexelsProcessed += job->Process(uint32(MathUtil::Max(0, int64(IdealTexelsPerFrame) - int64(numTexelsProcessed))));

            ++numProcessedJobs;
        }

        if (job->IsCompleted())
        {
            HandleCompletedJob(job);

            it = m_queue.Erase(it);

            --numRunningJobs;

            continue;
        }

        ++it;
    }

    // spin up new jobs
    for (auto it = m_queue.Begin(); it != m_queue.End();)
    {
        BakeJobBase* job = it->Get();

        if (!job->IsRunning() && !job->IsCompleted())
        {
            if (numRunningJobs < MaxConcurrentJobs
                && (m_config.traceMode != LightmapTraceMode::GPU_PATH_TRACING || currentGpuMemUsageMB + gpuMemUsagePerJobMB <= IdealGpuMemUsageMB))
            {
                job->Start();

                if (m_config.traceMode == LightmapTraceMode::GPU_PATH_TRACING)
                {
                    currentGpuMemUsageMB += gpuMemUsagePerJobMB;
                }

                numRunningJobs++;
            }
        }

        ++it;
    }
}

void BakerBase::HandleCompletedJob(BakeJobBase* job)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    HYP_DEFER({
        --m_numJobs;
    });

    if (job->GetResult().HasError())
    {
        HYP_LOG(Lightmap, Error, "Lightmap job {} failed with error: {}", job->GetUUID(), job->GetResult().GetError().GetMessage());

        return;
    }

    HandleCompletedJob_Internal(job);

    for (UniquePtr<ILightmapRenderer>& lightmapRenderer : m_lightmapRenderers)
    {
        lightmapRenderer->CleanJobData(job);
    }

    const int percentage = MathUtil::Floor(double(m_initialNumJobs - m_numJobs) / double(m_initialNumJobs) * 100.0);

    HYP_LOG(Lightmap, Info, "Baking {} ... ({}%)",
        m_source ? m_source->Id() : ObjIdBase(), percentage);
}

#pragma endregion BakerBase

} // namespace Baking

} // namespace Hyperion
