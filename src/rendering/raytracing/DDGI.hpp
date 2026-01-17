/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/Shared.hpp>
#include <rendering/RenderObject.hpp>
#include <rendering/raytracing/RenderRaytracingPipeline.hpp>

#include <core/math/BoundingBox.hpp>

#include <core/Types.hpp>

#include <random>

namespace Hyperion {

struct RenderSetup;

enum ProbeSystemFlags : uint32
{
    PROBE_SYSTEM_FLAGS_NONE = 0x0,
    PROBE_SYSTEM_FLAGS_FIRST_RUN = 0x1
};

struct DDGIInfo
{
    static constexpr uint32 irradianceOctahedronSize = 8;
    static constexpr uint32 depthOctahedronSize = 16;
    static constexpr Vec3u probeBorder = Vec3u { 2, 0, 2 };

    BoundingBox aabb;
    float probeDistance = 2.5f;
    uint32 numRaysPerProbe = 32;

    HYP_FORCE_INLINE const Vec3f& GetOrigin() const
    {
        return aabb.min;
    }

    HYP_FORCE_INLINE Vec3u NumProbesPerDimension() const
    {
        const Vec3f probesPerDimension = MathUtil::Ceil((aabb.GetExtent() / probeDistance) + Vec3f(probeBorder));

        return Vec3u(probesPerDimension);
    }

    HYP_FORCE_INLINE uint32 NumProbes() const
    {
        const Vec3u perDimension = NumProbesPerDimension();

        return perDimension.x * perDimension.y * perDimension.z;
    }

    HYP_FORCE_INLINE Vec2u GetImageDimensions() const
    {
        return { uint32(MathUtil::NextPowerOf2(NumProbes())), numRaysPerProbe };
    }
};

struct RotationMatrixGenerator
{
    Mat4f matrix;
    std::random_device randomDevice;
    std::mt19937 mt { randomDevice() };
    std::uniform_real_distribution<float> angle { 0.0f, 359.0f };
    std::uniform_real_distribution<float> axis { -1.0f, 1.0f };

    const Mat4f& Next()
    {
        return matrix = Mat4f::Rotation({ Vec3f { axis(mt), axis(mt), axis(mt) }.Normalize(),
                   MathUtil::DegToRad(angle(mt)) });
    }
};

struct DDGIProbeData
{
    Vec3f position;
};

class HYP_API DDGI
{
public:
    DDGI(DDGIInfo&& gridInfo);
    DDGI(const DDGI& other) = delete;
    DDGI& operator=(const DDGI& other) = delete;
    ~DDGI();

    const GpuBufferRef& GetRadianceBuffer() const
    {
        return m_radianceBuffer;
    }

    const GpuImageRef& GetIrradianceImage() const
    {
        return m_irradianceImage;
    }

    const GpuImageViewRef& GetIrradianceImageView() const
    {
        return m_irradianceImageView;
    }

    const GpuImageRef& GetDepthImage() const
    {
        return m_depthImage;
    }

    const GpuImageViewRef& GetDepthImageView() const
    {
        return m_depthImageView;
    }

    const GpuBufferRef& GetConstantBuffer(uint32 frameIndex) const
    {
        return m_cBuffers[frameIndex];
    }

    void Create();

    void Render(Frame* frame, const RenderSetup& renderSetup);

private:
    void FillProbeGrid();

    void CreateConstantBuffers();
    void CreateStorageBuffers();

    void UpdatePipelineState(Frame* frame, const RenderSetup& renderSetup);
    void UpdateUniforms(Frame* frame, const RenderSetup& renderSetup);

    DDGIInfo m_gridInfo;
    Array<DDGIProbeData, DynamicAllocator> m_probeData;

    ComputePipelineRef m_updateIrradiance;
    ComputePipelineRef m_updateDepth;
    ComputePipelineRef m_copyBorderTexelsIrradiance;
    ComputePipelineRef m_copyBorderTexelsDepth;

    RaytracingPipelineRef m_pipeline;

    FixedArray<GpuBufferRef, NumFramesInFlight> m_cBuffers;
    FixedArray<GpuBufferRef, NumFramesInFlight> m_lightsBuffers;
    GpuBufferRef m_radianceBuffer;

    GpuImageRef m_irradianceImage;
    GpuImageViewRef m_irradianceImageView;
    GpuImageRef m_depthImage;
    GpuImageViewRef m_depthImageView;

    RotationMatrixGenerator m_randomGenerator;
    uint32 m_counter;
};

} // namespace Hyperion
