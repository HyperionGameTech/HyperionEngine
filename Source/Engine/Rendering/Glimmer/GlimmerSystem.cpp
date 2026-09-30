/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Glimmer/GlimmerSystem.hpp>
#include <Rendering/Glimmer/GlimmerCVars.hpp>
#include <Rendering/Glimmer/GlimmerChannel.hpp>
#include <Rendering/Glimmer/GlimmerTechnique.hpp>

#include <Scene/World.hpp>
#include <Scene/Camera/Camera.hpp>
#include <Scene/Sky/DynamicSkySystem.hpp>

#include <Framework/EngineGlobals.hpp>
#include <Framework/View.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Threading/Threads.hpp>

#include <Core/Profiling/ProfileScope.hpp>

#include <GlimmerSystem.generated.inl>

namespace Hyperion {

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
        | ViewFlags::SKIP_LIGHTMAP_VOLUMES
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

    if (!DynamicSkySystem::FindViewerPosition(GetWorld(), viewerPosition) && m_hasRegion && !force)
    {
        return;
    }

    // the region moves in steps, so whatever the technique builds from the scene only rebuilds now and then
    const GlimmerSceneRegionParams regionParams = GetGlimmerSceneRegionParams(GetActiveGlimmerTechniqueType());

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

    // looking straight down, so the up vector has to be along Z
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

    SetSceneViewActive(IsGlimmerSceneRequired());
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

    SetSceneViewActive(IsGlimmerSceneRequired());

    if (!m_isSceneViewActive || !m_channel)
    {
        return;
    }

    UpdateSceneRegion(false);

    GlimmerChannelState state;
    Array<GlimmerGroundUpload> groundUploads;

    Vec3f viewerPosition;
    state.hasViewer = DynamicSkySystem::FindViewerPosition(GetWorld(), viewerPosition);

    if (state.hasViewer)
    {
        state.viewerPosition = viewerPosition;

        m_groundClipmap.Update(GetWorld(), viewerPosition, groundUploads);
    }

    m_groundClipmap.FillState(state);

    m_channel->Publish(state, std::move(groundUploads));
}

} // namespace Hyperion
