/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Clouds/CloudPass.hpp>
#include <Rendering/Clouds/CloudResources.hpp>

#include <Rendering/Passes/DeferredPass.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/RenderSetup.hpp>
#include <Rendering/RenderProxy.hpp>
#include <Rendering/RenderProxyList.hpp>
#include <Rendering/CommandRecorder.hpp>
#include <Rendering/CBufferAllocator.hpp>
#include <Rendering/DepthPyramidRenderer.hpp>
#include <Rendering/Frame.hpp>
#include <Rendering/GBuffer.hpp>
#include <Rendering/Mesh.hpp>
#include <Rendering/Texture.hpp>
#include <Rendering/TextureViewCache.hpp>
#include <Rendering/SamplerCache.hpp>
#include <Rendering/PlaceholderData.hpp>
#include <Rendering/ShaderManager.hpp>
#include <Rendering/StencilMasks.hpp>

#include <Rendering/Util/DeletionQueue.hpp>
#include <Rendering/Util/MeshBuilder.hpp>

#include <Scene/View.hpp>
#include <Scene/Light.hpp>
#include <Scene/EnvProbe.hpp>
#include <Scene/Sky/CloudEffectVolume.hpp>

#include <Framework/CVarManager.hpp>
#include <Framework/EngineStats.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Utilities/DeferredScope.hpp>

///////////////////////////////////////////////////////////
////////// https://www.jpgrenier.org/clouds.html //////////
///////////////////////////////////////////////////////////

namespace Hyperion {

extern uint32 GetFrameCounter();

CVar<uint32> g_cvCloudsTraceSteps { "Rendering.Clouds.TraceSteps", 48 };
CVar<uint32> g_cvCloudsLightSteps { "Rendering.Clouds.LightSteps", 6 };

static EngineStatGpuTimer s_statCloudTrace("Rendering/GPU/CloudTrace");
static EngineStatGpuTimer s_statCloudReconstruct("Rendering/GPU/CloudReconstruct");
static EngineStatGpuTimer s_statCloudComposite("Rendering/GPU/CloudComposite");

// ordered 2x2 pattern, so any four consecutive frames trace every pixel of a block once
static const Vec2u TraceOffsetSequence[4] = {
    Vec2u { 0, 0 },
    Vec2u { 1, 1 },
    Vec2u { 1, 0 },
    Vec2u { 0, 1 }
};

struct CloudTraceConstants
{
    Vec2u traceDimensions;
    uint32 traceSteps;
    uint32 lightSteps;

    uint32 frameCounter;
    uint32 hasSun;
    Vec2u traceOffset;

    Vec2u historyDimensions;
    uint32 _pad0;
    uint32 _pad1;
};

struct CloudReconstructConstants
{
    Mat4f previousViewProjection;

    Vec2u traceDimensions;
    Vec2u historyDimensions;

    Vec2u traceOffset;
    uint32 historyValid;
    uint32 _pad0;
};

CloudPass::CloudPass(const Vec2u& extent, GBuffer* gbuffer)
    : m_extent(extent),
      m_historyExtent(MathUtil::Max((extent.x + 1) / 2, 1u), MathUtil::Max((extent.y + 1) / 2, 1u)),
      m_traceExtent(MathUtil::Max((m_historyExtent.x + 1) / 2, 1u), MathUtil::Max((m_historyExtent.y + 1) / 2, 1u)),
      m_gbuffer(gbuffer),
      m_historyIndex(0),
      m_lastRenderedFrame(~0u),
      m_hasRenderedThisFrame(false)
{
}

CloudPass::~CloudPass()
{
    if (m_traceTexture.IsValid())
    {
        EnqueueDeletion(std::move(m_traceTexture));
    }

    if (m_traceDistanceTexture.IsValid())
    {
        EnqueueDeletion(std::move(m_traceDistanceTexture));
    }

    for (uint32 historySlot = 0; historySlot < 2; historySlot++)
    {
        if (m_historyTextures[historySlot].IsValid())
        {
            EnqueueDeletion(std::move(m_historyTextures[historySlot]));
        }

        if (m_historyDistanceTextures[historySlot].IsValid())
        {
            EnqueueDeletion(std::move(m_historyDistanceTextures[historySlot]));
        }
    }
}

void CloudPass::Create()
{
    const auto createCloudTexture = [](const Vec2u& extent, TextureFormat format, Name name)
    {
        Handle<Texture> texture = MakeHandle<Texture>(TextureDesc {
            TextureType::Texture2D,
            format,
            Vec3u { extent.x, extent.y, 1 },
            TextureFilterMode::Linear,
            TextureFilterMode::Linear,
            TextureWrapMode::ClampToEdge,
            1,
            ImageUsage::Storage | ImageUsage::Sampled });

        texture->SetIsTransient(true);
        texture->SetName(name);

        Check(texture->Create());

        return texture;
    };

    m_traceTexture = createCloudTexture(m_traceExtent, TextureFormat::RGBA16F, NAME("CloudTraceTexture"));
    m_traceDistanceTexture = createCloudTexture(m_traceExtent, TextureFormat::R32F, NAME("CloudTraceDistanceTexture"));

    for (uint32 historySlot = 0; historySlot < 2; historySlot++)
    {
        m_historyTextures[historySlot] = createCloudTexture(m_historyExtent, TextureFormat::RGBA16F, NAME_FMT("CloudHistoryTexture{}", historySlot));
        m_historyDistanceTextures[historySlot] = createCloudTexture(m_historyExtent, TextureFormat::R32F, NAME_FMT("CloudHistoryDistanceTexture{}", historySlot));
    }

    m_quadMesh = MeshBuilder::Quad();
    m_quadMesh->SetName(NAME("CloudCompositeQuad"));
    m_quadMesh->SetFlags(MeshFlags::ViewIndependent);
    m_quadMesh->SetIsTransient(true);
    m_quadMesh->UploadGpuData();
}

void CloudPass::Render(Frame* frame, const RenderSetup& renderSetup)
{
    HYP_SCOPE;
    AssertDebug(renderSetup.view != nullptr);

    m_hasRenderedThisFrame = false;

    const CloudResources& cloudResources = *RI.cloudResources;

    // the noise fills in over the first frames clouds are active; tracing against placeholders would flash
    if (!cloudResources.IsActive() || !cloudResources.IsNoiseReady())
    {
        return;
    }

    DeferredPassData* deferredPassData = DynamicCast<DeferredPassData>(renderSetup.passData);
    AssertDebug(deferredPassData != nullptr);

    RenderProxyList& rpl = GetConsumerProxyList(renderSetup.view);
    rpl.BeginRead();

    HYP_DEFER({ rpl.EndRead(); });

    if (!rpl.GetEffectVolumes().GetElements<CloudEffectVolume>().Any())
    {
        return;
    }

    RenderProxyCamera* cameraProxy = static_cast<RenderProxyCamera*>(GetRenderProxy(renderSetup.view->GetCamera()));

    if (!cameraProxy)
    {
        return;
    }

    const uint32 frameCounter = GetFrameCounter();
    const Vec2u traceOffset = TraceOffsetSequence[frameCounter % 4];

    const Vec3f cameraPosition = cameraProxy->bufferData.cameraPosition.GetXYZ();
    const Vec3f cameraDirection = cameraProxy->bufferData.cameraDirection.GetXYZ();

    // history is only reprojectable from the frame directly before, taken from a similar viewpoint
    const bool isCameraCut = cameraDirection.Dot(m_previousCameraDirection) < CameraCutMinDirectionDot
        || cameraPosition.Distance(m_previousCameraPosition) > CameraCutMaxMoveDistance;

    const bool isHistoryValid = m_lastRenderedFrame != ~0u
        && frameCounter == m_lastRenderedFrame + 1
        && !isCameraCut;

    ENGINE_STAT_GPU_SCOPE(&s_statCloudTrace);

    LightShaderData sunShaderData {};
    bool hasSun = false;

    for (Light* light : rpl.GetLights())
    {
        if (light->GetLightType() != LightType::Directional)
        {
            continue;
        }

        if (RenderProxyLight* lightProxy = static_cast<RenderProxyLight*>(GetRenderProxy(light)))
        {
            sunShaderData = lightProxy->bufferData;
            hasSun = true;

            break;
        }
    }

    EnvProbeShaderData skyProbeShaderData {};

    // haze blends toward the sky probe's cloud-free capture - the probe's convolved maps have the clouds themselves in them
    GpuImageViewRef skyTextureView = RI.placeholderData->GetImageViewCube1x1R8();

    const auto& skyProbes = rpl.GetEnvProbes().GetElements<SkyProbe>();

    if (skyProbes.Any())
    {
        SkyProbe* skyProbe = static_cast<SkyProbe*>(*skyProbes.Begin());

        if (RenderProxyEnvProbe* skyProbeProxy = static_cast<RenderProxyEnvProbe*>(GetRenderProxy(skyProbe)))
        {
            skyProbeShaderData = skyProbeProxy->bufferData;
        }

        const Handle<Texture>& skyboxTexture = skyProbe->GetSkyboxCubemap();

        if (skyboxTexture.IsValid() && skyboxTexture->IsCreated())
        {
            skyTextureView = RI.textureViewCache->GetOrCreate(skyboxTexture.Get());
        }
    }

    CloudTraceConstants traceConstants {};
    traceConstants.traceDimensions = m_traceExtent;
    traceConstants.traceSteps = MathUtil::Max(g_cvCloudsTraceSteps.Get(), 1u);
    traceConstants.lightSteps = g_cvCloudsLightSteps.Get();
    traceConstants.frameCounter = frameCounter;
    traceConstants.hasSun = hasSun ? 1u : 0u;
    traceConstants.traceOffset = traceOffset;
    traceConstants.historyDimensions = m_historyExtent;

    GpuBuffer* cbuffer = nullptr;
    size_t cbufferOffset = 0;
    size_t cbufferSize = 0;

    RI.cbufferAllocator->Write(&cameraProxy->bufferData);
    RI.cbufferAllocator->Write(&sunShaderData);
    RI.cbufferAllocator->Write(&skyProbeShaderData);
    cloudResources.WriteShaderData(*RI.cbufferAllocator, rpl);
    RI.cbufferAllocator->Write(&traceConstants);
    RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

    // Hi-Z mip 2: a texel covers the same 4x4 full resolution block as a trace texel
    const uint32 depthPyramidMip = MathUtil::Min(2u, deferredPassData->depthPyramidRenderer->GetTotalMips() - 1);

    CommandRecorder& cr = frame->cr;

    cr << InsertBarrier(m_traceTexture->GetGpuImage(), ResourceState::UnorderedAccess);
    cr << InsertBarrier(m_traceDistanceTexture->GetGpuImage(), ResourceState::UnorderedAccess);

    cr << SetCurrentShader(ShaderDesc(NAME("CloudTrace")));

    uint32 uniformIndex = 0;

    cr << SetShaderUniform(uniformIndex++, "OutCloudTexture"_sh, RI.textureViewCache->GetOrCreate(m_traceTexture.Get()));
    cr << SetShaderUniform(uniformIndex++, "OutCloudDistanceTexture"_sh, RI.textureViewCache->GetOrCreate(m_traceDistanceTexture.Get()));

    cr << SetShaderUniform(uniformIndex++, "CloudWeatherMapTexture"_sh, cloudResources.GetWeatherMapView());
    cr << SetShaderUniform(uniformIndex++, "CloudShapeNoiseTexture"_sh, cloudResources.GetShapeNoiseView());
    cr << SetShaderUniform(uniformIndex++, "CloudDetailNoiseTexture"_sh, cloudResources.GetDetailNoiseView());

    cr << SetShaderUniform(uniformIndex++, "DepthPyramidTexture"_sh, RI.textureViewCache->GetOrCreate(deferredPassData->depthPyramidRenderer->GetHZBTexture(), depthPyramidMip, 1));

    cr << SetShaderUniform(uniformIndex++, "SkyTexture"_sh, skyTextureView);
    cr << SetShaderUniform(uniformIndex++, "BlueNoiseBuffer"_sh, RI.blueNoiseBuffer);

    cr << SetShaderUniform(uniformIndex++, "SamplerLinear"_sh, RI.placeholderData->GetSamplerLinearMipmap());

    cr << SetShaderUniform(uniformIndex++, "CloudTraceConstants"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));

    cr << DispatchCompute(Vec3u { (m_traceExtent.x + 7) / 8, (m_traceExtent.y + 7) / 8, 1 });

    cr << InsertBarrier(m_traceTexture->GetGpuImage(), ResourceState::ShaderResource);
    cr << InsertBarrier(m_traceDistanceTexture->GetGpuImage(), ResourceState::ShaderResource);

    Reconstruct(frame, *cameraProxy, isHistoryValid, traceOffset);

    m_previousViewProjection = cameraProxy->bufferData.viewProjMat;
    m_previousCameraPosition = cameraPosition;
    m_previousCameraDirection = cameraDirection;

    m_lastRenderedFrame = frameCounter;
    m_hasRenderedThisFrame = true;
}

void CloudPass::Reconstruct(Frame* frame, const RenderProxyCamera& cameraProxy, bool isHistoryValid, const Vec2u& traceOffset)
{
    ENGINE_STAT_GPU_SCOPE(&s_statCloudReconstruct);

    const uint32 previousHistoryIndex = m_historyIndex;
    const uint32 nextHistoryIndex = m_historyIndex ^ 1u;

    CloudReconstructConstants reconstructConstants {};
    reconstructConstants.previousViewProjection = m_previousViewProjection;
    reconstructConstants.traceDimensions = m_traceExtent;
    reconstructConstants.historyDimensions = m_historyExtent;
    reconstructConstants.traceOffset = traceOffset;
    reconstructConstants.historyValid = isHistoryValid ? 1u : 0u;

    GpuBuffer* cbuffer = nullptr;
    size_t cbufferOffset = 0;
    size_t cbufferSize = 0;

    RI.cbufferAllocator->Write(&cameraProxy.bufferData);
    RI.cbufferAllocator->Write(&reconstructConstants);
    RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

    // clamped so reprojected lookups at the screen edge don't wrap to the opposite side
    Sampler* clampedLinearSampler = RI.samplerCache->GetOrCreate(SamplerDesc {
        TextureFilterMode::Linear,
        TextureFilterMode::Linear,
        TextureWrapMode::ClampToEdge
    });

    Texture* outHistoryTexture = m_historyTextures[nextHistoryIndex].Get();
    Texture* outHistoryDistanceTexture = m_historyDistanceTextures[nextHistoryIndex].Get();

    CommandRecorder& cr = frame->cr;

    cr << InsertBarrier(outHistoryTexture->GetGpuImage(), ResourceState::UnorderedAccess);
    cr << InsertBarrier(outHistoryDistanceTexture->GetGpuImage(), ResourceState::UnorderedAccess);

    cr << SetCurrentShader(ShaderDesc(NAME("CloudReconstruct")));

    uint32 uniformIndex = 0;

    cr << SetShaderUniform(uniformIndex++, "InTraceTexture"_sh, RI.textureViewCache->GetOrCreate(m_traceTexture.Get()));
    cr << SetShaderUniform(uniformIndex++, "InTraceDistanceTexture"_sh, RI.textureViewCache->GetOrCreate(m_traceDistanceTexture.Get()));

    cr << SetShaderUniform(uniformIndex++, "InHistoryTexture"_sh, RI.textureViewCache->GetOrCreate(m_historyTextures[previousHistoryIndex].Get()));
    cr << SetShaderUniform(uniformIndex++, "InHistoryDistanceTexture"_sh, RI.textureViewCache->GetOrCreate(m_historyDistanceTextures[previousHistoryIndex].Get()));

    cr << SetShaderUniform(uniformIndex++, "OutHistoryTexture"_sh, RI.textureViewCache->GetOrCreate(outHistoryTexture));
    cr << SetShaderUniform(uniformIndex++, "OutHistoryDistanceTexture"_sh, RI.textureViewCache->GetOrCreate(outHistoryDistanceTexture));

    cr << SetShaderUniform(uniformIndex++, "SamplerLinear"_sh, clampedLinearSampler);

    cr << SetShaderUniform(uniformIndex++, "CloudReconstructConstants"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));

    cr << DispatchCompute(Vec3u { (m_historyExtent.x + 7) / 8, (m_historyExtent.y + 7) / 8, 1 });

    cr << InsertBarrier(outHistoryTexture->GetGpuImage(), ResourceState::ShaderResource);
    cr << InsertBarrier(outHistoryDistanceTexture->GetGpuImage(), ResourceState::ShaderResource);

    m_historyIndex = nextHistoryIndex;
}

void CloudPass::Composite(Frame* frame, const RenderSetup& renderSetup)
{
    HYP_SCOPE;
    AssertDebug(renderSetup.framebuffer != nullptr);

    if (!m_hasRenderedThisFrame)
    {
        return;
    }

    ENGINE_STAT_GPU_SCOPE(&s_statCloudComposite);

    // clamped so the half resolution texture doesn't wrap its opposite edge into the screen border
    Sampler* compositeSampler = RI.samplerCache->GetOrCreate(SamplerDesc {
        TextureFilterMode::Linear,
        TextureFilterMode::Linear,
        TextureWrapMode::ClampToEdge
    });

    CommandRecorder& cr = frame->cr;

    cr << SetCurrentFramebuffer(renderSetup.framebuffer);
    cr << SetCurrentViewport(renderSetup.viewport);

    cr << SetInputLayout(StaticVertexInputLayout<VT_Simple>);
    cr << SetFaceCullMode(FaceCullMode::Back);
    cr << SetFillMode(FillMode::Fill);
    cr << SetTopology(Topology::Triangles);
    cr << SetDepthTest(false);
    cr << SetDepthWrite(false);

    // sky pixels only - geometry is never behind a ground level camera's clouds
    cr << SetStencilTest(true);
    cr << SetStencilFunction(StencilFunction { StencilOp::Keep, StencilOp::Keep, StencilOp::Keep, StencilCompareOp::Equal });
    cr << SetStencilState(SkyStencilMask, SkyStencilMask, 0x0);

    // premultiplied: color = cloud + sky * transmittance, destination alpha untouched
    cr << SetCurrentBlendFunction(BlendFunction(BlendModeFactor::One, BlendModeFactor::SrcAlpha, BlendModeFactor::Zero, BlendModeFactor::One));

    cr << SetCurrentShader(ShaderDesc(NAME("CloudComposite")));

    cr << SetShaderUniform(0, "SamplerLinear"_sh, compositeSampler);
    cr << SetShaderUniform(1, "InCloudTexture"_sh, RI.textureViewCache->GetOrCreate(m_historyTextures[m_historyIndex].Get()));

    cr << CommitDrawState();

    cr << BindVertexBuffer(m_quadMesh->GetVertexBuffer(0));
    cr << BindIndexBuffer(m_quadMesh->GetIndexBuffer(0));

    cr << DrawIndexed(6);

    cr << SetDepthTest(true);
    cr << SetDepthWrite(true);
    cr << SetStencilTest(false);
    cr << SetCurrentBlendFunction(BlendFunction::None());

    cr << SetCurrentFramebuffer(nullptr);
}

} // namespace Hyperion
