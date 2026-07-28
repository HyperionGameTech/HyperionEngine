#include <HyperionPch.hpp>

#include <Baking/Lightmaps/LightmapPathTraceGpu.hpp>

#include <Baking/LightmapTexel.hpp>

#include <Rendering/AccelerationStructure.hpp>
#include <Rendering/RayTracingPipeline.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/RenderHelpers.hpp>
#include <Rendering/RendererMain.hpp>
#include <Rendering/RenderProxyList.hpp>
#include <Rendering/RenderProxy.hpp>
#include <Rendering/RenderCommand.hpp>
#include <Rendering/RenderTypes.hpp>
#include <Rendering/RenderConfig.hpp>
#include <Rendering/Device.hpp>
#include <Rendering/Frame.hpp>
#include <Rendering/ShaderManager.hpp>
#include <Rendering/Mesh.hpp>
#include <Rendering/Material.hpp>
#include <Rendering/Texture.hpp>
#include <Rendering/Pass.hpp>
#include <Rendering/DescriptorSet.hpp>
#include <Rendering/ShaderInstance.hpp>
#include <Rendering/PlaceholderData.hpp>
#include <Rendering/TextureViewCache.hpp>
#include <Rendering/Buffers.hpp>

#include <Rendering/Util/DeletionQueue.hpp>
#include <Rendering/Util/ShaderCompiler.hpp>

#include <Rendering/BLASBuilder.hpp>

#include <Scene/World.hpp>
#include <Scene/EnvProbe.hpp>
#include <Scene/Light.hpp>
#include <Scene/ProbeVolume.hpp>
#include <Scene/View.hpp>
#include <Scene/EntityManager.hpp>
#include <Scene/LightmapVolume.hpp>

#include <Scene/Components/MeshComponent.hpp>
#include <Scene/Components/TransformComponent.hpp>

#include <Scene/Util/VoxelOctree.hpp>

#include <Scene/Camera/Camera.hpp>
#include <Scene/Camera/OrthoCamera.hpp>

#include <Core/Threading/TaskSystem.hpp>
#include <Core/Threading/TaskThread.hpp>
#include <Core/Threading/ThreadSignal.hpp>

#include <Core/Utilities/Time.hpp>
#include <Core/Utilities/DeferredScope.hpp>
#include <Core/Utilities/Float16.hpp>

#include <Core/Math/Triangle.hpp>

#include <Rendering/Util/MeshBuilder.hpp>

#include <System/AppContext.hpp>

#include <Framework/EngineDriver.hpp>
#include <Framework/EngineGlobals.hpp>

namespace Hyperion {

struct GpuLightmapperReadyNotification : ThreadSignal
{
};

static constexpr uint32 LightmapVolumeMaxBoundLights = 16;
static constexpr uint32 LightmapVolumeMaxBoundEnvProbes = 4;

static const ShaderPropertyId s_propMaxLights = InternShaderProperty(ShaderProperty(NAME("MAX_LIGHTS"), int(LightmapVolumeMaxBoundLights)));
static const ShaderPropertyId s_propMaxEnvProbes = InternShaderProperty(ShaderProperty(NAME("MAX_ENV_PROBES"), int(LightmapVolumeMaxBoundEnvProbes)));

namespace Baking {

#pragma region PathTracer

static const ShaderPropertyId s_lightmapModeProperties[uint32(LightmapShadingType::MAX)] = {
    InternShaderProperty(ShaderProperty(NAME("MODE"), NAME("IRRADIANCE"))),
    InternShaderProperty(ShaderProperty(NAME("MODE"), NAME("FULL"))),
    InternShaderProperty(ShaderProperty(NAME("MODE"), NAME("SHADOW"))),
    InternShaderProperty(ShaderProperty(NAME("MODE"), NAME("DISTANCE"))),
    InternShaderProperty(ShaderProperty(NAME("MODE"), NAME("BENT_NORMAL")))
};

static ShaderDesc GetShaderDesc(LightmapShadingType shadingType)
{
    ShaderPropertySet shaderProperties;
    shaderProperties.Add(s_propMaxLights);
    shaderProperties.Add(s_propMaxEnvProbes);
    shaderProperties.Add(s_lightmapModeProperties[uint32(shadingType)]);

    return ShaderDesc(NAME("LightmapPathTracer"), shaderProperties);
}

PathTracer::PathTracer(
    BakerBase* baker,
    const Handle<Scene>& scene,
    LightmapShadingType shadingType,
    uint32 maxTexelsPerFrame)
    : m_baker(baker),
      m_scene(scene),
      m_shadingType(shadingType),
      m_maxTexelsPerFrame(maxTexelsPerFrame)
{
    Assert(m_baker != nullptr);
    m_readyNotification = MakeShared<GpuLightmapperReadyNotification>();
}

PathTracer::~PathTracer()
{
    EnqueueDeletion(std::move(m_tlas));

    m_jobData.Clear();
}

void PathTracer::CreateBuffers(BakeJobBase* job)
{
    JobData& jd = m_jobData[job];
    Assert(!jd.isCreated);

    AssertDebug(jd.raysBuffer == nullptr);

    jd.raysBuffer = RI.MakeGpuBuffer(GpuBufferType::StructuredBuffer, sizeof(Vec4f) * 2 * m_maxTexelsPerFrame, alignof(Vec4f));
    jd.raysBuffer->SetIsCpuAccessible(true);
    Check(jd.raysBuffer->Create());

    jd.hitsBufferGpu = RWStructuredBuffer(m_maxTexelsPerFrame, sizeof(LightmapHit));
    jd.hitsBufferGpu.Initialize();

    jd.cbuffer = RI.MakeGpuBuffer(GpuBufferType::ConstantBuffer, 8192, 256);
    Check(jd.cbuffer->Create());
}

void PathTracer::Create()
{
    m_readyNotification->Signal();
}

void PathTracer::CleanJobData(BakeJobBase* job)
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

    // @NOTE this was commented out due to a gross crash, we need to re-enable it,
    // but with proper care
    // m_jobData.Erase(jobDataIt);
}

bool PathTracer::CanRender() const
{
    return m_readyNotification != nullptr
        && m_readyNotification->IsSignalled();
}

void PathTracer::CreateAccelerationStructures()
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

    RenderProxyList& rpl = *view->GetRenderProxyList(GetRingIndex());
    AssertDebug(rpl.isShared);

    rpl.BeginRead();
    HYP_DEFER({ rpl.EndRead(); });

    AssertDebug(rpl.GetMeshEntities().NumCurrent() != 0);

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

        BottomLevelASRef blas = BLASBuilder::Build(meshProxy->mesh, meshProxy->material);
        Assert(blas != nullptr);

        blas->SetTransform(meshProxy->bufferData.modelMatrix);

        if (meshProxy->material != nullptr)
        {
            const uint32 materialBinding = Resources::GetBinding(meshProxy->material);
            blas->SetMaterialBinding(materialBinding);
        }

        if (!blas->IsCreated())
        {
            Check(blas->Create());
        }

        const uint64 key = entity->Id().GetHashCode().Value();

        if (!m_tlas->ContainsBLAS(key))
        {
            m_tlas->AddBLAS(key, blas);
        }

        hasBlas = true;
    }

    if (!hasBlas)
    {
        HYP_LOG(Lightmap, Warning, "No bottom-level acceleration structures found. Skipping top-level acceleration structure creation.");

        HYP_BREAKPOINT;

        return;
    }

    Check(m_tlas->Create());
}

void PathTracer::UpdatePipelineState(Frame* frame, BakeJobBase* job)
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

void PathTracer::ReadHitsBuffer(
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

    if (!m_jobData.Contains(job))
    {
        HYP_LOG(Lightmap, Warning, "Job data missing");

        callback({});

        return;
    }

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
    Check(readbackBuffer->Create());

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

            RI.GetCurrentFrame()->OnFrameEnd.Bind(
                                                [&payload, buffer, cb = std::move(callback)](Frame*)
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

bool PathTracer::Render(Frame* frame, const RenderSetup& renderSetup, BakeJobBase* job, Span<const LightmapRay> rays, uint32 rayOffset)
{
    AssertOnThread(g_renderThread);

    if (rays.Size() == 0)
    {
        return false;
    }

    Assert(CanRender());

    AssertDebug(renderSetup.world);

    RenderProxyList& rpl = *renderSetup.view->GetRenderProxyList(GetRingIndex()); // GetConsumerProxyList(renderSetup.view);
    rpl.BeginRead();
    HYP_DEFER({ rpl.EndRead(); });

    AssertDebug(rpl.isShared);

    CreateAccelerationStructures();

    if (!m_tlas || !m_tlas->IsCreated())
    {
        // no BottomLevelAS to process if TLAS not created
        HYP_LOG(Lightmap, Error, "No top level acceleration structure created, cannot bake lightmap");
        return false;
    }

    UpdatePipelineState(frame, job);

    JobData& jd = m_jobData[job];

    GpuBuffer* cbuffer = jd.cbuffer;
    Assert(cbuffer != nullptr);

    { // Fill constants buffer

        RayTracingConstants constants {};
        constants.rayOffset = rayOffset;

        Array<Pair<Light*, LightShaderData*>, RenderTempAllocator> tempLights;
        Array<Pair<EnvProbe*, EnvProbeShaderData*>, RenderTempAllocator> tempEnvProbes;

        uint32& numBoundLights = constants.numBoundLights;
        uint32& numBoundEnvProbes = constants.numBoundEnvProbes;

        for (Light* light : rpl.GetLights())
        {
            const LightType lightType = light->GetLightType();

            if (lightType != LightType::Directional && lightType != LightType::Point && lightType != LightType::Spot)
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
            const bool contributesDiffuseLighting = (envProbe->IsAmbientProbe() || (envProbe->GetEnvProbeFlags() & EPF_DIFFUSE))
                && envProbe->GetDiffuseStrength() > 0.0f;

            if (envProbe != m_baker->GetSource() && contributesDiffuseLighting) // we don't want to bind a probe if it is being baked!
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
            auto it = tempEnvProbes.FindIf(
                [envProbe = renderSetup.envProbe](const auto& pair)
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
                  [&](const Pair<EnvProbe*, EnvProbeShaderData*>& a, const Pair<EnvProbe*, EnvProbeShaderData*>& b)
                  {
                      const bool aIsSky = a.first->IsA(SkyProbe::StaticClass());
                      const bool bIsSky = b.first->IsA(SkyProbe::StaticClass());

                      if (aIsSky ^ bIsSky)
                      {
                          return !aIsSky;
                      }
                      
                      const Vec3f aProbePosition = a.second->worldPosition.GetXYZ();
                      const Vec3f bProbePosition = b.second->worldPosition.GetXYZ();

                      const Vec3f center = m_baker->GetView()->cachedBounds.GetCenter();

                      const float aDistSq = (aProbePosition - center).LengthSquared();
                      const float bDistSq = (bProbePosition - center).LengthSquared();

                      if (aDistSq == bDistSq)
                      {
                          return a.first->Id() < b.first->Id();
                      }

                      return aDistSq < bDistSq;
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
                static const EnvProbeShaderData s_dummyEnvProbeShaderData {};
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

        // Note: don't use arena allocator, won't be able to allocate enough memory.
        // What we could do, is preallocate this for all the frames to reuse.
        Array<Vec4f> rayData;
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
    cr << SetShaderUniform(1, "MeshDescriptionsBuffer"_sh, m_tlas->GetMeshDescriptionsBuffer());
    cr << SetShaderUniform(2, "HitsBuffer"_sh, jd.hitsBufferGpu.gpuBuffer, ShaderDataOffset(0, sizeof(Vec4f)));
    cr << SetShaderUniform(3, "RaysBuffer"_sh, jd.raysBuffer, ShaderDataOffset(0, sizeof(Vec4f)));
    cr << SetShaderUniform(5, "MaterialsBuffer"_sh, RI.namedBuffers[NamedBuffer::Materials]);
    cr << SetShaderUniform(6, "CBuffer"_sh, cbuffer);

    cr << SetShaderUniform(7, "SamplerNearest"_sh, RI.placeholderData->GetSamplerNearest());
    cr << SetShaderUniform(8, "SamplerLinear"_sh, RI.placeholderData->GetSamplerLinear());

    cr << SetShaderUniform(9, "BlueNoiseBuffer"_sh, RI.blueNoiseBuffer);

    cr << SetShaderUniform(10, "WorldsBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);
    cr << SetShaderUniform(11, "EntitiesBuffer"_sh, RI.namedBuffers[NamedBuffer::Entities]);

    cr << SetShaderUniform(12, "EnvProbesColorTexture"_sh, RI.textureViewCache->GetOrCreate(RI.envProbesColorTexture));
    cr << SetShaderUniform(13, "EnvProbesDepthTexture"_sh, RI.textureViewCache->GetOrCreate(RI.envProbesDepthTexture));

    Assert(jd.hitsBufferGpu.gpuBuffer->Size() >= rays.Size() * sizeof(LightmapHit));
    Assert(jd.raysBuffer->Size() >= rays.Size() * 2 * sizeof(Vec4f));

    cr << InsertBarrier(jd.hitsBufferGpu.gpuBuffer, RS_UNORDERED_ACCESS);
    cr << TraceRays(Vec3u { uint32(rays.Size()), 1, 1 });

    cr.Done();

    return true;
}

#pragma endregion PathTracer

} // namespace Baking

} // namespace Hyperion
