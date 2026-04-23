/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <baking/Baker.hpp>
#include <baking/BakeJob.hpp>
#include <baking/BakeData.hpp>
#include <baking/BakerThreadPool.hpp>

#include <baking/lightmaps/LightmapPathTraceGpu.hpp>

#include <rendering/RenderInterface.hpp>
#include <rendering/RenderHelpers.hpp>
#include <rendering/RenderCollection.hpp>
#include <rendering/RenderObject.hpp>
#include <rendering/RenderConfig.hpp>
#include <rendering/Device.hpp>
#include <rendering/Frame.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/MaterialInstance.hpp>
#include <rendering/Texture.hpp>
#include <rendering/RendererBase.hpp>

#include <rendering/util/DeletionQueue.hpp>

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

#include <Core/threading/TaskSystem.hpp>
#include <Core/threading/TaskThread.hpp>

#include <Core/utilities/Time.hpp>
#include <Core/utilities/DeferredScope.hpp>
#include <Core/utilities/Float16.hpp>

#include <Core/math/Triangle.hpp>

#include <util/MeshBuilder.hpp>

#include <system/AppContext.hpp>
#include <system/MessageBox.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>

#include <Baker.generated.inl>

namespace Hyperion {

namespace Baking {

// Changing tile size will change the number of jobs that get enqueued.
// Smaller tile size = more jobs required to complete the bake
static constexpr uint32 TileSize = 32;

// Too many concurrent jobs will cause excessive memory usage and thrashing
static constexpr uint32 MaxConcurrentJobs = 8;

// Not per frame per se, but during Update() we check if we can spin up more jobs,
// this will add a hard cap on number of texels beind process simultaneously (translates to rays
// traced, for lightmap jobs)
static constexpr uint32 IdealTexelsPerFrame = 200000;

// Pretty self explanatory - but this is an estimate (See the below function, GetEstimatedGPUMemUsagePerJob)
static constexpr double IdealGpuMemUsageMB = 1024 * 2;

static constexpr double ProgressWindowSeconds = 30.0;

// for LightmapVolume gpu trace job
static inline double GetEstimatedGPUMemUsageForJob(const BakerConfig& config)
{
    double usage = 0.0;

    constexpr double RaysBufferSizeMB = (TileSize * TileSize * sizeof(Vec4f) * 2) / 1024.0 / 1024.0;
    constexpr double HitsBufferSizeMB = (TileSize * TileSize * sizeof(LightmapHit)) / 1024.0 / 1024.0;

    usage += RaysBufferSizeMB * config.numSamples * NumFramesInFlight;
    usage += HitsBufferSizeMB;

    return usage;
}

static void EmptyViewCollectFunction(RenderProxyList&)
{
}

#pragma region BakerBase

BakerBase::BakerBase(BakerConfig&& config, ObjectBase* source, const Handle<Scene>& scene, const BoundingBox& aabb)
    : m_config(std::move(config)),
      m_source(source),
      m_scene(scene),
      m_aabb(aabb),
      m_threadPool(nullptr),
      m_numJobs(0),
      m_initialNumJobs(0),
      m_updateTimer { 1.0 }, // every second
      m_lastProgressPercent(0.0),
      m_isComplete(false)
{
    AssertDebug(m_source != nullptr);
}

BakerBase::~BakerBase()
{
    m_queue.Clear();

    if (m_view != nullptr)
    {
        EnqueueDeletion(std::move(m_view));
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

void BakerBase::Initialize()
{
    if (PerformsRayTracing())
    {
        if (!g_renderInterface->GetRenderConfig().rayTracing)
        {
            SystemMessageBox(MessageBoxType::CRITICAL)
                .Title("Ray tracing must be enabled")
                .Text("This baking technique requires support for ray tracing which doesn't appear to be supported on this device (or it has been explicitly disabled via config).")
                .Show();
        }
        
        Handle<Camera> camera = MakeHandle<Camera>();
        camera->SetName(NAME_FMT("{}_Camera", InstanceClass()->GetName()));
        camera->AddCameraController(MakeHandle<OrthoCameraController>());
        camera->SetFarClip(1000.0f);
        InitObject(camera);

        // dummy output target
        FramebufferDesc framebufferDesc;
        framebufferDesc.extent = Vec2u::One();
        framebufferDesc.attachments[0] = { TextureType::Texture2D, TextureFormat::R8 };
        framebufferDesc.numAttachments = 1;

        BoundingBox bounds;
        
        if (OnlyOverlappingElements())
        {
            bounds = m_aabb;

            if (!bounds.IsValid() && m_source->IsA(VolumeBase::StaticClass()))
            {
                VolumeBase* volume = static_cast<VolumeBase*>(m_source);
                bounds = volume->GetWorldBounds();
            }
        }

        ViewDesc viewDesc {
            .flags = ViewFlags::BAKER_VIEW
                |  ViewFlags::COLLECT_STATIC_ENTITIES
                | ViewFlags::NO_FRUSTUM_CULLING
                | ViewFlags::SKIP_ENV_GRIDS
                | ViewFlags::SKIP_LIGHTMAP_VOLUMES | ViewFlags::SKIP_PARTICLE_VOLUMES | ViewFlags::SKIP_FOG_VOLUMES
                | ViewFlags::RAY_TRACING
                | ViewFlags::NO_DRAW_CALLS
                | ViewFlags::NOT_MULTI_BUFFERED,
            .framebufferDesc = framebufferDesc,
            .scenes = { m_scene },
            .camera = camera,
            .bounds = bounds
        };

        const Array<Handle<Scene>>& scenes = m_scene->GetWorld()->GetScenes();

        // Add backdrop scenes
        for (const Handle<Scene>& scene : scenes)
        {
            if ((scene->GetSceneFlags() & SceneFlags::BACKDROP) && !viewDesc.scenes.Contains(scene.Get()))
            {
                viewDesc.scenes.PushBack(scene.Get());
            }
        }

        m_view = MakeHandle<View>(viewDesc);
        m_view->SetName(NAME_FMT("{}_View", InstanceClass()->GetName()));
        InitObject(m_view);

        m_view->UpdateViewport();
        m_view->UpdateVisibility();
        m_view->CollectSync();

        // don't want to collect again, just keep the view data around until we're done with it.
        m_view->SetOverrideCollectFunctor(&EmptyViewCollectFunction);
    }

    Initialize_Internal();

    Build();

    if (PerformsRayTracing())
    {
        CreateLightmapRenderers();
    }

    if (NumThreads() > 0)
    {
        m_threadPool = new BakerThreadPool(GetInnerType(), NumThreads());
        m_threadPool->Start();
    }
}

BakeJobParams BakerBase::CreateLightmapJobParams(size_t startIndex, size_t endIndex)
{
    BakeJobParams jobParams {
        &m_config,
        m_scene,
        m_view,
        m_bakeEntities.ToSpan().Slice(startIndex, endIndex - startIndex),
        &m_bakeEntitiesByEntity,
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

    if (!g_renderInterface->GetRenderConfig().rayTracing)
    {
        HYP_LOG(Lightmap, Error, "GPU path tracing is not supported on this device");

        return nullptr;
    }

    return MakeUnique<LightmapRenderer_GpuPathTracing>(this, m_scene, shadingType, maxTexelsPerFrame);
}

void BakerBase::CreateLightmapRenderers()
{
}

void BakerBase::Build()
{
    HYP_SCOPE;
    
    Assert(m_numJobs == 0, "Cannot initialize lightmap renderer -- jobs currently running!");

    // Build jobs
    HYP_LOG(Lightmap, Verbose, "Building graph for lightmapper");

    EntityManager& mgr = *m_scene->GetEntityManager();

    m_bakeEntities.Clear();
    m_bakeEntitiesByEntity.Clear();

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
        if (meshComponent.material->GetBucket() != RenderBucket::Opaque
            && meshComponent.material->GetBucket() != RenderBucket::Lightmapped
            && meshComponent.material->GetBucket() != RenderBucket::Translucent)
        {
            continue;
        }

        const BoundingBox& worldAabb = boundingBoxComponent.worldAabb;

        if (onlyOverlappingElements && !m_aabb.Overlaps(worldAabb))
        {
         //   continue; // must be inside volume to be considered
        }

        m_bakeEntities.PushBack(BakeEntity {
            MakeStrongRef(entity),
            meshComponent.mesh,
            meshComponent.material,
            Transform(transformComponent.translation, transformComponent.scale, transformComponent.rotation).GetMatrix(),
            boundingBoxComponent.worldAabb
        });
    }

    // set pointers in map after pushing, so that the addresses are stable
    for (size_t index = 0; index < m_bakeEntities.Size(); index++)
    {
        BakeEntity& bakeEntity = m_bakeEntities[index];

        m_bakeEntitiesByEntity.Set(bakeEntity.entity, &bakeEntity);
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

            const Vec2i tileCoord = Vec2i(int32(x / TileSize), int32(y / TileSize));
            tileBuckets[tileCoord].PushBack(i);
        }

        HYP_LOG(Lightmap, Verbose, "Dispatching {} tile jobs for {} valid texels", tileBuckets.Size(), bakeData.texels.Size());

        for (auto& it : tileBuckets)
        {
            UniquePtr<BakeJobBase> job = CreateJob(CreateLightmapJobParams(0, m_bakeEntities.Size()));
            Assert(job != nullptr);

            job->SetTexelIndices(std::move(it.second));
            AddJob(std::move(job));
        }
    }
    else
    {
        UniquePtr<BakeJobBase> job = CreateJob(CreateLightmapJobParams(0, m_bakeEntities.Size()));
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

    m_isComplete = false;

    m_initialNumJobs = m_numJobs;

    m_bakingClock.Start();
    m_lastProgressPercent = 0.0;
    m_progressSamples.Clear();
}

void BakerBase::Update(float delta)
{
    HYP_SCOPE;

    uint32 numTexelsProcessed = 0;
    uint32 numRunningJobs = 0;

    double gpuMemUsagePerJobMB = GetEstimatedGPUMemUsageForJob(m_config);
    AssertDebug(gpuMemUsagePerJobMB <= IdealGpuMemUsageMB);

    double currentGpuMemUsageMB = 0.0;

    Mutex::Guard guard(m_queueMutex);

    if (m_queue.Empty())
    {
        OnCompleted();

        return;
    }

    if (PerformsRayTracing())
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
        }

        if (job->IsCompleted())
        {
            HandleCompletedJob(job);

            it = m_queue.Erase(it);

            --numRunningJobs;

            if (m_queue.Empty())
            {
                return;
            }

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
                && (!PerformsRayTracing() || currentGpuMemUsageMB + gpuMemUsagePerJobMB <= IdealGpuMemUsageMB))
            {
                job->Start();

                if (PerformsRayTracing())
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

    const double progressPercent = double(m_initialNumJobs - m_numJobs) / double(m_initialNumJobs) * 100.0;
    const int percentage = MathUtil::Floor(progressPercent);

    const double elapsedMs = m_bakingClock.ElapsedMs();
    const double elapsedSeconds = elapsedMs / 1000.0;

    // elapsed, percent
    m_progressSamples.EmplaceBack(elapsedSeconds, progressPercent);

    // Remove samples outside the window
    const double windowStartTime = elapsedSeconds - ProgressWindowSeconds;
    while (m_progressSamples.Size() > 1 && m_progressSamples.Front().first < windowStartTime)
    {
        m_progressSamples.PopFront();
    }

    if (MathUtil::Floor(progressPercent - m_lastProgressPercent) >= 1)
    {
        String timeEstimateStr;
        if (m_progressSamples.Size() >= 2)
        {
            // Calculate progress rate using windowed samples
            const auto& oldestSample = m_progressSamples.Front();
            const auto& newestSample = m_progressSamples.Back();

            const double deltaTime = newestSample.first - oldestSample.first;
            const double deltaProgress = newestSample.second - oldestSample.second;

            if (deltaTime > 0.1 && deltaProgress > 0.001)
            {
                const double progressPerSecond = deltaProgress / deltaTime;
                const double remainingProgress = 100.0 - progressPercent;
                const double remainingSeconds = remainingProgress / progressPerSecond;

                if (remainingSeconds >= 60.0)
                {
                    const int remainingMinutes = int(remainingSeconds / 60.0);
                    const int remainingSecs = int(remainingSeconds) % 60;
                    timeEstimateStr = HYP_FORMAT("~{}m {}s", remainingMinutes, remainingSecs);
                }
                else
                {
                    timeEstimateStr = HYP_FORMAT("~{}s", int(remainingSeconds));
                }
            }
            else
            {
                timeEstimateStr = "calculating time";
            }
        }
        else
        {
            timeEstimateStr = "calculating time";
        }

        String elapsedStr;
        if (elapsedSeconds >= 60.0)
        {
            const int elapsedMinutes = int(elapsedSeconds / 60.0);
            const int elapsedSecs = int(elapsedSeconds) % 60;
            elapsedStr = HYP_FORMAT("{}m {}s", elapsedMinutes, elapsedSecs);
        }
        else
        {
            elapsedStr = HYP_FORMAT("{}s", int(elapsedSeconds));
        }

        HYP_LOG(Lightmap, Info, "Baking {} ... ({}%) - Elapsed: {}, {} remaining", m_source ? m_source->Id() : ObjIdBase(), percentage, elapsedStr, timeEstimateStr);

        m_lastProgressPercent = progressPercent;
    }
}

void BakerBase::OnCompleted()
{
    m_bakingClock.Stop();
    m_isComplete = true;

    OnCompleted_Internal();

    OnComplete();

    const double totalElapsedMs = m_bakingClock.ElapsedMs();
    const double totalElapsedSeconds = totalElapsedMs / 1000.0;

    String totalElapsedStr;
    if (totalElapsedSeconds >= 60.0)
    {
        const int totalMinutes = int(totalElapsedSeconds / 60.0);
        const int totalSecs = int(totalElapsedSeconds) % 60;
        totalElapsedStr = HYP_FORMAT("{}m {}s", totalMinutes, totalSecs);
    }
    else
    {
        totalElapsedStr = HYP_FORMAT("{}s", int(totalElapsedSeconds));
    }

    // mark the source object dirty if it is an AssetObject 
    if (m_source->IsA(AssetObject::StaticClass()))
    {
        static_cast<AssetObject*>(m_source)->MarkDirty();
    }

    HYP_LOG(Lightmap, Info, "Baking complete for {} - Total time: {}", m_source ? m_source->Id() : ObjIdBase(), totalElapsedStr);
}

void BakerBase::AddJob(UniquePtr<BakeJobBase>&& job)
{
    if (!job)
    {
        return;
    }

    job->m_lightmapper = this;

    Mutex::Guard guard(m_queueMutex);
    m_queue.PushBack(std::move(job));

    ++m_numJobs;
}

#pragma endregion BakerBase

} // namespace Baking

} // namespace Hyperion
