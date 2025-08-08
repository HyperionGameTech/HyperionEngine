/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/Defines.hpp>

#include <rendering/Deferred.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/DepthPyramidRenderer.hpp>
#include <rendering/env_grid/EnvGridRenderer.hpp>
#include <rendering/env_probe/EnvProbeRenderer.hpp>
#include <rendering/RenderMaterial.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/SafeDeleter.hpp>
#include <rendering/GraphicsPipelineCache.hpp>
#include <rendering/SSRRenderer.hpp>
#include <rendering/SSGI.hpp>
#include <rendering/HbaoPass.hpp>
#include <rendering/DepthOfField.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/Material.hpp>
#include <rendering/Texture.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/rt/MeshBlasBuilder.hpp>
#include <rendering/rt/RaytracingReflections.hpp>
#include <rendering/rt/DDGI.hpp>

#include <rendering/debug/DebugDrawer.hpp>

#include <rendering/RenderObject.hpp>
#include <rendering/RenderGpuBuffer.hpp>
#include <rendering/RenderDevice.hpp>
#include <rendering/RenderDescriptorSet.hpp>
#include <rendering/RenderGraphicsPipeline.hpp>

#include <scene/World.hpp>
#include <scene/View.hpp>
#include <scene/EnvGrid.hpp>
#include <scene/EnvProbe.hpp>

#include <core/config/Config.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/filesystem/FsUtil.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <system/AppContext.hpp>

#include <util/Float16.hpp>

#include <util/MeshBuilder.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>

namespace hyperion {

static constexpr float g_cameraJitterScale = 0.25f;

static constexpr TextureFormat g_envGridRadianceFormat = TF_RGBA8;
static constexpr TextureFormat g_envGridIrradianceFormat = TF_R11G11B10F;

static constexpr TextureFormat g_envGridPassFormats[EGPM_MAX] = {
    g_envGridRadianceFormat,  // EGPM_RADIANCE
    g_envGridIrradianceFormat // EGPM_IRRADIANCE
};

static const Float16 g_ltcMatrix[] = {
#include <rendering/inl/LTCMatrix.inl>
};

static_assert(sizeof(g_ltcMatrix) == 64 * 64 * 4 * 2, "Invalid LTC matrix size");

static const Float16 g_ltcBrdf[] = {
#include <rendering/inl/LTCBRDF.inl>
};

static_assert(sizeof(g_ltcBrdf) == 64 * 64 * 4 * 2, "Invalid LTC BRDF size");

// Maps individual light types to per-light specific properties.
static const FixedArray<ShaderProperties, LT_MAX> g_deferredLightTypeProperties {
    ShaderProperties { { NAME("LIGHT_TYPE_DIRECTIONAL") } },
    ShaderProperties { { NAME("LIGHT_TYPE_POINT") } },
    ShaderProperties { { NAME("LIGHT_TYPE_SPOT") } },
    ShaderProperties { { NAME("LIGHT_TYPE_AREA_RECT") } }
};

HYP_API extern const GlobalConfig& GetGlobalConfig();

void GetDeferredShaderProperties(ShaderProperties& outShaderProperties)
{
    static const GlobalConfig& globalConfig = GetGlobalConfig();
    static const IRenderConfig& renderConfig = g_renderBackend->GetRenderConfig();

    outShaderProperties.Set(NAME("RT_REFLECTIONS_ENABLED"), renderConfig.IsRaytracingSupported() && globalConfig.Get("rendering.raytracing.reflections.enabled").ToBool());
    outShaderProperties.Set(NAME("RT_GI_ENABLED"), renderConfig.IsRaytracingSupported() && globalConfig.Get("rendering.raytracing.globalIllumination.enabled").ToBool());
    outShaderProperties.Set(NAME("ENV_GRID_ENABLED"), globalConfig.Get("rendering.envGrid.globalIllumination.enabled").ToBool());
    outShaderProperties.Set(NAME("HBIL_ENABLED"), globalConfig.Get("rendering.hbil.enabled").ToBool());
    outShaderProperties.Set(NAME("HBAO_ENABLED"), globalConfig.Get("rendering.hbao.enabled").ToBool());

    if (globalConfig.Get("rendering.raytracing.pathTracing.enabled").ToBool())
    {
        outShaderProperties.Set(NAME("PATHTRACER"));
    }
    else if (globalConfig.Get("rendering.debug.reflections").ToBool())
    {
        outShaderProperties.Set(NAME("DEBUG_REFLECTIONS"));
    }
    else if (globalConfig.Get("rendering.debug.irradiance").ToBool())
    {
        outShaderProperties.Set(NAME("DEBUG_IRRADIANCE"));
    }
}

static constexpr TypeId g_envProbeTypeToTypeId[EPT_MAX] = {
    TypeId::ForType<SkyProbe>(),        // EPT_SKY
    TypeId::ForType<ReflectionProbe>(), // EPT_REFLECTION
    TypeId::ForType<EnvProbe>(),        // EPT_SHADOW (fixme when derived class)
    TypeId::ForType<EnvProbe>()         // EPT_AMBIENT (fixme when derived class)
};

#pragma region Deferred pass

DeferredPass::DeferredPass(DeferredPassMode mode, Vec2u extent, GBuffer* gbuffer)
    : FullScreenPass(TF_RGBA16F, extent, gbuffer),
      m_mode(mode)
{
    SetBlendFunction(BlendFunction::Additive());
}

DeferredPass::~DeferredPass()
{
    SafeRelease(std::move(m_ltcSampler));
}

void DeferredPass::Create()
{
    FullScreenPass::Create();
}

void DeferredPass::CreatePipeline(const RenderableAttributeSet& renderableAttributes)
{
    if (m_mode == DeferredPassMode::INDIRECT_LIGHTING)
    {
        ShaderProperties shaderProperties;
        GetDeferredShaderProperties(shaderProperties);

        m_shader = g_shaderManager->GetOrCreate(NAME("DeferredIndirect"), shaderProperties);
        Assert(m_shader.IsValid());

        FullScreenPass::CreatePipeline(renderableAttributes);
        return;
    }

    // linear transform cosines texture data
    if (!m_ltcSampler)
    {
        m_ltcSampler = g_renderBackend->MakeSampler(
            TFM_NEAREST,
            TFM_LINEAR,
            TWM_CLAMP_TO_EDGE);

        DeferCreate(m_ltcSampler);

        ByteBuffer ltcMatrixData(sizeof(g_ltcMatrix), g_ltcMatrix);

        m_ltcMatrixTexture = CreateObject<Texture>(TextureData {
            TextureDesc {
                TT_TEX2D,
                TF_RGBA16F,
                Vec3u { 64, 64, 1 },
                TFM_LINEAR,
                TFM_LINEAR,
                TWM_CLAMP_TO_EDGE },
            std::move(ltcMatrixData) });
        m_ltcMatrixTexture->SetName(NAME("LtcMatrixLut"));
        InitObject(m_ltcMatrixTexture);

        ByteBuffer ltcBrdfData(sizeof(g_ltcBrdf), g_ltcBrdf);

        m_ltcBrdfTexture = CreateObject<Texture>(TextureData {
            TextureDesc {
                TT_TEX2D,
                TF_RGBA16F,
                Vec3u { 64, 64, 1 },
                TFM_LINEAR,
                TFM_LINEAR,
                TWM_CLAMP_TO_EDGE },
            std::move(ltcBrdfData) });

        m_ltcMatrixTexture->SetName(NAME("LtcBrdfLut"));
        InitObject(m_ltcBrdfTexture);
    }

    for (uint32 i = 0; i < LT_MAX; i++)
    {
        ShaderProperties shaderProperties;
        GetDeferredShaderProperties(shaderProperties);

        shaderProperties.Merge(g_deferredLightTypeProperties[i]);

        ShaderRef shader = g_shaderManager->GetOrCreate(NAME("DeferredDirect"), shaderProperties);
        Assert(shader != nullptr);

        const DescriptorTableDeclaration& descriptorTableDecl = shader->GetCompiledShader()->GetDescriptorTableDeclaration();

        DescriptorTableRef descriptorTable = g_renderBackend->MakeDescriptorTable(&descriptorTableDecl);

        for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
        {
            const DescriptorSetRef& descriptorSet = descriptorTable->GetDescriptorSet(NAME("DeferredDirectDescriptorSet"), frameIndex);
            Assert(descriptorSet.IsValid());

            descriptorSet->SetElement(NAME("MaterialsBuffer"), g_renderGlobalState->gpuBuffers[GRB_MATERIALS]->GetBuffer(frameIndex));

            descriptorSet->SetElement(NAME("LTCSampler"), m_ltcSampler);
            descriptorSet->SetElement(NAME("LTCMatrixTexture"), g_renderBackend->GetTextureImageView(m_ltcMatrixTexture));
            descriptorSet->SetElement(NAME("LTCBRDFTexture"), g_renderBackend->GetTextureImageView(m_ltcBrdfTexture));
        }

        DeferCreate(descriptorTable);

        GraphicsPipelineRef graphicsPipeline = g_renderGlobalState->graphicsPipelineCache->GetOrCreate(
            shader,
            descriptorTable,
            { &m_framebuffer, 1 },
            renderableAttributes);

        m_directLightGraphicsPipelines[i] = graphicsPipeline;

        if (i == 0)
        {
            m_graphicsPipeline = graphicsPipeline;
        }
    }
}

void DeferredPass::Resize_Internal(Vec2u newSize)
{
    SafeRelease(std::move(m_directLightGraphicsPipelines));

    // NOTE: Don't bother discarding sampler, we don't recreate it if it already exists.

    FullScreenPass::Resize_Internal(newSize);
}

void DeferredPass::Render(FrameBase* frame, const RenderSetup& rs)
{
    HYP_SCOPE;

    AssertDebug(rs.IsValid());
    AssertDebug(rs.HasView());
    AssertDebug(rs.passData != nullptr);

    if (m_mode == DeferredPassMode::INDIRECT_LIGHTING)
    {
        RenderToFramebuffer(frame, rs, nullptr);

        return;
    }

    RenderProxyList& rpl = RenderApi_GetConsumerProxyList(rs.view);
    rpl.BeginRead();
    HYP_DEFER({ rpl.EndRead(); });

    // no lights bound, do not render direct shading at all
    if (rpl.GetLights().NumCurrent() == 0)
    {
        return;
    }

    static const bool useBindlessTextures = g_renderBackend->GetRenderConfig().IsBindlessSupported();

    // render with each light
    for (uint32 lightTypeIndex = 0; lightTypeIndex < LT_MAX; lightTypeIndex++)
    {
        const LightType lightType = LightType(lightTypeIndex);

        bool isFirstLight = true;

        for (Light* light : rpl.GetLights())
        {
            if (light->GetLightType() != lightTypeIndex)
            {
                continue;
            }

            const GraphicsPipelineRef& pipeline = m_directLightGraphicsPipelines[lightTypeIndex];

            const uint32 globalDescriptorSetIndex = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Global"));
            const uint32 viewDescriptorSetIndex = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(NAME("View"));
            const uint32 materialDescriptorSetIndex = lightType == LT_AREA_RECT
                ? pipeline->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Material"))
                : ~0u;
            
            const uint32 deferredDirectDescriptorSetIndex = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(NAME("DeferredDirectDescriptorSet"));

            if (isFirstLight)
            {
                pipeline->SetPushConstants(m_pushConstantData.Data(), m_pushConstantData.Size());

                frame->renderQueue << BindGraphicsPipeline(pipeline);

                // Bind textures globally (bindless)
                if (materialDescriptorSetIndex != ~0u && useBindlessTextures)
                {
                    frame->renderQueue << BindDescriptorSet(
                        pipeline->GetDescriptorTable()->GetDescriptorSet(NAME("Material"), frame->GetFrameIndex()),
                        pipeline,
                        ArrayMap<Name, uint32> {},
                        materialDescriptorSetIndex);
                }

                if (deferredDirectDescriptorSetIndex != ~0u)
                {
                    frame->renderQueue << BindDescriptorSet(
                        pipeline->GetDescriptorTable()->GetDescriptorSet(NAME("DeferredDirectDescriptorSet"), frame->GetFrameIndex()),
                        pipeline,
                        ArrayMap<Name, uint32> {},
                        deferredDirectDescriptorSetIndex);
                }

                frame->renderQueue << BindVertexBuffer(m_fullScreenQuad->GetVertexBuffer());
                frame->renderQueue << BindIndexBuffer(m_fullScreenQuad->GetIndexBuffer());

                isFirstLight = false;
            }

            frame->renderQueue << BindDescriptorSet(
                pipeline->GetDescriptorTable()->GetDescriptorSet(NAME("Global"), frame->GetFrameIndex()),
                pipeline,
                ArrayMap<Name, uint32> {
                    { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(rs.view->GetCamera()) },
                    { NAME("CurrentLight"), ShaderDataOffset<LightShaderData>(light, 0) } },
                globalDescriptorSetIndex);

            frame->renderQueue << BindDescriptorSet(
                rs.passData->descriptorSets[frame->GetFrameIndex()],
                pipeline,
                ArrayMap<Name, uint32> {},
                viewDescriptorSetIndex);

            // Bind material descriptor set (for area lights)
            if (materialDescriptorSetIndex != ~0u && !useBindlessTextures)
            {
                const DescriptorSetRef& materialDescriptorSet = g_renderGlobalState->materialDescriptorSetManager->ForBoundMaterial(light->GetMaterial(), frame->GetFrameIndex());

                frame->renderQueue << BindDescriptorSet(
                    materialDescriptorSet,
                    pipeline,
                    ArrayMap<Name, uint32> {},
                    materialDescriptorSetIndex);
            }

            frame->renderQueue << DrawIndexed(m_fullScreenQuad->NumIndices());
        }
    }
}

#pragma endregion Deferred pass

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

    RenderableAttributeSet renderableAttributes(
        MeshAttributes {
            .vertexAttributes = staticMeshVertexAttributes },
        MaterialAttributes {
            .fillMode = FM_FILL,
            .blendFunction = BlendFunction::None(),
            .flags = MAF_NONE });

    m_shader = g_shaderManager->GetOrCreate(NAME("Tonemap"));

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

#pragma region TonemapPass

LightmapPass::LightmapPass(const FramebufferRef& framebuffer, Vec2u extent, GBuffer* gbuffer)
    : FullScreenPass(
          ShaderRef::Null(),
          DescriptorTableRef::Null(),
          framebuffer,
          TF_RGBA8,
          extent,
          gbuffer)
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

    RenderableAttributeSet renderableAttributes(
        MeshAttributes {
            .vertexAttributes = staticMeshVertexAttributes },
        MaterialAttributes {
            .fillMode = FM_FILL,
            .blendFunction = BlendFunction(
                BMF_SRC_ALPHA, BMF_ONE_MINUS_SRC_ALPHA,
                BMF_ONE, BMF_ONE_MINUS_SRC_ALPHA),
            .flags = MAF_NONE });

    m_shader = g_shaderManager->GetOrCreate(NAME("ApplyLightmap"));

    FullScreenPass::CreatePipeline(renderableAttributes);
}

void LightmapPass::Resize_Internal(Vec2u newSize)
{
    FullScreenPass::Resize_Internal(newSize);
}

void LightmapPass::Render(FrameBase* frame, const RenderSetup& rs)
{
    FullScreenPass::Render(frame, rs);
}

#pragma endregion LightmapPass

#pragma region Env grid pass

static EnvGridApplyMode EnvGridTypeToApplyEnvGridMode(EnvGridType type)
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
        HYP_FAIL("Invalid EnvGridType");
    }
}

EnvGridPass::EnvGridPass(EnvGridPassMode mode, Vec2u extent, GBuffer* gbuffer)
    : FullScreenPass(g_envGridPassFormats[mode], extent, gbuffer),
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

    RenderableAttributeSet renderableAttributes(
        MeshAttributes {
            .vertexAttributes = staticMeshVertexAttributes },
        MaterialAttributes {
            .fillMode = FM_FILL,
            .blendFunction = BlendFunction(BMF_SRC_ALPHA, BMF_ONE_MINUS_SRC_ALPHA,
                BMF_ONE, BMF_ONE_MINUS_SRC_ALPHA),
            .flags = MAF_NONE });

    if (m_mode == EGPM_RADIANCE)
    {
        m_shader = g_shaderManager->GetOrCreate(NAME("ApplyEnvGrid"), ShaderProperties { { NAME("MODE_RADIANCE") } });

        FullScreenPass::CreatePipeline(renderableAttributes);

        return;
    }

    static const FixedArray<Pair<EnvGridApplyMode, ShaderProperties>, EGAM_MAX> applyEnvGridPasses = {
        Pair<EnvGridApplyMode, ShaderProperties> { EGAM_SH, ShaderProperties { { NAME("MODE_IRRADIANCE"), NAME("IRRADIANCE_MODE_SH") } } },
        Pair<EnvGridApplyMode, ShaderProperties> { EGAM_VOXEL, ShaderProperties { { NAME("MODE_IRRADIANCE"), NAME("IRRADIANCE_MODE_VOXEL") } } },
        Pair<EnvGridApplyMode, ShaderProperties> { EGAM_LIGHT_FIELD, ShaderProperties { { NAME("MODE_IRRADIANCE"), NAME("IRRADIANCE_MODE_LIGHT_FIELD") } } }
    };

    for (const auto& it : applyEnvGridPasses)
    {
        ShaderRef shader = g_shaderManager->GetOrCreate(NAME("ApplyEnvGrid"), it.second);
        Assert(shader.IsValid());

        const DescriptorTableDeclaration& descriptorTableDecl = shader->GetCompiledShader()->GetDescriptorTableDeclaration();

        DescriptorTableRef descriptorTable = g_renderBackend->MakeDescriptorTable(&descriptorTableDecl);
        DeferCreate(descriptorTable);

        GraphicsPipelineRef graphicsPipeline = g_renderGlobalState->graphicsPipelineCache->GetOrCreate(
            shader,
            descriptorTable,
            { &m_framebuffer, 1 },
            renderableAttributes);

        m_graphicsPipelines[uint32(it.first)] = std::move(graphicsPipeline);
    }

    m_graphicsPipeline = m_graphicsPipelines[EGAM_SH];
}

void EnvGridPass::Resize_Internal(Vec2u newSize)
{
    SafeRelease(std::move(m_graphicsPipelines));

    FullScreenPass::Resize_Internal(newSize);
}

void EnvGridPass::Render(FrameBase* frame, const RenderSetup& rs)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    AssertDebug(rs.IsValid());
    AssertDebug(rs.HasView());
    AssertDebug(rs.passData != nullptr);

    RenderProxyList& rpl = RenderApi_GetConsumerProxyList(rs.view);
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

    for (EnvGrid* envGrid : rpl.GetEnvGrids())
    {
        const GraphicsPipelineRef& graphicsPipeline = m_mode == EGPM_RADIANCE
            ? m_graphicsPipeline
            : m_graphicsPipelines[EnvGridTypeToApplyEnvGridMode(envGrid->GetEnvGridType())];

        Assert(graphicsPipeline.IsValid());

        const uint32 globalDescriptorSetIndex = graphicsPipeline->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Global"));
        const uint32 viewDescriptorSetIndex = graphicsPipeline->GetDescriptorTable()->GetDescriptorSetIndex(NAME("View"));

        graphicsPipeline->SetPushConstants(m_pushConstantData.Data(), m_pushConstantData.Size());

        if (ShouldRenderHalfRes())
        {
            const Vec2i viewportOffset = (Vec2i(m_framebuffer->GetExtent().x, 0) / 2) * (RenderApi_GetWorldBufferData()->frameCounter & 1);
            const Vec2u viewportExtent = Vec2u(m_framebuffer->GetExtent().x / 2, m_framebuffer->GetExtent().y);

            frame->renderQueue << BindGraphicsPipeline(graphicsPipeline, viewportOffset, viewportExtent);
        }
        else
        {
            frame->renderQueue << BindGraphicsPipeline(graphicsPipeline);
        }

        frame->renderQueue << BindDescriptorSet(
            graphicsPipeline->GetDescriptorTable()->GetDescriptorSet(NAME("Global"), frameIndex),
            graphicsPipeline,
            ArrayMap<Name, uint32> {
                { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(rs.view->GetCamera()) },
                { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(envGrid, 0) } },
            globalDescriptorSetIndex);

        frame->renderQueue << BindDescriptorSet(
            rs.passData->descriptorSets[frameIndex],
            graphicsPipeline,
            ArrayMap<Name, uint32> {},
            viewDescriptorSetIndex);

        frame->renderQueue << BindVertexBuffer(m_fullScreenQuad->GetVertexBuffer());
        frame->renderQueue << BindIndexBuffer(m_fullScreenQuad->GetIndexBuffer());
        frame->renderQueue << DrawIndexed(m_fullScreenQuad->NumIndices());
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

#pragma endregion Env grid pass

#pragma region ReflectionsPass

ReflectionsPass::ReflectionsPass(Vec2u extent, GBuffer* gbuffer, const GpuImageViewRef& mipChainImageView, const GpuImageViewRef& deferredResultImageView)
    : FullScreenPass(TF_R10G10B10A2, extent, gbuffer),
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

    CreatePipeline(RenderableAttributeSet(
        MeshAttributes {
            .vertexAttributes = staticMeshVertexAttributes },
        MaterialAttributes {
            .fillMode = FM_FILL,
            .blendFunction = BlendFunction(BMF_SRC_ALPHA, BMF_ONE_MINUS_SRC_ALPHA,
                BMF_ONE, BMF_ONE_MINUS_SRC_ALPHA),
            .flags = MAF_NONE }));
}

void ReflectionsPass::CreatePipeline(const RenderableAttributeSet& renderableAttributes)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    static const FixedArray<Pair<CubemapType, ShaderProperties>, CMT_MAX> cubemapPasses = {
        Pair<CubemapType, ShaderProperties> { CMT_DEFAULT, ShaderProperties {} },
        Pair<CubemapType, ShaderProperties> { CMT_PARALLAX_CORRECTED, ShaderProperties { { NAME("ENV_PROBE_PARALLAX_CORRECTED") } } }
    };

    for (const auto& it : cubemapPasses)
    {
        ShaderRef shader = g_shaderManager->GetOrCreate(NAME("ApplyReflectionProbe"), it.second);
        Assert(shader.IsValid());

        const DescriptorTableDeclaration& descriptorTableDecl = shader->GetCompiledShader()->GetDescriptorTableDeclaration();

        DescriptorTableRef descriptorTable = g_renderBackend->MakeDescriptorTable(&descriptorTableDecl);
        DeferCreate(descriptorTable);

        GraphicsPipelineRef graphicsPipeline = g_renderGlobalState->graphicsPipelineCache->GetOrCreate(
            shader,
            descriptorTable,
            { &m_framebuffer, 1 },
            renderableAttributes);

        m_cubemapGraphicsPipelines[it.first] = std::move(graphicsPipeline);
    }

    m_graphicsPipeline = m_cubemapGraphicsPipelines[CMT_DEFAULT];
}

bool ReflectionsPass::ShouldRenderSSR() const
{
    static const ConfigurationValue& ssrEnabled = GetGlobalConfig().Get("rendering.ssr.enabled");
    static const ConfigurationValue& raytracingReflectionsEnabled = GetGlobalConfig().Get("rendering.raytracing.reflections.enabled");

    return ssrEnabled.ToBool(true) && !raytracingReflectionsEnabled.ToBool(false);
}

void ReflectionsPass::CreateSSRRenderer()
{
    m_ssrRenderer = MakeUnique<SSRRenderer>(SSRRendererConfig::FromConfig(), m_gbuffer, m_mipChainImageView, m_deferredResultImageView);
    m_ssrRenderer->Create();

    ShaderRef renderTextureToScreenShader = g_shaderManager->GetOrCreate(NAME("RenderTextureToScreen"));
    Assert(renderTextureToScreenShader.IsValid());

    const DescriptorTableDeclaration& descriptorTableDecl = renderTextureToScreenShader->GetCompiledShader()->GetDescriptorTableDeclaration();
    DescriptorTableRef descriptorTable = g_renderBackend->MakeDescriptorTable(&descriptorTableDecl);

    for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
    {
        const DescriptorSetRef& descriptorSet = descriptorTable->GetDescriptorSet(NAME("RenderTextureToScreenDescriptorSet"), frameIndex);
        Assert(descriptorSet != nullptr);

        descriptorSet->SetElement(NAME("InTexture"), g_renderBackend->GetTextureImageView(m_ssrRenderer->GetFinalResultTexture()));
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

    SafeRelease(std::move(m_mipChainImageView));
    SafeRelease(std::move(m_deferredResultImageView));
    SafeRelease(std::move(m_cubemapGraphicsPipelines));

    FullScreenPass::Resize_Internal(newSize);

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

    const uint32 frameIndex = frame->GetFrameIndex();

    RenderProxyList& rpl = RenderApi_GetConsumerProxyList(rs.view);
    rpl.BeginRead();

    HYP_DEFER({ rpl.EndRead(); });

    if (ShouldRenderSSR())
    {
        m_ssrRenderer->Render(frame, rs);
    }

    // Sky renders first
    static const FixedArray<EnvProbeType, CMT_MAX> envProbeTypes {
        EPT_SKY,
        EPT_REFLECTION
    };

    static const FixedArray<CubemapType, CMT_MAX> cubemapTypes {
        CMT_DEFAULT,           // EPT_SKY
        CMT_PARALLAX_CORRECTED // EPT_REFLECTION
    };

    FixedArray<Pair<GraphicsPipelineRef*, Array<EnvProbe*>>, CMT_MAX> passPtrs;

    for (uint32 cubemapType = 0; cubemapType < CMT_MAX; cubemapType++)
    {
        passPtrs[cubemapType] = { &m_cubemapGraphicsPipelines[cubemapType], {} };

        const EnvProbeType envProbeType = envProbeTypes[cubemapType];

        for (EnvProbe* envProbe : rpl.GetEnvProbes().GetElements(g_envProbeTypeToTypeId[envProbeType]))
        {
            passPtrs[cubemapType].second.PushBack(envProbe);
        }
    }

    frame->renderQueue << BeginFramebuffer(GetFramebuffer());

    // render previous frame's result to screen
    if (!m_isFirstFrame && m_renderTextureToScreenPass != nullptr)
    {
        RenderPreviousTextureToScreen(frame, rs);
    }

    uint32 numRenderedEnvProbes = 0;

    for (uint32 envProbeTypeIndex = 0; envProbeTypeIndex < ArraySize(envProbeTypes); envProbeTypeIndex++)
    {
        const EnvProbeType envProbeType = envProbeTypes[envProbeTypeIndex];
        const CubemapType cubemapType = cubemapTypes[envProbeTypeIndex];

        const Pair<GraphicsPipelineRef*, Array<EnvProbe*>>& it = passPtrs[cubemapType];

        if (it.second.Empty())
        {
            continue;
        }

        const GraphicsPipelineRef& graphicsPipeline = *it.first;
        const Array<EnvProbe*>& envProbes = it.second;

        graphicsPipeline->SetPushConstants(m_pushConstantData.Data(), m_pushConstantData.Size());

        if (ShouldRenderHalfRes())
        {
            const Vec2i viewportOffset = (Vec2i(m_framebuffer->GetExtent().x, 0) / 2) * (RenderApi_GetWorldBufferData()->frameCounter & 1);
            const Vec2u viewportExtent = Vec2u(m_framebuffer->GetExtent().x / 2, m_framebuffer->GetExtent().y);

            frame->renderQueue << BindGraphicsPipeline(graphicsPipeline, viewportOffset, viewportExtent);
        }
        else
        {
            frame->renderQueue << BindGraphicsPipeline(graphicsPipeline);
        }

        const uint32 globalDescriptorSetIndex = graphicsPipeline->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Global"));
        const uint32 viewDescriptorSetIndex = graphicsPipeline->GetDescriptorTable()->GetDescriptorSetIndex(NAME("View"));

        for (EnvProbe* envProbe : envProbes)
        {
            if (numRenderedEnvProbes >= g_maxBoundReflectionProbes)
            {
                HYP_LOG(Rendering, Warning, "Attempting to render too many reflection probes.");

                break;
            }

            // RenderProxyEnvProbe* envProbeProxy = static_cast<RenderProxyEnvProbe*>(RenderApi_GetRenderProxy(envProbe->Id()));
            // Assert(envProbeProxy != nullptr);
            // AssertDebug(envProbeProxy->bufferData.textureIndex != ~0u, "EnvProbe texture index not set: not bound for proxy %p at frame idx %u!", (void*)envProbeProxy,
            //     RenderApi_GetFrameIndex_RenderThread());

            frame->renderQueue << BindDescriptorSet(
                graphicsPipeline->GetDescriptorTable()->GetDescriptorSet(NAME("Global"), frameIndex),
                graphicsPipeline,
                ArrayMap<Name, uint32> {
                    { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(rs.view->GetCamera()) },
                    { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(envProbe) } },
                globalDescriptorSetIndex);

            frame->renderQueue << BindDescriptorSet(
                rs.passData->descriptorSets[frameIndex],
                graphicsPipeline,
                ArrayMap<Name, uint32> {},
                viewDescriptorSetIndex);

            frame->renderQueue << BindVertexBuffer(m_fullScreenQuad->GetVertexBuffer());
            frame->renderQueue << BindIndexBuffer(m_fullScreenQuad->GetIndexBuffer());
            frame->renderQueue << DrawIndexed(m_fullScreenQuad->NumIndices());

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

#pragma region DeferredPassData

DeferredPassData::~DeferredPassData()
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

    SafeRelease(std::move(finalPassDescriptorSet));
}

#pragma endregion DeferredPassData

#pragma region RaytracingPassData

RaytracingPassData::~RaytracingPassData()
{
    SafeRelease(std::move(raytracingTlases));
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

#define CHECK_FRAMEBUFFER_SIZE(fb)                                                                    \
    Assert(fb->GetExtent() == passData.viewport.extent,                                               \
        "Deferred pass framebuffer extent does not match viewport extent! Expected {}x{}, got {}x{}", \
        passData.viewport.extent.x, passData.viewport.extent.y,                                       \
        fb->GetExtent().x, fb->GetExtent().y)

Handle<PassData> DeferredRenderer::CreateViewPassData(View* view, PassDataExt&)
{
    HYP_SCOPE;

    Assert(view != nullptr);

    if (view->GetFlags() & ViewFlags::GBUFFER)
    {
        Handle<DeferredPassData> pd = CreateObject<DeferredPassData>();
        DeferredPassData& passData = *pd;

        passData.view = view->WeakHandleFromThis();
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

        passData.indirectPass = CreateObject<DeferredPass>(DeferredPassMode::INDIRECT_LIGHTING, passData.viewport.extent, gbuffer);
        passData.indirectPass->Create();

        passData.directPass = CreateObject<DeferredPass>(DeferredPassMode::DIRECT_LIGHTING, passData.viewport.extent, gbuffer);
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
        passData.lightmapPass = CreateObject<LightmapPass>(translucentFbo, passData.viewport.extent, gbuffer);
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

        passData.view = view->WeakHandleFromThis();
        passData.viewport = view->GetViewport();

        return pd;
    }

    HYP_LOG(Rendering, Fatal,
        "Cannot create PassData for View {}! View does not have any flags set that would allow us to create PassData for it. View flags: {}",
        view->Id(), uint32(view->GetFlags()));

    return Handle<PassData>::empty;
}

void DeferredRenderer::CreateViewFinalPassDescriptorSet(View* view, DeferredPassData& passData)
{
    HYP_SCOPE;

    ShaderRef renderTextureToScreenShader = g_shaderManager->GetOrCreate(NAME("RenderTextureToScreen_UI"));
    Assert(renderTextureToScreenShader.IsValid());

    const GpuImageViewRef& inputImageView = m_rendererConfig.taaEnabled
        ? g_renderBackend->GetTextureImageView(passData.temporalAa->GetResultTexture())
        : passData.combinePass->GetFinalImageView();

    Assert(inputImageView.IsValid());

    const DescriptorTableDeclaration& descriptorTableDecl = renderTextureToScreenShader->GetCompiledShader()->GetDescriptorTableDeclaration();

    DescriptorSetDeclaration* decl = descriptorTableDecl.FindDescriptorSetDeclaration(NAME("RenderTextureToScreenDescriptorSet"));
    Assert(decl != nullptr);

    const DescriptorSetLayout layout { decl };

    DescriptorSetRef descriptorSet = g_renderBackend->MakeDescriptorSet(layout);
    descriptorSet->SetDebugName(NAME("FinalPassDescriptorSet"));
    descriptorSet->SetElement(NAME("InTexture"), inputImageView);

    DeferCreate(descriptorSet);

    SafeRelease(std::move(passData.finalPassDescriptorSet));

    passData.finalPassDescriptorSet = std::move(descriptorSet);
}

void DeferredRenderer::CreateViewDescriptorSets(View* view, DeferredPassData& passData)
{
    HYP_SCOPE;

    const DescriptorSetDeclaration* decl = g_renderGlobalState->globalDescriptorTable->GetDeclaration()->FindDescriptorSetDeclaration(NAME("View"));
    Assert(decl != nullptr);

    const DescriptorSetLayout layout { decl };

    FixedArray<DescriptorSetRef, g_framesInFlight> descriptorSets;

    static const FixedArray<Name, GTN_MAX> gbufferTextureNames {
        NAME("GBufferAlbedoTexture"),
        NAME("GBufferNormalsTexture"),
        NAME("GBufferMaterialTexture"),
        NAME("GBufferLightmapTexture"),
        NAME("GBufferVelocityTexture"),
        NAME("GBufferWSNormalsTexture"),
        NAME("GBufferTranslucentTexture")
    };

    const FramebufferRef& opaqueFbo = view->GetOutputTarget().GetFramebuffer(RB_OPAQUE);
    const FramebufferRef& lightmapFbo = view->GetOutputTarget().GetFramebuffer(RB_LIGHTMAP);
    const FramebufferRef& translucentFbo = view->GetOutputTarget().GetFramebuffer(RB_TRANSLUCENT);

    // depth attachment goes into separate slot
    AttachmentBase* depthAttachment = opaqueFbo->GetAttachment(GTN_MAX - 1);
    Assert(depthAttachment != nullptr);

    for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
    {
        DescriptorSetRef descriptorSet = g_renderBackend->MakeDescriptorSet(layout);
        descriptorSet->SetDebugName(NAME_FMT("SceneViewDescriptorSet_{}", frameIndex));

        if (g_renderBackend->GetRenderConfig().IsDynamicDescriptorIndexingSupported())
        {
            uint32 gbufferElementIndex = 0;

            // not including depth texture here (hence the - 1)
            for (uint32 attachmentIndex = 0; attachmentIndex < GTN_MAX - 1; attachmentIndex++)
            {
                descriptorSet->SetElement(NAME("GBufferTextures"), gbufferElementIndex++, opaqueFbo->GetAttachment(attachmentIndex)->GetImageView());
            }

            // add translucent bucket's albedo
            descriptorSet->SetElement(NAME("GBufferTextures"), gbufferElementIndex++, translucentFbo->GetAttachment(0)->GetImageView());
        }
        else
        {
            for (uint32 attachmentIndex = 0; attachmentIndex < GTN_MAX - 1; attachmentIndex++)
            {
                descriptorSet->SetElement(gbufferTextureNames[attachmentIndex], opaqueFbo->GetAttachment(attachmentIndex)->GetImageView());
            }

            // add translucent bucket's albedo
            descriptorSet->SetElement(NAME("GBufferTranslucentTexture"), translucentFbo->GetAttachment(0)->GetImageView());
        }

        descriptorSet->SetElement(NAME("GBufferDepthTexture"), depthAttachment->GetImageView());

        descriptorSet->SetElement(NAME("GBufferMipChain"), g_renderBackend->GetTextureImageView(passData.mipChain));

        descriptorSet->SetElement(NAME("PostProcessingUniforms"), passData.postProcessing->GetUniformBuffer());

        descriptorSet->SetElement(NAME("DepthPyramidResult"), passData.depthPyramidRenderer->GetResultImageView());

        descriptorSet->SetElement(NAME("TAAResultTexture"), g_renderBackend->GetTextureImageView(passData.temporalAa->GetResultTexture()));

        if (passData.reflectionsPass->ShouldRenderSSR())
        {
            descriptorSet->SetElement(NAME("SSRResultTexture"), g_renderBackend->GetTextureImageView(passData.reflectionsPass->GetSSRRenderer()->GetFinalResultTexture()));
        }
        else
        {
            descriptorSet->SetElement(NAME("SSRResultTexture"), g_renderGlobalState->placeholderData->GetImageView2D1x1R8());
        }

        if (passData.ssgi)
        {
            descriptorSet->SetElement(NAME("SSGIResultTexture"), g_renderBackend->GetTextureImageView(passData.ssgi->GetFinalResultTexture()));
        }
        else
        {
            descriptorSet->SetElement(NAME("SSGIResultTexture"), g_renderGlobalState->placeholderData->GetImageView2D1x1R8());
        }

        if (passData.hbao)
        {
            descriptorSet->SetElement(NAME("SSAOResultTexture"), passData.hbao->GetFinalImageView());
        }
        else
        {
            descriptorSet->SetElement(NAME("SSAOResultTexture"), g_renderGlobalState->placeholderData->GetImageView2D1x1R8());
        }

        descriptorSet->SetElement(NAME("DeferredResult"), passData.combinePass->GetFinalImageView());

        descriptorSet->SetElement(NAME("DeferredIndirectResultTexture"), passData.indirectPass->GetFinalImageView());

        descriptorSet->SetElement(NAME("ReflectionProbeResultTexture"), passData.reflectionsPass->GetFinalImageView());

        descriptorSet->SetElement(NAME("EnvGridRadianceResultTexture"), passData.envGridRadiancePass->GetFinalImageView());
        descriptorSet->SetElement(NAME("EnvGridIrradianceResultTexture"), passData.envGridIrradiancePass->GetFinalImageView());

        HYP_GFX_ASSERT(descriptorSet->Create());

        descriptorSets[frameIndex] = std::move(descriptorSet);
    }

    passData.descriptorSets = std::move(descriptorSets);
}

void DeferredRenderer::CreateViewCombinePass(View* view, DeferredPassData& passData)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    // The combine pass will render into the translucent bucket's framebuffer with the shaded result.
    const FramebufferRef& translucentFbo = view->GetOutputTarget().GetFramebuffer(RB_TRANSLUCENT);
    Assert(translucentFbo != nullptr);

    ShaderRef renderTextureToScreenShader = g_shaderManager->GetOrCreate(NAME("RenderTextureToScreen_UI"));
    Assert(renderTextureToScreenShader.IsValid());

    const DescriptorTableDeclaration& descriptorTableDecl = renderTextureToScreenShader->GetCompiledShader()->GetDescriptorTableDeclaration();
    DescriptorTableRef descriptorTable = g_renderBackend->MakeDescriptorTable(&descriptorTableDecl);

    for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
    {
        const DescriptorSetRef& descriptorSet = descriptorTable->GetDescriptorSet(NAME("RenderTextureToScreenDescriptorSet"), frameIndex);
        Assert(descriptorSet != nullptr);

        descriptorSet->SetElement(NAME("InTexture"), passData.indirectPass->GetFinalImageView());
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

void DeferredRenderer::CreateViewRaytracingPasses(View* view, DeferredPassData& passData)
{
    // Is hardware ray tracing supported at all?
    // We could still create TLAS without necessarily creating reflection and global illumination pass data,
    // and then ray tracing features could be dynamically enabled or disabled.
    static const bool shouldEnableRaytracingStatic = g_renderBackend->GetRenderConfig().IsRaytracingSupported();

    if (!shouldEnableRaytracingStatic)
    {
        return;
    }

    const bool shouldEnableRaytracingForView = view->GetRaytracingView().IsValid()
        && GetGlobalConfig().Get("rendering.raytracing.enabled").ToBool();

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
    SafeRelease(std::move(passData.raytracingTlases));

    // Hack to fix driver crash when building TLAS with no meshes
    Handle<Mesh> defaultMesh = MeshBuilder::Cube();
    InitObject(defaultMesh);

    BLASRef blas = MeshBlasBuilder::Build(defaultMesh);
    HYP_GFX_ASSERT(blas->Create());

    for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
    {
        TLASRef& tlas = passData.raytracingTlases[frameIndex];

        tlas = g_renderBackend->MakeTLAS();
        tlas->AddBLAS(blas);

        HYP_GFX_ASSERT(tlas->Create());
    }
}

void DeferredRenderer::ResizeView(Viewport viewport, View* view, DeferredPassData& passData)
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

    SafeRelease(std::move(passData.descriptorSets));
    CreateViewDescriptorSets(view, passData);

    SafeRelease(std::move(passData.finalPassDescriptorSet));
    CreateViewFinalPassDescriptorSet(view, passData);

    CreateViewRaytracingPasses(view, passData);

    passData.view = view->WeakHandleFromThis();
    passData.viewport = viewport;
}

void DeferredRenderer::RenderFrame(FrameBase* frame, const RenderSetup& rs)
{
    HYP_SCOPE;

    Assert(rs.IsValid());

    RenderStatsCounts counts {};

    Array<RenderProxyList*> renderProxyLists;

    HYP_DEFER({ for (RenderProxyList* rpl : renderProxyLists) rpl->EndRead(); });

    // Collect view-independent renderable types from all views, binned
    /// @TODO: We could use the existing binning by subclass that ResourceTracker now provides.
    FixedArray<Array<EnvProbe*>, EPT_MAX> envProbes;
    FixedArray<Array<Light*>, LT_MAX> lights;
    Array<EnvGrid*> envGrids;

    // For rendering EnvGrids and EnvProbes, we use a directional light from one of the Views that references it (if found)
    /// TODO: This could be a little bit more robust.
    HashMap<EnvGrid*, Light*> envGridLights;
    HashMap<EnvProbe*, Light*> envProbeLights;

    // Render UI to render targets
    for (RendererBase* renderer : g_renderGlobalState->globalRenderers[GRT_UI])
    {
        renderer->RenderFrame(frame, rs);
    }

    // init view pass data and collect global rendering resources
    // (env probes, env grids)
    for (const Handle<View>& view : rs.world->GetViews())
    {
        AssertDebug(view.IsValid());

        RenderProxyList& rpl = RenderApi_GetConsumerProxyList(view);
        rpl.BeginRead();

        renderProxyLists.PushBack(&rpl);

        const Handle<PassData>& pd = FetchViewPassData(view);
        Assert(pd != nullptr);

        if (view->GetFlags() & ViewFlags::GBUFFER)
        {
            DeferredPassData* pdCasted = ObjCast<DeferredPassData>(pd.Get());
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

            RenderSetup newRs = rs;
            newRs.passData = pd;
            newRs.view = view;

            UpdateRaytracingView(frame, newRs);
        }

        for (Light* light : rpl.GetLights())
        {
            AssertDebug(light != nullptr);

            lights[light->GetLightType()].PushBack(light);
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

            envProbes[envProbe->GetEnvProbeType()].PushBack(envProbe);
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

            envGrids.PushBack(envGrid);
        }
    }

    // Render global environment probes and grids and set fallbacks
    RenderSetup newRs = rs;

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
                newRs.light = light;
                shadowRenderer->RenderFrame(frame, newRs);
                newRs.light = nullptr;
            }
        }
    }

    {
        // Set sky as fallback probe
        if (envProbes[EPT_SKY].Any())
        {
            newRs.envProbe = envProbes[EPT_SKY][0];
        }

        if (lights[LT_DIRECTIONAL].Any())
        {
            newRs.light = lights[LT_DIRECTIONAL][0];
        }

        if (envProbes.Any())
        {
            for (uint32 envProbeType = 0; envProbeType <= EPT_REFLECTION; envProbeType++)
            {
                if (RendererBase* renderer = g_renderGlobalState->globalRenderers[GRT_ENV_PROBE][envProbeType])
                {
                    for (EnvProbe* envProbe : envProbes[envProbeType])
                    {
                        newRs.envProbe = envProbe;

                        renderer->RenderFrame(frame, newRs);

                        newRs.envProbe = nullptr;

                        counts[ERS_ENV_PROBES]++;
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
                // Set global directional light as fallback
                if (envGridLights.Contains(envGrid))
                {
                    newRs.light = envGridLights[envGrid];
                }

                newRs.envGrid = envGrid;

                g_renderGlobalState->globalRenderers[GRT_ENV_GRID][0]->RenderFrame(frame, newRs);

                newRs.light = nullptr;
                newRs.envGrid = nullptr;

                counts[ERS_ENV_GRIDS]++;
            }
        }
    }

    // reset renderer state back to what it was before
    newRs = rs;

    for (const Handle<View>& view : rs.world->GetViews())
    {
        AssertDebug(view.IsValid());

        if (!(view->GetFlags() & ViewFlags::GBUFFER))
        {
            continue;
        }

        const Handle<DeferredPassData>& pd = ObjCast<DeferredPassData>(FetchViewPassData(view));
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

#ifdef HYP_ENABLE_RENDER_STATS
        RenderProxyList& rpl = RenderApi_GetConsumerProxyList(view);
        // RenderProxyList already be in read state (see above)

        counts[ERS_VIEWS]++;
        counts[ERS_TEXTURES] += rpl.GetTextures().NumCurrent();
        counts[ERS_MATERIALS] += rpl.GetMaterials().NumCurrent();
        counts[ERS_LIGHTMAP_VOLUMES] += rpl.GetLightmapVolumes().NumCurrent();
        counts[ERS_LIGHTS] += rpl.GetLights().NumCurrent();
        counts[ERS_ENV_GRIDS] += rpl.GetEnvGrids().NumCurrent();
        counts[ERS_ENV_PROBES] += rpl.GetEnvProbes().NumCurrent();
#endif
    }

    RenderApi_AddRenderStats(counts);
}

void DeferredRenderer::RenderFrameForView(FrameBase* frame, const RenderSetup& rs)
{
    HYP_SCOPE;

    Assert(rs.IsValid());
    Assert(rs.HasView());

    uint32 globalFrameIndex = RenderApi_GetFrameIndex();
    if (m_lastFrameData.frameId != globalFrameIndex)
    {
        m_lastFrameData.frameId = globalFrameIndex;
        m_lastFrameData.passData.Clear();
    }

    View* view = rs.view;

    Assert(view->GetFlags() & ViewFlags::GBUFFER);

    RenderProxyList& rpl = RenderApi_GetConsumerProxyList(view);
    rpl.BeginRead();
    HYP_DEFER({ rpl.EndRead(); });

    RenderCollector& renderCollector = RenderApi_GetRenderCollector(view);

    DeferredPassData* passDataCasted = ObjCast<DeferredPassData>(rs.passData);
    AssertDebug(passDataCasted != nullptr);

    DeferredPassData& passData = *passDataCasted;

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
        RenderProxyCamera* cameraProxy = static_cast<RenderProxyCamera*>(RenderApi_GetRenderProxy(view->GetCamera()->Id()));
        Assert(cameraProxy != nullptr);

        CameraShaderData& cameraBufferData = cameraProxy->bufferData;

        if (cameraBufferData.projection[3][3] < MathUtil::epsilonF)
        {
            const uint32 frameCounter = RenderApi_GetWorldBufferData()->frameCounter + 1;

            Vec4f jitter = Vec4f::Zero();
            Matrix4::Jitter(frameCounter, cameraBufferData.dimensions.x, cameraBufferData.dimensions.y, jitter);

            cameraBufferData.jitter = jitter * g_cameraJitterScale;

            RenderApi_UpdateGpuData(view->GetCamera()->Id());
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

            if (raytracingPassData->raytracingTlases[frameIndex] != nullptr)
            {
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
            frame->renderQueue << BindGraphicsPipeline(passData.combinePass->GetGraphicsPipeline());

            frame->renderQueue << BindDescriptorTable(
                passData.combinePass->GetGraphicsPipeline()->GetDescriptorTable(),
                passData.combinePass->GetGraphicsPipeline(),
                ArrayMap<Name, ArrayMap<Name, uint32>> {},
                frameIndex);

            frame->renderQueue << BindVertexBuffer(passData.combinePass->GetQuadMesh()->GetVertexBuffer());
            frame->renderQueue << BindIndexBuffer(passData.combinePass->GetQuadMesh()->GetIndexBuffer());
            frame->renderQueue << DrawIndexed(passData.combinePass->GetQuadMesh()->NumIndices());
        }

        // Render the objects to have lightmaps applied into the translucent pass framebuffer with a full screen quad.
        // Apply lightmaps over the now shaded opaque objects.
        passData.lightmapPass->RenderToFramebuffer(frame, rs, translucentFbo);

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
        Pair<View*, DeferredPassData*> { view, &passData },
        [view](const Pair<View*, DeferredPassData*>& a, const Pair<View*, DeferredPassData*>& b)
        {
            return a.second->priority < b.second->priority;
        });

    m_lastFrameData.passData.Insert(lastFrameDataIt, Pair<View*, DeferredPassData*> { view, &passData });
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

    const uint32 frameIndex = frame->GetFrameIndex();

    RaytracingPassData* pd = ObjCast<RaytracingPassData>(rs.passData);

    RenderProxyList& rpl = RenderApi_GetConsumerProxyList(rs.view);
    rpl.BeginRead();
    HYP_DEFER({ rpl.EndRead(); });

    if (!pd->raytracingTlases[frameIndex])
    {
        for (TLASRef& tlas : pd->raytracingTlases)
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

        AssertDebug(meshProxy->mesh != nullptr);

        BLASRef& blas = meshProxy->raytracingData.bottomLevelAccelerationStructures[frame->GetFrameIndex()];

        const bool materialsDiffer = blas != nullptr
            && blas->GetMaterial() != meshProxy->material;

        if (!blas || materialsDiffer)
        {
            if (blas != nullptr)
            {
                for (TLASRef& tlas : pd->raytracingTlases)
                {
                    tlas->RemoveBLAS(blas);
                }

                SafeRelease(std::move(blas));
            }

            blas = MeshBlasBuilder::Build(meshProxy->mesh, meshProxy->material);
            Assert(blas != nullptr);

            blas->SetTransform(meshProxy->bufferData.modelMatrix);

            if (!blas->IsCreated())
            {
                HYP_GFX_ASSERT(blas->Create());
            }
        }
        else
        {
            const uint32 materialBinding = RenderApi_RetrieveResourceBinding(meshProxy->material.Id());
            const bool materialBindingsDiffer = blas->GetMaterialBinding() != materialBinding;

            if (materialBindingsDiffer)
            {
                // needs to rebuild mesh descriptions if material binding changed
                blas->SetMaterialBinding(materialBinding);
            }

            blas->SetTransform(meshProxy->bufferData.modelMatrix);
        }

        if (!pd->raytracingTlases[frameIndex]->HasBLAS(blas))
        {
            for (TLASRef& tlas : pd->raytracingTlases)
            {
                tlas->AddBLAS(blas);
            }

            hasBlas = true;
        }
    }

    if (!pd->raytracingTlases[frameIndex]->IsCreated())
    {
        if (hasBlas)
        {
            for (TLASRef& tlas : pd->raytracingTlases)
            {
                HYP_GFX_ASSERT(tlas->Create());
            }
        }

        return;
    }

    RTUpdateStateFlags updateStateFlags = RTUpdateStateFlagBits::RT_UPDATE_STATE_FLAGS_NONE;
    pd->raytracingTlases[frameIndex]->UpdateStructure(updateStateFlags);
}

void DeferredRenderer::PerformOcclusionCulling(FrameBase* frame, const RenderSetup& rs, RenderCollector& renderCollector)
{
    HYP_SCOPE;

    constexpr uint32 bucketMask = (1 << RB_OPAQUE)
        | (1 << RB_LIGHTMAP)
        | (1 << RB_SKYBOX)
        | (1 << RB_TRANSLUCENT)
        | (1 << RB_DEBUG);

    renderCollector.PerformOcclusionCulling(frame, rs, bucketMask);
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

    DeferredPassData* pd = ObjCast<DeferredPassData>(rs.passData);

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
