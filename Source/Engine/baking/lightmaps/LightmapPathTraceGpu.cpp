#include <HyperionPch.hpp>

#include <baking/lightmaps/LightmapPathTraceGpu.hpp>

#include <baking/LightmapTexel.hpp>

#include <rendering/AccelerationStructure.hpp>
#include <rendering/RayTracingPipeline.hpp>

#include <rendering/RenderInterface.hpp>
#include <rendering/RenderHelpers.hpp>
#include <rendering/RendererMain.hpp>
#include <rendering/RenderProxyList.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/RenderCommand.hpp>
#include <rendering/RenderTypes.hpp>
#include <rendering/RenderConfig.hpp>
#include <rendering/Device.hpp>
#include <rendering/Frame.hpp>
#include <rendering/ShaderManager.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/MaterialInstance.hpp>
#include <rendering/Texture.hpp>
#include <rendering/Pass.hpp>
#include <rendering/DescriptorSet.hpp>
#include <rendering/ShaderInstance.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/TextureViewCache.hpp>
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

#include <rendering/util/MeshBuilder.hpp>

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

    m_jobData.Clear();
}

void LightmapRenderer_GpuPathTracing::CreateBuffers(BakeJobBase* job)
{
    JobData& jd = m_jobData[job];
    Assert(!jd.isCreated);

    AssertDebug(jd.raysBuffer == nullptr);

    jd.raysBuffer = RI.MakeGpuBuffer(GpuBufferType::StructuredBuffer, sizeof(Vec4f) * 2 * m_maxTexelsPerFrame, alignof(Vec4f));
    jd.raysBuffer->SetIsCpuAccessible(true);
    CheckResult(jd.raysBuffer->Create());

    jd.hitsBufferGpu = RWStructuredBuffer(m_maxTexelsPerFrame, sizeof(LightmapHit));
    jd.hitsBufferGpu.Initialize();

    jd.cbuffer = RI.MakeGpuBuffer(GpuBufferType::ConstantBuffer, 8192, 256);
    CheckResult(jd.cbuffer->Create());
}

void LightmapRenderer_GpuPathTracing::Create()
{
    m_readyNotification->Signal();
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

    //m_jobData.Erase(jobDataIt);
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
        m_tlas = RI.MakeTLAS();
    }
    else if (m_tlas->IsCreated())
    {
        return; // already created
    }

    bool hasBlas = false;

    const Handle<View>& view = m_baker->GetView();
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
        HYP_LOG(Lightmap, Warning, "No blas; cannot create tlas");
        return;
    }

    CheckResult(m_tlas->Create());
}

void LightmapRenderer_GpuPathTracing::UpdatePipelineState(Frame* frame, BakeJobBase* job)
{
    Assert(m_baker != nullptr);

    JobData& jd = m_jobData[job];

    if (jd.isCreated)
    {
        return;
    }

    CreateBuffers(job);

    jd.isCreated = true;
}

void LightmapRenderer_GpuPathTracing::ReadHitsBuffer(
    Frame* frame,
    BakeJobBase* job,
    size_t count,
    Proc<void(Span<LightmapHit> hits)>&& callback)
{
    if (count == 0)
    {
        callback({});
        return;
    }

    Assert(m_jobData.Contains(job));

    JobData& jd = m_jobData[job];
    Assert(jd.isCreated);

    RWStructuredBuffer& hitsBuffer = jd.hitsBufferGpu;

    if (!hitsBuffer.cpuBuffer.Size())
    {
        callback({});
        return;
    }

    GpuBufferRef readbackBuffer = RI.MakeGpuBuffer(GpuBufferType::ReadbackBuffer, count * sizeof(LightmapHit));
    readbackBuffer->SetIsCpuAccessible(true);
    CheckResult(readbackBuffer->Create());

    struct ReadbackHitsDataPayload
    {
        GpuBuffer* buffer;
        Proc<void(Span<LightmapHit> hits)> callback;
    };

    class ReadbackHitsDataCmd : public CmdBase
    {
    public:
        ReadbackHitsDataPayload* payload;

        explicit ReadbackHitsDataCmd(ReadbackHitsDataPayload* payload)
            : payload(payload)
        {
        }

        static void InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
        {
            ReadbackHitsDataCmd* cmdCasted = static_cast<ReadbackHitsDataCmd*>(cmd);

            ReadbackHitsDataPayload& payload = *cmdCasted->payload;

            GpuBuffer* buffer = payload.buffer;
            auto& callback = payload.callback;

            RI.GetCurrentFrame()->OnFrameEnd.Bind([&payload, buffer, cb = std::move(callback)](Frame*)
                {
                    Span<LightmapHit> hits;
                    hits.first = reinterpret_cast<LightmapHit*>(buffer->Map());
                    hits.last = hits.first + (buffer->Size() / sizeof(LightmapHit));

                    cb(hits);

                    buffer->Release();

                    delete &payload;
                })
                .Detach();
        }
    };

    CommandRecorder& cr = RI.commandRecorderAllocator.GetCommandRecorder();

    cr << InsertBarrier(hitsBuffer.gpuBuffer, RS_COPY_SRC);
    cr << InsertBarrier(readbackBuffer, RS_COPY_DST);

    cr << CopyBuffer(hitsBuffer.gpuBuffer, readbackBuffer, uint32(count * sizeof(LightmapHit)));

    ReadbackHitsDataPayload* payload = new ReadbackHitsDataPayload;
    payload->buffer = readbackBuffer.Get();
    payload->buffer->AddRef();

    payload->callback = std::move(callback);

    cr << ReadbackHitsDataCmd(payload);

    cr.Done();
}

void LightmapRenderer_GpuPathTracing::Render(Frame* frame, const RenderSetup& renderSetup, BakeJobBase* job, Span<const LightmapRay> rays, uint32 rayOffset)
{
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

    if (!m_tlas || !m_tlas->IsCreated())
    {
        // no GpuBlas to process if TLAS not created
        HYP_LOG(Lightmap, Error, "No top level acceleration structure created, cannot bake lightmap");
        return;
    }

    UpdatePipelineState(frame, job);

    JobData& jd = m_jobData[job];

    GpuBuffer* cbuffer = jd.cbuffer;
    Assert(cbuffer != nullptr);

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
                && envProbe != m_baker->GetSource()) // we don't want to bind a probe if it is being baked!
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
            && renderSetup.envProbe != m_baker->GetSource()
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

        ubyte* cbufferPtr = reinterpret_cast<ubyte*>(cbuffer->Map());
        size_t cbufferWriteOffset = 0;
        const size_t cbufferSize = cbuffer->Size();

        AssertDebug(cbufferWriteOffset + sizeof(RayTracingConstants) <= cbufferSize);

        Memory::Copy(cbufferPtr, &constants, sizeof(RayTracingConstants));
        cbufferWriteOffset += sizeof(RayTracingConstants);

        for (uint32 i = 0; i < LightmapVolumeMaxBoundLights; i++)
        {
            if (i < uint32(tempLights.Size()))
            {
                AssertDebug(cbufferWriteOffset + sizeof(LightShaderData) <= cbufferSize);

                Memory::Copy(cbufferPtr + cbufferWriteOffset, tempLights[i].second, sizeof(LightShaderData));
                cbufferWriteOffset += sizeof(LightShaderData);

                continue;
            }

            LightShaderData dummy {};

            AssertDebug(cbufferWriteOffset + sizeof(LightShaderData) <= cbufferSize);

            Memory::Copy(cbufferPtr + cbufferWriteOffset, &dummy, sizeof(LightShaderData));
            cbufferWriteOffset += sizeof(LightShaderData);
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

            AssertDebug(cbufferWriteOffset + sizeof(EnvProbeShaderData) <= cbufferSize);

            Memory::Copy(cbufferPtr + cbufferWriteOffset, pEnvProbeShaderData, sizeof(EnvProbeShaderData));
            cbufferWriteOffset += sizeof(EnvProbeShaderData);
        }

        cbuffer->Flush(0, cbufferWriteOffset);
    }

    Assert(m_tlas && m_tlas->IsCreated());

    { // rays buffer
        GpuBufferRef& raysBuffer = jd.raysBuffer;
        Assert(raysBuffer != nullptr && raysBuffer->IsCreated());

        Array<Vec4f, DynamicAllocator> rayData;
        rayData.Resize(rays.Size() * 2);

        for (size_t i = 0; i < rays.Size(); i++)
        {
            rayData[i * 2] = Vec4f(rays[i].ray.position, 1.0f);
            rayData[i * 2 + 1] = Vec4f(rays[i].ray.direction, 0.0f);
        }

        Assert(raysBuffer->Size() >= rayData.ByteSize());
        raysBuffer->Copy(rayData.ByteSize(), rayData.Data());
        raysBuffer->Flush(0, rayData.ByteSize());
    }

    CommandRecorder& cr = RI.commandRecorderAllocator.GetCommandRecorder();

    cr << SetCurrentShader(GetShaderDesc(m_shadingType));

    cr << SetShaderUniform(0, "TLAS"_sh, m_tlas);
    cr << SetShaderUniform(1, "MeshDescriptionsBuffer"_sh, m_tlas->GetMeshDescriptionsBuffer(), ShaderDataOffset(0, sizeof(MeshDescription)));
    cr << SetShaderUniform(2, "HitsBuffer"_sh, jd.hitsBufferGpu.gpuBuffer, ShaderDataOffset(0, sizeof(Vec4f)));
    cr << SetShaderUniform(3, "RaysBuffer"_sh, jd.raysBuffer, ShaderDataOffset(0, sizeof(Vec4f)));
    cr << SetShaderUniform(5, "MaterialsBuffer"_sh, RI.namedBuffers[NamedBuffer::Materials]);
    cr << SetShaderUniform(6, "CBuffer"_sh, cbuffer);

    cr << SetShaderUniform(7, "SamplerNearest"_sh, RI.placeholderData->GetSamplerNearest());
    cr << SetShaderUniform(8, "SamplerLinear"_sh, RI.placeholderData->GetSamplerLinear());

    cr << SetShaderUniform(9, "BlueNoiseBuffer"_sh, RI.blueNoiseBuffer);

    cr << SetShaderUniform(10, "WorldsBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);
    cr << SetShaderUniform(11, "EntitiesBuffer"_sh, RI.namedBuffers[NamedBuffer::Entities]);

    cr << SetShaderUniform(12, "EnvProbesTexture"_sh, RI.textureViewCache->GetOrCreate(RI.envProbesTexture));

    Assert(jd.hitsBufferGpu.gpuBuffer->Size() >= rays.Size() * sizeof(LightmapHit));
    Assert(jd.raysBuffer->Size() >= rays.Size() * 2 * sizeof(Vec4f));

    cr << InsertBarrier(jd.hitsBufferGpu.gpuBuffer, RS_UNORDERED_ACCESS);
    cr << TraceRays(Vec3u { uint32(rays.Size()), 1, 1 });

    cr.Done();
}

#pragma endregion LightmapRenderer_GpuPathTracing

} // namespace Baking

} // namespace Hyperion
