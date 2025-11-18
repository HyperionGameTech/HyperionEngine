/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <rendering/raytracing/RaytracingReflections.hpp>
#include <rendering/raytracing/DDGI.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/renderers/DeferredRenderer.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/RenderBackend.hpp>
#include <rendering/RenderFrame.hpp>
#include <rendering/RenderGpuBuffer.hpp>
#include <rendering/RenderResult.hpp>
#include <rendering/Texture.hpp>
#include <rendering/RenderCollection.hpp>
#include <rendering/RenderDescriptorSet.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <scene/View.hpp>
#include <scene/Light.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/EnvGrid.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <engine/EngineGlobals.hpp>

#include <RaytracingReflections.generated.inl>

namespace hyperion {

#pragma region Render commands

struct UnsetRTRadianceImageInGlobalDescriptorSet : RenderCommand
{
    virtual ~UnsetRTRadianceImageInGlobalDescriptorSet() override = default;

    virtual RendererResult operator()() override
    {
        RendererResult result;

        // remove result image from global descriptor set
        for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
        {
            g_renderGlobalState->globalDescriptorTable->GetDescriptorSet("Global", frameIndex)->SetElement("RTRadianceResultTexture", g_renderGlobalState->placeholderData->GetImageView2D1x1R8());
        }

        return result;
    }
};

#pragma endregion Render commands

RaytracingReflections::RaytracingReflections(RaytracingReflectionsConfig&& config, GBuffer* gbuffer)
    : m_config(std::move(config)),
      m_gbuffer(gbuffer)
{
}

RaytracingReflections::~RaytracingReflections()
{
    SafeDelete(std::move(m_raytracingPipeline));

    SafeDelete(std::move(m_uniformBuffers));

    // remove result image from global descriptor set
    SafeDelete(std::move(m_texture));

    PUSH_RENDER_COMMAND(UnsetRTRadianceImageInGlobalDescriptorSet);
}

void RaytracingReflections::Create()
{
    CreateImages();
    CreateUniformBuffer();
    CreateTemporalBlending();
}

void RaytracingReflections::UpdatePipelineState(FrameBase* frame, const RenderSetup& renderSetup)
{
    HYP_SCOPE;

    RaytracingPassData* pd = ObjCast<RaytracingPassData>(renderSetup.passData);
    Assert(pd != nullptr);

    const auto setDescriptorElements = [this, pd](DescriptorSetBase* descriptorSet, const GpuTlasRef& tlas, uint32 frameIndex)
    {
        Assert(tlas != nullptr);

        descriptorSet->SetElement("TLAS", tlas);
        descriptorSet->SetElement("MeshDescriptionsBuffer", tlas->GetMeshDescriptionsBuffer());
        descriptorSet->SetElement("OutputImage", g_renderBackend->GetTextureImageView(m_texture));
        descriptorSet->SetElement("RTRadianceUniforms", m_uniformBuffers[frameIndex]);
        descriptorSet->SetElement("MaterialsBuffer", g_renderGlobalState->gpuBuffers[GRB_MATERIALS]->GetBuffer(frameIndex));
    };

    if (m_raytracingPipeline != nullptr)
    {
        DescriptorSetBase* descriptorSet = m_raytracingPipeline->GetDescriptorTable()->GetDescriptorSet("RTRadianceDescriptorSet", frame->GetFrameIndex());
        Assert(descriptorSet != nullptr);

        setDescriptorElements(descriptorSet, pd->raytracingTlases[frame->GetFrameIndex()], frame->GetFrameIndex());

        descriptorSet->UpdateDirtyState();
        descriptorSet->Update(true); //! temp

        return;
    }

    static const Name shaderNames[] = { NAME("RTRadiance"), NAME("PathTracer") };

    ShaderRef shader = g_shaderManager->GetOrCreate(shaderNames[IsPathTracer()]);
    Assert(shader != nullptr);

    DescriptorTableRef descriptorTable = g_renderBackend->MakeDescriptorTable(
        shader->GetCompiledShader()->GetDescriptorTableDeclaration());

    for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
    {
        DescriptorSetBase* descriptorSet = descriptorTable->GetDescriptorSet("RTRadianceDescriptorSet", frameIndex);
        Assert(descriptorSet != nullptr);

        setDescriptorElements(descriptorSet, pd->raytracingTlases[frameIndex], frameIndex);
    }

    HYP_GFX_ASSERT(descriptorTable->Create());

    m_raytracingPipeline = g_renderBackend->MakeRaytracingPipeline(shader, descriptorTable);
    HYP_GFX_ASSERT(m_raytracingPipeline->Create());

    for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
    {
        descriptorTable->Update(frameIndex, /* force */ true);

        g_renderGlobalState->globalDescriptorTable->GetDescriptorSet("Global", frameIndex)
            ->SetElement("RTRadianceResultTexture", g_renderBackend->GetTextureImageView(m_temporalBlending->GetResultTexture()));
    }
}

void RaytracingReflections::UpdateUniforms(FrameBase* frame, const RenderSetup& renderSetup)
{
    RenderProxyList& rpl = RenderApi::GetConsumerProxyList(renderSetup.view);
    rpl.BeginRead();

    HYP_DEFER({ rpl.EndRead(); });

    RTRadianceUniforms uniforms {};
    uniforms.minRoughness = 0.4f;
    uniforms.outputImageResolution = Vec2i(m_config.extent);

    uint32 numBoundLights = 0;

    const uint32 maxBoundLights = ArraySize(uniforms.lightIndices);

    for (Light* light : rpl.GetLights())
    {
        const LightType lightType = light->GetLightType();

        if (lightType != LT_DIRECTIONAL && lightType != LT_POINT)
        {
            continue;
        }

        if (numBoundLights >= maxBoundLights)
        {
            break;
        }

        uniforms.lightIndices[numBoundLights++] = RenderApi::RetrieveResourceBinding(light);
    }

    uniforms.numBoundLights = numBoundLights;

    m_uniformBuffers[frame->GetFrameIndex()]->Copy(sizeof(uniforms), &uniforms);
}

void RaytracingReflections::Render(FrameBase* frame, const RenderSetup& renderSetup)
{
    HYP_NAMED_SCOPE("Ray traced reflections");
    
    AssertDebug(renderSetup.world && renderSetup.view);

    RaytracingPassData* pd = ObjCast<RaytracingPassData>(renderSetup.passData);
    AssertDebug(pd != nullptr);

    DeferredRendererPassData* parentPass = pd->parentPass;
    AssertDebug(parentPass != nullptr);

    UpdatePipelineState(frame, renderSetup);
    UpdateUniforms(frame, renderSetup);

    // Reset progressive blending if the camera view matrix has changed (for path tracing)
    if (IsPathTracer())
    {
        RenderProxyCamera* cameraProxy = static_cast<RenderProxyCamera*>(RenderApi::GetRenderProxy(renderSetup.view->GetCamera()));
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

    const uint32 viewDescriptorSetIndex = m_raytracingPipeline->GetDescriptorTable()->GetDescriptorSetIndex("View");
    AssertDebug(viewDescriptorSetIndex != ~0u);

    frame->renderQueue << BindRaytracingPipeline(m_raytracingPipeline);

    frame->renderQueue << BindDescriptorTable(
        m_raytracingPipeline->GetDescriptorTable(),
        m_raytracingPipeline,
        { { "Global",
            { { "CamerasBuffer", ShaderDataOffset<CameraShaderData>(renderSetup.view->GetCamera()) },
                { "EnvGridsBuffer", ShaderDataOffset<EnvGridShaderData>(renderSetup.envGrid, 0) },
                { "CurrentEnvProbe", ShaderDataOffset<EnvProbeShaderData>(renderSetup.envProbe, 0) } } } },
        frame->GetFrameIndex());

    frame->renderQueue << BindDescriptorSet(
        parentPass->descriptorSets[frame->GetFrameIndex()],
        m_raytracingPipeline,
        {},
        viewDescriptorSetIndex);

    frame->renderQueue << InsertBarrier(m_texture->GetGpuImage(), RS_UNORDERED_ACCESS);

    const Vec3u imageExtent = m_texture->GetGpuImage()->GetExtent();

    const SizeType numPixels = imageExtent.Volume();
    const SizeType halfNumPixels = numPixels / 2;

    frame->renderQueue << TraceRays(m_raytracingPipeline, Vec3u { uint32(numPixels), 1, 1 });
    frame->renderQueue << InsertBarrier(m_texture->GetGpuImage(), RS_SHADER_RESOURCE);

    // Create a new RenderSetup for temporal blending as it will need to bind View descriptors,
    // which we don't have on RaytracingPassData
    RenderSetup newRenderSetup = renderSetup;
    newRenderSetup.passData = parentPass;

    m_temporalBlending->Render(frame, newRenderSetup);
}

void RaytracingReflections::CreateImages()
{
    Assert(m_config.extent.Volume() != 0);

    m_texture = CreateObject<Texture>(TextureDesc {
        TT_TEX2D,
        TF_RGBA8,
        Vec3u { m_config.extent, 1 },
        TFM_NEAREST,
        TFM_NEAREST,
        TWM_CLAMP_TO_EDGE,
        1,
        IU_SAMPLED | IU_STORAGE });

    m_texture->SetName(NAME("RaytracingReflectionsTexture"));

    InitObject(m_texture);
}

void RaytracingReflections::CreateUniformBuffer()
{
    RTRadianceUniforms uniforms {};

    for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
    {
        m_uniformBuffers[frameIndex] = g_renderBackend->MakeGpuBuffer(GpuBufferType::CBUFF, sizeof(RTRadianceUniforms));
        m_uniformBuffers[frameIndex]->SetDebugName(NAME_FMT("RaytracingReflectionsUniformBuffer_{}", frameIndex));

        HYP_GFX_ASSERT(m_uniformBuffers[frameIndex]->Create());
        m_uniformBuffers[frameIndex]->Copy(sizeof(uniforms), &uniforms);
    }
}

void RaytracingReflections::CreateTemporalBlending()
{
    m_temporalBlending = MakeUnique<TemporalBlending>(
        m_config.extent,
        TF_RGBA8,
        IsPathTracer()
            ? TemporalBlendTechnique::TECHNIQUE_4 // progressive blending
            : TemporalBlendTechnique::TECHNIQUE_1,
        IsPathTracer()
            ? TemporalBlendFeedback::HIGH
            : TemporalBlendFeedback::HIGH,
        g_renderBackend->GetTextureImageView(m_texture),
        m_gbuffer);

    m_temporalBlending->Create();
}

} // namespace hyperion
