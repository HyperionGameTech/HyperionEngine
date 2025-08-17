/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#include <scene/View.hpp>
#include <scene/Scene.hpp>
#include <scene/Light.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/Material.hpp>
#include <scene/EnvGrid.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/lightmapper/LightmapVolume.hpp>
#include <scene/camera/Camera.hpp>
#include <scene/animation/Skeleton.hpp>

#include <scene/EntityManager.hpp>
#include <scene/EntityTag.hpp>

#include <scene/components/MeshComponent.hpp>
#include <scene/components/TransformComponent.hpp>
#include <scene/components/BoundingBoxComponent.hpp>
#include <scene/components/VisibilityStateComponent.hpp>
#include <scene/components/SkyComponent.hpp>

#include <rendering/RenderGlobalState.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/RenderBackend.hpp>
#include <rendering/Texture.hpp>
#include <rendering/util/SafeDeleter.hpp>
#include <rendering/subsystems/sky/SkydomeRenderer.hpp>

#include <core/object/HypClass.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/threading/Task.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>

// #define HYP_DISABLE_VISIBILITY_CHECK
// #define HYP_VISIBILITY_CHECK_DEBUG

namespace hyperion {

#pragma region ViewOutputTarget

ViewOutputTarget::ViewOutputTarget()
{
}

ViewOutputTarget::ViewOutputTarget(const FramebufferRef& framebuffer)
    : m_impl(framebuffer)
{
    AssertDebug(framebuffer != nullptr);
}

ViewOutputTarget::ViewOutputTarget(const Handle<GBuffer>& gbuffer)
    : m_impl(gbuffer)
{
    Assert(gbuffer != nullptr);
}

ViewOutputTarget::~ViewOutputTarget()
{
    SafeDelete(std::move(m_impl));
}

const Handle<GBuffer>& ViewOutputTarget::GetGBuffer() const
{
    if (!m_impl.Is<GBuffer>())
    {
        return Handle<GBuffer>::Null();
    }

    return static_cast<const Handle<GBuffer>&>(m_impl);
}

const FramebufferRef& ViewOutputTarget::GetFramebuffer() const
{
    if (m_impl.Is<FramebufferBase>())
    {
        return static_cast<const FramebufferRef&>(m_impl);
    }

    if (m_impl.Is<GBuffer>())
    {
        return static_cast<const Handle<GBuffer>&>(m_impl)->GetBucket(RenderBucket::RB_OPAQUE).GetFramebuffer();
    }

    return FramebufferRef::Null();
}

const FramebufferRef& ViewOutputTarget::GetFramebuffer(RenderBucket rb) const
{
    if (m_impl.Is<FramebufferBase>())
    {
        return static_cast<const FramebufferRef&>(m_impl);
    }

    if (m_impl.Is<GBuffer>())
    {
        return static_cast<const Handle<GBuffer>&>(m_impl)->GetBucket(rb).GetFramebuffer();
    }

    return FramebufferRef::Null();
}

Span<const FramebufferRef> ViewOutputTarget::GetFramebuffers() const
{
    if (!m_impl.IsValid())
    {
        return {};
    }

    if (m_impl.Is<GBuffer>())
    {
        return static_cast<const Handle<GBuffer>&>(m_impl)->GetFramebuffers();
    }

    return { &static_cast<const FramebufferRef&>(m_impl), 1 };
}

#pragma endregion ViewOutputTarget

#pragma region View

View::View()
    : View(ViewDesc {})
{
}

View::View(const ViewDesc& viewDesc)
    : m_viewDesc(viewDesc),
      m_flags(viewDesc.flags),
      m_viewport(viewDesc.viewport),
      m_camera(MakeStrongRef(viewDesc.camera)),
      m_priority(viewDesc.priority),
      m_readbackTextureGpuImages {},
      m_overrideAttributes(viewDesc.overrideAttributes),
      m_collectionTaskBatch(nullptr)
{
    for (Scene* scene : m_viewDesc.scenes)
    {
        if (!scene)
        {
            continue;
        }

        m_scenes.PushBack(MakeStrongRef(scene));
    }

    for (auto it = std::begin(m_renderProxyLists); it != std::end(m_renderProxyLists); ++it)
    {
        if ((m_flags & ViewFlags::NOT_MULTI_BUFFERED) && it != std::begin(m_renderProxyLists))
        {
            *it = *(it - 1);

            continue;
        }

        *it = new RenderProxyList(/* isShared */ true, /* useRefCounting */ true);
    }
}

View::~View()
{
    Assert(m_collectionTaskBatch == nullptr, "Collection tasks pending on View destruction!");

    for (auto it = std::begin(m_renderProxyLists); it != std::end(m_renderProxyLists); ++it)
    {
        // if render proxy lists aren't unique, we just delete the first one and break the loop
        if (it != std::begin(m_renderProxyLists) && *it == *(it - 1))
        {
            break;
        }

        delete *it;
    }
}

void View::Init()
{
    Assert(m_camera.IsValid(), "Camera is not valid for View with Id #%u", Id().Value());
    InitObject(m_camera);

    for (Viewport& viewportBuffered : m_viewportBuffered)
    {
        viewportBuffered = m_viewport;
    }

    const Vec2u extent = MathUtil::Max(m_viewDesc.outputTargetDesc.extent, Vec2u::One());

    if (m_viewDesc.flags & ViewFlags::GBUFFER)
    {
        AssertDebug(m_viewDesc.outputTargetDesc.attachments.Empty(),
            "View with GBuffer flag cannot have output target attachments defined, as it will use GBuffer instead.");

        m_outputTarget = ViewOutputTarget(CreateObject<GBuffer>(extent));
    }
    else if (m_viewDesc.outputTargetDesc.attachments.Any())
    {
        FramebufferRef framebuffer = g_renderBackend->MakeFramebuffer(extent, m_viewDesc.outputTargetDesc.numViews);

        for (uint32 attachmentIndex = 0; attachmentIndex < m_viewDesc.outputTargetDesc.attachments.Size(); ++attachmentIndex)
        {
            const ViewOutputTargetAttachmentDesc& attachmentDesc = m_viewDesc.outputTargetDesc.attachments[attachmentIndex];

            AttachmentRef attachment = framebuffer->AddAttachment(
                attachmentIndex,
                attachmentDesc.format,
                attachmentDesc.imageType,
                attachmentDesc.loadOp,
                attachmentDesc.storeOp);

            attachment->SetClearColor(attachmentDesc.clearColor);
        }

        DeferCreate(framebuffer);

        m_outputTarget = ViewOutputTarget(framebuffer);
    }

    Assert(m_outputTarget.IsValid(), "View with id #%u must have a valid output target!", Id().Value());

    if (m_flags & ViewFlags::ENABLE_READBACK)
    {
        CreateReadbackTexture();
    }

    SetReady(true);
}

bool View::TestRay(const Ray& ray, RayTestResults& outResults, bool useBvh) const
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread | ThreadCategory::THREAD_CATEGORY_TASK);

    bool hasHits = false;

    for (const Handle<Scene>& scene : m_scenes)
    {
        Assert(scene.IsValid());

        if (scene->GetOctree().TestRay(ray, outResults, useBvh))
        {
            hasHits = true;
        }
    }

    return hasHits;
}

void View::UpdateViewport()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread);
    AssertReady();

    const uint32 idx = RenderApi_GetFrameIndex();

    m_viewportBuffered[idx] = m_viewport;

    if (m_readbackTexture)
    {
        m_readbackTextureGpuImages[idx] = m_readbackTexture->GetGpuImage();
    }
}

void View::UpdateVisibility()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread);
    AssertReady();

    if (!m_camera.IsValid())
    {
        HYP_LOG(Scene, Warning, "Camera is not valid for View with Id #%u, cannot update visibility!", Id().Value());
        return;
    }

    for (const Handle<Scene>& scene : m_scenes)
    {
        scene->GetOctree().CalculateVisibility(m_camera);
    }
}

void View::BeginAsyncCollection(TaskBatch& batch)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread);
    AssertReady();

    AssertDebug(m_collectionTaskBatch == nullptr, "m_collectionTaskBatch is not nullptr, already collecting?");
    m_collectionTaskBatch = &batch;

    RenderProxyList& rpl = RenderApi_GetProducerProxyList(this);

    batch.AddTask([this, &rpl]()
        {
            rpl.BeginWrite();

            rpl.viewport = m_viewport;
            rpl.priority = m_priority;

            CollectCameras(rpl);
            CollectLights(rpl);
            CollectLightmapVolumes(rpl);
            CollectEnvGrids(rpl);
            CollectEnvProbes(rpl);
            CollectMeshEntities(rpl);

            rpl.EndWrite();
        });
}

void View::EndAsyncCollection()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread);
    AssertReady();

    Assert(m_collectionTaskBatch != nullptr);
    Assert(m_collectionTaskBatch->IsCompleted());

    m_collectionTaskBatch = nullptr;
}

void View::CollectSync()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread);
    AssertReady();

    TaskBatch taskBatch;
    BeginAsyncCollection(taskBatch);

    taskBatch.ExecuteBlocking();

    EndAsyncCollection();
}

const Viewport& View::GetViewport() const
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread | g_renderThread);

    if (Threads::IsOnThread(g_gameThread))
    {
        return m_viewport;
    }

    AssertReady();

    return m_viewportBuffered[RenderApi_GetFrameIndex()];
}

void View::SetViewport(const Viewport& viewport)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread);

    m_viewport = viewport;

    if (IsInitCalled())
    {
        if (m_flags & ViewFlags::ENABLE_READBACK)
        {
            m_readbackTexture.Reset();

            CreateReadbackTexture();
        }

        m_viewportBuffered[RenderApi_GetFrameIndex()] = viewport;
    }
}

GpuImageBase* View::GetReadbackTextureGpuImage() const
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread | g_renderThread);

    return m_readbackTextureGpuImages[RenderApi_GetFrameIndex()];
}

void View::CreateReadbackTexture()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread);

    m_readbackTexture.Reset();
    m_readbackTexture = CreateObject<Texture>(TextureDesc {
        TT_TEX2D,
        m_viewDesc.readbackTextureFormat,
        Vec3u { m_viewport.extent, 1 },
        TFM_NEAREST,
        TFM_NEAREST,
        TWM_CLAMP_TO_EDGE,
        1,
        IU_SAMPLED });

    if (IsInitCalled())
    {
        InitObject(m_readbackTexture);
    }

    if (IsReady())
    {
        // notify change
        OnReadbackTextureChanged(m_readbackTexture);
    }
    else
    {
        // set buffered gpu images before render thread sees them
        for (uint32 i = 0; i < ArraySize(m_readbackTextureGpuImages); i++)
        {
            m_readbackTextureGpuImages[i] = m_readbackTexture->GetGpuImage();
        }
    }
}

void View::SetPriority(int priority)
{
    m_priority = priority;
}

void View::AddScene(const Handle<Scene>& scene)
{
    HYP_SCOPE;

    if (!scene.IsValid())
    {
        return;
    }

    if (m_scenes.Contains(scene))
    {
        return;
    }

    m_scenes.PushBack(scene);

    if (IsInitCalled())
    {
        InitObject(scene);
    }
}

void View::RemoveScene(Scene* scene)
{
    HYP_SCOPE;

    if (!scene)
    {
        return;
    }

    auto it = m_scenes.FindIf([scene](const auto& item)
        {
            return item.Get() == scene;
        });

    if (it == m_scenes.End())
    {
        return;
    }

    m_scenes.Erase(it);
}

ResourceTrackerDiff View::CollectMeshEntities(RenderProxyList& rpl)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread | ThreadCategory::THREAD_CATEGORY_TASK);
    AssertReady();

    if (!m_camera.IsValid())
    {
        HYP_LOG(Scene, Warning, "Camera is not valid for View with Id #%u, cannot collect entities!", Id().Value());

        return rpl.GetMeshEntities().GetDiff();
    }

    const ObjId<Camera> cameraId = m_camera->Id();

    for (const Handle<Scene>& scene : m_scenes)
    {
        Assert(scene.IsValid());
        Assert(scene->IsReady());

        if (scene->GetFlags() & SceneFlags::DETACHED)
        {
            HYP_LOG(Scene, Warning, "Scene \"{}\" has DETACHED flag set, cannot collect entities for render collector!", scene->GetName());

            continue;
        }

        const VisibilityStateSnapshot visibilityStateSnapshot = scene->GetOctree().GetVisibilityState().GetSnapshot(cameraId);

        uint32 numCollectedEntities = 0;
        uint32 numSkippedEntities = 0;

        switch (uint32(m_flags) & uint32(ViewFlags::COLLECT_ALL_ENTITIES))
        {
        case uint32(ViewFlags::COLLECT_ALL_ENTITIES):
            if (m_flags & ViewFlags::NO_FRUSTUM_CULLING)
            {
                for (auto [entity, meshComponent] : scene->GetEntityManager()->GetEntitySet<MeshComponent>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
                {
                    ++numCollectedEntities;

                    rpl.GetMeshEntities().Track(entity->Id(), entity, entity->GetRenderProxyVersionPtr());

                    if (const Handle<Material>& material = meshComponent.material)
                    {
                        rpl.GetMaterials().Track(material.Id(), material.Get(), material->GetRenderProxyVersionPtr());

                        for (const auto& it : material->GetTextures())
                        {
                            const Handle<Texture>& texture = it.second;

                            if (!texture.IsValid())
                            {
                                continue;
                            }

                            rpl.GetTextures().Track(texture.Id(), texture.Get());
                        }
                    }

                    if (const Handle<Skeleton>& skeleton = meshComponent.skeleton)
                    {
                        rpl.GetSkeletons().Track(skeleton.Id(), skeleton.Get(), skeleton->GetRenderProxyVersionPtr());
                    }
                }
            }
            else
            {
                for (auto [entity, meshComponent, visibilityStateComponent] : scene->GetEntityManager()->GetEntitySet<MeshComponent, VisibilityStateComponent>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
                {
                    if (!(visibilityStateComponent.flags & VISIBILITY_STATE_FLAG_ALWAYS_VISIBLE))
                    {
#ifndef HYP_DISABLE_VISIBILITY_CHECK
                        if (!visibilityStateComponent.visibilityState)
                        {
#ifdef HYP_VISIBILITY_CHECK_DEBUG
                            ++numSkippedEntities;
#endif
                            continue;
                        }

                        if (!visibilityStateComponent.visibilityState->GetSnapshot(cameraId).ValidToParent(visibilityStateSnapshot))
                        {
#ifdef HYP_VISIBILITY_CHECK_DEBUG
                            ++numSkippedEntities;
#endif

                            continue;
                        }
#endif
                    }

                    ++numCollectedEntities;

                    rpl.GetMeshEntities().Track(entity->Id(), entity, entity->GetRenderProxyVersionPtr());

                    if (const Handle<Material>& material = meshComponent.material)
                    {
                        rpl.GetMaterials().Track(material.Id(), material.Get(), material->GetRenderProxyVersionPtr());

                        for (const auto& it : material->GetTextures())
                        {
                            const Handle<Texture>& texture = it.second;

                            if (!texture.IsValid())
                            {
                                continue;
                            }

                            rpl.GetTextures().Track(texture.Id(), texture.Get());
                        }
                    }

                    if (const Handle<Skeleton>& skeleton = meshComponent.skeleton)
                    {
                        rpl.GetSkeletons().Track(skeleton.Id(), skeleton.Get(), skeleton->GetRenderProxyVersionPtr());
                    }
                }
            }

            break;

        case uint32(ViewFlags::COLLECT_STATIC_ENTITIES):
            if (m_flags & ViewFlags::NO_FRUSTUM_CULLING)
            {
                for (auto [entity, meshComponent, _] : scene->GetEntityManager()->GetEntitySet<MeshComponent, EntityTagComponent<EntityTag::STATIC>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
                {
                    ++numCollectedEntities;

                    rpl.GetMeshEntities().Track(entity->Id(), entity, entity->GetRenderProxyVersionPtr());

                    if (const Handle<Material>& material = meshComponent.material)
                    {
                        rpl.GetMaterials().Track(material.Id(), material.Get(), material->GetRenderProxyVersionPtr());

                        for (const auto& it : material->GetTextures())
                        {
                            const Handle<Texture>& texture = it.second;

                            if (!texture.IsValid())
                            {
                                continue;
                            }

                            rpl.GetTextures().Track(texture.Id(), texture.Get());
                        }
                    }

                    if (const Handle<Skeleton>& skeleton = meshComponent.skeleton)
                    {
                        rpl.GetSkeletons().Track(skeleton.Id(), skeleton.Get(), skeleton->GetRenderProxyVersionPtr());
                    }
                }
            }
            else
            {
                for (auto [entity, meshComponent, visibilityStateComponent, _] : scene->GetEntityManager()->GetEntitySet<MeshComponent, VisibilityStateComponent, EntityTagComponent<EntityTag::STATIC>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
                {
                    if (!(visibilityStateComponent.flags & VISIBILITY_STATE_FLAG_ALWAYS_VISIBLE))
                    {
#ifndef HYP_DISABLE_VISIBILITY_CHECK
                        if (!visibilityStateComponent.visibilityState)
                        {
#ifdef HYP_VISIBILITY_CHECK_DEBUG
                            ++numSkippedEntities;
#endif
                            continue;
                        }

                        if (!visibilityStateComponent.visibilityState->GetSnapshot(cameraId).ValidToParent(visibilityStateSnapshot))
                        {
#ifdef HYP_VISIBILITY_CHECK_DEBUG
                            ++numSkippedEntities;
#endif

                            continue;
                        }
#endif
                    }

                    ++numCollectedEntities;

                    rpl.GetMeshEntities().Track(entity->Id(), entity, entity->GetRenderProxyVersionPtr());

                    if (const Handle<Material>& material = meshComponent.material)
                    {
                        rpl.GetMaterials().Track(material.Id(), material.Get(), material->GetRenderProxyVersionPtr());

                        for (const auto& it : material->GetTextures())
                        {
                            const Handle<Texture>& texture = it.second;

                            if (!texture.IsValid())
                            {
                                continue;
                            }

                            rpl.GetTextures().Track(texture.Id(), texture.Get());
                        }
                    }

                    if (const Handle<Skeleton>& skeleton = meshComponent.skeleton)
                    {
                        rpl.GetSkeletons().Track(skeleton.Id(), skeleton.Get(), skeleton->GetRenderProxyVersionPtr());
                    }
                }
            }

            break;

        case uint32(ViewFlags::COLLECT_DYNAMIC_ENTITIES):
            if (m_flags & ViewFlags::NO_FRUSTUM_CULLING)
            {
                for (auto [entity, meshComponent, _] : scene->GetEntityManager()->GetEntitySet<MeshComponent, EntityTagComponent<EntityTag::DYNAMIC>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
                {
                    ++numCollectedEntities;

                    rpl.GetMeshEntities().Track(entity->Id(), entity, entity->GetRenderProxyVersionPtr());

                    if (const Handle<Material>& material = meshComponent.material)
                    {
                        rpl.GetMaterials().Track(material.Id(), material.Get(), material->GetRenderProxyVersionPtr());

                        for (const auto& it : material->GetTextures())
                        {
                            const Handle<Texture>& texture = it.second;

                            if (!texture.IsValid())
                            {
                                continue;
                            }

                            rpl.GetTextures().Track(texture.Id(), texture.Get());
                        }
                    }

                    if (const Handle<Skeleton>& skeleton = meshComponent.skeleton)
                    {
                        rpl.GetSkeletons().Track(skeleton.Id(), skeleton.Get(), skeleton->GetRenderProxyVersionPtr());
                    }
                }
            }
            else
            {
                for (auto [entity, meshComponent, visibilityStateComponent, _] : scene->GetEntityManager()->GetEntitySet<MeshComponent, VisibilityStateComponent, EntityTagComponent<EntityTag::DYNAMIC>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
                {
                    if (!(visibilityStateComponent.flags & VISIBILITY_STATE_FLAG_ALWAYS_VISIBLE))
                    {
#ifndef HYP_DISABLE_VISIBILITY_CHECK
                        if (!visibilityStateComponent.visibilityState)
                        {
#ifdef HYP_VISIBILITY_CHECK_DEBUG
                            ++numSkippedEntities;
#endif
                            continue;
                        }

                        if (!visibilityStateComponent.visibilityState->GetSnapshot(cameraId).ValidToParent(visibilityStateSnapshot))
                        {
#ifdef HYP_VISIBILITY_CHECK_DEBUG
                            ++numSkippedEntities;
#endif

                            continue;
                        }
#endif
                    }

                    ++numCollectedEntities;

                    rpl.GetMeshEntities().Track(entity->Id(), entity, entity->GetRenderProxyVersionPtr());

                    if (const Handle<Material>& material = meshComponent.material)
                    {
                        rpl.GetMaterials().Track(material.Id(), material.Get(), material->GetRenderProxyVersionPtr());

                        for (const auto& it : material->GetTextures())
                        {
                            const Handle<Texture>& texture = it.second;

                            if (!texture.IsValid())
                            {
                                continue;
                            }

                            rpl.GetTextures().Track(texture.Id(), texture.Get());
                        }
                    }

                    if (const Handle<Skeleton>& skeleton = meshComponent.skeleton)
                    {
                        rpl.GetSkeletons().Track(skeleton.Id(), skeleton.Get(), skeleton->GetRenderProxyVersionPtr());
                    }
                }
            }

            break;
        default:
            break;
        }

#ifdef HYP_VISIBILITY_CHECK_DEBUG
        HYP_LOG(Scene, Debug, "Collected {} entities for View {}, {} skipped", numCollectedEntities, Id(), numSkippedEntities);
#endif
    }

    ResourceTrackerDiff meshesDiff = rpl.GetMeshEntities().GetDiff();

    if (meshesDiff.NeedsUpdate())
    {
        Array<Entity*> added;
        rpl.GetMeshEntities().GetAdded(added, /* includeChanged */ true);

        for (Entity* entity : added)
        {
            auto&& [meshComponent, transformComponent, boundingBoxComponent] = entity->GetEntityManager()->TryGetComponents<MeshComponent, TransformComponent, BoundingBoxComponent>(entity);
            AssertDebug(meshComponent != nullptr);

            RenderProxyMesh& meshProxy = *rpl.GetMeshEntities().SetProxy(entity->Id(), RenderProxyMesh());
            meshProxy.entity = MakeWeakRef(entity);
            meshProxy.mesh = meshComponent->mesh;
            meshProxy.material = meshComponent->material;
            meshProxy.skeleton = meshComponent->skeleton;
            meshProxy.instanceData = meshComponent->instanceData;
            meshProxy.bufferData.modelMatrix = transformComponent ? transformComponent->transform.GetMatrix() : Matrix4::Identity();
            meshProxy.bufferData.previousModelMatrix = meshComponent->previousModelMatrix;
            meshProxy.bufferData.worldAabbMax = boundingBoxComponent ? boundingBoxComponent->worldAabb.max : MathUtil::MinSafeValue<Vec3f>();
            meshProxy.bufferData.worldAabbMin = boundingBoxComponent ? boundingBoxComponent->worldAabb.min : MathUtil::MaxSafeValue<Vec3f>();
            meshProxy.bufferData.userData = reinterpret_cast<EntityShaderData::EntityUserData&>(meshComponent->userData);
        }
    }

    return meshesDiff;
}

void View::CollectCameras(RenderProxyList& rpl)
{
    HYP_SCOPE;

    if (m_flags & ViewFlags::SKIP_CAMERAS)
    {
        return;
    }

    for (const Handle<Scene>& scene : m_scenes)
    {
        Assert(scene.IsValid());
        Assert(scene->IsReady());

        for (auto [entity, _] : scene->GetEntityManager()->GetEntitySet<EntityType<Camera>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            Camera* camera = static_cast<Camera*>(entity);

            rpl.GetCameras().Track(camera->Id(), camera, camera->GetRenderProxyVersionPtr());
        }
    }
}

void View::CollectLights(RenderProxyList& rpl)
{
    HYP_SCOPE;

    if (m_flags & ViewFlags::SKIP_LIGHTS)
    {
        return;
    }

    for (const Handle<Scene>& scene : m_scenes)
    {
        Assert(scene.IsValid());
        Assert(scene->IsReady());

        for (auto [entity, _] : scene->GetEntityManager()->GetEntitySet<EntityType<Light>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            Light* light = static_cast<Light*>(entity);

            bool isLightInFrustum = false;

            if (m_flags & ViewFlags::NO_FRUSTUM_CULLING)
            {
                isLightInFrustum = true;
            }
            else
            {
                switch (light->GetLightType())
                {
                case LT_DIRECTIONAL:
                    isLightInFrustum = true;
                    break;
                case LT_POINT:
                    isLightInFrustum = m_camera->GetFrustum().ContainsBoundingSphere(light->GetBoundingSphere());
                    break;
                case LT_SPOT:
                    // @TODO Implement frustum culling for spot lights
                    isLightInFrustum = true;
                    break;
                case LT_AREA_RECT:
                    isLightInFrustum = m_camera->GetFrustum().ContainsAABB(light->GetAABB());
                    break;
                default:
                    break;
                }
            }

            if (isLightInFrustum)
            {
                rpl.GetLights().Track(light->Id(), light, light->GetRenderProxyVersionPtr());

                if (light->GetMaterial().IsValid())
                {
                    rpl.GetMaterials().Track(light->GetMaterial()->Id(), light->GetMaterial().Get());

                    for (const auto& it : light->GetMaterial()->GetTextures())
                    {
                        const Handle<Texture>& texture = it.second;

                        if (!texture.IsValid())
                        {
                            continue;
                        }

                        rpl.GetTextures().Track(texture->Id(), texture.Get());
                    }
                }
            }
        }
    }
}

void View::CollectLightmapVolumes(RenderProxyList& rpl)
{
    HYP_SCOPE;

    if (m_flags & ViewFlags::SKIP_LIGHTMAP_VOLUMES)
    {
        return;
    }

    for (const Handle<Scene>& scene : m_scenes)
    {
        Assert(scene.IsValid());
        Assert(scene->IsReady());

        for (auto [entity, _] : scene->GetEntityManager()->GetEntitySet<EntityType<LightmapVolume>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            LightmapVolume* lightmapVolume = ObjCast<LightmapVolume>(entity);
            Assert(lightmapVolume != nullptr);

            const BoundingBox& volumeAabb = lightmapVolume->GetAABB();

            if (!volumeAabb.IsValid() || !volumeAabb.IsFinite())
            {
                HYP_LOG(Scene, Warning, "Lightmap volume {} has an invalid AABB in view {}", lightmapVolume->Id(), Id());

                continue;
            }

            if (!m_camera->GetFrustum().ContainsAABB(volumeAabb))
            {
                continue;
            }

            rpl.GetLightmapVolumes().Track(lightmapVolume->Id(), lightmapVolume, lightmapVolume->GetRenderProxyVersionPtr());
        }
    }
}

void View::CollectEnvGrids(RenderProxyList& rpl)
{
    HYP_SCOPE;

    if (m_flags & ViewFlags::SKIP_ENV_GRIDS)
    {
        return;
    }

    for (const Handle<Scene>& scene : m_scenes)
    {
        Assert(scene.IsValid());
        Assert(scene->IsReady());

        for (auto [entity, _] : scene->GetEntityManager()->GetEntitySet<EntityType<EnvGrid>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            EnvGrid* envGrid = static_cast<EnvGrid*>(entity);

            const BoundingBox& gridAabb = envGrid->GetAABB();

            if (!gridAabb.IsValid() || !gridAabb.IsFinite())
            {
                HYP_LOG(Scene, Warning, "EnvGrid {} has an invalid AABB in view {}", envGrid->Id(), Id());

                continue;
            }

            if (!m_camera->GetFrustum().ContainsAABB(gridAabb))
            {
                HYP_LOG(Scene, Debug, "EnvGrid {} is not in frustum of View {}", envGrid->Id(), Id());

                continue;
            }

            rpl.GetEnvGrids().Track(envGrid->Id(), envGrid, envGrid->GetRenderProxyVersionPtr());
        }
    }
}

void View::CollectEnvProbes(RenderProxyList& rpl)
{
    HYP_SCOPE;

    if (m_flags & ViewFlags::SKIP_ENV_PROBES)
    {
        return;
    }

    for (const Handle<Scene>& scene : m_scenes)
    {
        Assert(scene.IsValid());
        Assert(scene->IsReady());

        for (auto [entity, _] : scene->GetEntityManager()->GetEntitySet<EntityType<EnvProbe>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            EnvProbe* probe = static_cast<EnvProbe*>(entity);

            if (!probe->IsSkyProbe())
            {
                const BoundingBox& probeAabb = probe->GetAABB();

                if (!probeAabb.IsValid() || !probeAabb.IsFinite())
                {
                    HYP_LOG(Scene, Warning, "EnvProbe {} has an invalid AABB in view {}", probe->Id(), Id());

                    continue;
                }

                if (!(m_flags & ViewFlags::NO_FRUSTUM_CULLING) && !m_camera->GetFrustum().ContainsAABB(probeAabb))
                {
                    continue;
                }
            }

            rpl.GetEnvProbes().Track(probe->Id(), probe, probe->GetRenderProxyVersionPtr());
        }

        for (auto [entity, skyComponent] : scene->GetEntityManager()->GetEntitySet<SkyComponent>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            if (skyComponent.subsystem)
            {
                AssertDebug(skyComponent.subsystem->GetEnvProbe()->IsA<SkyProbe>());

                rpl.GetEnvProbes().Track(skyComponent.subsystem->GetEnvProbe()->Id(), skyComponent.subsystem->GetEnvProbe(), skyComponent.subsystem->GetEnvProbe()->GetRenderProxyVersionPtr());
            }
        }
    }
}

#pragma endregion View

} // namespace hyperion
