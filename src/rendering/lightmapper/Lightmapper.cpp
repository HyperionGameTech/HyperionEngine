/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <rendering/lightmapper/Lightmapper.hpp>
#include <rendering/lightmapper/LightmapPathTraceCpu.hpp>
#include <rendering/lightmapper/LightmapPathTraceGpu.hpp>

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

#include <asset/TextureAsset.hpp>

#include <scene/BVH.hpp>
#include <scene/World.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/Light.hpp>
#include <scene/EnvGrid.hpp>
#include <scene/View.hpp>

#include <scene/util/VoxelOctree.hpp>

#include <scene/lightmapper/LightmapVolume.hpp>

#include <scene/camera/Camera.hpp>
#include <scene/camera/OrthoCamera.hpp>

#include <scene/EntityManager.hpp>
#include <scene/components/MeshComponent.hpp>
#include <scene/components/TransformComponent.hpp>
#include <scene/components/BoundingBoxComponent.hpp>

#include <core/object/HypClass.hpp>

#include <core/threading/TaskSystem.hpp>
#include <core/threading/TaskThread.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/utilities/Time.hpp>
#include <core/utilities/DeferredScope.hpp>

#include <core/math/Triangle.hpp>

#include <util/Float16.hpp>
#include <util/MeshBuilder.hpp>

#include <system/AppContext.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>

namespace hyperion {

#pragma region Render commands

struct RENDER_COMMAND(LightmapRender)
    : RenderCommand
{
    LightmapJob* job;
    Handle<World> world;
    Handle<View> view;
    Array<LightmapRay> rays;
    uint32 rayOffset;

    RENDER_COMMAND(LightmapRender)(LightmapJob* job, const Handle<World>& world, const Handle<View>& view, Array<LightmapRay>&& rays, uint32 rayOffset)
        : job(job),
          world(world),
          view(view),
          rays(std::move(rays)),
          rayOffset(rayOffset)
    {
        job->numConcurrentRenderingTasks.Increment(1, MemoryOrder::RELEASE);
    }

    virtual ~RENDER_COMMAND(LightmapRender)() override
    {
        job->numConcurrentRenderingTasks.Decrement(1, MemoryOrder::RELEASE);
    }

    virtual RendererResult operator()() override
    {
        FrameBase* frame = g_renderBackend->GetCurrentFrame();

        RenderSetup renderSetup { world, view };

        RenderProxyList* rpl = nullptr;

        if (view)
        {
            rpl = &RenderApi_GetConsumerProxyList(view);
        }

        if (rpl)
        {
            rpl->BeginRead();
        }

        HYP_DEFER({ if (rpl) rpl->EndRead(); });

        if (rpl)
        {
            if (const auto& skyProbes = rpl->GetEnvProbes().GetElements<SkyProbe>(); skyProbes.Any())
            {
                renderSetup.envProbe = skyProbes.Front();
            }
        }

        {
            // Read ray hits from last time this frame was rendered
            Array<LightmapRay> previousRays;
            job->GetPreviousFrameRays(previousRays);

            // Read previous frame hits into CPU buffer
            if (previousRays.Size() != 0)
            {
                Array<LightmapHit> hitsBuffer;
                hitsBuffer.Resize(previousRays.Size());

                AssertDebug(job->GetParams().renderers != nullptr);

                for (UniquePtr<ILightmapRenderer>& lightmapRenderer : *job->GetParams().renderers)
                {
                    AssertDebug(lightmapRenderer != nullptr);

                    lightmapRenderer->ReadHitsBuffer(frame, hitsBuffer);

                    job->IntegrateRayHits(previousRays, hitsBuffer, lightmapRenderer->GetShadingType());
                }
            }

            job->SetPreviousFrameRays(rays);
        }

        if (rays.Any())
        {
            AssertDebug(job->GetParams().renderers != nullptr);

            for (UniquePtr<ILightmapRenderer>& lightmapRenderer : *job->GetParams().renderers)
            {
                AssertDebug(lightmapRenderer != nullptr);

                lightmapRenderer->Render(frame, renderSetup, job, rays, rayOffset);
            }
        }

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

#pragma region LightmapperConfig

void LightmapperConfig::PostLoadCallback()
{
    if (traceMode == LightmapTraceMode::GPU_PATH_TRACING)
    {
        if (!g_renderBackend->GetRenderConfig().IsRaytracingSupported())
        {
            traceMode = LightmapTraceMode::CPU_PATH_TRACING;

            HYP_LOG(Lightmap, Warning, "GPU path tracing is not supported on this device. Falling back to CPU path tracing.");
        }
    }
}

#pragma endregion LightmapperConfig

#pragma region LightmapJob

static constexpr uint32 g_maxConcurrentRenderingTasksPerJob = 1;

LightmapJob::LightmapJob(LightmapJobParams&& params)
    : m_params(std::move(params)),
      m_texelIndex(0),
      m_lightmapElement(nullptr),
      m_lastLoggedPercentage(0),
      numConcurrentRenderingTasks(0)
{
}

LightmapJob::~LightmapJob()
{
    for (TaskBatch* taskBatch : m_currentTasks)
    {
        taskBatch->AwaitCompletion();

        delete taskBatch;
    }
    
    delete m_lightmapElement;
}

void LightmapJob::Start()
{
    m_runningSemaphore.Produce(1, [this](bool)
        {
            if (!m_uvMap.HasValue())
            {
                // No elements to process
                if (!m_params.subElementsView)
                {
                    m_uvMap = LightmapUVMap {};

                    return;
                }

                HYP_LOG(Lightmap, Info, "Lightmap job {}: Enqueue task to build UV map", m_uuid);

                m_uvBuilder = LightmapUVBuilder { { m_params.subElementsView } };

                m_buildUvMapTask = TaskSystem::GetInstance().Enqueue([this]() -> TResult<LightmapUVMap>
                    {
                        return m_uvBuilder.Build();
                    },
                    TaskThreadPoolName::THREAD_POOL_BACKGROUND);
            }
        });
}

void LightmapJob::Stop()
{
    m_runningSemaphore.Release(1);
}

void LightmapJob::Stop(const Error& error)
{
    HYP_LOG(Lightmap, Error, "Lightmap job {} stopped with error: {}", m_uuid, error.GetMessage());

    m_result = error;

    Stop();
}

bool LightmapJob::IsCompleted() const
{
    return !m_runningSemaphore.IsInSignalState();
}

void LightmapJob::AddTask(TaskBatch* taskBatch)
{
    Mutex::Guard guard(m_currentTasksMutex);

    m_currentTasks.PushBack(taskBatch);
}

HYP_DISABLE_OPTIMIZATION;
void LightmapJob::Process()
{
    Assert(IsRunning());
    Assert(!m_result.HasError(), "Unhandled error in lightmap job: {}", *m_result.GetError().GetMessage());

    if (!m_uvMap.HasValue())
    {
        // wait for uv map to finish building

        // If uv map is not valid, it must have a task that is building it
        Assert(m_buildUvMapTask.IsValid());

        if (!m_buildUvMapTask.IsCompleted())
        {
            // return early so we don't block - we need to wait for build task to complete before processing
            return;
        }

        if (TResult<LightmapUVMap>& uvMapResult = m_buildUvMapTask.Await(); uvMapResult.HasValue())
        {
            m_uvMap = std::move(*uvMapResult);
        }
        else
        {
            Stop(uvMapResult.GetError());

            return;
        }

        if (m_uvMap.HasValue())
        {
            LightmapElement lightmapElement;
            if (!m_params.volume->AddElement(*m_uvMap, lightmapElement, /* shrinkToFit */ true, /* downscaleLimit */ 0.1f))
            {
                Stop(HYP_MAKE_ERROR(Error, "Failed to add LightmapElement to LightmapVolume for lightmap job {}! UV map size: {}",
                    m_uuid, Vec2u(m_uvMap->width, m_uvMap->height)));

                return;
            }

            if (m_lightmapElement != nullptr)
            {
                *m_lightmapElement = std::move(lightmapElement);
            }
            else
            {
                m_lightmapElement = new LightmapElement(std::move(lightmapElement));
            }

            // Flatten texel indices, grouped by mesh IDs to prevent unnecessary loading/unloading
            m_texelIndices.Reserve(m_uvMap->uvs.Size());

            for (const auto& it : m_uvMap->meshToUvIndices)
            {
                m_texelIndices.Concat(it.second);
            }

            // Free up memory
            m_uvMap->meshToUvIndices.Clear();
        }
        else
        {
            // Mark as ready to stop further processing
            Stop(HYP_MAKE_ERROR(Error, "Failed to build UV map for lightmap job {}", m_uuid));
        }

        return;
    }

    View* view = m_params.view;
    Assert(view != nullptr);

    view->UpdateViewport();
    view->UpdateVisibility();
    view->CollectSync();
    
    if (numConcurrentRenderingTasks.Get(MemoryOrder::ACQUIRE) >= g_maxConcurrentRenderingTasksPerJob)
    {
        // Wait for current rendering tasks to complete before enqueueing new ones.

        return;
    }

    {
        Mutex::Guard guard(m_currentTasksMutex);

        if (m_currentTasks.Any())
        {
            for (SizeType taskIndex = 0; taskIndex < m_currentTasks.Size(); taskIndex++)
            {
                TaskBatch* taskBatch = m_currentTasks[taskIndex];

                if (!taskBatch->IsCompleted())
                {
                    // Skip this call

                    return;
                }
            }

            for (SizeType taskIndex = 0; taskIndex < m_currentTasks.Size(); taskIndex++)
            {
                TaskBatch* taskBatch = m_currentTasks[taskIndex];

                taskBatch->AwaitCompletion();

                delete taskBatch;
            }

            m_currentTasks.Clear();
        }
    }

    bool hasRemainingRays = false;

    {
        Mutex::Guard guard(m_previousFrameRaysMutex);

        if (m_previousFrameRays.Any())
        {
            hasRemainingRays = true;
        }
    }

    if (!hasRemainingRays
        && m_texelIndex >= m_texelIndices.Size() * m_params.config->numSamples
        && numConcurrentRenderingTasks.Get(MemoryOrder::ACQUIRE) == 0)
    {
        HYP_LOG(Lightmap, Debug, "Lightmap job {}: All texels processed ({} / {}), stopping", m_uuid, m_texelIndex, m_texelIndices.Size() * m_params.config->numSamples);

        Stop();

        return;
    }

    AssertDebug(m_params.renderers != nullptr);
    Array<UniquePtr<ILightmapRenderer>>& lightmapRenderers = *m_params.renderers;

    AssertDebug(lightmapRenderers[0] != nullptr);

    for (UniquePtr<ILightmapRenderer>& lightmapRenderer : lightmapRenderers)
    {
        AssertDebug(lightmapRenderer != nullptr);

        if (!lightmapRenderer->CanRender())
        {
            HYP_LOG(Lightmap, Info, "Waiting for lightmap renderers to be ready...");

            return;
        }
    }

    const SizeType maxRays = MathUtil::Min(lightmapRenderers[0]->MaxRaysPerFrame(), m_params.config->maxRaysPerFrame);

    Array<LightmapRay> rays;
    rays.Reserve(maxRays);

    GatherRays(maxRays, rays);

    for (UniquePtr<ILightmapRenderer>& lightmapRenderer : lightmapRenderers)
    {
        lightmapRenderer->UpdateRays(rays);
    }

    const double percentage = double(m_texelIndex) / double(m_texelIndices.Size() * m_params.config->numSamples) * 100.0;

    if (MathUtil::Abs(MathUtil::Floor(percentage) - MathUtil::Floor(m_lastLoggedPercentage)) >= 1)
    {
        HYP_LOG(Lightmap, Debug, "Lightmap job {}: Texel {} / {} ({}%)",
            m_uuid.ToString(), m_texelIndex, m_texelIndices.Size() * m_params.config->numSamples, percentage);

        m_lastLoggedPercentage = percentage;
    }

    World* world = GetScene()->GetWorld();
    Assert(world != nullptr);

    const uint32 rayOffset = uint32(m_texelIndex % (m_texelIndices.Size() * m_params.config->numSamples));

    PUSH_RENDER_COMMAND(LightmapRender, this, MakeStrongRef(world), m_params.view, std::move(rays), rayOffset);
}
HYP_ENABLE_OPTIMIZATION;

#pragma endregion LightmapJob

#pragma region Lightmapper

Lightmapper::Lightmapper(LightmapperConfig&& config, const Handle<Scene>& scene, const BoundingBox& aabb)
    : m_config(std::move(config)),
      m_scene(scene),
      m_aabb(aabb),
      m_numJobs { 0 }
{
}

Lightmapper::~Lightmapper()
{
    m_lightmapRenderers = {};

    m_queue.Clear();

    if (m_view != nullptr)
    {
        m_scene->GetWorld()->RemoveView(m_view);
        m_view.Reset();
    }
}

bool Lightmapper::IsComplete() const
{
    return m_numJobs.Get(MemoryOrder::ACQUIRE) == 0;
}

void Lightmapper::Initialize()
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
            | ViewFlags::SKIP_ENV_GRIDS | ViewFlags::SKIP_LIGHTMAP_VOLUMES
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

    m_volume = CreateObject<LightmapVolume>(m_aabb);
    m_volume->SetName(Name::Unique("LightmapVolume"));
    InitObject(m_volume);

    m_volume->AddComponent<BoundingBoxComponent>(BoundingBoxComponent { m_aabb, m_aabb });

    m_scene->GetRoot()->AddChild(m_volume);

    Initialize_Internal();

    Build();

    for (uint32 i = 0; i < uint32(LightmapShadingType::MAX); i++)
    {
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
        default:
            HYP_UNREACHABLE();
        }

        UniquePtr<ILightmapRenderer>& lightmapRenderer = m_lightmapRenderers.EmplaceBack();
        lightmapRenderer = CreateRenderer(LightmapShadingType(i));

        if (!lightmapRenderer)
        {
            continue;
        }

        lightmapRenderer->Create();
    }

    Assert(m_lightmapRenderers.Any());
}

LightmapJobParams Lightmapper::CreateLightmapJobParams(SizeType startIndex, SizeType endIndex)
{
    AssertDebug(m_volume != nullptr);

    LightmapJobParams jobParams {
        &m_config,
        m_scene,
        m_volume,
        m_view,
        m_subElements.ToSpan().Slice(startIndex, endIndex - startIndex),
        &m_subElementsByEntity,
        &m_lightmapRenderers
    };

    return jobParams;
}

void Lightmapper::Build()
{
    HYP_SCOPE;
    const uint32 idealTrianglesPerJob = m_config.idealTrianglesPerJob;

    Assert(m_numJobs.Get(MemoryOrder::ACQUIRE) == 0, "Cannot initialize lightmap renderer -- jobs currently running!");

    // Build jobs
    HYP_LOG(Lightmap, Info, "Building graph for lightmapper");

    EntityManager& mgr = *m_scene->GetEntityManager();

    m_subElements.Clear();
    m_subElementsByEntity.Clear();

    for (auto [entity, meshComponent, transformComponent, boundingBoxComponent] : mgr.GetEntitySet<MeshComponent, TransformComponent, BoundingBoxComponent>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
    {
        if (entity->InstanceClass() != Entity::Class())
        {
            // skip non-Entity instances (we only want Entities with MeshComponent)
            continue;
        }

        if (!meshComponent.mesh.IsValid())
        {
            HYP_LOG(Lightmap, Info, "Skip entity with invalid mesh on MeshComponent");

            continue;
        }

        if (!meshComponent.material.IsValid())
        {
            HYP_LOG(Lightmap, Info, "Skip entity with invalid material on MeshComponent");

            continue;
        }

        // Only process opaque and translucent materials
        if (meshComponent.material->GetBucket() != RB_OPAQUE && meshComponent.material->GetBucket() != RB_TRANSLUCENT)
        {
            HYP_LOG(Lightmap, Info, "Skip entity with bucket that is not opaque or translucent");

            continue;
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

    for (SizeType index = 0; index < m_subElements.Size(); index++)
    {
        LightmapSubElement& subElement = m_subElements[index];

        m_subElementsByEntity.Set(subElement.entity, &subElement);

        if (idealTrianglesPerJob != 0 && numTriangles != 0 && numTriangles + subElement.mesh->NumIndices() / 3 > idealTrianglesPerJob)
        {
            UniquePtr<LightmapJob> job = CreateJob(CreateLightmapJobParams(startIndex, index + 1));

            startIndex = index + 1;

            AddJob(std::move(job));

            numTriangles = 0;
        }

        numTriangles += subElement.mesh->NumIndices() / 3;
    }

    if (startIndex < m_subElements.Size() - 1)
    {
        UniquePtr<LightmapJob> job = CreateJob(CreateLightmapJobParams(startIndex, m_subElements.Size()));

        AddJob(std::move(job));
    }
}

void Lightmapper::Update(float delta)
{
    HYP_SCOPE;

    uint32 numJobs = m_numJobs.Get(MemoryOrder::ACQUIRE);

    Mutex::Guard guard(m_queueMutex);

    Assert(!m_queue.Empty());
    LightmapJob* job = m_queue.Front().Get();

    // Start job if not started
    if (!job->IsRunning())
    {
        job->Start();
    }

    job->Process();

    if (job->IsCompleted())
    {
        HandleCompletedJob(job);
    }
}

void Lightmapper::HandleCompletedJob(LightmapJob* job)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread);

    HYP_DEFER({
        m_queue.Pop();
        m_numJobs.Decrement(1, MemoryOrder::RELEASE);
    });

    if (job->GetResult().HasError())
    {
        HYP_LOG(Lightmap, Error, "Lightmap job {} failed with error: {}", job->GetUUID(), job->GetResult().GetError().GetMessage());

        return;
    }

    HYP_LOG(Lightmap, Debug, "Tracing completed for lightmapping job {} ({} subelements)", job->GetUUID(), job->GetSubElements().Size());

    const LightmapUVMap& uvMap = job->GetUVMap();

    LightmapElement* lightmapElement = job->GetLightmapElement();

    if (lightmapElement == nullptr)
    {
        HYP_LOG(Lightmap, Debug, "Lightmap element is null, skipping building LightmapElement textures for job {}", job->GetUUID());

        return;
    }

    if (!m_volume->BuildElementTextures(uvMap, lightmapElement->id))
    {
        HYP_LOG(Lightmap, Error, "Failed to build LightmapElement textures for LightmapVolume, element id: {}", lightmapElement->id);

        return;
    }

    HYP_LOG(Lightmap, Debug, "Lightmap job {}: Building element with id {}, UV offset: {}, Scale: {}", job->GetUUID(), lightmapElement->id,
        lightmapElement->offsetUv, lightmapElement->scale);

    for (SizeType subElementIndex = 0; subElementIndex < job->GetSubElements().Size(); subElementIndex++)
    {
        LightmapSubElement& subElement = job->GetSubElements()[subElementIndex];

        auto updateMeshData = [&]()
        {
            const Handle<Mesh>& mesh = subElement.mesh;
            Assert(mesh.IsValid());

            Assert(subElementIndex < job->GetUVBuilder().GetMeshData().Size());

            const LightmapMeshData& lightmapMeshData = job->GetUVBuilder().GetMeshData()[subElementIndex];
            Assert(lightmapMeshData.mesh == mesh);

            MeshData newMeshData;
            newMeshData.desc.meshAttributes = mesh->GetMeshAttributes();
            newMeshData.desc.numVertices = uint32(lightmapMeshData.vertices.Size());
            newMeshData.desc.numIndices = uint32(lightmapMeshData.indices.Size());
            newMeshData.vertexData = lightmapMeshData.vertices;
            newMeshData.indexData = ByteBuffer(lightmapMeshData.indices.ToByteView());

            for (SizeType i = 0; i < newMeshData.vertexData.Size(); i++)
            {
                Vec2f& lightmapUv = newMeshData.vertexData[i].texcoord1;
                lightmapUv.y = 1.0f - lightmapUv.y; // Invert Y coordinate for lightmaps
                lightmapUv *= lightmapElement->scale;
                lightmapUv += Vec2f(lightmapElement->offsetUv.x, lightmapElement->offsetUv.y);
            }

            mesh->SetMeshData(newMeshData);
        };

        updateMeshData();

        bool isNewMaterial = false;

        subElement.material = subElement.material.IsValid() ? subElement.material->Clone() : CreateObject<Material>();
        isNewMaterial = true;

        subElement.material->SetBucket(RB_LIGHTMAP);

        subElement.material->SetTexture(MaterialTextureKey::IRRADIANCE_MAP, m_volume->GetAtlasTexture(lightmapElement->GetAtlasIndex(), LTT_IRRADIANCE));
        subElement.material->SetTexture(MaterialTextureKey::RADIANCE_MAP, m_volume->GetAtlasTexture(lightmapElement->GetAtlasIndex(), LTT_RADIANCE));

        auto updateMeshComponent = [entityManagerWeak = MakeWeakRef(m_scene->GetEntityManager()), lightmapElementId = lightmapElement->id, volume = m_volume, subElement = subElement, newMaterial = (isNewMaterial ? subElement.material : Handle<Material>::empty)]()
        {
            Handle<EntityManager> entityManager = entityManagerWeak.Lock();

            if (!entityManager)
            {
                HYP_LOG(Lightmap, Error, "Failed to lock EntityManager while updating lightmap element");

                return;
            }

            const Handle<Entity>& entity = subElement.entity;

            if (entityManager->HasComponent<MeshComponent>(entity))
            {
                MeshComponent& meshComponent = entityManager->GetComponent<MeshComponent>(entity);

                if (newMaterial.IsValid())
                {
                    InitObject(newMaterial);

                    meshComponent.material = std::move(newMaterial);
                }

                meshComponent.lightmapVolume = volume.ToWeak();
                meshComponent.lightmapElementId = lightmapElementId;
                meshComponent.lightmapVolumeUuid = volume->GetUUID();
            }
            else
            {
                Assert(newMaterial.IsValid());
                InitObject(newMaterial);

                MeshComponent meshComponent {};
                meshComponent.mesh = subElement.mesh;
                meshComponent.material = newMaterial;
                meshComponent.lightmapVolume = volume.ToWeak();
                meshComponent.lightmapElementId = lightmapElementId;
                meshComponent.lightmapVolumeUuid = volume->GetUUID();

                entityManager->AddComponent<MeshComponent>(entity, std::move(meshComponent));
            }

            entityManager->AddTag<EntityTag::UPDATE_RENDER_PROXY>(entity);
        };

        if (Threads::IsOnThread(m_scene->GetEntityManager()->GetOwnerThreadId()))
        {
            // If we are on the same thread, we can update the mesh component immediately
            updateMeshComponent();
        }
        else
        {
            // Enqueue the update to be performed on the owner thread
            ThreadBase* thread = Threads::GetThread(m_scene->GetEntityManager()->GetOwnerThreadId());
            Assert(thread != nullptr);

            thread->GetScheduler().Enqueue(std::move(updateMeshComponent), TaskEnqueueFlags::FIRE_AND_FORGET);
        }
    }

}

#pragma endregion Lightmapper

} // namespace hyperion
