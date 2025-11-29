/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <lightmapper/Lightmapper.hpp>

#include <rendering/RenderObject.hpp>

#include <core/memory/RefCountedPtr.hpp>

namespace hyperion {

class RenderProxyList;
struct GpuLightmapperReadyNotification;

class HYP_API LightmapRenderer_GpuPathTracing : public ILightmapRenderer
{
public:
    LightmapRenderer_GpuPathTracing(
        LightmapperBase* lightmapper,
        const Handle<Scene>& scene,
        LightmapShadingType shadingType,
        uint32 maxRaysPerFrame);
    LightmapRenderer_GpuPathTracing(const LightmapRenderer_GpuPathTracing& other) = delete;
    LightmapRenderer_GpuPathTracing& operator=(const LightmapRenderer_GpuPathTracing& other) = delete;
    LightmapRenderer_GpuPathTracing(LightmapRenderer_GpuPathTracing&& other) noexcept = delete;
    LightmapRenderer_GpuPathTracing& operator=(LightmapRenderer_GpuPathTracing&& other) noexcept = delete;
    virtual ~LightmapRenderer_GpuPathTracing() override;

    HYP_FORCE_INLINE const RaytracingPipelineRef& GetPipeline() const
    {
        return m_raytracingPipeline;
    }

    virtual uint32 MaxRaysPerFrame() const override
    {
        return uint32(-1);
    }

    virtual LightmapShadingType GetShadingType() const override
    {
        return m_shadingType;
    }

    virtual bool CanRender() const override;

    virtual void Create() override;
    virtual void CleanJobData(LightmapJobBase* job) override;
    virtual void ReadHitsBuffer(Frame* frame, LightmapJobBase* job, Span<LightmapHit> outHits) override;
    virtual void Render(Frame* frame, const RenderSetup& renderSetup, LightmapJobBase* job, Span<const LightmapRay> rays, uint32 rayOffset) override;

private:
    struct JobData
    {
        FixedArray<GpuBufferRef, NumFramesInFlight> UniformBuffers;
        FixedArray<GpuBufferRef, NumFramesInFlight> RayBuffers;
        FixedArray<DescriptorSetRef, NumFramesInFlight> Sets;
        GpuBufferRef HitsBufferGpu;
        bool IsCreated = false;
    };

    void UpdatePipelineState(Frame* frame, LightmapJobBase* job);
    void CreateBuffers(LightmapJobBase* job);
    void CreateAccelerationStructures();
    void UpdateUniforms(Frame* frame, LightmapJobBase* job, uint32 rayOffset);

    Handle<Scene> m_scene;
    LightmapShadingType m_shadingType;
    uint32 m_maxRaysPerFrame;

    HashMap<LightmapJobBase*, JobData, DynamicNodeAllocator> m_jobData;

    RC<GpuLightmapperReadyNotification> m_readyNotification;

    GpuTlasRef m_tlas;

    RaytracingPipelineRef m_raytracingPipeline;
};

} // namespace hyperion
