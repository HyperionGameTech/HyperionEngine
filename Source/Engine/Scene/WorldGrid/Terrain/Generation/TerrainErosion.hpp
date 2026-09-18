/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <Core/Utilities/Span.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Types.hpp>

#include <cmath>

namespace Hyperion {

struct TerrainErosionSettings
{
    uint32 seed = 0;
    float spacing = 2.0f;
    uint32 iterations = 60;
    uint32 routingInterval = 4;
    float erodibility = 0.06f;
    float diffusion = 0.05f;
    float talusSlope = 1.2f;
};

///optional per-sample outputs of TerrainErodeHeightfield - leave a span empty to skip it
struct TerrainErosionOutputs
{
    ///samples draining through each sample, itself included
    Span<float> upstreamSamples;

    ///total material hillslope diffusion and talus collapse moved onto each sample over the run, in world units
    Span<float> depositedDepth;
};

///RGBA8 per sample, generated alongside eroded heights and saved with them
struct TerrainErosionMasks
{
    static constexpr uint32 NumChannels = 4;

    ///log2 of the upstream sample count, over MaxFlowLog2
    static constexpr uint32 FlowChannel = 0;

    ///how much deeper erosion cut than in the surrounding area, log2(1 + world units) over MaxDepthLog2
    static constexpr uint32 IncisionChannel = 1;

    ///material piled up by hillslope transport, encoded like IncisionChannel
    static constexpr uint32 DepositionChannel = 2;

    ///0.5 is flat, above is a hollow - how far the surrounding ring sits above the sample, over the ring's radius
    static constexpr uint32 ConcavityChannel = 3;

    static constexpr float MaxFlowLog2 = 16.0f;
    static constexpr float MaxDepthLog2 = 5.0f;
    static constexpr float ConcavityRange = 0.5f;

    ///in erosion samples
    static constexpr int32 ConcavityRadius = 4;
    static constexpr int32 IncisionRadius = 16;

    static HYP_FORCE_INLINE float DecodeFlowLog2(ubyte value)
    {
        return float(value) / 255.0f * MaxFlowLog2;
    }

    static HYP_FORCE_INLINE float DecodeDepth(ubyte value)
    {
        return MathUtil::Pow(2.0f, float(value) / 255.0f * MaxDepthLog2) - 1.0f;
    }

    ///-ConcavityRange (ridge) to ConcavityRange (hollow)
    static HYP_FORCE_INLINE float DecodeConcavity(ubyte value)
    {
        return (float(value) / 255.0f - 0.5f) * 2.0f * ConcavityRange;
    }

    static HYP_FORCE_INLINE float EncodeFlowLog2(float flowLog2)
    {
        return MathUtil::Clamp(flowLog2 / MaxFlowLog2, 0.0f, 1.0f);
    }

    static HYP_FORCE_INLINE float EncodeDepth(float depth)
    {
        return MathUtil::Clamp(std::log2(1.0f + MathUtil::Max(depth, 0.0f)) / MaxDepthLog2, 0.0f, 1.0f);
    }

    static HYP_FORCE_INLINE float EncodeConcavity(float concavity)
    {
        return MathUtil::Clamp(0.5f + concavity / (2.0f * ConcavityRange), 0.0f, 1.0f);
    }

    ///masks for a sample erosion has never touched
    static void FillNeutral(Span<ubyte> outMasks);
};

void TerrainErodeHeightfield(
    Span<float> heights,
    uint32 size,
    int32 originX,
    int32 originZ,
    const TerrainErosionSettings& settings,
    const TerrainErosionOutputs& outputs = {});

///encodes TerrainErosionMasks for every sample of a heightfield eroded from \p baseHeights
void TerrainBuildErosionMasks(
    Span<const float> baseHeights,
    Span<const float> erodedHeights,
    Span<const float> upstreamSamples,
    Span<const float> depositedDepth,
    uint32 size,
    float spacing,
    Span<ubyte> outMasks);

} // namespace Hyperion
