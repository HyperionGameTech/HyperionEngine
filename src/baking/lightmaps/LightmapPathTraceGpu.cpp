#include <HyperionPch.hpp>

#include <baking/lightmaps/LightmapPathTraceGpu.hpp>

#include <baking/LightmapTexel.hpp>

#include <rendering/AccelerationStructure.hpp>
#include <rendering/RayTracingPipeline.hpp>

#include <rendering/RenderInterface.hpp>
#include <rendering/RenderHelpers.hpp>
#include <rendering/RenderCollection.hpp>
#include <rendering/RenderProxyList.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/RenderCommand.hpp>
#include <rendering/RenderObject.hpp>
#include <rendering/RenderConfig.hpp>
#include <rendering/Device.hpp>
#include <rendering/Frame.hpp>
#include <rendering/ShaderManager.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/Material.hpp>
#include <rendering/Texture.hpp>
#include <rendering/RendererBase.hpp>
#include <rendering/DescriptorSet.hpp>
#include <rendering/Shader.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/TextureViewCache.hpp>

#include <rendering/util/SafeDeleter.hpp>
#include <rendering/util/ShaderCompiler.hpp>

#include <rendering/MeshBlasBuilder.hpp>

#include <scene/World.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/Light.hpp>
#include <scene/EnvGrid.hpp>
#include <scene/View.hpp>
#include <scene/EntityManager.hpp>
#include <scene/LightmapVolume.hpp>

#include <scene/components/MeshComponent.hpp>
#include <scene/components/TransformComponent.hpp>

#include <scene/util/VoxelOctree.hpp>

#include <scene/camera/Camera.hpp>
#include <scene/camera/OrthoCamera.hpp>

#include <core/threading/TaskSystem.hpp>
#include <core/threading/TaskThread.hpp>
#include <core/threading/Semaphore.hpp>

#include <core/utilities/Time.hpp>
#include <core/utilities/DeferredScope.hpp>
#include <core/utilities/Float16.hpp>

#include <core/math/Triangle.hpp>

#include <util/MeshBuilder.hpp>

#include <system/AppContext.hpp>

#include <engine/EngineDriver.hpp>
#include <engine/EngineGlobals.hpp>

namespace Hyperion {

struct GpuLightmapperReadyNotification : Semaphore<int>
{
};

static constexpr uint32 MaxBoundLights = sizeof(RayTracingConstants::lightIndices) / sizeof(uint32);  

#pragma region Render commands

struct CreateLightmapGPUPathTracerUniformBuffer : RenderCommand
{
    GpuBufferRef uniformBuffer;

    CreateLightmapGPUPathTracerUniformBuffer(GpuBufferRef uniformBuffer)
        : uniformBuffer(std::move(uniformBuffer))
    {
    }

    virtual RendererResult operator()() override
    {
        CheckResultOrReturn(uniformBuffer->Create());
        uniformBuffer->Memset(sizeof(RayTracingConstants), 0x0);

        return {};
    }
};

struct SetGpuLightmapperReady : RenderCommand
{
    RC<GpuLightmapperReadyNotification> notification;

    SetGpuLightmapperReady(const RC<GpuLightmapperReadyNotification>& notification)
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

namespace Baking {

#pragma region LightmapRenderer_GpuPathTracing

static ShaderDesc GetShaderDesc(LightmapShadingType shadingType)
{
    ShaderPropertySet shaderProperties;
    shaderProperties.Add(InternShaderProperty(ShaderProperty(NAME("MAX_LIGHTS"), int(MaxBoundLights))));

    switch (shadingType)
    {
    case LightmapShadingType::RADIANCE:
        shaderProperties.Add(InternShaderProperty(ShaderProperty(NAME("MODE"), NAME("RADIANCE"))));
        break;
    case LightmapShadingType::IRRADIANCE:
        shaderProperties.Add(InternShaderProperty(ShaderProperty(NAME("MODE"), NAME("IRRADIANCE"))));
        break;
    case LightmapShadingType::FULL:
        shaderProperties.Add(InternShaderProperty(ShaderProperty(NAME("MODE"), NAME("FULL"))));
        break;
    default:
        HYP_UNREACHABLE();
    }

    return ShaderDesc(NAME("LightmapPathTracer"), shaderProperties);
}

LightmapRenderer_GpuPathTracing::LightmapRenderer_GpuPathTracing(
    BakerBase* lightmapper,
    const Handle<Scene>& scene,
    LightmapShadingType shadingType,
    uint32 maxTexelsPerFrame)
    : ILightmapRenderer(lightmapper),
      m_scene(scene),
      m_shadingType(shadingType),
      m_maxTexelsPerFrame(maxTexelsPerFrame)
{
    m_readyNotification = MakeRefCountedPtr<GpuLightmapperReadyNotification>();
}

LightmapRenderer_GpuPathTracing::~LightmapRenderer_GpuPathTracing()
{
    SafeDelete(std::move(m_tlas));

    for (KeyValuePair<BakeJobBase*, JobData>& it : m_jobData)
    {
        SafeDelete(std::move(it.second.cBuffer));
        SafeDelete(std::move(it.second.raysBuffer));
        SafeDelete(std::move(it.second.lightsBuffer));
        SafeDelete(std::move(it.second.hitsBufferGpu));
    }
}

void LightmapRenderer_GpuPathTracing::CreateBuffers(BakeJobBase* job)
{
    JobData& jd = m_jobData[job];

    jd.cBuffer = g_renderInterface->MakeGpuBuffer(GpuBufferType::CONSTANT_BUFFER, sizeof(RayTracingConstants));
    PUSH_RENDER_COMMAND(CreateLightmapGPUPathTracerUniformBuffer, jd.cBuffer);

    jd.raysBuffer = g_renderInterface->MakeGpuBuffer(GpuBufferType::STORAGE_BUFFER, sizeof(Vec4f) * 2 * m_maxTexelsPerFrame, alignof(Vec4f));
    jd.raysBuffer->SetRequireCpuAccessible(true);

    jd.lightsBuffer = g_renderInterface->MakeGpuBuffer(GpuBufferType::CONSTANT_BUFFER, sizeof(LightShaderData) * MaxBoundLights);

    // ATOMIC_COUNTER type allows readback to cpu.
    jd.hitsBufferGpu = g_renderInterface->MakeGpuBuffer(GpuBufferType::ATOMIC_COUNTER, sizeof(LightmapHit) * m_maxTexelsPerFrame, alignof(Vec4f));

    DeferCreate(jd.hitsBufferGpu);
    DeferCreate(jd.raysBuffer);
    DeferCreate(jd.lightsBuffer);
}

void LightmapRenderer_GpuPathTracing::Create()
{
    PUSH_RENDER_COMMAND(SetGpuLightmapperReady, m_readyNotification);
}

void LightmapRenderer_GpuPathTracing::CleanJobData(BakeJobBase* job)
{
    if (!job)
    {
        return;
    }

    auto jobDataIt = m_jobData.Find(job);
    AssertDebug(jobDataIt != m_jobData.End());

    if (jobDataIt == m_jobData.End())
    {
        return;
    }

    m_jobData.Erase(jobDataIt);
}

bool LightmapRenderer_GpuPathTracing::CanRender() const
{
    return m_readyNotification != nullptr && m_readyNotification->IsInSignalState();
}

void LightmapRenderer_GpuPathTracing::CreateAccelerationStructures()
{
    if (!m_tlas)
    {
        /// Create acceleration structure
        m_tlas = g_renderInterface->MakeTLAS();
    }
    else if (m_tlas->IsCreated())
    {
        return; // already created
    }

    bool hasBlas = false;

    const Handle<View>& view = m_lightmapper->GetView();
    Assert(view != nullptr);

    RenderProxyList& rpl = GetConsumerProxyList(view);
    rpl.BeginRead();
    HYP_DEFER({ rpl.EndRead(); });

    for (Entity* entity : rpl.GetMeshEntities())
    {
        AssertDebug(entity != nullptr);

        RenderProxyMesh* meshProxy = rpl.GetMeshEntities().GetProxy(entity->Id());
        Assert(meshProxy != nullptr);

        AssertDebug(meshProxy->mesh != nullptr);

        GpuBlasRef blas = MeshBlasBuilder::Build(meshProxy->mesh, meshProxy->material);
        Assert(blas != nullptr);

        blas->SetTransform(meshProxy->bufferData.modelMatrix);

        if (meshProxy->material != nullptr)
        {
            const uint32 materialBinding = RetrieveResourceBinding(meshProxy->material);
            blas->SetMaterialBinding(materialBinding);
        }

        if (!blas->IsCreated())
        {
            CheckResult(blas->Create());
        }

        if (!m_tlas->HasGpuBlas(blas))
        {
            m_tlas->AddGpuBlas(blas);

            hasBlas = true;
        }
    }

    if (!hasBlas)
    {
        return;
    }

    CheckResult(m_tlas->Create());
}

void LightmapRenderer_GpuPathTracing::UpdatePipelineState(Frame* frame, BakeJobBase* job)
{
    HYP_SCOPE;

    Assert(m_lightmapper != nullptr);

    JobData& jd = m_jobData[job];

    if (jd.isCreated)
    {
        return;
    }

    CreateBuffers(job);

    jd.isCreated = true;
}

void LightmapRenderer_GpuPathTracing::UpdateUniforms(Frame* frame, BakeJobBase* job, uint32 rayOffset)
{
    struct UpdateConstants
    {
        View* view = nullptr;
        GpuBuffer* cBuffer = nullptr;
        GpuBuffer* lightsBuffer = nullptr;
        uint32 rayOffset;

        void operator()(Frame*)
        {
            AssertDebug(view && cBuffer && lightsBuffer);

            RenderProxyList& rpl = GetConsumerProxyList(view);
            rpl.BeginRead();
            HYP_DEFER({ rpl.EndRead(); });

            RayTracingConstants uniforms {};
            Memory::Fill(&uniforms, 0, sizeof(uniforms));

            uniforms.rayOffset = rayOffset;

            uint32 numBoundLights = 0;

            for (Light* light : rpl.GetLights())
            {
                const LightType lightType = light->GetLightType();

                if (lightType != LT_DIRECTIONAL && lightType != LT_POINT)
                {
                    continue;
                }

                if (numBoundLights >= MaxBoundLights)
                {
                    break;
                }
                
                RenderProxyLight* lightProxy = static_cast<RenderProxyLight*>(GetRenderProxy(light));
                Assert(lightProxy != nullptr);

                lightsBuffer->Copy(numBoundLights * sizeof(LightShaderData), sizeof(LightShaderData), &lightProxy->bufferData);

                uniforms.lightIndices[numBoundLights++] = RetrieveResourceBinding(light);
            }

            uniforms.numBoundLights = numBoundLights;

            cBuffer->Copy(sizeof(uniforms), &uniforms);
        }
    };
    
    JobData& jd = m_jobData[job];

    frame->OnFrameEnd.Bind(UpdateConstants {
        m_lightmapper->GetView(),
        jd.cBuffer,
        jd.lightsBuffer,
        rayOffset
    }).Detach();
}

void LightmapRenderer_GpuPathTracing::ReadHitsBuffer(Frame* frame, BakeJobBase* job, Span<LightmapHit> outHits)
{
    Assert(m_tlas != nullptr);

    JobData& jd = m_jobData[job];

    const GpuBufferRef& hitsBuffer = jd.hitsBufferGpu;

    if (!hitsBuffer || !hitsBuffer->IsCreated())
    {
        return; // no hit data
    }

    Assert(hitsBuffer->Size() >= outHits.Size() * sizeof(LightmapHit));

    GpuBufferRef stagingBuffer = g_renderInterface->MakeGpuBuffer(GpuBufferType::STAGING_BUFFER, outHits.Size() * sizeof(LightmapHit));
    Assert(stagingBuffer->Create());

    UniquePtr<SingleTimeCommands> singleTimeCommands = g_renderInterface->GetSingleTimeCommands();

    singleTimeCommands->Push([&](RenderQueue& renderQueue)
        {
            const ResourceState previousResourceState = hitsBuffer->GetResourceState();

            renderQueue << InsertBarrier(hitsBuffer, RS_COPY_SRC);
            renderQueue << InsertBarrier(stagingBuffer, RS_COPY_DST);

            renderQueue << CopyBuffer(hitsBuffer, stagingBuffer, uint32(outHits.Size() * sizeof(LightmapHit)));

            renderQueue << InsertBarrier(stagingBuffer, RS_COPY_SRC);
            renderQueue << InsertBarrier(hitsBuffer, previousResourceState);
        });

    Assert(singleTimeCommands->Execute());

    stagingBuffer->Read(sizeof(LightmapHit) * outHits.Size(), outHits.Data());
    stagingBuffer.Reset();
}

void LightmapRenderer_GpuPathTracing::Render(Frame* frame, const RenderSetup& renderSetup, BakeJobBase* job, Span<const LightmapRay> rays, uint32 rayOffset)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    if (rays.Size() == 0)
    {
        return;
    }

    Assert(CanRender());

    AssertDebug(renderSetup.world);

    const uint32 frameIndex = frame->GetFrameIndex();
    const uint32 previousFrameIndex = (frame->GetFrameIndex() + NumFramesInFlight - 1) % NumFramesInFlight;

    CreateAccelerationStructures();

    if (!m_tlas->IsCreated())
    {
        // no GpuBlas to process if TLAS not created
        return;
    }

    UpdatePipelineState(frame, job);
    UpdateUniforms(frame, job, rayOffset);

    JobData& jd = m_jobData[job];

    Assert(m_tlas && m_tlas->IsCreated());

    { // rays buffer
        Array<Vec4f, DynamicAllocator> rayData;
        rayData.Resize(rays.Size() * 2);

        for (SizeType i = 0; i < rays.Size(); i++)
        {
            rayData[i * 2] = Vec4f(rays[i].ray.position, 1.0f);
            rayData[i * 2 + 1] = Vec4f(rays[i].ray.direction, 0.0f);
        }
        
        GpuBufferRef& raysBuffer = jd.raysBuffer;

        struct UpdateRaysBuffer
        {
            GpuBuffer* raysBuffer;
            Array<Vec4f, DynamicAllocator> rayData;

            void operator()(Frame*)
            {
                Assert(raysBuffer->Size() >= rayData.ByteSize());
                raysBuffer->Copy(rayData.ByteSize(), rayData.Data());
                raysBuffer->Flush(0, rayData.ByteSize());
            }
        };

        frame->OnFrameEnd.Bind(UpdateRaysBuffer { raysBuffer, std::move(rayData) }).Detach();
    }

    RenderQueue& rq = frame->renderQueue;

    rq << SetCurrentShader(GetShaderDesc(m_shadingType));

    rq << SetShaderUniform(0, "TLAS"_sh, m_tlas);
    rq << SetShaderUniform(1, "MeshDescriptionsBuffer"_sh, m_tlas->GetMeshDescriptionsBuffer());
    rq << SetShaderUniform(2, "HitsBuffer"_sh, jd.hitsBufferGpu);
    rq << SetShaderUniform(3, "RaysBuffer"_sh, jd.raysBuffer);
    rq << SetShaderUniform(4, "Lights"_sh, jd.lightsBuffer);
    rq << SetShaderUniform(5, "MaterialsBuffer"_sh, g_renderInterface->gpuBuffers[GRB_MATERIALS]->GetBuffer(frameIndex));
    rq << SetShaderUniform(6, "RayTracingConstants"_sh, jd.cBuffer);
    
    rq << SetShaderUniform(7, "SamplerNearest"_sh, g_renderInterface->placeholderData->GetSamplerNearest());
    rq << SetShaderUniform(8, "SamplerLinear"_sh, g_renderInterface->placeholderData->GetSamplerLinear());

    rq << SetShaderUniform(9, "BlueNoiseBuffer"_sh, g_renderInterface->blueNoiseBuffer);

    rq << SetShaderUniform(10, "WorldsBuffer"_sh, g_renderInterface->gpuBuffers[GRB_WORLDS]->GetBuffer(frameIndex));
    rq << SetShaderUniform(11, "EntitiesBuffer"_sh, g_renderInterface->gpuBuffers[GRB_ENTITIES]->GetBuffer(frameIndex));
    
    rq << SetShaderUniform(12, "EnvProbesTexture"_sh, g_renderInterface->textureViewCache->GetOrCreate(g_renderInterface->envProbesTexture));
    rq << SetShaderUniform(13, "EnvProbesBuffer"_sh, g_renderInterface->gpuBuffers[GRB_ENV_PROBES]->GetBuffer(frameIndex));

    if (renderSetup.envProbe)
        rq << SetShaderUniform(14, "CurrentEnvProbe"_sh, g_renderInterface->gpuBuffers[GRB_ENV_PROBES]->GetBuffer(frameIndex), TShaderDataOffset<EnvProbeShaderData>(renderSetup.envProbe));
    else
        rq << SetShaderUniform(14, "CurrentEnvProbe"_sh, g_renderInterface->gpuBuffers[GRB_ENV_PROBES]->GetBuffer(frameIndex), TShaderDataOffset<EnvProbeShaderData>(0));

    frame->renderQueue << InsertBarrier(jd.hitsBufferGpu, RS_UNORDERED_ACCESS);
    frame->renderQueue << TraceRays(Vec3u { uint32(rays.Size()), 1, 1 });
    frame->renderQueue << InsertBarrier(jd.hitsBufferGpu, RS_UNORDERED_ACCESS);
}

#pragma endregion LightmapRenderer_GpuPathTracing

} // namespace Baking

} // namespace Hyperion
