#include <HyperionPch.hpp>

#include <lightmapper/LightmapPathTraceCpu.hpp>
#include <lightmapper/LightmapAccelerationStructure.hpp>
#include <lightmapper/LightmapVolume.hpp>

#include <rendering/RenderInterface.hpp>
#include <rendering/RenderHelpers.hpp>
#include <rendering/RenderCollection.hpp>
#include <rendering/RenderBackend.hpp>
#include <rendering/RenderObject.hpp>
#include <rendering/RenderConfig.hpp>
#include <rendering/Device.hpp>
#include <rendering/Frame.hpp>
#include <rendering/RenderProxyList.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/Material.hpp>
#include <rendering/Texture.hpp>
#include <rendering/RendererBase.hpp>

#include <asset/TextureAsset.hpp>

#include <scene/BVH.hpp>
#include <scene/World.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/Light.hpp>
#include <scene/EnvGrid.hpp>
#include <scene/View.hpp>

#include <scene/EntityManager.hpp>
#include <scene/components/MeshComponent.hpp>
#include <scene/components/TransformComponent.hpp>

#include <scene/util/VoxelOctree.hpp>

#include <scene/camera/Camera.hpp>
#include <scene/camera/OrthoCamera.hpp>

#include <core/config/Config.hpp>

#include <core/threading/TaskSystem.hpp>
#include <core/threading/TaskThread.hpp>

#include <core/utilities/Time.hpp>
#include <core/utilities/DeferredScope.hpp>
#include <core/utilities/Float16.hpp>

#include <core/math/Triangle.hpp>

#include <util/MeshBuilder.hpp>

#include <system/AppContext.hpp>

#include <engine/EngineDriver.hpp>

namespace Hyperion {

namespace CoreApi {
extern const GlobalConfig& GetGlobalConfig();
} // namespace CoreApi

static constexpr uint32 MaxBouncesCpu = 4;

#pragma region LightmapThreadPool

uint32 LightmapThreadPool::NumThreadsToCreate()
{
    uint32 numThreads = CoreApi::GetGlobalConfig().Get("Lightmapper.NumThreadsPerJob").ToUInt32(4);
    return MathUtil::Clamp(numThreads, 1u, NumCores());
}

#pragma endregion LightmapThreadPool

#pragma region LightmapRenderer_CpuPathTracing

LightmapRenderer_CpuPathTracing::LightmapRenderer_CpuPathTracing(
    LightmapperBase* lightmapper,
    LightmapTopLevelAccelerationStructure* accelerationStructure,
    LightmapThreadPool* threadPool,
    const Handle<Scene>& scene,
    LightmapShadingType shadingType)
    : ILightmapRenderer(lightmapper),
      m_accelerationStructure(accelerationStructure),
      m_threadPool(threadPool),
      m_scene(scene),
      m_shadingType(shadingType)
{
    AssertDebug(accelerationStructure != nullptr);
    AssertDebug(threadPool != nullptr);
}

LightmapRenderer_CpuPathTracing::~LightmapRenderer_CpuPathTracing()
{
}

void LightmapRenderer_CpuPathTracing::Create()
{
}

void LightmapRenderer_CpuPathTracing::CleanJobData(LightmapJobBase* job)
{
    auto it = m_jobData.Find(job);

    if (it == m_jobData.End())
    {
        return;
    }

    Assert(AtomicAdd(&it->second.numTracingTasks, 0) == 0,
        "Cannot clean job data while tracing is in progress");

    m_jobData.Erase(it);
}

void LightmapRenderer_CpuPathTracing::ReadHitsBuffer(Frame* frame, LightmapJobBase* job, Span<LightmapHit> outHits)
{
    AssertOnThread(g_renderThread);

    auto it = m_jobData.Find(job);

    if (it == m_jobData.End())
    {
        return;
    }

    JobData& jobData = it->second;

    Assert(AtomicAdd(&jobData.numTracingTasks, 0) == 0,
        "Cannot read hits buffer while tracing is in progress");

    Assert(outHits.Size() == jobData.hitsBuffer.Size());

    Memory::MemCpy(outHits.Data(), jobData.hitsBuffer.Data(), jobData.hitsBuffer.ByteSize());
}

Vec3f LightmapRenderer_CpuPathTracing::EvaluateDiffuseLighting(LightmapJobBase* job, Light* light, const LightShaderData& bufferData, const Vec3f& albedo, const Vec3f& position, const Vec3f& normal)
{
    Assert(light != nullptr);

    switch (light->GetLightType())
    {
    case LT_DIRECTIONAL:
    {
        // return (ByteUtil::UnpackVec4f(SwapEndian(bufferData.colorPacked)) * MathUtil::Max(0.0f, normal.Dot(bufferData.positionIntensity.GetXYZ().Normalized())) * bufferData.positionIntensity.w).GetXYZ();
        const Vec3f wi = -bufferData.positionIntensity.GetXYZ().Normalized();
        const float NoL = MathUtil::Max(0.0f, normal.Dot(wi));
        if (NoL <= 0.0f)
        {
            return Vec3f(0.0f);
        }

        const float shadow = TraceShadowRay(job, position, normal, wi);
        if (MathUtil::ApproxEqual(shadow, 0.0f))
        {
            // skip
            return Vec3f(0.0f);
        }

        // Lambert BRDF with delta light sampling (pdf = 1)
        const Vec3f f = albedo * (1.0f / MathUtil::pi<float>);
        const Vec3f Li = bufferData.color.GetXYZ() * bufferData.positionIntensity.w;

        return f * Li * NoL;
    }
    case LT_POINT:
    {
        const float radius = Float16::FromRaw(bufferData.radiusFalloffPacked & 0xFFFFu);

        Vec3f lightDir = (bufferData.positionIntensity.GetXYZ() - position).Normalized();
        float dist = (bufferData.positionIntensity.GetXYZ() - position).Length();
        float distSqr = dist * dist;

        float invRadius = 1.0f / radius;
        float factor = distSqr * (invRadius * invRadius);
        float smoothFactor = MathUtil::Max(1.0f - (factor * factor), 0.0f);

        return (bufferData.color * ((smoothFactor * smoothFactor) / MathUtil::Max(distSqr, 1e4f)) * bufferData.positionIntensity.w).GetXYZ();
    }
    default:
        // Not implemented
        return Vec3f::Zero();
    }
}

LightmapRenderer_CpuPathTracing::SharedCpuData* LightmapRenderer_CpuPathTracing::CreateSharedCpuData(RenderProxyList& rpl)
{
    rpl.BeginRead();

    SharedCpuData* sharedCpuData = new SharedCpuData();

    for (Light* light : rpl.GetLights())
    {
        RenderProxyLight* lightProxy = static_cast<RenderProxyLight*>(RenderApi::GetRenderProxy(light));

        if (lightProxy)
        {
            sharedCpuData->lightData[light] = lightProxy->bufferData;
        }
    }

    for (EnvProbe* envProbe : rpl.GetEnvProbes().GetElements<SkyProbe>())
    {
        RenderProxyEnvProbe* envProbeProxy = static_cast<RenderProxyEnvProbe*>(RenderApi::GetRenderProxy(envProbe));

        if (envProbeProxy)
        {
            sharedCpuData->envProbeData[envProbe] = envProbeProxy->bufferData;
        }
    }

    rpl.EndRead();

    return sharedCpuData;
}

void LightmapRenderer_CpuPathTracing::Render(Frame* frame, const RenderSetup& renderSetup, LightmapJobBase* job, Span<const LightmapRay> rays, uint32 rayOffset)
{
    AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.world && renderSetup.view);

    RenderProxyList& rpl = RenderApi::GetConsumerProxyList(renderSetup.view);

    SharedCpuData* sharedCpuData = CreateSharedCpuData(rpl);

    JobData& jobData = m_jobData[job];

    Assert(AtomicAdd(&jobData.numTracingTasks, 0) == 0,
        "Trace is already in progress");

    Handle<Texture> envProbeTexture;

    if (renderSetup.envProbe)
    {
        // prepare env probe texture to be sampled on the CPU in the tasks
        envProbeTexture = renderSetup.envProbe->GetPrefilteredEnvMap();
    }

    jobData.hitsBuffer.Resize(rays.Size());

    jobData.currentRays.Resize(rays.Size());
    Memory::MemCpy(jobData.currentRays.Data(), rays.Data(), jobData.currentRays.ByteSize());

    AtomicAdd(&jobData.numTracingTasks, rays.Size());

    TaskBatch* taskBatch = new TaskBatch();
    taskBatch->pool = m_threadPool;

    const uint32 numItems = uint32(jobData.currentRays.Size());
    const uint32 numBatches = m_threadPool->GetProcessorAffinity();
    const uint32 itemsPerBatch = (numItems + numBatches - 1) / numBatches;

    for (uint32 batchIndex = 0; batchIndex < numBatches; batchIndex++)
    {
        taskBatch->AddTask([this, view = renderSetup.view, sharedCpuData, jobDataPtr = &jobData, job, batchIndex, itemsPerBatch, numItems, envProbeTexture](...)
            {
                uint32 seed = std::rand();

                const uint32 offsetIndex = batchIndex * itemsPerBatch;
                const uint32 maxIndex = MathUtil::Min(offsetIndex + itemsPerBatch, numItems);

                for (uint32 index = offsetIndex; index < maxIndex; index++)
                {
                    HYP_DEFER({ AtomicDecrement(&jobDataPtr->numTracingTasks); });

                    const LightmapRay& firstRay = jobDataPtr->currentRays[index];

                    Vec3f N0 = firstRay.ray.direction.Normalized(); // first ray direction is set to surface normal.
                    Vec3f origin = firstRay.ray.position + N0 * 0.01f;

                    Vec3f radiance = Vec3f(0.0f);
                    Vec3f beta = Vec3f(1.0f);

                    Vec3f direction;
                    if (m_shadingType == LightmapShadingType::IRRADIANCE)
                    {
                        Vec3f rnd(MathUtil::RandomFloat(seed), MathUtil::RandomFloat(seed), MathUtil::RandomFloat(seed));
                        direction = MathUtil::RandomInHemisphere(rnd, N0).Normalize();
                    }
                    else
                    {
                        direction = N0;
                    }

                    for (int bounceIndex = 0; bounceIndex < MaxBouncesCpu; ++bounceIndex)
                    {
                        LightmapRay ray = firstRay;
                        ray.ray = Ray { origin, direction };

                        LightmapRayHitPayload payload {};
                        TraceSingleRayOnCPU(job, ray, payload);

                        if (payload.distance < 0.0f)
                        {
                            if (envProbeTexture.IsValid())
                            {
                                Vec3f env = envProbeTexture->SampleCube(direction).GetXYZ();
                                radiance += beta * env;
                            }
                            break;
                        }

                        Vec3f albedo = payload.albedo;
                        Vec3f f = albedo * (1.0f / MathUtil::pi<float>);

                        Vec3f hitPos = origin + direction * payload.distance;
                        Vec3f N = payload.normal.Normalized();

                        if (!MathUtil::ApproxEqual(payload.emissive, Vec3f::Zero()))
                        {
                            radiance += beta * payload.emissive;
                        }

                        for (const auto& [light, lightBuf] : sharedCpuData->lightData)
                        {
                            radiance += beta * EvaluateDiffuseLighting(job, light, lightBuf, albedo, hitPos, N);
                        }

                        if (m_shadingType != LightmapShadingType::IRRADIANCE)
                        {
                            break;
                        }

                        Vec3f rnd(MathUtil::RandomFloat(seed), MathUtil::RandomFloat(seed), MathUtil::RandomFloat(seed));
                        Vec3f wi = MathUtil::RandomInHemisphere(rnd, N).Normalize();

                        float cosTheta = MathUtil::Max(0.0f, N.Dot(wi));
                        const float pdf = 1.0f / (2.0f * MathUtil::pi<float>);

                        beta *= f * (cosTheta / pdf);

                        if (bounceIndex >= 2)
                        {
                            float p = MathUtil::Clamp(beta.Max(), 0.05f, 0.99f);
                            if (MathUtil::RandomFloat(seed) > p)
                            {
                                break;
                            }
                            beta /= p;
                        }

                        direction = wi;

                        float sign = N.Dot(direction) > 0.0f ? 1.0f : -1.0f;
                        origin = hitPos + N * (0.01f * sign);
                    }

                    // write result
                    jobDataPtr->hitsBuffer[index].color = radiance;
                }
            });
    }

    taskBatch->OnComplete
        .Bind([sharedCpuData, numBatches, job]()
            {
                delete sharedCpuData;
            })
        .Detach();

    TaskSystem::GetInstance().EnqueueBatch(taskBatch);

    job->AddTask(taskBatch);
}

void LightmapRenderer_CpuPathTracing::TraceSingleRayOnCPU(LightmapJobBase* job, const LightmapRay& ray, LightmapRayHitPayload& outPayload)
{
    outPayload.albedo = Vec3f(0.0f);
    outPayload.emissive = Vec3f(0.0f);
    outPayload.radiance = Vec3f(0.0f);
    outPayload.normal = Vec3f(0.0f);
    outPayload.distance = -1.0f;
    outPayload.barycentricCoords = Vec3f(0.0f);
    outPayload.meshId = ObjId<Mesh>::invalid;
    outPayload.triangleIndex = ~0u;

    if (!m_accelerationStructure)
    {
        HYP_LOG(Lightmap, Warning, "No acceleration structure set while tracing on CPU, cannot perform trace");

        return;
    }

    LightmapRayTestResults results = m_accelerationStructure->TestRay(ray.ray);

    if (!results.Any())
    {
        return;
    }

    for (const LightmapRayHit& hit : results)
    {
        if (hit.distance + 0.0001f <= 0.0f)
        {
            continue;
        }

        Assert(hit.entity.IsValid());

        auto it = job->GetParams().subElementsByEntity->Find(hit.entity);

        Assert(it != job->GetParams().subElementsByEntity->End());
        Assert(it->second != nullptr);

        const LightmapSubElement& subElement = *it->second;

        const ObjId<Mesh> meshId = subElement.mesh->Id();

        const Vec3f barycentricCoords = hit.barycentricCoords;

        const Triangle& triangle = hit.triangle;

        const Vec2f uv = triangle.GetPoint(0).GetTexCoord0() * barycentricCoords.x
            + triangle.GetPoint(1).GetTexCoord0() * barycentricCoords.y
            + triangle.GetPoint(2).GetTexCoord0() * barycentricCoords.z;

        Vec4f albedo = Vec4f(subElement.material->GetParameter(MATERIAL_KEY_ALBEDO));

        // sample albedo texture, if present
        if (const Handle<Texture>& albedoTexture = subElement.material->GetTexture(MaterialTextureKey::ALBEDO_MAP))
        {
            Vec4f albedoTextureColor = albedoTexture->Sample2D(uv);

            albedo *= albedoTextureColor;
        }

        outPayload.emissive = Vec3f(0.0f);
        outPayload.albedo = MathUtil::Clamp(albedo.GetXYZ(), Vec3f(0.0f), Vec3f(1.0f));
        outPayload.barycentricCoords = barycentricCoords;
        outPayload.meshId = meshId;
        outPayload.triangleIndex = hit.id;
        outPayload.normal = hit.normal;
        outPayload.distance = hit.distance;

        return;
    }
}

float LightmapRenderer_CpuPathTracing::TraceShadowRay(LightmapJobBase* job, const Vec3f& pos, const Vec3f& dir, const Vec3f& wi)
{
    const float eps = 1e-3f;
    const float sign = dir.Dot(wi) > 0.0f ? 1.0f : -1.0f;

    LightmapRay shadowRay {};

    shadowRay.triangleIndex = 0;
    shadowRay.ray.position = pos + dir * (eps * sign);
    shadowRay.ray.direction = wi;

    LightmapRayHitPayload payload {};
    TraceSingleRayOnCPU(job, shadowRay, payload);

    return float(payload.distance >= 0.0f);
}

#pragma endregion LightmapRenderer_CpuPathTracing

} // namespace Hyperion
