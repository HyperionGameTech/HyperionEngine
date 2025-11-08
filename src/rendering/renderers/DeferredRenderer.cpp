/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <rendering/renderers/DeferredRenderer.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/DepthPyramidRenderer.hpp>
#include <rendering/renderers/EnvGridRenderer.hpp>
#include <rendering/renderers/EnvProbeRenderer.hpp>
#include <rendering/RenderMaterial.hpp>
#include <rendering/RenderGlobalState.hpp>
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
#include <rendering/RenderGpuBuffer.hpp>
#include <rendering/RenderDevice.hpp>
#include <rendering/RenderDescriptorSet.hpp>
#include <rendering/RenderGraphicsPipeline.hpp>
#include <rendering/RenderBackend.hpp>
#include <rendering/RenderSwapchain.hpp>
#include <rendering/RenderCollection.hpp>
#include <rendering/shader_compiler/ShaderCompiler.hpp>
#include <rendering/raytracing/MeshBlasBuilder.hpp>
#include <rendering/raytracing/RaytracingReflections.hpp>
#include <rendering/raytracing/DDGI.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <engine/DebugDrawer.hpp>

#include <scene/World.hpp>
#include <scene/View.hpp>
#include <scene/EnvGrid.hpp>
#include <scene/EnvProbe.hpp>

#include <lightmapper/LightmapVolume.hpp>

#include <core/config/Config.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/filesystem/FsUtil.hpp>

#include <core/utilities/DeferredScope.hpp>
#include <core/utilities/Float16.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <system/AppContext.hpp>

#include <util/MeshBuilder.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>
#include <engine/EngineStats.hpp>

#include <DeferredRenderer.generated.inl>

namespace hyperion {

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

static constexpr StringHash GBufferTextureNames[] = {
    StringHash("GBufferAlbedoTexture"),
    StringHash("GBufferNormalsTexture"),
    StringHash("GBufferMaterialTexture"),
    StringHash("GBufferLightmapTexture"),
    StringHash("GBufferVelocityTexture"),
    StringHash("GBufferWSNormalsTexture"),
    StringHash("GBufferTranslucentTexture")
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
EngineStatCounter<uint32> g_statEnvProbes("Rendering/EnvProbes");
EngineStatCounter<uint32> g_statEnvGrids("Rendering/EnvGrids");
EngineStatCounter<uint32> g_statDebugDraws("Rendering/DebugDraws");

extern const GlobalConfig& CoreApi_GetGlobalConfig();

static void GetDeferredShaderProperties(
    DeferredPassMode mode,
    ShaderProperties& outShaderProperties,
    const RenderProxyList* rpl = nullptr,
    LightType lightType = LT_INVALID)
{
    static const GlobalConfig& s_globalConfig = CoreApi_GetGlobalConfig();
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
    TypeId::ForType<EnvProbe>(),        // EPT_SHADOW (@TODO: Remove)
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

DeferredPass::DeferredPass(DeferredPassMode mode, Vec2u extent, GBuffer* gbuffer)
    : FullScreenPass(TF_RGBA16F, extent, gbuffer),
      m_mode(mode),
      m_directLightGraphicsPipelines()
{
    SetBlendFunction(BlendFunction::Additive());
}

DeferredPass::~DeferredPass()
{
    SafeDelete(std::move(m_ltcSampler));
}

void DeferredPass::Create()
{
    FullScreenPass::Create();

    // linear transform cosines texture data
    if (m_mode == DPM_DIRECT_LIGHTING && !m_ltcSampler)
    {
        m_ltcSampler = g_renderBackend->MakeSampler(
            TFM_NEAREST,
            TFM_LINEAR,
            TWM_CLAMP_TO_EDGE);

        DeferCreate(m_ltcSampler);

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

    const RenderableAttributeSet renderableAttributes {
        MeshAttributes { .vertexAttributes = shaderProperties.GetRequiredVertexAttributes() },
        MaterialAttributes {
            .fillMode = FM_FILL,
            .blendFunction = m_blendFunction,
            .flags = MAF_NONE }
    };

    if (m_mode == DPM_INDIRECT_LIGHTING)
    {
        m_shader = g_shaderManager->GetOrCreate(NAME("DeferredIndirect"), shaderProperties);
        Assert(m_shader.IsValid());

        return g_renderGlobalState->graphicsPipelineCache->GetOrCreate(
            m_shader,
            nullptr,
            { &m_framebuffer, 1 },
            renderableAttributes);
    }

    ShaderRef shader = g_shaderManager->GetOrCreate(NAME("DeferredDirect"), shaderProperties);
    Assert(shader != nullptr);

    DescriptorTableRef descriptorTable = g_renderBackend->MakeDescriptorTable(
        shader->GetCompiledShader()->GetDescriptorTableDeclaration());

    for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
    {
        const DescriptorSetRef& descriptorSet = descriptorTable->GetDescriptorSet("DeferredDirectDescriptorSet", frameIndex);
        Assert(descriptorSet.IsValid());

        descriptorSet->SetElement("MaterialsBuffer", g_renderGlobalState->gpuBuffers[GRB_MATERIALS]->GetBuffer(frameIndex));

        descriptorSet->SetElement("LTCSampler", m_ltcSampler);
        descriptorSet->SetElement("LTCMatrixTexture", g_renderBackend->GetTextureImageView(m_ltcMatrixTexture));
        descriptorSet->SetElement("LTCBRDFTexture", g_renderBackend->GetTextureImageView(m_ltcBrdfTexture));
    }

    DeferCreate(descriptorTable);

    return g_renderGlobalState->graphicsPipelineCache->GetOrCreate(
        shader,
        descriptorTable,
        { &m_framebuffer, 1 },
        renderableAttributes);
}

void DeferredPass::Resize_Internal(Vec2u newSize)
{
    FullScreenPass::Resize_Internal(newSize);

    m_directLightGraphicsPipelines = {};
}

void DeferredPass::Render(FrameBase* frame, const RenderSetup& rs)
{
    HYP_SCOPE;
    ENGINE_STAT_SCOPE(&s_deferredPassTimer);

    AssertDebug(rs.IsValid());
    AssertDebug(rs.HasView());
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
        // check needs invalidation
        ShaderProperties shaderProperties;
        GetDeferredShaderProperties(m_mode, shaderProperties, &rpl);

        if (ShouldRecreatePipeline(m_graphicsPipelineCacheHandle, shaderProperties))
        {
            m_graphicsPipelineCacheHandle = CreatePipeline(shaderProperties);
            AssertDebug(!ShouldRecreatePipeline(m_graphicsPipelineCacheHandle, shaderProperties));
        }

        RenderToFramebuffer(frame, rs, nullptr);

        return;
    }
    default:
        HYP_UNREACHABLE();
        return;
    }

    const bool useBindlessTextures = g_renderBackend->GetRenderConfig().bindlessTextures;

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

            const uint32 globalDescriptorSetIndex = pipeline->GetDescriptorTable()->GetDescriptorSetIndex("Global");
            const uint32 viewDescriptorSetIndex = pipeline->GetDescriptorTable()->GetDescriptorSetIndex("View");
            const uint32 materialDescriptorSetIndex = lightType == LT_AREA_RECT
                ? pipeline->GetDescriptorTable()->GetDescriptorSetIndex("Material")
                : ~0u;

            const uint32 deferredDirectDescriptorSetIndex = pipeline->GetDescriptorTable()->GetDescriptorSetIndex("DeferredDirectDescriptorSet");

            if (pipelineChanged)
            {
                pipeline->SetPushConstants(m_pushConstantData.Data(), m_pushConstantData.Size());

                frame->renderQueue << BindGraphicsPipeline(pipeline, viewport);

                // Bind textures globally (bindless)
                if (materialDescriptorSetIndex != ~0u && useBindlessTextures)
                {
                    frame->renderQueue << BindDescriptorSet(
                        pipeline->GetDescriptorTable()->GetDescriptorSet("Material", frame->GetFrameIndex()),
                        pipeline,
                        {},
                        materialDescriptorSetIndex);
                }

                if (deferredDirectDescriptorSetIndex != ~0u)
                {
                    frame->renderQueue << BindDescriptorSet(
                        pipeline->GetDescriptorTable()->GetDescriptorSet("DeferredDirectDescriptorSet", frame->GetFrameIndex()),
                        pipeline,
                        {},
                        deferredDirectDescriptorSetIndex);
                }

                frame->renderQueue << BindVertexBuffer(m_fullScreenQuad->GetVertexBuffer());
                frame->renderQueue << BindIndexBuffer(m_fullScreenQuad->GetIndexBuffer());
            }

            frame->renderQueue << BindDescriptorSet(
                pipeline->GetDescriptorTable()->GetDescriptorSet("Global", frame->GetFrameIndex()),
                pipeline,
                { { "CamerasBuffer", ShaderDataOffset<CameraShaderData>(rs.view->GetCamera()) },
                    { "CurrentLight", ShaderDataOffset<LightShaderData>(light, 0) } },
                globalDescriptorSetIndex);

            frame->renderQueue << BindDescriptorSet(
                rs.passData->descriptorSets[frame->GetFrameIndex()],
                pipeline,
                {},
                viewDescriptorSetIndex);

            // Bind material descriptor set (for area lights)
            if (materialDescriptorSetIndex != ~0u && !useBindlessTextures)
            {
                const DescriptorSetRef& materialDescriptorSet = g_renderGlobalState->materialDescriptorSetManager->ForBoundMaterial(light->GetMaterial(), frame->GetFrameIndex());

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
    Threads::AssertOnThread(g_renderThread);

    FullScreenPass::Create();
}

void TonemapPass::CreatePipeline()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

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

    if (g_renderBackend->GetSwapchain()->IsPqHdr())
    {
        shaderProperties.Set(ShaderProperty(NAME("OUTPUT"), NAME("PQ_HDR")));
    }
    else
    {
        shaderProperties.Set(ShaderProperty(NAME("OUTPUT"), NAME("SDR")));
    }

    m_shader = g_shaderManager->GetOrCreate(NAME("Tonemap"), shaderProperties);

    FullScreenPass::CreatePipeline(renderableAttributes);
}

void TonemapPass::Resize_Internal(Vec2u newSize)
{
    FullScreenPass::Resize_Internal(newSize);
}

void TonemapPass::Render(FrameBase* frame, const RenderSetup& rs)
{
    FullScreenPass::Render(frame, rs);
}

#pragma endregion TonemapPass

#pragma region LightmapPass

struct LightmapVolumeUniforms
{
    float irradianceWeights[4];
    float radianceWeights[4];

    uint32 numAtlases;
};

LightmapPass::LightmapPass()
    : FullScreenPass(FSP_EXTERNAL_FRAMEBUFFER)
{
}

LightmapPass::~LightmapPass()
{
}

void LightmapPass::Create()
{
    Threads::AssertOnThread(g_renderThread);

    FullScreenPass::Create();
}

void LightmapPass::CreatePipeline()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

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

    RenderableAttributeSet renderableAttributes(
        meshAttributes,
        materialAttributes);

    m_shader = g_shaderManager->GetOrCreate(NAME("ApplyLightmap"));

    FullScreenPass::CreatePipeline(renderableAttributes);
}

void LightmapPass::Resize_Internal(Vec2u newSize)
{
    FullScreenPass::Resize_Internal(newSize);
}

void LightmapPass::Render_Internal(FrameBase* frame, const RenderSetup& renderSetup, GraphicsPipelineBase* graphicsPipeline)
{
    HYP_SCOPE;

    constexpr StringHash IrradianceTextureNames[LightmapVolume::MaxAtlases] = {
        StringHash("IrradianceTexture0"),
        StringHash("IrradianceTexture1"),
        StringHash("IrradianceTexture2"),
        StringHash("IrradianceTexture3")
    };

    constexpr StringHash RadianceTextureNames[LightmapVolume::MaxAtlases] = {
        StringHash("RadianceTexture0"),
        StringHash("RadianceTexture1"),
        StringHash("RadianceTexture2"),
        StringHash("RadianceTexture3")
    };

    AssertDebug(renderSetup.lightmapVolume != nullptr);

    RenderProxyLightmapVolume* proxy = static_cast<RenderProxyLightmapVolume*>(RenderApi::GetRenderProxy(renderSetup.lightmapVolume));
    Assert(proxy != nullptr);

    LightmapVolumePassData& data = GetLightmapVolumePassData(renderSetup.lightmapVolume);
    /// @TODO: Add clean up of data after lightmap volume has been removed

    if (!data.descriptorSet
        || !proxy->atlasIrradianceTextures.CompareBitwise(data.atlasIrradianceTextures)
        || !proxy->atlasRadianceTextures.CompareBitwise(data.atlasRadianceTextures))
    {
        AssertDebug(m_shader != nullptr);

        DescriptorSetRef& descriptorSet = data.descriptorSet;

        if (!descriptorSet)
        {
            const DescriptorTableDeclaration* descriptorTableDecl = m_shader->GetCompiledShader()->GetDescriptorTableDeclaration();
            Assert(descriptorTableDecl != nullptr);

            DescriptorSetDeclaration* decl = descriptorTableDecl->FindDescriptorSetDeclaration("LightmapVolume");
            Assert(decl != nullptr);

            const DescriptorSetLayout layout { decl };

            descriptorSet = g_renderBackend->MakeDescriptorSet(layout);
            descriptorSet->SetDebugName(NAME_FMT("LightmapPassDescriptorSet_{}", renderSetup.lightmapVolume->GetName()));

            AssertDebug(descriptorSet != nullptr);
        }

        LightmapVolumeUniforms uniforms {};
        static_assert(HYP_ARRAY_SIZE(LightmapVolumeUniforms::irradianceWeights) == LightmapVolume::MaxAtlases);
        static_assert(HYP_ARRAY_SIZE(LightmapVolumeUniforms::radianceWeights) == LightmapVolume::MaxAtlases);

        uniforms.numAtlases = proxy->numAtlases;

        GpuBufferRef uniformBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::CBUFF, sizeof(LightmapVolumeUniforms));
        Assert(uniformBuffer->Create());

        for (uint32 atlasIndex = 0; atlasIndex < LightmapVolume::MaxAtlases; atlasIndex++)
        {
            Texture* irradianceTexture = atlasIndex < proxy->numAtlases ? proxy->atlasIrradianceTextures[atlasIndex] : nullptr;
            Texture* radianceTexture = atlasIndex < proxy->numAtlases ? proxy->atlasRadianceTextures[atlasIndex] : nullptr;

            uniforms.irradianceWeights[atlasIndex] = irradianceTexture ? 1.0f : 0.0f;
            uniforms.radianceWeights[atlasIndex] = radianceTexture ? 1.0f : 0.0f;

            descriptorSet->SetElement(IrradianceTextureNames[atlasIndex], g_renderBackend->GetTextureImageView(irradianceTexture != nullptr ? MakeStrongRef(irradianceTexture) : g_renderGlobalState->placeholderData->defaultTexture2d));
            descriptorSet->SetElement(RadianceTextureNames[atlasIndex], g_renderBackend->GetTextureImageView(radianceTexture != nullptr ? MakeStrongRef(radianceTexture) : g_renderGlobalState->placeholderData->defaultTexture2d));
        }

        uniformBuffer->Copy(sizeof(uniforms), &uniforms);

        descriptorSet->SetElement("LightmapVolumeUniforms", uniformBuffer);

        if (descriptorSet->IsCreated())
        {
            descriptorSet->Update(true);
        }
        else
        {
            Assert(descriptorSet->Create());
        }

        data.atlasIrradianceTextures = proxy->atlasIrradianceTextures;
        data.atlasRadianceTextures = proxy->atlasRadianceTextures;
    }

    const uint32 descriptorSetIndex = graphicsPipeline->GetDescriptorTable()->GetDescriptorSetIndex("LightmapVolume");
    AssertDebug(descriptorSetIndex != ~0u);

    frame->renderQueue << BindDescriptorSet(
        data.descriptorSet,
        graphicsPipeline,
        {},
        descriptorSetIndex);
}

#pragma endregion LightmapPass

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
    Threads::AssertOnThread(g_renderThread);

    FullScreenPass::Create();
}

void EnvGridPass::CreatePipeline()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

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

        DeferCreate(descriptorTable);

        GraphicsPipelineCacheHandle cacheHandle = g_renderGlobalState->graphicsPipelineCache->GetOrCreate(
            shader,
            descriptorTable,
            { &m_framebuffer, 1 },
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

void EnvGridPass::Render(FrameBase* frame, const RenderSetup& rs)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    AssertDebug(rs.IsValid());
    AssertDebug(rs.HasView());
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

        const uint32 globalDescriptorSetIndex = graphicsPipeline->GetDescriptorTable()->GetDescriptorSetIndex("Global");
        const uint32 viewDescriptorSetIndex = graphicsPipeline->GetDescriptorTable()->GetDescriptorSetIndex("View");

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
            graphicsPipeline->GetDescriptorTable()->GetDescriptorSet("Global", frameIndex),
            graphicsPipeline,
            { { "CamerasBuffer", ShaderDataOffset<CameraShaderData>(rs.view->GetCamera()) },
                { "EnvGridsBuffer", ShaderDataOffset<EnvGridShaderData>(envGrid, 0) } },
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
    : FullScreenPass(TF_RGBA16F, extent, gbuffer),
      m_mipChainImageView(mipChainImageView),
      m_deferredResultImageView(deferredResultImageView),
      m_isFirstFrame(true)
{
    SetBlendFunction(BlendFunction(
        BMF_SRC_ALPHA, BMF_ONE_MINUS_SRC_ALPHA,
        BMF_ONE, BMF_ONE_MINUS_SRC_ALPHA));
}

ReflectionsPass::~ReflectionsPass()
{
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
        .blendFunction = BlendFunction(BMF_SRC_ALPHA, BMF_ONE_MINUS_SRC_ALPHA,
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
    Threads::AssertOnThread(g_renderThread);

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

        DeferCreate(descriptorTable);

        GraphicsPipelineCacheHandle cacheHandle = g_renderGlobalState->graphicsPipelineCache->GetOrCreate(
            shader,
            descriptorTable,
            { &m_framebuffer, 1 },
            renderableAttributes);

        m_cubemapGraphicsPipelines[it.first] = std::move(cacheHandle);
    }

    m_graphicsPipelineCacheHandle = m_cubemapGraphicsPipelines[CMT_DEFAULT];
}

bool ReflectionsPass::ShouldRenderSSR() const
{
    static const ConfigurationValue& s_ssrEnabled = CoreApi_GetGlobalConfig().Get("Rendering.SSR.Enabled");
    static const ConfigurationValue& s_raytracingReflectionsEnabled = CoreApi_GetGlobalConfig().Get("Rendering.RayTracing.Reflections.Enabled");

    return s_ssrEnabled.ToBool(true) && !s_raytracingReflectionsEnabled.ToBool(false);
}

void ReflectionsPass::CreateSSRRenderer()
{
    m_ssrRenderer = MakeUnique<SSRRenderer>(SSRRendererConfig::FromConfig(), m_gbuffer, m_mipChainImageView, m_deferredResultImageView);
    m_ssrRenderer->Create();

    ShaderRef renderTextureToScreenShader = g_shaderManager->GetOrCreate(NAME("RenderTextureToScreen"));
    Assert(renderTextureToScreenShader.IsValid());

    DescriptorTableRef descriptorTable = g_renderBackend->MakeDescriptorTable(
        renderTextureToScreenShader->GetCompiledShader()->GetDescriptorTableDeclaration());

    for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
    {
        const DescriptorSetRef& descriptorSet = descriptorTable->GetDescriptorSet("RenderTextureToScreenDescriptorSet", frameIndex);
        Assert(descriptorSet != nullptr);

        descriptorSet->SetElement("InTexture", g_renderBackend->GetTextureImageView(m_ssrRenderer->GetFinalResultTexture()));
    }

    DeferCreate(descriptorTable);

    m_renderSsrToScreenPass = CreateObject<FullScreenPass>(
        renderTextureToScreenShader,
        std::move(descriptorTable),
        m_imageFormat,
        m_extent,
        m_gbuffer);

    // Use alpha blending to blend SSR into the reflection probes
    m_renderSsrToScreenPass->SetBlendFunction(BlendFunction(
        BMF_SRC_ALPHA, BMF_ONE_MINUS_SRC_ALPHA,
        BMF_ONE, BMF_ONE_MINUS_SRC_ALPHA));

    m_renderSsrToScreenPass->Create();
}

void ReflectionsPass::Resize_Internal(Vec2u newSize)
{
    HYP_SCOPE;

    SafeDelete(std::move(m_mipChainImageView));
    SafeDelete(std::move(m_deferredResultImageView));

    FullScreenPass::Resize_Internal(newSize);

    m_cubemapGraphicsPipelines = {};

    if (ShouldRenderSSR())
    {
        CreateSSRRenderer();
    }
}

void ReflectionsPass::Render(FrameBase* frame, const RenderSetup& rs)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    AssertDebug(rs.IsValid());
    AssertDebug(rs.HasView());
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

    FixedArray<Array<EnvProbe*>, CMT_MAX> probesPerCubemapType;

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

        const Array<EnvProbe*>& probes = probesPerCubemapType[cubemapType];

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

        const uint32 globalDescriptorSetIndex = graphicsPipeline->GetDescriptorTable()->GetDescriptorSetIndex("Global");
        const uint32 viewDescriptorSetIndex = graphicsPipeline->GetDescriptorTable()->GetDescriptorSetIndex("View");

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
                graphicsPipeline->GetDescriptorTable()->GetDescriptorSet("Global", frameIndex),
                graphicsPipeline,
                { { "CamerasBuffer", ShaderDataOffset<CameraShaderData>(rs.view->GetCamera()) },
                    { "CurrentEnvProbe", ShaderDataOffset<EnvProbeShaderData>(envProbe) } },
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
}

#pragma endregion RaytracingPassData

#pragma region DeferredRenderer

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

        const FramebufferRef& opaqueFbo = view->GetOutputTarget().GetFramebuffer(RB_OPAQUE);
        const FramebufferRef& lightmapFbo = view->GetOutputTarget().GetFramebuffer(RB_LIGHTMAP);
        const FramebufferRef& translucentFbo = view->GetOutputTarget().GetFramebuffer(RB_TRANSLUCENT);

        CHECK_FRAMEBUFFER_SIZE(opaqueFbo);
        CHECK_FRAMEBUFFER_SIZE(lightmapFbo);
        CHECK_FRAMEBUFFER_SIZE(translucentFbo);

        passData.envGridRadiancePass = CreateObject<EnvGridPass>(EGPM_RADIANCE, passData.viewport.extent, gbuffer);
        passData.envGridRadiancePass->Create();

        passData.envGridIrradiancePass = CreateObject<EnvGridPass>(EGPM_IRRADIANCE, passData.viewport.extent, gbuffer);
        passData.envGridIrradiancePass->Create();

        passData.ssgi = MakeUnique<SSGI>(SSGIConfig::FromConfig(), gbuffer);
        passData.ssgi->Create();

        passData.postProcessing = MakeUnique<PostProcessing>();
        passData.postProcessing->Create();

        passData.indirectPass = CreateObject<DeferredPass>(DPM_INDIRECT_LIGHTING, passData.viewport.extent, gbuffer);
        passData.indirectPass->Create();

        passData.directPass = CreateObject<DeferredPass>(DPM_DIRECT_LIGHTING, passData.viewport.extent, gbuffer);
        passData.directPass->Create();

        passData.depthPyramidRenderer = MakeUnique<DepthPyramidRenderer>(gbuffer);
        passData.depthPyramidRenderer->Create();

        passData.cullData.depthPyramidImageView = passData.depthPyramidRenderer->GetResultImageView();
        passData.cullData.depthPyramidDimensions = passData.depthPyramidRenderer->GetExtent();

        passData.mipChain = CreateObject<Texture>(TextureDesc {
            TT_TEX2D,
            opaqueFbo->GetAttachment(0)->GetFormat(),
            Vec3u(opaqueFbo->GetExtent(), 1),
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

        passData.temporalAa = MakeUnique<TemporalAA>(passData.tonemapPass->GetFinalImageView(), passData.viewport.extent, gbuffer);
        passData.temporalAa->Create();

        CreateViewDescriptorSets(view, passData);
        CreateViewFinalPassDescriptorSet(view, passData);
        CreateViewRaytracingPasses(view, passData);

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

    ShaderRef renderTextureToScreenShader = g_shaderManager->GetOrCreate(NAME("RenderTextureToScreen_UI"));
    Assert(renderTextureToScreenShader.IsValid());

    const GpuImageViewRef& inputImageView = m_rendererConfig.taaEnabled
        ? g_renderBackend->GetTextureImageView(passData.temporalAa->GetResultTexture())
        : passData.combinePass->GetFinalImageView();

    Assert(inputImageView.IsValid());

    const DescriptorTableDeclaration* descriptorTableDecl = renderTextureToScreenShader->GetCompiledShader()->GetDescriptorTableDeclaration();
    Assert(descriptorTableDecl != nullptr);

    DescriptorSetDeclaration* decl = descriptorTableDecl->FindDescriptorSetDeclaration("RenderTextureToScreenDescriptorSet");
    Assert(decl != nullptr);

    const DescriptorSetLayout layout { decl };

    DescriptorSetRef descriptorSet = g_renderBackend->MakeDescriptorSet(layout);
    descriptorSet->SetDebugName(NAME("FinalPassDescriptorSet"));
    descriptorSet->SetElement("InTexture", inputImageView);

    Assert(descriptorSet->Create());

    SafeDelete(std::move(passData.finalPassDescriptorSet));

    passData.finalPassDescriptorSet = std::move(descriptorSet);
}

void DeferredRenderer::CreateViewDescriptorSets(View* view, DeferredRendererPassData& passData)
{
    HYP_SCOPE;

    const DescriptorSetDeclaration* decl = g_renderGlobalState->globalDescriptorTable->GetDeclaration()->FindDescriptorSetDeclaration("View");
    Assert(decl != nullptr);

    const DescriptorSetLayout layout { decl };

    FixedArray<DescriptorSetRef, NumFramesInFlight> descriptorSets;

    const FramebufferRef& opaqueFbo = view->GetOutputTarget().GetFramebuffer(RB_OPAQUE);
    const FramebufferRef& lightmapFbo = view->GetOutputTarget().GetFramebuffer(RB_LIGHTMAP);
    const FramebufferRef& translucentFbo = view->GetOutputTarget().GetFramebuffer(RB_TRANSLUCENT);

    // depth attachment goes into separate slot
    AttachmentBase* depthAttachment = opaqueFbo->GetAttachment(GTN_MAX - 1);
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
                descriptorSet->SetElement("GBufferTextures", gbufferElementIndex++, opaqueFbo->GetAttachment(attachmentIndex)->GetImageView());
            }

            // add translucent bucket's albedo
            descriptorSet->SetElement("GBufferTextures", gbufferElementIndex++, translucentFbo->GetAttachment(0)->GetImageView());
        }
        else
        {
            for (uint32 attachmentIndex = 0; attachmentIndex < GTN_MAX - 1; attachmentIndex++)
            {
                descriptorSet->SetElement(GBufferTextureNames[attachmentIndex], opaqueFbo->GetAttachment(attachmentIndex)->GetImageView());
            }

            // add translucent bucket's albedo
            descriptorSet->SetElement("GBufferTranslucentTexture", translucentFbo->GetAttachment(0)->GetImageView());
        }

        descriptorSet->SetElement("GBufferDepthTexture", depthAttachment->GetImageView());

        descriptorSet->SetElement("GBufferMipChain", g_renderBackend->GetTextureImageView(passData.mipChain));

        descriptorSet->SetElement("PostProcessingUniforms", passData.postProcessing->GetUniformBuffer());

        descriptorSet->SetElement("DepthPyramidResult", passData.depthPyramidRenderer->GetResultImageView());

        descriptorSet->SetElement("TAAResultTexture", g_renderBackend->GetTextureImageView(passData.temporalAa->GetResultTexture()));

        if (passData.reflectionsPass->ShouldRenderSSR())
        {
            descriptorSet->SetElement("SSRResultTexture", g_renderBackend->GetTextureImageView(passData.reflectionsPass->GetSSRRenderer()->GetFinalResultTexture()));
        }
        else
        {
            descriptorSet->SetElement("SSRResultTexture", g_renderGlobalState->placeholderData->GetImageView2D1x1R8());
        }

        if (passData.ssgi)
        {
            descriptorSet->SetElement("SSGIResultTexture", g_renderBackend->GetTextureImageView(passData.ssgi->GetFinalResultTexture()));
        }
        else
        {
            descriptorSet->SetElement("SSGIResultTexture", g_renderGlobalState->placeholderData->GetImageView2D1x1R8());
        }

        if (passData.hbao)
        {
            descriptorSet->SetElement("SSAOResultTexture", passData.hbao->GetFinalImageView());
        }
        else
        {
            descriptorSet->SetElement("SSAOResultTexture", g_renderGlobalState->placeholderData->GetImageView2D1x1R8());
        }

        descriptorSet->SetElement("DeferredResult", passData.combinePass->GetFinalImageView());

        descriptorSet->SetElement("DeferredIndirectResultTexture", passData.indirectPass->GetFinalImageView());

        descriptorSet->SetElement("ReflectionProbeResultTexture", passData.reflectionsPass->GetFinalImageView());

        descriptorSet->SetElement("EnvGridRadianceResultTexture", passData.envGridRadiancePass->GetFinalImageView());
        descriptorSet->SetElement("EnvGridIrradianceResultTexture", passData.envGridIrradiancePass->GetFinalImageView());

        HYP_GFX_ASSERT(descriptorSet->Create());

        descriptorSets[frameIndex] = std::move(descriptorSet);
    }

    passData.descriptorSets = std::move(descriptorSets);
}

void DeferredRenderer::CreateViewCombinePass(View* view, DeferredRendererPassData& passData)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    // The combine pass will render into the translucent bucket's framebuffer with the shaded result.
    const FramebufferRef& translucentFbo = view->GetOutputTarget().GetFramebuffer(RB_TRANSLUCENT);
    Assert(translucentFbo != nullptr);

    ShaderRef renderTextureToScreenShader = g_shaderManager->GetOrCreate(NAME("RenderTextureToScreen_UI"));
    Assert(renderTextureToScreenShader.IsValid());

    DescriptorTableRef descriptorTable = g_renderBackend->MakeDescriptorTable(
        renderTextureToScreenShader->GetCompiledShader()->GetDescriptorTableDeclaration());

    for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
    {
        const DescriptorSetRef& descriptorSet = descriptorTable->GetDescriptorSet("RenderTextureToScreenDescriptorSet", frameIndex);
        Assert(descriptorSet != nullptr);

        descriptorSet->SetElement("InTexture", passData.indirectPass->GetFinalImageView());
    }

    DeferCreate(descriptorTable);

    passData.combinePass = CreateObject<FullScreenPass>(
        renderTextureToScreenShader,
        std::move(descriptorTable),
        translucentFbo,
        translucentFbo->GetAttachment(0)->GetFormat(),
        translucentFbo->GetExtent(),
        nullptr);

    passData.combinePass->Create();
}

void DeferredRenderer::CreateViewRaytracingPasses(View* view, DeferredRendererPassData& passData)
{
    if (!g_renderBackend->GetRenderConfig().raytracing)
    {
        return;
    }

    const bool shouldEnableRaytracingForView = view->GetRaytracingView().IsValid()
        && CoreApi_GetGlobalConfig().Get("Rendering.RayTracing.Enabled").ToBool();

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
    Handle<Mesh> defaultMesh = MeshBuilder::Cube();
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
    Threads::AssertOnThread(g_renderThread);

    HYP_LOG(Rendering, Debug, "Resizing View '{}' to {}x{}", view->Id(), viewport.extent.x, viewport.extent.y);

    Assert(viewport.extent.Volume() > 0);

    passData.viewport = viewport;

    const Vec2u newSize = Vec2u(viewport.extent);

    GBuffer* gbuffer = view->GetOutputTarget().GetGBuffer();
    Assert(gbuffer != nullptr && gbuffer->IsCreated());

    gbuffer->Resize(newSize);

    const FramebufferRef& opaqueFbo = view->GetOutputTarget().GetFramebuffer(RB_OPAQUE);
    const FramebufferRef& lightmapFbo = view->GetOutputTarget().GetFramebuffer(RB_LIGHTMAP);
    const FramebufferRef& translucentFbo = view->GetOutputTarget().GetFramebuffer(RB_TRANSLUCENT);

    CHECK_FRAMEBUFFER_SIZE(opaqueFbo);
    CHECK_FRAMEBUFFER_SIZE(lightmapFbo);
    CHECK_FRAMEBUFFER_SIZE(translucentFbo);

    passData.hbao->Resize(newSize);

    passData.directPass->Resize(newSize);
    passData.indirectPass->Resize(newSize);

    passData.combinePass.Reset();
    CreateViewCombinePass(view, passData);

    passData.envGridRadiancePass->Resize(newSize);
    passData.envGridIrradiancePass->Resize(newSize);

    passData.reflectionsPass->Resize(newSize);

    passData.tonemapPass->Resize(newSize);

    passData.lightmapPass = CreateObject<LightmapPass>(translucentFbo, newSize, gbuffer);
    passData.lightmapPass->Create();

    passData.temporalAa = MakeUnique<TemporalAA>(passData.tonemapPass->GetFinalImageView(), newSize, gbuffer);
    passData.temporalAa->Create();

    passData.depthPyramidRenderer = MakeUnique<DepthPyramidRenderer>(gbuffer);
    passData.depthPyramidRenderer->Create();

    SafeDelete(std::move(passData.descriptorSets));
    CreateViewDescriptorSets(view, passData);

    SafeDelete(std::move(passData.finalPassDescriptorSet));
    CreateViewFinalPassDescriptorSet(view, passData);

    CreateViewRaytracingPasses(view, passData);

    passData.view = MakeWeakRef(view);
    passData.viewport = viewport;
}

void DeferredRenderer::RenderFrame(FrameBase* frame, const RenderSetup& rs)
{
    HYP_SCOPE;

    Assert(rs.IsValid());

    Array<RenderProxyList*, InlineAllocator<3, RenderAllocator>> renderProxyLists;

    HYP_DEFER({
        for (RenderProxyList* rpl : renderProxyLists)
        {
            rpl->EndRead();
        }
    });

    // Collect view-independent renderable types from all views, binned
    /// @TODO: We could use the existing binning by subclass that ResourceTracker now provides.
    FixedArray<FlatSet<EnvProbe*>, EPT_MAX> envProbes;
    FixedArray<FlatSet<Light*>, LT_MAX> lights;
    FlatSet<EnvGrid*> envGrids;

    // For rendering EnvGrids and EnvProbes, we use a directional light from one of the Views that references it (if found)
    FlatMap<EnvGrid*, Light*> envGridLights;
    FlatMap<EnvProbe*, Light*> envProbeLights;

    // Render UI to render targets
    for (RendererBase* renderer : g_renderGlobalState->globalRenderers[GRT_UI])
    {
        renderer->RenderFrame(frame, rs);
    }

    // init view pass data and collect global rendering resources
    // (env probes, env grids)
    for (View* view : rs.world->GetViews())
    {
        AssertDebug(view != nullptr);

        RenderProxyList& rpl = RenderApi::GetConsumerProxyList(view);
        rpl.BeginRead();

        renderProxyLists.PushBack(&rpl);

        const Handle<PassData>& pd = FetchViewPassData(view);
        Assert(pd != nullptr);

        if (view->GetFlags() & ViewFlags::GBUFFER)
        {
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
            RaytracingPassData* pdCasted = ObjCast<RaytracingPassData>(pd.Get());
            Assert(pdCasted != nullptr);

            RenderSetup newRs = rs.Fork();
            newRs.passData = pd;
            newRs.view = view;

            UpdateRaytracingView(frame, newRs);
        }

        for (Light* light : rpl.GetLights())
        {
            AssertDebug(light != nullptr);

            lights[light->GetLightType()].Insert(light);
        }

        for (EnvProbe* envProbe : rpl.GetEnvProbes())
        {
            if (envProbe->IsControlledByEnvGrid())
            {
                // skip it if it is controlled by an EnvGrid, we don't handle them here
                continue;
            }

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
        RendererBase* shadowRenderer = g_renderGlobalState->globalRenderers[GRT_SHADOW_MAP][lightType];

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
        RenderSetup newRs = rs.Fork();

        // Set sky as fallback probe
        if (envProbes[EPT_SKY].Any())
        {
            newRs.envProbe = envProbes[EPT_SKY].Front();
        }

        if (lights[LT_DIRECTIONAL].Any())
        {
            newRs.light = lights[LT_DIRECTIONAL].Front();
        }

        if (envProbes.Any())
        {
            for (uint32 envProbeType = 0; envProbeType <= EPT_REFLECTION; envProbeType++)
            {
                if (RendererBase* renderer = g_renderGlobalState->globalRenderers[GRT_ENV_PROBE][envProbeType])
                {
                    for (EnvProbe* envProbe : envProbes[envProbeType])
                    {
                        RenderSetup envProbeRs = newRs.Fork();
                        envProbeRs.envProbe = envProbe;

                        renderer->RenderFrame(frame, envProbeRs);
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
                RenderSetup envGridRs = newRs.Fork();

                // Set global directional light as fallback
                if (envGridLights.Contains(envGrid))
                {
                    envGridRs.light = envGridLights[envGrid];
                }

                envGridRs.envGrid = envGrid;

                g_renderGlobalState->globalRenderers[GRT_ENV_GRID][0]->RenderFrame(frame, envGridRs);

                envGridRs.light = nullptr;
                envGridRs.envGrid = nullptr;
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

        RenderSetup newRs = rs.Fork();

        const Handle<DeferredRendererPassData>& pd = ObjCast<DeferredRendererPassData>(FetchViewPassData(view));
        AssertDebug(pd != nullptr);
        AssertDebug(pd->viewport.extent.Volume() != 0);

        newRs.view = view;
        newRs.passData = pd;

        RenderFrameForView(frame, newRs);

        newRs.view = nullptr;
        newRs.passData = nullptr;

        if (view->GetFlags() & ViewFlags::ENABLE_READBACK)
        {
            GpuImageBase* dstImage = view->GetReadbackTextureGpuImage();

            if (dstImage != nullptr)
            {
                GpuImageBase* srcImage = m_rendererConfig.taaEnabled
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
        // RenderProxyList already be in read state (see above)

        g_statViews++;
        g_statTextures += rpl.GetTextures().NumCurrent();
        g_statMaterials += rpl.GetMaterials().NumCurrent();
        g_statLightmapVolumes += rpl.GetLightmapVolumes().NumCurrent();
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

void DeferredRenderer::RenderFrameForView(FrameBase* frame, const RenderSetup& rs)
{
    HYP_SCOPE;

    Assert(rs.IsValid());
    Assert(rs.HasView());

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

    const FramebufferRef& opaqueFbo = view->GetOutputTarget().GetFramebuffer(RB_OPAQUE);
    const FramebufferRef& lightmapFbo = view->GetOutputTarget().GetFramebuffer(RB_LIGHTMAP);
    const FramebufferRef& translucentFbo = view->GetOutputTarget().GetFramebuffer(RB_TRANSLUCENT);

    CHECK_FRAMEBUFFER_SIZE(opaqueFbo);
    CHECK_FRAMEBUFFER_SIZE(lightmapFbo);
    CHECK_FRAMEBUFFER_SIZE(translucentFbo);

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

    const bool useTemporalAa = passData.temporalAa != nullptr && m_rendererConfig.taaEnabled;

    const bool useReflectionProbes = rpl.GetEnvProbes().GetElements<SkyProbe>().Any()
        || rpl.GetEnvProbes().GetElements<ReflectionProbe>().Any();

    if (useTemporalAa)
    {
        // apply jitter to camera for TAA
        RenderProxyCamera* cameraProxy = static_cast<RenderProxyCamera*>(RenderApi::GetRenderProxy(view->GetCamera()));
        Assert(cameraProxy != nullptr);

        CameraShaderData& cameraBufferData = cameraProxy->bufferData;

        if (cameraBufferData.projection[3][3] < MathUtil::epsilonF)
        {
            const uint32 frameCounter = RenderApi::GetWorldBufferData()->frameCounter + 1;

            Vec4f jitter = Vec4f::Zero();
            Mat4f::Jitter(frameCounter, cameraBufferData.dimensions.x, cameraBufferData.dimensions.y, jitter);

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

    PerformOcclusionCulling(frame, rs, renderCollector);

    // if (doParticles)
    // {
    //     environment->GetParticleSystem()->UpdateParticles(frame, rs);
    // }

    // if (doGaussianSplatting)
    // {
    //     environment->GetGaussianSplatting()->UpdateSplats(frame, rs);
    // }

    passData.indirectPass->SetPushConstants(&deferredData, sizeof(deferredData));
    passData.directPass->SetPushConstants(&deferredData, sizeof(deferredData));

    { // render opaque objects into separate framebuffer
        frame->renderQueue << BeginFramebuffer(opaqueFbo);

        ExecuteDrawCalls(frame, rs, renderCollector, (1u << RB_OPAQUE));

        frame->renderQueue << EndFramebuffer(opaqueFbo);
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

    if (useReflectionProbes)
    {
        passData.reflectionsPass->SetPushConstants(&deferredData, sizeof(deferredData));
        passData.reflectionsPass->Render(frame, rs);
    }

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

                RenderSetup newRs = rs;
                newRs.passData = raytracingPassData;

                if (useRaytracingReflections)
                {
                    AssertDebug(passData.raytracingReflections != nullptr);
                    passData.raytracingReflections->Render(frame, newRs);
                }

                if (useRaytracingGlobalIllumination)
                {
                    AssertDebug(passData.ddgi != nullptr);
                    passData.ddgi->Render(frame, newRs);
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

        // if (const auto& skyProbes = rpl.trackedEnvProbes.GetElements<SkyProbe>(); skyProbes.Any())
        if (const auto& skyProbes = rpl.GetEnvProbes().GetElements<SkyProbe>(); skyProbes.Any())
        {
            newRenderSetup.envProbe = skyProbes.Front();
        }

        passData.ssgi->Render(frame, rs);
    }

    passData.postProcessing->RenderPre(frame, rs);

    // render indirect and direct lighting into the same framebuffer
    const FramebufferRef& deferredPassFramebuffer = passData.indirectPass->GetFramebuffer();
    CHECK_FRAMEBUFFER_SIZE(deferredPassFramebuffer);

    { // deferred lighting on opaque objects
        frame->renderQueue << BeginFramebuffer(deferredPassFramebuffer);

        passData.indirectPass->Render(frame, rs);
        passData.directPass->Render(frame, rs);

        frame->renderQueue << EndFramebuffer(deferredPassFramebuffer);
    }

    if (rpl.GetLightmapVolumes().NumCurrent())
    { // render objects to be lightmapped, separate from the opaque objects.
        // The lightmap bucket's framebuffer has a color attachment that will write into the opaque framebuffer's color attachment.
        frame->renderQueue << BeginFramebuffer(lightmapFbo);

        ExecuteDrawCalls(frame, rs, renderCollector, (1u << RB_LIGHTMAP));

        frame->renderQueue << EndFramebuffer(lightmapFbo);
    }

    { // generate mipchain after rendering opaque objects' lighting, now we can use it for transmission
        const GpuImageRef& srcImage = deferredPassFramebuffer->GetAttachment(0)->GetImage();
        GenerateMipChain(frame, rs, renderCollector, srcImage);
    }

    { // translucent objects
        frame->renderQueue << BeginFramebuffer(translucentFbo);

        { // Render the deferred lighting into the translucent pass framebuffer with a full screen quad.
            const GraphicsPipelineRef& pipeline = passData.combinePass->GetGraphicsPipeline();
            AssertDebug(pipeline != nullptr);

            frame->renderQueue << BindGraphicsPipeline(pipeline, viewport);

            frame->renderQueue << BindDescriptorTable(
                pipeline->GetDescriptorTable(),
                pipeline,
                {},
                frameIndex);

            frame->renderQueue << BindVertexBuffer(passData.combinePass->GetQuadMesh()->GetVertexBuffer());
            frame->renderQueue << BindIndexBuffer(passData.combinePass->GetQuadMesh()->GetIndexBuffer());
            frame->renderQueue << DrawIndexed(passData.combinePass->GetQuadMesh()->NumIndices());
        }

        for (LightmapVolume* lightmapVolume : rpl.GetLightmapVolumes())
        {
            RenderSetup newRs = rs.Fork();
            newRs.lightmapVolume = lightmapVolume;

            // Render the objects to have lightmaps applied into the translucent pass framebuffer with a full screen quad.
            // Apply lightmaps over the now shaded opaque objects.

            // @TODO: Use stencil buffer to separate by lightmap volume ID
            passData.lightmapPass->RenderToFramebuffer(frame, newRs, translucentFbo);
        }

        // begin translucent with forward rendering
        ExecuteDrawCalls(frame, rs, renderCollector, (1u << RB_TRANSLUCENT));
        ExecuteDrawCalls(frame, rs, renderCollector, (1u << RB_DEBUG));

        // if (doParticles)
        // {
        //     environment->GetParticleSystem()->Render(frame, rs);
        // }

        // if (doGaussianSplatting)
        // {
        //     environment->GetGaussianSplatting()->Render(frame, rs);
        // }

        ExecuteDrawCalls(frame, rs, renderCollector, (1u << RB_SKYBOX));

        // render debug draw
        g_engineDriver->GetDebugDrawer()->Render(frame, rs);

        frame->renderQueue << EndFramebuffer(translucentFbo);
    }

    { // render depth pyramid
        passData.depthPyramidRenderer->Render(frame);
        // update culling info now that depth pyramid has been rendered
        passData.cullData.depthPyramidImageView = passData.depthPyramidRenderer->GetResultImageView();
        passData.cullData.depthPyramidDimensions = passData.depthPyramidRenderer->GetExtent();
    }

    passData.postProcessing->RenderPost(frame, rs);

    passData.tonemapPass->Render(frame, rs);

    if (useTemporalAa)
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

void DeferredRenderer::UpdateRaytracingView(FrameBase* frame, const RenderSetup& rs)
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

        GpuBlasRef& blas = meshProxy->raytracingData.blas;

        const bool materialsDiffer = blas != nullptr
            && blas->GetMaterial() != meshProxy->material;

        if (!blas || materialsDiffer)
        {
            if (blas != nullptr)
            {
                for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
                {
                    pd->raytracingTlases[frameIndex]->RemoveGpuBlas(blas);
                }

                SafeDelete(std::move(blas));
            }

            blas = MeshBlasBuilder::Build(meshProxy->mesh, meshProxy->material);
            Assert(blas != nullptr);

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

void DeferredRenderer::PerformOcclusionCulling(FrameBase* frame, const RenderSetup& rs, RenderCollector& renderCollector)
{
    HYP_SCOPE;

    constexpr uint32 BucketMask = (1 << RB_OPAQUE)
        | (1 << RB_LIGHTMAP)
        | (1 << RB_SKYBOX)
        | (1 << RB_TRANSLUCENT)
        | (1 << RB_DEBUG);

    renderCollector.PerformOcclusionCulling(frame, rs, BucketMask);
}

void DeferredRenderer::ExecuteDrawCalls(FrameBase* frame, const RenderSetup& rs, RenderCollector& renderCollector, uint32 bucketMask)
{
    HYP_SCOPE;

    renderCollector.ExecuteDrawCalls(frame, rs, bucketMask);
}

void DeferredRenderer::GenerateMipChain(FrameBase* frame, const RenderSetup& rs, RenderCollector& renderCollector, const GpuImageRef& srcImage)
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

} // namespace hyperion
