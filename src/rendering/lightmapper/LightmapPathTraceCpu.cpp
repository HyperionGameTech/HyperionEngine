#include <rendering/lightmapper/LightmapPathTraceCpu.hpp>

#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/RenderHelpers.hpp>
#include <rendering/RenderCollection.hpp>
#include <rendering/RenderBackend.hpp>
#include <rendering/RenderObject.hpp>
#include <rendering/RenderConfig.hpp>
#include <rendering/RenderDevice.hpp>
#include <rendering/RenderFrame.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/Material.hpp>
#include <rendering/Texture.hpp>
#include <rendering/Renderer.hpp>

#include <asset/TextureAsset.hpp>

#include <scene/BVH.hpp>
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

#include <core/config/Config.hpp>

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

HYP_API extern const GlobalConfig& GetGlobalConfig();

static constexpr uint32 g_maxBouncesCpu = 4;

struct LightmapRayHit : RayHit
{
    Handle<Entity> entity;
    Triangle triangle;

    LightmapRayHit() = default;

    LightmapRayHit(const RayHit& rayHit, const Handle<Entity>& entity, const Triangle& triangle)
        : RayHit(rayHit),
          entity(entity),
          triangle(triangle)
    {
    }

    LightmapRayHit(const LightmapRayHit& other) = default;
    LightmapRayHit& operator=(const LightmapRayHit& other) = default;

    LightmapRayHit(LightmapRayHit&& other) noexcept
        : RayHit(static_cast<RayHit&&>(std::move(other))),
          entity(std::move(other.entity)),
          triangle(std::move(other.triangle))
    {
    }

    LightmapRayHit& operator=(LightmapRayHit&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        RayHit::operator=(static_cast<RayHit&&>(std::move(other)));
        entity = std::move(other.entity);
        triangle = std::move(other.triangle);

        return *this;
    }

    virtual ~LightmapRayHit() = default;

    bool operator==(const LightmapRayHit& other) const
    {
        return static_cast<const RayHit&>(*this) == static_cast<const RayHit&>(other)
            && entity == other.entity
            && triangle == other.triangle;
    }

    bool operator!=(const LightmapRayHit& other) const
    {
        return static_cast<const RayHit&>(*this) != static_cast<const RayHit&>(other)
            || entity != other.entity
            || triangle != other.triangle;
    }

    bool operator<(const LightmapRayHit& other) const
    {
        if (static_cast<const RayHit&>(*this) < static_cast<const RayHit&>(other))
        {
            return true;
        }

        if (entity < other.entity)
        {
            return true;
        }

        if (entity == other.entity && triangle.GetPosition() < other.triangle.GetPosition())
        {
            return true;
        }

        return false;
    }
};

using LightmapRayTestResults = FlatSet<LightmapRayHit>;

class LightmapBottomLevelAccelerationStructure
{
public:
    LightmapBottomLevelAccelerationStructure(const LightmapSubElement* subElement, const BVHNode* bvh)
        : m_subElement(subElement),
          m_root(bvh)
    {
        Assert(m_subElement != nullptr);
        Assert(m_root != nullptr);
    }

    LightmapBottomLevelAccelerationStructure(const LightmapBottomLevelAccelerationStructure& other) = delete;
    LightmapBottomLevelAccelerationStructure& operator=(const LightmapBottomLevelAccelerationStructure& other) = delete;

    LightmapBottomLevelAccelerationStructure(LightmapBottomLevelAccelerationStructure&& other) noexcept
        : m_subElement(other.m_subElement),
          m_root(other.m_root)
    {
        other.m_subElement = nullptr;
        other.m_root = nullptr;
    }

    LightmapBottomLevelAccelerationStructure& operator=(LightmapBottomLevelAccelerationStructure&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        m_subElement = other.m_subElement;
        m_root = std::move(other.m_root);

        other.m_subElement = nullptr;
        other.m_root = nullptr;

        return *this;
    }

    ~LightmapBottomLevelAccelerationStructure() = default;

    HYP_FORCE_INLINE const Handle<Entity>& GetEntity() const
    {
        return m_subElement->entity;
    }

    HYP_FORCE_INLINE const Transform& GetTransform() const
    {
        return m_subElement->transform;
    }

    LightmapRayTestResults TestRay(const Ray& ray) const
    {
        Assert(m_root != nullptr);

        LightmapRayTestResults results;

        const Matrix4& modelMatrix = m_subElement->transform.GetMatrix();

        const Ray localSpaceRay = modelMatrix.Inverted() * ray;

        RayTestResults localBvhResults = m_root->TestRay(localSpaceRay);

        if (localBvhResults.Any())
        {
            const Matrix4 normalMatrix = modelMatrix.Transposed().Inverted();

            RayTestResults bvhResults;

            for (RayHit hit : localBvhResults)
            {
                Vec4f transformedNormal = normalMatrix * Vec4f(hit.normal, 0.0f);
                hit.normal = transformedNormal.GetXYZ().Normalized();

                Vec4f transformedPosition = modelMatrix * Vec4f(hit.hitpoint, 1.0f);
                transformedPosition /= transformedPosition.w;

                hit.hitpoint = transformedPosition.GetXYZ();

                hit.distance = (hit.hitpoint - ray.position).Length();

                bvhResults.AddHit(hit);
            }

            for (const RayHit& rayHit : bvhResults)
            {
                Assert(rayHit.userData != nullptr);

                const BVHNode* bvhNode = static_cast<const BVHNode*>(rayHit.userData);

                const Triangle& triangle = bvhNode->triangles[rayHit.id];

                results.Emplace(rayHit, m_subElement->entity, triangle);
            }
        }

        return results;
    }

    HYP_FORCE_INLINE const BVHNode* GetRoot() const
    {
        return m_root;
    }

private:
    const LightmapSubElement* m_subElement;
    const BVHNode* m_root;
};

class LightmapTopLevelAccelerationStructure
{
public:
    ~LightmapTopLevelAccelerationStructure() = default;

    HYP_FORCE_INLINE const Transform& GetTransform() const
    {
        return Transform::identity;
    }

    LightmapRayTestResults TestRay(const Ray& ray) const
    {
        LightmapRayTestResults results;

        for (const LightmapBottomLevelAccelerationStructure& accelerationStructure : m_accelerationStructures)
        {
            if (!ray.TestAABB(accelerationStructure.GetTransform() * accelerationStructure.GetRoot()->aabb))
            {
                continue;
            }

            results.Merge(accelerationStructure.TestRay(ray));
        }

        return results;
    }

    void Add(const LightmapSubElement* subElement, const BVHNode* bvh)
    {
        m_accelerationStructures.EmplaceBack(subElement, bvh);
    }

    void RemoveAll()
    {
        m_accelerationStructures.Clear();
    }

private:
    Array<LightmapBottomLevelAccelerationStructure> m_accelerationStructures;
};

#pragma region LightmapThreadPool

uint32 LightmapThreadPool::NumThreadsToCreate()
{
    uint32 numThreads = GetGlobalConfig().Get("lightmapper.numThreadsPerJob").ToUInt32(4);
    return MathUtil::Clamp(numThreads, 1u, Threads::NumCores());
}

#pragma endregion LightmapThreadPool

#pragma region LightmapRenderer_CpuPathTracing

LightmapRenderer_CpuPathTracing::LightmapRenderer_CpuPathTracing(
    Lightmapper* lightmapper,
    LightmapTopLevelAccelerationStructure* accelerationStructure,
    LightmapThreadPool* threadPool,
    const Handle<Scene>& scene,
    LightmapShadingType shadingType)
    : ILightmapRenderer(lightmapper),
      m_accelerationStructure(accelerationStructure),
      m_threadPool(threadPool),
      m_scene(scene),
      m_shadingType(shadingType),
      m_numTracingTasks(0)
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

void LightmapRenderer_CpuPathTracing::UpdateRays(Span<const LightmapRay> rays)
{
}

void LightmapRenderer_CpuPathTracing::ReadHitsBuffer(FrameBase* frame, Span<LightmapHit> outHits)
{
    Threads::AssertOnThread(g_renderThread);

    Assert(m_numTracingTasks.Get(MemoryOrder::ACQUIRE) == 0,
        "Cannot read hits buffer while tracing is in progress");

    Assert(outHits.Size() == m_hitsBuffer.Size());

    Memory::MemCpy(outHits.Data(), m_hitsBuffer.Data(), m_hitsBuffer.ByteSize());
}

Vec3f LightmapRenderer_CpuPathTracing::EvaluateDiffuseLighting(Light* light, const LightShaderData& bufferData, const Vec3f& position, const Vec3f& normal)
{
    Assert(light != nullptr);

    switch (light->GetLightType())
    {
    case LT_DIRECTIONAL:
        return (ByteUtil::UnpackVec4f(SwapEndian(bufferData.colorPacked)) * MathUtil::Max(0.0f, normal.Dot(bufferData.positionIntensity.GetXYZ().Normalized())) * bufferData.positionIntensity.w).GetXYZ();
    case LT_POINT:
    {
        const float radius = Float16::FromRaw(bufferData.radiusFalloffPacked >> 16);

        Vec3f lightDir = (bufferData.positionIntensity.GetXYZ() - position).Normalized();
        float dist = (bufferData.positionIntensity.GetXYZ() - position).Length();
        float distSqr = dist * dist;

        float invRadius = 1.0f / radius;
        float factor = distSqr * (invRadius * invRadius);
        float smoothFactor = MathUtil::Max(1.0f - (factor * factor), 0.0f);

        return (ByteUtil::UnpackVec4f(SwapEndian(bufferData.colorPacked)) * ((smoothFactor * smoothFactor) / MathUtil::Max(distSqr, 1e4f)) * bufferData.positionIntensity.w).GetXYZ();
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
        RenderProxyLight* lightProxy = static_cast<RenderProxyLight*>(RenderApi_GetRenderProxy(light->Id()));

        if (lightProxy)
        {
            sharedCpuData->lightData[light] = lightProxy->bufferData;
        }
    }

    for (EnvProbe* envProbe : rpl.GetEnvProbes().GetElements<SkyProbe>())
    {
        RenderProxyEnvProbe* envProbeProxy = static_cast<RenderProxyEnvProbe*>(RenderApi_GetRenderProxy(envProbe->Id()));

        if (envProbeProxy)
        {
            sharedCpuData->envProbeData[envProbe] = envProbeProxy->bufferData;
        }
    }

    rpl.EndRead();

    return sharedCpuData;
}

void LightmapRenderer_CpuPathTracing::Render(FrameBase* frame, const RenderSetup& renderSetup, LightmapJob* job, Span<const LightmapRay> rays, uint32 rayOffset)
{
    Threads::AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.IsValid());
    AssertDebug(renderSetup.HasView());

    RenderProxyList& rpl = RenderApi_GetConsumerProxyList(renderSetup.view);

    SharedCpuData* sharedCpuData = CreateSharedCpuData(rpl);

    Assert(m_numTracingTasks.Get(MemoryOrder::ACQUIRE) == 0,
        "Trace is already in progress");

    Handle<Texture> envProbeTexture;

    if (renderSetup.envProbe)
    {
        // prepare env probe texture to be sampled on the CPU in the tasks
        envProbeTexture = renderSetup.envProbe->GetPrefilteredEnvMap();
    }

    m_hitsBuffer.Resize(rays.Size());

    m_currentRays.Resize(rays.Size());
    Memory::MemCpy(m_currentRays.Data(), rays.Data(), m_currentRays.ByteSize());

    m_numTracingTasks.Increment(rays.Size(), MemoryOrder::RELEASE);

    TaskBatch* taskBatch = new TaskBatch();
    taskBatch->pool = m_threadPool;

    const uint32 numItems = uint32(m_currentRays.Size());
    const uint32 numBatches = m_threadPool->GetProcessorAffinity();
    const uint32 itemsPerBatch = (numItems + numBatches - 1) / numBatches;

    for (uint32 batchIndex = 0; batchIndex < numBatches; batchIndex++)
    {
        taskBatch->AddTask([this, sharedCpuData, job, batchIndex, itemsPerBatch, numItems, envProbeTexture](...)
            {
                uint32 seed = std::rand();

                const uint32 offsetIndex = batchIndex * itemsPerBatch;
                const uint32 maxIndex = MathUtil::Min(offsetIndex + itemsPerBatch, numItems);

                for (uint32 index = offsetIndex; index < maxIndex; index++)
                {
                    HYP_DEFER({ m_numTracingTasks.Decrement(1, MemoryOrder::RELEASE); });

                    const LightmapRay& firstRay = m_currentRays[index];

                    FixedArray<LightmapRay, g_maxBouncesCpu + 1> recursiveRays;
                    FixedArray<LightmapRayHitPayload, g_maxBouncesCpu + 1> bounces;

                    int numBounces = 0;

                    Vec3f direction = firstRay.ray.direction.Normalized();

                    if (m_shadingType == LightmapShadingType::IRRADIANCE)
                    {
                        direction = MathUtil::RandomInHemisphere(
                            Vec3f(MathUtil::RandomFloat(seed), MathUtil::RandomFloat(seed), MathUtil::RandomFloat(seed)),
                            firstRay.ray.direction)
                                        .Normalize();
                    }

                    Vec3f origin = firstRay.ray.position + direction * 0.001f;

                    // Vec4f skyColor;

                    // if (envProbeTexture.IsValid()) {
                    //     Vec4f envProbeSample = envProbeTexture->SampleCube(direction);

                    //     skyColor = envProbeSample;
                    // }

                    for (int bounceIndex = 0; bounceIndex < g_maxBouncesCpu; bounceIndex++)
                    {
                        LightmapRay bounceRay = firstRay;

                        if (bounceIndex != 0)
                        {
                            bounceRay.meshId = bounces[bounceIndex - 1].meshId;
                            bounceRay.triangleIndex = bounces[bounceIndex - 1].triangleIndex;
                        }

                        bounceRay.ray = Ray {
                            origin,
                            direction
                        };

                        recursiveRays[bounceIndex] = bounceRay;

                        LightmapRayHitPayload& payload = bounces[bounceIndex];
                        payload = {};

                        TraceSingleRayOnCPU(job, bounceRay, payload);

                        if (payload.distance <= -1.0f)
                        {
                            payload.throughput = Vec3f(0.0f);

                            Assert(bounceIndex < bounces.Size());

                            // @TODO Sample environment map
                            const Vec3f normal = bounceRay.ray.direction;

                            // if (envProbeTexture.IsValid()) {
                            //     Vec4f envProbeSample = envProbeTexture->SampleCube(direction);

                            //     payload.emissive += envProbeSample;
                            // }

                            for (const auto& [light, lightBufferData] : sharedCpuData->lightData)
                            {
                                payload.emissive += EvaluateDiffuseLighting(light, lightBufferData, origin, normal);
                            }

                            ++numBounces;

                            break;
                        }

                        Vec3f hitPosition = origin + direction * payload.distance;

                        if (m_shadingType == LightmapShadingType::IRRADIANCE)
                        {
                            const Vec3f rnd = Vec3f(MathUtil::RandomFloat(seed), MathUtil::RandomFloat(seed), MathUtil::RandomFloat(seed));

                            direction = MathUtil::RandomInHemisphere(rnd, payload.normal).Normalize();
                        }
                        else
                        {
                            ++numBounces;
                            break;
                        }

                        origin = hitPosition + direction * 0.02f;

                        ++numBounces;
                    }

                    for (int bounceIndex = int(numBounces - 1); bounceIndex >= 0; bounceIndex--)
                    {
                        Vec3f radiance = bounces[bounceIndex].emissive;

                        if (bounceIndex != numBounces - 1)
                        {
                            radiance += bounces[bounceIndex + 1].radiance * bounces[bounceIndex].throughput;
                        }

                        float p = MathUtil::Max(radiance.x, MathUtil::Max(radiance.y, radiance.z));

                        if (MathUtil::RandomFloat(seed) > p)
                        {
                            break;
                        }

                        radiance /= MathUtil::Max(p, 0.0001f);

                        bounces[bounceIndex].radiance = radiance;
                    }

                    LightmapHit& hit = m_hitsBuffer[index];

                    if (numBounces != 0)
                    {
                        hit.color = bounces[0].radiance;

                        if (MathUtil::IsNaN(hit.color) || !MathUtil::IsFinite(hit.color))
                        {
                            HYP_LOG_ONCE(Lightmap, Warning, "NaN or infinite color detected while tracing rays");

                            hit.color = Vec3f(0.0f);
                        }
                    }
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

void LightmapRenderer_CpuPathTracing::TraceSingleRayOnCPU(LightmapJob* job, const LightmapRay& ray, LightmapRayHitPayload& outPayload)
{
    outPayload.throughput = Vec3f(0.0f);
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

        Vec4f albedo = Vec4f(subElement.material->GetParameter(Material::MATERIAL_KEY_ALBEDO));

        // sample albedo texture, if present
        if (const Handle<Texture>& albedoTexture = subElement.material->GetTexture(MaterialTextureKey::ALBEDO_MAP))
        {
            Vec4f albedoTextureColor = albedoTexture->Sample2D(uv);

            albedo *= albedoTextureColor;
        }

        outPayload.emissive = Vec3f(0.0f);
        outPayload.throughput = albedo.GetXYZ() * albedo.GetW();
        outPayload.barycentricCoords = barycentricCoords;
        outPayload.meshId = meshId;
        outPayload.triangleIndex = hit.id;
        outPayload.normal = hit.normal;
        outPayload.distance = hit.distance;

        return;
    }
}

#pragma endregion LightmapRenderer_CpuPathTracing

#pragma region Lightmapper_CpuPathTracing

Lightmapper_CpuPathTracing::Lightmapper_CpuPathTracing(LightmapperConfig&& config, const Handle<Scene>& scene, const BoundingBox& aabb)
    : Lightmapper(std::move(config), scene, aabb)
{
    if (m_threadPool.IsRunning())
    {
        m_threadPool.Stop();
    }
}

void Lightmapper_CpuPathTracing::Initialize_Internal()
{
    BuildResourceCache();

    m_threadPool.Start();
}

void Lightmapper_CpuPathTracing::Build_Internal()
{
    BuildAccelerationStructures();
}

void Lightmapper_CpuPathTracing::BuildAccelerationStructures()
{
    Assert(m_accelerationStructure == nullptr);
    m_accelerationStructure = MakeUnique<LightmapTopLevelAccelerationStructure>();

    if (m_subElements.Empty())
    {
        return;
    }

    for (LightmapSubElement& subElement : m_subElements)
    {
        if (!subElement.mesh->BuildBVH())
        {
            HYP_LOG(Lightmap, Error, "Failed to build BVH for mesh on entity {} in lightmapper", subElement.entity.Id());

            continue;
        }

        m_accelerationStructure->Add(&subElement, &subElement.mesh->GetBVH());
    }
}

/// Build cache to keep scene meshes, textures etc. in memory while we perform CPU path tracing
void Lightmapper_CpuPathTracing::BuildResourceCache()
{
    HYP_NAMED_SCOPE("Building lightmapper resource cache");

    HYP_LOG(Lightmap, Info, "Building lightmapper resource cache");

    Mutex mtx;

    TaskBatch taskBatch;

    auto callback = [&](LightmapSubElement& subElement, uint32, uint32) -> void
    {
        Array<CachedResource> localResources;

        if (subElement.mesh.IsValid())
        {
            Assert(subElement.mesh->GetAsset().IsValid());

            localResources.EmplaceBack(subElement.mesh->GetAsset(), *subElement.mesh->GetAsset()->GetResource());
        }

        if (subElement.material.IsValid())
        {
            for (const auto& it : subElement.material->GetTextures())
            {
                if (it.second.IsValid())
                {
                    Assert(it.second->GetAsset().IsValid());

                    localResources.EmplaceBack(it.second->GetAsset(), *it.second->GetAsset()->GetResource());
                }
            }
        }

        if (localResources.Any())
        {
            Mutex::Guard guard(mtx);

            for (CachedResource& cachedResource : localResources)
            {
                m_resourceCache.Set(std::move(cachedResource));
            }
        }
    };

    TaskSystem::GetInstance().ParallelForEach_Batch(
        taskBatch,
        (m_subElements.Size() + 255) / 256,
        m_subElements, callback);

    TaskSystem::GetInstance().EnqueueBatch(&taskBatch);

    while (!taskBatch.IsCompleted())
    {
        Threads::Sleep(1000);

        Mutex::Guard guard(mtx);

        HYP_LOG(Lightmap, Debug, "Waiting for lightmapper resource cache to finish building... ({} resources discovered)", m_resourceCache.Size());
    }
}

#pragma endregion Lightmapper_CpuPathTracing

} // namespace hyperion
