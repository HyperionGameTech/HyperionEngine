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

#include <Rendering/Util/DeletionQueue.hpp>

#include <Scene/Camera/Camera.hpp>

#include <Framework/EngineStats.hpp>
#include <Framework/View.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/FileSystem/FilePath.hpp>

#include <Util/Img/WritePng.hpp>

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

static constexpr uint32 StatsLogIntervalFrames = 120;

// Must match GlimmerSWRTDebugConstants in Shaders/Glimmer/GlimmerSWRTDebug.hlsl
struct GlimmerSWRTDebugConstants
{
    Vec4u dimensionsModeInstances;
    GlimmerFootprintMaskShaderData mask;
    Vec4f params;
    GlimmerGroundShaderData ground;
    GlimmerSpanShaderData spans;
};

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
    captureTexture = Handle<Texture>();
}

#pragma endregion GlimmerViewPassData

#pragma region GlimmerPass

GlimmerPass::GlimmerPass()
    : m_lastBLASCacheUpdateFrame(~0u),
      m_lastCaptureIndex(0),
      m_lastFrameCaptureIndex(0)
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

        scene->surfaceCache->Update(frame, channelState, groundUploads.ToSpan());
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

    if (g_cvGlimmerSWRTLogStats.Get() && frameCounter % StatsLogIntervalFrames == 0)
    {
        LogStats(*scene);
    }
}

bool GlimmerPass::RenderDebugView(Frame* frame, const RenderSetup& renderSetup, Framebuffer* gbufferFramebuffer, GpuImageViewRef& outImageView)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    const int debugView = g_cvGlimmerSWRTDebugView.Get();

    if (debugView <= int(GlimmerSWRTDebugView::None) || debugView >= int(GlimmerSWRTDebugView::Max))
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
        viewData->debugTexture = Handle<Texture>();

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

    cr << DispatchCompute(Vec3u { (extent.x + 7) / 8, (extent.y + 7) / 8, 1 });

    cr << InsertBarrier(viewData->debugTexture->GetGpuImage(), ResourceState::ShaderResource, ShaderModuleType::Pixel);

    s_statGlimmerSWRTDebugRays += extent.x * extent.y;

    if (g_cvGlimmerSWRTLogStats.Get() && GetFrameCounter() % StatsLogIntervalFrames == 0)
    {
        const float gpuMs = s_statGlimmerSWRTDebug.GetValue();
        const float raysPerSecond = gpuMs > 0.0f ? float(extent.x * extent.y) / (gpuMs * 0.001f) : 0.0f;

        HYP_LOG(Rendering, Info, "Glimmer SWRT debug: {}x{} primary rays, {} ms, {} Mrays/s",
            extent.x, extent.y, gpuMs, raysPerSecond / 1.0e6f);
    }

    const int captureIndex = g_cvGlimmerSWRTCaptureDebugView.Get();

    if (captureIndex != 0 && captureIndex != m_lastCaptureIndex)
    {
        m_lastCaptureIndex = captureIndex;

        CaptureTexture(viewData->debugTexture, "glimmer_swrt", captureIndex);
    }

    outImageView = RI.textureViewCache->GetOrCreate(viewData->debugTexture);

    return true;
}

static float HalfToFloat(uint16 half)
{
    const uint32 sign = uint32(half & 0x8000u) << 16;
    const uint32 exponent = (half >> 10) & 0x1Fu;
    const uint32 mantissa = half & 0x3FFu;

    uint32 bits;

    if (exponent == 0)
    {
        bits = sign; // denormals are far below anything visible in an 8 bit capture
    }
    else if (exponent == 31)
    {
        bits = sign | 0x7F800000u | (mantissa << 13);
    }
    else
    {
        bits = sign | ((exponent + 112u) << 23) | (mantissa << 13);
    }

    float result;
    Memory::Copy(&result, &bits, sizeof(float));

    return result;
}

static ubyte LinearToSrgbByte(float value)
{
    value = MathUtil::Clamp(value, 0.0f, 1.0f);

    const float srgb = value <= 0.0031308f
        ? value * 12.92f
        : 1.055f * MathUtil::Pow(value, 1.0f / 2.4f) - 0.055f;

    return ubyte(MathUtil::Clamp(srgb * 255.0f + 0.5f, 0.0f, 255.0f));
}

void GlimmerPass::CaptureTexture(const Handle<Texture>& texture, const char* prefix, int captureIndex)
{
    const Vec2u extent = texture->GetExtent().GetXY();
    const String filename = HYP_FORMAT("{}_{}.png", prefix, captureIndex);

    texture->EnqueueReadback(
        [extent, filename](GpuBuffer& buffer)
        {
            const size_t expectedSize = size_t(extent.x) * size_t(extent.y) * 4 * sizeof(uint16);

            if (buffer.Size() < expectedSize)
            {
                HYP_LOG(Rendering, Warning, "Glimmer debug capture readback returned {} bytes, expected {}", buffer.Size(), expectedSize);

                return;
            }

            const uint16* halves = static_cast<const uint16*>(buffer.Map());

            Array<ubyte> pixels;
            pixels.Resize(size_t(extent.x) * size_t(extent.y) * 3);

            for (size_t pixelIndex = 0; pixelIndex < size_t(extent.x) * size_t(extent.y); pixelIndex++)
            {
                for (size_t channel = 0; channel < 3; channel++)
                {
                    pixels[pixelIndex * 3 + channel] = LinearToSrgbByte(HalfToFloat(halves[pixelIndex * 4 + channel]));
                }
            }

            buffer.Unmap();

            const FilePath directory = FilePath::Current() / "GlimmerCaptures";

            if (!directory.Exists())
            {
                directory.MkDir();
            }

            const FilePath filepath = directory / filename.Data();

            if (WritePng::Write(filepath, extent.x, extent.y, 3, pixels.Data()))
            {
                HYP_LOG(Rendering, Info, "Glimmer debug capture written to {}", filepath);
            }
            else
            {
                HYP_LOG(Rendering, Warning, "Failed to write Glimmer debug capture to {}", filepath);
            }
        });
}

void GlimmerPass::WriteApplyShaderData(CBufferAllocator& cbufferAllocator, World* world) const
{
    GlimmerApplyShaderData shaderData {};

    GlimmerScenePassData* scene = g_cvGlimmerEnabled.Get() ? GetSceneForWorld(world) : nullptr;

    if (scene && scene->probeVolume && scene->probeVolume->IsReady())
    {
        shaderData.volume = scene->probeVolume->GetShaderData();
        shaderData.params = Vec4u(uint32(MathUtil::Max(g_cvGlimmerDebugVis.Get(), 0)), 0, 0, 0);
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

void GlimmerPass::CaptureFinalImage(Frame* frame, const RenderSetup& renderSetup, const GpuImageViewRef& finalImageView)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    const int captureIndex = g_cvGlimmerCaptureFrame.Get();

    if (captureIndex == 0 || captureIndex == m_lastFrameCaptureIndex || !renderSetup.view || !finalImageView.IsValid())
    {
        return;
    }

    const GpuImageRef& sourceImage = finalImageView->GetImage();

    if (!sourceImage.IsValid())
    {
        return;
    }

    m_lastFrameCaptureIndex = captureIndex;

    GlimmerViewPassData* viewData = DynamicCast<GlimmerViewPassData>(FetchViewPassData(renderSetup.view));
    AssertDebug(viewData != nullptr);

    const Vec3u extent = sourceImage->GetExtent();

    // the capture is read back as RGBA16F, which is what the tonemap and TAA outputs are
    if (!viewData->captureTexture.IsValid() || viewData->captureTexture->GetExtent() != extent)
    {
        viewData->captureTexture = MakeHandle<Texture>(TextureDesc {
            TextureType::Texture2D,
            TextureFormat::RGBA16F,
            extent,
            TextureFilterMode::Nearest,
            TextureFilterMode::Nearest,
            TextureWrapMode::ClampToEdge,
            1,
            ImageUsage::Sampled | ImageUsage::Attachment });

        viewData->captureTexture->SetIsTransient(true);
        viewData->captureTexture->SetName(NAME("GlimmerFrameCapture"));
        Check(viewData->captureTexture->Create());
    }

    CommandRecorder& cr = frame->cr;

    const GpuImageRef& captureImage = viewData->captureTexture->GetGpuImage();
    const ResourceState previousSourceState = sourceImage->GetResourceState();

    cr << InsertBarrier(sourceImage, ResourceState::CopySrc);
    cr << InsertBarrier(captureImage, ResourceState::CopyDst);
    cr << CopyImage(sourceImage, captureImage, extent);
    cr << InsertBarrier(captureImage, ResourceState::ShaderResource);

    cr << InsertBarrier(sourceImage,
        (previousSourceState != ResourceState::Undefined && previousSourceState != ResourceState::PreInitialized) ? previousSourceState : ResourceState::ShaderResource);

    CaptureTexture(viewData->captureTexture, "glimmer_frame", captureIndex);
}

void GlimmerPass::LogStats(const GlimmerScenePassData& scene) const
{
    const GlimmerBLASCacheStats blasStats = m_blasCache->GetStats();
    const GlimmerTLASStats& tlasStats = scene.tlas->GetStats();

    HYP_LOG(Rendering, Info,
        "Glimmer scene: {} instances, {} span instances ({} tris), {} TLAS nodes (depth {}), last build {} ms, {} builds, {} instances waiting on BLAS | "
        "BLAS: {} resident ({} tris), {} building, {} pending upload, {} failed | pool nodes {}/{}, tris {}/{}",
        tlasStats.numInstances,
        tlasStats.numSpanInstances,
        tlasStats.numSpanTriangles,
        tlasStats.numNodes,
        tlasStats.depth,
        tlasStats.lastBuildMs,
        tlasStats.numBuilds,
        tlasStats.numWaitingForBLAS,
        blasStats.numResident,
        blasStats.numResidentTriangles,
        blasStats.numBuilding,
        blasStats.numPendingUpload,
        blasStats.numFailed,
        blasStats.nodesUsed,
        blasStats.nodesCapacity,
        blasStats.trianglesUsed,
        blasStats.trianglesCapacity);
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
