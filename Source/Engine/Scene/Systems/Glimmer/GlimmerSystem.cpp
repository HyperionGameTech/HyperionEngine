/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/Systems/Glimmer/GlimmerSystem.hpp>

#include <Scene/World.hpp>
#include <Scene/Scene.hpp>
#include <Scene/EntityManager.hpp>
#include <Scene/Camera/Camera.hpp>
#include <Scene/Light/Light.hpp>
#include <Scene/Util/SceneHelpers.hpp>

#include <Rendering/Glimmer/GlimmerCVars.hpp>
#include <Rendering/Glimmer/GlimmerChannel.hpp>
#include <Rendering/Glimmer/GlimmerTechnique.hpp>
#include <Rendering/Glimmer/GlimmerHelpers.hpp>

#include <Framework/EngineGlobals.hpp>
#include <Framework/View.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Threading/Threads.hpp>
#include <Core/Threading/AtomicVar.hpp>

#include <Core/Profiling/ProfileScope.hpp>

#include <algorithm>

#include <GlimmerSystem.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Rendering);

extern AtomicVar<uint32> g_currentFrameRateLimit;

namespace /* Helpers */ {

static constexpr float LookAheadSeconds = 1.0f;
static constexpr float MaxLookAhead = 64.0f;
static constexpr float VelocityResponse = 3.0f;  // 1/s, smooths out frame to frame jitter in the camera's motion
static constexpr float TeleportDistance = 100.0f; // a move further than this in one frame is a jump, not motion
static constexpr float MaxBudgetScale = 4.0f;
static constexpr float StartupBudgetScale = 8.0f; // past what surfels and SH take (they stop at 4): the bricks build twice as fast

bool IsWorldGlimmerEnabled(const World* world)
{
    return world != nullptr && world->GetEnvironmentSettings().globalIllumination.glimmerEnabled;
}

} // namespace

#pragma region GlimmerSystem

GlimmerSystem::GlimmerSystem()
    : m_isSceneViewActive(false),
      m_regionCenter(Vec3f::Zero()),
      m_regionRadius(0.0f),
      m_hasRegion(false)
{
}

GlimmerSystem::~GlimmerSystem()
{
}

void GlimmerSystem::CreateSceneView()
{
    HYP_SCOPE;

    if (m_sceneView.IsValid() || EngineGlobals::IsHeadless())
    {
        return;
    }

    m_sceneCamera = MakeHandle<Camera>(1, 1);
    m_sceneCamera->SetName(NAME("GlimmerSceneCamera"));
    m_sceneCamera->SetNearClip(0.0f);
    m_sceneCamera->SetFarClip(SceneVerticalHalfExtent * 2.0f);
    InitObject(m_sceneCamera);

    ViewDesc viewDesc {};

    viewDesc.flags = ViewFlags::GLIMMER_SCENE_VIEW
        | ViewFlags::ALL_FOREGROUND_SCENES
        | ViewFlags::COLLECT_STATIC_ENTITIES
        | ViewFlags::SKIP_LIGHTS
        | ViewFlags::SKIP_CAMERAS
        | ViewFlags::SKIP_ENV_PROBES
        | ViewFlags::SKIP_PARTICLE_VOLUMES
        | ViewFlags::SKIP_FOG_VOLUMES
        | ViewFlags::SKIP_SPRITES
        | ViewFlags::SKIP_EFFECT_VOLUMES
        | ViewFlags::SKIP_DECALS
        | ViewFlags::NO_SHADOW_VIEWS
        | ViewFlags::NO_DRAW_CALLS
        | ViewFlags::NO_PARALLEL_DRAW_CALL_COLLECTION
        | ViewFlags::EXTERNAL_RENDERTARGET;

    viewDesc.camera = m_sceneCamera;

    m_sceneView = MakeHandle<View>(viewDesc);
    m_sceneView->name = NAME("GlimmerSceneView");

    m_hasRegion = false;
}

void GlimmerSystem::SetSceneViewActive(bool active)
{
    if (active == m_isSceneViewActive || !GetWorld())
    {
        return;
    }

    if (active)
    {
        CreateSceneView();

        if (!m_sceneView.IsValid())
        {
            return;
        }

        UpdateSceneRegion(true);

        GetWorld()->AddView(m_sceneView);
    }
    else if (m_sceneView.IsValid())
    {
        GetWorld()->RemoveView(m_sceneView);
    }

    m_isSceneViewActive = active;
}

void GlimmerSystem::UpdateSceneRegion(bool force)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    if (!m_sceneView.IsValid())
    {
        return;
    }

    Vec3f viewerPosition = m_regionCenter;

    if (!SceneHelpers::FindViewerPosition(*GetWorld(), viewerPosition) && m_hasRegion && !force)
    {
        return;
    }

    const GlimmerSceneRegionParams regionParams = GlimmerTechnique::GetSceneRegionParams();

    const float radius = regionParams.radius;
    const float snap = MathUtil::Max(regionParams.snap, 0.001f);

    const Vec2f offsetXZ = Vec2f(viewerPosition.x - m_regionCenter.x, viewerPosition.z - m_regionCenter.z);
    const float verticalOffset = MathUtil::Abs(viewerPosition.y - m_regionCenter.y);

    const bool needsRecenter = force
        || !m_hasRegion
        || radius != m_regionRadius
        || offsetXZ.Length() > regionParams.recenterDistance
        || verticalOffset > SceneVerticalHalfExtent * 0.25f;

    if (!needsRecenter)
    {
        return;
    }

    m_regionCenter = Vec3f(
        MathUtil::Floor(viewerPosition.x / snap) * snap,
        MathUtil::Floor(viewerPosition.y / snap) * snap,
        MathUtil::Floor(viewerPosition.z / snap) * snap);

    m_regionRadius = radius;
    m_hasRegion = true;

    const Vec3f eye = m_regionCenter + Vec3f(0.0f, SceneVerticalHalfExtent, 0.0f);

    const Mat4f viewMatrix = Mat4f::LookAt(eye, eye - Vec3f::UnitY(), Vec3f::UnitZ());
    const Mat4f projectionMatrix = Mat4f::Orthographic(-radius, radius, -radius, radius, 0.0f, SceneVerticalHalfExtent * 2.0f);

    m_sceneView->cachedMatrices.view = viewMatrix;
    m_sceneView->cachedMatrices.viewProj = projectionMatrix * viewMatrix;
    m_sceneView->cachedMatrices.invProj = projectionMatrix.Inverse();
    m_sceneView->cachedFrustum.SetFromViewProjectionMatrix(m_sceneView->cachedMatrices.viewProj);

    m_sceneCamera->SetWorldTranslation(eye);
}

void GlimmerSystem::OnAddedToWorld(World* world)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    SystemBase::OnAddedToWorld(world);

    if (!EngineGlobals::IsHeadless())
    {
        m_channel = MakeShared<GlimmerChannel>();
        GlimmerChannel::Register(world, m_channel);
    }

    SetSceneViewActive(GlimmerHelpers::IsGlimmerSceneRequired() && IsWorldGlimmerEnabled(world));
}

void GlimmerSystem::OnRemovedFromWorld(World* world)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    SetSceneViewActive(false);

    if (m_channel)
    {
        GlimmerChannel::Unregister(world);
        m_channel.Reset();
    }

    SystemBase::OnRemovedFromWorld(world);
}

void GlimmerSystem::Process(float delta, Span<Handle<Scene>> scenes)
{
    HYP_SCOPE;

    SetSceneViewActive(GlimmerHelpers::IsGlimmerSceneRequired() && IsWorldGlimmerEnabled(GetWorld()));

    if (!m_isSceneViewActive || !m_channel)
    {
        return;
    }

    UpdateSceneRegion(false);

    GlimmerChannelState state;
    Array<GlimmerGroundUpload, GlimmerAllocator> groundUploads;

    Vec3f viewerPosition;
    state.hasViewer = SceneHelpers::FindViewerPosition(*GetWorld(), viewerPosition);

    if (state.hasViewer)
    {
        state.viewerPosition = viewerPosition;

        UpdateViewerMotion(viewerPosition, delta, state);

        m_groundClipmap.Update(GetWorld(), viewerPosition, groundUploads);
    }
    else
    {
        m_hasLastViewer = false;
    }

    m_groundClipmap.FillState(state);

    CollectLocalLights(scenes, state.hasViewer ? viewerPosition : m_regionCenter, state);

    m_channel->Publish(state, std::move(groundUploads));
}

void GlimmerSystem::CollectLocalLights(
    Span<Handle<Scene>> scenes,
    const Vec3f& nearPosition,
    GlimmerChannelState& outState)
{
    HYP_SCOPE;

    outState.numLocalLights = 0;

    if (!m_hasRegion)
    {
        return;
    }

    using Candidate = Pair<
        float, /* distance*/
        GlimmerLocalLightState /* state */ >;

    Array<Candidate, GlimmerTempAllocator> candidates;

    const LayersMask& activeLayers = GetWorld()->GetActiveLayers();

    for (Scene* scene : scenes)
    {
        if (!ShouldProcessScene(scene))
        {
            continue;
        }

        for (auto [light] : scene->GetEntityManager()->GetEntitySet<EntityType<Light>>().GetScopedView(GetComponentInfos()))
        {
            const LightType lightType = light->GetLightType();

            if ((lightType != LightType::Point && lightType != LightType::Spot) || !light->IsStatic())
            {
                continue;
            }

            if (!light->HasNoLayers() && !light->IsInAnyLayers(activeLayers))
            {
                continue;
            }

            const BoundingSphere sphere = light->GetBoundingSphere(true);
            const Vec3f color = Vec4f(light->GetColor()).GetXYZ() * light->GetIntensity();

            if (sphere.radius <= 0.0f || MathUtil::Max(color.x, MathUtil::Max(color.y, color.z)) <= 0.0f)
            {
                continue;
            }

            const Vec3f fromRegion = sphere.center - m_regionCenter;

            if (MathUtil::Abs(fromRegion.x) > m_regionRadius + sphere.radius
                || MathUtil::Abs(fromRegion.z) > m_regionRadius + sphere.radius
                || MathUtil::Abs(fromRegion.y) > SceneVerticalHalfExtent + sphere.radius)
            {
                continue;
            }

            GlimmerLocalLightState state;
            state.key = light->Id().GetHashCode().Value();
            state.positionRadius = Vec4f(sphere.center, sphere.radius);
            state.color = Vec4f(color, 0.0f);

            if (lightType == LightType::Spot)
            {
                const Vec2f& spotAngles = light->GetSpotAngles();

                const float cosOuter = MathUtil::Cos(MathUtil::DegToRad(spotAngles.x));
                const float cosInner = MathUtil::Max(MathUtil::Cos(MathUtil::DegToRad(spotAngles.y)), cosOuter + 0.0001f);

                state.spotDirection = Vec4f(light->GetSpotDirection(), 1.0f);
                state.spotAngles = Vec4f(cosOuter, cosInner, 0.0f, 0.0f);
            }

            candidates.EmplaceBack(
                MathUtil::Max(sphere.center.Distance(nearPosition) - sphere.radius, 0.0f),
                state);
        }
    }

    std::sort(candidates.Begin(), candidates.End(), [](const Candidate& a, const Candidate& b)
        {
            return a.first != b.first
                ? a.first < b.first
                : a.second.key < b.second.key;
        });

    for (const Candidate& candidate : candidates)
    {
        if (outState.numLocalLights >= GlimmerMaxLocalLights)
        {
            break;
        }

        outState.localLights[outState.numLocalLights++] = candidate.second;
    }
}

void GlimmerSystem::UpdateViewerMotion(const Vec3f& viewerPosition, float delta, GlimmerChannelState& outState)
{
    const Vec3f moved = viewerPosition - m_lastViewerPosition;

    if (!m_hasLastViewer || delta <= 0.0f || moved.Length() > TeleportDistance)
    {
        m_viewerVelocity = Vec3f::Zero();
        m_timeSinceStart = 0.0f;
    }
    else
    {
        const float blend = 1.0f - MathUtil::Exp(-VelocityResponse * delta);

        m_viewerVelocity += (moved / delta - m_viewerVelocity) * blend;
    }

    m_lastViewerPosition = viewerPosition;
    m_hasLastViewer = true;

    Vec3f lookAhead = m_viewerVelocity * LookAheadSeconds;

    if (lookAhead.Length() > MaxLookAhead)
    {
        lookAhead = lookAhead.Normalized() * MaxLookAhead;
    }

    if (!g_cvGlimmerLookAhead.Get())
    {
        return;
    }

    m_timeSinceStart += MathUtil::Max(delta, 0.0f);

    const bool isBursting = m_timeSinceStart < g_cvGlimmerStartupBurstSeconds.Get();

    const uint32 frameRateLimit = g_currentFrameRateLimit.Get(MemoryOrder::RELAXED);
    const float scaleCapped = frameRateLimit > 0 ? 60.0f / float(frameRateLimit) : 1.0f;

    outState.lookAhead = lookAhead;
    outState.budgetScale = isBursting ? StartupBudgetScale : MathUtil::Clamp(MathUtil::Min(delta * 60.0f, scaleCapped), 1.0f, MaxBudgetScale);
}

#pragma endregion GlimmerSystem

} // namespace Hyperion
