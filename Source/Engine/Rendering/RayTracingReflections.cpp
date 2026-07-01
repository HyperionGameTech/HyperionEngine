/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/PlaceholderData.hpp>
#include <Rendering/ShaderManager.hpp>
#include <Rendering/Frame.hpp>
#include <Rendering/GpuBuffer.hpp>
#include <Rendering/RenderResult.hpp>
#include <Rendering/RenderProxyList.hpp>
#include <Rendering/RenderProxy.hpp>
#include <Rendering/Texture.hpp>
#include <Rendering/RendererMain.hpp>
#include <Rendering/ShaderInstance.hpp>
#include <Rendering/TextureViewCache.hpp>
#include <Rendering/DescriptorSet.hpp>
#include <Rendering/RenderHelpers.hpp>
#include <Rendering/GBuffer.hpp>
#include <Rendering/CBufferAllocator.hpp>
#include <Rendering/AccelerationStructure.hpp>
#include <Rendering/RayTracingReflections.hpp>
#include <Rendering/DDGI.hpp>

#include <Rendering/Passes/DeferredPass.hpp>

#include <Rendering/Shadows/ShadowMapCache.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <Framework/CVarManager.hpp>

#include <Scene/View.hpp>
#include <Scene/Light.hpp>
#include <Scene/EnvProbe.hpp>
#include <Scene/ProbeVolume.hpp>

#include <Core/Utilities/DeferredScope.hpp>

namespace Hyperion {

static constexpr uint32 MaxLights = 4;

static const Name s_shaderNames[] = { NAME("RayTracedReflections"), NAME("PathTracer") };

extern CVar<bool> g_cvPathTracing;

namespace DeferredRendererHelpers {

// Defined in DeferredPass.cpp
void FillShadowMapData(
    ShadowMapData& outShadowMapData,
    const ShadowMap& inShadowMap,
    uint32 cascadeIndex,
    View* shadowMapViewDynamic,
    View* shadowMapViewStatic);

} // namespace DeferredRendererHelpers

RayTracingReflections::RayTracingReflections(GBuffer* gbuffer)
    : m_gbuffer(gbuffer)
{
}

RayTracingReflections::~RayTracingReflections()
{
    // remove result image from global descriptor set
    EnqueueDeletion(std::move(m_texture));
}

const GpuImageViewRef& RayTracingReflections::GetFinalImageView() const
{
    if (m_temporalBlending != nullptr)
    {
        return RI.textureViewCache->GetOrCreate(m_temporalBlending->GetResultTexture());
    }

    return RI.textureViewCache->GetOrCreate(m_texture);
}

void RayTracingReflections::Create()
{
    CreateImages();
}

void RayTracingReflections::Render(Frame* frame, const RenderSetup& renderSetup)
{
    HYP_NAMED_SCOPE("Ray traced reflections");

    AssertDebug(renderSetup.world && renderSetup.view);

    RayTracingPassData* pd = DynamicCast<RayTracingPassData>(renderSetup.passData);
    AssertDebug(pd != nullptr);

    DeferredPassData* parentPass = pd->parentPass;
    AssertDebug(parentPass != nullptr);

    const uint32 frameIndex = frame->GetFrameIndex();
    const TopLevelASRef& tlas = pd->rayTracingTlases[frameIndex];
    Assert(tlas != nullptr);

    const StructuredBuffer& meshDescriptionsBuffer = tlas->GetMeshDescriptionsBuffer();

    const bool isPathTracer = g_cvPathTracing.Get();
    InitTemporalBlending(isPathTracer);

    // Reset progressive blending if the camera view matrix has changed (for path tracing)
    RenderProxyCamera* cameraProxy = static_cast<RenderProxyCamera*>(GetRenderProxy(renderSetup.view->GetCamera()));
    Assert(cameraProxy != nullptr);

    CameraShaderData cameraData = cameraProxy->bufferData;

    if (isPathTracer)
    {
        if (cameraData.viewMat != m_previousViewMatrix)
        {
            RenderSetup newRenderSetup = renderSetup;
            newRenderSetup.passData = parentPass;

            m_temporalBlending->ResetProgressiveBlending();
            m_temporalBlending->Render(frame, newRenderSetup);

            m_previousViewMatrix = cameraData.viewMat;
        }
    }

    frame->cr << InsertBarrier(m_texture->GetGpuImage(), RS_UNORDERED_ACCESS);

    // Set shader and uniforms
    ShaderPropertySet shaderProperties;

    if (renderSetup.envProbe != nullptr)
    {
        shaderProperties.Add(InternShaderProperty(ShaderProperty(NAME("HAS_ENV_PROBE"))));
    }

    frame->cr << SetCurrentShader(ShaderDesc(s_shaderNames[isPathTracer ? 1 : 0], shaderProperties));

    AssertDebug(parentPass->view.IsValid());

    Framebuffer* viewFramebuffer = parentPass->view.GetUnsafe()->GetOutputTarget().GetFramebuffer(GBufferPass::Opaque);
    AssertDebug(viewFramebuffer != nullptr);

    GpuBuffer* cbuffer = nullptr;
    size_t cbufferOffset = 0;
    size_t cbufferSize = 0;

    { // Update constants
        RayTracingConstants rayTracingConstants {};
        rayTracingConstants.minRoughness = 0.4f;
        rayTracingConstants.outputImageResolution = Vec2i(m_texture->GetExtent().GetXY());

        Array<Pair<Light*, LightShaderData*>, RenderAllocator> tempLights;

        uint32& numBoundLights = rayTracingConstants.numBoundLights;

        RenderProxyList& rpl = GetConsumerProxyList(renderSetup.view);
        rpl.BeginRead();
        HYP_DEFER({ rpl.EndRead(); });

        for (Light* light : rpl.GetLights())
        {
            const LightType lightType = light->GetLightType();

            if (lightType != LightType::Directional && lightType != LightType::Point)
            {
                continue;
            }

            if (numBoundLights >= MaxLights)
            {
                break;
            }

            RenderProxyLight* lightProxy = static_cast<RenderProxyLight*>(GetRenderProxy(light));
            Assert(lightProxy != nullptr);

            tempLights.EmplaceBack(light, &lightProxy->bufferData);

            ++numBoundLights;
        }

        RI.cbufferAllocator->Write(&rayTracingConstants);

        // write camera
        RI.cbufferAllocator->Write(&cameraData);

        for (uint32 i = 0; i < MaxLights; i++)
        {
            if (i < uint32(tempLights.Size()))
            {
                RI.cbufferAllocator->Write(tempLights[i].second);
                continue;
            }

            LightShaderData dummy {};
            RI.cbufferAllocator->Write(&dummy);
        }

        for (uint32 i = 0; i < MaxLights; i++)
        {
            ShadowMapData shadowMapData {};

            if (i < uint32(tempLights.Size()))
            {
                View* shadowMapViewDynamic;
                View* shadowMapViewStatic;

                Light* light = tempLights[i].first;

                const uint32 cascadeIndex = 0;

                ShadowMap* shadowMap = RI.shadowMapCache->GetShadowMap(
                    light,
                    renderSetup.view,
                    cascadeIndex,
                    shadowMapViewDynamic,
                    shadowMapViewStatic);

                if (shadowMap != nullptr)
                {
                    DeferredRendererHelpers::FillShadowMapData(
                        shadowMapData,
                        *shadowMap,
                        cascadeIndex,
                        shadowMapViewDynamic,
                        shadowMapViewStatic);
                }
            }

            RI.cbufferAllocator->Write(&shadowMapData);
        }

        RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);
    }

    frame->cr << SetShaderUniform(0, "GBufferAlbedoTexture"_sh, viewFramebuffer->GetAttachment(0)->GetImageView());
    frame->cr << SetShaderUniform(1, "GBufferNormalsTexture"_sh, viewFramebuffer->GetAttachment(1)->GetImageView());
    frame->cr << SetShaderUniform(2, "GBufferMaterialTexture"_sh, viewFramebuffer->GetAttachment(2)->GetImageView());
    frame->cr << SetShaderUniform(3, "GBufferDepthTexture"_sh, viewFramebuffer->GetAttachment(viewFramebuffer->NumAttachments() - 1)->GetImageView());

    frame->cr << SetShaderUniform(4, "SamplerNearest"_sh, RI.placeholderData->GetSamplerNearest());
    frame->cr << SetShaderUniform(5, "SamplerLinear"_sh, RI.placeholderData->GetSamplerLinearMipmap());
    frame->cr << SetShaderUniform(6, "TLAS"_sh, tlas);
    frame->cr << SetShaderUniform(7, "MeshDescriptionsBuffer"_sh, meshDescriptionsBuffer);
    frame->cr << SetShaderUniform(8, "OutputImage"_sh, RI.textureViewCache->GetOrCreate(m_texture));
    frame->cr << SetShaderUniform(9, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));

    frame->cr << SetShaderUniform(11, "BlueNoiseBuffer"_sh, RI.blueNoiseBuffer);

    frame->cr << SetShaderUniform(12, "ShadowMapsTextureArray"_sh, RI.shadowMapCache->GetAtlasImageView());
    frame->cr << SetShaderUniform(13, "PointLightShadowMapsTextureArray"_sh, RI.shadowMapCache->GetPointLightShadowMapImageView());

    frame->cr << SetShaderUniform(14, "MaterialsBuffer"_sh, RI.namedBuffers[NamedBuffer::Materials]);
    frame->cr << SetShaderUniform(15, "WorldsBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);

    if (renderSetup.envProbe != nullptr)
    {
        frame->cr << SetShaderUniform(18, "EnvProbesColorTexture"_sh, RI.textureViewCache->GetOrCreate(RI.envProbesColorTexture));
        frame->cr << SetShaderUniform(19, "EnvProbesDepthTexture"_sh, RI.textureViewCache->GetOrCreate(RI.envProbesDepthTexture));
        frame->cr << SetShaderUniform(20, "CurrentEnvProbe"_sh, RI.namedBuffers[NamedBuffer::EnvProbes], Resources::GetBinding(renderSetup.envProbe));
    }

    const Vec3u imageExtent = m_texture->GetGpuImage()->GetExtent();
    const size_t numPixels = imageExtent.Volume();

    frame->cr << TraceRays(Vec3u { uint32(numPixels), 1, 1 });
    frame->cr << InsertBarrier(m_texture->GetGpuImage(), RS_SHADER_RESOURCE);

    // Create a new RenderSetup for temporal blending as it will need to bind View descriptors,
    // which we don't have on RayTracingPassData
    RenderSetup newRenderSetup = renderSetup;
    newRenderSetup.passData = parentPass;

    m_temporalBlending->Render(frame, newRenderSetup);
}

void RayTracingReflections::CreateImages()
{
    Assert(m_gbuffer != nullptr && m_gbuffer->GetExtent().Volume() != 0);

    m_texture = MakeHandle<Texture>(TextureDesc {
        TextureType::Texture2D,
        TextureFormat::RGBA8,
        Vec3u(m_gbuffer->GetExtent(), 1),
        TFM_NEAREST,
        TFM_NEAREST,
        TWM_CLAMP_TO_EDGE,
        1,
        IU_SAMPLED | IU_STORAGE });

    m_texture->SetName(NAME("RayTracingReflectionsTexture"));

    CheckResult(m_texture->Create());
}

void RayTracingReflections::InitTemporalBlending(bool isPathTracer)
{
    const TemporalBlendTechnique technique = isPathTracer
        ? TemporalBlendTechnique::TECHNIQUE_4 // progressive blending
        : TemporalBlendTechnique::TECHNIQUE_1;

    if (m_temporalBlending != nullptr && m_temporalBlending->GetTechnique() == technique)
    {
        // already created and technique is the same
        return;
    }

    m_temporalBlending = MakeUnique<TemporalBlending>(
        m_gbuffer->GetExtent(),
        TextureFormat::RGBA8,
        technique,
        DefaultTemporalBlendingFeedback,
        RI.textureViewCache->GetOrCreate(m_texture),
        m_gbuffer);

    m_temporalBlending->Create();
}

} // namespace Hyperion
