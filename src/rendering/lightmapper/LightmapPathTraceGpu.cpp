#include <rendering/lightmapper/LightmapPathTraceGpu.hpp>

#include <rendering/rt/RenderAccelerationStructure.hpp>

#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/RenderHelpers.hpp>
#include <rendering/RenderCollection.hpp>
#include <rendering/RenderBackend.hpp>
#include <rendering/RenderCommand.hpp>
#include <rendering/RenderObject.hpp>
#include <rendering/RenderConfig.hpp>
#include <rendering/RenderDevice.hpp>
#include <rendering/RenderFrame.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/Material.hpp>
#include <rendering/Texture.hpp>
#include <rendering/Renderer.hpp>

#include <rendering/rt/MeshBlasBuilder.hpp>

#include <asset/TextureAsset.hpp>

#include <scene/World.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/Light.hpp>
#include <scene/EnvGrid.hpp>
#include <scene/View.hpp>

#include <scene/util/VoxelOctree.hpp>

#include <scene/lightmapper/LightmapVolume.hpp>

#include <scene/camera/Camera.hpp>
#include <scene/camera/OrthoCamera.hpp>

#include <scene/EntityManager.hpp>
#include <scene/components/LightmapVolumeComponent.hpp>
#include <scene/components/MeshComponent.hpp>
#include <scene/components/TransformComponent.hpp>
#include <scene/components/BoundingBoxComponent.hpp>

#include <core/threading/TaskSystem.hpp>
#include <core/threading/TaskThread.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/utilities/Time.hpp>
#include <core/utilities/DeferredScope.hpp>

#include <core/math/Triangle.hpp>

#include <util/Float16.hpp>
#include <util/MeshBuilder.hpp>

#include <system/AppContext.hpp>

#include <EngineGlobals.hpp>
#include <Engine.hpp>

namespace hyperion {

#pragma region Render commands

struct RENDER_COMMAND(CreateLightmapGPUPathTracerUniformBuffer)
    : RenderCommand
{
    GpuBufferRef uniformBuffer;

    RENDER_COMMAND(CreateLightmapGPUPathTracerUniformBuffer)(GpuBufferRef uniformBuffer)
        : uniformBuffer(std::move(uniformBuffer))
    {
    }

    virtual ~RENDER_COMMAND(CreateLightmapGPUPathTracerUniformBuffer)() override = default;

    virtual RendererResult operator()() override
    {
        HYP_GFX_CHECK(uniformBuffer->Create());
        uniformBuffer->Memset(sizeof(RTRadianceUniforms), 0x0);

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

#pragma region LightmapRenderer_GpuPathTracing

LightmapRenderer_GpuPathTracing::LightmapRenderer_GpuPathTracing(
    Lightmapper* lightmapper,
    const Handle<Scene>& scene,
    LightmapShadingType shadingType)
    : ILightmapRenderer(lightmapper),
      m_scene(scene),
      m_shadingType(shadingType)
{
    for (GpuBufferRef& uniformBuffer : m_uniformBuffers)
    {
        uniformBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::CBUFF, sizeof(RTRadianceUniforms));
    }

    for (GpuBufferRef& raysBuffer : m_raysBuffers)
    {
        raysBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::SSBO, sizeof(Vec4f) * 2 * (512 * 512));
    }

    m_hitsBufferGpu = g_renderBackend->MakeGpuBuffer(GpuBufferType::SSBO, sizeof(LightmapHit) * (512 * 512));
}

LightmapRenderer_GpuPathTracing::~LightmapRenderer_GpuPathTracing()
{
    SafeRelease(std::move(m_accelerationStructures));
    SafeRelease(std::move(m_uniformBuffers));
    SafeRelease(std::move(m_raysBuffers));
    SafeRelease(std::move(m_hitsBufferGpu));
    SafeRelease(std::move(m_raytracingPipeline));
}

void LightmapRenderer_GpuPathTracing::CreateUniformBuffer()
{
    for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
    {
        m_uniformBuffers[frameIndex] = g_renderBackend->MakeGpuBuffer(GpuBufferType::CBUFF, sizeof(RTRadianceUniforms));

        PUSH_RENDER_COMMAND(CreateLightmapGPUPathTracerUniformBuffer, m_uniformBuffers[frameIndex]);
    }
}

void LightmapRenderer_GpuPathTracing::Create()
{
    Assert(m_scene.IsValid());

    Assert(m_scene->GetWorld() != nullptr);
    Assert(m_scene->GetWorld()->IsReady());
}

void LightmapRenderer_GpuPathTracing::UpdatePipelineState(FrameBase* frame)
{
    HYP_SCOPE;

    Assert(m_lightmapper != nullptr);

    /// Create acceleration structure

    for (TLASRef& tlas : m_accelerationStructures)
    {
        if (!tlas)
        {
            tlas = g_renderBackend->MakeTLAS();
        }
    }

    if (m_accelerationStructures[frame->GetFrameIndex()]->IsCreated())
    {
        return; // already created
    }

    bool hasBlas = false;

    const Handle<View>& view = m_lightmapper->GetView();
    Assert(view != nullptr);

    RenderProxyList& rpl = RenderApi_GetConsumerProxyList(view);

    rpl.BeginRead();
    HYP_DEFER({ rpl.EndRead(); });

    for (Entity* entity : rpl.GetMeshEntities())
    {
        AssertDebug(entity != nullptr);

        RenderProxyMesh* meshProxy = rpl.GetMeshEntities().GetProxy(entity->Id());
        Assert(meshProxy != nullptr);

        AssertDebug(meshProxy->mesh != nullptr);

        BLASRef blas = MeshBlasBuilder::Build(meshProxy->mesh, meshProxy->material);
        Assert(blas != nullptr);

        blas->SetTransform(meshProxy->bufferData.modelMatrix);

        if (!blas->IsCreated())
        {
            HYP_GFX_ASSERT(blas->Create());
        }

        if (!m_accelerationStructures[frame->GetFrameIndex()]->HasBLAS(blas))
        {
            for (TLASRef& tlas : m_accelerationStructures)
            {
                tlas->AddBLAS(blas);
            }

            hasBlas = true;
        }
    }

    if (!hasBlas)
    {
        return;
    }

    for (TLASRef& tlas : m_accelerationStructures)
    {
        HYP_GFX_ASSERT(tlas->Create());
    }

    /// Buffers
    CreateUniformBuffer();

    DeferCreate(m_hitsBufferGpu);

    for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
    {
        DeferCreate(m_raysBuffers[frameIndex]);
    }
    
    /// Shader

    ShaderProperties shaderProperties;

    switch (m_shadingType)
    {
    case LightmapShadingType::RADIANCE:
        shaderProperties.Set(NAME("MODE_RADIANCE"));
        break;
    case LightmapShadingType::IRRADIANCE:
        shaderProperties.Set(NAME("MODE_IRRADIANCE"));
        break;
    default:
        HYP_UNREACHABLE();
    }

    ShaderRef shader = g_shaderManager->GetOrCreate(NAME("LightmapPathTracer"), shaderProperties);
    Assert(shader.IsValid());

    /// Descriptors

    const DescriptorTableDeclaration& descriptorTableDecl = shader->GetCompiledShader()->GetDescriptorTableDeclaration();

    DescriptorTableRef descriptorTable = g_renderBackend->MakeDescriptorTable(&descriptorTableDecl);

    for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
    {
        const DescriptorSetRef& descriptorSet = descriptorTable->GetDescriptorSet(NAME("RTRadianceDescriptorSet"), frameIndex);
        Assert(descriptorSet != nullptr);

        descriptorSet->SetElement(NAME("TLAS"), m_accelerationStructures[frameIndex]);
        descriptorSet->SetElement(NAME("MeshDescriptionsBuffer"), m_accelerationStructures[frameIndex]->GetMeshDescriptionsBuffer());
        descriptorSet->SetElement(NAME("HitsBuffer"), m_hitsBufferGpu);
        descriptorSet->SetElement(NAME("RaysBuffer"), m_raysBuffers[frameIndex]);

        descriptorSet->SetElement(NAME("LightsBuffer"), g_renderGlobalState->gpuBuffers[GRB_LIGHTS]->GetBuffer(frameIndex));
        descriptorSet->SetElement(NAME("MaterialsBuffer"), g_renderGlobalState->gpuBuffers[GRB_MATERIALS]->GetBuffer(frameIndex));

        descriptorSet->SetElement(NAME("RTRadianceUniforms"), m_uniformBuffers[frameIndex]);
    }

    DeferCreate(descriptorTable);

    /// Pipeline
    Assert(m_raytracingPipeline == nullptr);

    m_raytracingPipeline = g_renderBackend->MakeRaytracingPipeline(shader, descriptorTable);
    DeferCreate(m_raytracingPipeline);
        
    RTUpdateStateFlags updateStateFlags = RTUpdateStateFlagBits::RT_UPDATE_STATE_FLAGS_NONE;
    m_accelerationStructures[frame->GetFrameIndex()]->UpdateStructure(updateStateFlags);
}

void LightmapRenderer_GpuPathTracing::UpdateUniforms(FrameBase* frame, uint32 rayOffset)
{
    RTRadianceUniforms uniforms {};
    Memory::MemSet(&uniforms, 0, sizeof(uniforms));

    uniforms.rayOffset = rayOffset;

    // const uint32 maxBoundLights = MathUtil::Min(g_engine->GetRenderState()->NumBoundLights(), ArraySize(uniforms.lightIndices));
    // uint32 numBoundLights = 0;

    // for (uint32 lightType = 0; lightType < uint32(LT_MAX); lightType++) {
    //     if (numBoundLights >= maxBoundLights) {
    //         break;
    //     }

    //     for (const auto &it : g_engine->GetRenderState()->boundLights[lightType]) {
    //         if (numBoundLights >= maxBoundLights) {
    //             break;
    //         }

    //         uniforms.lightIndices[numBoundLights++] = it->GetBufferIndex();
    //     }
    // }

    // uniforms.numBoundLights = numBoundLights;

    // FIXME: Lights are now stored per-view.
    // We don't have a View for Lightmapper since it is for the entire World it is indirectly attached to.
    // We'll need to find a way to get the lights for the current view.
    // Ideas:
    // a) create a View for the Lightmapper and use that to get the lights. It will need to collect the lights on the Game thread so we'll need to add some kind of System to do that.
    // b) add a function to the RenderScene to get all the lights in the scene and use that to get the lights for the current view. This has a drawback that we will always have some RenderLight active when it could be inactive if it is not in any view.
    // OR: We can just use the lights in the current view and ignore the rest. This is a bit of a hack but it will work for now.
    //HYP_NOT_IMPLEMENTED();

    uniforms.numBoundLights = 0;

    m_uniformBuffers[frame->GetFrameIndex()]->Copy(sizeof(uniforms), &uniforms);
}

void LightmapRenderer_GpuPathTracing::UpdateRays(Span<const LightmapRay> rays)
{
}

void LightmapRenderer_GpuPathTracing::ReadHitsBuffer(FrameBase* frame, Span<LightmapHit> outHits)
{
    Assert(m_accelerationStructures[frame->GetFrameIndex()] != nullptr);

    const GpuBufferRef& hitsBuffer = m_hitsBufferGpu;
    Assert(hitsBuffer != nullptr && hitsBuffer->IsCreated());

    /*GpuBufferRef stagingBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::STAGING_BUFFER, outHits.Size() * sizeof(LightmapHit));
    HYP_GFX_ASSERT(stagingBuffer->Create());
    stagingBuffer->Memset(outHits.Size() * sizeof(LightmapHit), 0);

    UniquePtr<SingleTimeCommands> singleTimeCommands = g_renderBackend->GetSingleTimeCommands();

    singleTimeCommands->Push([&](RenderQueue& renderQueue)
        {
            const ResourceState previousResourceState = hitsBuffer->GetResourceState();

            // put src image in state for copying from
            renderQueue << InsertBarrier(hitsBuffer, RS_COPY_SRC);

            // put dst buffer in state for copying to
            renderQueue << InsertBarrier(stagingBuffer, RS_COPY_DST);

            renderQueue << CopyBuffer(stagingBuffer, hitsBuffer, outHits.Size() * sizeof(LightmapHit));

            renderQueue << InsertBarrier(stagingBuffer, RS_COPY_SRC);

            renderQueue << InsertBarrier(hitsBuffer, previousResourceState);
        });

    HYP_GFX_ASSERT(singleTimeCommands->Execute());

    stagingBuffer->Read(sizeof(LightmapHit) * outHits.Size(), outHits.Data());

    HYP_GFX_ASSERT(stagingBuffer->Destroy());*/
}

void LightmapRenderer_GpuPathTracing::Render(FrameBase* frame, const RenderSetup& renderSetup, LightmapJob* job, Span<const LightmapRay> rays, uint32 rayOffset)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.IsValid());

    const uint32 frameIndex = frame->GetFrameIndex();
    const uint32 previousFrameIndex = (frame->GetFrameIndex() + g_framesInFlight - 1) % g_framesInFlight;

    UpdatePipelineState(frame);
    UpdateUniforms(frame, rayOffset);

    if (m_accelerationStructures[frame->GetFrameIndex()] == nullptr)
    {
        return; // not ready yet this frame
    }

    { // rays buffer
        Array<Vec4f> rayData;
        rayData.Resize(rays.Size() * 2);

        for (SizeType i = 0; i < rays.Size(); i++)
        {
            rayData[i * 2] = Vec4f(rays[i].ray.position, 1.0f);
            rayData[i * 2 + 1] = Vec4f(rays[i].ray.direction, 0.0f);
        }

        bool raysBufferResized = false;

        HYP_GFX_ASSERT(m_raysBuffers[frame->GetFrameIndex()]->EnsureCapacity(rayData.ByteSize(), &raysBufferResized));
        m_raysBuffers[frame->GetFrameIndex()]->Copy(rayData.ByteSize(), rayData.Data());

        if (raysBufferResized)
        {
            m_raytracingPipeline->GetDescriptorTable()->GetDescriptorSet(NAME("RTRadianceDescriptorSet"), frame->GetFrameIndex())->SetElement(NAME("RaysBuffer"), m_raysBuffers[frame->GetFrameIndex()]);
        }

        bool hitsBufferResized = false;

        /*HYP_GFX_ASSERT(m_hitsBuffers[frame->GetFrameIndex()]->EnsureCapacity(rays.Size() * sizeof(LightmapHit), &hitsBufferResized));
        m_hitsBuffers[frame->GetFrameIndex()]->Memset(rays.Size() * sizeof(LightmapHit), 0);

        if (hitsBufferResized) {
            m_raytracingPipeline->GetDescriptorTable()->GetDescriptorSet(NAME("RTRadianceDescriptorSet"), frame->GetFrameIndex())
                ->SetElement(NAME("HitsBuffer"), m_hitsBuffers[frame->GetFrameIndex()]);
        }*/

        if (raysBufferResized || hitsBufferResized)
        {
            m_raytracingPipeline->GetDescriptorTable()->Update(frame->GetFrameIndex());
        }
    }

    frame->renderQueue << BindRaytracingPipeline(m_raytracingPipeline);

    frame->renderQueue << BindDescriptorTable(
        m_raytracingPipeline->GetDescriptorTable(),
        m_raytracingPipeline,
        ArrayMap<Name, ArrayMap<Name, uint32>> {
            { NAME("Global"),
                { { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(renderSetup.envGrid, 0) },
                    { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(renderSetup.envProbe, 0) } } } },
        frame->GetFrameIndex());

    frame->renderQueue << InsertBarrier(m_hitsBufferGpu, RS_UNORDERED_ACCESS);

    frame->renderQueue << TraceRays(
        m_raytracingPipeline,
        Vec3u { uint32(rays.Size()), 1, 1 });

    frame->renderQueue << InsertBarrier(m_hitsBufferGpu, RS_UNORDERED_ACCESS);
}

#pragma endregion LightmapRenderer_GpuPathTracing

#pragma region Lightmapper_GpuPathTracing

Lightmapper_GpuPathTracing::Lightmapper_GpuPathTracing(LightmapperConfig&& config, const Handle<Scene>& scene, const BoundingBox& aabb)
    : Lightmapper(std::move(config), scene, aabb)
{
}

#pragma endregion Lightmapper_GpuPathTracing

} // namespace hyperion
