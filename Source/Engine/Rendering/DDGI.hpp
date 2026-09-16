/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Rendering/Shared.hpp>
#include <Rendering/RenderTypes.hpp>
#include <Rendering/RenderProxy.hpp>

#include <Core/Math/BoundingBox.hpp>

#include <Core/Types.hpp>

#include <random>

namespace Hyperion {

struct RenderSetup;

struct DDGIInfo
{
    Vec3u probeCountsPerCascade = { 16, 8, 16 };
    float probeDistance = 2.5f;

    uint32 numCascades = 4;
    uint32 numRaysPerProbe = 16;

    float rayMaxDistance = 1000.0f;
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

class DDGI final
{
public:
    HYP_DEF_POOL_NEW_DELETE(g_renderPool);

    static constexpr uint32 MaxCascades = DDGIMaxCascades;
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

    const Handle<Texture>& GetIrradianceTexture() const
    {
        return m_irradianceTexture;
    }

    const Handle<Texture>& GetVisibilityTexture() const
    {
        return m_visibilityTexture;
    }

    const GpuBufferRef& GetConstantBuffer(uint32 frameIndex) const
    {
        return m_cbuffers[frameIndex];
    }

    void Create();

    void Render(Frame* frame, const RenderSetup& renderSetup);

private:
    struct CascadeState
    {
        Vec3i gridOffset = Vec3i::Zero();
        Vec3i gridOffsetPrev = Vec3i::Zero();
        float probeSpacing = 0.0f;
        float blendAlpha = 0.0f;
        uint32 updateInterval = 1;
        bool needsReset = true;
    };

    uint32 NumProbesPerCascade() const;
    uint32 NumProbesTotal() const;
    Vec2u GetRayDataDimensions() const;

    void InitializeCascades();
    void ScrollCascades(const Vec3f& cameraPosition);

    void CreateConstantBuffers();
    void CreateStorageBuffers();

    void UpdateUniforms(Frame* frame, const RenderSetup& renderSetup);

    DDGIInfo m_gridInfo;

    FixedArray<CascadeState, MaxCascades> m_cascades;
    uint32 m_cascadeUpdateMask;
    uint32 m_cascadeResetMask;

    FixedArray<GpuBufferRef, NumFramesInFlight> m_cbuffers;

    GpuBuffer* m_dynamicCBuffer = nullptr;
    size_t m_dynamicCBufferOffset = 0;
    size_t m_dynamicCBufferSize = 0;

    GpuBufferRef m_radianceBuffer;

    Handle<Texture> m_irradianceTexture;
    Handle<Texture> m_visibilityTexture;

    RotationMatrixGenerator m_randomGenerator;
    uint32 m_counter;
};

} // namespace Hyperion
