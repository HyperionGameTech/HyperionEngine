/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Glimmer/GlimmerPass.hpp>
#include <Rendering/Glimmer/GlimmerTechnique.hpp>
#include <Rendering/Glimmer/GlimmerSurfaceCache.hpp>
#include <Rendering/Glimmer/GlimmerBLASCache.hpp>
#include <Rendering/Glimmer/GlimmerTLAS.hpp>
#include <Rendering/Glimmer/GlimmerSpanCache.hpp>
#include <Rendering/Glimmer/GlimmerChannel.hpp>
#include <Rendering/Glimmer/GlimmerCVars.hpp>

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

#include <Core/Utilities/DeferredScope.hpp>

#include <Core/Threading/Threads.hpp>

#include <Core/Profiling/ProfileScope.hpp>

#include <Core/Logging/Logger.hpp>

#include <GlimmerPass.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Rendering);

static EngineStatGpuTimer s_statGlimmerScene("Rendering/GPU/Glimmer/Scene");

static EngineStatCounter<uint32> s_statGlimmerTerrainPatches("Rendering/Glimmer/TerrainPatches", false);
static EngineStatCounter<uint32> s_statGlimmerInstances("Rendering/Glimmer/Instances", false);
static EngineStatCounter<uint32> s_statGlimmerSpanInstances("Rendering/Glimmer/SpanInstances", false);
static EngineStatCounter<uint32> s_statGlimmerResidentBLASes("Rendering/Glimmer/ResidentBLASes", false);
static EngineStatCounter<uint32> s_statGlimmerBuildingBLASes("Rendering/Glimmer/BuildingBLASes", false);

// Terrain patches of a cell share its material and transform, so they merge into one entry covering all of them
static void CollectTerrainPatches(RenderProxyList& rpl, Array<GlimmerTerrainPatchShaderData>& outPatches)
{
    for (Entity* entity : rpl.GetMeshEntities())
    {
        RenderProxyMesh* proxy = rpl.GetMeshEntities().GetProxy(entity->Id());

        if (!proxy || !proxy->mesh || !proxy->material || proxy->numInstances != 0)
        {
            continue;
        }

        if (proxy->attributes.GetMaterialAttributes().shaderName != "Terrain"_sh)
        {
            continue;
        }

        const uint32 materialIndex = Resources::GetBinding(proxy->material);

        if (materialIndex == ~0u)
        {
            continue;
        }

        const Mat4f& objectToWorld = proxy->bufferData.modelMatrix;
        const Mat4f worldToObject = objectToWorld.Inverse();
        const BoundingBox worldBounds = objectToWorld * proxy->meshAabb;

        if (!worldBounds.IsValid() || !worldBounds.IsFinite())
        {
            continue;
        }

        GlimmerTerrainPatchShaderData patch {};
        Memory::Copy(&patch.worldToObject0, &worldToObject.values[0], sizeof(Vec4f));
        Memory::Copy(&patch.worldToObject2, &worldToObject.values[8], sizeof(Vec4f));
        patch.boundsXZ = Vec4f(worldBounds.min.x, worldBounds.min.z, worldBounds.max.x, worldBounds.max.z);
        patch.data = Vec4u(materialIndex, 0, 0, 0);

        GlimmerTerrainPatchShaderData* existing = outPatches.FindIf([&patch](const GlimmerTerrainPatchShaderData& other)
            {
                return other.data.x == patch.data.x && other.worldToObject0 == patch.worldToObject0 && other.worldToObject2 == patch.worldToObject2;
            });

        if (existing != outPatches.End())
        {
            existing->boundsXZ = Vec4f(
                MathUtil::Min(existing->boundsXZ.x, patch.boundsXZ.x),
                MathUtil::Min(existing->boundsXZ.y, patch.boundsXZ.y),
                MathUtil::Max(existing->boundsXZ.z, patch.boundsXZ.z),
                MathUtil::Max(existing->boundsXZ.w, patch.boundsXZ.w));

            continue;
        }

        if (outPatches.Size() < GlimmerMaxTerrainPatches)
        {
            outPatches.PushBack(patch);
        }
    }
}

#pragma region GlimmerScenePassData

GlimmerScenePassData::GlimmerScenePassData()
{
}

GlimmerScenePassData::~GlimmerScenePassData()
{
    technique.Reset();

    if (tlas && blasCache)
    {
        tlas->Release(*blasCache);
    }
}

#pragma endregion GlimmerScenePassData

#pragma region GlimmerViewPassData

GlimmerViewPassData::GlimmerViewPassData()
{
}

GlimmerViewPassData::~GlimmerViewPassData()
{
    debugTexture = Handle<Texture>();
}

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
    for (uint32 typeIndex = 0; typeIndex < uint32(GlimmerTechniqueType::Count); typeIndex++)
    {
        m_placeholderTechniques[typeIndex] = CreateGlimmerTechnique(GlimmerTechniqueType(typeIndex));
    }
}

void GlimmerPass::Shutdown()
{
    m_scenes.Clear();

    for (UniquePtr<GlimmerTechnique>& placeholderTechnique : m_placeholderTechniques)
    {
        placeholderTechnique.Reset();
    }
}

PassData* GlimmerPass::CreateViewPassData(View* view, PassDataExt&)
{
    if (view->GetFlags() & ViewFlags::GLIMMER_SCENE_VIEW)
    {
        GlimmerScenePassData* passData = new GlimmerScenePassData();
        passData->view = MakeWeakRef(view);
        passData->tlas = MakeUnique<GlimmerTLAS>();
        passData->surfaceCache = MakeUnique<GlimmerSurfaceCache>();
        passData->spanCache = MakeUnique<GlimmerSpanCache>();
        passData->technique = CreateGlimmerTechnique(GetActiveGlimmerTechniqueType());

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

const GlimmerTechnique& GlimmerPass::GetApplyTechnique(World* world) const
{
    GlimmerScenePassData* scene = g_cvGlimmerEnabled.Get() ? GetSceneForWorld(world) : nullptr;

    const GlimmerTechniqueType activeType = GetActiveGlimmerTechniqueType();

    if (scene && scene->technique->GetType() == activeType && scene->technique->IsReady())
    {
        return *scene->technique;
    }

    return *m_placeholderTechniques[uint32(activeType)];
}

void GlimmerPass::RenderFrame(Frame* frame, const RenderSetup& renderSetup)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    View* view = renderSetup.view;
    AssertDebug(view != nullptr && (view->GetFlags() & ViewFlags::GLIMMER_SCENE_VIEW));

    if (!IsGlimmerSceneRequired())
    {
        return;
    }

    GlimmerScenePassData* scene = DynamicCast<GlimmerScenePassData>(FetchViewPassData(view));
    AssertDebug(scene != nullptr);

    const uint32 frameCounter = GetFrameCounter();

    if (scene->lastUpdatedFrame == frameCounter)
    {
        return;
    }

    scene->lastUpdatedFrame = frameCounter;
    scene->world = renderSetup.world;

    if (scene->technique->GetType() != GetActiveGlimmerTechniqueType())
    {
        scene->technique = CreateGlimmerTechnique(GetActiveGlimmerTechniqueType());
    }

    m_scenes.Set(renderSetup.world, scene);

    RenderProxyList& rpl = GetConsumerProxyList(view);
    rpl.BeginRead();

    HYP_DEFER({ rpl.EndRead(); });

    ENGINE_STAT_GPU_SCOPE(&s_statGlimmerScene);

    // the system keeps the view's ortho box on the region; recover it from the matrices it set
    BoundingBox regionNDC;
    regionNDC.min = Vec3f(-1.0f);
    regionNDC.max = Vec3f(1.0f);

    const BoundingBox region = rpl.cachedMatrices.viewProj.Inverse() * regionNDC;

    if (!region.IsValid() || !region.IsFinite())
    {
        return;
    }

    scene->region = region;

    // every world's scene draws its BLASes from the same pool; acquired here so worlds without a scene never create it
    if (!scene->blasCache)
    {
        scene->blasCache = GlimmerBLASCache::AcquireShared();
    }

    scene->blasCache->UpdateOncePerFrame(frame);

    const bool tlasSwapped = scene->tlas->Update(frame, rpl, region, scene->technique->GetTracedRegion(region), *scene->blasCache);

    GlimmerTechniqueUpdateContext context;
    context.frame = frame;
    context.world = renderSetup.world;
    context.skyProbe = renderSetup.envProbe;
    context.sceneProxies = &rpl;
    context.region = region;
    context.blasCache = scene->blasCache.Get();
    context.tlas = scene->tlas.Get();
    context.tlasSwapped = tlasSwapped;

    GlimmerChannelState channelState;

    if (SharedPtr<GlimmerChannel> channel = GlimmerChannel::Get(renderSetup.world))
    {
        Array<GlimmerGroundUpload> groundUploads;

        channel->Consume(channelState, groundUploads);

        // the scene view only reaches the scene region; the cameras see the terrain out to the horizon
        Array<GlimmerTerrainPatchShaderData> terrainPatches;
        CollectTerrainPatches(rpl, terrainPatches);

        for (View* otherView : renderSetup.world->GetViews())
        {
            if (otherView == view || !(otherView->GetFlags() & ViewFlags::GBUFFER))
            {
                continue;
            }

            RenderProxyList& cameraRpl = GetConsumerProxyList(otherView);
            cameraRpl.BeginRead();

            CollectTerrainPatches(cameraRpl, terrainPatches);

            cameraRpl.EndRead();
        }

        s_statGlimmerTerrainPatches = uint32(terrainPatches.Size());

        scene->surfaceCache->Update(frame, channelState, groundUploads.ToSpan(), terrainPatches.ToSpan());
        scene->spanCache->Update(frame, channelState, *scene->tlas, *scene->blasCache, *scene->surfaceCache);

        context.channelState = &channelState;
        context.surfaceCache = scene->surfaceCache.Get();
        context.spanCache = scene->spanCache->GetSpansBuffer().IsValid() ? scene->spanCache.Get() : nullptr;
        context.updateLighting = g_cvGlimmerEnabled.Get() && channelState.hasViewer;
    }

    scene->technique->Update(context);

    const GlimmerBLASCacheStats blasStats = scene->blasCache->GetStats();

    s_statGlimmerInstances = scene->tlas->GetNumInstances();
    s_statGlimmerSpanInstances = scene->tlas->GetNumSpanInstances();
    s_statGlimmerResidentBLASes = blasStats.numResident;
    s_statGlimmerBuildingBLASes = blasStats.numBuilding + blasStats.numPendingUpload;
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

    if (!scene || !scene->blasCache)
    {
        return false;
    }

    GlimmerViewPassData* viewData = DynamicCast<GlimmerViewPassData>(FetchViewPassData(view));
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
    context.blasCache = scene->blasCache.Get();
    context.tlas = scene->tlas.Get();
    context.surfaceCache = scene->surfaceCache.Get();
    context.spanCache = scene->spanCache->GetSpansBuffer().IsValid() ? scene->spanCache.Get() : nullptr;
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
    const GlimmerTechnique& technique = GetApplyTechnique(world);

    GlimmerApplyShaderData shaderData {};

    if (technique.IsReady())
    {
        shaderData.params = Vec4u(g_cvGlimmerDebugView.Get() == int(GlimmerDebugView::Irradiance) ? 1u : 0u, 1u, 0, 0);
        shaderData.settings = Vec4f(MathUtil::Max(g_cvGlimmerIntensity.Get(), 0.0f), 0.0f, 0.0f, 0.0f);
    }

    cbufferAllocator.Write(&shaderData);

    technique.WriteApplyShaderData(cbufferAllocator);
}

uint32 GlimmerPass::BindApplyResources(CommandRecorder& cr, uint32 uniformIndex, World* world) const
{
    return GetApplyTechnique(world).BindApplyResources(cr, uniformIndex);
}

void GlimmerPass::OnFrameEnd(uint32 prevFrameIndex)
{
    // the base class deletes pass data of views that went away, so drop our pointers to it first
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
