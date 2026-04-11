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
#include <rendering/ShaderInstance.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/TextureViewCache.hpp>
#include <rendering/CBufferAllocator.hpp>
#include <rendering/Buffers.hpp>

#include <rendering/util/DeletionQueue.hpp>
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

#include <Core/threading/TaskSystem.hpp>
#include <Core/threading/TaskThread.hpp>
#include <Core/threading/ThreadSignal.hpp>

#include <Core/utilities/Time.hpp>
#include <Core/utilities/DeferredScope.hpp>
#include <Core/utilities/Float16.hpp>

#include <Core/math/Triangle.hpp>

#include <util/MeshBuilder.hpp>

#include <system/AppContext.hpp>

#include <engine/EngineDriver.hpp>
#include <engine/EngineGlobals.hpp>

namespace Hyperion {

struct GpuLightmapperReadyNotification : ThreadSignal
{
};

static constexpr uint32 LightmapVolumeMaxBoundLights = 16;
static constexpr uint32 LightmapVolumeMaxBoundEnvProbes = 4;

static const ShaderPropertyId s_propMaxLights = InternShaderProperty(ShaderProperty(NAME("MAX_LIGHTS"), int(LightmapVolumeMaxBoundLights)));
static const ShaderPropertyId s_propMaxEnvProbes = InternShaderProperty(ShaderProperty(NAME("MAX_ENV_PROBES"), int(LightmapVolumeMaxBoundEnvProbes)));

#pragma region Render commands

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
        notification->Signal();

        return {};
    }
};

#pragma endregion Render commands

namespace Baking {

#pragma region LightmapRenderer_GpuPathTracing

static const ShaderPropertyId s_lightmapModeProperties[uint32(LightmapShadingType::MAX)] = {
    InternShaderProperty(ShaderProperty(NAME("MODE"), NAME("IRRADIANCE"))),
    InternShaderProperty(ShaderProperty(NAME("MODE"), NAME("RADIANCE"))),
    InternShaderProperty(ShaderProperty(NAME("MODE"), NAME("FULL"))),
    InternShaderProperty(ShaderProperty(NAME("MODE"), NAME("SHADOW")))
};

static ShaderDesc GetShaderDesc(LightmapShadingType shadingType)
{
    ShaderPropertySet shaderProperties;
    shaderProperties.Add(s_propMaxLights);
    shaderProperties.Add(s_propMaxEnvProbes);
    shaderProperties.Add(s_lightmapModeProperties[uint32(shadingType)]);

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
    EnqueueDeletion(std::move(m_tlas));

    for (KeyValuePair<BakeJobBase*, JobData>& it : m_jobData)
    {
        EnqueueDeletion(std::move(it.second.raysBuffer));
        EnqueueDeletion(std::move(it.second.hitsBufferGpu));
    }
}

void LightmapRenderer_GpuPathTracing::CreateBuffers(BakeJobBase* job)
{
    JobData& jd = m_jobData[job];

    jd.raysBuffer = g_renderInterface->MakeGpuBuffer(GpuBufferType::STORAGE_BUFFER, sizeof(Vec4f) * 2 * m_maxTexelsPerFrame, alignof(Vec4f));
    jd.raysBuffer->SetIsCpuAccessible(true);

    // READBACK_BUFFER type allows readback to cpu.
    jd.hitsBufferGpu = g_renderInterface->MakeGpuBuffer(GpuBufferType::READBACK_BUFFER, sizeof(LightmapHit) * m_maxTexelsPerFrame, alignof(Vec4f));

    CheckResult(jd.hitsBufferGpu->Create());
    CheckResult(jd.raysBuffer->Create());
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
    return m_readyNotification != nullptr
        && m_readyNotification->IsSignalled();
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

        if (entity->GetScene()->GetSceneFlags() & SceneFlags::BACKDROP)
        {
            // Do NOT add entities that are part of a backdrop to the ray trace scene (for now)
            // currently we just use the SkyProbes, at some point it could be nice to include backdrop
            // scenes meshes
            continue;
        }

        RenderProxyMesh* meshProxy = rpl.GetMeshEntities().GetProxy(entity->Id());
        Assert(meshProxy != nullptr);

        AssertDebug(meshProxy->mesh != nullptr);

        GpuBlasRef blas = MeshBlasBuilder::Build(meshProxy->mesh, meshProxy->material);
        Assert(blas != nullptr);

        blas->SetTransform(meshProxy->bufferData.modelMatrix);

        if (meshProxy->material != nullptr)
        {
            const uint32 materialBinding = Resources::GetBinding(meshProxy->material);
            blas->SetMaterialBinding(materialBinding);
        }

        if (!blas->IsCreated())
        {
            CheckResult(blas->Create());
        }

        if (!m_tlas->HasGpuBlas(entity->Id().ToIndex()))
        {
            m_tlas->AddGpuBlas(entity->Id().ToIndex(), blas);

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

    singleTimeCommands->Push([&](CommandRecorder& cr)
        {
            const ResourceState previousResourceState = hitsBuffer->GetResourceState();

            cr << InsertBarrier(hitsBuffer, RS_COPY_SRC);
            cr << InsertBarrier(stagingBuffer, RS_COPY_DST);

            cr << CopyBuffer(hitsBuffer, stagingBuffer, uint32(outHits.Size() * sizeof(LightmapHit)));

            cr << InsertBarrier(stagingBuffer, RS_COPY_SRC);
            cr << InsertBarrier(hitsBuffer, previousResourceState);
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

    GpuBuffer* cbuffer = nullptr;
    size_t cbufferOffset = 0;
    size_t cbufferSize = 0;

    { // Fill constants buffer
        RenderProxyList& rpl = GetConsumerProxyList(renderSetup.view);
        rpl.BeginRead();
        HYP_DEFER({ rpl.EndRead(); });

        RayTracingConstants constants {};
        constants.rayOffset = rayOffset;
        
        Array<Pair<Light*, LightShaderData*>, RenderAllocator> tempLights;
        Array<Pair<EnvProbe*, EnvProbeShaderData*>, RenderAllocator> tempEnvProbes;

        uint32& numBoundLights = constants.numBoundLights;
        uint32& numBoundEnvProbes = constants.numBoundEnvProbes;

        for (Light* light : rpl.GetLights())
        {
            const LightType lightType = light->GetLightType();

            if (lightType != LightType::Directional && lightType != LightType::Point)
            {
                continue;
            }

            if (numBoundLights >= LightmapVolumeMaxBoundLights)
            {
                break;
            }

            RenderProxyLight* lightProxy = static_cast<RenderProxyLight*>(GetRenderProxy(light));
            Assert(lightProxy != nullptr);

            tempLights.EmplaceBack(light, &lightProxy->bufferData);

            ++numBoundLights;
        }

        for (EnvProbe* envProbe : rpl.GetEnvProbes())
        {
            if ((envProbe->IsA(SkyProbe::StaticClass()) /* || envProbe->IsA(ReflectionProbe::StaticClass()) */)
                && envProbe != m_lightmapper->GetSource()) // we don't want to bind a probe if it is being baked!
            {
                if (numBoundEnvProbes >= LightmapVolumeMaxBoundEnvProbes)
                {
                    break;
                }

                RenderProxyEnvProbe* envProbeProxy = static_cast<RenderProxyEnvProbe*>(GetRenderProxy(envProbe));
                Assert(envProbeProxy != nullptr);

                tempEnvProbes.EmplaceBack(envProbe, &envProbeProxy->bufferData);

                ++numBoundEnvProbes;
            }
        }

        if (renderSetup.envProbe != nullptr
            && renderSetup.envProbe != m_lightmapper->GetSource()
            && numBoundEnvProbes < LightmapVolumeMaxBoundEnvProbes)
        {
            auto it = tempEnvProbes.FindIf([envProbe = renderSetup.envProbe](const auto& pair)
                {
                    if (pair.first == envProbe)
                    {
                        return true;
                    }

                    return false;
                });

            if (it == tempEnvProbes.End())
            {
                RenderProxyEnvProbe* envProbeProxy = static_cast<RenderProxyEnvProbe*>(GetRenderProxy(renderSetup.envProbe));
                Assert(envProbeProxy != nullptr);

                tempEnvProbes.EmplaceBack(renderSetup.envProbe, &envProbeProxy->bufferData);

                ++numBoundEnvProbes;
            }
        }

        g_renderInterface->cbufferAllocator->Write(&constants);

        for (uint32 i = 0; i < LightmapVolumeMaxBoundLights; i++)
        {
            if (i < uint32(tempLights.Size()))
            {
                g_renderInterface->cbufferAllocator->Write(tempLights[i].second);
                continue;
            }
        
            LightShaderData dummy {};
            g_renderInterface->cbufferAllocator->Write(&dummy);
        }

        // sort so sky is last
        std::sort(tempEnvProbes.Begin(), tempEnvProbes.End(),
            [](const Pair<EnvProbe*, EnvProbeShaderData*>& a, const Pair<EnvProbe*, EnvProbeShaderData*>& b)
        {
            const bool aIsSky = a.first->IsA(SkyProbe::StaticClass());
            const bool bIsSky = b.first->IsA(SkyProbe::StaticClass());

            if (aIsSky && !bIsSky)
            {
                return false;
            }
                
            if (!aIsSky && bIsSky)
            {
                return true;
            }
                
            if (aIsSky && bIsSky)
            {
                return false;
            }

            return true;
        });

        for (uint32 i = 0; i < LightmapVolumeMaxBoundEnvProbes; i++)
        {
            const EnvProbeShaderData* pEnvProbeShaderData = nullptr;

            if (i < uint32(tempEnvProbes.Size()))
            {
                pEnvProbeShaderData = tempEnvProbes[i].second;
            }
            else
            {
                static const EnvProbeShaderData s_dummyEnvProbeShaderData;
                pEnvProbeShaderData = &s_dummyEnvProbeShaderData;
            }

            g_renderInterface->cbufferAllocator->Write(pEnvProbeShaderData);
        }

        g_renderInterface->cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);
    }

    JobData& jd = m_jobData[job];

    Assert(m_tlas && m_tlas->IsCreated());

    { // rays buffer
        Array<Vec4f, DynamicAllocator> rayData;
        rayData.Resize(rays.Size() * 2);

        for (size_t i = 0; i < rays.Size(); i++)
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

    CommandRecorder& cr = frame->cr;

    cr << SetCurrentShader(GetShaderDesc(m_shadingType));

    cr << SetShaderUniform(0, "TLAS"_sh, m_tlas);
    cr << SetShaderUniform(1, "MeshDescriptionsBuffer"_sh, m_tlas->GetMeshDescriptionsBuffer());
    cr << SetShaderUniform(2, "HitsBuffer"_sh, jd.hitsBufferGpu);
    cr << SetShaderUniform(3, "RaysBuffer"_sh, jd.raysBuffer);
    cr << SetShaderUniform(5, "MaterialsBuffer"_sh, g_renderInterface->gpuBuffers[GRB_MATERIALS]->GetBuffer(frameIndex));
    cr << SetShaderUniform(6, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));
    
    cr << SetShaderUniform(7, "SamplerNearest"_sh, g_renderInterface->placeholderData->GetSamplerNearest());
    cr << SetShaderUniform(8, "SamplerLinear"_sh, g_renderInterface->placeholderData->GetSamplerLinear());

    cr << SetShaderUniform(9, "BlueNoiseBuffer"_sh, g_renderInterface->blueNoiseBuffer);

    cr << SetShaderUniform(10, "WorldsBuffer"_sh, g_renderInterface->gpuBuffers[GRB_WORLDS]->GetBuffer(frameIndex));
    cr << SetShaderUniform(11, "EntitiesBuffer"_sh, g_renderInterface->gpuBuffers[GRB_ENTITIES]->GetBuffer(frameIndex));
    
    cr << SetShaderUniform(12, "EnvProbesTexture"_sh, g_renderInterface->textureViewCache->GetOrCreate(g_renderInterface->envProbesTexture));

    frame->cr << InsertBarrier(jd.hitsBufferGpu, RS_UNORDERED_ACCESS);
    frame->cr << TraceRays(Vec3u { uint32(rays.Size()), 1, 1 });
    frame->cr << InsertBarrier(jd.hitsBufferGpu, RS_UNORDERED_ACCESS);
}

#pragma endregion LightmapRenderer_GpuPathTracing

} // namespace Baking

} // namespace Hyperion
