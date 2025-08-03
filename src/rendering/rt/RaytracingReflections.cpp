/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/rt/RaytracingReflections.hpp>
#include <rendering/rt/DDGI.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/SafeDeleter.hpp>
#include <rendering/RenderBackend.hpp>
#include <rendering/RenderFrame.hpp>
#include <rendering/RenderGpuBuffer.hpp>
#include <rendering/RenderResult.hpp>
#include <rendering/Texture.hpp>

#include <scene/View.hpp>
#include <scene/Light.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/EnvGrid.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <EngineGlobals.hpp>

namespace hyperion {

enum RTRadianceUpdates : uint32
{
    RT_RADIANCE_UPDATES_NONE = 0x0,
    RT_RADIANCE_UPDATES_TLAS = 0x1,
    RT_RADIANCE_UPDATES_SHADOW_MAP = 0x2
};

#pragma region Render commands

struct RENDER_COMMAND(UnsetRTRadianceImageInGlobalDescriptorSet)
    : RenderCommand
{
    virtual ~RENDER_COMMAND(UnsetRTRadianceImageInGlobalDescriptorSet)() override = default;

    virtual RendererResult operator()() override
    {
        RendererResult result;

        // remove result image from global descriptor set
        for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
        {
            g_renderGlobalState->globalDescriptorTable->GetDescriptorSet(NAME("Global"), frameIndex)->SetElement(NAME("RTRadianceResultTexture"), g_renderGlobalState->placeholderData->GetImageView2D1x1R8());
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
    SafeRelease(std::move(m_raytracingPipeline));

    SafeRelease(std::move(m_uniformBuffers));

    // remove result image from global descriptor set
    g_safeDeleter->SafeRelease(std::move(m_texture));

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

    const auto setDescriptorElements = [this, pd](DescriptorSetBase* descriptorSet, const TLASRef& tlas, uint32 frameIndex)
        {
            Assert(tlas != nullptr);

            descriptorSet->SetElement(NAME("TLAS"), tlas);
            descriptorSet->SetElement(NAME("MeshDescriptionsBuffer"), tlas->GetMeshDescriptionsBuffer());

            descriptorSet->SetElement(NAME("OutputImage"), g_renderBackend->GetTextureImageView(m_texture));
            descriptorSet->SetElement(NAME("RTRadianceUniforms"), m_uniformBuffers[frameIndex]);
        };

    if (m_raytracingPipeline != nullptr)
    {
        DescriptorSetBase* descriptorSet = m_raytracingPipeline->GetDescriptorTable()->GetDescriptorSet(NAME("RTRadianceDescriptorSet"), frame->GetFrameIndex());
        Assert(descriptorSet != nullptr);

        setDescriptorElements(descriptorSet, pd->raytracingTlases[frame->GetFrameIndex()], frame->GetFrameIndex());

        descriptorSet->UpdateDirtyState();
        descriptorSet->Update(true); //! temp

        return;
    }

    static const Name shaderNames[] = { NAME("RTRadiance"), NAME("PathTracer") };

    ShaderRef shader = g_shaderManager->GetOrCreate(shaderNames[IsPathTracer()]);
    Assert(shader != nullptr);

    const DescriptorTableDeclaration& descriptorTableDecl = shader->GetCompiledShader()->GetDescriptorTableDeclaration();

    DescriptorTableRef descriptorTable = g_renderBackend->MakeDescriptorTable(&descriptorTableDecl);

    for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
    {
        DescriptorSetBase* descriptorSet = descriptorTable->GetDescriptorSet(NAME("RTRadianceDescriptorSet"), frameIndex);
        Assert(descriptorSet != nullptr);

        setDescriptorElements(descriptorSet, pd->raytracingTlases[frameIndex], frameIndex);
    }

    HYP_GFX_ASSERT(descriptorTable->Create());

    m_raytracingPipeline = g_renderBackend->MakeRaytracingPipeline(shader, descriptorTable);
    HYP_GFX_ASSERT(m_raytracingPipeline->Create());
    
    for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
    {
        descriptorTable->Update(frameIndex, /* force */ true);

        g_renderGlobalState->globalDescriptorTable->GetDescriptorSet(NAME("Global"), frameIndex)
            ->SetElement(NAME("RTRadianceResultTexture"), g_renderBackend->GetTextureImageView(m_temporalBlending->GetResultTexture()));
    }
}

void RaytracingReflections::UpdateUniforms(FrameBase* frame, const RenderSetup& renderSetup)
{
    RenderProxyList& rpl = RenderApi_GetConsumerProxyList(renderSetup.view);
    rpl.BeginRead();

    HYP_DEFER({ rpl.EndRead(); });

    RTRadianceUniforms uniforms {};

    Memory::MemSet(&uniforms, 0, sizeof(uniforms));

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

        uniforms.lightIndices[numBoundLights++] = RenderApi_RetrieveResourceBinding(light);
    }

    uniforms.numBoundLights = numBoundLights;

    m_uniformBuffers[frame->GetFrameIndex()]->Copy(sizeof(uniforms), &uniforms);
}

void RaytracingReflections::Render(FrameBase* frame, const RenderSetup& renderSetup)
{
    AssertDebug(renderSetup.IsValid());
    AssertDebug(renderSetup.HasView());

    RaytracingPassData* pd = ObjCast<RaytracingPassData>(renderSetup.passData);
    AssertDebug(pd != nullptr);

    DeferredPassData* parentPass = pd->parentPass;
    AssertDebug(parentPass != nullptr);

    UpdatePipelineState(frame, renderSetup);
    UpdateUniforms(frame, renderSetup);

    const uint32 viewDescriptorSetIndex = m_raytracingPipeline->GetDescriptorTable()->GetDescriptorSetIndex(NAME("View"));
    AssertDebug(viewDescriptorSetIndex != ~0u);

    frame->renderQueue << BindRaytracingPipeline(m_raytracingPipeline);

    frame->renderQueue << BindDescriptorTable(
        m_raytracingPipeline->GetDescriptorTable(),
        m_raytracingPipeline,
        ArrayMap<Name, ArrayMap<Name, uint32>> {
            { NAME("Global"),
                { { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(renderSetup.view->GetCamera()) },
                    { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(renderSetup.envGrid, 0) },
                    { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(renderSetup.envProbe, 0) } } } },
        frame->GetFrameIndex());

    frame->renderQueue << BindDescriptorSet(
        parentPass->descriptorSets[frame->GetFrameIndex()],
        m_raytracingPipeline,
        ArrayMap<Name, uint32> {},
        viewDescriptorSetIndex);

    frame->renderQueue << InsertBarrier(m_texture->GetGpuImage(), RS_UNORDERED_ACCESS);

    const Vec3u imageExtent = m_texture->GetGpuImage()->GetExtent();

    const SizeType numPixels = imageExtent.Volume();
    // const SizeType halfNumPixels = numPixels / 2;

    frame->renderQueue << TraceRays(m_raytracingPipeline, Vec3u { uint32(numPixels), 1, 1 });
    frame->renderQueue << InsertBarrier(m_texture->GetGpuImage(), RS_SHADER_RESOURCE);

    // Reset progressive blending if the camera view matrix has changed (for path tracing)
    if (IsPathTracer())
    {
        RenderProxyCamera* cameraProxy = static_cast<RenderProxyCamera*>(RenderApi_GetRenderProxy(renderSetup.view->GetCamera()));
        Assert(cameraProxy != nullptr);

        if (cameraProxy->bufferData.view != m_previousViewMatrix)
        {
            m_temporalBlending->ResetProgressiveBlending();

            m_previousViewMatrix = cameraProxy->bufferData.view;
        }
    }

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
        TF_RGBA16F,
        Vec3u { m_config.extent, 1 },
        TFM_NEAREST,
        TFM_NEAREST,
        TWM_CLAMP_TO_EDGE,
        1,
        IU_SAMPLED | IU_STORAGE });

    InitObject(m_texture);
}

void RaytracingReflections::CreateUniformBuffer()
{
    RTRadianceUniforms uniforms;
    Memory::MemSet(&uniforms, 0, sizeof(uniforms));

    m_uniformBuffers = {
        g_renderBackend->MakeGpuBuffer(GpuBufferType::CBUFF, sizeof(RTRadianceUniforms)),
        g_renderBackend->MakeGpuBuffer(GpuBufferType::CBUFF, sizeof(RTRadianceUniforms))
    };

    for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
    {
        m_uniformBuffers[frameIndex]->SetDebugName(NAME_FMT("RaytracingReflectionsUniformBuffer_{}", frameIndex));

        HYP_GFX_ASSERT(m_uniformBuffers[frameIndex]->Create());
        m_uniformBuffers[frameIndex]->Copy(sizeof(uniforms), &uniforms);
    }
}

void RaytracingReflections::CreateTemporalBlending()
{
    m_temporalBlending = MakeUnique<TemporalBlending>(
        m_config.extent,
        TF_RGBA16F,
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