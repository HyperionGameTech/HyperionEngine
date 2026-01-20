/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/RenderInterface.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/ShaderManager.hpp>
#include <rendering/RenderBackend.hpp>
#include <rendering/Frame.hpp>
#include <rendering/GpuBuffer.hpp>
#include <rendering/RenderResult.hpp>
#include <rendering/RenderProxyList.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/Texture.hpp>
#include <rendering/RenderCollection.hpp>
#include <rendering/DescriptorSet.hpp>
#include <rendering/Shader.hpp>
#include <rendering/TextureViewCache.hpp>

#include <rendering/renderers/DeferredRenderer.hpp>

#include <rendering/AccelerationStructure.hpp>
#include <rendering/RTReflections.hpp>
#include <rendering/DDGI.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <scene/View.hpp>
#include <scene/Light.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/EnvGrid.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <RayTracingReflections.generated.inl>

namespace Hyperion {

static const Name s_shaderNames[] = { NAME("RTRadiance"), NAME("PathTracer") };
static constexpr uint32 MaxLights = sizeof(RayTracingConstants::lightIndices) / sizeof(uint32);

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
            g_renderInterface->globalDescriptorTable->GetDescriptorSet("Global"_sh, frameIndex)
                ->SetElement("RTRadianceResultTexture"_sh, g_renderInterface->placeholderData->GetImageView2D1x1R8());
        }

        return result;
    }
};

#pragma endregion Render commands

RayTracingReflections::RayTracingReflections(RayTracingReflectionsConfig&& config, GBuffer* gbuffer)
    : m_config(std::move(config)),
      m_gbuffer(gbuffer)
{
}

RayTracingReflections::~RayTracingReflections()
{
    SafeDelete(std::move(m_rayTracingPipeline));

    // remove result image from global descriptor set
    SafeDelete(std::move(m_texture));

    PUSH_RENDER_COMMAND(UnsetRTRadianceImageInGlobalDescriptorSet);
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

void RayTracingReflections::UpdatePipelineState(Frame* frame, const RenderSetup& renderSetup)
{
    HYP_SCOPE;

    RayTracingPassData* pd = ObjCast<RayTracingPassData>(renderSetup.passData);
    Assert(pd != nullptr);

    const auto SetDescriptorElements = [this, pd](DescriptorSet* descriptorSet, const GpuTlasRef& tlas, uint32 frameIndex)
    {
        Assert(tlas != nullptr);

        descriptorSet->SetElement("TLAS"_sh, tlas);
        descriptorSet->SetElement("MeshDescriptionsBuffer"_sh, tlas->GetMeshDescriptionsBuffer());
        descriptorSet->SetElement("OutputImage"_sh, g_renderInterface->textureViewCache->GetOrCreate(m_texture));
        descriptorSet->SetElement("RayTracingConstants"_sh, pd->constants);
        descriptorSet->SetElement("Lights"_sh, pd->lightsBuffer);
        descriptorSet->SetElement("MaterialsBuffer"_sh, g_renderInterface->gpuBuffers[GRB_MATERIALS]->GetBuffer(frameIndex));
    };

    if (!m_rayTracingPipeline)
    {
        ShaderRef shader = g_shaderManager->GetOrCreate(
            s_shaderNames[IsPathTracer()],
            ShaderProperties({
                { NAME("MAX_LIGHTS"), int(MaxLights) }
            }));

        Assert(shader != nullptr);

        m_rayTracingPipeline = g_renderBackend->MakeRayTracingPipeline(shader, DescriptorTableRef::Null());
        Assert(m_rayTracingPipeline->Create());

        for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
        {
            g_renderInterface->globalDescriptorTable->GetDescriptorSet("Global"_sh, frameIndex)
                ->SetElement("RTRadianceResultTexture"_sh, g_renderInterface->textureViewCache->GetOrCreate(m_temporalBlending->GetResultTexture()));
        }
    }
    
    DescriptorSetRef& descriptorSet = pd->rayTracingDescriptorSets[frame->GetFrameIndex()];
    bool needsCreate = false;

    if (!descriptorSet)
    {
        const DescriptorTableDeclaration* tableDecl = m_rayTracingPipeline->GetShader()->GetCompiledShader()->GetDescriptorTableDeclaration();
        AssertDebug(tableDecl != nullptr);

        const DescriptorSetDeclaration* setDecl = tableDecl->FindDescriptorSetDeclaration("RTRadianceDescriptorSet"_sh);
        AssertDebug(setDecl != nullptr);

        DescriptorSetLayout setLayout { setDecl };

        descriptorSet = g_renderBackend->MakeDescriptorSet(setLayout);
        Assert(descriptorSet != nullptr);

        needsCreate = true;
    }
    
    SetDescriptorElements(descriptorSet, pd->rayTracingTlases[frame->GetFrameIndex()], frame->GetFrameIndex());

    if (needsCreate)
    {
        Assert(descriptorSet->Create());
    }
    else
    {
        descriptorSet->UpdateDirtyState();
        descriptorSet->Update(true); // temp
    }
}

void RayTracingReflections::UpdateUniforms(Frame* frame, const RenderSetup& renderSetup)
{
    RayTracingPassData* pd = ObjCast<RayTracingPassData>(renderSetup.passData);
    Assert(pd != nullptr);
    
    GpuBufferRef& constants = pd->constants;
    if (!constants)
    {
        constants = g_renderBackend->MakeGpuBuffer(GpuBufferType::CBUFF, sizeof(RayTracingConstants));
        constants->SetRequireCpuAccessible(true);
        constants->SetDebugName(NAME("RayTracingConstants"));
        Assert(constants->Create());
    }

    GpuBufferRef& lightsBuffer = pd->lightsBuffer;
    if (!lightsBuffer)
    {
        lightsBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::CBUFF, sizeof(LightShaderData) * MaxLights);
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

            RenderProxyList& rpl = RenderApi::GetConsumerProxyList(view);
            rpl.BeginRead();
            HYP_DEFER({ rpl.EndRead(); });
            
            GpuBufferRef& lightsBuffer = passData->lightsBuffer;
            AssertDebug(lightsBuffer != nullptr);

            GpuBufferRef& constants = passData->constants;
            AssertDebug(constants != nullptr);

            RayTracingConstants constantData {};
            constantData.minRoughness = 0.4f;
            constantData.outputImageResolution = extent;

            uint32 numBoundLights = 0;
    
            uint32* lightIndicesU32 = reinterpret_cast<uint32*>(constantData.lightIndices);
            Memory::MemSet(lightIndicesU32, 0, sizeof(constantData.lightIndices));

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

                RenderProxyLight* lightProxy = static_cast<RenderProxyLight*>(RenderApi::GetRenderProxy(light));
                Assert(lightProxy != nullptr);
                
                lightsBuffer->Copy(numBoundLights * sizeof(LightShaderData), sizeof(LightShaderData), &lightProxy->bufferData);

                lightIndicesU32[numBoundLights++] = RenderApi::RetrieveResourceBinding(light);
            }

            constantData.numBoundLights = numBoundLights;

            constants->Copy(sizeof(RayTracingConstants), &constantData);
            constants->Flush(0, sizeof(RayTracingConstants));

            lightsBuffer->Flush(0, sizeof(LightShaderData) * numBoundLights);
        }
    };

    g_renderBackend->GetCurrentFrame()->OnFrameEnd.Bind(UpdateRayTracingBuffers {
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
    UpdatePipelineState(frame, renderSetup);

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

    const DescriptorTableDeclaration* tableDecl = m_rayTracingPipeline->GetShader()->GetCompiledShader()->GetDescriptorTableDeclaration();
    AssertDebug(tableDecl != nullptr);

    static const uint32 s_globalDescriptorSetIndex = tableDecl->GetDescriptorSetIndex("Global"_sh);
    static const uint32 s_viewDescriptorSetIndex = tableDecl->GetDescriptorSetIndex("View"_sh);
    static const uint32 s_bindlessDescriptorSetIndex = tableDecl->GetDescriptorSetIndex("GlobalBindless"_sh);
    static const uint32 s_rayTracingDescriptorSetIndex = tableDecl->GetDescriptorSetIndex("RTRadianceDescriptorSet"_sh);

    frame->renderQueue << BindRayTracingPipeline(m_rayTracingPipeline);
    
    frame->renderQueue << BindDescriptorSet(
        g_renderInterface->globalDescriptorTable->GetDescriptorSet("Global"_sh, frame->GetFrameIndex()),
        m_rayTracingPipeline,
        { { "CamerasBuffer"_sh, ShaderDataOffset<CameraShaderData>(renderSetup.view->GetCamera()) },
            { "EnvGridsBuffer"_sh, ShaderDataOffset<EnvGridShaderData>(renderSetup.envGrid, 0) },
            { "CurrentEnvProbe"_sh, ShaderDataOffset<EnvProbeShaderData>(renderSetup.envProbe, 0) } },
        s_globalDescriptorSetIndex);

    frame->renderQueue << BindDescriptorSet(
        g_renderInterface->globalDescriptorTable->GetDescriptorSet("GlobalBindless"_sh, frame->GetFrameIndex()),
        m_rayTracingPipeline,
        {},
        s_bindlessDescriptorSetIndex);

    frame->renderQueue << BindDescriptorSet(
        parentPass->descriptorSets[frame->GetFrameIndex()],
        m_rayTracingPipeline,
        {},
        s_viewDescriptorSetIndex);
    
    frame->renderQueue << BindDescriptorSet(
        pd->rayTracingDescriptorSets[frame->GetFrameIndex()],
        m_rayTracingPipeline,
        {},
        s_rayTracingDescriptorSetIndex);

    frame->renderQueue << InsertBarrier(m_texture->GetGpuImage(), RS_UNORDERED_ACCESS);

    const Vec3u imageExtent = m_texture->GetGpuImage()->GetExtent();
    const SizeType numPixels = imageExtent.Volume();

    frame->renderQueue << TraceRays(m_rayTracingPipeline, Vec3u { uint32(numPixels), 1, 1 });
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

    m_texture = CreateObject<Texture>(TextureDesc {
        TT_TEX2D,
        TF_RGBA8,
        Vec3u { m_config.extent, 1 },
        TFM_NEAREST,
        TFM_NEAREST,
        TWM_CLAMP_TO_EDGE,
        1,
        IU_SAMPLED | IU_STORAGE });

    m_texture->SetName(NAME("RayTracingReflectionsTexture"));

    InitObject(m_texture);
}

void RayTracingReflections::CreateTemporalBlending()
{
    m_temporalBlending = MakeUnique<TemporalBlending>(
        m_config.extent,
        TF_RGBA8,
        IsPathTracer()
            ? TemporalBlendTechnique::TECHNIQUE_4 // progressive blending
            : TemporalBlendTechnique::TECHNIQUE_1,
        DefaultTemporalBlendingFeedback,
        g_renderInterface->textureViewCache->GetOrCreate(m_texture),
        m_gbuffer);

    m_temporalBlending->Create();
}

} // namespace Hyperion
