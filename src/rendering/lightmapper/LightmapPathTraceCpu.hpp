/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/lightmapper/Lightmapper.hpp>
#include <rendering/RenderProxy.hpp>

#include <core/threading/TaskSystem.hpp>

namespace hyperion {

struct LightmapHitsBuffer;
class LightmapThreadPool;
class LightmapTopLevelAccelerationStructure;
class LightmapJobBase;
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

class HYP_API LightmapRenderer_CpuPathTracing : public ILightmapRenderer
{
public:
    LightmapRenderer_CpuPathTracing(LightmapperBase* lightmapper, LightmapTopLevelAccelerationStructure* accelerationStructure, LightmapThreadPool* threadPool, const Handle<Scene>& scene, LightmapShadingType shadingType);
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
    virtual void Render(FrameBase* frame, const RenderSetup& renderSetup, LightmapJobBase* job, Span<const LightmapRay> rays, uint32 rayOffset) override;

private:
    struct SharedCpuData
    {
        HashMap<Light*, LightShaderData> lightData;
        HashMap<EnvProbe*, EnvProbeShaderData> envProbeData;
    };

    void TraceSingleRayOnCPU(LightmapJobBase* job, const LightmapRay& ray, LightmapRayHitPayload& outPayload);
    float TraceShadowRay(LightmapJobBase* job, const Vec3f& pos, const Vec3f& dir, const Vec3f& wi);
    Vec3f EvaluateDiffuseLighting(LightmapJobBase* job, Light* light, const LightShaderData& bufferData, const Vec3f& albedo, const Vec3f& position, const Vec3f& normal);

    static SharedCpuData* CreateSharedCpuData(RenderProxyList& rpl);

    LightmapTopLevelAccelerationStructure* m_accelerationStructure;
    LightmapThreadPool* m_threadPool;

    Handle<Scene> m_scene;
    LightmapShadingType m_shadingType;

    Array<LightmapHit, DynamicAllocator> m_hitsBuffer;

    Array<LightmapRay, DynamicAllocator> m_currentRays;

    AtomicVar<uint32> m_numTracingTasks;
};

} // namespace hyperion
