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
#include <Rendering/Glimmer/GlimmerHelpers.hpp>
#include <Rendering/Glimmer/SWRT/GlimmerSWRTProbeVolume.hpp>
#include <Rendering/Glimmer/SWRT/GlimmerSWRTCVars.hpp>

#include <Rendering/DebugDrawer.hpp>

#include <Scene/World.hpp>
#include <Scene/Camera/Camera.hpp>
#include <Scene/Util/SceneHelpers.hpp>

#include <Framework/EngineGlobals.hpp>
#include <Framework/View.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Threading/Threads.hpp>

#include <Core/Profiling/ProfileScope.hpp>

#include <GlimmerSystem.generated.inl>

namespace Hyperion {

// a quarter of a probe's rays on back faces puts it inside a solid
static constexpr float ProbeDebugInsideBackfaceFraction = 0.25f;

static Color GetGlimmerProbeDebugColor(GlimmerSWRTDebugProbes mode, const GlimmerProbeDebugRecord& record, const Vec3f& viewerPosition, float exposure)
{
    if (record.info.x == 0)
    {
        return Color(0.35f, 0.35f, 0.35f);
    }

    if (mode == GlimmerSWRTDebugProbes::Status)
    {
        if (record.info.z != 0)
        {
            return Color::Red();
        }

        const float backfaceFraction = float(record.info.y) / float(GlimmerProbeRays);

        if (backfaceFraction >= ProbeDebugInsideBackfaceFraction)
        {
            return Color::Magenta();
        }

        // green, through yellow as more of its rays hit back faces
        return Color(backfaceFraction / ProbeDebugInsideBackfaceFraction, 1.0f, 0.0f);
    }

    Vec3f irradiance = Vec3f(record.sh[0].x, record.sh[1].x, record.sh[2].x);

    if (mode == GlimmerSWRTDebugProbes::IrradianceTowardViewer)
    {
        const Vec3f toViewer = viewerPosition - record.position.GetXYZ();
        const float distance = toViewer.Length();

        if (distance > 1e-4f)
        {
            // L1 as the apply shaders evaluate it (GlimmerEvaluateL1)
            const Vec3f N = toViewer * Vec3f(1.0f / distance);

            const auto evaluateL1 = [&N](const Vec4f& sh)
            {
                return MathUtil::Max(sh.x + Vec3f(sh.y, sh.z, sh.w).Dot(N), 0.0f);
            };

            irradiance = Vec3f(evaluateL1(record.sh[0]), evaluateL1(record.sh[1]), evaluateL1(record.sh[2]));
        }
    }

    // debug draws aren't tonemapped with the scene, so a simple Reinhard keeps bright probes apart
    irradiance *= Vec3f(MathUtil::Max(exposure, 0.0f));

    return Color(
        irradiance.x / (1.0f + irradiance.x),
        irradiance.y / (1.0f + irradiance.y),
        irradiance.z / (1.0f + irradiance.z));
}

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

    if (!SceneHelpers::FindViewerPosition(*GetWorld(), viewerPosition) && m_hasRegion && !force)
    {
        return;
    }

    // the region moves in steps, so whatever the technique builds from the scene only rebuilds now and then
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
    state.hasViewer = SceneHelpers::FindViewerPosition(*GetWorld(), viewerPosition);

    if (state.hasViewer)
    {
        state.viewerPosition = viewerPosition;

        m_groundClipmap.Update(GetWorld(), viewerPosition, groundUploads);
    }

    m_groundClipmap.FillState(state);

    m_channel->Publish(state, std::move(groundUploads));

    if (state.hasViewer)
    {
        DebugDrawProbes(viewerPosition);
    }
}

void GlimmerSystem::DebugDrawProbes(const Vec3f& viewerPosition)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    const int mode = g_cvGlimmerSWRTDebugProbes.Get();

    if (mode <= int(GlimmerSWRTDebugProbes::None) || mode >= int(GlimmerSWRTDebugProbes::Max))
    {
        if (m_probeDebugRecords.Any())
        {
            m_probeDebugRecords = Array<GlimmerProbeDebugRecord>();
        }

        return;
    }

    // leaves the last readback in place when there's no newer one
    m_channel->ConsumeProbeDebug(m_probeDebugRecords);

    if (m_probeDebugRecords.Empty())
    {
        return;
    }

    const int cascadeFilter = g_cvGlimmerSWRTDebugProbesCascade.Get();
    const float radius = g_cvGlimmerSWRTDebugProbesRadius.Get();
    const float exposure = g_cvGlimmerSWRTDebugProbesExposure.Get();

    DebugDrawCommandList& dbg = DebugDrawer::GetInstance().CreateCommandList();

    for (uint32 recordIndex = 0; recordIndex < uint32(m_probeDebugRecords.Size()); recordIndex++)
    {
        const uint32 cascadeIndex = recordIndex / GlimmerProbesPerCascade;

        if (cascadeFilter >= 0 && int(cascadeIndex) != cascadeFilter)
        {
            continue;
        }

        const GlimmerProbeDebugRecord& record = m_probeDebugRecords[recordIndex];
        const Vec3f position = record.position.GetXYZ();

        if (position.Distance(viewerPosition) > radius)
        {
            continue;
        }

        const Color color = GetGlimmerProbeDebugColor(GlimmerSWRTDebugProbes(mode), record, viewerPosition, exposure);

        dbg.sphere(position, 0.1f * GetGlimmerProbeCascadeSpacing(cascadeIndex), color);
    }
}

} // namespace Hyperion
