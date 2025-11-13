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

#pragma region Render commands

#pragma endregion Render commands

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
    m_lightmapRenderers = {};

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

LightmapJobParams LightmapperBase::CreateLightmapJobParams(SizeType startIndex, SizeType endIndex)
{
    LightmapJobParams jobParams {
        &m_config,
        m_scene,
        m_view,
        m_subElements.ToSpan().Slice(startIndex, endIndex - startIndex),
        &m_subElementsByEntity,
        &m_lightmapRenderers
    };

    return jobParams;
}

UniquePtr<ILightmapRenderer> LightmapperBase::CreateRenderer(LightmapShadingType shadingType)
{
    switch (m_config.traceMode)
    {
    case LightmapTraceMode::GPU_PATH_TRACING:
        return MakeUnique<LightmapRenderer_GpuPathTracing>(this, m_scene, shadingType);
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

    for (auto [entity, meshComponent, transformComponent, boundingBoxComponent] : mgr.GetEntitySet<MeshComponent, TransformComponent, BoundingBoxComponent>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
    {
        if (entity->InstanceClass() != Entity::StaticClass())
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
            UniquePtr<LightmapJobBase> job = CreateJob(CreateLightmapJobParams(startIndex, index + 1));
            Assert(job != nullptr);

            startIndex = index + 1;

            AddJob(std::move(job));

            numTriangles = 0;
        }

        numTriangles += subElement.mesh->NumIndices() / 3;
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

    Mutex::Guard guard(m_queueMutex);

    Assert(!m_queue.Empty());
    LightmapJobBase* job = m_queue.Front().Get();

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

void LightmapperBase::HandleCompletedJob(LightmapJobBase* job)
{
    HYP_SCOPE;
    AssertOnThread(g_gameThread);

    HYP_DEFER({
        m_queue.Pop();
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
      m_volume(volume)
{
}

void Lightmapper<LightmapVolume>::Initialize_Internal()
{
    // no-op
}

void Lightmapper<LightmapVolume>::HandleCompletedJob_Internal(LightmapJobBase* job)
{
    HYP_SCOPE;

    LightmapJob<LightmapVolume>* jobCasted = static_cast<LightmapJob<LightmapVolume>*>(job);

    const LightmapData<LightmapVolume>& lightmapData = jobCasted->GetLightmapData();

    LightmapElement* lightmapElement = jobCasted->GetLightmapElement();

    if (lightmapElement == nullptr)
    {
        HYP_LOG(Lightmap, Debug, "Lightmap element is null, skipping building LightmapElement textures for job {}", job->GetUUID());

        return;
    }

    if (!m_volume->BuildElementTextures(lightmapData, lightmapElement->id))
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

            Assert(subElementIndex < lightmapData.GetMeshData().Size());

            const LightmapMeshData& lightmapMeshData = lightmapData.GetMeshData()[subElementIndex];
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

                    SafeDelete(std::move(meshComponent.material));

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

        if (IsOnThread(m_scene->GetEntityManager()->GetOwnerThreadId()))
        {
            // If we are on the same thread, we can update the mesh component immediately
            updateMeshComponent();
        }
        else
        {
            // Enqueue the update to be performed on the owner thread
            ThreadBase* thread = GetThreadById(m_scene->GetEntityManager()->GetOwnerThreadId());
            Assert(thread != nullptr);

            thread->GetScheduler().Enqueue(std::move(updateMeshComponent), TaskEnqueueFlags::FIRE_AND_FORGET);
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

    // @TODO
}

} // namespace hyperion
