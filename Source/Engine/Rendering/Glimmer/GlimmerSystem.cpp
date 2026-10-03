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
#include <Rendering/Vertex.hpp>

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

HYP_DECLARE_LOG_CHANNEL(Rendering);

//// @TODO: Refactor debug drawing stuff into a new GlimmerDebugDraw

static Color GetGlimmerProbeDebugColor(GlimmerSWRTDebugProbes mode, const GlimmerProbeDebugRecord& record, const Vec3f& viewerPosition, float exposure)
{
    const uint32 probeState = record.info.x & 0xFFu;

    if (probeState == GPS_BURIED)
    {
        return Color::Red();
    }

    if (probeState == GPS_INSIDE)
    {
        return Color::Magenta();
    }

    // open air: not traced
    if (probeState == GPS_IDLE)
    {
        return Color(0.15f, 0.2f, 0.45f);
    }

    if (record.info.w == 0)
    {
        return Color(0.35f, 0.35f, 0.35f);
    }

    if (mode == GlimmerSWRTDebugProbes::Status)
    {
        const float spacing = GetGlimmerProbeLevelSpacing(uint32(record.position.w));
        const Vec3f position = record.position.GetXYZ();

        // off its grid point: moved out of a solid, or off a surface it sat against
        const Vec3f gridPoint = Vec3f(
            (MathUtil::Floor(position.x / spacing) + 0.5f) * spacing,
            (MathUtil::Floor(position.y / spacing) + 0.5f) * spacing,
            (MathUtil::Floor(position.z / spacing) + 0.5f) * spacing);

        if (position.Distance(gridPoint) > 0.05f * spacing)
        {
            return Color::Cyan();
        }

        // green, through yellow as more of its rays hit back faces (a quarter of them moves it)
        const float backfaceFraction = float(record.info.y) / float(MathUtil::Max(record.info.z, 1u));

        return Color(MathUtil::Min(backfaceFraction / 0.25f, 1.0f), 1.0f, 0.0f);
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

    irradiance *= Vec3f(MathUtil::Max(exposure, 0.0f));

    return Color(
        irradiance.x / (1.0f + irradiance.x),
        irradiance.y / (1.0f + irradiance.y),
        irradiance.z / (1.0f + irradiance.z));
}

static RenderableAttributeSet MakeProbeBlockDebugAttributes()
{
    RenderableAttributeSet attributes;

    MeshAttributes& meshAttributes = attributes.GetMeshAttributes();
    meshAttributes.inputLayout = StaticVertexInputLayout<VT_Simple>;
    meshAttributes.topology = Topology::Triangles;

    MaterialAttributes& materialAttributes = attributes.GetMaterialAttributes();
    materialAttributes.bucket = RenderBucket::Debug;
    materialAttributes.fillMode = FillMode::Line;
    materialAttributes.cullFaces = FaceCullMode::None;
    materialAttributes.blendFunction = BlendFunction::None();
    materialAttributes.flags = MAF_DEPTH_TEST | MAF_DEPTH_WRITE;

    return attributes;
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
        UpdateProbeDebugRecords(delta);
        DebugDrawProbes(viewerPosition);
    }
}

void GlimmerSystem::UpdateProbeDebugRecords(float delta)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    static constexpr float ProbeStatsLogInterval = 1.0f;

    const int mode = g_cvGlimmerSWRTDebugProbes.Get();
    const bool isDrawingProbes = mode > int(GlimmerSWRTDebugProbes::None) && mode < int(GlimmerSWRTDebugProbes::Max);
    const bool isLoggingStats = g_cvGlimmerSWRTProbesLogStats.Get();

    if (!isDrawingProbes && !isLoggingStats)
    {
        if (m_probeDebugRecords.Any())
        {
            m_probeDebugRecords.Resize(0);
        }

        m_probeStatsLogTimer = 0.0f;

        return;
    }

    const bool hasNewRecords = m_channel->ConsumeProbeDebug(m_probeDebugRecords);

    if (!isLoggingStats)
    {
        return;
    }

    m_probeStatsLogTimer += delta;

    if (hasNewRecords && m_probeStatsLogTimer >= ProbeStatsLogInterval)
    {
        m_probeStatsLogTimer = 0.0f;

        LogProbeStats();
    }
}

void GlimmerSystem::LogProbeStats()
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    Camera* viewerCamera = SceneHelpers::FindViewerCamera(*GetWorld());

    struct LevelCounts
    {
        uint32 resident = 0;
        uint32 states[GPS_IDLE + 1] = {};
        uint32 activeOnScreen = 0;
    };

    LevelCounts levels[GlimmerProbeLevels];
    LevelCounts total;

    for (const GlimmerProbeDebugRecord& record : m_probeDebugRecords)
    {
        if (record.position.w < 0.0f)
        {
            continue;
        }

        const uint32 levelIndex = MathUtil::Min(uint32(record.position.w), GlimmerProbeLevels - 1);
        const uint32 probeState = MathUtil::Min(record.info.x & 0xFFu, uint32(GPS_IDLE));

        // a probe that just reset holds nothing the lighting uses yet
        const bool isOnScreen = probeState == GPS_ACTIVE
            && record.info.w != 0
            && viewerCamera != nullptr
            && viewerCamera->GetFrustum().ContainsPoint(record.position.GetXYZ());

        for (LevelCounts* counts : { &levels[levelIndex], &total })
        {
            counts->resident++;
            counts->states[probeState]++;
            counts->activeOnScreen += isOnScreen ? 1u : 0u;
        }
    }

    HYP_LOG(Rendering, Info, "Glimmer probes: {} resident, {} active ({} on screen), {} idle, {} inside, {} buried",
        total.resident, total.states[GPS_ACTIVE], total.activeOnScreen, total.states[GPS_IDLE], total.states[GPS_INSIDE], total.states[GPS_BURIED]);

    for (uint32 levelIndex = 0; levelIndex < GlimmerProbeLevels; levelIndex++)
    {
        const LevelCounts& counts = levels[levelIndex];

        HYP_LOG(Rendering, Info, "  level {} ({} m): {} resident, {} active ({} on screen), {} idle, {} inside, {} buried",
            levelIndex, GetGlimmerProbeLevelSpacing(levelIndex), counts.resident, counts.states[GPS_ACTIVE], counts.activeOnScreen,
            counts.states[GPS_IDLE], counts.states[GPS_INSIDE], counts.states[GPS_BURIED]);
    }
}

void GlimmerSystem::DebugDrawProbes(const Vec3f& viewerPosition)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    const int mode = g_cvGlimmerSWRTDebugProbes.Get();

    if (mode <= int(GlimmerSWRTDebugProbes::None) || mode >= int(GlimmerSWRTDebugProbes::Max))
    {
        return;
    }

    if (m_probeDebugRecords.Empty())
    {
        return;
    }

    const int levelFilter = g_cvGlimmerSWRTDebugProbesLevel.Get();
    const float radius = g_cvGlimmerSWRTDebugProbesRadius.Get();
    const float exposure = g_cvGlimmerSWRTDebugProbesExposure.Get();

    static const RenderableAttributeSet s_blockAttributes = MakeProbeBlockDebugAttributes();

    DebugDrawCommandList& dbg = DebugDrawer::GetInstance().CreateCommandList();

    for (uint32 slot = 0; slot < uint32(m_probeDebugRecords.Size()) / GlimmerProbesPerBlock; slot++)
    {
        const GlimmerProbeDebugRecord& firstRecord = m_probeDebugRecords[slot * GlimmerProbesPerBlock];

        if (firstRecord.position.w < 0.0f)
        {
            continue;
        }

        const uint32 levelIndex = uint32(firstRecord.position.w);

        if (levelFilter >= 0 && int(levelIndex) != levelFilter)
        {
            continue;
        }

        const float spacing = GetGlimmerProbeLevelSpacing(levelIndex);
        const float blockSize = spacing * float(GlimmerProbeBlock);

        // the first probe of a block is its corner one, so its grid point gives the block
        const Vec3f firstPosition = firstRecord.position.GetXYZ();
        const Vec3f blockCenter = Vec3f(
            (MathUtil::Floor(firstPosition.x / blockSize) + 0.5f) * blockSize,
            (MathUtil::Floor(firstPosition.y / blockSize) + 0.5f) * blockSize,
            (MathUtil::Floor(firstPosition.z / blockSize) + 0.5f) * blockSize);

        if (blockCenter.Distance(viewerPosition) > radius + blockSize)
        {
            continue;
        }

        dbg.box(Transform(blockCenter, Vec3f(blockSize * 0.5f), Quat4f::Identity()), levelIndex == 0 ? Color::White() : Color(1.0f, 0.6f, 0.1f), s_blockAttributes);

        for (uint32 probe = 0; probe < GlimmerProbesPerBlock; probe++)
        {
            const GlimmerProbeDebugRecord& record = m_probeDebugRecords[slot * GlimmerProbesPerBlock + probe];
            const Vec3f position = record.position.GetXYZ();

            if (position.Distance(viewerPosition) > radius)
            {
                continue;
            }

            // idle probes hold nothing to show but where they are
            if ((record.info.x & 0xFFu) == GPS_IDLE && GlimmerSWRTDebugProbes(mode) != GlimmerSWRTDebugProbes::Status)
            {
                continue;
            }

            dbg.sphere(position, 0.1f * spacing, GetGlimmerProbeDebugColor(GlimmerSWRTDebugProbes(mode), record, viewerPosition, exposure));
        }
    }
}

} // namespace Hyperion
