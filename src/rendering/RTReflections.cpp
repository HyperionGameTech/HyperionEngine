/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

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
#include <rendering/Shader.hpp>
#include <rendering/TextureViewCache.hpp>
#include <rendering/DescriptorSet.hpp>
#include <rendering/RenderHelpers.hpp>

#include <rendering/renderers/DeferredRenderer.hpp>

#include <rendering/AccelerationStructure.hpp>
#include <rendering/RTReflections.hpp>
#include <rendering/DDGI.hpp>

#include <rendering/shadows/ShadowMapAllocator.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <scene/View.hpp>
#include <scene/Light.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/EnvGrid.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <RTReflections.generated.inl>

namespace Hyperion {

static const Name s_shaderNames[] = { NAME("RayTracedReflections"), NAME("PathTracer") };
static constexpr uint32 MaxLights = sizeof(RayTracingConstants::lightIndices) / sizeof(uint32);

RayTracingReflections::RayTracingReflections(RayTracingReflectionsConfig&& config, GBuffer* gbuffer)
    : m_config(std::move(config)),
      m_gbuffer(gbuffer)
{
}

RayTracingReflections::~RayTracingReflections()
{
    // remove result image from global descriptor set
    SafeDelete(std::move(m_texture));
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
    CreateTemporalBlending();
}

void RayTracingReflections::UpdateUniforms(Frame* frame, const RenderSetup& renderSetup)
{
    RayTracingPassData* pd = ObjCast<RayTracingPassData>(renderSetup.passData);
    Assert(pd != nullptr);
    
    GpuBufferRef& cBuffer = pd->cBuffer;
    if (!cBuffer)
    {
        cBuffer = g_renderInterface->MakeGpuBuffer(GpuBufferType::CONSTANT_BUFFER, sizeof(RayTracingConstants));
        cBuffer->SetDebugName(NAME("RayTracingCBuffer"));
        Assert(cBuffer->Create());
    }

    GpuBufferRef& lightsBuffer = pd->lightsBuffer;
    if (!lightsBuffer)
    {
        lightsBuffer = g_renderInterface->MakeGpuBuffer(GpuBufferType::CONSTANT_BUFFER, sizeof(LightShaderData) * MaxLights);
        lightsBuffer->SetRequireCpuAccessible(true);
        lightsBuffer->SetDebugName(NAME("RayTracingLightsBuffer"));
        Assert(lightsBuffer->Create());
    }

    struct UpdateRayTracingBuffers
    {
        Vec2i extent;
        View* view = nullptr;
        RayTracingPassData* passData = nullptr;

        void operator()(Frame*)
        {
            AssertDebug(view && passData);

            RenderProxyList& rpl = GetConsumerProxyList(view);
            rpl.BeginRead();
            HYP_DEFER({ rpl.EndRead(); });
            
            GpuBufferRef& lightsBuffer = passData->lightsBuffer;
            AssertDebug(lightsBuffer != nullptr);

            GpuBufferRef& cBuffer = passData->cBuffer;
            AssertDebug(cBuffer != nullptr);

            RayTracingConstants constantData {};
            constantData.minRoughness = 0.4f;
            constantData.outputImageResolution = extent;

            uint32 numBoundLights = 0;
    
            uint32* lightIndicesU32 = reinterpret_cast<uint32*>(constantData.lightIndices);
            Memory::Fill(lightIndicesU32, 0, sizeof(constantData.lightIndices));

            for (Light* light : rpl.GetLights())
            {
                const LightType lightType = light->GetLightType();

                if (lightType != LT_DIRECTIONAL && lightType != LT_POINT)
                {
                    continue;
                }

                if (numBoundLights >= MaxLights)
                {
                    break;
                }

                RenderProxyLight* lightProxy = static_cast<RenderProxyLight*>(GetRenderProxy(light));
                Assert(lightProxy != nullptr);
                
                lightsBuffer->Copy(numBoundLights * sizeof(LightShaderData), sizeof(LightShaderData), &lightProxy->bufferData);

                lightIndicesU32[numBoundLights++] = RetrieveResourceBinding(light);
            }

            constantData.numBoundLights = numBoundLights;

            cBuffer->Copy(sizeof(RayTracingConstants), &constantData);
        }
    };

    g_renderInterface->GetCurrentFrame()->OnFrameEnd.Bind(UpdateRayTracingBuffers {
        Vec2i(m_config.extent),
        renderSetup.view,
        pd
    }).Detach();
}

void RayTracingReflections::Render(Frame* frame, const RenderSetup& renderSetup)
{
    HYP_NAMED_SCOPE("Ray traced reflections");

    AssertDebug(renderSetup.world && renderSetup.view);

    RayTracingPassData* pd = ObjCast<RayTracingPassData>(renderSetup.passData);
    AssertDebug(pd != nullptr);

    DeferredRendererPassData* parentPass = pd->parentPass;
    AssertDebug(parentPass != nullptr);
    
    UpdateUniforms(frame, renderSetup);

    const uint32 frameIndex = frame->GetFrameIndex();
    const GpuTlasRef& tlas = pd->rayTracingTlases[frameIndex];
    Assert(tlas != nullptr);

    const GpuBufferRef& meshDescriptionsBuffer = tlas->GetMeshDescriptionsBuffer();
    Assert(meshDescriptionsBuffer != nullptr && meshDescriptionsBuffer->IsCreated());

    // Reset progressive blending if the camera view matrix has changed (for path tracing)
    if (IsPathTracer())
    {
        RenderProxyCamera* cameraProxy = static_cast<RenderProxyCamera*>(GetRenderProxy(renderSetup.view->GetCamera()));
        Assert(cameraProxy != nullptr);

        if (cameraProxy->bufferData.viewMat != m_previousViewMatrix)
        {
            RenderSetup newRenderSetup = renderSetup;
            newRenderSetup.passData = parentPass;

            m_temporalBlending->ResetProgressiveBlending();
            m_temporalBlending->Render(frame, newRenderSetup);

            m_previousViewMatrix = cameraProxy->bufferData.viewMat;
        }
    }

    frame->renderQueue << InsertBarrier(m_texture->GetGpuImage(), RS_UNORDERED_ACCESS);

    // Set shader and uniforms
    ShaderPropertySet shaderProperties;
    shaderProperties.Add(InternShaderProperty(ShaderProperty(NAME("MAX_LIGHTS"), int(MaxLights))));

    if (renderSetup.envProbe != nullptr)
        shaderProperties.Add(InternShaderProperty(ShaderProperty(NAME("HAS_ENV_PROBE"))));

    frame->renderQueue << SetCurrentShader(ShaderDesc(s_shaderNames[IsPathTracer()], shaderProperties));

    AssertDebug(parentPass->view.IsValid());

    Framebuffer* viewFramebuffer = parentPass->view.GetUnsafe()->GetOutputTarget().GetFramebuffer(RB_OPAQUE);
    AssertDebug(viewFramebuffer != nullptr);

    AssertDebug(pd->cBuffer != nullptr);
    
    frame->renderQueue << SetShaderUniform(0, "GBufferAlbedoTexture"_sh, viewFramebuffer->GetAttachment(0)->GetImageView());
    frame->renderQueue << SetShaderUniform(1, "GBufferNormalsTexture"_sh, viewFramebuffer->GetAttachment(1)->GetImageView());
    frame->renderQueue << SetShaderUniform(2, "GBufferMaterialTexture"_sh, viewFramebuffer->GetAttachment(2)->GetImageView());
    frame->renderQueue << SetShaderUniform(3, "GBufferDepthTexture"_sh, viewFramebuffer->GetAttachment(viewFramebuffer->NumAttachments() - 1)->GetImageView());

    frame->renderQueue << SetShaderUniform(4, "SamplerNearest"_sh, g_renderInterface->placeholderData->GetSamplerNearest());
    frame->renderQueue << SetShaderUniform(5, "SamplerLinear"_sh, g_renderInterface->placeholderData->GetSamplerLinearMipmap());
    frame->renderQueue << SetShaderUniform(6, "TLAS"_sh, tlas);
    frame->renderQueue << SetShaderUniform(7, "MeshDescriptionsBuffer"_sh, meshDescriptionsBuffer);
    frame->renderQueue << SetShaderUniform(8, "OutputImage"_sh, g_renderInterface->textureViewCache->GetOrCreate(m_texture));
    frame->renderQueue << SetShaderUniform(9, "RayTracingConstants"_sh, pd->cBuffer);
    frame->renderQueue << SetShaderUniform(10, "Lights"_sh, pd->lightsBuffer);
    frame->renderQueue << SetShaderUniform(11, "BlueNoiseBuffer"_sh, g_renderInterface->blueNoiseBuffer);

    frame->renderQueue << SetShaderUniform(12, "ShadowMapsTextureArray"_sh, g_renderInterface->shadowMapAllocator->GetAtlasImageView());
    frame->renderQueue << SetShaderUniform(13, "PointLightShadowMapsTextureArray"_sh, g_renderInterface->shadowMapAllocator->GetPointLightShadowMapImageView());

    frame->renderQueue << SetShaderUniform(14, "MaterialsBuffer"_sh, g_renderInterface->gpuBuffers[GRB_MATERIALS]->GetBuffer(frameIndex));
    frame->renderQueue << SetShaderUniform(15, "EntitiesBuffer"_sh, g_renderInterface->gpuBuffers[GRB_ENTITIES]->GetBuffer(frameIndex));
    frame->renderQueue << SetShaderUniform(16, "WorldsBuffer"_sh, g_renderInterface->gpuBuffers[GRB_WORLDS]->GetBuffer(frameIndex));
    frame->renderQueue << SetShaderUniform(17, "CamerasBuffer"_sh, g_renderInterface->gpuBuffers[GRB_CAMERAS]->GetBuffer(frameIndex), TShaderDataOffset<CameraShaderData>(parentPass->view.GetUnsafe()->GetCamera()));

    if (renderSetup.envProbe != nullptr)
        frame->renderQueue << SetShaderUniform(18, "CurrentEnvProbe"_sh, g_renderInterface->gpuBuffers[GRB_ENV_PROBES]->GetBuffer(frameIndex), TShaderDataOffset<EnvProbeShaderData>(renderSetup.envProbe));

    const Vec3u imageExtent = m_texture->GetGpuImage()->GetExtent();
    const SizeType numPixels = imageExtent.Volume();

    frame->renderQueue << TraceRays(Vec3u { uint32(numPixels), 1, 1 });
    frame->renderQueue << InsertBarrier(m_texture->GetGpuImage(), RS_SHADER_RESOURCE);

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

    InitObject(m_texture);
}

void RayTracingReflections::CreateTemporalBlending()
{
    m_temporalBlending = MakeUnique<TemporalBlending>(
        m_config.extent,
        TextureFormat::RGBA8,
        IsPathTracer()
            ? TemporalBlendTechnique::TECHNIQUE_4 // progressive blending
            : TemporalBlendTechnique::TECHNIQUE_1,
        DefaultTemporalBlendingFeedback,
        g_renderInterface->textureViewCache->GetOrCreate(m_texture),
        m_gbuffer);

    m_temporalBlending->Create();
}

} // namespace Hyperion
