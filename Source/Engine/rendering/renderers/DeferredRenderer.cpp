/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/renderers/DeferredRenderer.hpp>
#include <rendering/renderers/EnvProbeRenderer.hpp>

#include <rendering/RenderGroup.hpp>
#include <rendering/MaterialTextureCache.hpp>
#include <rendering/ShaderManager.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/DepthPyramidRenderer.hpp>
#include <rendering/RenderInterface.hpp>
#include <rendering/GraphicsPipelineCache.hpp>
#include <rendering/SSRRenderer.hpp>
#include <rendering/SSGI.hpp>
#include <rendering/HbaoPass.hpp>
#include <rendering/DepthOfField.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/Material.hpp>
#include <rendering/Texture.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/RenderObject.hpp>
#include <rendering/GpuBuffer.hpp>
#include <rendering/Device.hpp>
#include <rendering/DescriptorSet.hpp>
#include <rendering/GraphicsPipeline.hpp>
#include <rendering/Swapchain.hpp>
#include <rendering/RenderCollection.hpp>
#include <rendering/RenderProxyList.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/shadows/ShadowMapAllocator.hpp>
#include <rendering/ConstantsAllocator.hpp>
#include <rendering/TextureViewCache.hpp>

#include <rendering/AccelerationStructure.hpp>
#include <rendering/RayTracingPipeline.hpp>
#include <rendering/MeshBlasBuilder.hpp>
#include <rendering/RTReflections.hpp>
#include <rendering/DDGI.hpp>

#include <rendering/util/ShaderCompiler.hpp>
#include <rendering/util/DeletionQueue.hpp>

#include <rendering/DebugDrawer.hpp>

#include <scene/World.hpp>
#include <scene/View.hpp>
#include <scene/EnvGrid.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/FogVolume.hpp>
#include <scene/ParticleVolume.hpp>
#include <scene/LightmapVolume.hpp>

#include <Core/config/Config.hpp>

#include <Core/filesystem/FsUtil.hpp>

#include <Core/utilities/DeferredScope.hpp>
#include <Core/utilities/Float16.hpp>

#include <system/AppContext.hpp>

#include <util/MeshBuilder.hpp>

#include <engine/EngineDriver.hpp>
#include <engine/EngineStats.hpp>

#include <DeferredRenderer.generated.inl>

namespace Hyperion {

static constexpr float CameraJitterScale = 0.25f;

static const Float16 s_ltcMatrix[] = {
#include <rendering/inl/LTCMatrix.inl>
};

static_assert(sizeof(s_ltcMatrix) == 64 * 64 * 4 * 2, "Invalid LTC matrix size");

static const Float16 s_ltcBrdf[] = {
#include <rendering/inl/LTCBRDF.inl>
};

static_assert(sizeof(s_ltcBrdf) == 64 * 64 * 4 * 2, "Invalid LTC BRDF size");

// Maps individual light types to per-light specific properties.
static const FixedArray<ShaderPropertySet, LT_MAX> s_deferredLightTypeProperties {
    ShaderPropertySet { { InternShaderProperty(ShaderProperty(NAME("LIGHT_TYPE"), NAME("DIRECTIONAL"))) } },
    ShaderPropertySet { { InternShaderProperty(ShaderProperty(NAME("LIGHT_TYPE"), NAME("POINT"))) } },
    ShaderPropertySet { { InternShaderProperty(ShaderProperty(NAME("LIGHT_TYPE"), NAME("SPOT"))) } },
    ShaderPropertySet { { InternShaderProperty(ShaderProperty(NAME("LIGHT_TYPE"), NAME("AREA_RECT"))) } }
};

static const ShaderPropertyId s_propHasDiffuseMap = InternShaderProperty(ShaderProperty(NAME("HAS_DIFFUSE_MAP")));

static constexpr StringHash GBufferTextureNames[GTN_MAX] = {
    "GBufferAlbedoTexture"_sh,
    "GBufferNormalsTexture"_sh,
    "GBufferMaterialTexture"_sh,
    "GBufferVelocityTexture"_sh,
    "GBufferDepthTexture"_sh
};

static EngineStatTimer s_deferredPassTimer("Rendering/Deferred/DeferredPass");
static EngineStatTimer s_deferredDirectLightingTimer("Rendering/Deferred/DirectLighting");
static EngineStatTimer s_deferredIndirectLightingTimer("Rendering/Deferred/IndirectLighting");

// Global stat counter instances
EngineStatCounter<uint32> g_statDrawCalls("Rendering/DrawCalls");
EngineStatCounter<uint32> g_statInstancedDrawCalls("Rendering/InstancedDrawCalls");
EngineStatCounter<uint32> g_statTriangles("Rendering/Triangles");
EngineStatCounter<uint32> g_statRenderGroups("Rendering/RenderGroups");
EngineStatCounter<uint32> g_statViews("Rendering/Views");
EngineStatCounter<uint32> g_statTextures("Rendering/Textures");
EngineStatCounter<uint32> g_statMaterials("Rendering/Materials");
EngineStatCounter<uint32> g_statLights("Rendering/Lights");
EngineStatCounter<uint32> g_statLightmapVolumes("Rendering/LightmapVolumes");
EngineStatCounter<uint32> g_statParticleVolumes("Rendering/ParticleVolumes");
EngineStatCounter<uint32> g_statEnvProbes("Rendering/EnvProbes");
EngineStatCounter<uint32> g_statEnvGrids("Rendering/EnvGrids");
EngineStatCounter<uint32> g_statDebugDraws("Rendering/DebugDraws");

namespace CoreApi {
extern const GlobalConfig& GetGlobalConfig();
} // namespace CoreApi

static void GetDeferredShaderProperties(
    DeferredPassMode mode,
    ShaderPropertySet& outShaderProperties,
    const RenderProxyList* rpl = nullptr,
    LightType lightType = LT_INVALID)
{
    static const GlobalConfig& s_globalConfig = CoreApi::GetGlobalConfig();
    static const IRenderConfig& s_renderConfig = g_renderInterface->GetRenderConfig();

    MergeGlobalShaderProperties(outShaderProperties);

#define DEF_STATIC_CONFIGURATION_VALUE(name, path)                        \
    static const ConfigurationValue& s_##name = s_globalConfig.Get(path); \
    const bool name = s_##name.ToBool()

    DEF_STATIC_CONFIGURATION_VALUE(rayTracingReflections, "Rendering.RayTracing.Reflections.Enabled");
    DEF_STATIC_CONFIGURATION_VALUE(rayTracingGlobalIllumination, "Rendering.RayTracing.GI.Enabled");
    DEF_STATIC_CONFIGURATION_VALUE(hbil, "Rendering.HBIL.Enabled");
    DEF_STATIC_CONFIGURATION_VALUE(hbao, "Rendering.HBAO.Enabled");
    DEF_STATIC_CONFIGURATION_VALUE(ssgi, "Rendering.SSGI.Enabled");
    DEF_STATIC_CONFIGURATION_VALUE(pathTracing, "Rendering.RayTracing.PathTracing.Enabled");

    DEF_STATIC_CONFIGURATION_VALUE(debugReflections, "Rendering.Debug.Reflections");
    DEF_STATIC_CONFIGURATION_VALUE(debugIrradiance, "Rendering.Debug.Irradiance");

#undef DEF_STATIC_CONFIGURATION_VALUE

    static const ShaderPropertyId s_propHBAOEnabled = InternShaderProperty(ShaderProperty(NAME("HBAO_ENABLED")));
    static const ShaderPropertyId s_propHBILEnabled = InternShaderProperty(ShaderProperty(NAME("HBIL_ENABLED")));
    static const ShaderPropertyId s_propSSGIEnabled = InternShaderProperty(ShaderProperty(NAME("SSGI_ENABLED")));

    static const ShaderPropertyId s_propRayTracingReflections = InternShaderProperty(ShaderProperty(NAME("RT_REFLECTIONS")));
    static const ShaderPropertyId s_propRayTracingGlobalIllumination = InternShaderProperty(ShaderProperty(NAME("RT_GI")));
    static const ShaderPropertyId s_propPathTracer = InternShaderProperty(ShaderProperty(NAME("PATHTRACER")));

    static const ShaderPropertyId s_propDebugReflections = InternShaderProperty(ShaderProperty(NAME("DEBUG_REFLECTIONS")));
    static const ShaderPropertyId s_propDebugIrradiance = InternShaderProperty(ShaderProperty(NAME("DEBUG_IRRADIANCE")));

    outShaderProperties.Set(s_propHBAOEnabled, hbao);

    if (mode == DPM_INDIRECT_LIGHTING)
    {
        outShaderProperties.Set(s_propRayTracingReflections, s_renderConfig.rayTracing && rayTracingReflections);
        outShaderProperties.Set(s_propRayTracingGlobalIllumination, s_renderConfig.rayTracing && rayTracingGlobalIllumination);

        outShaderProperties.Set(s_propHBILEnabled, hbil);
        outShaderProperties.Set(s_propSSGIEnabled, ssgi);
    }

    if (s_renderConfig.rayTracing && pathTracing)
    {
        outShaderProperties.Add(s_propPathTracer);
    }
    else if (debugReflections)
    {
        outShaderProperties.Add(s_propDebugReflections);
    }
    else if (debugIrradiance)
    {
        outShaderProperties.Add(s_propDebugIrradiance);
    }

    if (lightType != LT_INVALID)
    {
        outShaderProperties = outShaderProperties | s_deferredLightTypeProperties[uint32(lightType)];
    }
}

static const TypeId s_envProbeTypeToTypeId[EPT_MAX] = {
    TypeId::ForType<SkyProbe>(),        // EPT_SKY
    TypeId::ForType<ReflectionProbe>(), // EPT_REFLECTION
    TypeId::ForType<EnvProbe>()         // EPT_AMBIENT (fixme when derived class)
};

#pragma region DeferredPass

DeferredPass::DeferredPass(DeferredPassMode mode, Vec2u extent, GBuffer* gbuffer, const FramebufferRef& framebuffer)
    : FullScreenPass(ShaderDesc(), framebuffer, TextureFormat::RGBA16F, extent, gbuffer, FSP_EXTERNAL_RENDERTARGET),
      m_mode(mode)
{
    Assert(m_framebuffer.IsValid());

    if (mode == DPM_DIRECT_LIGHTING)
    {
        SetBlendFunction(BlendFunction::Additive());
    }
}

DeferredPass::~DeferredPass()
{
    EnqueueDeletion(std::move(m_ltcSampler));
}

void DeferredPass::Create()
{
    AssertOnThread(g_renderThread);

    FullScreenPass::Create();

    // linear transform cosines texture data
    if (m_mode == DPM_DIRECT_LIGHTING && !m_ltcSampler)
    {
        m_ltcSampler = g_renderInterface->MakeSampler(
            TFM_NEAREST,
            TFM_LINEAR,
            TWM_CLAMP_TO_EDGE);

        Assert(m_ltcSampler->Create());

        ByteBuffer ltcMatrixData(sizeof(s_ltcMatrix), s_ltcMatrix);

        m_ltcMatrixTexture = MakeHandle<Texture>(
            TextureDesc {
                TextureType::Texture2D,
                TextureFormat::RGBA16F,
                Vec3u { 64, 64, 1 },
                TFM_LINEAR,
                TFM_LINEAR,
                TWM_CLAMP_TO_EDGE
            },
            ltcMatrixData.ToByteView());

        m_ltcMatrixTexture->SetName(NAME("LTC_Matrix"));
        InitObject(m_ltcMatrixTexture);

        ByteBuffer ltcBrdfData(sizeof(s_ltcBrdf), s_ltcBrdf);

        m_ltcBrdfTexture = MakeHandle<Texture>(
            TextureDesc {
                TextureType::Texture2D,
                TextureFormat::RGBA16F,
                Vec3u { 64, 64, 1 },
                TFM_LINEAR,
                TFM_LINEAR,
                TWM_CLAMP_TO_EDGE
            },
            ltcBrdfData.ToByteView());

        m_ltcBrdfTexture->SetName(NAME("LTC_BRDF"));
        InitObject(m_ltcBrdfTexture);
    }
}

void DeferredPass::Resize_Internal(Vec2u newSize)
{
    FullScreenPass::Resize_Internal(newSize);
}

void DeferredPass::RenderToFramebuffer_Internal(Frame* frame, const RenderSetup& rs, Framebuffer* framebuffer)
{
    HYP_SCOPE;
    ENGINE_STAT_SCOPE(&s_deferredPassTimer);

    AssertDebug(rs.world && rs.view);
    AssertDebug(rs.passData != nullptr);

    ENGINE_STAT_SCOPE(
        m_mode == DPM_DIRECT_LIGHTING
            ? &s_deferredDirectLightingTimer
            : &s_deferredIndirectLightingTimer);

    const uint32 frameIndex = frame->GetFrameIndex();

    const Viewport& viewport = rs.view->GetViewport();

    RenderProxyList& rpl = GetConsumerProxyList(rs.view);
    rpl.BeginRead();
    HYP_DEFER({ rpl.EndRead(); });

    if (m_mode == DPM_DIRECT_LIGHTING && rpl.GetLights().NumCurrent() == 0)
    {
        return; // nothing to do for direct pass if no lights active
    }

    RenderQueue& rq = frame->renderQueue;

    rq << SetCurrentView(
        rs.view->GetOutputTarget().GetFramebuffer()->GetRenderTargetDesc(),
        rs.view->GetViewport());

    rq << SetVertexAttributes(VertexAttribute::Position | VertexAttribute::Normal | VertexAttribute::TexCoord0);
    rq << SetTopology(TOP_TRIANGLES);
    rq << SetFillMode(FM_FILL);
    rq << SetCurrentBlendFunction(m_blendFunction);
    rq << SetStencilTest(true);
    rq << SetDepthWrite(false);
    rq << SetDepthTest(false);
    rq << SetStencilFunction(StencilFunction {
        .passOp = SO_KEEP,
        .failOp = SO_KEEP,
        .depthFailOp = SO_KEEP,
        .compareOp = SCO_EQUAL });

    // stencil state: only render where stencil == 0 (non-lightmapped geometry)
    rq << SetStencilState(0, LightmapStencilMask, 0x0);

    HYP_DEFER({
        // reset states
        rq << SetCurrentBlendFunction(BlendFunction::None());
        rq << SetStencilState(0, 0xFF, 0x0);
        rq << SetDepthWrite(true);
        rq << SetDepthTest(true);
        rq << SetStencilTest(false);
    });

    uint32 numShaderUniforms = 0;

    rq << SetShaderUniform(numShaderUniforms++, "SamplerLinear"_sh, g_renderInterface->placeholderData->GetSamplerLinearMipmap());
    rq << SetShaderUniform(numShaderUniforms++, "SamplerNearest"_sh, g_renderInterface->placeholderData->GetSamplerNearest());

    rq << SetShaderUniform(numShaderUniforms++, "CamerasBuffer"_sh, g_renderInterface->gpuBuffers[GRB_CAMERAS]->GetBuffer(frameIndex), TShaderDataOffset<CameraShaderData>(rs.view->GetCamera()));
    rq << SetShaderUniform(numShaderUniforms++, "EntitiesBuffer"_sh, g_renderInterface->gpuBuffers[GRB_ENTITIES]->GetBuffer(frameIndex));
    rq << SetShaderUniform(numShaderUniforms++, "WorldsBuffer"_sh, g_renderInterface->gpuBuffers[GRB_WORLDS]->GetBuffer(frameIndex));
    rq << SetShaderUniform(numShaderUniforms++, "MaterialsBuffer"_sh, g_renderInterface->gpuBuffers[GRB_MATERIALS]->GetBuffer(frameIndex));

    rq << SetShaderUniform(numShaderUniforms++, "ShadowMapsTextureArray"_sh, g_renderInterface->shadowMapAllocator->GetAtlasImageView());
    rq << SetShaderUniform(numShaderUniforms++, "PointLightShadowMapsTextureArray"_sh, g_renderInterface->shadowMapAllocator->GetPointLightShadowMapImageView());

    if (rs.envGrid != nullptr)
        rq << SetShaderUniform(numShaderUniforms++, "EnvGridsBuffer"_sh, g_renderInterface->gpuBuffers[GRB_ENV_GRIDS]->GetBuffer(frameIndex), TShaderDataOffset<EnvGridShaderData>(rs.envGrid));
    else
        rq << SetShaderUniform(numShaderUniforms++, "EnvGridsBuffer"_sh, g_renderInterface->gpuBuffers[GRB_ENV_GRIDS]->GetBuffer(frameIndex), TShaderDataOffset<EnvGridShaderData>(0));

    DeferredRendererPassData* dpd = ObjCast<DeferredRendererPassData>(rs.passData);
    AssertDebug(dpd != nullptr);

    const FramebufferRef& opaquePassFramebuffer = dpd->view.GetUnsafe()->GetOutputTarget().GetFramebuffer(RB_OPAQUE);

    for (uint32 attachmentIndex = 0; attachmentIndex < GTN_MAX; attachmentIndex++)
    {
        rq << SetShaderUniform(numShaderUniforms++, GBufferTextureNames[attachmentIndex], opaquePassFramebuffer->GetAttachment(attachmentIndex)->GetImageView());
    }

    rq << SetShaderUniform(numShaderUniforms++, "GBufferMipChain"_sh, g_renderInterface->textureViewCache->GetOrCreate(dpd->mipChain));

    if (dpd->hbao != nullptr)
        rq << SetShaderUniform(numShaderUniforms++, "SSAOResultTexture"_sh, dpd->hbao->GetFinalImageView());

    if (dpd->reflectionsPass != nullptr)
        rq << SetShaderUniform(numShaderUniforms++, "ReflectionProbeResultTexture"_sh, dpd->reflectionsPass->GetFinalImageView());

    if (m_mode == DPM_INDIRECT_LIGHTING)
    {
        if (dpd->ssgi != nullptr)
            rq << SetShaderUniform(numShaderUniforms++, "SSGIResultTexture"_sh, g_renderInterface->textureViewCache->GetOrCreate(dpd->ssgi->GetFinalResultTexture()));

        if (dpd->rayTracingReflections != nullptr)
            rq << SetShaderUniform(numShaderUniforms++, "RTRadianceResultTexture"_sh, dpd->rayTracingReflections->GetFinalImageView());

        if (dpd->ddgi)
        {
            rq << SetShaderUniform(numShaderUniforms++, "DDGIConstants"_sh, dpd->ddgi->GetConstantBuffer(frameIndex));
            rq << SetShaderUniform(numShaderUniforms++, "DDGIIrradianceTexture"_sh, dpd->ddgi->GetIrradianceImageView());
            rq << SetShaderUniform(numShaderUniforms++, "DDGIDepthTexture"_sh, dpd->ddgi->GetDepthImageView());
        }
    }

    if (m_mode == DPM_INDIRECT_LIGHTING)
    {
        ShaderPropertySet shaderProperties;
        GetDeferredShaderProperties(DPM_INDIRECT_LIGHTING, shaderProperties, &rpl);

        rq << SetCurrentShader(ShaderDesc(NAME("DeferredIndirect"), shaderProperties));

        RenderFullScreenQuad(frame, rs);

        return;
    }

    // last LightType we rendered
    LightType prevLightType = LT_INVALID;

    // render with each light
    for (uint32 lightTypeIndex = 0; lightTypeIndex < LT_MAX; lightTypeIndex++)
    {
        const LightType lightType = LightType(lightTypeIndex);

        for (Light* light : rpl.GetLights())
        {
            if (light->GetLightType() != lightTypeIndex)
            {
                continue;
            }

            RenderProxyLight* lightProxy = static_cast<RenderProxyLight*>(GetRenderProxy(light));
            AssertDebug(lightProxy != nullptr);

            if (lightType != prevLightType)
            {
                ShaderPropertySet shaderProperties;
                GetDeferredShaderProperties(DPM_DIRECT_LIGHTING, shaderProperties, &rpl, lightType);

                rq << SetCurrentShader(ShaderDesc(NAME("DeferredDirect"), shaderProperties));
            }

            uint32 localNumShaderUniforms = numShaderUniforms;
            rq << SetShaderUniform(localNumShaderUniforms++, "CurrentLight"_sh, g_renderInterface->gpuBuffers[GRB_LIGHTS]->GetBuffer(frameIndex), TShaderDataOffset<LightShaderData>(light));

            if (lightType == LT_AREA_RECT)
            {
                if (lightProxy != nullptr && lightProxy->lightMaterial != nullptr)
                {
                    RenderProxyMaterial* materialProxy = static_cast<RenderProxyMaterial*>(GetRenderProxy(lightProxy->lightMaterial));
                    AssertDebug(materialProxy != nullptr);

                    if (materialProxy->attributes.textureMask & uint32(MaterialTextureKey::Diffuse))
                    {
                        const uint32 materialBoundIndex = RetrieveResourceBinding(lightProxy->lightMaterial);
                        AssertDebug(materialBoundIndex != ~0u);

                        Span<const GpuImageViewRef> imageViews = g_renderInterface->materialTextureCache->imageViews.Get(materialBoundIndex);
                        AssertDebug(imageViews.Size() >= materialProxy->boundTextures.Size());

                        rq << SetShaderUniform(localNumShaderUniforms++, "DiffuseMap"_sh, imageViews[materialProxy->boundTextureIndices[0]]);
                    }

                    rq << SetShaderUniform(localNumShaderUniforms++, "CurrentMaterial"_sh, g_renderInterface->gpuBuffers[GRB_MATERIALS]->GetBuffer(frameIndex), TShaderDataOffset<MaterialShaderData>(lightProxy->lightMaterial));
                }
                else
                {
                    rq << SetShaderUniform(localNumShaderUniforms++, "CurrentMaterial"_sh, g_renderInterface->gpuBuffers[GRB_MATERIALS]->GetBuffer(frameIndex), TShaderDataOffset<MaterialShaderData>(0));
                }

                rq << SetShaderUniform(localNumShaderUniforms++, "LTCSampler"_sh, m_ltcSampler);

                if (m_ltcMatrixTexture != nullptr)
                    rq << SetShaderUniform(localNumShaderUniforms++, "LTCMatrixTexture"_sh, g_renderInterface->textureViewCache->GetOrCreate(m_ltcMatrixTexture));

                if (m_ltcBrdfTexture != nullptr)
                    rq << SetShaderUniform(localNumShaderUniforms++, "LTCBRDFTexture"_sh, g_renderInterface->textureViewCache->GetOrCreate(m_ltcBrdfTexture));
            }

            RenderFullScreenQuad(frame, rs);

            // Bind material descriptor set (for area lights)

            //// @TOOD FIxme use new way!!!
            // if (materialDescriptorSetIndex != ~0u)
            //{
            //     const DescriptorSetRef& materialDescriptorSet = g_renderInterface->materialDescriptorSetManager->ForBoundMaterial(light->GetMaterial(), frame->GetFrameIndex());

            //    frame->renderQueue << BindDescriptorSet(
            //        materialDescriptorSet,
            //        pipeline,
            //        {},
            //        materialDescriptorSetIndex);
            //}

            prevLightType = lightType;
        }
    }
}

#pragma endregion DeferredPass

#pragma region TonemapPass

TonemapPass::TonemapPass(Vec2u extent, GBuffer* gbuffer)
    : FullScreenPass(TextureFormat::R11G11B10F, extent, gbuffer)
{
    const VertexAttributeSet vertexAttributes = VertexAttribute::Position | VertexAttribute::Normal | VertexAttribute::TexCoord0;

    ShaderPropertySet shaderProperties;
    shaderProperties.Add(InternShaderProperty(ShaderProperty(NAME("OUTPUT"), NAME("SDR"))));

    m_shaderDesc = ShaderDesc(NAME("Tonemap"), shaderProperties);
}

TonemapPass::~TonemapPass()
{
}

void TonemapPass::Resize_Internal(Vec2u newSize)
{
    FullScreenPass::Resize_Internal(newSize);
}

void TonemapPass::Render(Frame* frame, const RenderSetup& rs)
{
    Begin(frame, rs);

    RenderQueue& rq = frame->renderQueue;

    DeferredRendererPassData* dpd = ObjCast<DeferredRendererPassData>(rs.passData);
    AssertDebug(dpd != nullptr);

    const uint32 frameIndex = frame->GetFrameIndex();
    const FramebufferRef& inputsFramebuffer = dpd->view.GetUnsafe()->GetOutputTarget().GetFramebuffer(RB_OPAQUE);

    uint32 numShaderUniforms = 0;

    rq << SetShaderUniform(numShaderUniforms++, "GBufferAlbedoTexture"_sh, inputsFramebuffer->GetAttachment(GTN_ALBEDO)->GetImageView());
    rq << SetShaderUniform(numShaderUniforms++, "GBufferNormalsTexture"_sh, inputsFramebuffer->GetAttachment(GTN_NORMALS)->GetImageView());
    rq << SetShaderUniform(numShaderUniforms++, "GBufferMaterialTexture"_sh, inputsFramebuffer->GetAttachment(GTN_MATERIAL)->GetImageView());
    rq << SetShaderUniform(numShaderUniforms++, "GBufferVelocityTexture"_sh, inputsFramebuffer->GetAttachment(GTN_VELOCITY)->GetImageView());
    rq << SetShaderUniform(numShaderUniforms++, "GBufferDepthTexture"_sh, inputsFramebuffer->GetAttachment(GTN_DEPTH)->GetImageView());

    Framebuffer* translucentPassFramebuffer = dpd->view.GetUnsafe()->GetOutputTarget().GetFramebuffer(RB_TRANSLUCENT);
    AssertDebug(translucentPassFramebuffer != nullptr);

    rq << SetShaderUniform(numShaderUniforms++, "DeferredResult"_sh, translucentPassFramebuffer->GetAttachment(GTN_ALBEDO)->GetImageView());

    rq << SetShaderUniform(numShaderUniforms++, "ShadowMapsTextureArray"_sh, g_renderInterface->shadowMapAllocator->GetAtlasImageView());

    rq << SetShaderUniform(numShaderUniforms++, "GBufferMipChain"_sh, g_renderInterface->textureViewCache->GetOrCreate(dpd->mipChain));

    rq << SetShaderUniform(numShaderUniforms++, "SamplerNearest"_sh, g_renderInterface->placeholderData->GetSamplerNearest());
    rq << SetShaderUniform(numShaderUniforms++, "SamplerLinear"_sh, g_renderInterface->placeholderData->GetSamplerLinear());

    if (dpd->rayTracingReflections)
    {
        rq << SetShaderUniform(numShaderUniforms++, "RTRadianceResultTexture"_sh, dpd->rayTracingReflections->GetFinalImageView());
    }
    else
    {
        rq << SetShaderUniform(numShaderUniforms++, "RTRadianceResultTexture"_sh, g_renderInterface->placeholderData->GetImageView2D1x1R8());
    }

    if (dpd->ssgi)
    {
        rq << SetShaderUniform(numShaderUniforms++, "SSGIResultTexture"_sh, g_renderInterface->textureViewCache->GetOrCreate(dpd->ssgi->GetFinalResultTexture()));
    }
    else
    {
        rq << SetShaderUniform(numShaderUniforms++, "SSGIResultTexture"_sh, g_renderInterface->placeholderData->GetImageView2D1x1R8());
    }

    if (dpd->temporalAa)
    {
        rq << SetShaderUniform(numShaderUniforms++, "TAAResultTexture"_sh, g_renderInterface->textureViewCache->GetOrCreate(dpd->temporalAa->GetResultTexture()));
    }
    else
    {
        rq << SetShaderUniform(numShaderUniforms++, "TAAResultTexture"_sh, g_renderInterface->placeholderData->GetImageView2D1x1R8());
    }

    Texture* ssrTexture = dpd->reflectionsPass->ShouldRenderSSR()
        ? dpd->reflectionsPass->GetSSRRenderer()->GetFinalResultTexture()
        : nullptr;

    if (ssrTexture)
    {
        rq << SetShaderUniform(numShaderUniforms++, "SSRResultTexture"_sh, g_renderInterface->textureViewCache->GetOrCreate(ssrTexture));
    }
    else
    {
        rq << SetShaderUniform(numShaderUniforms++, "SSRResultTexture"_sh, g_renderInterface->placeholderData->GetImageView2D1x1R8());
    }

    if (dpd->hbao)
    {
        rq << SetShaderUniform(numShaderUniforms++, "SSAOResultTexture"_sh, dpd->hbao->GetFinalImageView());
    }
    else
    {
        rq << SetShaderUniform(numShaderUniforms++, "SSAOResultTexture"_sh, g_renderInterface->placeholderData->GetImageView2D1x1R8());
    }

    rq << SetShaderUniform(numShaderUniforms++, "DeferredIndirectResultTexture"_sh, dpd->deferredShadingFramebuffer->GetAttachment(0)->GetImageView());

    rq << SetShaderUniform(numShaderUniforms++, "PostProcessingUniforms"_sh, dpd->postProcessing->GetUniformBuffer());

    rq << SetShaderUniform(numShaderUniforms++, "CamerasBuffer"_sh, g_renderInterface->gpuBuffers[GRB_CAMERAS]->GetBuffer(frameIndex), TShaderDataOffset<CameraShaderData>(rs.view->GetCamera()));
    rq << SetShaderUniform(numShaderUniforms++, "WorldsBuffer"_sh, g_renderInterface->gpuBuffers[GRB_WORLDS]->GetBuffer(frameIndex));

    RenderFullScreenQuad(frame, rs);

    End(frame, rs);
}

#pragma endregion TonemapPass

#pragma region LightmapPass

struct LightmapVolumeUniforms
{
    float irradianceWeight;
    float radianceWeight;

    uint32 numAtlases;
};

LightmapPass::LightmapPass()
    : FullScreenPass(TextureFormat::RGBA16F, nullptr, FSP_EXTERNAL_RENDERTARGET)
{
    m_shaderDesc = ShaderDesc(NAME("ApplyLightmap"));
}

LightmapPass::~LightmapPass()
{
    for (auto& data : m_lightmapVolumePassData)
    {
        EnqueueDeletion(std::move(data.uniformBuffers));
    }
}

void LightmapPass::Create()
{
    AssertOnThread(g_renderThread);

    FullScreenPass::Create();
}

void LightmapPass::Resize_Internal(Vec2u newSize)
{
    FullScreenPass::Resize_Internal(newSize);
}

void LightmapPass::RenderToFramebuffer_Internal(Frame* frame, const RenderSetup& renderSetup, Framebuffer* framebuffer)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.world && renderSetup.volume && renderSetup.view);

    const uint32 frameIndex = frame->GetFrameIndex();

    LightmapVolume* volume = ObjCast<LightmapVolume>(renderSetup.volume);
    AssertDebug(volume != nullptr);

    RenderProxyLightmapVolume* proxy = static_cast<RenderProxyLightmapVolume*>(GetRenderProxy(volume));
    Assert(proxy != nullptr);

    if (proxy->numAtlases == 0)
    {
        return; // nothing to do
    }

    DeferredRendererPassData* dpd = ObjCast<DeferredRendererPassData>(renderSetup.passData);
    AssertDebug(dpd != nullptr);

    Framebuffer* viewFramebuffer = dpd->view.GetUnsafe()->GetOutputTarget().GetFramebuffer(RB_OPAQUE);
    AssertDebug(viewFramebuffer != nullptr);

    const VertexAttributeSet vertexAttributes = VertexAttribute::Position | VertexAttribute::Normal | VertexAttribute::TexCoord0;

    RenderQueue& rq = frame->renderQueue;

    rq << SetCurrentShader(m_shaderDesc);

    rq << SetCurrentView(
        viewFramebuffer->GetRenderTargetDesc(),
        renderSetup.view->GetViewport());

    rq << SetVertexAttributes(vertexAttributes);

    rq << SetFaceCullMode(FCM_BACK);
    rq << SetFillMode(FM_FILL);
    rq << SetTopology(TOP_TRIANGLES);

    rq << SetDepthTest(false);
    rq << SetDepthWrite(false);

    rq << SetStencilTest(true);
    rq << SetStencilFunction(StencilFunction {
        .passOp = SO_KEEP,
        .failOp = SO_KEEP,
        .depthFailOp = SO_KEEP,
        .compareOp = SCO_EQUAL // match values with equal atlas index when we render
    });

    HYP_DEFER({
        // reset states
        rq << SetCurrentBlendFunction(BlendFunction::None());
        rq << SetStencilState(0, 0xFF, 0x0);
        rq << SetDepthWrite(true);
        rq << SetDepthTest(true);
        rq << SetStencilTest(false);
    });

    LightmapVolumePassData& data = GetLightmapVolumePassData(volume);

    uint32 numShaderUniforms = 0;

    // GBuffer textures
    rq << SetShaderUniform(numShaderUniforms++, "GBufferAlbedoTexture"_sh, viewFramebuffer->GetAttachment(GTN_ALBEDO)->GetImageView());
    rq << SetShaderUniform(numShaderUniforms++, "GBufferNormalsTexture"_sh, viewFramebuffer->GetAttachment(GTN_NORMALS)->GetImageView());
    rq << SetShaderUniform(numShaderUniforms++, "GBufferMaterialTexture"_sh, viewFramebuffer->GetAttachment(GTN_MATERIAL)->GetImageView());
    rq << SetShaderUniform(numShaderUniforms++, "GBufferDepthTexture"_sh, viewFramebuffer->GetAttachment(GTN_DEPTH)->GetImageView());
    rq << SetShaderUniform(numShaderUniforms++, "GBufferVelocityTexture"_sh, viewFramebuffer->GetAttachment(GTN_VELOCITY)->GetImageView());
    rq << SetShaderUniform(numShaderUniforms++, "GBufferMipChain"_sh, g_renderInterface->textureViewCache->GetOrCreate(dpd->mipChain));

    // Samplers
    rq << SetShaderUniform(numShaderUniforms++, "SamplerNearest"_sh, g_renderInterface->placeholderData->GetSamplerNearest());
    rq << SetShaderUniform(numShaderUniforms++, "SamplerLinear"_sh, g_renderInterface->placeholderData->GetSamplerLinear());

    // Shadows
    rq << SetShaderUniform(numShaderUniforms++, "ShadowMapsTextureArray"_sh, g_renderInterface->shadowMapAllocator->GetAtlasImageView());
    rq << SetShaderUniform(numShaderUniforms++, "PointLightShadowMapsTextureArray"_sh, g_renderInterface->shadowMapAllocator->GetPointLightShadowMapImageView());

    // Cameras and Worlds buffers
    rq << SetShaderUniform(numShaderUniforms++, "CamerasBuffer"_sh, g_renderInterface->gpuBuffers[GRB_CAMERAS]->GetBuffer(frameIndex), TShaderDataOffset<CameraShaderData>(renderSetup.view->GetCamera()));
    rq << SetShaderUniform(numShaderUniforms++, "WorldsBuffer"_sh, g_renderInterface->gpuBuffers[GRB_WORLDS]->GetBuffer(frameIndex));

    // Env probes
    rq << SetShaderUniform(numShaderUniforms++, "EnvProbesTexture"_sh, g_renderInterface->textureViewCache->GetOrCreate(g_renderInterface->envProbesTexture));
    rq << SetShaderUniform(numShaderUniforms++, "EnvProbesBuffer"_sh, g_renderInterface->gpuBuffers[GRB_ENV_PROBES]->GetBuffer(frameIndex));

    if (renderSetup.envProbe != nullptr)
        rq << SetShaderUniform(numShaderUniforms++, "CurrentEnvProbe"_sh, g_renderInterface->gpuBuffers[GRB_ENV_PROBES]->GetBuffer(frameIndex), TShaderDataOffset<EnvProbeShaderData>(renderSetup.envProbe));
    else
        rq << SetShaderUniform(numShaderUniforms++, "CurrentEnvProbe"_sh, g_renderInterface->gpuBuffers[GRB_ENV_PROBES]->GetBuffer(frameIndex), TShaderDataOffset<EnvProbeShaderData>(0));

    if (renderSetup.envGrid != nullptr)
        rq << SetShaderUniform(numShaderUniforms++, "EnvGridsBuffer"_sh, g_renderInterface->gpuBuffers[GRB_ENV_GRIDS]->GetBuffer(frameIndex), TShaderDataOffset<EnvGridShaderData>(renderSetup.envGrid));
    else
        rq << SetShaderUniform(numShaderUniforms++, "EnvGridsBuffer"_sh, g_renderInterface->gpuBuffers[GRB_ENV_GRIDS]->GetBuffer(frameIndex), TShaderDataOffset<EnvProbeShaderData>(0));

    if (dpd->reflectionsPass != nullptr)
        rq << SetShaderUniform(numShaderUniforms++, "ReflectionProbeResultTexture"_sh, dpd->reflectionsPass->GetFinalImageView());

    if (dpd->ssgi != nullptr)
        rq << SetShaderUniform(numShaderUniforms++, "SSGIResultTexture"_sh, g_renderInterface->textureViewCache->GetOrCreate(dpd->ssgi->GetFinalResultTexture()));

    if (dpd->hbao != nullptr)
        rq << SetShaderUniform(numShaderUniforms++, "SSAOResultTexture"_sh, dpd->hbao->GetFinalImageView());

    if (dpd->rayTracingReflections != nullptr)
        rq << SetShaderUniform(numShaderUniforms++, "RTRadianceResultTexture"_sh, dpd->rayTracingReflections->GetFinalImageView());

    if (data.uniformBuffers.Size() < proxy->numAtlases)
    {
        data.uniformBuffers.Resize(proxy->numAtlases);
    }

    for (uint32 atlasIndex = 0; atlasIndex < proxy->numAtlases; atlasIndex++)
    {
        Texture* irradianceTexture = proxy->atlasIrradianceTextures[atlasIndex];
        Texture* radianceTexture = proxy->atlasRadianceTextures[atlasIndex];

        LightmapVolumeUniforms uniforms {};
        uniforms.numAtlases = proxy->numAtlases;
        uniforms.irradianceWeight = irradianceTexture ? 1.0f : 0.0f;
        uniforms.radianceWeight = radianceTexture ? 1.0f : 0.0f;

        GpuBufferRef& uniformBuffer = data.uniformBuffers[atlasIndex];

        if (!uniformBuffer)
        {
            uniformBuffer = g_renderInterface->MakeGpuBuffer(GpuBufferType::CONSTANT_BUFFER, sizeof(LightmapVolumeUniforms));
            CheckResult(uniformBuffer->Create());
        }

        uniformBuffer->Copy(sizeof(uniforms), &uniforms);

        // only draw elems in the volume with a stencil reference of the atlas index (+1)
        rq << SetStencilState(atlasIndex + 1, LightmapStencilMask, 0x0);

        uint32 localNumShaderUniforms = numShaderUniforms;

        rq << SetShaderUniform(localNumShaderUniforms++, "IrradianceTexture"_sh, g_renderInterface->textureViewCache->GetOrCreate(irradianceTexture != nullptr ? irradianceTexture : g_renderInterface->placeholderData->defaultTexture2d));
        rq << SetShaderUniform(localNumShaderUniforms++, "RadianceTexture"_sh, g_renderInterface->textureViewCache->GetOrCreate(radianceTexture != nullptr ? radianceTexture : g_renderInterface->placeholderData->defaultTexture2d));
        rq << SetShaderUniform(localNumShaderUniforms++, "LightmapSampler"_sh, g_renderInterface->placeholderData->GetSamplerLinear());
        rq << SetShaderUniform(localNumShaderUniforms++, "LightmapVolumeUniforms"_sh, uniformBuffer);

        RenderFullScreenQuad(frame, renderSetup);
    }

    // reset stencil state back to default
    rq << SetStencilState(0, 0xFF, 0x0);

    m_isFirstFrame = false;
}

#pragma endregion LightmapPass

#pragma region FogVolumePass

static constexpr uint32 MaxBoundLightsPerFogVolume = 16;

FogVolumePass::FogVolumePass()
    : FullScreenPass(TextureFormat::RGBA16F, nullptr, FSP_EXTERNAL_RENDERTARGET)
{
}

FogVolumePass::~FogVolumePass()
{
    for (FogVolumePassData& data : m_fogVolumePassData)
    {
        EnqueueDeletion(std::move(data.cBuffer));
    }
}

void FogVolumePass::Create()
{
    AssertOnThread(g_renderThread);

    m_volumeMesh = MeshBuilder::Cube(true);
    m_volumeMesh->SetFlags(MeshFlags::ViewIndependent);
    m_volumeMesh->SetName(NAME("FogVolumeMesh"));
    InitObject(m_volumeMesh);

    ShaderPropertySet shaderProperties;
    shaderProperties.Add(InternShaderProperty(ShaderProperty(NAME("MAX_LIGHTS"), int(MaxBoundLightsPerFogVolume))));

    m_shaderDesc = ShaderDesc(NAME("ApplyFogVolume"), shaderProperties);

    FullScreenPass::Create();
}

void FogVolumePass::Resize_Internal(Vec2u newSize)
{
    FullScreenPass::Resize_Internal(newSize);
}

void FogVolumePass::RenderToFramebuffer_Internal(Frame* frame, const RenderSetup& renderSetup, Framebuffer* framebuffer)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.world && renderSetup.volume && renderSetup.view);

    FogVolume* volume = ObjCast<FogVolume>(renderSetup.volume);
    AssertDebug(volume != nullptr);

    RenderProxyFogVolume* proxy = static_cast<RenderProxyFogVolume*>(GetRenderProxy(volume));
    Assert(proxy != nullptr);

    FogVolumePassData& data = GetFogVolumePassData(volume);
    data.noiseTexture = proxy->noiseTexture;
    data.volumeTexture = proxy->volumeTexture;

    UpdateUniforms(frame, renderSetup, data);

    RenderQueue& rq = frame->renderQueue;

    rq << SetCurrentView(
        renderSetup.view->GetOutputTarget().GetFramebuffer()->GetRenderTargetDesc(),
        renderSetup.view->GetViewport());

    rq << SetTopology(m_volumeMesh->GetTopology());
    rq << SetVertexAttributes(m_volumeMesh->GetVertexAttributes());

    rq << SetFillMode(FM_FILL);
    rq << SetDepthWrite(false);
    rq << SetDepthTest(false);
    rq << SetStencilTest(false);
    rq << SetFaceCullMode(FCM_FRONT); // cull front faces to render inside of the volume
    rq << SetCurrentBlendFunction(BlendFunction(
        BMF_SRC_ALPHA, BMF_ONE_MINUS_SRC_ALPHA,
        BMF_ONE, BMF_ONE_MINUS_SRC_ALPHA));

    rq << SetCurrentShader(m_shaderDesc);

    rq << SetShaderUniform(0, "SamplerLinear"_sh, g_renderInterface->placeholderData->GetSamplerLinearMipmap());
    rq << SetShaderUniform(1, "SamplerNearest"_sh, g_renderInterface->placeholderData->GetSamplerNearest());

    DeferredRendererPassData* dpd = ObjCast<DeferredRendererPassData>(renderSetup.passData);
    AssertDebug(dpd != nullptr);

    const FramebufferRef& opaquePassFramebuffer = dpd->view.GetUnsafe()->GetOutputTarget().GetFramebuffer(RB_OPAQUE);

    for (uint32 attachmentIndex = 0; attachmentIndex < GTN_MAX; attachmentIndex++)
    {
        rq << SetShaderUniform(2 + attachmentIndex, GBufferTextureNames[attachmentIndex], opaquePassFramebuffer->GetAttachment(attachmentIndex)->GetImageView());
    }

    rq << SetShaderUniform(2 + GTN_MAX, "CamerasBuffer"_sh, g_renderInterface->gpuBuffers[GRB_CAMERAS]->GetBuffer(frame->GetFrameIndex()), TShaderDataOffset<CameraShaderData>(renderSetup.view->GetCamera()));

    rq << SetShaderUniform(3 + GTN_MAX, "ShadowMapsTextureArray"_sh, g_renderInterface->shadowMapAllocator->GetAtlasImageView());
    rq << SetShaderUniform(4 + GTN_MAX, "PointLightShadowMapsTextureArray"_sh, g_renderInterface->shadowMapAllocator->GetPointLightShadowMapImageView());

    if (data.volumeTexture)
        rq << SetShaderUniform(5 + GTN_MAX, "DataMap"_sh, g_renderInterface->textureViewCache->GetOrCreate(data.volumeTexture));

    if (data.noiseTexture)
        rq << SetShaderUniform(6 + GTN_MAX, "NoiseMap"_sh, g_renderInterface->textureViewCache->GetOrCreate(data.noiseTexture));

    rq << SetShaderUniform(7 + GTN_MAX, "FogVolumeUniforms"_sh, data.cBuffer);

    rq << CommitDrawState();

    rq << BindVertexBuffer(m_volumeMesh->GetVertexBuffer());
    rq << BindIndexBuffer(m_volumeMesh->GetIndexBuffer());
    rq << DrawIndexed(36); // draw cube

    // reset states
    rq << SetCurrentBlendFunction(BlendFunction::None());
    rq << SetDepthTest(true);
    rq << SetDepthWrite(true);

    m_isFirstFrame = false;
}

void FogVolumePass::UpdateUniforms(Frame* frame, const RenderSetup& renderSetup, FogVolumePassData& data)
{
    HYP_SCOPE;

    AssertDebug(renderSetup.world && renderSetup.view);

    RenderProxyList& rpl = GetConsumerProxyList(renderSetup.view);

    if (!data.cBuffer)
    {
        data.cBuffer = g_renderInterface->MakeGpuBuffer(
            GpuBufferType::CONSTANT_BUFFER,
            sizeof(FogVolumeShaderData) + sizeof(LightShaderData) * MaxBoundLightsPerFogVolume);
        Assert(data.cBuffer->Create());
    }

    GpuBufferBase* cBuffer = data.cBuffer;
    AssertDebug(cBuffer != nullptr);

    RenderProxyFogVolume* proxy = static_cast<RenderProxyFogVolume*>(GetRenderProxy(data.volume));
    Assert(proxy != nullptr);

    FogVolumeShaderData shaderData = proxy->bufferData;

    uint32& numBoundLights = shaderData.numBoundLights;
    numBoundLights = 0;

    uint32* lightIndicesU32 = reinterpret_cast<uint32*>(shaderData.lightIndices);

    for (Light* light : rpl.GetLights())
    {
        const LightType lightType = light->GetLightType();

        if (lightType != LT_DIRECTIONAL && lightType != LT_POINT)
        {
            continue;
        }

        if (numBoundLights >= MaxBoundLightmapVolumes)
        {
            break;
        }

        RenderProxyLight* lightProxy = static_cast<RenderProxyLight*>(GetRenderProxy(light));
        Assert(lightProxy != nullptr);

        cBuffer->Copy(sizeof(FogVolumeShaderData) + (numBoundLights * sizeof(LightShaderData)), sizeof(LightShaderData), &lightProxy->bufferData);

        lightIndicesU32[numBoundLights++] = RetrieveResourceBinding(light);
    }

    cBuffer->Copy(sizeof(FogVolumeShaderData), &shaderData);
}

#pragma endregion FogVolumePass

#pragma region ReflectionsPass

// Sky renders first
constexpr FixedArray<EnvProbeType, CMT_MAX> EnvProbeTypes {
    EPT_SKY,
    EPT_REFLECTION
};

constexpr FixedArray<CubemapType, CMT_MAX> CubemapTypes {
    CMT_DEFAULT,           // EPT_SKY
    CMT_PARALLAX_CORRECTED // EPT_REFLECTION
};

static const FixedArray<Pair<CubemapType, ShaderPropertySet>, CMT_MAX> s_cubemapPasses = {
    Pair<CubemapType, ShaderPropertySet> { CMT_DEFAULT, ShaderPropertySet {} },
    Pair<CubemapType, ShaderPropertySet> { CMT_PARALLAX_CORRECTED, ShaderPropertySet { { InternShaderProperty(ShaderProperty(NAME("ENV_PROBE_PARALLAX_CORRECTED"))) } } }
};

ReflectionsPass::ReflectionsPass(Vec2u extent, GBuffer* gbuffer, const GpuImageViewRef& mipChainImageView)
    : FullScreenPass(TextureFormat::RGBA16F, extent, gbuffer),
      m_mipChainImageView(mipChainImageView),
      m_isFirstFrame(true)
{
    m_shaderDesc = ShaderDesc(NAME("ApplyReflectionProbe"));

    SetBlendFunction(BlendFunction(
        BMF_SRC_ALPHA, BMF_ONE_MINUS_SRC_ALPHA,
        BMF_ONE, BMF_ONE_MINUS_SRC_ALPHA));
}

ReflectionsPass::~ReflectionsPass()
{
    EnqueueDeletion(std::move(m_mipChainImageView));

    m_ssrRenderer.Reset();
}

void ReflectionsPass::Create()
{
    HYP_SCOPE;

    FullScreenPass::Create();

    CreateSSRRenderer();
}

bool ReflectionsPass::ShouldRenderSSR() const
{
    static const ConfigurationValue& s_ssrEnabled = CoreApi::GetGlobalConfig().Get("Rendering.SSR.Enabled");
    static const ConfigurationValue& s_rayTracingReflectionsEnabled = CoreApi::GetGlobalConfig().Get("Rendering.RayTracing.Reflections.Enabled");

    return s_ssrEnabled.ToBool(true) && !s_rayTracingReflectionsEnabled.ToBool(false);
}

void ReflectionsPass::CreateSSRRenderer()
{
    m_ssrRenderer = MakeUnique<SSRRenderer>(SSRRendererConfig::FromConfig(), m_gbuffer, m_mipChainImageView);
    m_ssrRenderer->Create();
}

void ReflectionsPass::Resize_Internal(Vec2u newSize)
{
    HYP_SCOPE;

    FullScreenPass::Resize_Internal(newSize);
}

void ReflectionsPass::Render(Frame* frame, const RenderSetup& rs)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    AssertDebug(rs.world && rs.view);
    AssertDebug(rs.passData != nullptr);

    const uint32 frameIndex = frame->GetFrameIndex();

    RenderProxyList& rpl = GetConsumerProxyList(rs.view);
    rpl.BeginRead();
    HYP_DEFER({ rpl.EndRead(); });

    Viewport viewport = rs.view->GetViewport();

    if (ShouldRenderHalfRes())
    {
        const Vec2i viewportOffset = (Vec2i(m_framebuffer->GetExtent().x, 0) / 2) * (GetWorldBufferData()->frameCounter & 1);
        const Vec2u viewportExtent = Vec2u(m_framebuffer->GetExtent().x / 2, m_framebuffer->GetExtent().y);

        viewport = Viewport { viewportExtent, viewportOffset };
    }

    RenderQueue& rq = frame->renderQueue;

    if (ShouldRenderSSR())
    {
        m_ssrRenderer->Render(frame, rs);
    }

    rq << SetTopology(TOP_TRIANGLES);
    rq << SetVertexAttributes(VertexAttribute::Position | VertexAttribute::Normal | VertexAttribute::TexCoord0);

    rq << SetCurrentView(
        rs.view->GetOutputTarget().GetFramebuffer()->GetRenderTargetDesc(),
        rs.view->GetViewport());

    rq << SetCurrentShader(m_shaderDesc);

    rq << SetDepthTest(false);
    rq << SetDepthWrite(false);
    rq << SetStencilTest(false);
    rq << SetCurrentBlendFunction(BlendFunction(
        BMF_SRC_ALPHA, BMF_ONE_MINUS_SRC_ALPHA,
        BMF_ONE, BMF_ONE_MINUS_SRC_ALPHA));
    rq << SetFillMode(FM_FILL);
    rq << SetFaceCullMode(FCM_BACK);

    HYP_DEFER({
        rq << SetCurrentBlendFunction(BlendFunction::None());
        rq << SetDepthTest(true);
        rq << SetDepthWrite(true);
    });

    FixedArray<Array<EnvProbe*, RenderTempAllocator>, CMT_MAX> probesPerCubemapType;

    for (uint32 cubemapType = 0; cubemapType < CMT_MAX; cubemapType++)
    {
        const EnvProbeType envProbeType = EnvProbeTypes[cubemapType];

        for (EnvProbe* envProbe : rpl.GetEnvProbes().GetElements(s_envProbeTypeToTypeId[envProbeType]))
        {
            probesPerCubemapType[cubemapType].PushBack(envProbe);
        }
    }

    rq << BeginFramebuffer(GetFramebuffer());

    // render previous frame's result to screen if doing temporal blending (and not checkerboarded)
    if (!m_isFirstFrame && UsesTemporalBlending() && !ShouldRenderHalfRes())
    {
        DrawHistoryTexture(frame, rs);
    }

    rq << SetShaderUniform(0, "SamplerLinear"_sh, g_renderInterface->placeholderData->GetSamplerLinearMipmap());
    rq << SetShaderUniform(1, "SamplerNearest"_sh, g_renderInterface->placeholderData->GetSamplerNearest());

    DeferredRendererPassData* dpd = ObjCast<DeferredRendererPassData>(rs.passData);
    AssertDebug(dpd != nullptr);

    const FramebufferRef& opaquePassFramebuffer = dpd->view.GetUnsafe()->GetOutputTarget().GetFramebuffer(RB_OPAQUE);

    for (uint32 attachmentIndex = 0; attachmentIndex < GTN_MAX; attachmentIndex++)
    {
        rq << SetShaderUniform(2 + attachmentIndex, GBufferTextureNames[attachmentIndex], opaquePassFramebuffer->GetAttachment(attachmentIndex)->GetImageView());
    }

    rq << SetShaderUniform(2 + GTN_MAX, "CamerasBuffer"_sh, g_renderInterface->gpuBuffers[GRB_CAMERAS]->GetBuffer(frame->GetFrameIndex()), TShaderDataOffset<CameraShaderData>(rs.view->GetCamera()));
    rq << SetShaderUniform(3 + GTN_MAX, "WorldsBuffer"_sh, g_renderInterface->gpuBuffers[GRB_WORLDS]->GetBuffer(frame->GetFrameIndex()));
    rq << SetShaderUniform(4 + GTN_MAX, "EnvProbesBuffer"_sh, g_renderInterface->gpuBuffers[GRB_ENV_PROBES]->GetBuffer(frame->GetFrameIndex()));

    rq << SetShaderUniform(10 + GTN_MAX, "BlueNoiseBuffer"_sh, g_renderInterface->blueNoiseBuffer);
    rq << SetShaderUniform(11 + GTN_MAX, "SphereSamplesBuffer"_sh, g_renderInterface->sphereSamplesBuffer);

    rq << SetShaderUniform(12 + GTN_MAX, "GBufferMipChain"_sh, g_renderInterface->textureViewCache->GetOrCreate(dpd->mipChain));

    rq << SetShaderUniform(13 + GTN_MAX, "EnvProbesTexture"_sh, g_renderInterface->textureViewCache->GetOrCreate(g_renderInterface->envProbesTexture));

    uint32 numRenderedEnvProbes = 0;

    for (uint32 envProbeTypeIndex = 0; envProbeTypeIndex < ArraySize(EnvProbeTypes); envProbeTypeIndex++)
    {
        const EnvProbeType envProbeType = EnvProbeTypes[envProbeTypeIndex];
        const CubemapType cubemapType = CubemapTypes[envProbeTypeIndex];

        const Array<EnvProbe*, RenderTempAllocator>& probes = probesPerCubemapType[cubemapType];

        if (probes.Empty())
        {
            continue;
        }

        for (EnvProbe* envProbe : probes)
        {
            if (numRenderedEnvProbes >= MaxBoundReflectionProbes)
            {
                HYP_LOG(Rendering, Warning, "Attempting to render too many reflection probes.");

                break;
            }

            rq << SetShaderUniform(5 + GTN_MAX, "CurrentEnvProbe"_sh, g_renderInterface->gpuBuffers[GRB_ENV_PROBES]->GetBuffer(frame->GetFrameIndex()), TShaderDataOffset<EnvProbeShaderData>(envProbe));

            RenderFullScreenQuad(frame, rs);

            ++numRenderedEnvProbes;
        }
    }

    if (ShouldRenderSSR())
    {
        const Handle<Texture>& ssrTexture = m_ssrRenderer->GetFinalResultTexture();

        // render SSR to screen
        RenderTargetDesc renderTargetDesc = rs.view->GetOutputTarget().GetFramebuffer()->GetRenderTargetDesc();
        renderTargetDesc.attachments[0].loadOp = LoadOperation::LOAD;
        renderTargetDesc.attachments[0].blendFunction = BlendFunction(BMF_SRC_ALPHA, BMF_ONE_MINUS_SRC_ALPHA, BMF_ONE, BMF_ONE_MINUS_SRC_ALPHA);

        rq << SetCurrentView(renderTargetDesc, rs.view->GetViewport());

        rq << SetCurrentShader(ShaderDesc(NAME("BlitTexture")));

        // reset
        rq << SetDepthTest(false);
        rq << SetDepthWrite(false);
        rq << SetCurrentBlendFunction(BlendFunction(BMF_SRC_ALPHA, BMF_ONE_MINUS_SRC_ALPHA, BMF_ONE, BMF_ONE_MINUS_SRC_ALPHA));

        rq << SetShaderUniform(0, "SamplerLinear"_sh, g_renderInterface->placeholderData->GetSamplerLinear());
        rq << SetShaderUniform(1, "WorldsBuffer"_sh, g_renderInterface->gpuBuffers[GRB_WORLDS]->GetBuffer(frame->GetFrameIndex()));
        rq << SetShaderUniform(2, "InTexture"_sh, g_renderInterface->textureViewCache->GetOrCreate(ssrTexture));

        RenderFullScreenQuad(frame, rs);

        rq << SetDepthTest(true);
        rq << SetDepthWrite(true);
        rq << SetCurrentBlendFunction(BlendFunction::None());
    }

    frame->renderQueue << EndFramebuffer(GetFramebuffer());

    if (ShouldRenderHalfRes())
    {
        MergeHalfResTextures(frame, rs);
    }

    if (UsesTemporalBlending())
    {
        if (!ShouldRenderHalfRes())
        {
            CopyResultToPreviousTexture(frame, rs);
        }

        m_temporalBlending->Render(frame, rs);
    }

    m_isFirstFrame = false;
}

#pragma endregion ReflectionsPass

#pragma region DeferredRendererPassData

DeferredRendererPassData::~DeferredRendererPassData()
{
    depthPyramidRenderer.Reset();

    hbao.Reset();

    temporalAa.Reset();

    // m_dofBlur->Destroy();

    ssgi.Reset();

    postProcessing->Destroy();
    postProcessing.Reset();

    combinePass.Reset();

    reflectionsPass.Reset();

    lightmapPass.Reset();
    tonemapPass.Reset();
    mipChain.Reset();
    indirectPass.Reset();
    directPass.Reset();

    rayTracingReflections.Reset();
    ddgi.Reset();
}

#pragma endregion DeferredPassData

#pragma region RayTracingPassData

RayTracingPassData::~RayTracingPassData()
{
    EnqueueDeletion(std::move(cBuffer));
    EnqueueDeletion(std::move(lightsBuffer));
    EnqueueDeletion(std::move(rayTracingTlases));
}

#pragma endregion RayTracingPassData

#pragma region DeferredRenderer

static FramebufferRef CreateDeferredShadingFramebuffer(GBuffer* gbuffer)
{
    RenderTargetDesc renderTargetDesc {};
    renderTargetDesc.extent = gbuffer->GetExtent();

    FramebufferRef framebuffer = g_renderInterface->MakeFramebuffer(renderTargetDesc);

    TextureDesc textureDesc;
    textureDesc.type = TextureType::Texture2D;
    textureDesc.format = TextureFormat::RGBA16F;
    textureDesc.extent = Vec3u { gbuffer->GetExtent(), 1 };
    textureDesc.filterModeMin = TFM_NEAREST;
    textureDesc.filterModeMag = TFM_NEAREST;
    textureDesc.wrapMode = TWM_CLAMP_TO_EDGE;
    textureDesc.imageUsage = IU_ATTACHMENT | IU_SAMPLED;

    Attachment* colorAttachment = framebuffer->AddAttachment(
        0,
        g_renderInterface->MakeImage(textureDesc),
        LoadOperation::CLEAR,
        StoreOperation::STORE);

    // depth for stencil testing
    Attachment* depthAttachment = framebuffer->AddAttachment(
        1,
        gbuffer->GetBucket(RB_OPAQUE).GetGBufferAttachment(GTN_DEPTH)->GetImage(),
        LoadOperation::LOAD,
        StoreOperation::STORE);

    CheckResult(framebuffer->Create());

#ifdef HYP_DEBUG_MODE
    colorAttachment->GetImage()->SetDebugName(NAME("DeferredShadingTarget_Color"));
#endif

    return framebuffer;
}

DeferredRenderer::DeferredRenderer()
    : m_rendererConfig(RendererConfig::FromConfig())
{
}

DeferredRenderer::~DeferredRenderer()
{
}

void DeferredRenderer::Initialize()
{
    m_quadMesh = MeshBuilder::Quad();
    m_quadMesh->SetFlags(MeshFlags::ViewIndependent);
    InitObject(m_quadMesh);
}

void DeferredRenderer::Shutdown()
{
    m_quadMesh.Reset();
}

PassData* DeferredRenderer::CreateViewPassData(View* view, PassDataExt&)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    Assert(view != nullptr);

    if (view->GetFlags() & ViewFlags::GBUFFER)
    {
        DeferredRendererPassData* pd = new DeferredRendererPassData();
        DeferredRendererPassData& passData = *pd;

        passData.view = MakeWeakRef(view);
        passData.viewport = view->GetViewport();

        GBuffer* gbuffer = view->GetOutputTarget().GetGBuffer();
        Assert(gbuffer != nullptr);

        if (gbuffer->GetExtent() != passData.viewport.extent)
        {
            gbuffer->Resize(passData.viewport.extent);
        }

        gbuffer->Create();

        AssertDebug(gbuffer->IsCreated());

        HYP_LOG(Rendering, Verbose, "Creating renderer for view '{}' with GBuffer '{}'", view->Id(), gbuffer->GetExtent());

        const FramebufferRef& opaquePassFramebuffer = view->GetOutputTarget().GetFramebuffer(RB_OPAQUE);
        const FramebufferRef& lightmapPassFramebuffer = view->GetOutputTarget().GetFramebuffer(RB_LIGHTMAP);

        passData.ssgi = MakeUnique<SSGI>(SSGIConfig::FromConfig(), gbuffer);
        passData.ssgi->Create();

        passData.postProcessing = MakeUnique<PostProcessing>();
        passData.postProcessing->Create();

        passData.deferredShadingFramebuffer = CreateDeferredShadingFramebuffer(gbuffer);

        passData.indirectPass = MakeHandle<DeferredPass>(DPM_INDIRECT_LIGHTING, passData.viewport.extent, gbuffer, passData.deferredShadingFramebuffer);
        passData.indirectPass->Create();

        passData.directPass = MakeHandle<DeferredPass>(DPM_DIRECT_LIGHTING, passData.viewport.extent, gbuffer, passData.deferredShadingFramebuffer);
        passData.directPass->Create();

        passData.depthPyramidRenderer = MakeUnique<DepthPyramidRenderer>(gbuffer);
        passData.depthPyramidRenderer->Create();

        passData.cullData.depthPyramidImageView = passData.depthPyramidRenderer->GetResultImageView();
        passData.cullData.depthPyramidDimensions = passData.depthPyramidRenderer->GetExtent();

        passData.mipChain = MakeHandle<Texture>(TextureDesc {
            TextureType::Texture2D,
            opaquePassFramebuffer->GetAttachment(0)->GetFormat(),
            Vec3u(opaquePassFramebuffer->GetExtent(), 1),
            TFM_LINEAR_MIPMAP,
            TFM_LINEAR_MIPMAP,
            TWM_CLAMP_TO_EDGE
        });

        InitObject(passData.mipChain);

        passData.hbao = MakeHandle<HBAO>(HBAOConfig::FromConfig(), passData.viewport.extent, gbuffer);
        passData.hbao->Create();

        // m_dofBlur = MakeUnique<DOFBlur>(gbuffer->GetResolution(), gbuffer);
        // m_dofBlur->Create();

        passData.reflectionsPass = MakeHandle<ReflectionsPass>(
            passData.viewport.extent,
            gbuffer,
            g_renderInterface->textureViewCache->GetOrCreate(passData.mipChain));

        passData.reflectionsPass->Create();

        passData.tonemapPass = MakeHandle<TonemapPass>(passData.viewport.extent, gbuffer);
        passData.tonemapPass->Create();

        // We'll render the lightmap pass into the translucent framebuffer after deferred shading has been applied to OPAQUE objects.
        passData.lightmapPass = MakeHandle<LightmapPass>();
        passData.lightmapPass->Create();

        passData.fogVolumePass = MakeHandle<FogVolumePass>();
        passData.fogVolumePass->Create();

        passData.temporalAa = MakeUnique<TemporalAA>(passData.tonemapPass->GetFinalImageView(), passData.viewport.extent, gbuffer);
        passData.temporalAa->Create();

        CreateViewRayTracingPasses(view, passData);

        return pd;
    }
    else if ((view->GetFlags() & ViewFlags::RAY_TRACING) && g_renderInterface->GetRenderConfig().rayTracing)
    {
        RayTracingPassData* pd = new RayTracingPassData();
        RayTracingPassData& passData = *pd;

        passData.view = MakeWeakRef(view);
        passData.viewport = view->GetViewport();

        return pd;
    }

    HYP_LOG(Rendering, Fatal,
        "Cannot create PassData for View {}! View does not have any flags set that would allow us to create PassData for it. View flags: {}",
        view->Id(), uint32(view->GetFlags()));

    return nullptr;
}

void DeferredRenderer::CreateViewRayTracingPasses(View* view, DeferredRendererPassData& passData)
{
    AssertOnThread(g_renderThread);

    if (!g_renderInterface->GetRenderConfig().rayTracing)
    {
        return;
    }

    const bool shouldEnableRayTracingForView = view->GetRayTracingView().IsValid()
        && CoreApi::GetGlobalConfig().Get("Rendering.RayTracing.Enabled").ToBool();

    if (!shouldEnableRayTracingForView)
    {
        passData.rayTracingReflections.Reset();
        passData.ddgi.Reset();

        return;
    }

    GBuffer* gbuffer = view->GetOutputTarget().GetGBuffer();
    AssertDebug(gbuffer != nullptr);

    passData.rayTracingReflections = MakeUnique<RayTracingReflections>(RayTracingReflectionsConfig::FromConfig(), gbuffer);
    passData.rayTracingReflections->Create();

    /// FIXME: Proper AABB for DDGI
    passData.ddgi = MakeUnique<DDGI>(DDGIInfo { .aabb = { { -40.0f, -5.0f, -40.0f }, { 40.0f, 40.0f, 40.0f } } });
    passData.ddgi->Create();
}

void DeferredRenderer::CreateViewTopLevelAccelerationStructures(View* view, RayTracingPassData& passData)
{
    EnqueueDeletion(std::move(passData.rayTracingTlases));

    // Hack to fix driver crash when building TLAS with no meshes
    Handle<Mesh> defaultMesh = MeshBuilder::Cube(true);
    defaultMesh->SetFlags(MeshFlags::ViewIndependent);
    InitObject(defaultMesh);

    GpuBlasRef blas = MeshBlasBuilder::Build(defaultMesh);
    CheckResult(blas->Create());

    for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
    {
        GpuTlasRef& tlas = passData.rayTracingTlases[frameIndex];

        tlas = g_renderInterface->MakeTLAS();
        tlas->AddGpuBlas(blas);

        CheckResult(tlas->Create());
    }
}

void DeferredRenderer::ResizeView(Viewport viewport, View* view, DeferredRendererPassData& passData)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    HYP_LOG(Rendering, Verbose, "Resizing View '{}' to {}x{}", view->Id(), viewport.extent.x, viewport.extent.y);

    Assert(viewport.extent.Volume() > 0);

    passData.viewport = viewport;

    const Vec2u newSize = Vec2u(viewport.extent);

    GBuffer* gbuffer = view->GetOutputTarget().GetGBuffer();
    Assert(gbuffer != nullptr && gbuffer->IsCreated());

    gbuffer->Resize(newSize);

    const FramebufferRef& opaquePassFramebuffer = view->GetOutputTarget().GetFramebuffer(RB_OPAQUE);
    const FramebufferRef& lightmapPassFramebuffer = view->GetOutputTarget().GetFramebuffer(RB_LIGHTMAP);

    {
        if (passData.deferredShadingFramebuffer.IsValid())
        {
            EnqueueDeletion(std::move(passData.deferredShadingFramebuffer));
        }

        passData.deferredShadingFramebuffer = CreateDeferredShadingFramebuffer(gbuffer);
    }

    passData.directPass->Resize(newSize);
    passData.indirectPass->Resize(newSize);

    passData.hbao->Resize(newSize);

    passData.reflectionsPass.Reset();
    passData.reflectionsPass = MakeHandle<ReflectionsPass>(
        newSize,
        gbuffer,
        g_renderInterface->textureViewCache->GetOrCreate(passData.mipChain));

    passData.reflectionsPass->Create();

    passData.tonemapPass = MakeHandle<TonemapPass>(passData.viewport.extent, gbuffer);
    passData.tonemapPass->Create();

    passData.lightmapPass = MakeHandle<LightmapPass>();
    passData.lightmapPass->Create();

    passData.fogVolumePass = MakeHandle<FogVolumePass>();
    passData.fogVolumePass->Create();

    passData.temporalAa = MakeUnique<TemporalAA>(passData.tonemapPass->GetFinalImageView(), newSize, gbuffer);
    passData.temporalAa->Create();

    passData.depthPyramidRenderer = MakeUnique<DepthPyramidRenderer>(gbuffer);
    passData.depthPyramidRenderer->Create();

    CreateViewRayTracingPasses(view, passData);

    passData.view = MakeWeakRef(view);
}

void DeferredRenderer::RenderFrame(Frame* frame, const RenderSetup& rs)
{
    HYP_SCOPE;

    AssertDebug(rs.world);

    if (rs.world->GetViews().Size() == 0)
    {
        // No views to render
        return;
    }

    Array<RenderProxyList*, InlineAllocator<8, RenderAllocator>> renderProxyLists;

    HYP_DEFER({
        for (RenderProxyList* rpl : renderProxyLists)
        {
            rpl->EndRead();
        }
    });

    // Collect view-independent renderable types from all views, binned
    //// \todo : We could use the existing binning by subclass that ResourceTracker now provides.
    FixedArray<FlatSet<EnvProbe*>, EPT_MAX> envProbes;
    FixedArray<FlatSet<Light*>, LT_MAX> lights;
    FlatSet<EnvGrid*> envGrids;

    // For rendering EnvGrids and EnvProbes, we use a directional light from one of the Views that references it (if found)
    FlatMap<EnvGrid*, Light*> envGridLights;
    FlatMap<EnvProbe*, Light*> envProbeLights;

    // init view pass data and collect global rendering resources
    // (env probes, env grids)
    for (View* view : rs.world->GetViews())
    {
        AssertDebug(view != nullptr);

        RenderProxyList& rpl = GetConsumerProxyList(view);
        rpl.BeginRead();

        renderProxyLists.PushBack(&rpl);

        if (view->GetFlags() & ViewFlags::GBUFFER)
        {
            PassData* pd = FetchViewPassData(view);
            Assert(pd != nullptr);

            DeferredRendererPassData* pdCasted = ObjCast<DeferredRendererPassData>(pd);
            Assert(pdCasted != nullptr);

            const Viewport vp = view->GetViewport();

            if (pdCasted->viewport != vp)
            {
                ResizeView(vp, view, *pdCasted);
            }

            pdCasted->priority = view->GetPriority();
        }
        else if ((view->GetFlags() & ViewFlags::RAY_TRACING) && g_renderInterface->GetRenderConfig().rayTracing)
        {
            PassData* pd = FetchViewPassData(view);
            Assert(pd != nullptr);

            RayTracingPassData* pdCasted = ObjCast<RayTracingPassData>(pd);
            Assert(pdCasted != nullptr);

            RenderSetup newRS = rs.Fork();
            newRS.passData = pd;
            newRS.view = view;

            UpdateRayTracingView(frame, newRS);
        }

        for (Light* light : rpl.GetLights())
        {
            AssertDebug(light != nullptr);

            lights[light->GetLightType()].Insert(light);
        }

        for (EnvProbe* envProbe : rpl.GetEnvProbes())
        {
            if (envProbes[envProbe->GetEnvProbeType()].Contains(envProbe))
            {
                continue;
            }

            if (!envProbeLights.Contains(envProbe))
            {
                for (Light* light : rpl.GetLights())
                {
                    AssertDebug(light != nullptr);

                    if (light->GetLightType() == LT_DIRECTIONAL)
                    {
                        envProbeLights[envProbe] = light;

                        break;
                    }
                }
            }

            envProbes[envProbe->GetEnvProbeType()].Insert(envProbe);
        }

        for (EnvGrid* envGrid : rpl.GetEnvGrids())
        {
            if (envGrids.Contains(envGrid))
            {
                continue;
            }

            if (!envGridLights.Contains(envGrid))
            {
                for (Light* light : rpl.GetLights())
                {
                    if (light->GetLightType() == LT_DIRECTIONAL)
                    {
                        envGridLights[envGrid] = light;

                        break;
                    }
                }
            }

            envGrids.Insert(envGrid);
        }
    }

    // Render shadows for shadow casting lights
    for (uint32 lightType = 0; lightType < LT_MAX; lightType++)
    {
        RendererBase* shadowRenderer = g_renderInterface->globalRenderers[GRT_SHADOW_MAP][lightType];

        if (!lights[lightType].Any() || !shadowRenderer)
        {
            // No lights of that LightType bound or there is no defined ShadowRenderer
            continue;
        }

        /// TODO: We'll need a new PassData type (ShadowPassData ?) in order to store the textures / image views (in the case of atlas textures)
        /// and we'll need some state to tell if we need to re-render the shadows.
        for (Light* light : lights[lightType])
        {
            AssertDebug(light != nullptr);

            if (light->GetLightFlags() & LF_SHADOW)
            {
                RenderSetup shadowRs = rs.Fork();
                shadowRs.light = light;

                shadowRenderer->RenderFrame(frame, shadowRs);
            }
        }
    }

    {
        RenderSetup envProbeBaseRS = rs.Fork();

        // Set sky as fallback probe
        if (envProbes[EPT_SKY].Any())
        {
            envProbeBaseRS.envProbe = envProbes[EPT_SKY].Front();
        }

        if (lights[LT_DIRECTIONAL].Any())
        {
            envProbeBaseRS.light = lights[LT_DIRECTIONAL].Front();
        }

        if (envProbes.Any())
        {
            // check for dynamic env probes to render
            for (uint32 envProbeType = 0; envProbeType <= EPT_REFLECTION; envProbeType++)
            {
                if (RendererBase* renderer = g_renderInterface->globalRenderers[GRT_ENV_PROBE][envProbeType])
                {
                    for (EnvProbe* envProbe : envProbes[envProbeType])
                    {
                        if (envProbe->IsBaked())
                        {
                            continue; // skip baked
                        }

                        RenderSetup envProbeRS = envProbeBaseRS.Fork();
                        envProbeRS.envProbe = envProbe;

                        renderer->RenderFrame(frame, envProbeRS);
                    }
                }
                else
                {
                    HYP_LOG(Rendering, Warning, "No EnvProbeRenderer found for EnvProbeType {}! Skipping rendering of env probes of this type.", EPT_REFLECTION);
                }
            }
        }

        if (envGrids.Any())
        {
            for (EnvGrid* envGrid : envGrids)
            {
                RenderSetup envGridRS = envProbeBaseRS.Fork();

                // Set global directional light as fallback
                if (envGridLights.Contains(envGrid))
                {
                    envGridRS.light = envGridLights[envGrid];
                }

                envGridRS.envGrid = envGrid;

                g_renderInterface->globalRenderers[GRT_ENV_GRID][0]->RenderFrame(frame, envGridRS);
            }
        }
    }

    for (View* view : rs.world->GetViews())
    {
        AssertDebug(view != nullptr);

        if (!(view->GetFlags() & ViewFlags::GBUFFER))
        {
            continue;
        }

        RenderSetup viewRS = rs.Fork();

        DeferredRendererPassData* pd = ObjCast<DeferredRendererPassData>(FetchViewPassData(view));
        AssertDebug(pd != nullptr);
        AssertDebug(pd->viewport.extent.Volume() != 0);

        viewRS.view = view;
        viewRS.passData = pd;

        RenderFrameForView(frame, viewRS);

        viewRS.view = nullptr;
        viewRS.passData = nullptr;

        if (view->GetFlags() & ViewFlags::ENABLE_READBACK)
        {
            GpuImage* dstImage = view->GetReadbackTextureGpuImage();

            if (dstImage != nullptr)
            {
                GpuImage* srcImage = m_rendererConfig.taaEnabled
                    ? pd->temporalAa->GetResultTexture()->GetGpuImage()
                    : pd->tonemapPass->GetFinalImageView()->GetImage();

                Assert(srcImage != nullptr);

                const ResourceState previousResourceState = srcImage->GetResourceState();

                // wait for the image to be ready before readback
                if (previousResourceState == RS_UNDEFINED)
                {
                    HYP_LOG(Rendering, Warning, "Src image in UNDEFINED resource state; skipping texture blit.");

                    continue;
                }

                frame->renderQueue << InsertBarrier(srcImage, RS_COPY_SRC);

                AssertDebug(dstImage->IsCreated());

                frame->renderQueue << InsertBarrier(dstImage, RS_COPY_DST);
                frame->renderQueue << Blit(srcImage, dstImage);
                frame->renderQueue << InsertBarrier(dstImage, RS_SHADER_RESOURCE);

                frame->renderQueue << InsertBarrier(srcImage, previousResourceState);
            }
        }

        RenderProxyList& rpl = GetConsumerProxyList(view);

        g_statViews++;
        g_statTextures += rpl.GetTextures().NumCurrent();
        g_statMaterials += rpl.GetMaterials().NumCurrent();
        g_statLightmapVolumes += rpl.GetLightmapVolumes().NumCurrent();
        g_statParticleVolumes += rpl.GetParticleVolumes().NumCurrent();
        g_statLights += rpl.GetLights().NumCurrent();
        g_statEnvGrids += rpl.GetEnvGrids().NumCurrent();
        g_statEnvProbes += rpl.GetEnvProbes().NumCurrent();

#if 0
        HYP_LOG(Rendering, Verbose, "View '{}' used {} textures, {} materials, {} lightmap volumes, {} lights, {} env grids and {} env probes.",
            view->Id(),
            rpl.GetTextures().NumCurrent(),
            rpl.GetMaterials().NumCurrent(),
            rpl.GetLightmapVolumes().NumCurrent(),
            rpl.GetLights().NumCurrent(),
            rpl.GetEnvGrids().NumCurrent(),
            rpl.GetEnvProbes().NumCurrent());
#endif
    }
}

void DeferredRenderer::RenderFrameForView(Frame* frame, const RenderSetup& rs)
{
    HYP_SCOPE;

    AssertDebug(rs.world && rs.view);

    uint32 slot = GetRingIndex();
    if (m_lastFrameData.frameId != slot)
    {
        m_lastFrameData.frameId = slot;
        m_lastFrameData.passData.Clear();
    }

    View* view = rs.view;

    const Viewport& viewport = view->GetViewport();

    Assert(view->GetFlags() & ViewFlags::GBUFFER);

    RenderProxyList& rpl = GetConsumerProxyList(view);
    rpl.BeginRead();
    HYP_DEFER({ rpl.EndRead(); });

    RenderCollector& renderCollector = GetRenderCollector(view);

    DeferredRendererPassData* passDataCasted = ObjCast<DeferredRendererPassData>(rs.passData);
    AssertDebug(passDataCasted != nullptr);

    DeferredRendererPassData& passData = *passDataCasted;

    const uint32 frameIndex = frame->GetFrameIndex();

    Framebuffer* opaquePassFramebuffer = view->GetOutputTarget().GetFramebuffer(RB_OPAQUE);
    Framebuffer* lightmapPassFramebuffer = view->GetOutputTarget().GetFramebuffer(RB_LIGHTMAP);
    Framebuffer* translucentPassFramebuffer = view->GetOutputTarget().GetFramebuffer(RB_TRANSLUCENT);
    Framebuffer* debugPassFramebuffer = view->GetOutputTarget().GetFramebuffer(RB_DEBUG);

    const bool doParticles = true;

    const bool useRayTracingReflections = (m_rendererConfig.pathTracer || m_rendererConfig.rayTracingReflections)
        && view->GetRayTracingView().IsValid()
        && passData.rayTracingReflections != nullptr;

    const bool useRayTracingGlobalIllumination = m_rendererConfig.rayTracingGlobalIllumination
        && view->GetRayTracingView().IsValid()
        && passData.ddgi != nullptr;

    if (passData.temporalAa != nullptr && m_rendererConfig.taaEnabled)
    {
        // apply jitter to camera for TAA
        RenderProxyCamera* cameraProxy = static_cast<RenderProxyCamera*>(GetRenderProxy(view->GetCamera()));
        Assert(cameraProxy != nullptr);

        CameraShaderData& cameraBufferData = cameraProxy->bufferData;

        if (MathUtil::ApproxEqual(cameraBufferData.projMat[3][3], 0.0f))
        {
            const uint32 frameCounter = GetWorldBufferData()->frameCounter + 1;

            Vec4f jitter = Vec4f::Zero();
            Mat4f::Jitter(frameCounter, viewport.extent.x, viewport.extent.y, jitter);

            cameraBufferData.jitter = jitter * CameraJitterScale;

            UpdateGpuData(view->GetCamera());
        }
    }

    PerformOcclusionCulling(frame, rs, renderCollector);

    { // render opaque objects into separate framebuffer
        frame->renderQueue << BeginFramebuffer(opaquePassFramebuffer);

        ExecuteDrawCalls(frame, rs, renderCollector, (1u << RB_OPAQUE));

        frame->renderQueue << EndFramebuffer(opaquePassFramebuffer);
    }

    // render lightmap volume objects
    if (rpl.GetLightmapVolumes().NumCurrent())
    {
        // render objects to be lightmapped, separate from the opaque objects.
        // The lightmap bucket's framebuffer has a color attachment that will write into the opaque framebuffer's color attachment.
        frame->renderQueue << BeginFramebuffer(lightmapPassFramebuffer);

        ExecuteDrawCalls(frame, rs, renderCollector, (1u << RB_LIGHTMAP));

        frame->renderQueue << EndFramebuffer(lightmapPassFramebuffer);
    }

    passData.reflectionsPass->Render(frame, rs);

    if ((useRayTracingGlobalIllumination || useRayTracingReflections) && view->GetRayTracingView().IsValid())
    {
        Handle<View> rayTracingView = view->GetRayTracingView().Lock();

        if (rayTracingView != nullptr)
        {
            RayTracingPassData* rayTracingPassData = ObjCast<RayTracingPassData>(FetchViewPassData(rayTracingView));
            Assert(rayTracingPassData != nullptr);

            const GpuTlasRef& tlas = rayTracingPassData->rayTracingTlases[frameIndex];

            if (tlas && tlas->IsCreated())
            {
                Assert(tlas->GetMeshDescriptionsBuffer() != nullptr);

                rayTracingPassData->parentPass = &passData;

                RenderSetup rayTracingRS = rs.Fork();
                rayTracingRS.passData = rayTracingPassData;

                // set sky as fallback

                // Set first found sky probe as fallback probe
                auto& skyProbes = rpl.GetEnvProbes().GetElements<SkyProbe>();
                if (skyProbes.Any())
                {
                    rayTracingRS.envProbe = skyProbes.Front();
                }

                if (useRayTracingReflections)
                {
                    AssertDebug(passData.rayTracingReflections != nullptr);
                    passData.rayTracingReflections->Render(frame, rayTracingRS);
                }

                if (useRayTracingGlobalIllumination)
                {
                    AssertDebug(passData.ddgi != nullptr);
                    passData.ddgi->Render(frame, rayTracingRS);
                }

                // unset parent pass after using it
                rayTracingPassData->parentPass = nullptr;
            }
        }
    }

    if (m_rendererConfig.hbaoEnabled || m_rendererConfig.hbilEnabled)
    {
        passData.hbao->Render(frame, rs);
    }

    if (m_rendererConfig.ssgiEnabled)
    {
        RenderSetup newRenderSetup = rs;

        if (const auto& skyProbes = rpl.GetEnvProbes().GetElements<SkyProbe>(); skyProbes.Any())
        {
            newRenderSetup.envProbe = skyProbes.Front();
        }

        passData.ssgi->Render(frame, rs);
    }

    passData.postProcessing->RenderPre(frame, rs);

    { // deferred lighting on opaque objects
        frame->renderQueue << InsertBarrier(
            passData.deferredShadingFramebuffer->GetAttachment(1)->GetImage(),
            RS_DEPTH_STENCIL);

        frame->renderQueue << BeginFramebuffer(passData.deferredShadingFramebuffer);

        passData.indirectPass->RenderToFramebuffer(frame, rs, passData.deferredShadingFramebuffer);
        passData.directPass->RenderToFramebuffer(frame, rs, passData.deferredShadingFramebuffer);

        // apply baked lighting over lightmapped objects
        for (LightmapVolume* lightmapVolume : rpl.GetLightmapVolumes())
        {
            RenderSetup lightmapPassRS = rs.Fork();
            lightmapPassRS.volume = lightmapVolume;

            // Render the objects to have lightmaps applied into the translucent pass framebuffer with a full screen quad.
            // Apply lightmaps over the now shaded opaque objects.
            passData.lightmapPass->RenderToFramebuffer(frame, lightmapPassRS, passData.deferredShadingFramebuffer);
        }

        frame->renderQueue << EndFramebuffer(passData.deferredShadingFramebuffer);
    }

    { // generate mipchain after rendering opaque objects' lighting, now we can use it for transmission
        const GpuImageRef& srcImage = passData.deferredShadingFramebuffer->GetAttachment(0)->GetImage();
        GenerateMipChain(frame, rs, renderCollector, srcImage);
    }

    { // combined + translucent (forward pass)
        frame->renderQueue << BeginFramebuffer(translucentPassFramebuffer);

        { // Render the deferred lighting into the translucent pass framebuffer with a full screen quad.

            // We need some state struct we can set up one time and reuse + clone instead of this..
            // And we should also have a RAII struct so we can apply these states to the render interface, and when destructed, will
            // undo them!
            // Id' like to not use the render queue for this at some point but keep that functionality around for deferred command recording
            // (parallel)
            // but for stuff directly on the render thread we should just do g_renderInterface->SetTopology(...) etc. (and obv have something like g_renderInterface->SetDrawState(...) which sets in bulk)

            frame->renderQueue << SetCurrentView(
                rs.view->GetOutputTarget().GetFramebuffer()->GetRenderTargetDesc(),
                rs.view->GetViewport());

            frame->renderQueue << SetVertexAttributes(VertexAttribute::Position | VertexAttribute::Normal | VertexAttribute::TexCoord0);
            frame->renderQueue << SetFaceCullMode(FCM_BACK);
            frame->renderQueue << SetFillMode(FM_FILL);
            frame->renderQueue << SetTopology(TOP_TRIANGLES);
            frame->renderQueue << SetDepthTest(false);
            frame->renderQueue << SetDepthWrite(false);
            frame->renderQueue << SetStencilTest(false);

            frame->renderQueue << SetCurrentShader(ShaderDesc(NAME("BlitTexture")));

            frame->renderQueue << SetShaderUniform(0, "SamplerLinear"_sh, g_renderInterface->placeholderData->GetSamplerLinear());
            frame->renderQueue << SetShaderUniform(1, "WorldsBuffer"_sh, g_renderInterface->gpuBuffers[GRB_WORLDS]->GetBuffer(frame->GetFrameIndex()));
            frame->renderQueue << SetShaderUniform(2, "InTexture"_sh, passData.deferredShadingFramebuffer->GetAttachment(0)->GetImageView());

            frame->renderQueue << CommitDrawState();

            frame->renderQueue << BindVertexBuffer(m_quadMesh->GetVertexBuffer());
            frame->renderQueue << BindIndexBuffer(m_quadMesh->GetIndexBuffer());

            frame->renderQueue << DrawIndexed(6);

            // reset
            frame->renderQueue << SetDepthTest(true);
            frame->renderQueue << SetDepthWrite(true);
        }

        // begin translucent with forward rendering
        ExecuteDrawCalls(frame, rs, renderCollector, (1u << RB_TRANSLUCENT));
        ExecuteDrawCalls(frame, rs, renderCollector, (1u << RB_SKYBOX));

        // render fog volumes
        for (FogVolume* fogVolume : rpl.GetFogVolumes())
        {
            RenderSetup fogVolumeRS = rs.Fork();
            fogVolumeRS.volume = fogVolume;

            passData.fogVolumePass->RenderToFramebuffer(frame, fogVolumeRS, translucentPassFramebuffer);
        }

        // render particles
        if (rpl.GetParticleVolumes().NumCurrent())
        {
            for (ParticleVolume* particleVolume : rpl.GetParticleVolumes())
            {
                RenderSetup particleVolumeRS = rs.Fork();
                particleVolumeRS.volume = particleVolume;

                g_renderInterface->globalRenderers[GRT_PARTICLE_VOLUME][0]->RenderFrame(frame, particleVolumeRS);
            }
        }

        frame->renderQueue << EndFramebuffer(translucentPassFramebuffer);
    }

    { // render depth pyramid
        passData.depthPyramidRenderer->Render(frame);
        // update culling info now that depth pyramid has been rendered
        passData.cullData.depthPyramidImageView = passData.depthPyramidRenderer->GetResultImageView();
        passData.cullData.depthPyramidDimensions = passData.depthPyramidRenderer->GetExtent();
    }

    // debug draw
    if (renderCollector.mappingsByBucket[RB_DEBUG].Any()
        || DebugDrawer::GetInstance().NumEnqueuedDrawCommands() > 0)
    {
        frame->renderQueue << BeginFramebuffer(debugPassFramebuffer);

        ExecuteDrawCalls(frame, rs, renderCollector, (1u << RB_DEBUG));

        DebugDrawer::GetInstance().Render(frame, rs);

        frame->renderQueue << EndFramebuffer(debugPassFramebuffer);
    }

    passData.postProcessing->RenderPost(frame, rs);

    passData.tonemapPass->Render(frame, rs);

    if (passData.temporalAa != nullptr && m_rendererConfig.taaEnabled)
    {
        passData.temporalAa->Render(frame, rs);
    }

    // depth of field
    // m_dofBlur->Render(frame);

    // Ordered by View priority
    auto lastFrameDataIt = std::lower_bound(
        m_lastFrameData.passData.Begin(),
        m_lastFrameData.passData.End(),
        Pair<View*, DeferredRendererPassData*> { view, &passData },
        [view](const Pair<View*, DeferredRendererPassData*>& a, const Pair<View*, DeferredRendererPassData*>& b)
        {
            return a.second->priority < b.second->priority;
        });

    m_lastFrameData.passData.Insert(lastFrameDataIt, Pair<View*, DeferredRendererPassData*> { view, &passData });
}

void DeferredRenderer::UpdateRayTracingView(Frame* frame, const RenderSetup& rs)
{
    HYP_SCOPE;

    View* view = rs.view;
    AssertDebug(view != nullptr);

    if (!(view->GetFlags() & ViewFlags::RAY_TRACING) || !g_renderInterface->GetRenderConfig().rayTracing)
    {
        return;
    }

    const uint32 currentFrameIndex = frame->GetFrameIndex();

    RayTracingPassData* pd = ObjCast<RayTracingPassData>(rs.passData);

    RenderProxyList& rpl = GetConsumerProxyList(rs.view);
    rpl.BeginRead();
    HYP_DEFER({ rpl.EndRead(); });

    if (!pd->rayTracingTlases[currentFrameIndex])
    {
        for (GpuTlasRef& tlas : pd->rayTracingTlases)
        {
            tlas = g_renderInterface->MakeTLAS();
        }
    }

    bool hasBlas = false;

    for (Entity* entity : rpl.GetMeshEntities())
    {
        AssertDebug(entity != nullptr);

        RenderProxyMesh* meshProxy = rpl.GetMeshEntities().GetProxy(entity->Id());
        Assert(meshProxy != nullptr);

        AssertDebug(meshProxy->mesh != nullptr && meshProxy->mesh->IsReady());
        AssertDebug(meshProxy->material != nullptr && meshProxy->material->IsReady());

        const RenderBucket bucket = meshProxy->material->GetRenderAttributes().bucket;

        if (bucket != RB_OPAQUE
            && bucket != RB_LIGHTMAP
            && bucket != RB_TRANSLUCENT)
        {
            continue;
        }

        const GpuBlasRef& cachedBlas = m_meshRTData.GetOrCreateBLAS(entity, meshProxy->mesh, meshProxy->material);

        if (!cachedBlas)
        {
            HYP_LOG(Rendering, Error, "Failed to build BLAS for Mesh {}", meshProxy->mesh->GetName());
            continue;
        }

        GpuBlasRef& blas = meshProxy->rayTracingData.blas;

        if (blas != cachedBlas)
        {
            if (blas != nullptr)
            {
                for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
                {
                    pd->rayTracingTlases[frameIndex]->RemoveGpuBlas(blas);
                }
            }

            blas = cachedBlas;
        }

        if (!blas->IsCreated())
        {
            blas->SetTransform(meshProxy->bufferData.modelMatrix);

            const uint32 materialBinding = RetrieveResourceBinding(meshProxy->material);
            blas->SetMaterialBinding(materialBinding);

            CheckResult(blas->Create());
        }
        else
        {
            const uint32 materialBinding = RetrieveResourceBinding(meshProxy->material);

            blas->SetMaterialBinding(materialBinding);
            blas->SetTransform(meshProxy->bufferData.modelMatrix);
        }

        if (!pd->rayTracingTlases[currentFrameIndex]->HasGpuBlas(blas))
        {
            for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
            {
                pd->rayTracingTlases[frameIndex]->AddGpuBlas(meshProxy->rayTracingData.blas);
            }

            hasBlas = true;
        }
    }

    if (!pd->rayTracingTlases[currentFrameIndex]->IsCreated())
    {
        if (hasBlas)
        {
            for (GpuTlasRef& tlas : pd->rayTracingTlases)
            {
                CheckResult(tlas->Create());
            }
        }

        return;
    }

    RTUpdateStateFlags updateStateFlags = RTUpdateStateFlagBits::RT_UPDATE_STATE_FLAGS_NONE;
    pd->rayTracingTlases[currentFrameIndex]->UpdateStructure(updateStateFlags);
}

void DeferredRenderer::PerformOcclusionCulling(Frame* frame, const RenderSetup& rs, RenderCollector& renderCollector)
{
    HYP_SCOPE;

    constexpr uint32 BucketMask = (1 << RB_OPAQUE)
        | (1 << RB_LIGHTMAP)
        | (1 << RB_SKYBOX)
        | (1 << RB_TRANSLUCENT)
        | (1 << RB_DEBUG);

    renderCollector.PerformOcclusionCulling(frame, rs, BucketMask);
}

void DeferredRenderer::ExecuteDrawCalls(
    Frame* frame,
    const RenderSetup& rs,
    RenderCollector& renderCollector,
    uint32 bucketMask)
{
    HYP_SCOPE;

    renderCollector.ExecuteDrawCalls(frame, rs, bucketMask);
}

void DeferredRenderer::GenerateMipChain(Frame* frame, const RenderSetup& rs, RenderCollector& renderCollector, const GpuImageRef& srcImage)
{
    HYP_SCOPE;

    const uint32 frameIndex = frame->GetFrameIndex();

    DeferredRendererPassData* pd = ObjCast<DeferredRendererPassData>(rs.passData);

    const GpuImageRef& mipmappedResult = pd->mipChain->GetGpuImage();
    Assert(mipmappedResult.IsValid());

    frame->renderQueue << InsertBarrier(srcImage, RS_COPY_SRC);
    frame->renderQueue << InsertBarrier(mipmappedResult, RS_COPY_DST);

    // Blit into the mipmap chain img
    frame->renderQueue << BlitRect(
        srcImage,
        mipmappedResult,
        Rect<uint32> { 0, 0, srcImage->GetExtent().x, srcImage->GetExtent().y },
        Rect<uint32> { 0, 0, mipmappedResult->GetExtent().x, mipmappedResult->GetExtent().y });

    frame->renderQueue << GenerateMipmaps(mipmappedResult);

    frame->renderQueue << InsertBarrier(srcImage, RS_SHADER_RESOURCE);
}

#pragma endregion DeferredRenderer

} // namespace Hyperion
