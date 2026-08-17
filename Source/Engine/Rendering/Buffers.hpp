/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Memory/Allocator/SlabAllocator.hpp>
#include <Core/Memory/Memory.hpp>
#include <Core/Memory/Pimpl.hpp>

#include <Core/Threading/DataRaceDetector.hpp>

#include <Core/Containers/String.hpp>
#include <Core/Containers/FixedArray.hpp>

#include <Core/Utilities/IndexAllocator.hpp>
#include <Core/Utilities/Range.hpp>

#include <Core/Reflection/TypeInfoFwd.hpp>

#include <Core/Defines.hpp>

#include <Rendering/RenderTypes.hpp>
#include <Rendering/Shared.hpp>
#include <Rendering/GpuBuffer.hpp>
#include <Rendering/Frame.hpp>

#include <Core/Math/Mat4f.hpp>

#include <Core/Constants.hpp>
#include <Core/Types.hpp>

namespace Hyperion {

struct alignas(16) ParticleShaderData
{
    Vec4f position;   //   4 x 4 = 16
    Vec4f velocity;   // + 4 x 4 = 32
    Vec4f color;      // + 4 x 4 = 48
    Vec4f attributes; // + 4 x 4 = 64
};

static_assert(sizeof(ParticleShaderData) == 64);

struct CubemapUniforms
{
    Mat4f projectionMatrices[6];
    Mat4f viewMatrices[6];
};

static_assert(sizeof(CubemapUniforms) % 256 == 0);

struct ImmediateDrawShaderData
{
    Mat4f transform;
    uint32 colorPacked;
    uint32 envProbeType;
    uint32 envProbeIndex;
    uint32 idx; // ~0u == culled
};

static_assert(sizeof(ImmediateDrawShaderData) == 80);

struct SH9Buffer
{
    Vec4f values[16];
};

static_assert(sizeof(SH9Buffer) == 256);

struct SHTile
{
    Vec4f coeffsWeights[9];
};

static_assert(sizeof(SHTile) == 144);

struct VoxelUniforms
{
    Vec4f extent;
    Vec4f aabbMax;
    Vec4f aabbMin;
    Vec4u dimensions; // num mipmaps stored in w component
};

static_assert(sizeof(VoxelUniforms) == 64);

struct RayTracingConstants
{
    uint32 numBoundLights;
    uint32 numBoundEnvProbes;
    uint32 rayOffset; // for lightmapper
    float minRoughness;

    Vec2i outputImageResolution;
    float maxDistance;
};

class StagingBufferPool
{
public:
    StagingBufferPool();

    void OnFrameStart(uint32 newFrameIndex);
    void OnFrameEnd(uint32 prevFrameIndex);

    GpuBuffer* AcquireStagingBuffer(size_t bufferSize);

private:
    Pimpl<struct StagingBufferPoolImpl> m_impl;
};

} // namespace Hyperion
