/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/View.hpp>
#include <Scene/Scene.hpp>
#include <Scene/Light.hpp>
#include <Scene/ProbeVolume.hpp>
#include <Scene/EnvProbe.hpp>
#include <Scene/EntityManager.hpp>
#include <Scene/EntityTag.hpp>
#include <Scene/ParticleVolume.hpp>
#include <Scene/FogVolume.hpp>
#include <Scene/LightmapVolume.hpp>
#include <Scene/Sprite.hpp>
#include <Scene/TextSprite.hpp>

#include <Scene/Camera/Camera.hpp>
#include <Scene/Animation/Skeleton.hpp>

#include <Scene/Components/MeshComponent.hpp>
#include <Scene/Components/TransformComponent.hpp>
#include <Scene/Components/BoundingBoxComponent.hpp>
#include <Scene/Components/VisibilityStateComponent.hpp>
#include <Scene/Components/LightmapElementComponent.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/RendererMain.hpp>
#include <Rendering/RenderProxyList.hpp>
#include <Rendering/RenderProxy.hpp>
#include <Rendering/InstancedMeshData.hpp>
#include <Rendering/GBuffer.hpp>
#include <Rendering/Texture.hpp>
#include <Rendering/Mesh.hpp>
#include <Rendering/Material.hpp>

#include <Rendering/Shadows/ShadowMapCache.hpp>
#include <Rendering/Shadows/ShadowCameraHelper.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <Asset/AssetRegistry.hpp>

#include <Core/Math/Mat3f.hpp>

#include <Core/Threading/Task.hpp>

#include <Framework/EngineDriver.hpp>

// #define HYP_DISABLE_VISIBILITY_CHECK
// #define HYP_VISIBILITY_CHECK_DEBUG

#include <View.generated.inl>

namespace Hyperion {

static constexpr float ShadowCascadeClipDistances[] = { 0.075f, 0.15f, 0.3f, 1.0f };
static constexpr float ShadowCascadeMaxDistance = 150.0f;

// Always mark dirty at this value
static const int s_dirtyResourceVersion = -1;
static const int* s_dirtyResourceVersionPtr = &s_dirtyResourceVersion;

#define GET_RESOURCE_VERSION(res) (!m_markAllAsDirty ? (res)->GetRenderProxyVersionPtr() : s_dirtyResourceVersionPtr)

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

    if (m_impl->IsA<GBuffer>())
    {
        return StaticCast<GBuffer>(m_impl);
    }

    return Handle<GBuffer>::Null();
}

const FramebufferRef& ViewOutputTarget::GetFramebuffer() const
{
    if (!m_impl)
    {
        return FramebufferRef::Null();
    }

    if (m_impl->IsA<GBuffer>())
    {
        return StaticCast<GBuffer>(m_impl)->GetPass(GBufferPass::Opaque).framebuffer;
    }

    return StaticCast<Framebuffer>(m_impl);
}

const FramebufferRef& ViewOutputTarget::GetFramebuffer(GBufferPass pass) const
{
    if (!m_impl)
    {
        return FramebufferRef::Null();
    }

    if (m_impl->IsA<GBuffer>())
    {
        return StaticCast<GBuffer>(m_impl)->GetPass(pass).framebuffer;
    }

    return StaticCast<Framebuffer>(m_impl);
}

Span<const FramebufferRef> ViewOutputTarget::GetFramebuffers() const
{
    if (!m_impl)
    {
        return {};
    }

    if (m_impl->IsA(GBuffer::StaticClass()))
    {
        return StaticCast<GBuffer>(m_impl)->GetFramebuffers();
    }

    return { &StaticCast<Framebuffer>(m_impl), 1 };
}

#pragma endregion ViewOutputTarget

#pragma region View

View::View()
    : View(ViewDesc {})
{
}

View::View(const ViewDesc& viewDesc, Name name)
    : desc(viewDesc),
      name(name),
      flags(viewDesc.flags),
      priority(viewDesc.priority),
      m_camera(viewDesc.camera),
      m_overrideAttributes(viewDesc.overrideAttributes),
      m_collectionTaskBatch(nullptr),
      m_overrideCollectFunctor(nullptr)
{
    for (Scene* scene : viewDesc.scenes)
    {
        if (!scene)
        {
            continue;
        }

        m_scenes.PushBack(scene);
    }

    for (auto it = std::begin(m_renderProxyLists); it != std::end(m_renderProxyLists); ++it)
    {
        if ((flags & ViewFlags::NOT_MULTI_BUFFERED) && it != std::begin(m_renderProxyLists))
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

    for (size_t i = 0; i < std::size(m_renderProxyLists); i++)
    {
        if ((flags & ViewFlags::NOT_MULTI_BUFFERED) && i > 0)
        {
            break;
        }

        delete m_renderProxyLists[i];
    }
}

void View::Init()
{
    if (m_camera)
    {
        InitObject(m_camera);
    }

    const Vec2u extent = MathUtil::Max(desc.framebufferDesc.extent, Vec2u::One());

    if (!(flags & ViewFlags::EXTERNAL_RENDERTARGET))
    {
        if (flags & ViewFlags::GBUFFER)
        {
            AssertDebug(desc.framebufferDesc.numAttachments == 0,
                        "View with GBuffer flag cannot have output target attachments defined, as it will use GBuffer instead.");

            m_outputTarget = ViewOutputTarget(MakeHandle<GBuffer>(extent));
        }
        else if (desc.framebufferDesc.numAttachments > 0)
        {
            FramebufferRef framebuffer = RI.MakeFramebuffer(desc.framebufferDesc);

#ifdef HYP_RHI_DEBUG_NAMES
            if (name.IsValid())
            {
                framebuffer->SetDebugName(NAME_FMT("{}Framebuffer", name));
            }
            else
            {
                framebuffer->SetDebugName(NAME_FMT("{}Framebuffer", Id()));
            }
#endif

            for (uint32 attachmentIndex = 0; attachmentIndex < desc.framebufferDesc.numAttachments; attachmentIndex++)
            {
                const AttachmentDesc& attachmentDesc = desc.framebufferDesc.attachments[attachmentIndex];
                AssertDebug(attachmentDesc.format != InvalidTextureFormat);

                Attachment* attachment = framebuffer->AddAttachment(
                    attachmentIndex,
                    attachmentDesc);

                attachment->SetClearColor(Vec4f(attachmentDesc.clearColor));
            }

            Check(framebuffer->Create());

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
    //AssertOnThread(g_simThread | g_visThread);
    AssertReady();

    if (!(flags & ViewFlags::SHADOW_VIEW))
    {
        // Shadow views update their own frustums and VP matrices (View::PrepareShadowViews())

        // Cubemap face views do not automatically update the sub-frustum
        if (!(flags & ViewFlags::CUBEMAP_FACE_VIEW))
        {
            if (m_camera)
            {
                cachedMatrices.view = m_camera->GetViewMatrix();
                cachedMatrices.viewProj = m_camera->GetViewProjectionMatrix();
                cachedMatrices.invProj = m_camera->GetProjectionMatrix().Inverse();
            }
            else
            {
                cachedMatrices = {};
            }
        }

        cachedFrustum.SetFromViewProjectionMatrix(cachedMatrices.viewProj);

        cachedBounds = BoundingBox();
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

    if (flags & (ViewFlags::SKIP_LIGHTS | ViewFlags::SHADOW_VIEW | ViewFlags::NO_SHADOW_VIEWS))
    {
        return;
    }

    // Collect all shadow casting lights into here so we can sort by distance / visibility to prioritize
    // closer lights' casting shadows.
    Array<Light*, SceneTempAllocator> allShadowCastingLights;
    allShadowCastingLights.Reserve(8);

    for (Scene* scene : m_scenes)
    {
        for (auto [entity, _] : scene->GetEntityManager()->GetEntitySet<EntityType<Light>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            Light* light = static_cast<Light*>(entity);

            if (!(light->GetLightFlags() & LightFlags::ShadowCaster))
            {
                continue;
            }

            bool isLightInFrustum = false;

            if (flags & ViewFlags::NO_FRUSTUM_CULLING)
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
                    isLightInFrustum = cachedFrustum.ContainsBoundingSphere(light->GetBoundingSphere(true));
                    break;
                case LightType::Spot:
                    /// \todo Implement frustum culling for spot lights
                    isLightInFrustum = true;
                    break;
                case LightType::AreaRect:
                    isLightInFrustum = cachedFrustum.ContainsAABB(light->GetWorldBounds());
                    break;
                default:
                    break;
                }
            }

            if (!isLightInFrustum)
            {
                // Skip shadow view creation/update if the light is totally out of view.
                continue;
            }

            allShadowCastingLights.PushBack(light);
        }
    }

    std::sort(allShadowCastingLights.Begin(), allShadowCastingLights.End(), LightSorter(*m_camera, cachedFrustum));

    for (Light* light : allShadowCastingLights)
    {
        const Vec2u shadowMapDimensions = light->GetShadowMapDimensions();

        const bool hasBakedStaticShadows = (light->GetLightFlags() & LightFlags::BakeStaticShadows);
        const bool cacheStaticShadowMaps = !hasBakedStaticShadows && (light->GetLightFlags() & LightFlags::CacheStaticShadowMaps);

        View* shadowViewsStatic[6] {};
        View* shadowViewsDynamic[6] {};

        const bool isOmni = (light->GetLightType() == LightType::Point);
        const bool isDirectional = (light->GetLightType() == LightType::Directional);

        const uint32 numCascades = (isDirectional ? light->GetNumShadowMapCascades() : 1);

        const uint32 numShadowViews = isOmni ? 6 : numCascades;

        // outside of loop because we share a view matrix for CSM
        Mat4f shadowViewMatrix;
        Mat4f shadowViewProjMatrix;
        Mat4f shadowInvProjMatrix;

        Frustum csmMainCameraFrustum;

        // Calculate total world bounds for CSM
        BoundingSphere worldBoundsSphere;
        bool isWorldBoundsSphereValid = false;

        // Scenes that are applicable to the shadow view
        Array<Scene*, SceneTempAllocator> shadowViewScenes;
        shadowViewScenes.Reserve(m_scenes.Size());

        // Calculate frustum to use for calculating splits
        if (isDirectional)
        {
            Assert(m_camera != nullptr);

            const Frustum& mainCameraFrustum = m_camera->GetFrustum();

            const float mainCameraFarRatio = MathUtil::Clamp(ShadowCascadeMaxDistance / m_camera->GetFarClip(), 0.0f, 1.0f);
            csmMainCameraFrustum = (mainCameraFarRatio >= 0.9999f ? mainCameraFrustum : mainCameraFrustum.SubFrustum(0.0f, mainCameraFarRatio));
        }

        // Collect scenes
        for (Scene* scene : m_scenes)
        {
            static constexpr EnumFlags<SceneFlags> SceneFlagFilterMask = (SceneFlags::FOREGROUND | SceneFlags::BACKDROP | SceneFlags::DETACHED);
            static constexpr EnumFlags<SceneFlags> SceneFlagFilterDesired = (SceneFlags::FOREGROUND);

            if ((scene->GetSceneFlags() & SceneFlagFilterMask) != SceneFlagFilterDesired)
            {
                continue;
            }

            if (isDirectional)
            {
                if (!(scene->GetSceneFlags() & SceneFlags::HAS_OCTREE))
                {
                    // Only want scenes with an octree for CSM
                    continue;
                }

                const BoundingBox& sceneWorldBounds = scene->GetOctree().GetAABB();
                BoundingSphere sceneWorldBoundsSphere = BoundingSphere(sceneWorldBounds);

                if (!sceneWorldBoundsSphere.IsFinite() || !sceneWorldBoundsSphere.IsValid())
                {
                    HYP_LOG_ONCE(Scene, Warning, "Scene has invalid bounding sphere (Scene: {})", scene->GetName());
                    continue;
                }

                if (isWorldBoundsSphereValid)
                {
                    worldBoundsSphere.Extend(sceneWorldBoundsSphere);
                }
                else
                {
                    worldBoundsSphere = sceneWorldBoundsSphere;
                    isWorldBoundsSphereValid = true;
                }
            }

            shadowViewScenes.PushBack(scene);
        }

        // Shared CSM view matrix, anchored to the scene bounds so it is stable as the camera moves.
        if (isDirectional && isWorldBoundsSphereValid)
        {
            shadowViewMatrix = ShadowCameraHelpers::CalculateShadowViewMatrix(
                worldBoundsSphere,
                light->GetWorldTranslation().Normalized());
        }

        for (uint32 shadowViewIndex = 0; shadowViewIndex < numShadowViews; shadowViewIndex++)
        {
            Frustum shadowViewFrustum;
            BoundingBox shadowViewBounds;

            float depthRange = 0.0f;

            if (isDirectional)
            {
                const float nearRatio = (shadowViewIndex == 0) ? 0.0f : ShadowCascadeClipDistances[shadowViewIndex - 1];
                const float farRatio = ShadowCascadeClipDistances[shadowViewIndex];

                const Vec3f lightDir = light->GetWorldTranslation().Normalized();

                shadowViewBounds = ShadowCameraHelpers::CalculateCascadeBounds(
                    csmMainCameraFrustum,
                    worldBoundsSphere,
                    shadowViewMatrix,
                    shadowMapDimensions,
                    nearRatio,
                    farRatio,
                    lightDir);

                const Mat4f cascadeProjMatrix = Mat4f::Orthographic(
                    shadowViewBounds.min.x, shadowViewBounds.max.x,
                    shadowViewBounds.min.y, shadowViewBounds.max.y,
                    shadowViewBounds.min.z, shadowViewBounds.max.z);

                shadowInvProjMatrix = cascadeProjMatrix.Inverse();
                shadowViewProjMatrix = cascadeProjMatrix * shadowViewMatrix;

                shadowViewFrustum.SetFromViewProjectionMatrix(shadowViewProjMatrix);

                depthRange = shadowViewBounds.max.z - shadowViewBounds.min.z;
            }

            // Create Shadow views
            shadowViewsDynamic[shadowViewIndex] = RI.shadowMapCache->GetOrCreateShadowView(
                this,
                light,
                shadowViewIndex,
                depthRange,
                /* isStatic */ false);

            if (!shadowViewsDynamic[shadowViewIndex])
            {
                // failed to allocate shadow view - out of slots is most likely cause
                // skip processing for this light.
                HYP_LOG_ONCE(Scene, Warning, "Failed to allocate shadow view for light {}, view: {} (id: {})", light->GetName(), GetName(), Id());
                break;
            }

            // We need a view specifically for static objects if we either:
            // - cache shadow maps for statics independently
            // - use baked shadow maps for statics
            if (cacheStaticShadowMaps || hasBakedStaticShadows)
            {
                shadowViewsStatic[shadowViewIndex] = RI.shadowMapCache->GetOrCreateShadowView(
                    this,
                    light,
                    shadowViewIndex,
                    depthRange,
                    /* isStatic */ true);
            }

            if (!isDirectional)
            {
                Camera* shadowCamera = shadowViewsDynamic[0]->GetCamera();
                Assert(shadowCamera != nullptr);

                // Update shadow map camera on first view seen.
                if (shadowViewIndex == 0)
                {
                    shadowCamera->SetWorldTranslation(light->GetWorldTranslation());
                }

                if (isOmni)
                {
                    // Calculate matrices and frustum for this cube face

                    shadowViewMatrix = Mat4f::LookAt(Texture::s_cubemapDirections[shadowViewIndex].first, Texture::s_cubemapDirections[shadowViewIndex].second)
                        * Mat4f::Translation(-shadowCamera->GetWorldTranslation());

                    shadowViewProjMatrix = shadowCamera->GetProjectionMatrix() * shadowViewMatrix;
                    shadowInvProjMatrix = shadowCamera->GetProjectionMatrix().Inverse();

                    shadowViewFrustum.SetFromViewProjectionMatrix(shadowViewProjMatrix);
                }
                else
                {
                    // @TODO : Matrix calculations for other light types.
                    HYP_LOG_ONCE(Scene, Warning, "Shadow matrix calculation not implemented for light type: {}", EnumToString(light->GetLightType()));

                    continue;
                }
            }

            for (View* shadowView : { shadowViewsDynamic[shadowViewIndex], shadowViewsStatic[shadowViewIndex] })
            {
                if (!shadowView || outShadowViews.Contains(shadowView))
                {
                    continue;
                }

                shadowView->cachedMatrices.view = shadowViewMatrix;
                shadowView->cachedMatrices.viewProj = shadowViewProjMatrix;
                shadowView->cachedMatrices.invProj = shadowInvProjMatrix;

                shadowView->cachedFrustum = shadowViewFrustum;

                shadowView->cachedBounds = shadowViewBounds;

                shadowView->m_scenes.Resize(shadowViewScenes.Size());
                std::copy(shadowViewScenes.Begin(), shadowViewScenes.End(), shadowView->m_scenes.Begin());

                const bool shouldSkipUnchangedViews = (shadowView == shadowViewsStatic[shadowViewIndex]);

                if (shouldSkipUnchangedViews)
                {
                    HashCode inputHash = HashCode::GetHashCode(*light->GetRenderProxyVersionPtr())
                        .Combine(shadowViewProjMatrix.GetHashCode());

                    for (Scene* shadowViewScene : shadowViewScenes)
                    {
                        if (!shadowViewScene || !(shadowViewScene->GetSceneFlags() & SceneFlags::HAS_OCTREE))
                        {
                            continue;
                        }

                        inputHash = inputHash.Combine(shadowViewScene->GetOctree().GetEntryListHash<EntityTag::MobStatic>());
                    }

                    shadowView->collectionState.UpdateInputs(inputHash, GetFrameCounter());
                }

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

    if (m_overrideCollectFunctor.IsValid())
    {
        batch.AddTask(
            [this]()
            {
                UpdateVisibility();

                m_overrideCollectFunctor(GetProducerProxyList(this));
            });

        return;
    }

    batch.AddTask(
        [this]()
        {
            UpdateVisibility();

            RenderProxyList& rpl = GetProducerProxyList(this);

            rpl.BeginWrite();

            rpl.priority = priority;

            rpl.cachedMatrices = cachedMatrices;
            rpl.cachedBounds = cachedBounds;

            CollectCameras(rpl);
            CollectLights(rpl);
            CollectLightmapVolumes(rpl);
            CollectParticleVolumes(rpl);
            CollectFogVolumes(rpl);
            CollectProbeVolumes(rpl);
            CollectEnvProbes(rpl);
            CollectSprites(rpl);
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

    m_markAllAsDirty = false;
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
    this->priority = priority;
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
        if (scene->GetSceneFlags() & SceneFlags::DETACHED)
        {
            HYP_LOG(Scene, Warning, "Scene \"{}\" has DETACHED flag set, cannot collect entities for render collector!", scene->GetName());

            continue;
        }

        const VisibilityStateSnapshot visibilityStateSnapshot = scene->GetOctree().GetVisibilityState().GetSnapshot(Id());

        [[maybe_unused]] uint32 numCollectedEntities = 0;
        [[maybe_unused]] uint32 numSkippedEntities = 0;

        switch (uint32(flags) & uint32(ViewFlags::COLLECT_ALL_ENTITIES))
        {
        case uint32(ViewFlags::COLLECT_ALL_ENTITIES):
            if ((flags & ViewFlags::NO_FRUSTUM_CULLING) || !(scene->GetSceneFlags() & SceneFlags::HAS_OCTREE))
            {
                for (auto [entity, meshComponent, boundingBoxComponent] : scene->GetEntityManager()->GetEntitySet<MeshComponent, BoundingBoxComponent>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
                {
                    if (desc.bounds.IsValid() && !desc.bounds.Overlaps(boundingBoxComponent.worldAabb))
                    {
                        continue;
                    }

                    if (!meshComponent.mesh || !meshComponent.material)
                    {
                        continue;
                    }

                    ++numCollectedEntities;

                    rpl.GetMeshEntities().Track(entity->Id(), entity, GET_RESOURCE_VERSION(entity));

                    if (Mesh* mesh = meshComponent.mesh)
                    {
                        rpl.GetMeshes().Track(mesh->Id(), mesh);
                    }

                    if (Material* material = meshComponent.material)
                    {
                        rpl.GetMaterials().Track(material->Id(), material, GET_RESOURCE_VERSION(material));

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
                        rpl.GetSkeletons().Track(meshComponent.skeleton->Id(), meshComponent.skeleton, GET_RESOURCE_VERSION(meshComponent.skeleton));
                    }
                }
            }
            else
            {
                for (auto [entity, meshComponent, boundingBoxComponent, visibilityStateComponent] : scene->GetEntityManager()->GetEntitySet<MeshComponent, BoundingBoxComponent, VisibilityStateComponent>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
                {
                    if (desc.bounds.IsValid() && !desc.bounds.Overlaps(boundingBoxComponent.worldAabb))
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

                    if (!meshComponent.mesh || !meshComponent.material)
                    {
                        continue;
                    }

                    ++numCollectedEntities;

                    rpl.GetMeshEntities().Track(entity->Id(), entity, GET_RESOURCE_VERSION(entity));

                    if (Mesh* mesh = meshComponent.mesh)
                    {
                        rpl.GetMeshes().Track(mesh->Id(), mesh);
                    }

                    if (Material* material = meshComponent.material)
                    {
                        rpl.GetMaterials().Track(material->Id(), material, GET_RESOURCE_VERSION(material));

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
                        rpl.GetSkeletons().Track(meshComponent.skeleton->Id(), meshComponent.skeleton, GET_RESOURCE_VERSION(meshComponent.skeleton));
                    }
                }
            }

            break;

        case uint32(ViewFlags::COLLECT_STATIC_ENTITIES):
            if ((flags & ViewFlags::NO_FRUSTUM_CULLING) || !(scene->GetSceneFlags() & SceneFlags::HAS_OCTREE))
            {
                for (auto [entity, meshComponent, boundingBoxComponent, _] : scene->GetEntityManager()->GetEntitySet<MeshComponent, BoundingBoxComponent, TagComponent<EntityTag::MobStatic>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
                {
                    if (desc.bounds.IsValid() && !desc.bounds.Overlaps(boundingBoxComponent.worldAabb))
                    {
                        continue;
                    }

                    if (!meshComponent.mesh || !meshComponent.material)
                    {
                        continue;
                    }

                    ++numCollectedEntities;

                    rpl.GetMeshEntities().Track(entity->Id(), entity, GET_RESOURCE_VERSION(entity));

                    if (Mesh* mesh = meshComponent.mesh)
                    {
                        rpl.GetMeshes().Track(mesh->Id(), mesh);
                    }

                    if (Material* material = meshComponent.material)
                    {
                        rpl.GetMaterials().Track(material->Id(), material, GET_RESOURCE_VERSION(material));

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
                        rpl.GetSkeletons().Track(meshComponent.skeleton->Id(), meshComponent.skeleton, GET_RESOURCE_VERSION(meshComponent.skeleton));
                    }
                }
            }
            else
            {
                for (auto [entity, meshComponent, boundingBoxComponent, visibilityStateComponent, _] : scene->GetEntityManager()->GetEntitySet<MeshComponent, BoundingBoxComponent, VisibilityStateComponent, TagComponent<EntityTag::MobStatic>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
                {
                    if (desc.bounds.IsValid() && !desc.bounds.Overlaps(boundingBoxComponent.worldAabb))
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

                    if (!meshComponent.mesh || !meshComponent.material)
                    {
                        continue;
                    }

                    ++numCollectedEntities;

                    rpl.GetMeshEntities().Track(entity->Id(), entity, GET_RESOURCE_VERSION(entity));

                    if (Mesh* mesh = meshComponent.mesh)
                    {
                        rpl.GetMeshes().Track(mesh->Id(), mesh);
                    }

                    if (Material* material = meshComponent.material)
                    {
                        rpl.GetMaterials().Track(material->Id(), material, GET_RESOURCE_VERSION(material));

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
                        rpl.GetSkeletons().Track(meshComponent.skeleton->Id(), meshComponent.skeleton, GET_RESOURCE_VERSION(meshComponent.skeleton));
                    }
                }
            }

            break;

        case uint32(ViewFlags::COLLECT_DYNAMIC_ENTITIES):
            if ((flags & ViewFlags::NO_FRUSTUM_CULLING) || !(scene->GetSceneFlags() & SceneFlags::HAS_OCTREE))
            {
                for (auto [entity, meshComponent, boundingBoxComponent, _] : scene->GetEntityManager()->GetEntitySet<MeshComponent, BoundingBoxComponent, TagComponent<EntityTag::MobDynamic>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
                {
                    if (desc.bounds.IsValid() && !desc.bounds.Overlaps(boundingBoxComponent.worldAabb))
                    {
                        continue;
                    }

                    if (!meshComponent.mesh || !meshComponent.material)
                    {
                        continue;
                    }

                    ++numCollectedEntities;

                    rpl.GetMeshEntities().Track(entity->Id(), entity, GET_RESOURCE_VERSION(entity));

                    if (Mesh* mesh = meshComponent.mesh)
                    {
                        rpl.GetMeshes().Track(mesh->Id(), mesh);
                    }

                    if (Material* material = meshComponent.material)
                    {
                        rpl.GetMaterials().Track(material->Id(), material, GET_RESOURCE_VERSION(material));

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
                        rpl.GetSkeletons().Track(meshComponent.skeleton->Id(), meshComponent.skeleton, GET_RESOURCE_VERSION(meshComponent.skeleton));
                    }
                }
            }
            else
            {
                for (auto [entity, meshComponent, boundingBoxComponent, visibilityStateComponent, _] : scene->GetEntityManager()->GetEntitySet<MeshComponent, BoundingBoxComponent, VisibilityStateComponent, TagComponent<EntityTag::MobDynamic>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
                {
                    if (desc.bounds.IsValid() && !desc.bounds.Overlaps(boundingBoxComponent.worldAabb))
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

                    if (!meshComponent.mesh || !meshComponent.material)
                    {
                        continue;
                    }

                    ++numCollectedEntities;

                    rpl.GetMeshEntities().Track(entity->Id(), entity, GET_RESOURCE_VERSION(entity));

                    if (Mesh* mesh = meshComponent.mesh)
                    {
                        rpl.GetMeshes().Track(mesh->Id(), mesh);
                    }

                    if (Material* material = meshComponent.material)
                    {
                        rpl.GetMaterials().Track(material->Id(), material, GET_RESOURCE_VERSION(material));

                        for (Texture* texture : material->GetTextures())
                        {
                            if (!texture)
                            {
                                continue;
                            }

                            rpl.GetTextures().Track(texture->Id(), texture);
                        }
                    }

                    if (meshComponent.skeleton != nullptr)
                    {
                        rpl.GetSkeletons().Track(meshComponent.skeleton->Id(), meshComponent.skeleton, GET_RESOURCE_VERSION(meshComponent.skeleton));
                    }
                }
            }

            break;
        default:
            break;
        }

#ifdef HYP_VISIBILITY_CHECK_DEBUG
        HYP_LOG(Scene, Debug, "Collected {} entities for View {}, {} skipped", numCollectedEntities, GetName(), numSkippedEntities);
#endif
    }
}

void View::CollectCameras(RenderProxyList& rpl)
{
    HYP_SCOPE;

    // still want to consider our own camera for update
    if (m_camera)
    {
        rpl.GetCameras().Track(m_camera->Id(), m_camera, GET_RESOURCE_VERSION(m_camera));
    }

    if (flags & ViewFlags::SKIP_CAMERAS)
    {
        return;
    }

    for (Scene* scene : m_scenes)
    {
        for (auto [entity, _] : scene->GetEntityManager()->GetEntitySet<EntityType<Camera>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            Camera* camera = StaticCast<Camera>(entity);

            rpl.GetCameras().Track(camera->Id(), camera, GET_RESOURCE_VERSION(camera));
        }
    }
}

void View::CollectLights(RenderProxyList& rpl)
{
    HYP_SCOPE;

    if (flags & ViewFlags::SKIP_LIGHTS)
    {
        return;
    }

    for (Scene* scene : m_scenes)
    {
        for (auto [entity, _] : scene->GetEntityManager()->GetEntitySet<EntityType<Light>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            Light* light = StaticCast<Light>(entity);

            bool isLightInFrustum = false;
            bool lightCastsShadows = light->GetLightFlags() & LightFlags::ShadowCaster;

            if (flags & ViewFlags::NO_FRUSTUM_CULLING)
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
                    isLightInFrustum = cachedFrustum.ContainsBoundingSphere(light->GetBoundingSphere(true));
                    break;
                case LightType::Spot:
                    /// \todo Implement frustum culling for spot lights
                    isLightInFrustum = true;
                    break;
                case LightType::AreaRect:
                    isLightInFrustum = cachedFrustum.ContainsAABB(light->GetWorldBounds());
                    break;
                default:
                    break;
                }
            }

            if (isLightInFrustum)
            {
                rpl.GetLights().Track(light->Id(), light, GET_RESOURCE_VERSION(light));

                if (light->GetMaterial().IsValid())
                {
                    rpl.GetMaterials().Track(light->GetMaterial()->Id(), light->GetMaterial().Get());

                    for (Texture* texture : light->GetMaterial()->GetTextures())
                    {
                        if (!texture)
                        {
                            continue;
                        }

                        rpl.GetTextures().Track(texture->Id(), texture);
                    }
                }
            }
        }
    }
}

void View::CollectLightmapVolumes(RenderProxyList& rpl)
{
    HYP_SCOPE;

    if (flags & ViewFlags::SKIP_LIGHTMAP_VOLUMES)
    {
        return;
    }

    for (Scene* scene : m_scenes)
    {
        for (auto [entity, _] : scene->GetEntityManager()->GetEntitySet<EntityType<LightmapVolume>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            LightmapVolume* lightmapVolume = StaticCast<LightmapVolume>(entity);

            const BoundingBox worldBounds = lightmapVolume->GetWorldBounds();

            if (!worldBounds.IsValid() || !worldBounds.IsFinite())
            {
                HYP_LOG(Scene, Warning, "Lightmap volume {} has an invalid AABB in view {}", lightmapVolume->Id(), Id());

                continue;
            }

            if (desc.bounds.IsValid() && !desc.bounds.Overlaps(worldBounds))
            {
                continue;
            }

            if (!cachedFrustum.ContainsAABB(worldBounds))
            {
                continue;
            }

            rpl.GetLightmapVolumes().Track(lightmapVolume->Id(), lightmapVolume, GET_RESOURCE_VERSION(lightmapVolume));
        }
    }
}

void View::CollectParticleVolumes(RenderProxyList& rpl)
{
    HYP_SCOPE;

    if (flags & ViewFlags::SKIP_PARTICLE_VOLUMES)
    {
        return;
    }

    for (Scene* scene : m_scenes)
    {
        for (auto [entity, _] : scene->GetEntityManager()->GetEntitySet<EntityType<ParticleVolume>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            ParticleVolume* volume = StaticCast<ParticleVolume>(entity);

            const BoundingBox worldBounds = volume->GetWorldBounds();

            if (!worldBounds.IsValid() || !worldBounds.IsFinite())
            {
                HYP_LOG(Scene, Warning, "ParticleVolume {} has an invalid AABB in view {}", volume->Id(), Id());
                continue;
            }

            if (desc.bounds.IsValid() && !desc.bounds.Overlaps(worldBounds))
            {
                continue;
            }

            if (!(flags & ViewFlags::NO_FRUSTUM_CULLING))
            {
                if (!cachedFrustum.ContainsAABB(worldBounds))
                {
                    continue;
                }
            }

            if (volume->texture.IsValid())
            {
                rpl.GetTextures().Track(volume->texture.Id(), volume->texture);
            }

            if (volume->mesh.IsValid())
            {
                rpl.GetMeshes().Track(volume->mesh.Id(), volume->mesh);
            }

            rpl.GetParticleVolumes().Track(volume->Id(), volume, GET_RESOURCE_VERSION(volume));
        }
    }
}

void View::CollectFogVolumes(RenderProxyList& rpl)
{
    HYP_SCOPE;

    if (flags & ViewFlags::SKIP_FOG_VOLUMES)
    {
        return;
    }

    for (Scene* scene : m_scenes)
    {
        for (auto [entity, _] : scene->GetEntityManager()->GetEntitySet<EntityType<FogVolume>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            FogVolume* volume = StaticCast<FogVolume>(entity);

            const BoundingBox worldBounds = volume->GetWorldBounds();

            if (!worldBounds.IsValid() || !worldBounds.IsFinite())
            {
                HYP_LOG(Scene, Warning, "FogVolume {} has an invalid AABB in view {}", volume->Id(), Id());
                continue;
            }

            if (desc.bounds.IsValid() && !desc.bounds.Overlaps(worldBounds))
            {
                continue;
            }

            if (!(flags & ViewFlags::NO_FRUSTUM_CULLING))
            {
                if (!cachedFrustum.ContainsAABB(worldBounds))
                {
                    continue;
                }
            }

            rpl.GetFogVolumes().Track(volume->Id(), volume, GET_RESOURCE_VERSION(volume));
        }
    }
}

void View::CollectProbeVolumes(RenderProxyList& rpl)
{
    HYP_SCOPE;

    if (flags & ViewFlags::SKIP_PROBE_VOLUMES)
    {
        return;
    }

    for (Scene* scene : m_scenes)
    {
        for (auto [entity, _] : scene->GetEntityManager()->GetEntitySet<EntityType<ProbeVolume>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            ProbeVolume* probeVolume = StaticCast<ProbeVolume>(entity);

            const BoundingBox worldBounds = probeVolume->GetWorldBounds();

            if (!worldBounds.IsValid() || !worldBounds.IsFinite())
            {
                HYP_LOG(Scene, Warning, "ProbeVolume {} has an invalid AABB in view {}", probeVolume->Id(), Id());

                continue;
            }

            if (desc.bounds.IsValid() && !desc.bounds.Overlaps(worldBounds))
            {
                continue;
            }

            if (!cachedFrustum.ContainsAABB(worldBounds))
            {
                HYP_LOG(Scene, Verbose, "ProbeVolume {} is not in frustum of View {}", probeVolume->Id(), Id());

                continue;
            }

            bool anyProbeOwnsView = false;

            for (IrradianceProbe* probe : probeVolume->GetProbes())
            {
                if (!probe)
                {
                    continue;
                }

                if (desc.flags & ViewFlags::ENV_PROBE_VIEW)
                {
                    bool skipProbe = false;

                    // Skip probes that are owners of this view.
                    for (uint8 envProbeViewIndex = 0; envProbeViewIndex < 6; envProbeViewIndex++)
                    {
                        View* envProbeView = probe->GetView(envProbeViewIndex);

                        if (envProbeView == this)
                        {
                            skipProbe = true;
                            break;
                        }
                    }

                    if (skipProbe)
                    {
                        anyProbeOwnsView = true;
                        break;
                    }
                }

                rpl.GetEnvProbes().Track(probe->Id(), probe, GET_RESOURCE_VERSION(probe));
            }

            if (anyProbeOwnsView)
            {
                // Skip, as to not create circular dependency
                continue;
            }

            rpl.GetProbeVolumes().Track(probeVolume->Id(), probeVolume, GET_RESOURCE_VERSION(probeVolume));
        }
    }
}

void View::CollectEnvProbes(RenderProxyList& rpl)
{
    HYP_SCOPE;

    if (flags & ViewFlags::SKIP_ENV_PROBES)
    {
        return;
    }

    for (Scene* scene : m_scenes)
    {
        for (auto [entity, _] : scene->GetEntityManager()->GetEntitySet<EntityType<EnvProbe>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            EnvProbe* probe = StaticCast<EnvProbe>(entity);

            if (desc.flags & ViewFlags::ENV_PROBE_VIEW)
            {
                bool skipProbe = false;

                // Skip env probes that own this view (don't want to create circular dependency)
                for (uint8 envProbeViewIndex = 0; envProbeViewIndex < 6; envProbeViewIndex++)
                {
                    View* envProbeView = probe->GetView(envProbeViewIndex);

                    if (envProbeView == this)
                    {
                        skipProbe = true;
                        break;
                    }
                }

                if (skipProbe)
                {
                    continue;
                }
            }

            if (!probe->IsSkyProbe())
            {
                const BoundingBox worldBounds = probe->GetWorldBounds();

                if (!worldBounds.IsValid() || !worldBounds.IsFinite())
                {
                    HYP_LOG(Scene, Warning, "EnvProbe {} has an invalid AABB in view {}", probe->Id(), Id());

                    continue;
                }

                if (desc.bounds.IsValid() && !desc.bounds.Overlaps(worldBounds))
                {
                    continue;
                }

                if (!(flags & ViewFlags::NO_FRUSTUM_CULLING) && !cachedFrustum.ContainsAABB(worldBounds))
                {
                    continue;
                }
            }

            rpl.GetEnvProbes().Track(probe->Id(), probe, GET_RESOURCE_VERSION(probe));
        }
    }
}

void View::CollectSprites(RenderProxyList& rpl)
{
    HYP_SCOPE;

    if (flags & ViewFlags::SKIP_SPRITES)
    {
        return;
    }

    for (Scene* scene : m_scenes)
    {
        for (auto [entity, _] : scene->GetEntityManager()->GetEntitySet<EntityType<Sprite>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            Sprite* sprite = StaticCast<Sprite>(entity);

            rpl.GetSprites().Track(sprite->Id(), sprite, GET_RESOURCE_VERSION(sprite));

            if (sprite->IsA<TextSprite>())
            {
                TextSprite* textSprite = StaticCast<TextSprite>(sprite);

                if (Texture* fontAtlasTexture = textSprite->GetFontAtlasTexture())
                {
                    rpl.GetTextures().Track(fontAtlasTexture->Id(), fontAtlasTexture);
                }
            }
        }
    }
}

#pragma endregion View

} // namespace Hyperion
