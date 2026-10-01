/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Glimmer/GlimmerSpanCache.hpp>
#include <Rendering/Glimmer/GlimmerTLAS.hpp>
#include <Rendering/Glimmer/GlimmerBLASCache.hpp>
#include <Rendering/Glimmer/GlimmerSurfaceCache.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/RenderHelpers.hpp>
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

static_assert(GlimmerSpanMaxRects == 4);

static constexpr uint32 SpanGroupSize = 64;

struct GlimmerSpanSplatConstants
{
    Vec4i window;   // xy = absolute texel of the window origin, z = level, w = rects to fill
    Vec4f params;   // x = texel size, y = 1 / texel size, z = minimum height of foliage above the ground
    Vec4u counts;   // x = span instances, y = span chunks, z = groups along x
    Vec4i rects[GlimmerSpanMaxRects]; // xy = min, zw = max (exclusive)
    GlimmerGroundShaderData ground;
};

#pragma region GlimmerSpanCache

GlimmerSpanCache::GlimmerSpanCache()
    : m_seenGeneration(~0u),
      m_shaderData {}
{
    for (uint32 levelIndex = 0; levelIndex < GlimmerGroundLevels; levelIndex++)
    {
        m_builtGenerations[levelIndex] = ~0u;
        m_builtOrigins[levelIndex] = Vec2i(0, 0);
        m_changedWholeWindow[levelIndex] = true;

        const float texelSize = GetGlimmerGroundTexelSize(levelIndex);
        m_shaderData.levels[levelIndex].params = Vec4f(texelSize, 1.0f / texelSize, 0.0f, 0.0f);
    }
}

GlimmerSpanCache::~GlimmerSpanCache()
{
    EnqueueDeletion(std::move(m_spansBuffer));
}

const GpuBufferRef& GlimmerSpanCache::GetSpansBuffer() const
{
    return m_spansBuffer;
}

bool GlimmerSpanCache::RectList::Add(const Rect& rect)
{
    if (rect.IsEmpty())
    {
        return true;
    }

    if (count == GlimmerSpanMaxRects)
    {
        return false;
    }

    rects[count++] = rect;

    return true;
}

void GlimmerSpanCache::FillLevel(
    Frame* frame,
    uint32 levelIndex,
    const Vec2i& windowOrigin,
    const RectList& rects,
    const GlimmerTLAS& tlas,
    const GlimmerBLASCache& blasCache,
    const GlimmerSurfaceCache& surfaceCache)
{
    HYP_SCOPE;

    CommandRecorder& cr = frame->cr;

    const float texelSize = GetGlimmerGroundTexelSize(levelIndex);

    const auto dispatchPass = [&](const StaticShaderPropertyId& modeProperty, uint32 numThreads)
    {
        const Vec3u groups = helpers::WrapComputeGroupCount((numThreads + SpanGroupSize - 1) / SpanGroupSize);

        GlimmerSpanSplatConstants constants {};
        constants.window = Vec4i(windowOrigin.x, windowOrigin.y, int32(levelIndex), int32(rects.count));
        constants.params = Vec4f(texelSize, 1.0f / texelSize, GlimmerSpansMinFoliageHeight, 0.0f);
        constants.counts = Vec4u(tlas.GetNumSpanInstances(), tlas.GetNumSpanChunks(), groups.x, 0);

        for (uint32 rectIndex = 0; rectIndex < rects.count; rectIndex++)
        {
            const Rect& rect = rects.rects[rectIndex];

            constants.rects[rectIndex] = Vec4i(rect.min.x, rect.min.y, rect.max.x, rect.max.y);
        }

        constants.ground = surfaceCache.GetGroundShaderData();

        GpuBuffer* cbuffer = nullptr;
        size_t cbufferOffset = 0;
        size_t cbufferSize = 0;

        RI.cbufferAllocator->Write(&constants);
        RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

        ShaderPropertySet shaderProperties;
        shaderProperties.Add(modeProperty);

        cr << SetCurrentShader(ShaderDesc(NAME("GlimmerSpanSplat"), shaderProperties));

        uint32 uniformIndex = 0;

        cr << SetShaderUniform(uniformIndex++, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));
        cr << SetShaderUniform(uniformIndex++, "SpansBuffer"_sh, m_spansBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
        cr << SetShaderUniform(uniformIndex++, "SpanInstancesBuffer"_sh, tlas.GetSpanInstancesBuffer().Get(), ShaderDataOffset(0, sizeof(GlimmerSpanInstanceShaderData)));
        cr << SetShaderUniform(uniformIndex++, "SpanChunksBuffer"_sh, tlas.GetSpanChunksBuffer().Get(), ShaderDataOffset(0, sizeof(GlimmerSpanChunkShaderData)));
        cr << SetShaderUniform(uniformIndex++, "GlimmerBLASTrianglesBuffer"_sh, blasCache.GetTrianglesBuffer().Get(), ShaderDataOffset(0, sizeof(GlimmerTriangle)));
        cr << SetShaderUniform(uniformIndex++, "MaterialsBuffer"_sh, RI.namedBuffers[NamedBuffer::Materials]);
        cr << SetShaderUniform(uniformIndex++, "SamplerLinearMipmap"_sh, RI.placeholderData->GetSamplerLinearMipmap());
        cr << SetShaderUniform(uniformIndex++, "GlimmerGroundTexture"_sh, surfaceCache.GetGroundImageView());

        cr << DispatchCompute(groups);

        cr << InsertBarrier(m_spansBuffer.Get(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);
    };

    cr << InsertBarrier(m_spansBuffer.Get(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);

    dispatchPass(s_propSpanModeClear, GlimmerGroundResolution * GlimmerGroundResolution);

    if (tlas.GetNumSpanChunks() != 0)
    {
        // a group per chunk
        dispatchPass(s_propSpanModeSplat, tlas.GetNumSpanChunks() * SpanGroupSize);
    }

    cr << InsertBarrier(m_spansBuffer.Get(), ResourceState::ShaderResource, ShaderModuleType::Compute);

    m_builtOrigins[levelIndex] = windowOrigin;
    m_builtGenerations[levelIndex] = tlas.GetGeneration();

    m_shaderData.levels[levelIndex].window = Vec4i(windowOrigin.x, windowOrigin.y, 1, 0);
}

void GlimmerSpanCache::Update(Frame* frame, const GlimmerChannelState& state, const GlimmerTLAS& tlas, const GlimmerBLASCache& blasCache, const GlimmerSurfaceCache& surfaceCache)
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

    // a swap only has to redo where its span instances changed, on every level; one missed means not knowing where that was
    if (tlas.GetGeneration() != m_seenGeneration)
    {
        const bool isWholeWindow = tlas.IsSpanFullyDirty() || tlas.GetGeneration() != m_seenGeneration + 1;

        for (uint32 levelIndex = 0; levelIndex < GlimmerGroundLevels; levelIndex++)
        {
            const float invTexelSize = 1.0f / GetGlimmerGroundTexelSize(levelIndex);

            m_changedWholeWindow[levelIndex] |= isWholeWindow;

            for (const BoundingBox& bounds : tlas.GetSpanDirtyBounds())
            {
                const Rect rect {
                    Vec2i(int32(MathUtil::Floor(bounds.min.x * invTexelSize)), int32(MathUtil::Floor(bounds.min.z * invTexelSize))),
                    Vec2i(int32(MathUtil::Floor(bounds.max.x * invTexelSize)) + 1, int32(MathUtil::Floor(bounds.max.z * invTexelSize)) + 1)
                };

                m_changedWholeWindow[levelIndex] |= !m_changed[levelIndex].Add(rect);
            }
        }

        m_seenGeneration = tlas.GetGeneration();
    }

    // one level per frame keeps the splat's cost spread out; the finest level is what the near probes see
    for (uint32 levelIndex = 0; levelIndex < GlimmerGroundLevels; levelIndex++)
    {
        const GlimmerGroundLevelState& groundLevel = state.groundLevels[levelIndex];

        const Vec2i windowOrigin = groundLevel.windowOrigin;
        const Rect window = GetGlimmerGroundWindow(windowOrigin);

        const bool isGroundComplete = groundLevel.validMin == windowOrigin
            && groundLevel.validMax == windowOrigin + Vec2i(int32(GlimmerGroundResolution), int32(GlimmerGroundResolution));

        const Vec2i builtOrigin = m_builtOrigins[levelIndex];

        // a jump keeps nothing
        const bool isFullRebuild = m_changedWholeWindow[levelIndex]
            || MathUtil::Abs(windowOrigin.x - builtOrigin.x) >= int32(GlimmerGroundResolution)
            || MathUtil::Abs(windowOrigin.y - builtOrigin.y) >= int32(GlimmerGroundResolution);

        RectList& filledWithoutGround = m_filledWithoutGround[levelIndex];

        RectList rects;
        bool isWholeWindow = isFullRebuild;

        if (!isFullRebuild)
        {
            // only what scrolled in
            Rect columns;
            Rect rows;
            GetGlimmerScrolledRects(GetGlimmerGroundWindow(builtOrigin), window, columns, rows);

            rects.Add(columns);
            rects.Add(rows);

            // what scrolled out is gone; the rest waits for its ground, or goes again now that it's there
            RectList stillWithoutGround;

            for (uint32 rectIndex = 0; rectIndex < filledWithoutGround.count; rectIndex++)
            {
                const Rect rect = Rect::Intersect(filledWithoutGround.rects[rectIndex], window);

                if (isGroundComplete)
                {
                    isWholeWindow |= !rects.Add(rect);
                }
                else
                {
                    isWholeWindow |= !stillWithoutGround.Add(rect);
                }
            }

            filledWithoutGround = stillWithoutGround;

            for (uint32 rectIndex = 0; rectIndex < m_changed[levelIndex].count; rectIndex++)
            {
                isWholeWindow |= !rects.Add(Rect::Intersect(m_changed[levelIndex].rects[rectIndex], window));
            }
        }

        m_changed[levelIndex] = RectList();
        m_changedWholeWindow[levelIndex] = false;

        if (isWholeWindow)
        {
            rects = RectList();
            rects.Add(window);

            filledWithoutGround = RectList();
        }

        if (rects.count == 0)
        {
            continue;
        }

        if (!isGroundComplete)
        {
            for (uint32 rectIndex = 0; rectIndex < rects.count; rectIndex++)
            {
                if (!filledWithoutGround.Add(rects.rects[rectIndex]))
                {
                    filledWithoutGround = RectList();
                    filledWithoutGround.Add(window);

                    break;
                }
            }
        }

        ENGINE_STAT_GPU_SCOPE(&s_statGlimmerSpans);

        FillLevel(frame, levelIndex, windowOrigin, rects, tlas, blasCache, surfaceCache);

        break;
    }
}

#pragma endregion GlimmerSpanCache

} // namespace Hyperion
