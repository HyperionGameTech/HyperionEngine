/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/ShaderManager.hpp>

#include <rendering/Shared.hpp>
#include <rendering/RenderObject.hpp>
#include <rendering/rt/RenderRaytracingPipeline.hpp>

#include <core/math/BoundingBox.hpp>

#include <core/Types.hpp>

#include <random>

namespace hyperion {

struct RenderSetup;

enum ProbeSystemFlags : uint32
{
    PROBE_SYSTEM_FLAGS_NONE = 0x0,
    PROBE_SYSTEM_FLAGS_FIRST_RUN = 0x1
};

struct ProbeRayData
{
    Vec4f directionDepth;
    Vec4f origin;
    Vec4f normal;
    Vec4f color;
};

static_assert(sizeof(ProbeRayData) == 64);

struct DDGIUniforms
{
    Vec4f aabbMax;
    Vec4f aabbMin;
    Vec4u probeBorder;
    Vec4u probeCounts;
    Vec4u gridDimensions;
    Vec4u imageDimensions;

    float probeDistance;
    uint32 numRaysPerProbe;
    uint32 numBoundLights;
    uint32 flags;

    Vec4u lightIndices[4];
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
    Matrix4 matrix;
    std::random_device randomDevice;
    std::mt19937 mt { randomDevice() };
    std::uniform_real_distribution<float> angle { 0.0f, 359.0f };
    std::uniform_real_distribution<float> axis { -1.0f, 1.0f };

    const Matrix4& Next()
    {
        return matrix = Matrix4::Rotation({ Vec3f { axis(mt), axis(mt), axis(mt) }.Normalize(),
                   MathUtil::DegToRad(angle(mt)) });
    }
};

struct Probe
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

    const Array<Probe>& GetProbes() const
    {
        return m_probes;
    }

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

    void Create();

    void Render(FrameBase* frame, const RenderSetup& renderSetup);

private:
    void CreateUniformBuffer();
    void CreateStorageBuffers();

    void UpdatePipelineState(FrameBase* frame, const RenderSetup& renderSetup);
    void UpdateUniforms(FrameBase* frame, const RenderSetup& renderSetup);

    DDGIInfo m_gridInfo;
    Array<Probe> m_probes;

    FixedArray<uint32, g_framesInFlight> m_updates;

    ComputePipelineRef m_updateIrradiance;
    ComputePipelineRef m_updateDepth;
    ComputePipelineRef m_copyBorderTexelsIrradiance;
    ComputePipelineRef m_copyBorderTexelsDepth;

    RaytracingPipelineRef m_pipeline;

    FixedArray<GpuBufferRef, g_framesInFlight> m_uniformBuffers;
    GpuBufferRef m_radianceBuffer;

    GpuImageRef m_irradianceImage;
    GpuImageViewRef m_irradianceImageView;
    GpuImageRef m_depthImage;
    GpuImageViewRef m_depthImageView;

    DDGIUniforms m_uniforms;

    RotationMatrixGenerator m_randomGenerator;
    uint32 m_time;
};

} // namespace hyperion
