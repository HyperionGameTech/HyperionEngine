/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/lightmapper/Lightmapper.hpp>

#include <rendering/RenderObject.hpp>

namespace hyperion {

class RenderProxyList;

class HYP_API LightmapJob_GpuPathTracing : public LightmapJob
{
public:
    LightmapJob_GpuPathTracing(LightmapJobParams&& params)
        : LightmapJob(std::move(params))
    {
    }

    virtual ~LightmapJob_GpuPathTracing() override = default;
};

class HYP_API LightmapRenderer_GpuPathTracing : public ILightmapRenderer
{
public:
    LightmapRenderer_GpuPathTracing(Lightmapper* lightmapper, const Handle<Scene>& scene, LightmapShadingType shadingType);
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

    virtual void Create() override;
    virtual void UpdateRays(Span<const LightmapRay> rays) override;
    virtual void ReadHitsBuffer(FrameBase* frame, Span<LightmapHit> outHits) override;
    virtual void Render(FrameBase* frame, const RenderSetup& renderSetup, LightmapJob* job, Span<const LightmapRay> rays, uint32 rayOffset) override;

private:
    void CreateUniformBuffer();
    void UpdatePipelineState(FrameBase* frame);
    void UpdateUniforms(FrameBase* frame, uint32 rayOffset);

    Handle<Scene> m_scene;
    LightmapShadingType m_shadingType;

    FixedArray<GpuBufferRef, g_framesInFlight> m_uniformBuffers;
    FixedArray<GpuBufferRef, g_framesInFlight> m_raysBuffers;

    GpuBufferRef m_hitsBufferGpu;

    FixedArray<TLASRef, g_framesInFlight> m_accelerationStructures;

    RaytracingPipelineRef m_raytracingPipeline;
};

HYP_CLASS()
class HYP_API Lightmapper_GpuPathTracing : public Lightmapper
{
    HYP_OBJECT_BODY(Lightmapper_GpuPathTracing);

public:
    Lightmapper_GpuPathTracing(LightmapperConfig&& config, const Handle<Scene>& scene, const BoundingBox& aabb);
    virtual ~Lightmapper_GpuPathTracing() override = default;

protected:
    virtual UniquePtr<LightmapJob> CreateJob(LightmapJobParams&& params)
    {
        return MakeUnique<LightmapJob_GpuPathTracing>(std::move(params));
    }

    virtual UniquePtr<ILightmapRenderer> CreateRenderer(LightmapShadingType shadingType) override
    {
        return MakeUnique<LightmapRenderer_GpuPathTracing>(this, m_scene, shadingType);
    }
};

} // namespace hyperion
