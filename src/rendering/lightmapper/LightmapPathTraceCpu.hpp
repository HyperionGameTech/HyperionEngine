/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/lightmapper/Lightmapper.hpp>
#include <rendering/RenderProxy.hpp>

#include <core/threading/TaskSystem.hpp>

namespace hyperion {

struct LightmapHitsBuffer;
class LightmapThreadPool;
class LightmapTopLevelAccelerationStructure;
class LightmapJob;
class LightmapVolume;
class AssetObject;
class Light;
class EnvProbe;
class RenderProxyList;
class View;
struct RenderSetup;

class LightmapperWorkerThread : public TaskThread
{
public:
    LightmapperWorkerThread(ThreadId id)
        : TaskThread(id)
    {
    }

    virtual ~LightmapperWorkerThread() override = default;
};

class LightmapThreadPool : public TaskThreadPool
{
public:
    LightmapThreadPool()
        : TaskThreadPool(TypeWrapper<LightmapperWorkerThread>(), "LightmapperWorker", NumThreadsToCreate())
    {
    }

    virtual ~LightmapThreadPool() override = default;

private:
    static uint32 NumThreadsToCreate();
};

class HYP_API LightmapJob_CpuPathTracing : public LightmapJob
{
public:
    LightmapJob_CpuPathTracing(LightmapJobParams&& params)
        : LightmapJob(std::move(params))
    {
    }

    virtual ~LightmapJob_CpuPathTracing() override = default;
};

class HYP_API LightmapRenderer_CpuPathTracing : public ILightmapRenderer
{
public:
    LightmapRenderer_CpuPathTracing(LightmapTopLevelAccelerationStructure* accelerationStructure, LightmapThreadPool* threadPool, const Handle<Scene>& scene, LightmapShadingType shadingType);
    LightmapRenderer_CpuPathTracing(const LightmapRenderer_CpuPathTracing& other) = delete;
    LightmapRenderer_CpuPathTracing& operator=(const LightmapRenderer_CpuPathTracing& other) = delete;
    LightmapRenderer_CpuPathTracing(LightmapRenderer_CpuPathTracing&& other) noexcept = delete;
    LightmapRenderer_CpuPathTracing& operator=(LightmapRenderer_CpuPathTracing&& other) noexcept = delete;
    virtual ~LightmapRenderer_CpuPathTracing() override;

    virtual uint32 MaxRaysPerFrame() const override
    {
        return uint32(-1);
    }

    virtual LightmapShadingType GetShadingType() const override
    {
        return m_shadingType;
    }

    virtual void Create() override;
    virtual void UpdateRays(Span<const LightmapRay> rays) override;
    virtual void ReadHitsBuffer(FrameBase* frame, Span<LightmapHit> outHits) override;
    virtual void Render(FrameBase* frame, const RenderSetup& renderSetup, LightmapJob* job, Span<const LightmapRay> rays, uint32 rayOffset) override;

private:
    struct SharedCpuData
    {
        HashMap<Light*, LightShaderData> lightData;
        HashMap<EnvProbe*, EnvProbeShaderData> envProbeData;
    };

    void TraceSingleRayOnCPU(LightmapJob* job, const LightmapRay& ray, LightmapRayHitPayload& outPayload);

    static Vec3f EvaluateDiffuseLighting(Light* light, const LightShaderData& bufferData, const Vec3f& position, const Vec3f& normal);

    static SharedCpuData* CreateSharedCpuData(RenderProxyList& rpl);

    LightmapTopLevelAccelerationStructure* m_accelerationStructure;
    LightmapThreadPool* m_threadPool;

    Handle<Scene> m_scene;
    LightmapShadingType m_shadingType;

    Array<LightmapHit, DynamicAllocator> m_hitsBuffer;

    Array<LightmapRay, DynamicAllocator> m_currentRays;

    AtomicVar<uint32> m_numTracingTasks;
};

class HYP_API Lightmapper_CpuPathTracing : public Lightmapper
{
    struct CachedResource
    {
        Handle<AssetObject> assetObject;
        ResourceHandle resourceHandle;

        CachedResource() = default;

        CachedResource(const Handle<AssetObject>& assetObject, const ResourceHandle& resourceHandle)
            : assetObject(assetObject),
              resourceHandle(resourceHandle)
        {
        }

        CachedResource(const CachedResource& other) = delete;
        CachedResource& operator=(const CachedResource& other) = delete;

        CachedResource(CachedResource&& other) noexcept
            : assetObject(std::move(other.assetObject)),
              resourceHandle(std::move(other.resourceHandle))
        {
            other.resourceHandle.Reset();
        }

        CachedResource& operator=(CachedResource&& other) noexcept
        {
            if (this == &other)
            {
                return *this;
            }

            resourceHandle.Reset();

            assetObject = std::move(other.assetObject);
            resourceHandle = std::move(other.resourceHandle);

            return *this;
        }

        ~CachedResource()
        {
            // destruct the ResourceHandle before assetobject is destructed,
            // so it destructing the AssetObject doesn't try to wait for the resource's ref count to reach zero
            resourceHandle.Reset();
        }
    };

    using ResourceCache = HashSet<CachedResource, &CachedResource::assetObject, HashTable_DynamicNodeAllocator<CachedResource>>;

public:
    Lightmapper_CpuPathTracing(LightmapperConfig&& config, const Handle<Scene>& scene, const BoundingBox& aabb);
    virtual ~Lightmapper_CpuPathTracing() override = default;

private:
    virtual UniquePtr<LightmapJob> CreateJob(LightmapJobParams&& params)
    {
        return MakeUnique<LightmapJob_CpuPathTracing>(std::move(params));
    }

    virtual UniquePtr<ILightmapRenderer> CreateRenderer(LightmapShadingType shadingType) override
    {
        return MakeUnique<LightmapRenderer_CpuPathTracing>(m_accelerationStructure.Get(), &m_threadPool, m_scene, shadingType);
    }

    virtual void Initialize_Internal() override;
    virtual void Build_Internal() override;

    void BuildResourceCache();
    void BuildAccelerationStructures();

    UniquePtr<LightmapTopLevelAccelerationStructure> m_accelerationStructure;
    ResourceCache m_resourceCache;

    LightmapThreadPool m_threadPool;
};

} // namespace hyperion
