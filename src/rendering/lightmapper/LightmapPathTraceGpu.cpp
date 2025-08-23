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

#include <rendering/util/SafeDeleter.hpp>

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
#include <scene/components/MeshComponent.hpp>
#include <scene/components/TransformComponent.hpp>
#include <scene/components/BoundingBoxComponent.hpp>

#include <core/threading/TaskSystem.hpp>
#include <core/threading/TaskThread.hpp>
#include <core/threading/Semaphore.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/utilities/Time.hpp>
#include <core/utilities/DeferredScope.hpp>

#include <core/math/Triangle.hpp>

#include <util/Float16.hpp>
#include <util/MeshBuilder.hpp>

#include <system/AppContext.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>

namespace hyperion {

struct GpuLightmapperReadyNotification : Semaphore<int>
{
};

#pragma region Render commands

struct RENDER_COMMAND(CreateLightmapGPUPathTracerUniformBuffer)
    : RenderCommand
{
    GpuBufferRef uniformBuffer;

    RENDER_COMMAND(CreateLightmapGPUPathTracerUniformBuffer)(GpuBufferRef uniformBuffer)
        : uniformBuffer(std::move(uniformBuffer))
    {
    }

    virtual RendererResult operator()() override
    {
        HYP_GFX_CHECK(uniformBuffer->Create());
        uniformBuffer->Memset(sizeof(RTRadianceUniforms), 0x0);

        return {};
    }
};

struct RENDER_COMMAND(SetGpuLightmapperReady)
    : RenderCommand
{
    RC<GpuLightmapperReadyNotification> notification;

    RENDER_COMMAND(SetGpuLightmapperReady)(const RC<GpuLightmapperReadyNotification>& notification)
        : notification(notification)
    {
        Assert(notification != nullptr);
    }

    virtual RendererResult operator()() override
    {
        notification->Produce();

        return {};
    }
};

#pragma endregion Render commands

#pragma region LightmapJob_GpuPathTracing

void LightmapJob_GpuPathTracing::GatherRays(uint32 maxRayHits, Array<LightmapRay>& outRays)
{
    for (uint32 rayIndex = 0; rayIndex < maxRayHits && HasRemainingTexels(); ++rayIndex)
    {
        const uint32 texelIndex = NextTexel();

        LightmapRay ray = m_uvMap->uvs[texelIndex].ray;
        ray.texelIndex = texelIndex;

        outRays.PushBack(ray);
    }
}

void LightmapJob_GpuPathTracing::IntegrateRayHits(Span<const LightmapRay> rays, Span<const LightmapHit> hits, LightmapShadingType shadingType)
{
    Assert(rays.Size() == hits.Size());

    LightmapUVMap& uvMap = GetUVMap();

    for (SizeType i = 0; i < hits.Size(); i++)
    {
        const LightmapRay& ray = rays[i];
        const LightmapHit& hit = hits[i];

        LightmapUV& uv = uvMap.uvs[ray.texelIndex];

        switch (shadingType)
        {
        case LightmapShadingType::RADIANCE:
            uv.radiance += Vec4f(hit.color, 1.0f); //= Vec4f(MathUtil::Lerp(uv.radiance.GetXYZ() * uv.radiance.w, hit.color.GetXYZ(), hit.color.w), 1.0f);
            break;
        case LightmapShadingType::IRRADIANCE:
            uv.irradiance += Vec4f(hit.color, 1.0f); //= Vec4f(MathUtil::Lerp(uv.irradiance.GetXYZ() * uv.irradiance.w, hit.color.GetXYZ(), hit.color.w), 1.0f);
            break;
        default:
            HYP_UNREACHABLE();
        }
    }
}

#pragma endregion LightmapJob_GpuPathTracing

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
        raysBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::SSBO, sizeof(Vec4f) * 2 * (512 * 512), alignof(Vec4f));
        raysBuffer->SetRequireCpuAccessible(true);
    }

    // ATOMIC_COUNTER type allows readback to cpu.
    m_hitsBufferGpu = g_renderBackend->MakeGpuBuffer(GpuBufferType::ATOMIC_COUNTER, sizeof(LightmapHit) * (512 * 512), alignof(Vec4f));

    m_readyNotification = MakeRefCountedPtr<GpuLightmapperReadyNotification>();
}

LightmapRenderer_GpuPathTracing::~LightmapRenderer_GpuPathTracing()
{
    SafeDelete(std::move(m_tlas));
    SafeDelete(std::move(m_uniformBuffers));
    SafeDelete(std::move(m_raysBuffers));
    SafeDelete(std::move(m_hitsBufferGpu));
    SafeDelete(std::move(m_raytracingPipeline));
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

    PUSH_RENDER_COMMAND(SetGpuLightmapperReady, m_readyNotification);
}

bool LightmapRenderer_GpuPathTracing::CanRender() const
{
    return m_readyNotification != nullptr && m_readyNotification->IsInSignalState();
}

void LightmapRenderer_GpuPathTracing::UpdatePipelineState(FrameBase* frame)
{
    HYP_SCOPE;

    Assert(m_lightmapper != nullptr);

    if (!m_tlas)
    {
        /// Create acceleration structure
        m_tlas = g_renderBackend->MakeTLAS();
    }
    else if (m_tlas->IsCreated())
    {
        RTUpdateStateFlags updateStateFlags = RTUpdateStateFlagBits::RT_UPDATE_STATE_FLAGS_NONE;
        m_tlas->UpdateStructure(updateStateFlags);

        if (updateStateFlags)
        {
            Assert(m_raytracingPipeline != nullptr);

            const DescriptorSetRef& descriptorSet = m_raytracingPipeline->GetDescriptorTable()->GetDescriptorSet("RTRadianceDescriptorSet", frame->GetFrameIndex());
            Assert(descriptorSet != nullptr);

            descriptorSet->Update(true);
        }

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

        if (meshProxy->material != nullptr)
        {
            const uint32 materialBinding = RenderApi_RetrieveResourceBinding(meshProxy->material);
            blas->SetMaterialBinding(materialBinding);
        }

        if (!blas->IsCreated())
        {
            HYP_GFX_ASSERT(blas->Create());
        }

        if (!m_tlas->HasBLAS(blas))
        {
            m_tlas->AddBLAS(blas);

            hasBlas = true;
        }
    }

    if (!hasBlas)
    {
        return;
    }

    HYP_GFX_ASSERT(m_tlas->Create());

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
        const DescriptorSetRef& descriptorSet = descriptorTable->GetDescriptorSet("RTRadianceDescriptorSet", frameIndex);
        Assert(descriptorSet != nullptr);

        descriptorSet->SetElement("TLAS", m_tlas);
        descriptorSet->SetElement("MeshDescriptionsBuffer", m_tlas->GetMeshDescriptionsBuffer());
        descriptorSet->SetElement("HitsBuffer", m_hitsBufferGpu);
        descriptorSet->SetElement("RaysBuffer", m_raysBuffers[frameIndex]);

        descriptorSet->SetElement("LightsBuffer", g_renderGlobalState->gpuBuffers[GRB_LIGHTS]->GetBuffer(frameIndex));
        descriptorSet->SetElement("MaterialsBuffer", g_renderGlobalState->gpuBuffers[GRB_MATERIALS]->GetBuffer(frameIndex));

        descriptorSet->SetElement("RTRadianceUniforms", m_uniformBuffers[frameIndex]);
    }

    DeferCreate(descriptorTable);

    /// Pipeline
    Assert(m_raytracingPipeline == nullptr);

    m_raytracingPipeline = g_renderBackend->MakeRaytracingPipeline(shader, descriptorTable);
    DeferCreate(m_raytracingPipeline);
}

void LightmapRenderer_GpuPathTracing::UpdateUniforms(FrameBase* frame, uint32 rayOffset)
{
    RTRadianceUniforms uniforms {};
    Memory::MemSet(&uniforms, 0, sizeof(uniforms));

    uniforms.rayOffset = rayOffset;

    RenderProxyList& rpl = RenderApi_GetConsumerProxyList(m_lightmapper->GetView());
    rpl.BeginRead();
    HYP_DEFER({ rpl.EndRead(); });

    const uint32 maxBoundLights = ArraySize(uniforms.lightIndices);
    uint32 numBoundLights = 0;

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

void LightmapRenderer_GpuPathTracing::UpdateRays(Span<const LightmapRay> rays)
{
}

void LightmapRenderer_GpuPathTracing::ReadHitsBuffer(FrameBase* frame, Span<LightmapHit> outHits)
{
    Assert(m_tlas != nullptr);

    const GpuBufferRef& hitsBuffer = m_hitsBufferGpu;

    if (!hitsBuffer || !hitsBuffer->IsCreated())
    {
        return; // no hit data
    }

    Assert(hitsBuffer->Size() >= outHits.Size() * sizeof(LightmapHit));

    GpuBufferRef stagingBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::STAGING_BUFFER, outHits.Size() * sizeof(LightmapHit), alignof(Vec4f));
    Assert(stagingBuffer->Create());
    stagingBuffer->Memset(outHits.Size() * sizeof(LightmapHit), 0);

    UniquePtr<SingleTimeCommands> singleTimeCommands = g_renderBackend->GetSingleTimeCommands();

    singleTimeCommands->Push([&](RenderQueue& renderQueue)
        {
            const ResourceState previousResourceState = hitsBuffer->GetResourceState();

            renderQueue << InsertBarrier(hitsBuffer, RS_COPY_SRC);
            renderQueue << InsertBarrier(stagingBuffer, RS_COPY_DST);

            renderQueue << CopyBuffer(hitsBuffer, stagingBuffer, outHits.Size() * sizeof(LightmapHit));

            renderQueue << InsertBarrier(stagingBuffer, RS_COPY_SRC);
            renderQueue << InsertBarrier(hitsBuffer, previousResourceState);
        });

    Assert(singleTimeCommands->Execute());

    stagingBuffer->Read(sizeof(LightmapHit) * outHits.Size(), outHits.Data());

#ifdef HYP_DEBUG_MODE
    // nan / infinity check for debugging
    for (LightmapHit& hit : outHits)
    {
        Assert(!MathUtil::IsNaN(hit.color) && MathUtil::IsFinite(hit.color));
    }
#endif

    stagingBuffer.Reset();
}

void LightmapRenderer_GpuPathTracing::Render(FrameBase* frame, const RenderSetup& renderSetup, LightmapJob* job, Span<const LightmapRay> rays, uint32 rayOffset)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    Assert(CanRender());

    AssertDebug(renderSetup.IsValid());

    const uint32 frameIndex = frame->GetFrameIndex();
    const uint32 previousFrameIndex = (frame->GetFrameIndex() + g_framesInFlight - 1) % g_framesInFlight;

    UpdatePipelineState(frame);

    if (!m_tlas->IsCreated())
    {
        // no BLAS to process if TLAS not created
        return;
    }

    UpdateUniforms(frame, rayOffset);

    Assert(m_tlas && m_tlas->IsCreated());

    { // rays buffer
        Array<Vec4f> rayData;
        rayData.Resize(rays.Size() * 2);

        for (SizeType i = 0; i < rays.Size(); i++)
        {
            rayData[i * 2] = Vec4f(rays[i].ray.position, 1.0f);
            rayData[i * 2 + 1] = Vec4f(rays[i].ray.direction, 0.0f);
        }

        Assert(m_raysBuffers[frame->GetFrameIndex()]->Size() >= rayData.ByteSize());
        m_raysBuffers[frame->GetFrameIndex()]->Copy(rayData.ByteSize(), rayData.Data());

        // bool raysBufferResized = false;

        // HYP_GFX_ASSERT(m_raysBuffers[frame->GetFrameIndex()]->EnsureCapacity(rayData.ByteSize(), &raysBufferResized));
        // m_raysBuffers[frame->GetFrameIndex()]->Copy(rayData.ByteSize(), rayData.Data());

        // if (raysBufferResized)
        //{
        //     m_raytracingPipeline->GetDescriptorTable()->GetDescriptorSet("RTRadianceDescriptorSet", frame->GetFrameIndex())->SetElement("RaysBuffer", m_raysBuffers[frame->GetFrameIndex()]);
        // }

        // bool hitsBufferResized = false;

        ///*HYP_GFX_ASSERT(m_hitsBuffers[frame->GetFrameIndex()]->EnsureCapacity(rays.Size() * sizeof(LightmapHit), &hitsBufferResized));
        // m_hitsBuffers[frame->GetFrameIndex()]->Memset(rays.Size() * sizeof(LightmapHit), 0);

        // if (hitsBufferResized) {
        //     m_raytracingPipeline->GetDescriptorTable()->GetDescriptorSet("RTRadianceDescriptorSet", frame->GetFrameIndex())
        //         ->SetElement("HitsBuffer", m_hitsBuffers[frame->GetFrameIndex()]);
        // }*/

        // if (raysBufferResized || hitsBufferResized)
        //{
        //     m_raytracingPipeline->GetDescriptorTable()->Update(frame->GetFrameIndex());
        // }
    }

    frame->renderQueue << BindRaytracingPipeline(m_raytracingPipeline);

    frame->renderQueue << BindDescriptorTable(
        m_raytracingPipeline->GetDescriptorTable(),
        m_raytracingPipeline,
        { { "Global",
            { { "EnvGridsBuffer", ShaderDataOffset<EnvGridShaderData>(renderSetup.envGrid, 0) },
                { "CurrentEnvProbe", ShaderDataOffset<EnvProbeShaderData>(renderSetup.envProbe, 0) } } } },
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
