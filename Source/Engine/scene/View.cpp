/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

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
#include <rendering/InstancedMeshData.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/Texture.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/MaterialInstance.hpp>

#include <rendering/shadows/ShadowMapCache.hpp>
#include <rendering/shadows/ShadowCameraHelper.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <asset/AssetRegistry.hpp>

#include <Core/threading/Task.hpp>

#include <engine/EngineDriver.hpp>

// #define HYP_DISABLE_VISIBILITY_CHECK
// #define HYP_VISIBILITY_CHECK_DEBUG

#include <View.generated.inl>

namespace Hyperion {

HYP_API extern Pool* g_scenePool;

struct LightSorter
{
    Camera& camera;
    const Frustum& frustum;

    LightSorter(Camera& camera, const Frustum& frustum)
        : camera(camera),
          frustum(frustum)
    {
    }

    bool operator()(Light* a, Light* b) const
    {
        // Prirotize directional lights first
        if (a->GetLightType() != b->GetLightType())
        {
            return a->GetLightType() == LightType::Directional;
        }

        // Then sort by distance to camera (closest first)
        const float aDistance = (a->GetWorldTranslation() - camera.GetWorldTranslation()).LengthSquared();
        const float bDistance = (b->GetWorldTranslation() - camera.GetWorldTranslation()).LengthSquared();

        return aDistance < bDistance;
    }
};

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
    EnqueueDeletion(std::move(m_impl));
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
        return static_cast<GBuffer&>(*m_impl).GetBucket(RenderBucket::Opaque).GetFramebuffer();
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

View::View(const ViewDesc& viewDesc, Name name)
    : m_viewDesc(viewDesc),
      m_name(name),
      m_flags(viewDesc.flags),
      m_camera(MakeStrongRef(viewDesc.camera)),
      m_priority(viewDesc.priority),
      m_overrideAttributes(viewDesc.overrideAttributes),
      m_collectionTaskBatch(nullptr),
      m_overrideCollectFunctor(nullptr)
{
    for (Scene* scene : m_viewDesc.scenes)
    {
        if (!scene)
        {
            continue;
        }

        m_scenes.PushBack(scene);
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
        EnqueueDeletion(std::move(m_camera));
    }
}

void View::Init()
{
    if (m_camera.IsValid())
    {
        InitObject(m_camera);
    }

    const Vec2u extent = MathUtil::Max(m_viewDesc.framebufferDesc.extent, Vec2u::One());

    if (!(m_viewDesc.flags & ViewFlags::EXTERNAL_RENDERTARGET))
    {
        if (m_viewDesc.flags & ViewFlags::GBUFFER)
        {
            AssertDebug(m_viewDesc.framebufferDesc.numAttachments == 0,
                "View with GBuffer flag cannot have output target attachments defined, as it will use GBuffer instead.");

            m_outputTarget = ViewOutputTarget(MakeHandle<GBuffer>(extent));
        }
        else if (m_viewDesc.framebufferDesc.numAttachments > 0)
        {
            FramebufferRef framebuffer = g_renderInterface->MakeFramebuffer(m_viewDesc.framebufferDesc);

#if HYP_DEBUG_MODE
            if (m_name.IsValid())
            {
                framebuffer->SetDebugName(NAME_FMT("{}Framebuffer", m_name));
            }
            else
            {
                framebuffer->SetDebugName(NAME_FMT("{}Framebuffer", Id()));
            }
#endif

            for (uint32 attachmentIndex = 0; attachmentIndex < m_viewDesc.framebufferDesc.numAttachments; attachmentIndex++)
            {
                const AttachmentDesc& attachmentDesc = m_viewDesc.framebufferDesc.attachments[attachmentIndex];
                AssertDebug(attachmentDesc.format != InvalidTextureFormat);

                Attachment* attachment = framebuffer->AddAttachment(
                    attachmentIndex,
                    attachmentDesc);

                attachment->SetClearColor(Vec4f(attachmentDesc.clearColor));
            }

            CheckResult(framebuffer->Create());

            m_outputTarget = ViewOutputTarget(framebuffer);
        }

        Assert(m_outputTarget.IsValid(), "View with id {} must have a valid output target!", Id());
    }

    SetReady(true);
}

bool View::TestRay(const Ray& ray, RayTestResults& outResults, EnumFlags<RayTestFlags> flags) const
{
    HYP_SCOPE;
    AssertOnThread(g_simThread | ThreadCategory::THREAD_CATEGORY_TASK);

    bool hasHits = false;

    for (Scene* scene : m_scenes)
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
}

void View::UpdateVisibility()
{
    HYP_SCOPE;
    AssertOnThread(g_simThread | g_visThread);
    AssertReady();

    if (m_camera.IsValid())
    {
        m_subFrustum = m_camera->GetFrustum();
    }
    else
    {
        m_subFrustum = Frustum {};
    }

    for (Scene* scene : m_scenes)
    {
        AssertDebug(scene != nullptr);

        if (!scene || !(scene->GetSceneFlags() & SceneFlags::HAS_OCTREE))
        {
            continue;
        }

        scene->GetOctree().CalculateVisibility(this);
    }
}

void View::PrepareShadowViews(Array<View*, SceneTempAllocator>& outShadowViews)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    if (m_flags & (ViewFlags::SKIP_LIGHTS | ViewFlags::SHADOW_VIEW))
    {
        return;
    }

    // Collect all shadow casting lights into here so we can sort by distance / visibility to prioritize
    // closer lights' casting shadows.
    Array<Light*, RenderAllocator> allShadowCastingLights;
    allShadowCastingLights.Reserve(8);

    for (Scene* scene : m_scenes)
    {
        AssertDebug(scene && scene->IsReady());

        for (auto [entity, _] : scene->GetEntityManager()->GetEntitySet<EntityType<Light>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            Light* light = static_cast<Light*>(entity);

            if (!(light->GetLightFlags() & LightFlags::ShadowCaster))
                continue;
            
            bool isLightInFrustum = false;

            if (m_flags & ViewFlags::NO_FRUSTUM_CULLING)
                isLightInFrustum = true;
            else
            {
                switch (light->GetLightType())
                {
                case LightType::Directional:
                    isLightInFrustum = true;
                    break;
                case LightType::Point:
                    isLightInFrustum = m_subFrustum.ContainsBoundingSphere(light->GetBoundingSphere(true));
                    break;
                case LightType::Spot:
                    /// \todo Implement frustum culling for spot lights
                    isLightInFrustum = true;
                    break;
                case LightType::AreaRect:
                    isLightInFrustum = m_subFrustum.ContainsAABB(light->GetWorldBounds());
                    break;
                default:
                    break;
                }
            }

            //if (!isLightInFrustum)
                // Skip shadow view creation/update if the light is totally out of view.
            //    continue;

            allShadowCastingLights.PushBack(light);
        }
    }

    std::sort(allShadowCastingLights.Begin(), allShadowCastingLights.End(), LightSorter(*m_camera, m_subFrustum));

    for (Light* light : allShadowCastingLights)
    {
        const bool hasBakedStaticShadows = (light->GetLightFlags() & LightFlags::BakeStaticShadows);
        const bool cacheStaticShadowMaps = !hasBakedStaticShadows && (light->GetLightFlags() & LightFlags::CacheStaticShadowMaps);

        View* shadowViewsStatic[MaxShadowMapCascades] {};
        View* shadowViewsDynamic[MaxShadowMapCascades] {};
                    
        for (uint32 cascadeIndex = 0; cascadeIndex < light->GetNumShadowMapCascades(); cascadeIndex++)
        {
            shadowViewsDynamic[cascadeIndex] = g_renderInterface->shadowMapCache->GetOrCreateShadowView(
                this,
                light,
                cascadeIndex,
                /* isStatic */ false);

            if (!shadowViewsDynamic[cascadeIndex])
            {
                // failed to allocate shadow view - out of slots is most likely cause
                // skip processing for this light.
                break;
            }

            // We need a view specifically for static objects if we either:
            // - cache shadow maps for statics independently
            // - use baked shadow maps for statics
            if (cacheStaticShadowMaps || hasBakedStaticShadows)
            {
                shadowViewsStatic[cascadeIndex] = g_renderInterface->shadowMapCache->GetOrCreateShadowView(
                    this,
                    light,
                    cascadeIndex,
                    /* isStatic */ true);
            }

            // Update shadow map camera
            if (cascadeIndex == 0)
            {
                BoundingBox shadowBounds;

                Camera* shadowCamera = shadowViewsDynamic[0]->GetCamera();
                Assert(shadowCamera != nullptr);

                switch (light->GetLightType())
                {
                case LightType::Directional:
                {
                    ShadowCameraHelper::UpdateShadowCameraDirectional(
                        *shadowCamera,
                        Vec3f::Zero(), //m_camera.IsValid() ? m_camera->GetTranslation() : Vec3f::Zero(),
                        light->GetWorldTranslation().Normalized() * 1000.0f,
                        40.0f);

                    break;
                }
                case LightType::Point:
                    shadowBounds = light->GetWorldBounds();

                    shadowCamera->SetTranslation(light->GetWorldTranslation());

                    break;
                default:
                    HYP_LOG_ONCE(Scene, Warning, "Shadow view update not implemented for light type {}", EnumToString(light->GetLightType()));
                    break;
                }
            }

            for (View* shadowView : { shadowViewsDynamic[cascadeIndex], shadowViewsStatic[cascadeIndex] })
            {
                if (!shadowView || outShadowViews.Contains(shadowView))
                {
                    continue;
                }

                shadowView->m_scenes = m_scenes;

                outShadowViews.PushBack(shadowView);
            }
        }
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

    if (m_overrideCollectFunctor.IsValid())
    {
        batch.AddTask([&fn = m_overrideCollectFunctor, &rpl]() { fn(rpl); });

        return;
    }

    batch.AddTask([this, &rpl]()
        {
            rpl.BeginWrite();

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

void View::SetPriority(int priority)
{
    m_priority = priority;
}

void View::AddScene(Scene* scene)
{
    if (!scene)
    {
        return;
    }

    if (m_scenes.Contains(scene))
    {
        return;
    }

    m_scenes.PushBack(scene);
}

void View::RemoveScene(Scene* scene)
{
    if (!scene)
    {
        return;
    }

    auto it = m_scenes.Find(scene);

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

    for (Scene* scene : m_scenes)
    {
        AssertDebug(scene && scene->IsReady());

        if (scene->GetSceneFlags() & SceneFlags::DETACHED)
        {
            HYP_LOG(Scene, Warning, "Scene \"{}\" has DETACHED flag set, cannot collect entities for render collector!", scene->GetName());

            continue;
        }

        const VisibilityStateSnapshot visibilityStateSnapshot = scene->GetOctree().GetVisibilityState().GetSnapshot(Id());

        uint32 numCollectedEntities = 0;
        uint32 numSkippedEntities = 0;

        switch (uint32(m_flags) & uint32(ViewFlags::COLLECT_ALL_ENTITIES))
        {
        case uint32(ViewFlags::COLLECT_ALL_ENTITIES):
            if ((m_flags & ViewFlags::NO_FRUSTUM_CULLING) || !(scene->GetSceneFlags() & SceneFlags::HAS_OCTREE))
            {
                for (auto [entity, meshComponent, boundingBoxComponent] : scene->GetEntityManager()->GetEntitySet<MeshComponent, BoundingBoxComponent>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
                {
                    if (m_viewDesc.bounds.IsValid() && !m_viewDesc.bounds.Overlaps(boundingBoxComponent.worldAabb))
                    {
                        continue;
                    }

                    ++numCollectedEntities;

                    rpl.GetMeshEntities().Track(entity->Id(), entity, entity->GetRenderProxyVersionPtr());

                    if (Mesh* mesh = meshComponent.mesh)
                    {
                        rpl.GetMeshes().Track(mesh->Id(), mesh);
                    }

                    if (MaterialInstance* material = meshComponent.material)
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
                for (auto [entity, meshComponent, boundingBoxComponent, visibilityStateComponent] : scene->GetEntityManager()->GetEntitySet<MeshComponent, BoundingBoxComponent, VisibilityStateComponent>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
                {
                    if (m_viewDesc.bounds.IsValid() && !m_viewDesc.bounds.Overlaps(boundingBoxComponent.worldAabb))
                    {
                        continue;
                    }

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

                        if (!visibilityStateComponent.visibilityState->GetSnapshot(Id()).ValidToParent(visibilityStateSnapshot))
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

                    if (MaterialInstance* material = meshComponent.material)
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
                for (auto [entity, meshComponent, boundingBoxComponent, _] : scene->GetEntityManager()->GetEntitySet<MeshComponent, BoundingBoxComponent, TagComponent<EntityTag::MobStatic>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
                {
                    if (m_viewDesc.bounds.IsValid() && !m_viewDesc.bounds.Overlaps(boundingBoxComponent.worldAabb))
                    {
                        continue;
                    }

                    ++numCollectedEntities;

                    rpl.GetMeshEntities().Track(entity->Id(), entity, entity->GetRenderProxyVersionPtr());

                    if (Mesh* mesh = meshComponent.mesh)
                    {
                        rpl.GetMeshes().Track(mesh->Id(), mesh);
                    }

                    if (MaterialInstance* material = meshComponent.material)
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
                for (auto [entity, meshComponent, boundingBoxComponent, visibilityStateComponent, _] : scene->GetEntityManager()->GetEntitySet<MeshComponent, BoundingBoxComponent, VisibilityStateComponent, TagComponent<EntityTag::MobStatic>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
                {
                    if (m_viewDesc.bounds.IsValid() && !m_viewDesc.bounds.Overlaps(boundingBoxComponent.worldAabb))
                    {
                        continue;
                    }

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

                        if (!visibilityStateComponent.visibilityState->GetSnapshot(Id()).ValidToParent(visibilityStateSnapshot))
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

                    if (MaterialInstance* material = meshComponent.material)
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
                for (auto [entity, meshComponent, boundingBoxComponent, _] : scene->GetEntityManager()->GetEntitySet<MeshComponent, BoundingBoxComponent, TagComponent<EntityTag::MobDynamic>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
                {
                    if (m_viewDesc.bounds.IsValid() && !m_viewDesc.bounds.Overlaps(boundingBoxComponent.worldAabb))
                    {
                        continue;
                    }

                    ++numCollectedEntities;

                    rpl.GetMeshEntities().Track(entity->Id(), entity, entity->GetRenderProxyVersionPtr());

                    if (Mesh* mesh = meshComponent.mesh)
                    {
                        rpl.GetMeshes().Track(mesh->Id(), mesh);
                    }

                    if (MaterialInstance* material = meshComponent.material)
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
                for (auto [entity, meshComponent, boundingBoxComponent, visibilityStateComponent, _] : scene->GetEntityManager()->GetEntitySet<MeshComponent, BoundingBoxComponent, VisibilityStateComponent, TagComponent<EntityTag::MobDynamic>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
                {
                    if (m_viewDesc.bounds.IsValid() && !m_viewDesc.bounds.Overlaps(boundingBoxComponent.worldAabb))
                    {
                        continue;
                    }

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

                        if (!visibilityStateComponent.visibilityState->GetSnapshot(Id()).ValidToParent(visibilityStateSnapshot))
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

                    if (MaterialInstance* material = meshComponent.material)
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
        HYP_LOG(Scene, Verbose, "Collected {} entities for View {}, {} skipped", numCollectedEntities, Id(), numSkippedEntities);
#endif
    }

    Resources::ResourceTrackerDiff meshesDiff = rpl.GetMeshEntities().GetDiff();

    if (meshesDiff.NeedsUpdate())
    {
        Array<Entity*> added;
        rpl.GetMeshEntities().GetAdded(added, /* includeChanged */ true);

        for (Entity* entity : added)
        {
            AssertDebug(entity->InstanceClass() == Entity::StaticClass());

            auto&& [meshComponent, transformComponent, boundingBoxComponent, lightmapElementComponent] = entity->GetEntityManager()->TryGetComponents<MeshComponent, TransformComponent, BoundingBoxComponent, LightmapElementComponent>(entity);
            
            AssertDebug(meshComponent != nullptr);

            RenderProxyMesh& meshProxy = *rpl.GetMeshEntities().SetProxy(entity->Id(), RenderProxyMesh());

            if (!meshComponent->mesh || !meshComponent->material)
                continue;

            meshProxy.forceRebind = false;
            meshProxy.entity = MakeWeakRef(entity);
            meshProxy.mesh = meshComponent->mesh;
            meshProxy.material = meshComponent->material;
            meshProxy.skeleton = meshComponent->skeleton;
            meshProxy.numIndices = meshComponent->mesh->NumIndices();
            meshProxy.numInstances = meshComponent->numInstances;
            meshProxy.enableAutoInstancing = meshComponent->enableAutoInstancing;
            meshProxy.lightmapVolume = lightmapElementComponent ? lightmapElementComponent->lightmapVolume.GetUnsafe() : nullptr;
            meshProxy.lightmapElementId = lightmapElementComponent ? lightmapElementComponent->lightmapElementId : InvalidLightmapElementId;
            meshProxy.attributes = RenderableAttributeSet(meshComponent->mesh->GetMeshAttributes(), meshComponent->material->GetAttributes());

            Mat4f transformMatrix = transformComponent->GetMatrix();
            
            if (meshComponent->enableAutoInstancing || meshComponent->numInstances)
            {
                AssertDebug(m_viewDesc.entityBatchClass == nullptr || m_viewDesc.entityBatchClass == MeshEntityInstanceBatch::StaticClass());

                AssertDebug(meshComponent->instanceData.IsLoaded());

                const Handle<InstancedMeshData>& imd = ObjCast<InstancedMeshData>(meshComponent->instanceData.Resolve());
                AssertDebug(imd.IsValid());

                if (imd.IsValid())
                {
                    auto scope = imd->GetReadScope();

                    for (uint32 i = 0; i < uint32(imd->buffers.Size()); i++)
                    {
                        if (imd->buffers[i].size == 0)
                            continue;

                        meshProxy.instanceData.buffers[i].SetSize(imd->buffers[i].size, false);

                        AssertDebug(imd->buffers[i].raw != nullptr);
                        Memory::Copy(meshProxy.instanceData.buffers[i].Data(), imd->buffers[i].raw, imd->buffers[i].size);

                        meshProxy.instanceData.bufferStructSizes[i] = imd->bufferStructSizes[i];
                        meshProxy.instanceData.bufferStructAlignments[i] = imd->bufferStructAlignments[i];
                    }
                }
            }
            else
            {
                meshProxy.instanceData = {};
            }

            meshProxy.bufferData.worldAabbMax = boundingBoxComponent ? boundingBoxComponent->worldAabb.max : MathUtil::MinSafeValue<Vec3f>();
            meshProxy.bufferData.worldAabbMin = boundingBoxComponent ? boundingBoxComponent->worldAabb.min : MathUtil::MaxSafeValue<Vec3f>();
            
            meshProxy.bufferData.modelMatrix = transformMatrix;
            meshProxy.bufferData.previousModelMatrix = meshComponent->previousModelMatrix;
            meshProxy.bufferData.normalMatrix = Mat3f(transformMatrix).Inverse().Transpose();
            meshProxy.bufferData.bucket = uint32(meshComponent->material->GetAttributes().bucket);
        }
    }
}

void View::CollectCameras(RenderProxyList& rpl)
{
    HYP_SCOPE;

    // still want to consider our own camera for update
    if (m_camera.IsValid())
    {
        rpl.GetCameras().Track(m_camera.Id(), m_camera, m_camera->GetRenderProxyVersionPtr());
    }

    if (m_flags & ViewFlags::SKIP_CAMERAS)
    {
        return;
    }

    for (Scene* scene : m_scenes)
    {
        AssertDebug(scene != nullptr && scene->IsReady());

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

    for (Scene* scene : m_scenes)
    {
        AssertDebug(scene && scene->IsReady());

        for (auto [entity, _] : scene->GetEntityManager()->GetEntitySet<EntityType<Light>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            Light* light = static_cast<Light*>(entity);

            bool isLightInFrustum = false;
            bool lightCastsShadows = light->GetLightFlags() & LightFlags::ShadowCaster;

            if (m_flags & ViewFlags::NO_FRUSTUM_CULLING)
            {
                isLightInFrustum = true;
            }
            else
            {
                switch (light->GetLightType())
                {
                case LightType::Directional:
                    isLightInFrustum = true;
                    break;
                case LightType::Point:
                    isLightInFrustum = m_subFrustum.ContainsBoundingSphere(light->GetBoundingSphere(true));
                    break;
                case LightType::Spot:
                    /// \todo Implement frustum culling for spot lights
                    isLightInFrustum = true;
                    break;
                case LightType::AreaRect:
                    isLightInFrustum = m_subFrustum.ContainsAABB(light->GetWorldBounds());
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

    for (Scene* scene : m_scenes)
    {
        AssertDebug(scene && scene->IsReady());

        for (auto [entity, _] : scene->GetEntityManager()->GetEntitySet<EntityType<LightmapVolume>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            LightmapVolume* lightmapVolume = ObjCast<LightmapVolume>(entity);
            Assert(lightmapVolume != nullptr);

            const BoundingBox worldBounds = lightmapVolume->GetWorldBounds();

            if (!worldBounds.IsValid() || !worldBounds.IsFinite())
            {
                HYP_LOG(Scene, Warning, "Lightmap volume {} has an invalid AABB in view {}", lightmapVolume->Id(), Id());

                continue;
            }

            if (m_viewDesc.bounds.IsValid() && !m_viewDesc.bounds.Overlaps(worldBounds))
            {
                continue;
            }

            if (!m_subFrustum.ContainsAABB(worldBounds))
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

    for (Scene* scene : m_scenes)
    {
        AssertDebug(scene && scene->IsReady());

        for (auto [entity, _] : scene->GetEntityManager()->GetEntitySet<EntityType<ParticleVolume>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            ParticleVolume* volume = static_cast<ParticleVolume*>(entity);

            const BoundingBox worldBounds = volume->GetWorldBounds();

            if (!worldBounds.IsValid() || !worldBounds.IsFinite())
            {
                HYP_LOG(Scene, Warning, "ParticleVolume {} has an invalid AABB in view {}", volume->Id(), Id());
                continue;
            }

            if (m_viewDesc.bounds.IsValid() && !m_viewDesc.bounds.Overlaps(worldBounds))
            {
                continue;
            }

            if (!(m_flags & ViewFlags::NO_FRUSTUM_CULLING))
            {
                if (!m_subFrustum.ContainsAABB(worldBounds))
                {
                    continue;
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

    for (Scene* scene : m_scenes)
    {
        AssertDebug(scene && scene->IsReady());

        for (auto [entity, _] : scene->GetEntityManager()->GetEntitySet<EntityType<FogVolume>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            FogVolume* volume = static_cast<FogVolume*>(entity);

            const BoundingBox worldBounds = volume->GetWorldBounds();

            if (!worldBounds.IsValid() || !worldBounds.IsFinite())
            {
                HYP_LOG(Scene, Warning, "FogVolume {} has an invalid AABB in view {}", volume->Id(), Id());
                continue;
            }

            if (m_viewDesc.bounds.IsValid() && !m_viewDesc.bounds.Overlaps(worldBounds))
            {
                continue;
            }

            if (!(m_flags & ViewFlags::NO_FRUSTUM_CULLING))
            {
                if (!m_subFrustum.ContainsAABB(worldBounds))
                {
                    continue;
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

    for (Scene* scene : m_scenes)
    {
        AssertDebug(scene && scene->IsReady());

        for (auto [entity, _] : scene->GetEntityManager()->GetEntitySet<EntityType<EnvGrid>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            EnvGrid* envGrid = static_cast<EnvGrid*>(entity);

            const BoundingBox worldBounds = envGrid->GetWorldBounds();

            if (!worldBounds.IsValid() || !worldBounds.IsFinite())
            {
                HYP_LOG(Scene, Warning, "EnvGrid {} has an invalid AABB in view {}", envGrid->Id(), Id());

                continue;
            }

            if (m_viewDesc.bounds.IsValid() && !m_viewDesc.bounds.Overlaps(worldBounds))
            {
                continue;
            }

            if (!m_subFrustum.ContainsAABB(worldBounds))
            {
                HYP_LOG(Scene, Verbose, "EnvGrid {} is not in frustum of View {}", envGrid->Id(), Id());

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

    for (Scene* scene : m_scenes)
    {
        AssertDebug(scene && scene->IsReady());

        for (auto [entity, _] : scene->GetEntityManager()->GetEntitySet<EntityType<EnvProbe>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            EnvProbe* probe = static_cast<EnvProbe*>(entity);

            if (!probe->IsSkyProbe())
            {
                const BoundingBox worldBounds = probe->GetWorldBounds();

                if (!worldBounds.IsValid() || !worldBounds.IsFinite())
                {
                    HYP_LOG(Scene, Warning, "EnvProbe {} has an invalid AABB in view {}", probe->Id(), Id());

                    continue;
                }

                if (m_viewDesc.bounds.IsValid() && !m_viewDesc.bounds.Overlaps(worldBounds))
                {
                    continue;
                }

                if (!(m_flags & ViewFlags::NO_FRUSTUM_CULLING) && !m_subFrustum.ContainsAABB(worldBounds))
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
