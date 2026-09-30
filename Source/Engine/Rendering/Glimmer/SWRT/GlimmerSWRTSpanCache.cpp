/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Glimmer/SWRT/GlimmerSWRTSpanCache.hpp>
#include <Rendering/Glimmer/SWRT/GlimmerTLAS.hpp>
#include <Rendering/Glimmer/SWRT/GlimmerBLASCache.hpp>
#include <Rendering/Glimmer/GlimmerSurfaceCache.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/CommandRecorder.hpp>
#include <Rendering/CBufferAllocator.hpp>
#include <Rendering/GpuBuffer.hpp>
#include <Rendering/PlaceholderData.hpp>
#include <Rendering/ShaderManager.hpp>
#include <Rendering/Frame.hpp>

#include <Rendering/Util/DeletionQueue.hpp>
#include <Rendering/Util/ShaderPropertyDictionary.hpp>

#include <Framework/EngineStats.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Profiling/ProfileScope.hpp>

namespace Hyperion {

static EngineStatGpuTimer s_statGlimmerSpans("Rendering/GPU/Glimmer/Spans");

static StaticShaderPropertyId s_propSpanModeClear { ShaderProperty(NAME("MODE"), NAME("CLEAR")) };
static StaticShaderPropertyId s_propSpanModeSplat { ShaderProperty(NAME("MODE"), NAME("SPLAT")) };

static constexpr uint32 SpanGroupSize = 64;
static constexpr uint32 MaxGroupsPerDimension = 65535;

// Must match GlimmerSpanSplatConstants in Shaders/Glimmer/SWRT/GlimmerSWRTSpanSplat.hlsl
struct GlimmerSpanSplatConstants
{
    Vec4i window;   // xy = absolute texel of the window origin, z = level
    Vec4f params;   // x = texel size, y = 1 / texel size, z = minimum height of foliage above the ground
    Vec4u counts;   // x = span instances, y = span triangles, z = groups along x
    GlimmerGroundShaderData ground;
};

GlimmerSWRTSpanCache::GlimmerSWRTSpanCache()
    : m_shaderData {}
{
    for (uint32 levelIndex = 0; levelIndex < GlimmerGroundLevels; levelIndex++)
    {
        m_builtGenerations[levelIndex] = ~0u;
        m_builtOrigins[levelIndex] = Vec2i(0, 0);
        m_builtWithCompleteGround[levelIndex] = false;

        const float texelSize = GetGlimmerGroundTexelSize(levelIndex);
        m_shaderData.levels[levelIndex].params = Vec4f(texelSize, 1.0f / texelSize, 0.0f, 0.0f);
    }
}

GlimmerSWRTSpanCache::~GlimmerSWRTSpanCache()
{
    EnqueueDeletion(std::move(m_spansBuffer));
}

const GpuBufferRef& GlimmerSWRTSpanCache::GetSpansBuffer() const
{
    return m_spansBuffer;
}

void GlimmerSWRTSpanCache::RebuildLevel(Frame* frame, uint32 levelIndex, const Vec2i& windowOrigin, const GlimmerTLAS& tlas, const GlimmerBLASCache& blasCache, const GlimmerSurfaceCache& surfaceCache)
{
    HYP_SCOPE;

    CommandRecorder& cr = frame->cr;

    const float texelSize = GetGlimmerGroundTexelSize(levelIndex);

    const auto dispatchPass = [&](const StaticShaderPropertyId& modeProperty, uint32 numThreads)
    {
        const uint32 numGroups = MathUtil::Max((numThreads + SpanGroupSize - 1) / SpanGroupSize, 1u);
        const uint32 groupsX = MathUtil::Min(numGroups, MaxGroupsPerDimension);
        const uint32 groupsY = (numGroups + groupsX - 1) / groupsX;

        GlimmerSpanSplatConstants constants {};
        constants.window = Vec4i(windowOrigin.x, windowOrigin.y, int32(levelIndex), 0);
        constants.params = Vec4f(texelSize, 1.0f / texelSize, GlimmerSpansMinFoliageHeight, 0.0f);
        constants.counts = Vec4u(tlas.GetNumSpanInstances(), tlas.GetNumSpanTriangles(), groupsX, 0);
        constants.ground = surfaceCache.GetGroundShaderData();

        GpuBuffer* cbuffer = nullptr;
        size_t cbufferOffset = 0;
        size_t cbufferSize = 0;

        RI.cbufferAllocator->Write(&constants);
        RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

        ShaderPropertySet shaderProperties;
        shaderProperties.Add(modeProperty);

        cr << SetCurrentShader(ShaderDesc(NAME("GlimmerSWRTSpanSplat"), shaderProperties));

        uint32 uniformIndex = 0;

        cr << SetShaderUniform(uniformIndex++, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));
        cr << SetShaderUniform(uniformIndex++, "SpansBuffer"_sh, m_spansBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
        cr << SetShaderUniform(uniformIndex++, "SpanInstancesBuffer"_sh, tlas.GetSpanInstancesBuffer().Get(), ShaderDataOffset(0, sizeof(GlimmerSpanInstanceShaderData)));
        cr << SetShaderUniform(uniformIndex++, "SpanTriangleOffsetsBuffer"_sh, tlas.GetSpanTriangleOffsetsBuffer().Get(), ShaderDataOffset(0, sizeof(uint32)));
        cr << SetShaderUniform(uniformIndex++, "GlimmerBLASTrianglesBuffer"_sh, blasCache.GetTrianglesBuffer().Get(), ShaderDataOffset(0, sizeof(GlimmerTriangle)));
        cr << SetShaderUniform(uniformIndex++, "MaterialsBuffer"_sh, RI.namedBuffers[NamedBuffer::Materials]);
        cr << SetShaderUniform(uniformIndex++, "SamplerLinearMipmap"_sh, RI.placeholderData->GetSamplerLinearMipmap());
        cr << SetShaderUniform(uniformIndex++, "GlimmerGroundTexture"_sh, surfaceCache.GetGroundImageView());

        cr << DispatchCompute(Vec3u { groupsX, groupsY, 1 });

        cr << InsertBarrier(m_spansBuffer.Get(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);
    };

    cr << InsertBarrier(m_spansBuffer.Get(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);

    dispatchPass(s_propSpanModeClear, GlimmerGroundResolution * GlimmerGroundResolution);

    if (tlas.GetNumSpanTriangles() != 0)
    {
        dispatchPass(s_propSpanModeSplat, tlas.GetNumSpanTriangles());
    }

    cr << InsertBarrier(m_spansBuffer.Get(), ResourceState::ShaderResource, ShaderModuleType::Compute);

    m_builtOrigins[levelIndex] = windowOrigin;
    m_builtGenerations[levelIndex] = tlas.GetGeneration();

    m_shaderData.levels[levelIndex].window = Vec4i(windowOrigin.x, windowOrigin.y, 1, 0);
}

void GlimmerSWRTSpanCache::Update(Frame* frame, const GlimmerChannelState& state, const GlimmerTLAS& tlas, const GlimmerBLASCache& blasCache, const GlimmerSurfaceCache& surfaceCache)
{
    HYP_SCOPE;

    if (!tlas.IsReady() || !blasCache.IsReady() || !tlas.GetSpanInstancesBuffer().IsValid())
    {
        return;
    }

    if (!m_spansBuffer.IsValid())
    {
        const size_t numValues = size_t(GlimmerGroundLevels) * GlimmerGroundResolution * GlimmerGroundResolution * GlimmerSpanValuesPerTexel;

        m_spansBuffer = RI.MakeGpuBuffer(GpuBufferType::RWStructuredBuffer, numValues * sizeof(uint32), alignof(uint32));
        Check(m_spansBuffer->Create());
    }

    // one level per frame keeps the splat's cost spread out; the finest level is what the near probes see
    for (uint32 levelIndex = 0; levelIndex < GlimmerGroundLevels; levelIndex++)
    {
        const Vec2i windowOrigin = state.groundLevels[levelIndex].windowOrigin;

        const GlimmerGroundLevelState& groundLevel = state.groundLevels[levelIndex];
        const bool isGroundComplete = groundLevel.validMin == windowOrigin
            && groundLevel.validMax == windowOrigin + Vec2i(int32(GlimmerGroundResolution), int32(GlimmerGroundResolution));

        if (m_builtGenerations[levelIndex] == tlas.GetGeneration() && m_builtOrigins[levelIndex] == windowOrigin
            && (m_builtWithCompleteGround[levelIndex] || !isGroundComplete))
        {
            continue;
        }

        m_builtWithCompleteGround[levelIndex] = isGroundComplete;

        ENGINE_STAT_GPU_SCOPE(&s_statGlimmerSpans);

        RebuildLevel(frame, levelIndex, windowOrigin, tlas, blasCache, surfaceCache);

        break;
    }
}

} // namespace Hyperion
