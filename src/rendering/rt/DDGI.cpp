/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/rt/DDGI.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/RenderFrame.hpp>
#include <rendering/RenderGpuBuffer.hpp>
#include <rendering/RenderComputePipeline.hpp>
#include <rendering/RenderDescriptorSet.hpp>
#include <rendering/RenderGpuImage.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <scene/View.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/EnvGrid.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/utilities/ByteUtil.hpp>
#include <core/utilities/DeferredScope.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>
#include <core/Types.hpp>

namespace hyperion {

enum ProbeSystemUpdates : uint32
{
    PROBE_SYSTEM_UPDATES_NONE = 0x0,
    PROBE_SYSTEM_UPDATES_TLAS = 0x1
};

static constexpr TextureFormat ddgiIrradianceFormat = TF_RGBA16F;
static constexpr TextureFormat ddgiDepthFormat = TF_RG16F;

#pragma region Render commands

struct RENDER_COMMAND(SetDDGIDescriptors)
    : RenderCommand
{
    FixedArray<GpuBufferRef, g_framesInFlight> uniformBuffers;
    GpuImageViewRef irradianceImageView;
    GpuImageViewRef depthImageView;

    RENDER_COMMAND(SetDDGIDescriptors)(
        const FixedArray<GpuBufferRef, g_framesInFlight>& uniformBuffers,
        const GpuImageViewRef& irradianceImageView,
        const GpuImageViewRef& depthImageView)
        : uniformBuffers(uniformBuffers),
          irradianceImageView(irradianceImageView),
          depthImageView(depthImageView)
    {
    }

    virtual ~RENDER_COMMAND(SetDDGIDescriptors)() override = default;

    virtual RendererResult operator()() override
    {
        for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
        {
            g_renderGlobalState->globalDescriptorTable->GetDescriptorSet("Global", frameIndex)
                ->SetElement("DDGIUniforms", uniformBuffers[frameIndex]);

            g_renderGlobalState->globalDescriptorTable->GetDescriptorSet("Global", frameIndex)
                ->SetElement("DDGIIrradianceTexture", irradianceImageView);

            g_renderGlobalState->globalDescriptorTable->GetDescriptorSet("Global", frameIndex)
                ->SetElement("DDGIDepthTexture", depthImageView);
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(UnsetDDGIDescriptors)
    : RenderCommand
{
    RENDER_COMMAND(UnsetDDGIDescriptors)()
    {
    }

    virtual ~RENDER_COMMAND(UnsetDDGIDescriptors)() override = default;

    virtual RendererResult operator()() override
    {
        // remove result image from global descriptor set
        for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
        {
            g_renderGlobalState->globalDescriptorTable->GetDescriptorSet("Global", frameIndex)
                ->SetElement("DDGIUniforms", g_renderGlobalState->placeholderData->GetOrCreateBuffer(GpuBufferType::CBUFF, sizeof(DDGIUniforms), false));

            g_renderGlobalState->globalDescriptorTable->GetDescriptorSet("Global", frameIndex)
                ->SetElement("DDGIIrradianceTexture", g_renderGlobalState->placeholderData->GetImageView2D1x1R8());

            g_renderGlobalState->globalDescriptorTable->GetDescriptorSet("Global", frameIndex)
                ->SetElement("DDGIDepthTexture", g_renderGlobalState->placeholderData->GetImageView2D1x1R8());
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(CreateDDGIRadianceBuffer)
    : RenderCommand
{
    GpuBufferRef radianceBuffer;
    DDGIInfo gridInfo;

    RENDER_COMMAND(CreateDDGIRadianceBuffer)(const GpuBufferRef& radianceBuffer, const DDGIInfo& gridInfo)
        : radianceBuffer(radianceBuffer),
          gridInfo(gridInfo)
    {
    }

    virtual ~RENDER_COMMAND(CreateDDGIRadianceBuffer)() override = default;

    virtual RendererResult operator()() override
    {
        HYP_GFX_CHECK(radianceBuffer->Create());
        radianceBuffer->Memset(radianceBuffer->Size(), 0);

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

DDGI::DDGI(DDGIInfo&& gridInfo)
    : m_gridInfo(std::move(gridInfo)),
      m_updates { PROBE_SYSTEM_UPDATES_NONE, PROBE_SYSTEM_UPDATES_NONE },
      m_time(0)
{
}

DDGI::~DDGI()
{
    SafeDelete(std::move(m_uniformBuffers));
    SafeDelete(std::move(m_radianceBuffer));
    SafeDelete(std::move(m_irradianceImage));
    SafeDelete(std::move(m_irradianceImageView));
    SafeDelete(std::move(m_depthImage));
    SafeDelete(std::move(m_depthImageView));
    SafeDelete(std::move(m_pipeline));
    SafeDelete(std::move(m_updateIrradiance));
    SafeDelete(std::move(m_updateDepth));
    SafeDelete(std::move(m_copyBorderTexelsIrradiance));
    SafeDelete(std::move(m_copyBorderTexelsDepth));

    PUSH_RENDER_COMMAND(UnsetDDGIDescriptors);
}

void DDGI::Create()
{
    const Vec3u grid = m_gridInfo.NumProbesPerDimension();
    m_probes.Resize(m_gridInfo.NumProbes());

    for (uint32 x = 0; x < grid.x; x++)
    {
        for (uint32 y = 0; y < grid.y; y++)
        {
            for (uint32 z = 0; z < grid.z; z++)
            {
                const uint32 index = x * grid.x * grid.y + y * grid.z + z;

                m_probes[index] = Probe {
                    (Vec3f { float(x), float(y), float(z) } - (Vec3f(m_gridInfo.probeBorder) * 0.5f)) * m_gridInfo.probeDistance
                };
            }
        }
    }

    CreateStorageBuffers();
    CreateUniformBuffer();

    PUSH_RENDER_COMMAND(
        SetDDGIDescriptors,
        m_uniformBuffers,
        m_irradianceImageView,
        m_depthImageView);
}

void DDGI::CreateUniformBuffer()
{
    m_uniforms.flags = PROBE_SYSTEM_FLAGS_FIRST_RUN;

    for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
    {
        m_uniformBuffers[frameIndex] = g_renderBackend->MakeGpuBuffer(GpuBufferType::CBUFF, sizeof(DDGIUniforms));
        DeferCreate(m_uniformBuffers[frameIndex]);
    }
}

void DDGI::CreateStorageBuffers()
{
    const Vec3u probeCounts = m_gridInfo.NumProbesPerDimension();

    m_radianceBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::SSBO, m_gridInfo.GetImageDimensions().x * m_gridInfo.GetImageDimensions().y * sizeof(ProbeRayData));
    m_radianceBuffer->SetDebugName(NAME("DDGI_RadianceBuffer"));
    m_radianceBuffer->SetRequireCpuAccessible(true); // TEMP

    PUSH_RENDER_COMMAND(CreateDDGIRadianceBuffer, m_radianceBuffer, m_gridInfo);

    { // irradiance image
        const Vec3u extent {
            (m_gridInfo.irradianceOctahedronSize + 2) * probeCounts.x * probeCounts.y + 2,
            (m_gridInfo.irradianceOctahedronSize + 2) * probeCounts.z + 2,
            1
        };

        m_irradianceImage = g_renderBackend->MakeImage(TextureDesc {
            TT_TEX2D,
            ddgiIrradianceFormat,
            extent,
            TFM_NEAREST,
            TFM_NEAREST,
            TWM_CLAMP_TO_EDGE,
            1,
            IU_STORAGE | IU_SAMPLED });

        DeferCreate(m_irradianceImage);
    }

    { // irradiance image view
        m_irradianceImageView = g_renderBackend->MakeImageView(m_irradianceImage);

        DeferCreate(m_irradianceImageView);
    }

    { // depth image
        const Vec3u extent {
            (m_gridInfo.depthOctahedronSize + 2) * probeCounts.x * probeCounts.y + 2,
            (m_gridInfo.depthOctahedronSize + 2) * probeCounts.z + 2,
            1
        };

        m_depthImage = g_renderBackend->MakeImage(TextureDesc {
            TT_TEX2D,
            ddgiDepthFormat,
            extent,
            TFM_NEAREST,
            TFM_NEAREST,
            TWM_CLAMP_TO_EDGE,
            1,
            IU_STORAGE | IU_SAMPLED });

        DeferCreate(m_depthImage);
    }

    { // depth image view
        m_depthImageView = g_renderBackend->MakeImageView(m_depthImage);

        DeferCreate(m_depthImageView);
    }
}

void DDGI::UpdatePipelineState(FrameBase* frame, const RenderSetup& renderSetup)
{
    HYP_SCOPE;

    RaytracingPassData* pd = ObjCast<RaytracingPassData>(renderSetup.passData);
    Assert(pd != nullptr);

    const auto setDescriptorElements = [this, pd](DescriptorSetBase* descriptorSet, const TLASRef& tlas, uint32 frameIndex)
    {
        Assert(tlas != nullptr);

        descriptorSet->SetElement("TLAS", tlas);
        descriptorSet->SetElement("MeshDescriptionsBuffer", tlas->GetMeshDescriptionsBuffer());
        descriptorSet->SetElement("DDGIUniforms", m_uniformBuffers[frameIndex]);
        descriptorSet->SetElement("ProbeRayData", m_radianceBuffer);
        descriptorSet->SetElement("MaterialsBuffer", g_renderGlobalState->gpuBuffers[GRB_MATERIALS]->GetBuffer(frameIndex));
    };

    if (m_pipeline != nullptr)
    {
        DescriptorSetBase* descriptorSet = m_pipeline->GetDescriptorTable()->GetDescriptorSet("DDGIDescriptorSet", frame->GetFrameIndex());
        Assert(descriptorSet != nullptr);

        setDescriptorElements(descriptorSet, pd->raytracingTlases[frame->GetFrameIndex()], frame->GetFrameIndex());

        descriptorSet->UpdateDirtyState();
        descriptorSet->Update(true); //! temp

        return;
    }

    // Create raytracing pipeline
    ShaderRef raytracingShader = g_shaderManager->GetOrCreate(NAME("DDGI"));
    Assert(raytracingShader != nullptr);

    const DescriptorTableDeclaration& descriptorTableDecl = raytracingShader->GetCompiledShader()->GetDescriptorTableDeclaration();

    DescriptorTableRef descriptorTable = g_renderBackend->MakeDescriptorTable(&descriptorTableDecl);

    for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
    {
        DescriptorSetBase* descriptorSet = descriptorTable->GetDescriptorSet("DDGIDescriptorSet", frameIndex);
        Assert(descriptorSet != nullptr);

        setDescriptorElements(descriptorSet, pd->raytracingTlases[frameIndex], frameIndex);
    }

    HYP_GFX_ASSERT(descriptorTable->Create());

    m_pipeline = g_renderBackend->MakeRaytracingPipeline(raytracingShader, descriptorTable);
    HYP_GFX_ASSERT(m_pipeline->Create());

    for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
    {
        descriptorTable->Update(frameIndex, /* force */ true);
    }

    // Create compute pipelines
    ShaderRef updateIrradianceShader = g_shaderManager->GetOrCreate(NAME("RTProbeUpdateIrradiance"));
    ShaderRef updateDepthShader = g_shaderManager->GetOrCreate(NAME("RTProbeUpdateDepth"));
    ShaderRef copyBorderTexelsIrradianceShader = g_shaderManager->GetOrCreate(NAME("RTCopyBorderTexelsIrradiance"));
    ShaderRef copyBorderTexelsDepthShader = g_shaderManager->GetOrCreate(NAME("RTCopyBorderTexelsDepth"));

    Pair<ShaderRef, ComputePipelineRef&> computePipelines[] = {
        { updateIrradianceShader, m_updateIrradiance },
        { updateDepthShader, m_updateDepth },
        { copyBorderTexelsIrradianceShader, m_copyBorderTexelsIrradiance },
        { copyBorderTexelsDepthShader, m_copyBorderTexelsDepth }
    };

    for (auto& [shader, computePipeline] : computePipelines)
    {
        const DescriptorTableDeclaration& descriptorTableDecl = shader->GetCompiledShader()->GetDescriptorTableDeclaration();

        DescriptorTableRef descriptorTable = g_renderBackend->MakeDescriptorTable(&descriptorTableDecl);

        for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
        {
            const DescriptorSetRef& descriptorSet = descriptorTable->GetDescriptorSet("DDGIDescriptorSet", frameIndex);
            Assert(descriptorSet != nullptr);

            descriptorSet->SetElement("DDGIUniforms", m_uniformBuffers[frameIndex]);
            descriptorSet->SetElement("ProbeRayData", m_radianceBuffer);

            descriptorSet->SetElement("OutputIrradianceImage", m_irradianceImageView);
            descriptorSet->SetElement("OutputDepthImage", m_depthImageView);
        }

        DeferCreate(descriptorTable);

        computePipeline = g_renderBackend->MakeComputePipeline(shader, descriptorTable);
        DeferCreate(computePipeline);
    }
}

void DDGI::UpdateUniforms(FrameBase* frame, const RenderSetup& renderSetup)
{
    RenderProxyList& rpl = RenderApi_GetConsumerProxyList(renderSetup.view);
    rpl.BeginRead();
    HYP_DEFER({ rpl.EndRead(); });

    const Vec2u gridImageDimensions = m_gridInfo.GetImageDimensions();
    const Vec3u numProbesPerDimension = m_gridInfo.NumProbesPerDimension();

    m_uniforms.aabbMax = Vec4f(m_gridInfo.aabb.max, 1.0f);
    m_uniforms.aabbMin = Vec4f(m_gridInfo.aabb.min, 1.0f);
    m_uniforms.probeBorder = Vec4u(m_gridInfo.probeBorder, 0);
    m_uniforms.probeCounts = { numProbesPerDimension.x, numProbesPerDimension.y, numProbesPerDimension.z, 0 };
    m_uniforms.gridDimensions = { gridImageDimensions.x, gridImageDimensions.y, 0, 0 };
    m_uniforms.imageDimensions = { m_irradianceImage->GetExtent().x, m_irradianceImage->GetExtent().y, m_depthImage->GetExtent().x, m_depthImage->GetExtent().y };
    m_uniforms.probeDistance = m_gridInfo.probeDistance;
    m_uniforms.numRaysPerProbe = m_gridInfo.numRaysPerProbe;
    m_uniforms.numBoundLights = 0;

    const uint32 maxBoundLights = sizeof(m_uniforms.lightIndices) / sizeof(uint32);

    uint32* lightIndicesU32 = reinterpret_cast<uint32*>(&m_uniforms.lightIndices);
    Memory::MemSet(lightIndicesU32, 0, sizeof(m_uniforms.lightIndices));

    for (Light* light : rpl.GetLights())
    {
        const LightType lightType = light->GetLightType();

        if (lightType != LT_DIRECTIONAL && lightType != LT_POINT)
        {
            continue;
        }

        if (m_uniforms.numBoundLights >= maxBoundLights)
        {
            break;
        }

        lightIndicesU32[m_uniforms.numBoundLights++] = RenderApi_RetrieveResourceBinding(light);
    }

    m_uniformBuffers[frame->GetFrameIndex()]->Copy(sizeof(DDGIUniforms), &m_uniforms);

    m_uniforms.flags &= ~PROBE_SYSTEM_FLAGS_FIRST_RUN;
}

void DDGI::Render(FrameBase* frame, const RenderSetup& renderSetup)
{
    Threads::AssertOnThread(g_renderThread);

    Assert(renderSetup.IsValid());
    Assert(renderSetup.HasView());
    Assert(renderSetup.passData != nullptr);

    UpdatePipelineState(frame, renderSetup);
    UpdateUniforms(frame, renderSetup);

    m_randomGenerator.Next();

    struct
    {
        Matrix4 matrix;
        uint32 time;
    } pushConstants;

    pushConstants.matrix = m_randomGenerator.matrix;
    pushConstants.time = m_time++;

    m_pipeline->SetPushConstants(&pushConstants, sizeof(pushConstants));

    frame->renderQueue << InsertBarrier(m_radianceBuffer, RS_UNORDERED_ACCESS);

    frame->renderQueue << BindRaytracingPipeline(m_pipeline);

    frame->renderQueue << BindDescriptorTable(
        m_pipeline->GetDescriptorTable(),
        m_pipeline,
        { { "Global",
            { { "CamerasBuffer", ShaderDataOffset<CameraShaderData>(renderSetup.view->GetCamera()) },
                { "EnvGridsBuffer", ShaderDataOffset<EnvGridShaderData>(renderSetup.envGrid, 0) },
                { "CurrentEnvProbe", ShaderDataOffset<EnvProbeShaderData>(renderSetup.envProbe, 0) } } } },
        frame->GetFrameIndex());

    frame->renderQueue << TraceRays(m_pipeline, Vec3u { m_gridInfo.NumProbes(), m_gridInfo.numRaysPerProbe, 1u });

    frame->renderQueue << InsertBarrier(m_radianceBuffer, RS_UNORDERED_ACCESS);

    // Compute irradiance for ray traced probes

    const Vec3u probeCounts = m_gridInfo.NumProbesPerDimension();

    frame->renderQueue << InsertBarrier(m_irradianceImage, RS_UNORDERED_ACCESS);
    frame->renderQueue << InsertBarrier(m_depthImage, RS_UNORDERED_ACCESS);

    frame->renderQueue << BindComputePipeline(m_updateIrradiance);

    frame->renderQueue << BindDescriptorTable(
        m_updateIrradiance->GetDescriptorTable(),
        m_updateIrradiance,
        {},
        frame->GetFrameIndex());

    frame->renderQueue << DispatchCompute(m_updateIrradiance, Vec3u { probeCounts.x * probeCounts.y, probeCounts.z, 1u });

    frame->renderQueue << BindComputePipeline(m_updateDepth);

    frame->renderQueue << BindDescriptorTable(
        m_updateDepth->GetDescriptorTable(),
        m_updateDepth,
        { { "Global",
            { { "CamerasBuffer", ShaderDataOffset<CameraShaderData>(renderSetup.view->GetCamera()) },
                { "EnvGridsBuffer", ShaderDataOffset<EnvGridShaderData>(renderSetup.envGrid, 0) },
                { "CurrentEnvProbe", ShaderDataOffset<EnvProbeShaderData>(renderSetup.envProbe, 0) } } } },
        frame->GetFrameIndex());

    frame->renderQueue << DispatchCompute(m_updateDepth, Vec3u { probeCounts.x * probeCounts.y, probeCounts.z, 1u });

#if 0 // @FIXME: Properly implement an optimized way to copy border texels without invoking for each pixel in the images.
    frame->renderQueue << InsertBarrier(m_irradianceImage, RS_UNORDERED_ACCESS);
    frame->renderQueue << InsertBarrier(m_depthImage, RS_UNORDERED_ACCESS);

    // now copy border texels
    frame->renderQueue << BindComputePipeline(m_copyBorderTexelsIrradiance);
    
    frame->renderQueue << BindDescriptorTable(
        m_copyBorderTexelsIrradiance->GetDescriptorTable(),
        m_copyBorderTexelsIrradiance,
        {
            { "Global",
                { { "CamerasBuffer", ShaderDataOffset<CameraShaderData>(renderSetup.view->GetCamera()) },
                    { "EnvGridsBuffer", ShaderDataOffset<EnvGridShaderData>(renderSetup.envGrid, 0) },
                    { "CurrentEnvProbe", ShaderDataOffset<EnvProbeShaderData>(renderSetup.envProbe, 0) } } } },
        frame->GetFrameIndex());

    
    frame->renderQueue << DispatchCompute(
        m_copyBorderTexelsIrradiance, 
        Vec3u {
            (probeCounts.x * probeCounts.y * (m_gridInfo.irradianceOctahedronSize + m_gridInfo.probeBorder.x)) + 7 / 8,
            (probeCounts.z * (m_gridInfo.irradianceOctahedronSize + m_gridInfo.probeBorder.z)) + 7 / 8,
            1u
        });
    
    frame->renderQueue << BindComputePipeline(m_copyBorderTexelsIrradiance);
    
    frame->renderQueue << BindDescriptorTable(
        m_copyBorderTexelsIrradiance->GetDescriptorTable(),
        m_copyBorderTexelsIrradiance,
        {
            { "Global",
                { { "CamerasBuffer", ShaderDataOffset<CameraShaderData>(renderSetup.view->GetCamera()) },
                    { "EnvGridsBuffer", ShaderDataOffset<EnvGridShaderData>(renderSetup.envGrid, 0) },
                    { "CurrentEnvProbe", ShaderDataOffset<EnvProbeShaderData>(renderSetup.envProbe, 0) } } } },
        frame->GetFrameIndex());

    
    frame->renderQueue << DispatchCompute(
        m_copyBorderTexelsIrradiance, 
        Vec3u {
            (probeCounts.x * probeCounts.y * (m_gridInfo.depthOctahedronSize + m_gridInfo.probeBorder.x)) + 15 / 16,
            (probeCounts.z * (m_gridInfo.depthOctahedronSize + m_gridInfo.probeBorder.z)) + 15 / 16,
            1u
        });
    
    frame->renderQueue << InsertBarrier(m_irradianceImage, RS_SHADER_RESOURCE);
    frame->renderQueue << InsertBarrier(m_depthImage, RS_SHADER_RESOURCE);
#endif
}

HYP_DESCRIPTOR_CBUFF(Global, DDGIUniforms, 1, sizeof(DDGIUniforms), false);
HYP_DESCRIPTOR_SRV(Global, DDGIIrradianceTexture, 1);
HYP_DESCRIPTOR_SRV(Global, DDGIDepthTexture, 1);

} // namespace hyperion
