/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/renderers/DeferredRenderer.hpp>
#include <rendering/renderers/EnvGridRenderer.hpp>
#include <rendering/renderers/EnvProbeRenderer.hpp>

#include <rendering/RenderGroup.hpp>
#include <rendering/ShaderManager.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/DepthPyramidRenderer.hpp>
#include <rendering/RenderMaterial.hpp>
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
#include <rendering/RenderBackend.hpp>
#include <rendering/Swapchain.hpp>
#include <rendering/RenderCollection.hpp>
#include <rendering/RenderProxyList.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/ConstantsAllocator.hpp>

#include <rendering/raytracing/RenderAccelerationStructure.hpp>
#include <rendering/raytracing/RenderRaytracingPipeline.hpp>
#include <rendering/raytracing/MeshBlasBuilder.hpp>
#include <rendering/raytracing/RaytracingReflections.hpp>
#include <rendering/raytracing/DDGI.hpp>

#include <rendering/util/ShaderCompiler.hpp>
#include <rendering/util/SafeDeleter.hpp>

#include <engine/DebugDrawer.hpp>

#include <scene/World.hpp>
#include <scene/View.hpp>
#include <scene/EnvGrid.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/FogVolume.hpp>
#include <scene/ParticleVolume.hpp>
#include <scene/LightmapVolume.hpp>

#include <core/config/Config.hpp>

#include <core/filesystem/FsUtil.hpp>

#include <core/utilities/DeferredScope.hpp>
#include <core/utilities/Float16.hpp>

#include <system/AppContext.hpp>

#include <util/MeshBuilder.hpp>

#include <engine/EngineDriver.hpp>
#include <engine/EngineStats.hpp>

#include <DeferredRenderer.generated.inl>

namespace Hyperion {

static constexpr float CameraJitterScale = 0.25f;

static constexpr TextureFormat EnvGridRadianceFormat = TF_RGBA8;
static constexpr TextureFormat EnvGridIrradianceFormat = TF_R11G11B10F;

static constexpr TextureFormat EnvGridPassFormats[EGPM_MAX] = {
    EnvGridRadianceFormat,  // EGPM_RADIANCE
    EnvGridIrradianceFormat // EGPM_IRRADIANCE
};

static const Float16 s_ltcMatrix[] = {
#include <rendering/inl/LTCMatrix.inl>
};

static_assert(sizeof(s_ltcMatrix) == 64 * 64 * 4 * 2, "Invalid LTC matrix size");

static const Float16 s_ltcBrdf[] = {
#include <rendering/inl/LTCBRDF.inl>
};

static_assert(sizeof(s_ltcBrdf) == 64 * 64 * 4 * 2, "Invalid LTC BRDF size");

// Maps individual light types to per-light specific properties.
static const FixedArray<ShaderProperties, LT_MAX> s_deferredLightTypeProperties {
    ShaderProperties { { ShaderProperty(NAME("LIGHT_TYPE"), NAME("DIRECTIONAL")) } },
    ShaderProperties { { ShaderProperty(NAME("LIGHT_TYPE"), NAME("POINT")) } },
    ShaderProperties { { ShaderProperty(NAME("LIGHT_TYPE"), NAME("SPOT")) } },
    ShaderProperties { { ShaderProperty(NAME("LIGHT_TYPE"), NAME("AREA_RECT")) } }
};

static constexpr StringHash GBufferTextureNames[GTN_MAX - 1] = {
    "GBufferAlbedoTexture"_sh,
    "GBufferNormalsTexture"_sh,
    "GBufferMaterialTexture"_sh,
    "GBufferVelocityTexture"_sh
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
    ShaderProperties& outShaderProperties,
    const RenderProxyList* rpl = nullptr,
    LightType lightType = LT_INVALID)
{
    static const GlobalConfig& s_globalConfig = CoreApi::GetGlobalConfig();
    static const IRenderConfig& s_renderConfig = g_renderBackend->GetRenderConfig();

    outShaderProperties.SetRequiredVertexAttributes(
        VertexAttribute::MESH_INPUT_ATTRIBUTE_POSITION
        | VertexAttribute::MESH_INPUT_ATTRIBUTE_NORMAL
        | VertexAttribute::MESH_INPUT_ATTRIBUTE_TEXCOORD0);

    MergeGlobalShaderProperties(outShaderProperties);

#define DEF_STATIC_CONFIGURATION_VALUE(name, path)                        \
    static const ConfigurationValue& s_##name = s_globalConfig.Get(path); \
    const bool name = s_##name.ToBool()

    DEF_STATIC_CONFIGURATION_VALUE(raytracingReflections, "Rendering.RayTracing.Reflections.Enabled");
    DEF_STATIC_CONFIGURATION_VALUE(raytracingGlobalIllumination, "Rendering.RayTracing.GI.Enabled");
    DEF_STATIC_CONFIGURATION_VALUE(envGridGlobalIllumination, "Rendering.EnvGrid.GI.Enabled");
    DEF_STATIC_CONFIGURATION_VALUE(envGridReflections, "Rendering.EnvGrid.Reflections.Enabled");
    DEF_STATIC_CONFIGURATION_VALUE(hbil, "Rendering.HBIL.Enabled");
    DEF_STATIC_CONFIGURATION_VALUE(hbao, "Rendering.HBAO.Enabled");
    DEF_STATIC_CONFIGURATION_VALUE(ssgi, "Rendering.SSGI.Enabled");
    DEF_STATIC_CONFIGURATION_VALUE(pathTracing, "Rendering.RayTracing.PathTracing.Enabled");

    DEF_STATIC_CONFIGURATION_VALUE(debugReflections, "Rendering.Debug.Reflections");
    DEF_STATIC_CONFIGURATION_VALUE(debugIrradiance, "Rendering.Debug.Irradiance");

#undef DEF_STATIC_CONFIGURATION_VALUE

    if (mode == DPM_INDIRECT_LIGHTING)
    {
        outShaderProperties.Set(NAME("RT_REFLECTIONS"), s_renderConfig.raytracing && raytracingReflections);
        outShaderProperties.Set(NAME("RT_GI"), s_renderConfig.raytracing && raytracingGlobalIllumination);
        outShaderProperties.Set(NAME("ENV_GRID_GI"), rpl && rpl->GetEnvGrids().NumCurrent() > 0 && envGridGlobalIllumination);
        outShaderProperties.Set(NAME("ENV_GRID_REFLECTIONS"), rpl && rpl->GetEnvGrids().NumCurrent() > 0 && envGridReflections);
        outShaderProperties.Set(NAME("HBIL_ENABLED"), hbil);
        outShaderProperties.Set(NAME("HBAO_ENABLED"), hbao);
        outShaderProperties.Set(NAME("SSGI_ENABLED"), ssgi);
    }

    if (s_renderConfig.raytracing && pathTracing)
    {
        outShaderProperties.Set(ShaderProperty(NAME("PATHTRACER")));
    }
    else if (debugReflections)
    {
        outShaderProperties.Set(ShaderProperty(NAME("DEBUG_REFLECTIONS")));
    }
    else if (debugIrradiance)
    {
        outShaderProperties.Set(ShaderProperty(NAME("DEBUG_IRRADIANCE")));
    }

    if (lightType != LT_INVALID)
    {
        outShaderProperties.Merge(s_deferredLightTypeProperties[uint32(lightType)]);
    }
}

static const TypeId s_envProbeTypeToTypeId[EPT_MAX] = {
    TypeId::ForType<SkyProbe>(),        // EPT_SKY
    TypeId::ForType<ReflectionProbe>(), // EPT_REFLECTION
    TypeId::ForType<EnvProbe>()         // EPT_AMBIENT (fixme when derived class)
};

#pragma region DeferredPass

static inline bool ShouldRecreatePipeline(
    const GraphicsPipelineCacheHandle& handle,
    const ShaderProperties& shaderProperties)
{
    if (!handle.IsAlive())
    {
        return true;
    }

    const GraphicsPipelineRef& pipeline = *handle;
    Assert(pipeline.IsValid());

    if (pipeline->GetShader()->GetCompiledShader()->GetProperties().GetHashCode() != shaderProperties.GetHashCode())
    {
        HYP_LOG(Rendering, Debug, "Recreating graphics pipeline due to shader property mismatch:\n\nGot:\n{}\n\nExpected:\n{}\n",
            pipeline->GetShader()->GetCompiledShader()->GetProperties().ToString(),
            shaderProperties.ToString());

        return true;
    }

    return false;
}

DeferredPass::DeferredPass(DeferredPassMode mode, Vec2u extent, GBuffer* gbuffer, const FramebufferRef& framebuffer)
    : FullScreenPass(nullptr, nullptr, framebuffer, TF_RGBA16F, extent, gbuffer, FSP_EXTERNAL_RENDERTARGET),
      m_mode(mode),
      m_directLightGraphicsPipelines()
{
    Assert(m_framebuffer.IsValid());

    if (mode == DPM_DIRECT_LIGHTING)
    {
        SetBlendFunction(BlendFunction::Additive());
    }
}

DeferredPass::~DeferredPass()
{
    SafeDelete(std::move(m_ltcSampler));
}

void DeferredPass::Create()
{
    AssertOnThread(g_renderThread);

    FullScreenPass::Create();

    // linear transform cosines texture data
    if (m_mode == DPM_DIRECT_LIGHTING && !m_ltcSampler)
    {
        m_ltcSampler = g_renderBackend->MakeSampler(
            TFM_NEAREST,
            TFM_LINEAR,
            TWM_CLAMP_TO_EDGE);

        Assert(m_ltcSampler->Create());

        ByteBuffer ltcMatrixData(sizeof(s_ltcMatrix), s_ltcMatrix);

        m_ltcMatrixTexture = CreateObject<Texture>(
            TextureDesc {
                TT_TEX2D,
                TF_RGBA16F,
                Vec3u { 64, 64, 1 },
                TFM_LINEAR,
                TFM_LINEAR,
                TWM_CLAMP_TO_EDGE },
            TextureData { std::move(ltcMatrixData) });
        m_ltcMatrixTexture->SetName(NAME("LTC_Matrix"));
        InitObject(m_ltcMatrixTexture);

        ByteBuffer ltcBrdfData(sizeof(s_ltcBrdf), s_ltcBrdf);

        m_ltcBrdfTexture = CreateObject<Texture>(
            TextureDesc {
                TT_TEX2D,
                TF_RGBA16F,
                Vec3u { 64, 64, 1 },
                TFM_LINEAR,
                TFM_LINEAR,
                TWM_CLAMP_TO_EDGE },
            TextureData { std::move(ltcBrdfData) });

        m_ltcBrdfTexture->SetName(NAME("LTC_BRDF"));
        InitObject(m_ltcBrdfTexture);
    }
}

GraphicsPipelineCacheHandle DeferredPass::CreatePipeline(const ShaderProperties& shaderProperties)
{
    HYP_SCOPE;

    AssertDebug(m_framebuffer.IsValid());

    const MeshAttributes meshAttributes {
        .vertexAttributes = shaderProperties.GetRequiredVertexAttributes()
    };

    const MaterialAttributes materialAttributes {
        .fillMode = FM_FILL,
        .blendFunction = m_blendFunction,
        .flags = MAF_STENCIL_TEST,
        .stencilFunction = StencilFunction {
            .passOp = SO_KEEP,
            .failOp = SO_KEEP,
            .depthFailOp = SO_KEEP,
            .compareOp = SCO_EQUAL }
    };

    const RenderableAttributeSet renderableAttributes { meshAttributes, materialAttributes };
    
    RenderTargetDesc rtDesc = m_framebuffer->GetRenderTargetDesc();

    if (m_mode == DPM_INDIRECT_LIGHTING)
    {
        m_shader = g_shaderManager->GetOrCreate(NAME("DeferredIndirect"), shaderProperties);
        Assert(m_shader.IsValid());

        return g_renderInterface->graphicsPipelineCache->GetOrCreate(
            m_shader,
            &rtDesc,
            renderableAttributes);
    }

    ShaderRef shader = g_shaderManager->GetOrCreate(NAME("DeferredDirect"), shaderProperties);
    Assert(shader != nullptr);

    return g_renderInterface->graphicsPipelineCache->GetOrCreate(
        shader,
        &rtDesc,
        renderableAttributes);
}

void DeferredPass::Resize_Internal(Vec2u newSize)
{
    FullScreenPass::Resize_Internal(newSize);

    m_directLightGraphicsPipelines = {};
}

void DeferredPass::RenderToFramebuffer_Internal(Frame* frame, const RenderSetup& rs, Framebuffer* framebuffer)
{
    HYP_SCOPE;
    ENGINE_STAT_SCOPE(&s_deferredPassTimer);

    AssertDebug(rs.world && rs.view);
    AssertDebug(rs.passData != nullptr);

    const Viewport& viewport = rs.view->GetViewport();

    RenderProxyList& rpl = RenderApi::GetConsumerProxyList(rs.view);
    rpl.BeginRead();
    HYP_DEFER({ rpl.EndRead(); });

    ENGINE_STAT_SCOPE(
        m_mode == DPM_DIRECT_LIGHTING
            ? &s_deferredDirectLightingTimer
            : &s_deferredIndirectLightingTimer);

    switch (m_mode)
    {
    case DPM_DIRECT_LIGHTING:
        // no lights bound, do not render direct shading at all
        if (rpl.GetLights().NumCurrent() == 0)
        {
            return;
        }

        break;
    case DPM_INDIRECT_LIGHTING:
    {
        // stencil state: only render where stencil == 0 (non-lightmapped geometry)
        frame->renderQueue << SetStencilState(0, 0xFF, 0x0);

        // check needs invalidation
        ShaderProperties shaderProperties;
        GetDeferredShaderProperties(m_mode, shaderProperties, &rpl);

        if (ShouldRecreatePipeline(m_graphicsPipelineCacheHandle, shaderProperties))
        {
            m_graphicsPipelineCacheHandle = CreatePipeline(shaderProperties);
            AssertDebug(!ShouldRecreatePipeline(m_graphicsPipelineCacheHandle, shaderProperties));
        }

        FullScreenPass::RenderToFramebuffer_Internal(frame, rs, framebuffer);

        // reset stencil state
        frame->renderQueue << SetStencilState(0, 0xFF, 0xFF);

        return;
    }
    default:
        HYP_UNREACHABLE();
        return;
    }

    // stencil state: only render where stencil == 0 (non-lightmapped geometry)
    frame->renderQueue << SetStencilState(0, 0xFF, 0x0);

    const bool useBindlessTextures = g_renderBackend->GetRenderConfig().bindlessTextures;

    // last LightType we rendered
    LightType prevLightType = LT_INVALID;

    // render with each light
    for (uint32 lightTypeIndex = 0; lightTypeIndex < LT_MAX; lightTypeIndex++)
    {
        const LightType lightType = LightType(lightTypeIndex);

        DescriptorSetRef& directPassDescriptorSet = m_directPassDescriptorSets[lightTypeIndex][frame->GetFrameIndex()];

        for (Light* light : rpl.GetLights())
        {
            if (light->GetLightType() != lightTypeIndex)
            {
                continue;
            }

            bool pipelineChanged = false;

            if (lightType != prevLightType)
            {
                ShaderProperties shaderProperties;
                GetDeferredShaderProperties(m_mode, shaderProperties, &rpl, lightType);

                if (ShouldRecreatePipeline(m_directLightGraphicsPipelines[lightTypeIndex], shaderProperties))
                {
                    m_directLightGraphicsPipelines[lightTypeIndex] = CreatePipeline(shaderProperties);

                    AssertDebug(!ShouldRecreatePipeline(m_directLightGraphicsPipelines[lightTypeIndex], shaderProperties));
                }

                pipelineChanged = true;
            }

            const GraphicsPipelineRef& pipeline = *m_directLightGraphicsPipelines[lightTypeIndex];

            if (pipelineChanged && !directPassDescriptorSet.IsValid())
            {
                // create direct pass descriptor set
                const DescriptorTableDeclaration* descriptorTableDecl = pipeline->GetShader()->GetCompiledShader()->GetDescriptorTableDeclaration();
                AssertDebug(descriptorTableDecl != nullptr);

                const DescriptorSetDeclaration* descriptorSetDecl = descriptorTableDecl->FindDescriptorSetDeclaration("DeferredDirectDescriptorSet"_sh);
                AssertDebug(descriptorSetDecl != nullptr);

                directPassDescriptorSet = g_renderBackend->MakeDescriptorSet(DescriptorSetLayout(descriptorSetDecl));
                Assert(directPassDescriptorSet.IsValid());

                directPassDescriptorSet->SetElement("MaterialsBuffer"_sh, g_renderInterface->gpuBuffers[GRB_MATERIALS]->GetBuffer(frame->GetFrameIndex()));
                directPassDescriptorSet->SetElement("LTCSampler"_sh, m_ltcSampler);
                directPassDescriptorSet->SetElement("LTCMatrixTexture"_sh, g_renderBackend->GetTextureImageView(m_ltcMatrixTexture));
                directPassDescriptorSet->SetElement("LTCBRDFTexture"_sh, g_renderBackend->GetTextureImageView(m_ltcBrdfTexture));

                Assert(directPassDescriptorSet->Create());
            }

            const uint32 globalDescriptorSetIndex = pipeline->GetDescriptorSetIndex("Global"_sh);
            const uint32 viewDescriptorSetIndex = pipeline->GetDescriptorSetIndex("View"_sh);
            const uint32 materialDescriptorSetIndex = lightType == LT_AREA_RECT
                ? pipeline->GetDescriptorSetIndex("Material"_sh)
                : ~0u;

            const uint32 deferredDirectDescriptorSetIndex = pipeline->GetDescriptorSetIndex("DeferredDirectDescriptorSet"_sh);

            if (pipelineChanged)
            {
                pipeline->SetPushConstants(m_pushConstantData.Data(), m_pushConstantData.Size());

                frame->renderQueue << BindGraphicsPipeline(pipeline, viewport);

                // Bind textures globally (bindless)
                if (materialDescriptorSetIndex != ~0u && useBindlessTextures)
                {
                    frame->renderQueue << BindDescriptorSet(
                        g_renderInterface->globalDescriptorTable->GetDescriptorSet("Material"_sh, frame->GetFrameIndex()),
                        pipeline,
                        {},
                        materialDescriptorSetIndex);
                }

                if (deferredDirectDescriptorSetIndex != ~0u)
                {
                    frame->renderQueue << BindDescriptorSet(
                        directPassDescriptorSet,
                        pipeline,
                        {},
                        deferredDirectDescriptorSetIndex);
                }

                frame->renderQueue << BindVertexBuffer(m_fullScreenQuad->GetVertexBuffer());
                frame->renderQueue << BindIndexBuffer(m_fullScreenQuad->GetIndexBuffer());
            }

            frame->renderQueue << BindDescriptorSet(
                g_renderInterface->globalDescriptorTable->GetDescriptorSet("Global"_sh, frame->GetFrameIndex()),
                pipeline,
                { { "CamerasBuffer"_sh, ShaderDataOffset<CameraShaderData>(rs.view->GetCamera()) },
                    { "CurrentLight"_sh, ShaderDataOffset<LightShaderData>(light, 0) } },
                globalDescriptorSetIndex);

            frame->renderQueue << BindDescriptorSet(
                rs.passData->descriptorSets[frame->GetFrameIndex()],
                pipeline,
                {},
                viewDescriptorSetIndex);

            // Bind material descriptor set (for area lights)
            if (materialDescriptorSetIndex != ~0u && !useBindlessTextures)
            {
                const DescriptorSetRef& materialDescriptorSet = g_renderInterface->materialDescriptorSetManager->ForBoundMaterial(light->GetMaterial(), frame->GetFrameIndex());

                frame->renderQueue << BindDescriptorSet(
                    materialDescriptorSet,
                    pipeline,
                    {},
                    materialDescriptorSetIndex);
            }

            frame->renderQueue << DrawIndexed(6);

            prevLightType = lightType;
        }
    }

    // reset stencil state
    frame->renderQueue << SetStencilState(0, 0xFF, 0xFF);
}

#pragma endregion DeferredPass

#pragma region TonemapPass

TonemapPass::TonemapPass(Vec2u extent, GBuffer* gbuffer)
    : FullScreenPass(TF_R11G11B10F, extent, gbuffer)
{
}

TonemapPass::~TonemapPass()
{
}

void TonemapPass::Create()
{
    AssertOnThread(g_renderThread);

    FullScreenPass::Create();
}

void TonemapPass::CreatePipeline()
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    const MeshAttributes meshAttributes {
        VertexAttribute::MESH_INPUT_ATTRIBUTE_POSITION
        | VertexAttribute::MESH_INPUT_ATTRIBUTE_NORMAL
        | VertexAttribute::MESH_INPUT_ATTRIBUTE_TEXCOORD0
    };

    const MaterialAttributes materialAttributes {
        .fillMode = FM_FILL,
        .blendFunction = BlendFunction::None(),
        .flags = MAF_NONE
    };

    const RenderableAttributeSet renderableAttributes(
        meshAttributes,
        materialAttributes);

    ShaderProperties shaderProperties;

    /*if (g_renderBackend->GetSwapchain()->IsPqHdr())
    {
        shaderProperties.Set(ShaderProperty(NAME("OUTPUT"), NAME("PQ_HDR")));
    }
    else
    {*/
    shaderProperties.Set(ShaderProperty(NAME("OUTPUT"), NAME("SDR")));
    //}

    m_shader = g_shaderManager->GetOrCreate(NAME("Tonemap"), shaderProperties);

    FullScreenPass::CreatePipeline(renderableAttributes);
}

void TonemapPass::Resize_Internal(Vec2u newSize)
{
    FullScreenPass::Resize_Internal(newSize);
}

void TonemapPass::Render(Frame* frame, const RenderSetup& rs)
{
    FullScreenPass::Render(frame, rs);
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
    : FullScreenPass(TF_RGBA16F, nullptr, FSP_EXTERNAL_RENDERTARGET)
{
}

LightmapPass::~LightmapPass()
{
    for (auto& data : m_lightmapVolumePassData)
    {
        SafeDelete(std::move(data.descriptorSets));
    }
}

void LightmapPass::Create()
{
    AssertOnThread(g_renderThread);

    m_shader = g_shaderManager->GetOrCreate(NAME("ApplyLightmap"));
    Assert(m_shader != nullptr);

    FullScreenPass::Create();
}

const GraphicsPipelineRef& LightmapPass::GetGraphicsPipeline(Framebuffer* framebuffer, LightmapVolumePassData& data)
{
    LightmapVolume* volume = data.volume;
    AssertDebug(volume != nullptr);

    RenderProxyLightmapVolume* proxy = static_cast<RenderProxyLightmapVolume*>(RenderApi::GetRenderProxy(volume));
    Assert(proxy != nullptr);

    if (data.graphicsPipeline.IsAlive())
    {
        const GraphicsPipelineRef& graphicsPipeline = *data.graphicsPipeline;
        AssertDebug(graphicsPipeline != nullptr);

        if (graphicsPipeline->GetFramebuffers().Contains(framebuffer)
            && proxy->atlasIrradianceTextures.CompareBitwise(data.atlasIrradianceTextures)
            && proxy->atlasRadianceTextures.CompareBitwise(data.atlasRadianceTextures))
        {
            return graphicsPipeline;
        }
    }

    const MeshAttributes meshAttributes {
        VertexAttribute::MESH_INPUT_ATTRIBUTE_POSITION
        | VertexAttribute::MESH_INPUT_ATTRIBUTE_NORMAL
        | VertexAttribute::MESH_INPUT_ATTRIBUTE_TEXCOORD0
    };

    MaterialAttributes materialAttributes;
    materialAttributes.fillMode = FM_FILL;
    materialAttributes.blendFunction = BlendFunction(
        BMF_SRC_ALPHA, BMF_ONE_MINUS_SRC_ALPHA,
        BMF_ONE, BMF_ONE_MINUS_SRC_ALPHA);
    materialAttributes.flags = MAF_STENCIL_TEST;
    materialAttributes.stencilFunction = StencilFunction {
        .passOp = SO_KEEP,
        .failOp = SO_KEEP,
        .depthFailOp = SO_KEEP,
        .compareOp = SCO_EQUAL // match values with equal atlas index when we render
    };

    SafeDelete(std::move(data.descriptorSets));

    for (uint32 atlasIndex = 0; atlasIndex < proxy->numAtlases; atlasIndex++)
    {
        const DescriptorSetDeclaration* decl = m_shader->GetCompiledShader()->GetDescriptorTableDeclaration()->FindDescriptorSetDeclaration("LightmapVolume"_sh);
        Assert(decl != nullptr);

        const DescriptorSetLayout layout { decl };

        DescriptorSetRef& descriptorSet = data.descriptorSets.PushBack(g_renderBackend->MakeDescriptorSet(layout));

        Texture* irradianceTexture = proxy->atlasIrradianceTextures[atlasIndex];
        Texture* radianceTexture = proxy->atlasRadianceTextures[atlasIndex];

        LightmapVolumeUniforms uniforms {};
        uniforms.numAtlases = proxy->numAtlases;
        uniforms.irradianceWeight = irradianceTexture ? 1.0f : 0.0f;
        uniforms.radianceWeight = radianceTexture ? 1.0f : 0.0f;

        GpuBufferRef uniformBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::CBUFF, sizeof(LightmapVolumeUniforms));
        Assert(uniformBuffer->Create());
        uniformBuffer->Copy(sizeof(uniforms), &uniforms);

        descriptorSet->SetElement("IrradianceTexture"_sh, g_renderBackend->GetTextureImageView(irradianceTexture != nullptr ? MakeStrongRef(irradianceTexture) : g_renderInterface->placeholderData->defaultTexture2d));
        descriptorSet->SetElement("RadianceTexture"_sh, g_renderBackend->GetTextureImageView(radianceTexture != nullptr ? MakeStrongRef(radianceTexture) : g_renderInterface->placeholderData->defaultTexture2d));
        descriptorSet->SetElement("Sampler"_sh, g_renderInterface->placeholderData->GetSamplerLinear());
        descriptorSet->SetElement("GBufferSampler"_sh, g_renderInterface->placeholderData->GetSamplerNearest());
        descriptorSet->SetElement("LightmapVolumeUniforms"_sh, uniformBuffer);

        Assert(descriptorSet->Create());
    }

    RenderTargetDesc rtDesc = framebuffer->GetRenderTargetDesc();

    data.graphicsPipeline = g_renderInterface->graphicsPipelineCache->GetOrCreate(
        m_shader,
        &rtDesc,
        RenderableAttributeSet(meshAttributes, materialAttributes));

    data.atlasIrradianceTextures = proxy->atlasIrradianceTextures;
    data.atlasRadianceTextures = proxy->atlasRadianceTextures;

    return *data.graphicsPipeline;
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

    LightmapVolume* volume = ObjCast<LightmapVolume>(renderSetup.volume);
    AssertDebug(volume != nullptr);

    RenderProxyLightmapVolume* proxy = static_cast<RenderProxyLightmapVolume*>(RenderApi::GetRenderProxy(volume));
    Assert(proxy != nullptr);

    if (proxy->numAtlases == 0)
    {
        return; // nothing to do
    }

    LightmapVolumePassData& data = GetLightmapVolumePassData(volume);
    //// \todo : Add clean up of data after lightmap volume has been removed

    const GraphicsPipelineRef& graphicsPipeline = GetGraphicsPipeline(framebuffer, data);

    const uint32 globalDescriptorSetIndex = graphicsPipeline->GetDescriptorSetIndex("Global"_sh);
    const uint32 viewDescriptorSetIndex = graphicsPipeline->GetDescriptorSetIndex("View"_sh);
    const uint32 lightmapVolumeDescriptorSetIndex = graphicsPipeline->GetDescriptorSetIndex("LightmapVolume"_sh);

    for (uint32 atlasIndex = 0; atlasIndex < proxy->numAtlases; atlasIndex++)
    {
        // only draw elems in the volume with a stencil reference of the atlas index (+1)
        frame->renderQueue << SetStencilState(atlasIndex + 1, LightmapStencilMask, 0x0);

        frame->renderQueue << BindGraphicsPipeline(graphicsPipeline, Viewport { framebuffer->GetExtent() });

        frame->renderQueue << BindDescriptorSet(
            g_renderInterface->globalDescriptorTable->GetDescriptorSet("Global"_sh, frame->GetFrameIndex()),
            graphicsPipeline,
            { { "CamerasBuffer"_sh, ShaderDataOffset<CameraShaderData>(renderSetup.view->GetCamera()) } },
            globalDescriptorSetIndex);

        if (viewDescriptorSetIndex != ~0u)
        {
            AssertDebug(renderSetup.passData != nullptr);

            frame->renderQueue << BindDescriptorSet(
                renderSetup.passData->descriptorSets[frame->GetFrameIndex()],
                graphicsPipeline,
                {},
                viewDescriptorSetIndex);
        }

        frame->renderQueue << BindDescriptorSet(
            data.descriptorSets[atlasIndex],
            graphicsPipeline,
            {},
            lightmapVolumeDescriptorSetIndex);

        frame->renderQueue << BindVertexBuffer(m_fullScreenQuad->GetVertexBuffer());
        frame->renderQueue << BindIndexBuffer(m_fullScreenQuad->GetIndexBuffer());
        frame->renderQueue << DrawIndexed(6); /// \todo : Draw a box transformed to the size of the lightmap volume
    }

    // reset stencil state back to default
    frame->renderQueue << SetStencilState(0, 0xFF, 0xFF);

    m_isFirstFrame = false;
}

#pragma endregion LightmapPass

#pragma region FogVolumePass

static constexpr uint32 MaxBoundLightsPerFogVolume = 16;

FogVolumePass::FogVolumePass()
    : FullScreenPass(TF_RGBA16F, nullptr, FSP_EXTERNAL_RENDERTARGET)
{
}

FogVolumePass::~FogVolumePass()
{
    for (FogVolumePassData& data : m_fogVolumePassData)
    {
        SafeDelete(std::move(data.descriptorTable));
        SafeDelete(std::move(data.cBuffer));
    }
}

void FogVolumePass::Create()
{
    AssertOnThread(g_renderThread);

    m_volumeMesh = MeshBuilder::Cube(true);
    m_volumeMesh->SetFlags(MF_VIEW_INDEPENDENT);
    m_volumeMesh->SetName(NAME("FogVolumeMesh"));
    InitObject(m_volumeMesh);

    ShaderProperties shaderProperties(m_volumeMesh->GetVertexAttributes());
    shaderProperties.Set(ShaderProperty(NAME("MAX_LIGHTS"), int(MaxBoundLightsPerFogVolume)));

    m_shader = g_shaderManager->GetOrCreate(NAME("ApplyFogVolume"), shaderProperties);
    Assert(m_shader != nullptr);

    FullScreenPass::Create();
}

const GraphicsPipelineRef& FogVolumePass::GetGraphicsPipeline(Framebuffer* framebuffer, FogVolumePassData& data)
{
    FogVolume* volume = data.volume;
    AssertDebug(volume != nullptr);

    RenderProxyFogVolume* proxy = static_cast<RenderProxyFogVolume*>(RenderApi::GetRenderProxy(volume));
    Assert(proxy != nullptr);

    if (data.graphicsPipeline.IsAlive())
    {
        const GraphicsPipelineRef& graphicsPipeline = *data.graphicsPipeline;
        AssertDebug(graphicsPipeline != nullptr);

        return graphicsPipeline;
    }

    const MeshAttributes meshAttributes = m_volumeMesh->GetMeshAttributes();

    MaterialAttributes materialAttributes;
    materialAttributes.fillMode = FM_FILL;
    materialAttributes.flags = MAF_NONE;
    materialAttributes.bucket = RB_TRANSLUCENT;
    materialAttributes.cullFaces = FCM_FRONT; // cull front faces to render inside of the volume
    // blending for fog volumes: src: src_alpha, dst: 1 - src_alpha
    materialAttributes.blendFunction = BlendFunction(
        BMF_SRC_ALPHA, BMF_ONE_MINUS_SRC_ALPHA,
        BMF_ONE, BMF_ONE_MINUS_SRC_ALPHA);

    RenderableAttributeSet renderableAttributes { meshAttributes, materialAttributes };

    if (!data.cBuffer)
    {
        data.cBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::CBUFF, sizeof(FogVolumeShaderData) + sizeof(LightShaderData) * MaxBoundLightsPerFogVolume);
        Assert(data.cBuffer->Create());
    }

    DescriptorTableRef descriptorTable = g_renderBackend->MakeDescriptorTable(m_shader->GetCompiledShader()->GetDescriptorTableDeclaration());

#ifdef HYP_DEBUG_MODE
    descriptorTable->SetDebugName(NAME_FMT("DescriptorTable_{}_{}", m_shader->GetCompiledShader()->GetName(), volume->GetName()));
#endif

    for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
    {
        const DescriptorSetRef& descriptorSet = descriptorTable->GetDescriptorSet("FogVolume"_sh, frameIndex);
        Assert(descriptorSet != nullptr);

        descriptorSet->SetElement("DataMap"_sh, proxy->volumeTexture != nullptr ? g_renderBackend->GetTextureImageView(MakeStrongRef(proxy->volumeTexture)) : g_renderBackend->GetTextureImageView(g_renderInterface->placeholderData->defaultTexture3d));
        descriptorSet->SetElement("NoiseMap"_sh, proxy->noiseTexture != nullptr ? g_renderBackend->GetTextureImageView(MakeStrongRef(proxy->noiseTexture)) : g_renderBackend->GetTextureImageView(g_renderInterface->placeholderData->defaultTexture3d));
        descriptorSet->SetElement("FogVolumeUniforms"_sh, data.cBuffer);
    }

    data.volumeTexture = proxy->volumeTexture;
    data.noiseTexture = proxy->noiseTexture;

    Assert(descriptorTable->Create());

    /// \todo Don't throw away old descriptor table if only uniforms changed!
    if (data.descriptorTable)
    {
        SafeDelete(std::move(data.descriptorTable));
    }

    data.descriptorTable = descriptorTable;
    
    RenderTargetDesc rtDesc = framebuffer->GetRenderTargetDesc();

    data.graphicsPipeline = g_renderInterface->graphicsPipelineCache->GetOrCreate(
        m_shader,
        &rtDesc,
        renderableAttributes);

    return *data.graphicsPipeline;
}

void FogVolumePass::Resize_Internal(Vec2u newSize)
{
    FullScreenPass::Resize_Internal(newSize);
}

void FogVolumePass::RenderToFramebuffer_Internal(Frame* frame, const RenderSetup& renderSetup, Framebuffer* framebuffer)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.world && renderSetup.volume);

    FogVolume* volume = ObjCast<FogVolume>(renderSetup.volume);
    AssertDebug(volume != nullptr);

    RenderProxyFogVolume* proxy = static_cast<RenderProxyFogVolume*>(RenderApi::GetRenderProxy(volume));
    Assert(proxy != nullptr);

    FogVolumePassData& data = GetFogVolumePassData(volume);

    if (proxy->forceRebind
        || proxy->volumeTexture != data.volumeTexture
        || proxy->noiseTexture != data.noiseTexture)
    {
        // force graphics pipeline re-creation
        data.graphicsPipeline = {};
    }

    const GraphicsPipelineRef& graphicsPipeline = GetGraphicsPipeline(framebuffer, data);

    UpdateUniforms(frame, renderSetup, data);

    frame->renderQueue << BindGraphicsPipeline(graphicsPipeline, Viewport { framebuffer->GetExtent() });

    frame->renderQueue << BindDescriptorTable(
        data.descriptorTable,
        graphicsPipeline,
        { { "Global"_sh, { { "CamerasBuffer"_sh, ShaderDataOffset<CameraShaderData>(renderSetup.view ? renderSetup.view->GetCamera() : nullptr, 0) } } } },
        frame->GetFrameIndex());

    const uint32 viewDescriptorSetIndex = m_shader->GetCompiledShader()->GetDescriptorTableDeclaration()->GetDescriptorSetIndex("View"_sh);

    if (viewDescriptorSetIndex != ~0u)
    {
        AssertDebug(renderSetup.passData != nullptr);

        frame->renderQueue << BindDescriptorSet(
            renderSetup.passData->descriptorSets[frame->GetFrameIndex()],
            graphicsPipeline,
            {},
            viewDescriptorSetIndex);
    }

    frame->renderQueue << BindVertexBuffer(m_volumeMesh->GetVertexBuffer());
    frame->renderQueue << BindIndexBuffer(m_volumeMesh->GetIndexBuffer());
    frame->renderQueue << DrawIndexed(36); // draw cube

    m_isFirstFrame = false;
}

void FogVolumePass::UpdateUniforms(Frame* frame, const RenderSetup& renderSetup, FogVolumePassData& data)
{
    HYP_SCOPE;

    AssertDebug(renderSetup.world && renderSetup.view);

    RenderProxyList& rpl = RenderApi::GetConsumerProxyList(renderSetup.view);

    GpuBufferBase* cBuffer = data.cBuffer;
    AssertDebug(cBuffer != nullptr);

    RenderProxyFogVolume* proxy = static_cast<RenderProxyFogVolume*>(RenderApi::GetRenderProxy(data.volume));
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

        RenderProxyLight* lightProxy = static_cast<RenderProxyLight*>(RenderApi::GetRenderProxy(light));
        Assert(lightProxy != nullptr);

        cBuffer->Copy(sizeof(FogVolumeShaderData) + (numBoundLights * sizeof(LightShaderData)), sizeof(LightShaderData), &lightProxy->bufferData);

        lightIndicesU32[numBoundLights++] = RenderApi::RetrieveResourceBinding(light);
    }

    cBuffer->Copy(sizeof(FogVolumeShaderData), &shaderData);
    cBuffer->Flush(0, sizeof(FogVolumeShaderData) + (numBoundLights * sizeof(LightShaderData)));
}

#pragma endregion FogVolumePass

#pragma region EnvGridPass

static inline EnvGridApplyMode EnvGridTypeToApplyMode(EnvGridType type)
{
    switch (type)
    {
    case EnvGridType::ENV_GRID_TYPE_SH:
        return EGAM_SH;
    case EnvGridType::ENV_GRID_TYPE_VOXEL:
        return EGAM_VOXEL;
    case EnvGridType::ENV_GRID_TYPE_LIGHT_FIELD:
        return EGAM_LIGHT_FIELD;
    default:
        HYP_UNREACHABLE();
    }
}

EnvGridPass::EnvGridPass(EnvGridPassMode mode, Vec2u extent, GBuffer* gbuffer)
    : FullScreenPass(EnvGridPassFormats[mode], extent, gbuffer),
      m_mode(mode),
      m_isFirstFrame(true)
{
    if (mode == EGPM_RADIANCE)
    {
        SetBlendFunction(BlendFunction(
            BMF_SRC_ALPHA, BMF_ONE_MINUS_SRC_ALPHA,
            BMF_ONE, BMF_ONE_MINUS_SRC_ALPHA));
    }
}

EnvGridPass::~EnvGridPass()
{
}

void EnvGridPass::Create()
{
    AssertOnThread(g_renderThread);

    FullScreenPass::Create();
}

void EnvGridPass::CreatePipeline()
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    const MeshAttributes meshAttributes {
        VertexAttribute::MESH_INPUT_ATTRIBUTE_POSITION
        | VertexAttribute::MESH_INPUT_ATTRIBUTE_NORMAL
        | VertexAttribute::MESH_INPUT_ATTRIBUTE_TEXCOORD0
    };

    const MaterialAttributes materialAttributes {
        .fillMode = FM_FILL,
        .blendFunction = BlendFunction(
            BMF_SRC_ALPHA, BMF_ONE_MINUS_SRC_ALPHA,
            BMF_ONE, BMF_ONE_MINUS_SRC_ALPHA),
        .flags = MAF_NONE
    };

    const RenderableAttributeSet renderableAttributes(
        meshAttributes,
        materialAttributes);

    if (m_mode == EGPM_RADIANCE)
    {
        m_shader = g_shaderManager->GetOrCreate(NAME("ApplyEnvGrid"), ShaderProperties { { NAME("MODE_RADIANCE") } });

        FullScreenPass::CreatePipeline(renderableAttributes);

        return;
    }

    static const FixedArray<ShaderProperties, EGAM_MAX> s_applyEnvGridPasses = {
        ShaderProperties { { ShaderProperty(NAME("MODE"), NAME("IRRADIANCE")), ShaderProperty(NAME("IRRADIANCE_MODE"), NAME("SH")) } },         // EGAM_SH
        ShaderProperties { { ShaderProperty(NAME("MODE"), NAME("IRRADIANCE")), ShaderProperty(NAME("IRRADIANCE_MODE"), NAME("VOXEL")) } },      // EGAM_VOXEL
        ShaderProperties { { ShaderProperty(NAME("MODE"), NAME("IRRADIANCE")), ShaderProperty(NAME("IRRADIANCE_MODE"), NAME("LIGHT_FIELD")) } } // EGAM_LIGHT_FIELD
    };

    for (uint32 passMode = 0; passMode < EGAM_MAX; passMode++)
    {
        const ShaderProperties& passProperties = s_applyEnvGridPasses[passMode];

        ShaderRef shader = g_shaderManager->GetOrCreate(NAME("ApplyEnvGrid"), passProperties);
        Assert(shader.IsValid());

        DescriptorTableRef descriptorTable = g_renderBackend->MakeDescriptorTable(
            shader->GetCompiledShader()->GetDescriptorTableDeclaration());

        Assert(descriptorTable->Create());
        
        RenderTargetDesc rtDesc = m_framebuffer->GetRenderTargetDesc();

        GraphicsPipelineCacheHandle cacheHandle = g_renderInterface->graphicsPipelineCache->GetOrCreate(
            shader,
            &rtDesc,
            renderableAttributes);

        m_graphicsPipelines[passMode] = std::move(cacheHandle);
    }

    m_graphicsPipelineCacheHandle = m_graphicsPipelines[EGAM_SH];
}

void EnvGridPass::Resize_Internal(Vec2u newSize)
{
    FullScreenPass::Resize_Internal(newSize);

    m_graphicsPipelines = {};
}

void EnvGridPass::Render(Frame* frame, const RenderSetup& rs)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    AssertDebug(rs.world && rs.view);
    AssertDebug(rs.passData != nullptr);

    const Viewport& viewport = rs.view->GetViewport();

    RenderProxyList& rpl = RenderApi::GetConsumerProxyList(rs.view);
    rpl.BeginRead();

    HYP_DEFER({ rpl.EndRead(); });

    // shouldn't be called if no env grids are present
    AssertDebug(rpl.GetEnvGrids().NumCurrent() != 0);

    const uint32 frameIndex = frame->GetFrameIndex();

    frame->renderQueue << BeginFramebuffer(m_framebuffer);

    // render previous frame's result to screen
    if (!m_isFirstFrame && m_renderTextureToScreenPass != nullptr)
    {
        RenderPreviousTextureToScreen(frame, rs);
    }

    auto selectPipeline = [this](LegacyEnvGrid* envGrid) -> GraphicsPipelineCacheHandle&
    {
        return m_mode == EGPM_RADIANCE
            ? m_graphicsPipelineCacheHandle
            : m_graphicsPipelines[EnvGridTypeToApplyMode(envGrid->GetEnvGridType())];
    };

    for (EnvGrid* envGrid : rpl.GetEnvGrids().GetElements<LegacyEnvGrid>())
    {
        LegacyEnvGrid* legacyEnvGrid = static_cast<LegacyEnvGrid*>(envGrid);

        GraphicsPipelineCacheHandle& cacheHandle = selectPipeline(legacyEnvGrid);

        if (!cacheHandle.IsAlive())
        {
            CreatePipeline();

            cacheHandle = selectPipeline(legacyEnvGrid);
            Assert(cacheHandle.IsAlive());
        }

        const GraphicsPipelineRef& graphicsPipeline = *cacheHandle;

        const uint32 globalDescriptorSetIndex = graphicsPipeline->GetDescriptorSetIndex("Global"_sh);
        const uint32 viewDescriptorSetIndex = graphicsPipeline->GetDescriptorSetIndex("View"_sh);

        graphicsPipeline->SetPushConstants(m_pushConstantData.Data(), m_pushConstantData.Size());

        if (ShouldRenderHalfRes())
        {
            const Vec2i viewportOffset = (Vec2i(m_framebuffer->GetExtent().x, 0) / 2) * (RenderApi::GetWorldBufferData()->frameCounter & 1);
            const Vec2u viewportExtent = Vec2u(m_framebuffer->GetExtent().x / 2, m_framebuffer->GetExtent().y);

            frame->renderQueue << BindGraphicsPipeline(graphicsPipeline, Viewport { viewportExtent, viewportOffset });
        }
        else
        {
            frame->renderQueue << BindGraphicsPipeline(graphicsPipeline, viewport);
        }

        frame->renderQueue << BindDescriptorSet(
            g_renderInterface->globalDescriptorTable->GetDescriptorSet("Global"_sh, frameIndex),
            graphicsPipeline,
            { { "CamerasBuffer"_sh, ShaderDataOffset<CameraShaderData>(rs.view->GetCamera()) },
                { "EnvGridsBuffer"_sh, ShaderDataOffset<EnvGridShaderData>(envGrid, 0) } },
            globalDescriptorSetIndex);

        frame->renderQueue << BindDescriptorSet(
            rs.passData->descriptorSets[frameIndex],
            graphicsPipeline,
            {},
            viewDescriptorSetIndex);

        frame->renderQueue << BindVertexBuffer(m_fullScreenQuad->GetVertexBuffer());
        frame->renderQueue << BindIndexBuffer(m_fullScreenQuad->GetIndexBuffer());
        frame->renderQueue << DrawIndexed(6);
    }

    frame->renderQueue << EndFramebuffer(m_framebuffer);

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

#pragma endregion EnvGridPass

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

ReflectionsPass::ReflectionsPass(Vec2u extent, GBuffer* gbuffer, const GpuImageViewRef& mipChainImageView, const GpuImageViewRef& deferredResultImageView)
    : FullScreenPass(TF_R10G10B10A2, extent, gbuffer),
      m_mipChainImageView(mipChainImageView),
      m_deferredResultImageView(deferredResultImageView),
      m_cachedSsrTexture(nullptr),
      m_isFirstFrame(true)
{
    SetBlendFunction(BlendFunction(
        BMF_SRC_ALPHA, BMF_ONE_MINUS_SRC_ALPHA,
        BMF_ONE, BMF_ONE_MINUS_SRC_ALPHA));
}

ReflectionsPass::~ReflectionsPass()
{
    SafeDelete(std::move(m_mipChainImageView));
    SafeDelete(std::move(m_deferredResultImageView));

    m_ssrRenderer.Reset();
}

void ReflectionsPass::Create()
{
    HYP_SCOPE;

    FullScreenPass::Create();

    CreateSSRRenderer();
}

void ReflectionsPass::CreatePipeline()
{
    HYP_SCOPE;

    const MeshAttributes meshAttributes {
        VertexAttribute::MESH_INPUT_ATTRIBUTE_POSITION
        | VertexAttribute::MESH_INPUT_ATTRIBUTE_NORMAL
        | VertexAttribute::MESH_INPUT_ATTRIBUTE_TEXCOORD0
    };

    const MaterialAttributes materialAttributes {
        .fillMode = FM_FILL,
        .blendFunction = BlendFunction(
            BMF_SRC_ALPHA, BMF_ONE_MINUS_SRC_ALPHA,
            BMF_ONE, BMF_ONE_MINUS_SRC_ALPHA),
        .flags = MAF_NONE
    };

    CreatePipeline(RenderableAttributeSet(
        meshAttributes,
        materialAttributes));
}

void ReflectionsPass::CreatePipeline(const RenderableAttributeSet& renderableAttributes)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    static const FixedArray<Pair<CubemapType, ShaderProperties>, CMT_MAX> s_cubemapPasses = {
        Pair<CubemapType, ShaderProperties> { CMT_DEFAULT, ShaderProperties {} },
        Pair<CubemapType, ShaderProperties> { CMT_PARALLAX_CORRECTED, ShaderProperties { { NAME("ENV_PROBE_PARALLAX_CORRECTED") } } }
    };

    for (const auto& it : s_cubemapPasses)
    {
        ShaderRef shader = g_shaderManager->GetOrCreate(NAME("ApplyReflectionProbe"), it.second);
        Assert(shader.IsValid());

        DescriptorTableRef descriptorTable = g_renderBackend->MakeDescriptorTable(
            shader->GetCompiledShader()->GetDescriptorTableDeclaration());

        Assert(descriptorTable->Create());
        m_cubemapDescriptorTables[it.first] = std::move(descriptorTable);
        
        RenderTargetDesc rtDesc = m_framebuffer->GetRenderTargetDesc();

        GraphicsPipelineCacheHandle cacheHandle = g_renderInterface->graphicsPipelineCache->GetOrCreate(
            shader,
            &rtDesc,
            renderableAttributes);

        m_cubemapGraphicsPipelines[it.first] = std::move(cacheHandle);
    }

    m_graphicsPipelineCacheHandle = m_cubemapGraphicsPipelines[CMT_DEFAULT];
}

bool ReflectionsPass::ShouldRenderSSR() const
{
    static const ConfigurationValue& s_ssrEnabled = CoreApi::GetGlobalConfig().Get("Rendering.SSR.Enabled");
    static const ConfigurationValue& s_raytracingReflectionsEnabled = CoreApi::GetGlobalConfig().Get("Rendering.RayTracing.Reflections.Enabled");

    return s_ssrEnabled.ToBool(true) && !s_raytracingReflectionsEnabled.ToBool(false);
}

void ReflectionsPass::CreateSSRRenderer()
{
    m_ssrRenderer = MakeUnique<SSRRenderer>(SSRRendererConfig::FromConfig(), m_gbuffer, m_mipChainImageView, m_deferredResultImageView);
    m_ssrRenderer->Create();
}

void ReflectionsPass::Resize_Internal(Vec2u newSize)
{
    HYP_SCOPE;

    FullScreenPass::Resize_Internal(newSize);

    m_cubemapGraphicsPipelines = {};
}

void ReflectionsPass::Render(Frame* frame, const RenderSetup& rs)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    AssertDebug(rs.world && rs.view);
    AssertDebug(rs.passData != nullptr);

    const Viewport& viewport = rs.view->GetViewport();

    const uint32 frameIndex = frame->GetFrameIndex();

    RenderProxyList& rpl = RenderApi::GetConsumerProxyList(rs.view);
    rpl.BeginRead();
    HYP_DEFER({ rpl.EndRead(); });

    if (ShouldRenderSSR())
    {
        m_ssrRenderer->Render(frame, rs);
    }

    FixedArray<Array<EnvProbe*, RenderTempAllocator>, CMT_MAX> probesPerCubemapType;

    for (uint32 cubemapType = 0; cubemapType < CMT_MAX; cubemapType++)
    {
        const EnvProbeType envProbeType = EnvProbeTypes[cubemapType];

        for (EnvProbe* envProbe : rpl.GetEnvProbes().GetElements(s_envProbeTypeToTypeId[envProbeType]))
        {
            probesPerCubemapType[cubemapType].PushBack(envProbe);
        }
    }

    frame->renderQueue << BeginFramebuffer(GetFramebuffer());

    // render previous frame's result to screen
    if (!m_isFirstFrame && m_renderTextureToScreenPass != nullptr)
    {
        RenderPreviousTextureToScreen(frame, rs);
    }

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

        if (!m_cubemapGraphicsPipelines[cubemapType].IsAlive())
        {
            CreatePipeline();

            AssertDebug(m_cubemapGraphicsPipelines[cubemapType].IsAlive());
        }

        const GraphicsPipelineRef& graphicsPipeline = *m_cubemapGraphicsPipelines[cubemapType];
        const DescriptorTable* descriptorTable = m_cubemapDescriptorTables[cubemapType];

        graphicsPipeline->SetPushConstants(m_pushConstantData.Data(), m_pushConstantData.Size());

        if (ShouldRenderHalfRes())
        {
            const Vec2i viewportOffset = (Vec2i(m_framebuffer->GetExtent().x, 0) / 2) * (RenderApi::GetWorldBufferData()->frameCounter & 1);
            const Vec2u viewportExtent = Vec2u(m_framebuffer->GetExtent().x / 2, m_framebuffer->GetExtent().y);

            frame->renderQueue << BindGraphicsPipeline(graphicsPipeline, Viewport { viewportExtent, viewportOffset });
        }
        else
        {
            frame->renderQueue << BindGraphicsPipeline(graphicsPipeline, viewport);
        }

        const uint32 globalDescriptorSetIndex = graphicsPipeline->GetDescriptorSetIndex("Global"_sh);
        const uint32 viewDescriptorSetIndex = graphicsPipeline->GetDescriptorSetIndex("View"_sh);

        for (EnvProbe* envProbe : probes)
        {
            if (numRenderedEnvProbes >= MaxBoundReflectionProbes)
            {
                HYP_LOG(Rendering, Warning, "Attempting to render too many reflection probes.");

                break;
            }

            // RenderProxyEnvProbe* envProbeProxy = static_cast<RenderProxyEnvProbe*>(RenderApi::GetRenderProxy(envProbe->Id()));
            // Assert(envProbeProxy != nullptr);
            // AssertDebug(envProbeProxy->bufferData.textureIndex != ~0u, "EnvProbe texture index not set: not bound for proxy %p at frame idx %u!", (void*)envProbeProxy,
            //     RenderApi::GetFrameIndex_RenderThread());

            frame->renderQueue << BindDescriptorSet(
                descriptorTable->GetDescriptorSet("Global"_sh, frameIndex),
                graphicsPipeline,
                { { "CamerasBuffer"_sh, ShaderDataOffset<CameraShaderData>(rs.view->GetCamera()) },
                    { "CurrentEnvProbe"_sh, ShaderDataOffset<EnvProbeShaderData>(envProbe) } },
                globalDescriptorSetIndex);

            frame->renderQueue << BindDescriptorSet(
                rs.passData->descriptorSets[frameIndex],
                graphicsPipeline,
                {},
                viewDescriptorSetIndex);

            frame->renderQueue << BindVertexBuffer(m_fullScreenQuad->GetVertexBuffer());
            frame->renderQueue << BindIndexBuffer(m_fullScreenQuad->GetIndexBuffer());
            frame->renderQueue << DrawIndexed(6);

            ++numRenderedEnvProbes;
        }
    }

    if (ShouldRenderSSR())
    {
        const Handle<Texture>& ssrTexture = m_ssrRenderer->GetFinalResultTexture();

        // Create or update render-to-screen pass if needed
        if (!m_renderSsrToScreenPass || !ssrTexture.IsValid() || m_cachedSsrTexture != ssrTexture)
        {
            SafeDelete(std::move(m_renderSsrToScreenPass));

            if (ssrTexture.IsValid())
            {
                ShaderRef renderTextureToScreenShader = g_shaderManager->GetOrCreate(NAME("RenderTextureToScreen"));
                Assert(renderTextureToScreenShader.IsValid());

                DescriptorTableRef descriptorTable = g_renderBackend->MakeDescriptorTable(
                    renderTextureToScreenShader->GetCompiledShader()->GetDescriptorTableDeclaration());

                for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
                {
                    const DescriptorSetRef& descriptorSet = descriptorTable->GetDescriptorSet("RenderTextureToScreenDescriptorSet"_sh, frameIndex);
                    Assert(descriptorSet != nullptr);

                    descriptorSet->SetElement("InTexture"_sh, g_renderBackend->GetTextureImageView(ssrTexture));
                }

                DeferCreate(descriptorTable);

                // FramebufferRef renderSsrToScreenFramebuffer = g_renderBackend->MakeFramebuffer(m_extent);
                // renderSsrToScreenFramebuffer->AddAttachment(
                //     0,
                //     GetFramebuffer()->GetAttachment(0)->GetImage(),
                //     LoadOperation::LOAD,
                //     StoreOperation::STORE);

                // Assert(renderSsrToScreenFramebuffer->Create());

                m_renderSsrToScreenPass = CreateObject<FullScreenPass>(
                    renderTextureToScreenShader,
                    std::move(descriptorTable),
                    GetFramebuffer(),
                    m_imageFormat,
                    m_extent,
                    m_gbuffer,
                    FSP_EXTERNAL_RENDERTARGET);

                // Use alpha blending to blend SSR into the reflection probes
                m_renderSsrToScreenPass->SetBlendFunction(BlendFunction(
                    BMF_SRC_ALPHA, BMF_ONE_MINUS_SRC_ALPHA,
                    BMF_ONE, BMF_ONE_MINUS_SRC_ALPHA));

                InitObject(m_renderSsrToScreenPass);
                m_renderSsrToScreenPass->Create();

                m_cachedSsrTexture = ssrTexture;
            }
        }

        m_renderSsrToScreenPass->RenderToFramebuffer(frame, rs, GetFramebuffer());
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

    envGridRadiancePass.Reset();
    envGridIrradiancePass.Reset();

    reflectionsPass.Reset();

    lightmapPass.Reset();
    tonemapPass.Reset();
    mipChain.Reset();
    indirectPass.Reset();
    directPass.Reset();

    raytracingReflections.Reset();
    ddgi.Reset();

    SafeDelete(std::move(finalPassDescriptorSet));
}

#pragma endregion DeferredPassData

#pragma region RaytracingPassData

RaytracingPassData::~RaytracingPassData()
{
    SafeDelete(std::move(raytracingTlases));
    SafeDelete(std::move(raytracingDescriptorSets));
    SafeDelete(std::move(constants));
    SafeDelete(std::move(lightsBuffer));
}

#pragma endregion RaytracingPassData

#pragma region DeferredRenderer

static FramebufferRef CreateDeferredShadingFramebuffer(GBuffer* gbuffer)
{
    FramebufferRef framebuffer = g_renderBackend->MakeFramebuffer(gbuffer->GetExtent());

    TextureDesc textureDesc;
    textureDesc.type = TT_TEX2D;
    textureDesc.format = TF_RGBA16F;
    textureDesc.extent = Vec3u { gbuffer->GetExtent(), 1 };
    textureDesc.filterModeMin = TFM_NEAREST;
    textureDesc.filterModeMag = TFM_NEAREST;
    textureDesc.wrapMode = TWM_CLAMP_TO_EDGE;
    textureDesc.imageUsage = IU_ATTACHMENT | IU_SAMPLED;

    AttachmentRef colorAttachment = framebuffer->AddAttachment(
        0,
        g_renderBackend->MakeImage(textureDesc),
        LoadOperation::CLEAR,
        StoreOperation::STORE);

    // depth for stencil testing
    AttachmentRef depthAttachment = framebuffer->AddAttachment(
        1,
        gbuffer->GetBucket(RB_OPAQUE).GetGBufferAttachment(GTN_DEPTH)->GetImage(),
        LoadOperation::LOAD,
        StoreOperation::STORE);

    Assert(framebuffer->Create());

    colorAttachment->GetImage()->SetDebugName(NAME("DeferredShadingTarget_Color"));

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
}

void DeferredRenderer::Shutdown()
{
}

//#define CHECK_FRAMEBUFFER_SIZE(fb)                                                                    \
//    Assert(fb->GetExtent() == passData.viewport.extent,                                               \
//        "Deferred pass framebuffer extent does not match viewport extent! Expected {}x{}, got {}x{}", \
//        passData.viewport.extent.x, passData.viewport.extent.y,                                       \
//        fb->GetExtent().x, fb->GetExtent().y)

#define CHECK_FRAMEBUFFER_SIZE(...)

Handle<PassData> DeferredRenderer::CreateViewPassData(View* view, PassDataExt&)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    Assert(view != nullptr);

    if (view->GetFlags() & ViewFlags::GBUFFER)
    {
        Handle<DeferredRendererPassData> pd = CreateObject<DeferredRendererPassData>();
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

        HYP_LOG(Rendering, Info, "Creating renderer for view '{}' with GBuffer '{}'", view->Id(), gbuffer->GetExtent());

        const FramebufferRef& opaquePassFramebuffer = view->GetOutputTarget().GetFramebuffer(RB_OPAQUE);
        CHECK_FRAMEBUFFER_SIZE(opaquePassFramebuffer);

        const FramebufferRef& lightmapPassFramebuffer = view->GetOutputTarget().GetFramebuffer(RB_LIGHTMAP);
        CHECK_FRAMEBUFFER_SIZE(lightmapPassFramebuffer);

        passData.envGridRadiancePass = CreateObject<EnvGridPass>(EGPM_RADIANCE, passData.viewport.extent, gbuffer);
        passData.envGridRadiancePass->Create();

        passData.envGridIrradiancePass = CreateObject<EnvGridPass>(EGPM_IRRADIANCE, passData.viewport.extent, gbuffer);
        passData.envGridIrradiancePass->Create();

        passData.ssgi = MakeUnique<SSGI>(SSGIConfig::FromConfig(), gbuffer);
        passData.ssgi->Create();

        passData.postProcessing = MakeUnique<PostProcessing>();
        passData.postProcessing->Create();

        passData.deferredShadingFramebuffer = CreateDeferredShadingFramebuffer(gbuffer);

        passData.indirectPass = CreateObject<DeferredPass>(DPM_INDIRECT_LIGHTING, passData.viewport.extent, gbuffer, passData.deferredShadingFramebuffer);
        passData.indirectPass->Create();

        passData.directPass = CreateObject<DeferredPass>(DPM_DIRECT_LIGHTING, passData.viewport.extent, gbuffer, passData.deferredShadingFramebuffer);
        passData.directPass->Create();

        passData.depthPyramidRenderer = MakeUnique<DepthPyramidRenderer>(gbuffer);
        passData.depthPyramidRenderer->Create();

        passData.cullData.depthPyramidImageView = passData.depthPyramidRenderer->GetResultImageView();
        passData.cullData.depthPyramidDimensions = passData.depthPyramidRenderer->GetExtent();

        passData.mipChain = CreateObject<Texture>(TextureDesc {
            TT_TEX2D,
            opaquePassFramebuffer->GetAttachment(0)->GetFormat(),
            Vec3u(opaquePassFramebuffer->GetExtent(), 1),
            TFM_LINEAR_MIPMAP,
            TFM_LINEAR_MIPMAP,
            TWM_CLAMP_TO_EDGE });

        InitObject(passData.mipChain);

        passData.hbao = CreateObject<HBAO>(HBAOConfig::FromConfig(), passData.viewport.extent, gbuffer);
        passData.hbao->Create();

        // m_dofBlur = MakeUnique<DOFBlur>(gbuffer->GetResolution(), gbuffer);
        // m_dofBlur->Create();

        CreateViewCombinePass(view, passData);

        passData.reflectionsPass = CreateObject<ReflectionsPass>(passData.viewport.extent, gbuffer, g_renderBackend->GetTextureImageView(passData.mipChain), passData.combinePass->GetFinalImageView());
        passData.reflectionsPass->Create();

        passData.tonemapPass = CreateObject<TonemapPass>(passData.viewport.extent, gbuffer);
        passData.tonemapPass->Create();

        // We'll render the lightmap pass into the translucent framebuffer after deferred shading has been applied to OPAQUE objects.
        passData.lightmapPass = CreateObject<LightmapPass>();
        passData.lightmapPass->Create();

        passData.fogVolumePass = CreateObject<FogVolumePass>();
        passData.fogVolumePass->Create();

        passData.temporalAa = MakeUnique<TemporalAA>(passData.tonemapPass->GetFinalImageView(), passData.viewport.extent, gbuffer);
        passData.temporalAa->Create();
        
        CreateViewRaytracingPasses(view, passData);
        CreateViewDescriptorSets(view, passData);
        CreateViewFinalPassDescriptorSet(view, passData);

        return pd;
    }
    else if (view->GetFlags() & ViewFlags::RAYTRACING)
    {
        Handle<RaytracingPassData> pd = CreateObject<RaytracingPassData>();
        RaytracingPassData& passData = *pd;

        passData.view = MakeWeakRef(view);
        passData.viewport = view->GetViewport();

        return pd;
    }

    HYP_LOG(Rendering, Fatal,
        "Cannot create PassData for View {}! View does not have any flags set that would allow us to create PassData for it. View flags: {}",
        view->Id(), uint32(view->GetFlags()));

    return Handle<PassData>::empty;
}

void DeferredRenderer::CreateViewFinalPassDescriptorSet(View* view, DeferredRendererPassData& passData)
{
    HYP_SCOPE;

    ShaderRef renderTextureToScreenShader = g_shaderManager->GetOrCreate(NAME("RenderTextureToScreen"));
    Assert(renderTextureToScreenShader.IsValid());

    const GpuImageViewRef& inputImageView = m_rendererConfig.taaEnabled
        ? g_renderBackend->GetTextureImageView(passData.temporalAa->GetResultTexture())
        : passData.tonemapPass->GetFinalImageView();

    Assert(inputImageView.IsValid());

    const DescriptorTableDeclaration* descriptorTableDecl = renderTextureToScreenShader->GetCompiledShader()->GetDescriptorTableDeclaration();
    Assert(descriptorTableDecl != nullptr);

    DescriptorSetDeclaration* decl = descriptorTableDecl->FindDescriptorSetDeclaration("RenderTextureToScreenDescriptorSet"_sh);
    Assert(decl != nullptr);

    const DescriptorSetLayout layout { decl };

    DescriptorSetRef descriptorSet = g_renderBackend->MakeDescriptorSet(layout);
    descriptorSet->SetDebugName(NAME("FinalPassDescriptorSet"));
    descriptorSet->SetElement("InTexture"_sh, inputImageView);

    Assert(descriptorSet->Create());

    SafeDelete(std::move(passData.finalPassDescriptorSet));

    passData.finalPassDescriptorSet = std::move(descriptorSet);
}

void DeferredRenderer::CreateViewDescriptorSets(View* view, DeferredRendererPassData& passData)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    const DescriptorSetDeclaration* decl = g_renderInterface->globalDescriptorTable->GetDeclaration()->FindDescriptorSetDeclaration("View"_sh);
    Assert(decl != nullptr);

    const DescriptorSetLayout layout { decl };

    FixedArray<DescriptorSetRef, NumFramesInFlight> descriptorSets;

    const FramebufferRef& opaquePassFramebuffer = view->GetOutputTarget().GetFramebuffer(RB_OPAQUE);

    // depth attachment goes into separate slot
    AttachmentBase* depthAttachment = opaquePassFramebuffer->GetAttachment(GTN_MAX - 1);
    Assert(depthAttachment != nullptr);

    for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
    {
        DescriptorSetRef descriptorSet = g_renderBackend->MakeDescriptorSet(layout);
        descriptorSet->SetDebugName(NAME_FMT("SceneViewDescriptorSet_{}", frameIndex));

        if (g_renderBackend->GetRenderConfig().dynamicDescriptorIndexing)
        {
            uint32 gbufferElementIndex = 0;

            // not including depth texture here (hence the - 1)
            for (uint32 attachmentIndex = 0; attachmentIndex < GTN_MAX - 1; attachmentIndex++)
            {
                descriptorSet->SetElement("GBufferTextures"_sh, gbufferElementIndex++, opaquePassFramebuffer->GetAttachment(attachmentIndex)->GetImageView());
            }
        }
        else
        {
            for (uint32 attachmentIndex = 0; attachmentIndex < GTN_MAX - 1; attachmentIndex++)
            {
                descriptorSet->SetElement(GBufferTextureNames[attachmentIndex], opaquePassFramebuffer->GetAttachment(attachmentIndex)->GetImageView());
            }
        }

        descriptorSet->SetElement("GBufferDepthTexture"_sh, depthAttachment->GetImageView());
        descriptorSet->SetElement("GBufferMipChain"_sh, g_renderBackend->GetTextureImageView(passData.mipChain));
        descriptorSet->SetElement("PostProcessingUniforms"_sh, passData.postProcessing->GetUniformBuffer());
        descriptorSet->SetElement("DepthPyramidResult"_sh, passData.depthPyramidRenderer->GetResultImageView());
        descriptorSet->SetElement("TAAResultTexture"_sh, g_renderBackend->GetTextureImageView(passData.temporalAa->GetResultTexture()));

        // Set SSR texture - use placeholder if not available yet
        Texture* ssrTexture = passData.reflectionsPass->ShouldRenderSSR()
            ? passData.reflectionsPass->GetSSRRenderer()->GetFinalResultTexture()
            : nullptr;

        if (ssrTexture)
        {
            descriptorSet->SetElement("SSRResultTexture"_sh, g_renderBackend->GetTextureImageView(MakeStrongRef(ssrTexture)));
            passData.cachedSsrTexture = ssrTexture;
        }
        else
        {
            descriptorSet->SetElement("SSRResultTexture"_sh, g_renderInterface->placeholderData->GetImageView2D1x1R8());
            passData.cachedSsrTexture = nullptr;
        }

        if (passData.ssgi)
        {
            descriptorSet->SetElement("SSGIResultTexture"_sh, g_renderBackend->GetTextureImageView(passData.ssgi->GetFinalResultTexture()));
        }
        else
        {
            descriptorSet->SetElement("SSGIResultTexture"_sh, g_renderInterface->placeholderData->GetImageView2D1x1R8());
        }

        if (passData.hbao)
        {
            descriptorSet->SetElement("SSAOResultTexture"_sh, passData.hbao->GetFinalImageView());
        }
        else
        {
            descriptorSet->SetElement("SSAOResultTexture"_sh, g_renderInterface->placeholderData->GetImageView2D1x1R8());
        }

        descriptorSet->SetElement("DeferredResult"_sh, passData.combinePass->GetFinalImageView());
        descriptorSet->SetElement("DeferredIndirectResultTexture"_sh, passData.deferredShadingFramebuffer->GetAttachment(0)->GetImageView());

        descriptorSet->SetElement("ReflectionProbeResultTexture"_sh, passData.reflectionsPass->GetFinalImageView());

        descriptorSet->SetElement("EnvGridRadianceResultTexture"_sh, passData.envGridRadiancePass->GetFinalImageView());
        descriptorSet->SetElement("EnvGridIrradianceResultTexture"_sh, passData.envGridIrradiancePass->GetFinalImageView());

        if (passData.ddgi)
        {
            descriptorSet->SetElement("DDGIConstants"_sh, passData.ddgi->GetConstantBuffer(frameIndex));
            descriptorSet->SetElement("DDGIIrradianceTexture"_sh, passData.ddgi->GetIrradianceImageView());
            descriptorSet->SetElement("DDGIDepthTexture"_sh, passData.ddgi->GetDepthImageView());
        }
        else
        {
            descriptorSet->SetElement("DDGIConstants"_sh, g_renderInterface->placeholderData->GetOrCreateBuffer(GpuBufferType::CBUFF, sizeof(DDGIConstants), /* exactSize */ true));
            descriptorSet->SetElement("DDGIIrradianceTexture"_sh, g_renderInterface->placeholderData->GetImageView2D1x1R8());
            descriptorSet->SetElement("DDGIDepthTexture"_sh, g_renderInterface->placeholderData->GetImageView2D1x1R8());
        }
        

        HYP_GFX_ASSERT(descriptorSet->Create());

        descriptorSets[frameIndex] = std::move(descriptorSet);
    }

    passData.descriptorSets = std::move(descriptorSets);
}

void DeferredRenderer::CreateViewCombinePass(View* view, DeferredRendererPassData& passData)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    const FramebufferRef& srcFramebuffer = view->GetOutputTarget().GetFramebuffer(RB_TRANSLUCENT);
    Assert(srcFramebuffer != nullptr);

    ShaderRef renderTextureToScreenShader = g_shaderManager->GetOrCreate(NAME("RenderTextureToScreen"));
    Assert(renderTextureToScreenShader.IsValid());

    DescriptorTableRef descriptorTable = g_renderBackend->MakeDescriptorTable(
        renderTextureToScreenShader->GetCompiledShader()->GetDescriptorTableDeclaration());

    for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
    {
        const DescriptorSetRef& descriptorSet = descriptorTable->GetDescriptorSet("RenderTextureToScreenDescriptorSet"_sh, frameIndex);
        Assert(descriptorSet != nullptr);

        descriptorSet->SetElement("InTexture"_sh, passData.deferredShadingFramebuffer->GetAttachment(0)->GetImageView());
    }

    Assert(descriptorTable->Create());

    passData.combinePass = CreateObject<FullScreenPass>(
        renderTextureToScreenShader,
        std::move(descriptorTable),
        srcFramebuffer,
        srcFramebuffer->GetAttachment(0)->GetFormat(),
        srcFramebuffer->GetExtent(),
        nullptr);

    passData.combinePass->Create();
}

void DeferredRenderer::CreateViewRaytracingPasses(View* view, DeferredRendererPassData& passData)
{
    AssertOnThread(g_renderThread);

    if (!g_renderBackend->GetRenderConfig().raytracing)
    {
        return;
    }

    const bool shouldEnableRaytracingForView = view->GetRaytracingView().IsValid()
        && CoreApi::GetGlobalConfig().Get("Rendering.RayTracing.Enabled").ToBool();

    if (!shouldEnableRaytracingForView)
    {
        passData.raytracingReflections.Reset();
        passData.ddgi.Reset();

        return;
    }

    GBuffer* gbuffer = view->GetOutputTarget().GetGBuffer();
    AssertDebug(gbuffer != nullptr);

    passData.raytracingReflections = MakeUnique<RaytracingReflections>(RaytracingReflectionsConfig::FromConfig(), gbuffer);
    passData.raytracingReflections->Create();

    /// FIXME: Proper AABB for DDGI
    passData.ddgi = MakeUnique<DDGI>(DDGIInfo { .aabb = { { -60.0f, -5.0f, -60.0f }, { 60.0f, 40.0f, 60.0f } } });
    passData.ddgi->Create();
}

void DeferredRenderer::CreateViewTopLevelAccelerationStructures(View* view, RaytracingPassData& passData)
{
    SafeDelete(std::move(passData.raytracingTlases));

    // Hack to fix driver crash when building TLAS with no meshes
    Handle<Mesh> defaultMesh = MeshBuilder::Cube(true);
    defaultMesh->SetFlags(MF_VIEW_INDEPENDENT);
    InitObject(defaultMesh);

    GpuBlasRef blas = MeshBlasBuilder::Build(defaultMesh);
    HYP_GFX_ASSERT(blas->Create());

    for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
    {
        GpuTlasRef& tlas = passData.raytracingTlases[frameIndex];

        tlas = g_renderBackend->MakeTLAS();
        tlas->AddGpuBlas(blas);

        HYP_GFX_ASSERT(tlas->Create());
    }
}

void DeferredRenderer::ResizeView(Viewport viewport, View* view, DeferredRendererPassData& passData)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    HYP_LOG(Rendering, Debug, "Resizing View '{}' to {}x{}", view->Id(), viewport.extent.x, viewport.extent.y);

    Assert(viewport.extent.Volume() > 0);

    passData.viewport = viewport;

    const Vec2u newSize = Vec2u(viewport.extent);

    GBuffer* gbuffer = view->GetOutputTarget().GetGBuffer();
    Assert(gbuffer != nullptr && gbuffer->IsCreated());

    gbuffer->Resize(newSize);

    const FramebufferRef& opaquePassFramebuffer = view->GetOutputTarget().GetFramebuffer(RB_OPAQUE);
    CHECK_FRAMEBUFFER_SIZE(opaquePassFramebuffer);

    const FramebufferRef& lightmapPassFramebuffer = view->GetOutputTarget().GetFramebuffer(RB_LIGHTMAP);
    CHECK_FRAMEBUFFER_SIZE(lightmapPassFramebuffer);

    {
        if (passData.deferredShadingFramebuffer.IsValid())
        {
            SafeDelete(std::move(passData.deferredShadingFramebuffer));
        }

        passData.deferredShadingFramebuffer = CreateDeferredShadingFramebuffer(gbuffer);
        CHECK_FRAMEBUFFER_SIZE(passData.deferredShadingFramebuffer);
    }

    passData.directPass->Resize(newSize);
    passData.indirectPass->Resize(newSize);

    passData.hbao->Resize(newSize);

    passData.combinePass.Reset();
    CreateViewCombinePass(view, passData);

    passData.envGridRadiancePass->Resize(newSize);
    passData.envGridIrradiancePass->Resize(newSize);

    passData.reflectionsPass.Reset();
    passData.reflectionsPass = CreateObject<ReflectionsPass>(
        newSize,
        gbuffer,
        g_renderBackend->GetTextureImageView(passData.mipChain),
        passData.combinePass->GetFinalImageView());
    passData.reflectionsPass->Create();

    passData.tonemapPass = CreateObject<TonemapPass>(passData.viewport.extent, gbuffer);
    passData.tonemapPass->Create();

    passData.lightmapPass = CreateObject<LightmapPass>();
    passData.lightmapPass->Create();

    passData.fogVolumePass = CreateObject<FogVolumePass>();
    passData.fogVolumePass->Create();

    passData.temporalAa = MakeUnique<TemporalAA>(passData.tonemapPass->GetFinalImageView(), newSize, gbuffer);
    passData.temporalAa->Create();

    passData.depthPyramidRenderer = MakeUnique<DepthPyramidRenderer>(gbuffer);
    passData.depthPyramidRenderer->Create();

    CreateViewRaytracingPasses(view, passData);

    SafeDelete(std::move(passData.descriptorSets));
    CreateViewDescriptorSets(view, passData);

    SafeDelete(std::move(passData.finalPassDescriptorSet));
    CreateViewFinalPassDescriptorSet(view, passData);

    passData.view = MakeWeakRef(view);
}

HYP_DISABLE_OPTIMIZATION;
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

        RenderProxyList& rpl = RenderApi::GetConsumerProxyList(view);
        rpl.BeginRead();

        renderProxyLists.PushBack(&rpl);

        if (view->GetFlags() & ViewFlags::GBUFFER)
        {
            const Handle<PassData>& pd = FetchViewPassData(view);
            Assert(pd != nullptr);

            DeferredRendererPassData* pdCasted = ObjCast<DeferredRendererPassData>(pd.Get());
            Assert(pdCasted != nullptr);

            const Viewport vp = view->GetViewport();

            if (pdCasted->viewport != vp)
            {
                ResizeView(vp, view, *pdCasted);
            }

            pdCasted->priority = view->GetPriority();
        }
        else if (view->GetFlags() & ViewFlags::RAYTRACING)
        {
            const Handle<PassData>& pd = FetchViewPassData(view);
            Assert(pd != nullptr);

            RaytracingPassData* pdCasted = ObjCast<RaytracingPassData>(pd.Get());
            Assert(pdCasted != nullptr);

            RenderSetup newRS = rs.Fork();
            newRS.passData = pd;
            newRS.view = view;

            UpdateRaytracingView(frame, newRS);
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

        const Handle<DeferredRendererPassData>& pd = ObjCast<DeferredRendererPassData>(FetchViewPassData(view));
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

        RenderProxyList& rpl = RenderApi::GetConsumerProxyList(view);

        g_statViews++;
        g_statTextures += rpl.GetTextures().NumCurrent();
        g_statMaterials += rpl.GetMaterials().NumCurrent();
        g_statLightmapVolumes += rpl.GetLightmapVolumes().NumCurrent();
        g_statParticleVolumes += rpl.GetParticleVolumes().NumCurrent();
        g_statLights += rpl.GetLights().NumCurrent();
        g_statEnvGrids += rpl.GetEnvGrids().NumCurrent();
        g_statEnvProbes += rpl.GetEnvProbes().NumCurrent();

#if 0
        HYP_LOG(Rendering, Debug, "View '{}' used {} textures, {} materials, {} lightmap volumes, {} lights, {} env grids and {} env probes.",
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

    uint32 slot = RenderApi::GetRingIndex();
    if (m_lastFrameData.frameId != slot)
    {
        m_lastFrameData.frameId = slot;
        m_lastFrameData.passData.Clear();
    }

    View* view = rs.view;

    const Viewport& viewport = view->GetViewport();

    Assert(view->GetFlags() & ViewFlags::GBUFFER);

    RenderProxyList& rpl = RenderApi::GetConsumerProxyList(view);
    rpl.BeginRead();
    HYP_DEFER({ rpl.EndRead(); });

    RenderCollector& renderCollector = RenderApi::GetRenderCollector(view);

    DeferredRendererPassData* passDataCasted = ObjCast<DeferredRendererPassData>(rs.passData);
    AssertDebug(passDataCasted != nullptr);

    DeferredRendererPassData& passData = *passDataCasted;

    const uint32 frameIndex = frame->GetFrameIndex();

    Framebuffer* opaquePassFramebuffer = view->GetOutputTarget().GetFramebuffer(RB_OPAQUE);
    CHECK_FRAMEBUFFER_SIZE(opaquePassFramebuffer);

    Framebuffer* lightmapPassFramebuffer = view->GetOutputTarget().GetFramebuffer(RB_LIGHTMAP);
    CHECK_FRAMEBUFFER_SIZE(lightmapPassFramebuffer);

    Framebuffer* translucentPassFramebuffer = view->GetOutputTarget().GetFramebuffer(RB_TRANSLUCENT);
    CHECK_FRAMEBUFFER_SIZE(translucentPassFramebuffer);

    Framebuffer* debugPassFramebuffer = view->GetOutputTarget().GetFramebuffer(RB_DEBUG);
    CHECK_FRAMEBUFFER_SIZE(debugPassFramebuffer);

    const bool doParticles = true;
    const bool doGaussianSplatting = false; // environment && environment->IsReady();

    const bool useRaytracingReflections = (m_rendererConfig.pathTracer || m_rendererConfig.raytracingReflections)
        && view->GetRaytracingView().IsValid()
        && passData.raytracingReflections != nullptr;

    const bool useRaytracingGlobalIllumination = m_rendererConfig.raytracingGlobalIllumination
        && view->GetRaytracingView().IsValid()
        && passData.ddgi != nullptr;

    const bool useHbao = m_rendererConfig.hbaoEnabled;
    const bool useHbil = m_rendererConfig.hbilEnabled;
    const bool useSsgi = m_rendererConfig.ssgiEnabled;

    const bool useEnvGridIrradiance = rpl.GetEnvGrids().NumCurrent() && m_rendererConfig.envGridGiEnabled;
    const bool useEnvGridRadiance = rpl.GetEnvGrids().NumCurrent() && m_rendererConfig.envGridRadianceEnabled;

    const bool useTAA = passData.temporalAa != nullptr && m_rendererConfig.taaEnabled;

    if (useTAA)
    {
        // apply jitter to camera for TAA
        RenderProxyCamera* cameraProxy = static_cast<RenderProxyCamera*>(RenderApi::GetRenderProxy(view->GetCamera()));
        Assert(cameraProxy != nullptr);

        CameraShaderData& cameraBufferData = cameraProxy->bufferData;

        if (MathUtil::ApproxEqual(cameraBufferData.projMat[3][3], 0.0f))
        {
            const uint32 frameCounter = RenderApi::GetWorldBufferData()->frameCounter + 1;

            Vec4f jitter = Vec4f::Zero();
            Mat4f::Jitter(frameCounter, viewport.extent.x, viewport.extent.y, jitter);

            cameraBufferData.jitter = jitter * CameraJitterScale;

            RenderApi::UpdateGpuData(view->GetCamera());
        }
    }

    struct
    {
        uint32 flags;
        uint32 screenWidth;
        uint32 screenHeight;
    } deferredData;

    Memory::MemSet(&deferredData, 0, sizeof(deferredData));

    deferredData.flags |= useHbao ? DEFERRED_FLAGS_HBAO_ENABLED : 0;
    deferredData.flags |= useHbil ? DEFERRED_FLAGS_HBIL_ENABLED : 0;
    deferredData.flags |= useRaytracingReflections ? DEFERRED_FLAGS_RT_RADIANCE_ENABLED : 0;
    deferredData.flags |= useRaytracingGlobalIllumination ? DEFERRED_FLAGS_DDGI_ENABLED : 0;

    deferredData.screenWidth = view->GetViewport().extent.x;  // rpl.viewport.extent.x;
    deferredData.screenHeight = view->GetViewport().extent.y; // rpl.viewport.extent.y;

    // Update SSR texture descriptor if it has changed
    if (passData.reflectionsPass->ShouldRenderSSR() && passData.reflectionsPass->GetSSRRenderer())
    {
        Texture* currentSsrTexture = passData.reflectionsPass->GetSSRRenderer()->GetFinalResultTexture();

        if (passData.cachedSsrTexture != currentSsrTexture)
        {
            // SSR texture has changed - update all frame descriptors
            for (uint32 i = 0; i < NumFramesInFlight; i++)
            {
                if (!currentSsrTexture)
                {
                    passData.descriptorSets[i]->SetElement("SSRResultTexture"_sh, g_renderInterface->placeholderData->GetImageView2D1x1R8());
                    continue;
                }

                passData.descriptorSets[i]->SetElement("SSRResultTexture"_sh, g_renderBackend->GetTextureImageView(MakeStrongRef(currentSsrTexture)));
            }

            passData.cachedSsrTexture = currentSsrTexture;
        }
    }

    PerformOcclusionCulling(frame, rs, renderCollector);

    // if (doGaussianSplatting)
    // {
    //     environment->GetGaussianSplatting()->UpdateSplats(frame, rs);
    // }

    passData.indirectPass->SetPushConstants(&deferredData, sizeof(deferredData));
    passData.directPass->SetPushConstants(&deferredData, sizeof(deferredData));

    { // render opaque objects into separate framebuffer
        frame->renderQueue << BeginFramebuffer(opaquePassFramebuffer);

        ExecuteDrawCalls(frame, rs, renderCollector, (1u << RB_OPAQUE));

        frame->renderQueue << EndFramebuffer(opaquePassFramebuffer);
    }

    // render lightmap volume objects
    if (rpl.GetLightmapVolumes().NumCurrent())
    {
        for (int attachmentIndex = 0; attachmentIndex < lightmapPassFramebuffer->NumAttachments(); attachmentIndex++)
        {
            AttachmentBase* attachment = lightmapPassFramebuffer->GetAttachment(attachmentIndex);

            if (attachment->GetLoadOperation() == LoadOperation::LOAD)
            {
                frame->renderQueue << InsertBarrier(attachment->GetImage(), attachment->IsDepthAttachment() ? RS_DEPTH_STENCIL : RS_RENDER_TARGET);
            }
        }

        // render objects to be lightmapped, separate from the opaque objects.
        // The lightmap bucket's framebuffer has a color attachment that will write into the opaque framebuffer's color attachment.
        frame->renderQueue << BeginFramebuffer(lightmapPassFramebuffer);

        ExecuteDrawCalls(frame, rs, renderCollector, (1u << RB_LIGHTMAP));

        frame->renderQueue << EndFramebuffer(lightmapPassFramebuffer);
    }

    if (useEnvGridIrradiance || useEnvGridRadiance)
    {
        if (useEnvGridIrradiance)
        {
            passData.envGridIrradiancePass->SetPushConstants(&deferredData, sizeof(deferredData));
            passData.envGridIrradiancePass->Render(frame, rs);
        }

        if (useEnvGridRadiance)
        {
            passData.envGridRadiancePass->SetPushConstants(&deferredData, sizeof(deferredData));
            passData.envGridRadiancePass->Render(frame, rs);
        }
    }

    passData.reflectionsPass->SetPushConstants(&deferredData, sizeof(deferredData));
    passData.reflectionsPass->Render(frame, rs);

    if ((useRaytracingGlobalIllumination || useRaytracingReflections) && view->GetRaytracingView().IsValid())
    {
        Handle<View> raytracingView = view->GetRaytracingView().Lock();

        if (raytracingView != nullptr)
        {
            const Handle<RaytracingPassData>& raytracingPassData = ObjCast<RaytracingPassData>(FetchViewPassData(raytracingView));
            Assert(raytracingPassData != nullptr);

            const GpuTlasRef& tlas = raytracingPassData->raytracingTlases[frameIndex];

            if (tlas && tlas->IsCreated())
            {
                HYP_GFX_ASSERT(tlas->GetMeshDescriptionsBuffer() != nullptr);

                raytracingPassData->parentPass = &passData;

                RenderSetup raytracingRS = rs.Fork();
                raytracingRS.passData = raytracingPassData;

                // set sky as fallback

                // Set first found sky probe as fallback probe
                auto& skyProbes = rpl.GetEnvProbes().GetElements<SkyProbe>();
                if (skyProbes.Any())
                {
                    raytracingRS.envProbe = skyProbes.Front();
                }

                if (useRaytracingReflections)
                {
                    AssertDebug(passData.raytracingReflections != nullptr);
                    passData.raytracingReflections->Render(frame, raytracingRS);
                }

                if (useRaytracingGlobalIllumination)
                {
                    AssertDebug(passData.ddgi != nullptr);
                    passData.ddgi->Render(frame, raytracingRS);
                }

                // unset parent pass after using it
                raytracingPassData->parentPass = nullptr;
            }
        }
    }

    if (useHbao || useHbil)
    {
        passData.hbao->Render(frame, rs);
    }

    if (useSsgi)
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
        // insert barriers for load operations where needed
        for (int attachmentIndex = 0; attachmentIndex < translucentPassFramebuffer->NumAttachments(); attachmentIndex++)
        {
            AttachmentBase* attachment = translucentPassFramebuffer->GetAttachment(attachmentIndex);

            if (attachment->GetLoadOperation() == LoadOperation::LOAD)
            {
                frame->renderQueue << InsertBarrier(attachment->GetImage(), attachment->IsDepthAttachment() ? RS_DEPTH_STENCIL : RS_RENDER_TARGET);
            }
        }

        frame->renderQueue << BeginFramebuffer(translucentPassFramebuffer);

        { // Render the deferred lighting into the translucent pass framebuffer with a full screen quad.
            const GraphicsPipelineRef& pipeline = passData.combinePass->GetGraphicsPipeline();
            AssertDebug(pipeline != nullptr);

            frame->renderQueue << BindGraphicsPipeline(pipeline, viewport);

            frame->renderQueue << BindDescriptorTable(
                passData.combinePass->GetDescriptorTable().GetUnchecked(),
                pipeline,
                {},
                frameIndex);

            frame->renderQueue << BindVertexBuffer(passData.combinePass->GetQuadMesh()->GetVertexBuffer());
            frame->renderQueue << BindIndexBuffer(passData.combinePass->GetQuadMesh()->GetIndexBuffer());
            frame->renderQueue << DrawIndexed(6);
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
    if (renderCollector.mappingsByBucket[RB_DEBUG].Any() || g_engineDriver->GetDebugDrawer()->NumEnqueuedDrawCommands() > 0)
    {
        for (int attachmentIndex = 0; attachmentIndex < debugPassFramebuffer->NumAttachments(); attachmentIndex++)
        {
            AttachmentBase* attachment = debugPassFramebuffer->GetAttachment(attachmentIndex);

            if (attachment->GetLoadOperation() == LoadOperation::LOAD)
            {
                frame->renderQueue << InsertBarrier(attachment->GetImage(), attachment->IsDepthAttachment() ? RS_DEPTH_STENCIL : RS_RENDER_TARGET);
            }
        }

        frame->renderQueue << BeginFramebuffer(debugPassFramebuffer);

        ExecuteDrawCalls(frame, rs, renderCollector, (1u << RB_DEBUG));
        g_engineDriver->GetDebugDrawer()->Render(frame, rs);

        frame->renderQueue << EndFramebuffer(debugPassFramebuffer);
    }

    passData.postProcessing->RenderPost(frame, rs);

    passData.tonemapPass->Render(frame, rs);

    if (useTAA)
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

#undef CHECK_FRAMEBUFFER_SIZE

void DeferredRenderer::UpdateRaytracingView(Frame* frame, const RenderSetup& rs)
{
    HYP_SCOPE;

    View* view = rs.view;
    AssertDebug(view != nullptr);

    if (!(view->GetFlags() & ViewFlags::RAYTRACING))
    {
        return;
    }

    const uint32 currentFrameIndex = frame->GetFrameIndex();

    RaytracingPassData* pd = ObjCast<RaytracingPassData>(rs.passData);

    RenderProxyList& rpl = RenderApi::GetConsumerProxyList(rs.view);
    rpl.BeginRead();
    HYP_DEFER({ rpl.EndRead(); });

    if (!pd->raytracingTlases[currentFrameIndex])
    {
        for (GpuTlasRef& tlas : pd->raytracingTlases)
        {
            tlas = g_renderBackend->MakeTLAS();
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

        GpuBlasRef& blas = meshProxy->raytracingData.blas;
    
        if (blas != cachedBlas)
        {
            if (blas != nullptr)
            {
                for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
                {
                    pd->raytracingTlases[frameIndex]->RemoveGpuBlas(blas);
                }
            }
            
            blas = cachedBlas;
        }

        if (!blas->IsCreated())
        {
            blas->SetTransform(meshProxy->bufferData.modelMatrix);

            const uint32 materialBinding = RenderApi::RetrieveResourceBinding(meshProxy->material);
            blas->SetMaterialBinding(materialBinding);

            HYP_GFX_ASSERT(blas->Create());
        }
        else
        {
            const uint32 materialBinding = RenderApi::RetrieveResourceBinding(meshProxy->material);

            blas->SetMaterialBinding(materialBinding);
            blas->SetTransform(meshProxy->bufferData.modelMatrix);
        }

        if (!pd->raytracingTlases[currentFrameIndex]->HasGpuBlas(blas))
        {
            for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
            {
                pd->raytracingTlases[frameIndex]->AddGpuBlas(meshProxy->raytracingData.blas);
            }

            hasBlas = true;
        }
    }

    if (!pd->raytracingTlases[currentFrameIndex]->IsCreated())
    {
        if (hasBlas)
        {
            for (GpuTlasRef& tlas : pd->raytracingTlases)
            {
                HYP_GFX_ASSERT(tlas->Create());
            }
        }

        return;
    }

    RTUpdateStateFlags updateStateFlags = RTUpdateStateFlagBits::RT_UPDATE_STATE_FLAGS_NONE;
    pd->raytracingTlases[currentFrameIndex]->UpdateStructure(updateStateFlags);
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
