/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Glimmer/GlimmerPass.hpp>
#include <Rendering/Glimmer/GlimmerBLASCache.hpp>
#include <Rendering/Glimmer/GlimmerTLAS.hpp>
#include <Rendering/Glimmer/GlimmerFootprintMask.hpp>
#include <Rendering/Glimmer/GlimmerSurfaceCache.hpp>
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
#include <Rendering/GBuffer.hpp>
#include <Rendering/GpuBuffer.hpp>
#include <Rendering/GpuImage.hpp>
#include <Rendering/GpuImageView.hpp>
#include <Rendering/Texture.hpp>
#include <Rendering/TextureViewCache.hpp>
#include <Rendering/ShaderManager.hpp>
#include <Rendering/PlaceholderData.hpp>
#include <Rendering/Frame.hpp>
#include <Rendering/Material.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

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
static EngineStatGpuTimer s_statGlimmerSWRTDebug("Rendering/GPU/Glimmer/SWRTDebug");

static EngineStatCounter<uint32> s_statGlimmerInstances("Rendering/Glimmer/Instances", false);
static EngineStatCounter<uint32> s_statGlimmerResidentBLASes("Rendering/Glimmer/ResidentBLASes", false);
static EngineStatCounter<uint32> s_statGlimmerBuildingBLASes("Rendering/Glimmer/BuildingBLASes", false);
static EngineStatCounter<uint32> s_statGlimmerSWRTDebugRays("Rendering/Glimmer/SWRTDebugRays");
static EngineStatCounter<uint32> s_statGlimmerTerrainPatches("Rendering/Glimmer/TerrainPatches", false);

// Must match GlimmerSWRTDebugConstants in Shaders/Glimmer/GlimmerSWRTDebug.hlsl
struct GlimmerSWRTDebugConstants
{
    Vec4u dimensionsModeInstances;
    GlimmerFootprintMaskShaderData mask;
    Vec4f params;
    GlimmerGroundShaderData ground;
    GlimmerSpanShaderData spans;
};

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
    : m_lastBLASCacheUpdateFrame(~0u)
{
}

GlimmerPass::~GlimmerPass()
{
}

void GlimmerPass::Initialize()
{
    m_blasCache = MakeShared<GlimmerBLASCache>();
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
        passData->blasCache = m_blasCache;
        passData->tlas = MakeUnique<GlimmerTLAS>();
        passData->footprintMask = MakeUnique<GlimmerFootprintMask>();
        passData->surfaceCache = MakeUnique<GlimmerSurfaceCache>();
        passData->spanCache = MakeUnique<GlimmerSpanCache>();
        passData->probeVolume = MakeUnique<GlimmerProbeVolume>();

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

    m_scenes.Set(renderSetup.world, scene);

    RenderProxyList& rpl = GetConsumerProxyList(view);
    rpl.BeginRead();

    HYP_DEFER({ rpl.EndRead(); });

    ENGINE_STAT_GPU_SCOPE(&s_statGlimmerScene);

    // shared by every world, so only once per frame
    if (m_lastBLASCacheUpdateFrame != frameCounter)
    {
        m_lastBLASCacheUpdateFrame = frameCounter;

        m_blasCache->Update(frame);
    }

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

    // SWRT only covers the middle of the collected region; the rest is only splatted into the heightfield
    const Vec3f regionCenter = region.GetCenter();
    const float swrtRadius = MathUtil::Min(MathUtil::Max(g_cvGlimmerNearFieldRadius.Get(), 8.0f), 0.5f * region.GetExtent().x);

    BoundingBox swrtRegion = region;
    swrtRegion.min.x = regionCenter.x - swrtRadius;
    swrtRegion.max.x = regionCenter.x + swrtRadius;
    swrtRegion.min.z = regionCenter.z - swrtRadius;
    swrtRegion.max.z = regionCenter.z + swrtRadius;

    if (scene->tlas->Update(frame, rpl, region, swrtRegion, *m_blasCache))
    {
        scene->footprintMask->Rebuild(frame, *scene->tlas, Vec2f(regionCenter.x, regionCenter.z), swrtRadius);
    }

    const GlimmerBLASCacheStats blasStats = m_blasCache->GetStats();

    SharedPtr<GlimmerChannel> channel = GlimmerChannel::Get(renderSetup.world);

    if (channel)
    {
        GlimmerChannelState channelState;
        Array<GlimmerGroundUpload> groundUploads;

        channel->Consume(channelState, groundUploads);

        // the scene view only reaches the SWRT region; the cameras see the terrain out to the horizon
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
        scene->spanCache->Update(frame, channelState, *scene->tlas, *m_blasCache, *scene->surfaceCache);

        if (g_cvGlimmerEnabled.Get() && channelState.hasViewer)
        {
            GlimmerProbeUpdateInputs inputs;
            inputs.viewerPosition = channelState.viewerPosition;
            inputs.surfaceCache = scene->surfaceCache.Get();
            inputs.spanCache = scene->spanCache.Get();
            inputs.blasCache = m_blasCache.Get();
            inputs.tlas = scene->tlas.Get();
            inputs.skyProbe = renderSetup.envProbe;

            scene->probeVolume->Update(frame, inputs);
        }
    }

    s_statGlimmerInstances = scene->tlas->GetNumInstances();
    s_statGlimmerResidentBLASes = blasStats.numResident;
    s_statGlimmerBuildingBLASes = blasStats.numBuilding + blasStats.numPendingUpload;
}

bool GlimmerPass::RenderDebugView(Frame* frame, const RenderSetup& renderSetup, Framebuffer* gbufferFramebuffer, GpuImageViewRef& outImageView)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    const int debugView = g_cvGlimmerDebugView.Get();

    if (debugView <= int(GlimmerDebugView::None) || debugView >= int(GlimmerDebugView::Irradiance))
    {
        return false;
    }

    View* view = renderSetup.view;

    if (!view || !view->GetCamera() || !gbufferFramebuffer)
    {
        return false;
    }

    GlimmerScenePassData* scene = GetSceneForWorld(renderSetup.world);

    if (!scene || !scene->tlas->IsReady() || !m_blasCache->IsReady())
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
        viewData->debugTexture->SetName(NAME("GlimmerSWRTDebugTexture"));
        Check(viewData->debugTexture->Create());
    }

    ENGINE_STAT_GPU_SCOPE(&s_statGlimmerSWRTDebug);

    GlimmerSWRTDebugConstants constants {};
    constants.dimensionsModeInstances = Vec4u(extent.x, extent.y, uint32(debugView), scene->tlas->GetNumInstances());
    constants.mask = scene->footprintMask->GetShaderData();
    constants.params = Vec4f(10000.0f, 0.0f, 0.0f, 0.0f);
    constants.ground = scene->surfaceCache->GetGroundShaderData();
    constants.spans = scene->spanCache->GetShaderData();

    GpuBuffer* cbuffer = nullptr;
    size_t cbufferOffset = 0;
    size_t cbufferSize = 0;

    RI.cbufferAllocator->Write(&constants);
    RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

    CommandRecorder& cr = frame->cr;

    const GpuImageRef& depthImage = gbufferFramebuffer->GetAttachment(GBufferTarget::Depth)->GetGpuImage();
    const GpuImageRef& normalsImage = gbufferFramebuffer->GetAttachment(GBufferTarget::Normals)->GetGpuImage();

    cr << InsertBarrier(depthImage, ResourceState::ShaderResource, ShaderModuleType::Compute);
    cr << InsertBarrier(normalsImage, ResourceState::ShaderResource, ShaderModuleType::Compute);
    cr << InsertBarrier(viewData->debugTexture->GetGpuImage(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);

    cr << SetCurrentShader(ShaderDesc(NAME("GlimmerSWRTDebug")));

    uint32 uniformIndex = 0;

    cr << SetShaderUniform(uniformIndex++, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));
    cr << SetShaderUniform(uniformIndex++, "OutImage"_sh, RI.textureViewCache->GetOrCreate(viewData->debugTexture));
    cr << SetShaderUniform(uniformIndex++, "GBufferDepthTexture"_sh, gbufferFramebuffer->GetAttachment(GBufferTarget::Depth)->GetImageView());
    cr << SetShaderUniform(uniformIndex++, "GBufferNormalsTexture"_sh, gbufferFramebuffer->GetAttachment(GBufferTarget::Normals)->GetImageView());
    cr << SetShaderUniform(uniformIndex++, "CamerasBuffer"_sh, RI.namedBuffers[NamedBuffer::Cameras], Resources::GetBinding(view->GetCamera()));
    cr << SetShaderUniform(uniformIndex++, "MaterialsBuffer"_sh, RI.namedBuffers[NamedBuffer::Materials]);
    cr << SetShaderUniform(uniformIndex++, "SamplerLinearMipmap"_sh, RI.placeholderData->GetSamplerLinearMipmap());
    cr << SetShaderUniform(uniformIndex++, "GlimmerTLASNodesBuffer"_sh, scene->tlas->GetNodesBuffer().Get(), ShaderDataOffset(0, sizeof(GlimmerBVHNode)));
    cr << SetShaderUniform(uniformIndex++, "GlimmerInstancesBuffer"_sh, scene->tlas->GetInstancesBuffer().Get(), ShaderDataOffset(0, sizeof(GlimmerInstanceShaderData)));
    cr << SetShaderUniform(uniformIndex++, "GlimmerBLASNodesBuffer"_sh, m_blasCache->GetNodesBuffer().Get(), ShaderDataOffset(0, sizeof(GlimmerBVHNode)));
    cr << SetShaderUniform(uniformIndex++, "GlimmerBLASTrianglesBuffer"_sh, m_blasCache->GetTrianglesBuffer().Get(), ShaderDataOffset(0, sizeof(GlimmerTriangle)));
    cr << SetShaderUniform(uniformIndex++, "FootprintMaskBuffer"_sh, scene->footprintMask->GetMaskBuffer().Get(), ShaderDataOffset(0, sizeof(uint32)));
    cr << SetShaderUniform(uniformIndex++, "GlimmerGroundTexture"_sh, scene->surfaceCache->GetGroundImageView());
    cr << SetShaderUniform(uniformIndex++, "GlimmerSpansBuffer"_sh, scene->spanCache->GetSpansBuffer().IsValid() ? scene->spanCache->GetSpansBuffer().Get() : scene->footprintMask->GetMaskBuffer().Get(), ShaderDataOffset(0, sizeof(uint32)));
    cr << SetShaderUniform(uniformIndex++, "GlimmerGroundAlbedoTexture"_sh, scene->surfaceCache->GetGroundAlbedoImageView());

    cr << DispatchCompute(Vec3u { (extent.x + 7) / 8, (extent.y + 7) / 8, 1 });

    cr << InsertBarrier(viewData->debugTexture->GetGpuImage(), ResourceState::ShaderResource, ShaderModuleType::Pixel);

    s_statGlimmerSWRTDebugRays += extent.x * extent.y;

    outImageView = RI.textureViewCache->GetOrCreate(viewData->debugTexture);

    return true;
}

void GlimmerPass::WriteApplyShaderData(CBufferAllocator& cbufferAllocator, World* world) const
{
    GlimmerApplyShaderData shaderData {};

    GlimmerScenePassData* scene = g_cvGlimmerEnabled.Get() ? GetSceneForWorld(world) : nullptr;

    if (scene && scene->probeVolume && scene->probeVolume->IsReady())
    {
        shaderData.volume = scene->probeVolume->GetShaderData();
        shaderData.params = Vec4u(g_cvGlimmerDebugView.Get() == int(GlimmerDebugView::Irradiance) ? 1u : 0u, 0, 0, 0);
    }

    cbufferAllocator.Write(&shaderData);
}

uint32 GlimmerPass::BindApplyResources(CommandRecorder& cr, uint32 uniformIndex, World* world) const
{
    GlimmerScenePassData* scene = g_cvGlimmerEnabled.Get() ? GetSceneForWorld(world) : nullptr;

    const GlimmerProbeVolume* probeVolume = (scene && scene->probeVolume && scene->probeVolume->IsReady())
        ? scene->probeVolume.Get()
        : nullptr;

    if (probeVolume)
    {
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeSH0Texture"_sh, probeVolume->GetSHImageView(0));
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeSH1Texture"_sh, probeVolume->GetSHImageView(1));
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeSH2Texture"_sh, probeVolume->GetSHImageView(2));
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeStateTexture"_sh, probeVolume->GetStateImageView());
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeBaseTexture"_sh, probeVolume->GetBaseImageView());
    }
    else
    {
        // never sampled: the zeroed constants tell lighting to skip Glimmer
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeSH0Texture"_sh, RI.placeholderData->GetImageView3D1x1x1R8());
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeSH1Texture"_sh, RI.placeholderData->GetImageView3D1x1x1R8());
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeSH2Texture"_sh, RI.placeholderData->GetImageView3D1x1x1R8());
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeStateTexture"_sh, RI.placeholderData->GetImageView3D1x1x1R8());
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeBaseTexture"_sh, RI.placeholderData->GetImageView2D1x1R8Array());
    }

    return uniformIndex;
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
