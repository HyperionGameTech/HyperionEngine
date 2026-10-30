/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Glimmer/GlimmerPass.hpp>
#include <Rendering/Glimmer/GlimmerTechnique.hpp>
#include <Rendering/Glimmer/GlimmerSurfaceCache.hpp>
#include <Rendering/Glimmer/GlimmerTriangleCache.hpp>
#include <Rendering/Glimmer/GlimmerSceneInstances.hpp>
#include <Rendering/Glimmer/GlimmerSpanCache.hpp>
#include <Rendering/Glimmer/GlimmerChannel.hpp>
#include <Rendering/Glimmer/GlimmerCVars.hpp>
#include <Rendering/Glimmer/GlimmerHelpers.hpp>

#include <Rendering/Glimmer/Bricks/GlimmerBrickWorld.hpp>

#include <Rendering/Passes/DeferredPass.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/RenderSetup.hpp>
#include <Rendering/RenderProxy.hpp>
#include <Rendering/RenderProxyList.hpp>
#include <Rendering/CommandRecorder.hpp>
#include <Rendering/CBufferAllocator.hpp>
#include <Rendering/Framebuffer.hpp>
#include <Rendering/GpuImage.hpp>
#include <Rendering/GpuImageView.hpp>
#include <Rendering/Texture.hpp>
#include <Rendering/TextureViewCache.hpp>
#include <Rendering/Frame.hpp>
#include <Rendering/Material.hpp>

#include <Scene/World.hpp>
#include <Scene/Entity.hpp>
#include <Scene/Camera/Camera.hpp>

#include <Framework/EngineStats.hpp>
#include <Framework/View.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Profiling/PerformanceClock.hpp>
#include <Core/Profiling/ProfileScope.hpp>

#include <Core/Utilities/DeferredScope.hpp>

#include <Core/Threading/Threads.hpp>


#include <Core/Logging/Logger.hpp>

#include <GlimmerPass.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Rendering);

static EngineStatGpuTimer s_statGlimmerScene("Rendering/GPU/Glimmer/Scene");

static EngineStatCounter<uint32> s_statGlimmerTerrainPatches("Rendering/Glimmer/TerrainPatches", false);
static EngineStatCounter<uint32> s_statGlimmerSpanInstances("Rendering/Glimmer/SpanInstances", false);
static EngineStatCounter<uint32> s_statGlimmerResidentMeshes("Rendering/Glimmer/ResidentMeshes", false);
static EngineStatCounter<uint32> s_statGlimmerBuildingMeshes("Rendering/Glimmer/BuildingMeshes", false);
static EngineStatCounter<uint32> s_statGlimmerMeshesWaitingForRoom("Rendering/Glimmer/MeshesWaitingForRoom", false);
static EngineStatCounter<uint32> s_statGlimmerPoolTriangles("Rendering/Glimmer/PoolTriangles", false);
static EngineStatCounter<uint32> s_statGlimmerBLASNodes("Rendering/Glimmer/BLASNodes", false);
static EngineStatCounter<uint32> s_statGlimmerTracedInstances("Rendering/Glimmer/TracedInstances", false);
static EngineStatCounter<uint32> s_statGlimmerTLASBuilds("Rendering/Glimmer/TLASBuilds", false);
static EngineStatCounter<uint32> s_statGlimmerLodErrorScalePercent("Rendering/Glimmer/LodErrorScalePercent", false);
static EngineStatCounter<uint32> s_statGlimmerSceneInstancesBuilds("Rendering/Glimmer/InstanceBuilds", false);
static EngineStatCounter<uint32> s_statGlimmerSceneInstancesEverywhereBuilds("Rendering/Glimmer/InstanceEverywhereBuilds", false);
static EngineStatCounter<uint32> s_statGlimmerSceneInstancesChanges("Rendering/Glimmer/InstanceChanges", false);
static EngineStatCounter<uint32> s_statGlimmerSceneInstancesLodOnlyChanges("Rendering/Glimmer/InstanceLodOnlyChanges", false);

static constexpr Pair<StringHash, GlimmerRestartParts> StatefulShaders[] = {
    { "GlimmerGroundCover"_sh, GlimmerRestartParts::GroundAlbedo },
    { "GlimmerGroundAlbedo"_sh, GlimmerRestartParts::GroundAlbedo },
    { "GlimmerGroundUpload"_sh, GlimmerRestartParts::GroundUploads },
    { "GlimmerSpanSplat"_sh, GlimmerRestartParts::SpanCache },
    { "GlimmerHeightBounds"_sh, GlimmerRestartParts::SpanCache },
    { "GlimmerBrickBuild"_sh, GlimmerRestartParts::Bricks },
    { "GlimmerBrickLight"_sh, GlimmerRestartParts::Bricks },
    { "GlimmerProbeAlloc"_sh, GlimmerRestartParts::Lighting },
    { "GlimmerProbeTrace"_sh, GlimmerRestartParts::Lighting },
    { "GlimmerProbeBlend"_sh, GlimmerRestartParts::Lighting },
    { "GlimmerSHOccupancySplat"_sh, GlimmerRestartParts::Lighting },
    { "GlimmerSHUpdate"_sh, GlimmerRestartParts::Lighting }
};

static constexpr uint32 NumStatefulShaders = uint32(GetArrayCount(StatefulShaders));


namespace  /* Helpers */ {

EnumFlags<GlimmerRestartParts> GetPartsToRestart(GlimmerRestartParts part)
{
    switch (part)
    {
    case GlimmerRestartParts::GroundAlbedo:
    case GlimmerRestartParts::SpanCache:
    case GlimmerRestartParts::Bricks:
        return part | GlimmerRestartParts::Lighting;
    default:
        return part;
    }
}

void LogStartupStages(GlimmerScenePassData& scene, uint32 numCompiling)
{
    const auto checkReached = [&scene](GlimmerStartupStage stage, bool isReached) -> bool
    {
        if (!isReached || (scene.loggedStages & stage))
        {
            return false;
        }

        scene.loggedStages |= stage;

        return true;
    };

    const double ms = PerformanceClock::TimeSince(scene.startTime);
    const uint32 frames = GetFrameCounter() - scene.startFrame;

    if (checkReached(GlimmerStartupStage::FirstUpdate, frames == 0))
    {
        HYP_LOG(Rendering, Info, "Glimmer start: first update took {} ms", uint32(ms));
    }

    if (checkReached(GlimmerStartupStage::WarmingUp, bool(scene.pendingRestart)))
    {
        HYP_LOG(Rendering, Info, "Glimmer start: held at {} ms, frame {} while {} pipelines compile", uint32(ms), frames, numCompiling);
    }

    const GlimmerSceneInstances& sceneInstances = *scene.sceneInstances;
    const GlimmerSceneInstancesStats& instancesStats = sceneInstances.GetStats();

    if (checkReached(GlimmerStartupStage::SceneInstances, sceneInstances.IsReady()))
    {
        const GlimmerTriangleCacheStats triangleStats = scene.triangleCache->GetStats();

        HYP_LOG(Rendering, Info, "Glimmer start: scene instances at {} ms, frame {} ({} span instances, build {} ms, {} builds, {} waiting for triangles; meshes {} resident, {} building, {} to upload)",
            uint32(ms), frames, instancesStats.numSpanInstances, uint32(instancesStats.lastBuildMs), instancesStats.numBuilds, instancesStats.numWaitingForTriangles,
            triangleStats.numResident, triangleStats.numBuilding, triangleStats.numPendingUpload);
    }

    if (checkReached(GlimmerStartupStage::Triangles, sceneInstances.IsReady() && instancesStats.numWaitingForTriangles == 0))
    {
        HYP_LOG(Rendering, Info, "Glimmer start: every mesh's triangles at {} ms, frame {} ({} builds)", uint32(ms), frames, instancesStats.numBuilds);
    }

    if (checkReached(GlimmerStartupStage::Spans, scene.spanCache && scene.spanCache->GetSpansBuffer().IsInitialized()))
    {
        HYP_LOG(Rendering, Info, "Glimmer start: spans at {} ms, frame {}", uint32(ms), frames);
    }

    if (!scene.technique)
    {
        return;
    }

    const GlimmerBrickWorld* brickWorld = scene.technique->GetBrickWorld();

    const bool hasTriangles = bool(scene.loggedStages & GlimmerStartupStage::Triangles);

    if (checkReached(GlimmerStartupStage::Bricks, hasTriangles && brickWorld && brickWorld->IsReady()))
    {
        HYP_LOG(Rendering, Info, "Glimmer start: bricks at {} ms, frame {}", uint32(ms), frames);
    }

    if (checkReached(GlimmerStartupStage::BricksSettled, hasTriangles && brickWorld && brickWorld->IsSettled()))
    {
        HYP_LOG(Rendering, Info, "Glimmer start: bricks settled at {} ms, frame {}", uint32(ms), frames);
    }

    if (checkReached(GlimmerStartupStage::SH, scene.technique->IsSHReady()))
    {
        HYP_LOG(Rendering, Info, "Glimmer start: SH at {} ms, frame {}", uint32(ms), frames);
    }

    if (checkReached(GlimmerStartupStage::Probes, scene.technique->IsProbesReady()))
    {
        HYP_LOG(Rendering, Info, "Glimmer start: probes at {} ms, frame {}", uint32(ms), frames);
    }
}

} // namespace

#pragma region GlimmerScenePassData

GlimmerScenePassData::GlimmerScenePassData()
{
}

GlimmerScenePassData::~GlimmerScenePassData()
{
    technique.Reset();

    if (sceneInstances && triangleCache)
    {
        sceneInstances->Release(*triangleCache);
    }
}

#pragma endregion GlimmerScenePassData

#pragma region GlimmerViewPassData

GlimmerViewPassData::GlimmerViewPassData() = default;
GlimmerViewPassData::~GlimmerViewPassData() = default;

#pragma endregion GlimmerViewPassData

#pragma region GlimmerPass

GlimmerPass::GlimmerPass()
{
}

GlimmerPass::~GlimmerPass()
{
}

void GlimmerPass::Initialize()
{
}

void GlimmerPass::Shutdown()
{
    m_scenes.Clear();
}

PassData* GlimmerPass::CreateViewPassData(View* view, PassDataExt&)
{
    if (view->GetFlags() & ViewFlags::GLIMMER_SCENE_VIEW)
    {
        GlimmerScenePassData* passData = new GlimmerScenePassData();
        passData->view = MakeWeakRef(view);
        passData->sceneInstances = MakeUniqueWithAllocator<GlimmerSceneInstances, GlimmerAllocator>();
        passData->surfaceCache = MakeUniqueWithAllocator<GlimmerSurfaceCache, GlimmerAllocator>();
        passData->spanCache = MakeUniqueWithAllocator<GlimmerSpanCache, GlimmerAllocator>();
        passData->technique = MakeUniqueWithAllocator<GlimmerTechnique, GlimmerAllocator>();

        return passData;
    }

    GlimmerViewPassData* passData = new GlimmerViewPassData();
    passData->view = MakeWeakRef(view);

    return passData;
}

GlimmerScenePassData* GlimmerPass::GetSceneForWorld(World* world) const
{
    if (const KeyValuePair<World*, GlimmerScenePassData*>* it = m_scenes.TryGet(world))
    {
        return it->second;
    }

    return nullptr;
}

const GlimmerTechnique* GlimmerPass::GetApplyTechnique(World* world) const
{
    const bool isEnabled = g_cvGlimmerEnabled.Get()
        && (GetWorldBufferData()->environmentFlags & uint32(WorldEnvironmentFlags::Glimmer));

    GlimmerScenePassData* scene = isEnabled ? GetSceneForWorld(world) : nullptr;

    if (!scene || scene->pendingRestart)
    {
        return nullptr;
    }

    return scene->technique.Get();
}

void GlimmerPass::RenderFrame(Frame* frame, const RenderSetup& renderSetup)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    View* view = renderSetup.view;
    AssertDebug(view != nullptr && (view->GetFlags() & ViewFlags::GLIMMER_SCENE_VIEW));

    if (!GlimmerHelpers::IsGlimmerSceneRequired() || !(GetWorldBufferData()->environmentFlags & uint32(WorldEnvironmentFlags::Glimmer)))
    {
        return;
    }

    GlimmerScenePassData* scene = TryCast<GlimmerScenePassData>(FetchViewPassData(view));
    AssertDebug(scene != nullptr);

    const uint32 frameCounter = GetFrameCounter();

    if (scene->lastUpdatedFrame == frameCounter)
    {
        return;
    }

    const bool isFirstUpdate = scene->startTime == 0;

    if (isFirstUpdate)
    {
        scene->startTime = PerformanceClock::Now();
        scene->startFrame = frameCounter;
        scene->seenSkippedDispatches.Resize(NumStatefulShaders);
    }

    BackgroundCompileStatus compileStatus[NumStatefulShaders];

    for (uint32 shaderIndex = 0; shaderIndex < NumStatefulShaders; shaderIndex++)
    {
        compileStatus[shaderIndex] = RI.GetBackgroundCompileStatus(StatefulShaders[shaderIndex].first);

        uint32& seenSkippedDispatches = scene->seenSkippedDispatches[shaderIndex];

        if (compileStatus[shaderIndex].numSkippedDispatches != seenSkippedDispatches)
        {
            if (!isFirstUpdate)
            {
                scene->pendingRestart |= GetPartsToRestart(StatefulShaders[shaderIndex].second);
            }

            seenSkippedDispatches = compileStatus[shaderIndex].numSkippedDispatches;
        }
    }

    uint32 numCompiling = 0;
    bool isGroundUploadCompiling = false;

    for (uint32 shaderIndex = 0; shaderIndex < NumStatefulShaders; shaderIndex++)
    {
        const GlimmerRestartParts part = StatefulShaders[shaderIndex].second;

        if (scene->pendingRestart & part)
        {
            numCompiling += compileStatus[shaderIndex].numCompiling;
        }

        if (part == GlimmerRestartParts::GroundUploads && compileStatus[shaderIndex].numCompiling != 0)
        {
            isGroundUploadCompiling = true;
        }
    }

    const EnumFlags<GlimmerRestartParts> restartedParts = numCompiling == 0
        ? scene->pendingRestart
        : EnumFlags<GlimmerRestartParts>(GlimmerRestartParts::None);

    if (restartedParts)
    {
        if (restartedParts & GlimmerRestartParts::Bricks)
        {
            scene->technique.Reset();
            scene->technique = MakeUniqueWithAllocator<GlimmerTechnique, GlimmerAllocator>();
        }
        else if (restartedParts & GlimmerRestartParts::Lighting)
        {
            scene->technique->ResetLighting();
        }

        if (restartedParts & GlimmerRestartParts::SpanCache)
        {
            scene->spanCache = MakeUniqueWithAllocator<GlimmerSpanCache, GlimmerAllocator>();
        }

        if (restartedParts & GlimmerRestartParts::GroundAlbedo)
        {
            scene->surfaceCache->RefillAlbedo();
        }

        // writing the heights again marks them changed, which the parts reading them follow
        if (restartedParts & GlimmerRestartParts::GroundUploads)
        {
            scene->isReplayingGroundUploads = true;
        }

        scene->pendingRestart = GlimmerRestartParts::None;

        HYP_LOG(Rendering, Info, "Glimmer start: restarted at {} ms, frame {}, after pipelines compiled:{}{}{}{}{}",
            uint32(PerformanceClock::TimeSince(scene->startTime)), frameCounter - scene->startFrame,
            (restartedParts & GlimmerRestartParts::GroundUploads) ? " ground uploads" : "",
            (restartedParts & GlimmerRestartParts::GroundAlbedo) ? " ground albedo" : "",
            (restartedParts & GlimmerRestartParts::SpanCache) ? " spans" : "",
            (restartedParts & GlimmerRestartParts::Bricks) ? " bricks" : "",
            (restartedParts & GlimmerRestartParts::Lighting) ? " lighting" : "");
    }

    // the uploads it was given reached the GPU unless an upload dispatch could still be left out
    if (!scene->isReplayingGroundUploads && !(scene->pendingRestart & GlimmerRestartParts::GroundUploads) && !isGroundUploadCompiling)
    {
        scene->groundUploads.Clear();
    }

    scene->lastUpdatedFrame = frameCounter;
    scene->world = renderSetup.world;

    m_scenes.Set(renderSetup.world, scene);

    RenderProxyList& rpl = GetConsumerProxyList(view);
    rpl.BeginRead();

    HYP_DEFER({ rpl.EndRead(); });

    ENGINE_STAT_GPU_SCOPE(&s_statGlimmerScene);

    BoundingBox regionNDC;
    regionNDC.min = Vec3f(-1.0f);
    regionNDC.max = Vec3f(1.0f);

    const BoundingBox region = rpl.cachedMatrices.viewProj.Inverse() * regionNDC;

    if (!region.IsValid() || !region.IsFinite())
    {
        return;
    }

    scene->region = region;

    if (!scene->triangleCache)
    {
        scene->triangleCache = GlimmerTriangleCache::AcquireShared();
    }

    scene->triangleCache->UpdateOncePerFrame(frame);

    GlimmerChannelState channelState;
    Array<GlimmerGroundUpload, GlimmerAllocator> groundUploads;

    const SharedPtr<GlimmerChannel> channel = GlimmerChannel::Get(renderSetup.world);

    if (channel)
    {
        channel->Consume(channelState, groundUploads);
    }

    if (channelState.groundGeneration != scene->groundGeneration)
    {
        scene->groundGeneration = channelState.groundGeneration;
        scene->groundUploads.Clear();
    }

    const size_t numGivenGroundUploads = scene->isReplayingGroundUploads ? 0 : scene->groundUploads.Size();

    for (GlimmerGroundUpload& groundUpload : groundUploads)
    {
        scene->groundUploads.PushBack(std::move(groundUpload));
    }

    const Span<const GlimmerGroundUpload> newGroundUploads(scene->groundUploads.Data() + numGivenGroundUploads, scene->groundUploads.Size() - numGivenGroundUploads);

    const Vec3f viewerPosition = channelState.hasViewer ? channelState.viewerPosition : region.GetCenter();

    scene->sceneInstances->Update(frame, rpl, region, viewerPosition, *scene->triangleCache);

    GlimmerTechniqueUpdateContext context;
    context.frame = frame;
    context.world = renderSetup.world;
    context.skyProbe = renderSetup.envProbe;
    context.sceneProxies = &rpl;
    context.region = region;
    context.triangleCache = scene->triangleCache.Get();
    context.sceneInstances = scene->sceneInstances.Get();

    if (const DeferredPassData* cloudsPassData = TryCast<DeferredPassData>(renderSetup.passData))
    {
        context.cloudPass = cloudsPassData->cloudPass.Get();
    }

    if (channel)
    {
        Array<GlimmerTerrainPatchShaderData, GlimmerTempAllocator> terrainPatches;
        GlimmerHelpers::CollectGlimmerTerrainPatches(rpl, terrainPatches);

        for (View* otherView : renderSetup.world->GetViews())
        {
            if (otherView == view || !(otherView->GetFlags() & ViewFlags::GBUFFER))
            {
                continue;
            }

            RenderProxyList& cameraRpl = GetConsumerProxyList(otherView);
            cameraRpl.BeginRead();

            GlimmerHelpers::CollectGlimmerTerrainPatches(cameraRpl, terrainPatches);

            cameraRpl.EndRead();
        }

        s_statGlimmerTerrainPatches = uint32(terrainPatches.Size());

        scene->surfaceCache->Update(frame, channelState, newGroundUploads, terrainPatches.ToSpan());
        scene->isReplayingGroundUploads = false;

        scene->spanCache->Update(frame, channelState, *scene->sceneInstances, *scene->triangleCache, *scene->surfaceCache);

        context.channelState = &channelState;
        context.channel = channel.Get();
        context.surfaceCache = scene->surfaceCache.Get();
        context.spanCache = scene->spanCache->GetSpansBuffer().IsInitialized() ? scene->spanCache.Get() : nullptr;
        context.updateLighting = g_cvGlimmerEnabled.Get() && channelState.hasViewer;
    }

    scene->technique->Update(context);

    const GlimmerTriangleCacheStats triangleStats = scene->triangleCache->GetStats();

    s_statGlimmerSpanInstances = scene->sceneInstances->GetNumSpanInstances();
    s_statGlimmerResidentMeshes = triangleStats.numResident;
    s_statGlimmerBuildingMeshes = triangleStats.numBuilding + triangleStats.numPendingUpload;
    s_statGlimmerMeshesWaitingForRoom = triangleStats.numWaitingForRoom;
    s_statGlimmerPoolTriangles = triangleStats.trianglesUsed;
    s_statGlimmerBLASNodes = triangleStats.nodesUsed;
    s_statGlimmerTracedInstances = scene->sceneInstances->GetNumTracedInstances();
    s_statGlimmerLodErrorScalePercent = uint32(scene->triangleCache->GetLodErrorScale() * 100.0f + 0.5f);

    const GlimmerSceneInstancesStats& instancesStats = scene->sceneInstances->GetStats();

    s_statGlimmerSceneInstancesBuilds = instancesStats.numBuilds;
    s_statGlimmerSceneInstancesEverywhereBuilds = instancesStats.numEverywhereBuilds;
    s_statGlimmerTLASBuilds = instancesStats.numTLASBuilds;
    s_statGlimmerSceneInstancesChanges = instancesStats.numChanges;
    s_statGlimmerSceneInstancesLodOnlyChanges = instancesStats.numLodOnlyChanges;

    LogStartupStages(*scene, numCompiling);
}

bool GlimmerPass::RenderDebugView(Frame* frame, const RenderSetup& renderSetup, Framebuffer* gbufferFramebuffer, GpuImageViewRef& outImageView)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    const int debugView = g_cvGlimmerDebugView.Get();

    if (debugView < int(GlimmerDebugView::TechniqueFirst))
    {
        return false;
    }

    View* view = renderSetup.view;

    if (!view || !view->GetCamera() || !gbufferFramebuffer)
    {
        return false;
    }

    GlimmerScenePassData* scene = GetSceneForWorld(renderSetup.world);

    if (!scene || !scene->triangleCache)
    {
        return false;
    }

    GlimmerViewPassData* viewData = TryCast<GlimmerViewPassData>(FetchViewPassData(view));
    AssertDebug(viewData != nullptr);

    const Vec2u extent = MathUtil::Max(renderSetup.viewport.extent, Vec2u::One());

    if (!viewData->debugTexture.IsValid() || viewData->debugTexture->GetExtent().GetXY() != extent)
    {
        viewData->debugTexture = MakeHandle<Texture>(TextureDesc {
            TextureType::Texture2D,
            TextureFormat::RGBA16F,
            Vec3u(extent, 1),
            TextureFilterMode::Nearest,
            TextureFilterMode::Nearest,
            TextureWrapMode::ClampToEdge,
            1,
            ImageUsage::Storage | ImageUsage::Sampled });

        viewData->debugTexture->SetIsTransient(true);
        viewData->debugTexture->SetName(NAME("GlimmerDebugTexture"));
        Check(viewData->debugTexture->Create());
    }

    CommandRecorder& cr = frame->cr;

    cr << InsertBarrier(viewData->debugTexture->GetGpuImage(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);

    GlimmerDebugViewContext context;
    context.frame = frame;
    context.view = view;
    context.gbufferFramebuffer = gbufferFramebuffer;
    context.triangleCache = scene->triangleCache.Get();
    context.sceneInstances = scene->sceneInstances.Get();
    context.surfaceCache = scene->surfaceCache.Get();
    context.spanCache = scene->spanCache->GetSpansBuffer().IsInitialized() ? scene->spanCache.Get() : nullptr;
    context.outputImageView = RI.textureViewCache->GetOrCreate(viewData->debugTexture);
    context.extent = extent;
    context.techniqueView = debugView - int(GlimmerDebugView::TechniqueFirst) + 1;

    const bool rendered = scene->technique->RenderDebugView(context);

    cr << InsertBarrier(viewData->debugTexture->GetGpuImage(), ResourceState::ShaderResource, ShaderModuleType::Pixel);

    if (!rendered)
    {
        return false;
    }

    outImageView = context.outputImageView;

    return true;
}

void GlimmerPass::WriteApplyShaderData(CBufferAllocator& cbufferAllocator, World* world) const
{
    const GlimmerTechnique* technique = GetApplyTechnique(world);

    GlimmerApplyShaderData shaderData {};

    if (technique && technique->IsReady())
    {
        const int debugView = g_cvGlimmerDebugView.Get();
        const bool isApplyView = debugView == int(GlimmerDebugView::Irradiance) || debugView == int(GlimmerDebugView::Coverage);

        shaderData.params = Vec4u(isApplyView ? uint32(debugView) : 0u, 1u, uint32(MathUtil::Clamp(g_cvGlimmerDebugSH.Get(), 0, 3)), 0);
        shaderData.settings = Vec4f(MathUtil::Max(g_cvGlimmerIntensity.Get(), 0.0f), 0.0f, 0.0f, 0.0f);
    }

    cbufferAllocator.Write(&shaderData);

    GlimmerTechnique::WriteApplyShaderData(cbufferAllocator, technique);
}

uint32 GlimmerPass::BindApplyResources(CommandRecorder& cr, uint32 uniformIndex, World* world) const
{
    return GlimmerTechnique::BindApplyResources(cr, uniformIndex, GetApplyTechnique(world));
}

void GlimmerPass::OnFrameEnd(uint32 prevFrameIndex)
{
    // the base class deletes pass datas, so drop our pointers to it to prevent hanging onto dangled ones.
    for (auto it = m_scenes.Begin(); it != m_scenes.End();)
    {
        if (!it->second || it->second->view.Expired())
        {
            it = m_scenes.Erase(it);

            continue;
        }

        ++it;
    }

    PassBase::OnFrameEnd(prevFrameIndex);
}

#pragma endregion GlimmerPass

} // namespace Hyperion
