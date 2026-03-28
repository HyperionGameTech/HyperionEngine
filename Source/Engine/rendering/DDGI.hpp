/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <rendering/Shared.hpp>
#include <rendering/RenderObject.hpp>

#include <Core/math/BoundingBox.hpp>

#include <Core/Types.hpp>

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
    BoundingBox aabb;
    float probeDistance = 2.2f;
    uint32 numRaysPerProbe = 32;

    HYP_FORCE_INLINE const Vec3f& GetOrigin() const
    {
        return aabb.min;
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

class DDGI
{
public:
    HYP_DEF_POOL_NEW_DELETE(g_renderPool);

    static constexpr uint32 IrradianceOctahedronSize = 8;
    static constexpr uint32 DepthOctahedronSize = 8;
    static constexpr Vec3u ProbeBorder = Vec3u { 2, 0, 2 };

    explicit DDGI(DDGIInfo&& gridInfo);
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
        return m_cbuffers[frameIndex];
    }

    void Create();

    void Render(Frame* frame, const RenderSetup& renderSetup);

private:
    void FillProbeGrid();

    void CreateConstantBuffers();
    void CreateStorageBuffers();

    void UpdateUniforms(Frame* frame, const RenderSetup& renderSetup);

    DDGIInfo m_gridInfo;
    Array<DDGIProbeData, DynamicAllocator> m_probeData;

    FixedArray<GpuBufferRef, NumFramesInFlight> m_cbuffers;

    GpuBuffer* m_dynamicCBuffer = nullptr;
    size_t m_dynamicCBufferOffset = 0;
    size_t m_dynamicCBufferSize = 0;

    GpuBufferRef m_radianceBuffer;

    GpuImageRef m_irradianceImage;
    GpuImageViewRef m_irradianceImageView;
    GpuImageRef m_depthImage;
    GpuImageViewRef m_depthImageView;

    RotationMatrixGenerator m_randomGenerator;
    uint32 m_counter;
};

} // namespace Hyperion
