/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <RenderingPch.hpp>

#include <rendering/RenderInterface.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/ShaderManager.hpp>
#include <rendering/Frame.hpp>
#include <rendering/GpuBuffer.hpp>
#include <rendering/RenderResult.hpp>
#include <rendering/RenderProxyList.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/Texture.hpp>
#include <rendering/RenderCollection.hpp>
#include <rendering/ShaderInstance.hpp>
#include <rendering/TextureViewCache.hpp>
#include <rendering/DescriptorSet.hpp>
#include <rendering/RenderHelpers.hpp>
#include <rendering/CBufferAllocator.hpp>

#include <rendering/renderers/DeferredRenderer.hpp>

#include <rendering/AccelerationStructure.hpp>
#include <rendering/RayTracingReflections.hpp>
#include <rendering/DDGI.hpp>

#include <rendering/shadows/ShadowMapCache.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <engine/CVarManager.hpp>

#include <scene/View.hpp>
#include <scene/Light.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/EnvGrid.hpp>

#include <Core/utilities/DeferredScope.hpp>

#include <RayTracingReflections.generated.inl>

namespace Hyperion {

static const Name s_shaderNames[] = { NAME("RayTracedReflections"), NAME("PathTracer") };
static constexpr uint32 MaxLights = 4;

extern CVar<bool> cvPathTracing;

namespace DeferredRendererHelpers {

// Defined in DeferredRenderer.cpp
void FillShadowMapData(
    ShadowMapData& outShadowMapData,
    const ShadowMap& inShadowMap,
    View* shadowMapViewDynamic,
    View* shadowMapViewStatic);

} // namespace DeferredRendererHelpers

RayTracingReflections::RayTracingReflections(RayTracingReflectionsConfig&& config, GBuffer* gbuffer)
    : m_config(std::move(config)),
      m_gbuffer(gbuffer)
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
        return g_renderInterface->textureViewCache->GetOrCreate(m_temporalBlending->GetResultTexture());
    }

    return g_renderInterface->textureViewCache->GetOrCreate(m_texture);
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

    DeferredRendererPassData* parentPass = pd->parentPass;
    AssertDebug(parentPass != nullptr);

    const uint32 frameIndex = frame->GetFrameIndex();
    const GpuTlasRef& tlas = pd->rayTracingTlases[frameIndex];
    Assert(tlas != nullptr);

    const GpuBufferRef& meshDescriptionsBuffer = tlas->GetMeshDescriptionsBuffer();
    Assert(meshDescriptionsBuffer != nullptr && meshDescriptionsBuffer->IsCreated());

    const bool isPathTracer = cvPathTracing.Get();
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
    shaderProperties.Add(InternShaderProperty(ShaderProperty(NAME("MAX_LIGHTS"), int(MaxLights))));

    if (renderSetup.envProbe != nullptr)
        shaderProperties.Add(InternShaderProperty(ShaderProperty(NAME("HAS_ENV_PROBE"))));

    frame->cr << SetCurrentShader(ShaderDesc(s_shaderNames[isPathTracer ? 1 : 0], shaderProperties));

    AssertDebug(parentPass->view.IsValid());

    Framebuffer* viewFramebuffer = parentPass->view.GetUnsafe()->GetOutputTarget().GetFramebuffer(RenderBucket::Opaque);
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

        g_renderInterface->cbufferAllocator->Write(&rayTracingConstants);

        // write camera
        g_renderInterface->cbufferAllocator->Write(&cameraData);

        for (uint32 i = 0; i < MaxLights; i++)
        {
            if (i < uint32(tempLights.Size()))
            {
                g_renderInterface->cbufferAllocator->Write(tempLights[i].second);
                continue;
            }
        
            LightShaderData dummy {};
            g_renderInterface->cbufferAllocator->Write(&dummy);
        }

        for (uint32 i = 0; i < MaxLights; i++)
        {
            ShadowMapData shadowMapData {};

            if (i < uint32(tempLights.Size()))
            {
                View* shadowMapViewDynamic;
                View* shadowMapViewStatic;

                Light* light = tempLights[i].first;

                ShadowMap* shadowMap = g_renderInterface->shadowMapCache->GetShadowMap(
                    light,
                    renderSetup.view,
                    /* cascadeIndex */ 0,
                    shadowMapViewDynamic,
                    shadowMapViewStatic);

                if (shadowMap != nullptr)
                {
                    DeferredRendererHelpers::FillShadowMapData(
                        shadowMapData,
                        *shadowMap,
                        shadowMapViewDynamic,
                        shadowMapViewStatic);
                }
            }

            g_renderInterface->cbufferAllocator->Write(&shadowMapData);
        }
            
        g_renderInterface->cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);
    }
    
    frame->cr << SetShaderUniform(0, "GBufferAlbedoTexture"_sh, viewFramebuffer->GetAttachment(0)->GetImageView());
    frame->cr << SetShaderUniform(1, "GBufferNormalsTexture"_sh, viewFramebuffer->GetAttachment(1)->GetImageView());
    frame->cr << SetShaderUniform(2, "GBufferMaterialTexture"_sh, viewFramebuffer->GetAttachment(2)->GetImageView());
    frame->cr << SetShaderUniform(3, "GBufferDepthTexture"_sh, viewFramebuffer->GetAttachment(viewFramebuffer->NumAttachments() - 1)->GetImageView());

    frame->cr << SetShaderUniform(4, "SamplerNearest"_sh, g_renderInterface->placeholderData->GetSamplerNearest());
    frame->cr << SetShaderUniform(5, "SamplerLinear"_sh, g_renderInterface->placeholderData->GetSamplerLinearMipmap());
    frame->cr << SetShaderUniform(6, "TLAS"_sh, tlas);
    frame->cr << SetShaderUniform(7, "MeshDescriptionsBuffer"_sh, meshDescriptionsBuffer);
    frame->cr << SetShaderUniform(8, "OutputImage"_sh, g_renderInterface->textureViewCache->GetOrCreate(m_texture));
    frame->cr << SetShaderUniform(9, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));

    frame->cr << SetShaderUniform(11, "BlueNoiseBuffer"_sh, g_renderInterface->blueNoiseBuffer);

    frame->cr << SetShaderUniform(12, "ShadowMapsTextureArray"_sh, g_renderInterface->shadowMapCache->GetAtlasImageView());
    frame->cr << SetShaderUniform(13, "PointLightShadowMapsTextureArray"_sh, g_renderInterface->shadowMapCache->GetPointLightShadowMapImageView());

    frame->cr << SetShaderUniform(14, "MaterialsBuffer"_sh, g_renderInterface->namedBuffers[NamedBuffer::Materials].gpuBuffer);
    frame->cr << SetShaderUniform(15, "WorldsBuffer"_sh, g_renderInterface->namedBuffers[NamedBuffer::Worlds].gpuBuffer);

    if (renderSetup.envProbe != nullptr)
    {
        frame->cr << SetShaderUniform(18, "EnvProbesTexture"_sh, g_renderInterface->textureViewCache->GetOrCreate(g_renderInterface->envProbesTexture));
        frame->cr << SetShaderUniform(19, "CurrentEnvProbe"_sh, g_renderInterface->namedBuffers[NamedBuffer::EnvProbes].gpuBuffer, TShaderDataOffset<EnvProbeShaderData>(renderSetup.envProbe));
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
    Assert(m_config.extent.Volume() != 0);

    m_texture = MakeHandle<Texture>(TextureDesc {
        TextureType::Texture2D,
        TextureFormat::RGBA8,
        Vec3u { m_config.extent, 1 },
        TFM_NEAREST,
        TFM_NEAREST,
        TWM_CLAMP_TO_EDGE,
        1,
        IU_SAMPLED | IU_STORAGE
    });

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
        m_config.extent,
        TextureFormat::RGBA8,
        technique,
        DefaultTemporalBlendingFeedback,
        g_renderInterface->textureViewCache->GetOrCreate(m_texture),
        m_gbuffer);

    m_temporalBlending->Create();
}

} // namespace Hyperion
