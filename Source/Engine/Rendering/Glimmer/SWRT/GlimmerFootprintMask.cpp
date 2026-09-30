/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Glimmer/SWRT/GlimmerFootprintMask.hpp>
#include <Rendering/Glimmer/SWRT/GlimmerTLAS.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/CommandRecorder.hpp>
#include <Rendering/CBufferAllocator.hpp>
#include <Rendering/GpuBuffer.hpp>
#include <Rendering/Frame.hpp>
#include <Rendering/ShaderManager.hpp>

#include <Rendering/Util/DeletionQueue.hpp>
#include <Rendering/Util/ShaderPropertyDictionary.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Profiling/ProfileScope.hpp>

namespace Hyperion {

static StaticShaderPropertyId s_propMaskModeClear { ShaderProperty(NAME("MODE"), NAME("CLEAR")) };
static StaticShaderPropertyId s_propMaskModeRasterize { ShaderProperty(NAME("MODE"), NAME("RASTERIZE")) };
static StaticShaderPropertyId s_propMaskModeReduce { ShaderProperty(NAME("MODE"), NAME("REDUCE")) };

static constexpr uint32 MaskGroupSize = 64;
static constexpr uint32 MaxMaskResolution = 2048;
static constexpr uint32 MaxGroupsPerDimension = 65535;

// Must match GlimmerFootprintMaskConstants in Shaders/Glimmer/SWRT/GlimmerSWRTFootprintMask.hlsl
struct GlimmerFootprintMaskConstants
{
    GlimmerFootprintMaskShaderData mask;
    Vec4u passInfo;
};

static uint32 CalculateNumLevels(uint32 resolution)
{
    uint32 numLevels = 1;

    while ((resolution >> (numLevels - 1)) > 1)
    {
        numLevels++;
    }

    return numLevels;
}

static uint32 CalculateTotalCells(uint32 resolution, uint32 numLevels)
{
    uint32 totalCells = 0;

    for (uint32 level = 0; level < numLevels; level++)
    {
        const uint32 levelResolution = MathUtil::Max(resolution >> level, 1u);
        totalCells += levelResolution * levelResolution;
    }

    return totalCells;
}

GlimmerFootprintMask::GlimmerFootprintMask()
    : m_bufferResolution(0),
      m_shaderData {}
{
}

GlimmerFootprintMask::~GlimmerFootprintMask()
{
    EnqueueDeletion(std::move(m_maskBuffer));
    EnqueueDeletion(std::move(m_placeholderBuffer));
}

const GpuBufferRef& GlimmerFootprintMask::GetMaskBuffer() const
{
    if (m_maskBuffer.IsValid())
    {
        return m_maskBuffer;
    }

    if (!m_placeholderBuffer.IsValid())
    {
        GlimmerFootprintMask* self = const_cast<GlimmerFootprintMask*>(this);

        self->m_placeholderBuffer = RI.MakeGpuBuffer(GpuBufferType::RWStructuredBuffer, sizeof(uint32) * 2, alignof(uint32));
        Check(self->m_placeholderBuffer->Create());
    }

    return m_placeholderBuffer;
}

void GlimmerFootprintMask::EnsureBuffer(uint32 resolution, uint32 numLevels)
{
    if (m_maskBuffer.IsValid() && m_bufferResolution == resolution)
    {
        return;
    }

    EnqueueDeletion(std::move(m_maskBuffer));

    const uint32 totalCells = CalculateTotalCells(resolution, numLevels);

    m_maskBuffer = RI.MakeGpuBuffer(GpuBufferType::RWStructuredBuffer, size_t(totalCells) * 2 * sizeof(uint32), alignof(uint32));
    Check(m_maskBuffer->Create());

#ifdef HYP_RHI_DEBUG_NAMES
    m_maskBuffer->SetDebugName(NAME("GlimmerSWRTFootprintMask"));
#endif

    m_bufferResolution = resolution;
}

void GlimmerFootprintMask::Rebuild(Frame* frame, const GlimmerTLAS& tlas, const Vec2f& regionCenterXZ, float regionRadius)
{
    HYP_SCOPE;

    if (!tlas.IsReady())
    {
        return;
    }

    const float cellSize = GlimmerFootprintMaskCellSize;
    const uint32 resolution = MathUtil::Clamp(uint32(MathUtil::Ceil(2.0f * regionRadius / cellSize)), 1u, MaxMaskResolution);
    const uint32 numLevels = CalculateNumLevels(resolution);
    const uint32 totalCells = CalculateTotalCells(resolution, numLevels);

    EnsureBuffer(resolution, numLevels);

    const float halfExtent = 0.5f * float(resolution) * cellSize;

    m_shaderData.originCellSize = Vec4f(
        MathUtil::Floor((regionCenterXZ.x - halfExtent) / cellSize) * cellSize,
        MathUtil::Floor((regionCenterXZ.y - halfExtent) / cellSize) * cellSize,
        cellSize,
        1.0f);

    m_shaderData.info = Vec4u(resolution, numLevels, 0, 0);

    CommandRecorder& cr = frame->cr;

    // group counts are limited to 65535 per dimension, so large dispatches wrap onto y
    const auto dispatchPass = [&](const StaticShaderPropertyId& modeProperty, Vec4u passInfo, uint32 numGroups)
    {
        numGroups = MathUtil::Max(numGroups, 1u);

        const uint32 groupsX = MathUtil::Min(numGroups, MaxGroupsPerDimension);
        const uint32 groupsY = (numGroups + groupsX - 1) / groupsX;

        passInfo.w = groupsX;

        GlimmerFootprintMaskConstants constants {};
        constants.mask = m_shaderData;
        constants.passInfo = passInfo;

        GpuBuffer* cbuffer = nullptr;
        size_t cbufferOffset = 0;
        size_t cbufferSize = 0;

        RI.cbufferAllocator->Write(&constants);
        RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

        ShaderPropertySet shaderProperties;
        shaderProperties.Add(modeProperty);

        cr << SetCurrentShader(ShaderDesc(NAME("GlimmerSWRTFootprintMask"), shaderProperties));

        cr << SetShaderUniform(0, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));
        cr << SetShaderUniform(1, "FootprintMaskBuffer"_sh, m_maskBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
        cr << SetShaderUniform(2, "GlimmerInstanceBoundsBuffer"_sh, tlas.GetInstanceBoundsBuffer().Get(), ShaderDataOffset(0, sizeof(GlimmerInstanceBoundsShaderData)));

        cr << DispatchCompute(Vec3u { groupsX, groupsY, 1 });

        cr << InsertBarrier(m_maskBuffer.Get(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);
    };

    cr << InsertBarrier(m_maskBuffer.Get(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);

    dispatchPass(s_propMaskModeClear, Vec4u(0, 0, totalCells, 0), (totalCells + MaskGroupSize - 1) / MaskGroupSize);

    if (tlas.GetNumInstances() != 0)
    {
        dispatchPass(s_propMaskModeRasterize, Vec4u(tlas.GetNumInstances(), 0, 0, 0), tlas.GetNumInstances());
    }

    for (uint32 level = 1; level < numLevels; level++)
    {
        const uint32 levelResolution = MathUtil::Max(resolution >> level, 1u);

        dispatchPass(s_propMaskModeReduce, Vec4u(0, level, 0, 0), (levelResolution * levelResolution + MaskGroupSize - 1) / MaskGroupSize);
    }

    cr << InsertBarrier(m_maskBuffer.Get(), ResourceState::ShaderResource, ShaderModuleType::Compute);
}

} // namespace Hyperion
