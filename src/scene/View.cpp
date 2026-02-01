/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#include <ScenePch.hpp>

#include <scene/View.hpp>
#include <scene/Scene.hpp>
#include <scene/Light.hpp>
#include <scene/EnvGrid.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/EntityManager.hpp>
#include <scene/EntityTag.hpp>
#include <scene/ParticleVolume.hpp>
#include <scene/FogVolume.hpp>
#include <scene/LightmapVolume.hpp>

#include <scene/camera/Camera.hpp>
#include <scene/animation/Skeleton.hpp>

#include <scene/components/MeshComponent.hpp>
#include <scene/components/TransformComponent.hpp>
#include <scene/components/BoundingBoxComponent.hpp>
#include <scene/components/VisibilityStateComponent.hpp>
#include <scene/components/LightmapElementComponent.hpp>

#include <rendering/RenderInterface.hpp>
#include <rendering/RenderCollection.hpp>
#include <rendering/RenderProxyList.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/Texture.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/Material.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <core/threading/Task.hpp>

#include <engine/EngineDriver.hpp>

// #define HYP_DISABLE_VISIBILITY_CHECK
// #define HYP_VISIBILITY_CHECK_DEBUG

#include <View.generated.inl>

namespace Hyperion {

HYP_API extern Pool* g_scenePool;
HYP_API extern Pool* g_framePools[RingBufferDepth];

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
    if (!m_impl)
    {
        return Handle<GBuffer>::Null();
    }

    return ObjCast<GBuffer>(m_impl);
}

const FramebufferRef& ViewOutputTarget::GetFramebuffer() const
{
    if (!m_impl)
    {
        return FramebufferRef::Null();
    }

    if (m_impl->IsA(GBuffer::StaticClass()))
    {
        return static_cast<GBuffer&>(*m_impl).GetBucket(RenderBucket::RB_OPAQUE).GetFramebuffer();
    }

    return ObjCast<Framebuffer>(m_impl);
}

const FramebufferRef& ViewOutputTarget::GetFramebuffer(RenderBucket rb) const
{
    if (!m_impl)
    {
        return FramebufferRef::Null();
    }

    if (m_impl->IsA(GBuffer::StaticClass()))
    {
        return static_cast<GBuffer&>(*m_impl).GetBucket(rb).GetFramebuffer();
    }

    return ObjCast<Framebuffer>(m_impl);
}

Span<const FramebufferRef> ViewOutputTarget::GetFramebuffers() const
{
    if (!m_impl)
    {
        return {};
    }

    if (m_impl->IsA(GBuffer::StaticClass()))
    {
        return static_cast<GBuffer&>(*m_impl).GetFramebuffers();
    }

    return { &ObjCast<Framebuffer>(m_impl), 1 };
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

        *it = new RenderProxyList(g_scenePool, /* isShared */ true, /* useRefCounting */ true);
    }
}

View::~View()
{
    Assert(m_collectionTaskBatch == nullptr, "Collection tasks pending on View destruction!");

    for (uint32 i = 0; i < HYP_ARRAY_SIZE(m_renderProxyLists); i++)
    {
        if ((m_flags & ViewFlags::NOT_MULTI_BUFFERED) && i > 0)
        {
            break;
        }

        delete m_renderProxyLists[i];
    }

    if (m_camera != nullptr)
    {
        SafeDelete(std::move(m_camera));
    }

    if (m_readbackTexture != nullptr)
    {
        SafeDelete(std::move(m_readbackTexture));
    }
}

void View::Init()
{
    ObjectBase::Init();

    Assert(m_camera.IsValid(), "Camera is not valid for View with Id #%u", Id().Value());
    InitObject(m_camera);

    for (Viewport& viewportBuffered : m_viewportBuffered)
    {
        viewportBuffered = m_viewport;
    }

    const Vec2u extent = MathUtil::Max(m_viewDesc.renderTargetDesc.extent, Vec2u::One());

    if (m_viewDesc.flags & ViewFlags::GBUFFER)
    {
        AssertDebug(m_viewDesc.renderTargetDesc.numAttachments == 0,
            "View with GBuffer flag cannot have output target attachments defined, as it will use GBuffer instead.");

        m_outputTarget = ViewOutputTarget(MakeHandle<GBuffer>(extent));
    }
    else if (m_viewDesc.renderTargetDesc.numAttachments > 0)
    {
        FramebufferRef framebuffer = g_renderInterface->MakeFramebuffer(m_viewDesc.renderTargetDesc);

        for (uint32 attachmentIndex = 0; attachmentIndex < m_viewDesc.renderTargetDesc.numAttachments; attachmentIndex++)
        {
            const AttachmentDesc& attachmentDesc = m_viewDesc.renderTargetDesc.attachments[attachmentIndex];
            AssertDebug(attachmentDesc.format != TF_NONE);

            Attachment* attachment = framebuffer->AddAttachment(
                attachmentIndex,
                attachmentDesc.format,
                attachmentDesc.imageType,
                attachmentDesc.loadOp,
                attachmentDesc.storeOp);

            attachment->SetClearColor(Vec4f(
                attachmentDesc.clearColor[0],
                attachmentDesc.clearColor[1],
                attachmentDesc.clearColor[2],
                attachmentDesc.clearColor[3]));
        }

        DeferCreate(framebuffer);

        m_outputTarget = ViewOutputTarget(framebuffer);
    }

    Assert(m_outputTarget.IsValid(), "View with id {} must have a valid output target!", Id());

    if (m_flags & ViewFlags::ENABLE_READBACK)
    {
        CreateReadbackTexture();
    }

    SetReady(true);
}

bool View::TestRay(const Ray& ray, RayTestResults& outResults, EnumFlags<RayTestFlags> flags) const
{
    HYP_SCOPE;
    AssertOnThread(g_simThread | ThreadCategory::THREAD_CATEGORY_TASK);

    bool hasHits = false;

    for (const Handle<Scene>& scene : m_scenes)
    {
        AssertDebug(scene != nullptr);

        if (!scene || !(scene->GetSceneFlags() & SceneFlags::HAS_OCTREE))
        {
            continue;
        }

        if (scene->GetOctree().TestRay(ray, outResults, flags))
        {
            hasHits = true;
        }
    }

    return hasHits;
}

void View::UpdateViewport()
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);
    AssertReady();

    const uint32 slot = GetRingIndex();

    if ((m_flags & ViewFlags::MATCH_CAMERA_DIMENSIONS))
    {
        if (m_camera != nullptr)
        {
            const Vec2u newExtent = MathUtil::Max(Vec2u(m_camera->GetDimensions()), Vec2u::One());

            if (m_viewport.extent != newExtent)
            {
                HYP_LOG(Scene, Debug, "Matching viewport extent to Camera dimensions: {} -> {}", m_viewport.extent, newExtent);
            }

            m_viewport.extent = newExtent;
        }
        else
        {
            m_viewport.extent = Vec2u::One();
        }
    }

    Viewport& viewportBuffered = m_viewportBuffered[slot];
    viewportBuffered = m_viewport;

    if (m_readbackTexture)
    {
        m_readbackTextureGpuImages[slot] = m_readbackTexture->GetGpuImage();
    }
}

void View::UpdateVisibility()
{
    HYP_SCOPE;
    AssertOnThread(g_simThread | g_visThread);
    AssertReady();

    if (!m_camera.IsValid())
    {
        HYP_LOG(Scene, Warning, "Camera is not valid for View with Id #%u, cannot update visibility!", Id().Value());
        return;
    }

    for (Scene* scene : m_scenes)
    {
        AssertDebug(scene != nullptr);

        if (!scene || !(scene->GetSceneFlags() & SceneFlags::HAS_OCTREE))
        {
            continue;
        }

        scene->GetOctree().CalculateVisibility(m_camera);
    }
}

void View::BeginAsyncCollection(TaskBatch& batch)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);
    AssertReady();

    AssertDebug(m_collectionTaskBatch == nullptr, "m_collectionTaskBatch is not nullptr, already collecting?");
    m_collectionTaskBatch = &batch;

    RenderProxyList& rpl = GetProducerProxyList(this);

    batch.AddTask([this, &rpl]()
        {
            rpl.BeginWrite();

            rpl.viewport = m_viewport;
            rpl.priority = m_priority;

            CollectCameras(rpl);
            CollectLights(rpl);
            CollectLightmapVolumes(rpl);
            CollectParticleVolumes(rpl);
            CollectFogVolumes(rpl);
            CollectEnvGrids(rpl);
            CollectEnvProbes(rpl);
            CollectMeshEntities(rpl);

            rpl.EndWrite();
        });
}

void View::EndAsyncCollection()
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);
    AssertReady();

    Assert(m_collectionTaskBatch != nullptr);
    Assert(m_collectionTaskBatch->IsCompleted());

    m_collectionTaskBatch = nullptr;
}

void View::CollectSync()
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);
    AssertReady();

    TaskBatch taskBatch;
    BeginAsyncCollection(taskBatch);

    taskBatch.ExecuteBlocking();

    EndAsyncCollection();
}

const Viewport& View::GetViewport() const
{
    HYP_SCOPE;
    AssertOnThread(g_simThread | g_renderThread);

    if (IsOnThread(g_simThread))
    {
        return m_viewport;
    }

    AssertReady();

    return m_viewportBuffered[GetRingIndex()];
}

void View::SetViewport(const Viewport& viewport)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    if (viewport == m_viewport)
    {
        return;
    }

    const Vec2u prevExtent = m_viewport.extent;

    m_viewport = viewport;

    if (IsInitCalled())
    {
        if (prevExtent != m_viewport.extent && (m_flags & ViewFlags::ENABLE_READBACK))
        {
            HYP_LOG(Scene, Debug, "View viewport extent changed from {} to {}, recreating readback texture",
                prevExtent, m_viewport.extent);

            m_readbackTexture.Reset();

            CreateReadbackTexture();
        }

        m_viewportBuffered[GetRingIndex()] = viewport;
    }
}

GpuImage* View::GetReadbackTextureGpuImage() const
{
    HYP_SCOPE;
    AssertOnThread(g_simThread | g_renderThread);

    return m_readbackTextureGpuImages[GetRingIndex()];
}

void View::CreateReadbackTexture()
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    m_readbackTexture.Reset();
    m_readbackTexture = MakeHandle<Texture>(TextureDesc {
        TT_TEX2D,
        m_viewDesc.readbackTextureFormat,
        Vec3u { m_viewport.extent, 1 },
        TFM_NEAREST,
        TFM_NEAREST,
        TWM_CLAMP_TO_EDGE,
        1,
        IU_SAMPLED });

    m_readbackTexture->SetName(NAME_FMT("View_{}_ReadbackTexture", Id().Value()));

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

    if (!scene)
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

void View::CollectMeshEntities(RenderProxyList& rpl)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread | ThreadCategory::THREAD_CATEGORY_TASK);
    AssertReady();

    if (!m_camera.IsValid())
    {
        HYP_LOG(Scene, Warning, "Camera is not valid for View with Id #%u, cannot collect entities!", Id().Value());

        return;
    }

    const ObjId<Camera> cameraId = m_camera->Id();

    for (const Handle<Scene>& scene : m_scenes)
    {
        AssertDebug(scene && scene->IsReady());

        if (scene->GetSceneFlags() & SceneFlags::DETACHED)
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
            if ((m_flags & ViewFlags::NO_FRUSTUM_CULLING) || !(scene->GetSceneFlags() & SceneFlags::HAS_OCTREE))
            {
                for (auto [entity, meshComponent] : scene->GetEntityManager()->GetEntitySet<MeshComponent>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
                {
                    ++numCollectedEntities;

                    rpl.GetMeshEntities().Track(entity->Id(), entity, entity->GetRenderProxyVersionPtr());

                    if (Mesh* mesh = meshComponent.mesh)
                    {
                        rpl.GetMeshes().Track(mesh->Id(), mesh);
                    }

                    if (Material* material = meshComponent.material)
                    {
                        rpl.GetMaterials().Track(material->Id(), material, material->GetRenderProxyVersionPtr());

                        for (const Handle<Texture>& texture : material->GetTextures())
                        {
                            if (!texture.IsValid())
                            {
                                continue;
                            }

                            rpl.GetTextures().Track(texture.Id(), texture.Get());
                        }
                    }

                    if (meshComponent.skeleton != nullptr)
                    {
                        rpl.GetSkeletons().Track(meshComponent.skeleton->Id(), meshComponent.skeleton, meshComponent.skeleton->GetRenderProxyVersionPtr());
                    }
                }
            }
            else
            {
                for (auto [entity, meshComponent, visibilityStateComponent] : scene->GetEntityManager()->GetEntitySet<MeshComponent, VisibilityStateComponent>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
                {
                    if (!(visibilityStateComponent.flags & VisibilityStateFlags::ALWAYS_VISIBLE))
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

                    if (Mesh* mesh = meshComponent.mesh)
                    {
                        rpl.GetMeshes().Track(mesh->Id(), mesh);
                    }

                    if (Material* material = meshComponent.material)
                    {
                        rpl.GetMaterials().Track(material->Id(), material, material->GetRenderProxyVersionPtr());

                        for (const Handle<Texture>& texture : material->GetTextures())
                        {
                            if (!texture.IsValid())
                            {
                                continue;
                            }

                            rpl.GetTextures().Track(texture.Id(), texture.Get());
                        }
                    }

                    if (meshComponent.skeleton != nullptr)
                    {
                        rpl.GetSkeletons().Track(meshComponent.skeleton->Id(), meshComponent.skeleton, meshComponent.skeleton->GetRenderProxyVersionPtr());
                    }
                }
            }

            break;

        case uint32(ViewFlags::COLLECT_STATIC_ENTITIES):
            if ((m_flags & ViewFlags::NO_FRUSTUM_CULLING) || !(scene->GetSceneFlags() & SceneFlags::HAS_OCTREE))
            {
                for (auto [entity, meshComponent, _] : scene->GetEntityManager()->GetEntitySet<MeshComponent, TagComponent<EntityTag::MobStatic>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
                {
                    ++numCollectedEntities;

                    rpl.GetMeshEntities().Track(entity->Id(), entity, entity->GetRenderProxyVersionPtr());

                    if (Mesh* mesh = meshComponent.mesh)
                    {
                        rpl.GetMeshes().Track(mesh->Id(), mesh);
                    }

                    if (Material* material = meshComponent.material)
                    {
                        rpl.GetMaterials().Track(material->Id(), material, material->GetRenderProxyVersionPtr());

                        for (const Handle<Texture>& texture : material->GetTextures())
                        {
                            if (!texture.IsValid())
                            {
                                continue;
                            }

                            rpl.GetTextures().Track(texture.Id(), texture.Get());
                        }
                    }

                    if (meshComponent.skeleton != nullptr)
                    {
                        rpl.GetSkeletons().Track(meshComponent.skeleton->Id(), meshComponent.skeleton, meshComponent.skeleton->GetRenderProxyVersionPtr());
                    }
                }
            }
            else
            {
                for (auto [entity, meshComponent, visibilityStateComponent, _] : scene->GetEntityManager()->GetEntitySet<MeshComponent, VisibilityStateComponent, TagComponent<EntityTag::MobStatic>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
                {
                    if (!(visibilityStateComponent.flags & VisibilityStateFlags::ALWAYS_VISIBLE))
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

                    if (Mesh* mesh = meshComponent.mesh)
                    {
                        rpl.GetMeshes().Track(mesh->Id(), mesh);
                    }

                    if (Material* material = meshComponent.material)
                    {
                        rpl.GetMaterials().Track(material->Id(), material, material->GetRenderProxyVersionPtr());

                        for (const Handle<Texture>& texture : material->GetTextures())
                        {
                            if (!texture.IsValid())
                            {
                                continue;
                            }

                            rpl.GetTextures().Track(texture.Id(), texture.Get());
                        }
                    }

                    if (meshComponent.skeleton != nullptr)
                    {
                        rpl.GetSkeletons().Track(meshComponent.skeleton->Id(), meshComponent.skeleton, meshComponent.skeleton->GetRenderProxyVersionPtr());
                    }
                }
            }

            break;

        case uint32(ViewFlags::COLLECT_DYNAMIC_ENTITIES):
            if ((m_flags & ViewFlags::NO_FRUSTUM_CULLING) || !(scene->GetSceneFlags() & SceneFlags::HAS_OCTREE))
            {
                for (auto [entity, meshComponent, _] : scene->GetEntityManager()->GetEntitySet<MeshComponent, TagComponent<EntityTag::MobDynamic>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
                {
                    ++numCollectedEntities;

                    rpl.GetMeshEntities().Track(entity->Id(), entity, entity->GetRenderProxyVersionPtr());

                    if (Mesh* mesh = meshComponent.mesh)
                    {
                        rpl.GetMeshes().Track(mesh->Id(), mesh);
                    }

                    if (Material* material = meshComponent.material)
                    {
                        rpl.GetMaterials().Track(material->Id(), material, material->GetRenderProxyVersionPtr());

                        for (const Handle<Texture>& texture : material->GetTextures())
                        {
                            if (!texture.IsValid())
                            {
                                continue;
                            }

                            rpl.GetTextures().Track(texture.Id(), texture.Get());
                        }
                    }

                    if (meshComponent.skeleton != nullptr)
                    {
                        rpl.GetSkeletons().Track(meshComponent.skeleton->Id(), meshComponent.skeleton, meshComponent.skeleton->GetRenderProxyVersionPtr());
                    }
                }
            }
            else
            {
                for (auto [entity, meshComponent, visibilityStateComponent, _] : scene->GetEntityManager()->GetEntitySet<MeshComponent, VisibilityStateComponent, TagComponent<EntityTag::MobDynamic>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
                {
                    if (!(visibilityStateComponent.flags & VisibilityStateFlags::ALWAYS_VISIBLE))
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

                    if (Mesh* mesh = meshComponent.mesh)
                    {
                        rpl.GetMeshes().Track(mesh->Id(), mesh);
                    }

                    if (Material* material = meshComponent.material)
                    {
                        rpl.GetMaterials().Track(material->Id(), material, material->GetRenderProxyVersionPtr());

                        for (const Handle<Texture>& texture : material->GetTextures())
                        {
                            if (!texture.IsValid())
                            {
                                continue;
                            }

                            rpl.GetTextures().Track(texture.Id(), texture.Get());
                        }
                    }

                    if (meshComponent.skeleton != nullptr)
                    {
                        rpl.GetSkeletons().Track(meshComponent.skeleton->Id(), meshComponent.skeleton, meshComponent.skeleton->GetRenderProxyVersionPtr());
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
            AssertDebug(entity->InstanceClass() == Entity::StaticClass());

            auto&& [meshComponent, transformComponent, boundingBoxComponent, lightmapElementComponent] = entity->GetEntityManager()->TryGetComponents<MeshComponent, TransformComponent, BoundingBoxComponent, LightmapElementComponent>(entity);

            AssertDebug(meshComponent != nullptr);
            AssertDebug(meshComponent->mesh && meshComponent->mesh->IsReady());
            AssertDebug(meshComponent->material && meshComponent->material->IsReady());

            RenderProxyMesh& meshProxy = *rpl.GetMeshEntities().SetProxy(entity->Id(), RenderProxyMesh());

            meshProxy.version = *entity->GetRenderProxyVersionPtr();
            meshProxy.forceRebind = false;

            meshProxy.entity = MakeWeakRef(entity);
            meshProxy.mesh = meshComponent->mesh;
            meshProxy.material = meshComponent->material;
            meshProxy.skeleton = meshComponent->skeleton;
            meshProxy.numIndices = meshComponent->mesh->NumIndices();
            meshProxy.lightmapVolume = lightmapElementComponent ? lightmapElementComponent->lightmapVolume.GetUnsafe() : nullptr;
            meshProxy.lightmapElementId = lightmapElementComponent ? lightmapElementComponent->lightmapElementId : InvalidLightmapElementId;
            meshProxy.cachedAttributes = RenderableAttributeSet(meshComponent->mesh->GetMeshAttributes(), meshComponent->material->GetRenderAttributes());

            Mat4f transformMatrix = transformComponent->GetMatrix();
            
            meshProxy.instanceData = meshComponent->instanceData;
            meshProxy.instanceData.SetBufferData(0, &transformMatrix, 1);

            meshProxy.bufferData.worldAabbMax = boundingBoxComponent ? boundingBoxComponent->worldAabb.max : MathUtil::MinSafeValue<Vec3f>();
            meshProxy.bufferData.worldAabbMin = boundingBoxComponent ? boundingBoxComponent->worldAabb.min : MathUtil::MaxSafeValue<Vec3f>();
            
            meshProxy.bufferData.modelMatrix = transformMatrix;
            meshProxy.bufferData.previousModelMatrix = meshComponent->previousModelMatrix;
            meshProxy.bufferData.normalMatrix = Mat3f(transformMatrix).Inverse().Transpose();
        }
    }
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
                    /// \todo Implement frustum culling for spot lights
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

                    for (const Handle<Texture>& texture : light->GetMaterial()->GetTextures())
                    {
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

            const BoundingBox volumeAabb = lightmapVolume->GetWorldBounds();

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

void View::CollectParticleVolumes(RenderProxyList& rpl)
{
    HYP_SCOPE;

    if (m_flags & ViewFlags::SKIP_PARTICLE_VOLUMES)
    {
        return;
    }

    for (const Handle<Scene>& scene : m_scenes)
    {
        Assert(scene.IsValid());
        Assert(scene->IsReady());

        for (auto [entity, _] : scene->GetEntityManager()->GetEntitySet<EntityType<ParticleVolume>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            ParticleVolume* volume = static_cast<ParticleVolume*>(entity);

            const BoundingBox volumeAabb = volume->GetWorldBounds();

            if (!volumeAabb.IsValid() || !volumeAabb.IsFinite())
            {
                HYP_LOG(Scene, Warning, "ParticleVolume {} has an invalid AABB in view {}", volume->Id(), Id());
                continue;
            }

            if (!(m_flags & ViewFlags::NO_FRUSTUM_CULLING) && m_camera.IsValid())
            {
                if (!m_camera->GetFrustum().ContainsAABB(volumeAabb))
                {
                    // continue;
                }
            }

            rpl.GetParticleVolumes().Track(volume->Id(), volume, volume->GetRenderProxyVersionPtr());
        }
    }
}

void View::CollectFogVolumes(RenderProxyList& rpl)
{
    HYP_SCOPE;

    if (m_flags & ViewFlags::SKIP_FOG_VOLUMES)
    {
        return;
    }

    for (const Handle<Scene>& scene : m_scenes)
    {
        Assert(scene.IsValid());
        Assert(scene->IsReady());

        for (auto [entity, _] : scene->GetEntityManager()->GetEntitySet<EntityType<FogVolume>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            FogVolume* volume = static_cast<FogVolume*>(entity);

            const BoundingBox volumeAabb = volume->GetWorldBounds();

            if (!volumeAabb.IsValid() || !volumeAabb.IsFinite())
            {
                HYP_LOG(Scene, Warning, "FogVolume {} has an invalid AABB in view {}", volume->Id(), Id());
                continue;
            }

            if (!(m_flags & ViewFlags::NO_FRUSTUM_CULLING) && m_camera.IsValid())
            {
                if (!m_camera->GetFrustum().ContainsAABB(volumeAabb))
                {
                    // continue;
                }
            }

            rpl.GetFogVolumes().Track(volume->Id(), volume, volume->GetRenderProxyVersionPtr());
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

            const BoundingBox worldBounds = envGrid->GetWorldBounds();

            if (!worldBounds.IsValid() || !worldBounds.IsFinite())
            {
                HYP_LOG(Scene, Warning, "EnvGrid {} has an invalid AABB in view {}", envGrid->Id(), Id());

                continue;
            }

            if (!m_camera->GetFrustum().ContainsAABB(worldBounds))
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
                const BoundingBox& probeAabb = probe->GetWorldBounds();

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
    }
}

#pragma endregion View

} // namespace Hyperion
