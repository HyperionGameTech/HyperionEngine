/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <Scene/WorldGrid/Terrain/Generation/TerrainErosion.hpp>
#include <Scene/WorldGrid/Terrain/Generation/TerrainNoise.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Math/MathUtil.hpp>
#include <Core/Memory/Memory.hpp>
#include <Core/Profiling/ProfileScope.hpp>

#include <algorithm>
#include <cmath>

namespace Hyperion {

namespace {

constexpr int32 NeighborOffsetX[8] = { -1, 0, 1, -1, 1, -1, 0, 1 };
constexpr int32 NeighborOffsetZ[8] = { -1, -1, -1, 0, 0, 1, 1, 1 };
constexpr float NeighborDistance[8] = { 1.41421356f, 1.0f, 1.41421356f, 1.0f, 1.0f, 1.41421356f, 1.0f, 1.41421356f };

struct FloodEntry
{
    float height;
    int32 index;
};

HYP_FORCE_INLINE bool FloodEntryGreater(const FloodEntry& a, const FloodEntry& b)
{
    return a.height > b.height || (a.height == b.height && a.index > b.index);
}

} // namespace

void TerrainErosionMasks::FillNeutral(Span<ubyte> outMasks)
{
    for (size_t sampleIndex = 0; sampleIndex + NumChannels <= outMasks.Size(); sampleIndex += NumChannels)
    {
        outMasks[sampleIndex + FlowChannel] = 0;
        outMasks[sampleIndex + IncisionChannel] = 0;
        outMasks[sampleIndex + DepositionChannel] = 0;
        outMasks[sampleIndex + ConcavityChannel] = 128;
    }
}

void TerrainErodeHeightfield(
    Span<float> heights,
    uint32 size,
    int32 originX,
    int32 originZ,
    const TerrainErosionSettings& settings,
    const TerrainErosionOutputs& outputs)
{
    const size_t count = size_t(size) * size_t(size);

    Assert(heights.Size() == count, "Heightfield size mismatch");
    const bool outputUpstreamSamples = outputs.upstreamSamples.Size() != 0;
    const bool outputDepositedDepth = outputs.depositedDepth.Size() != 0;

    Assert(!outputUpstreamSamples || outputs.upstreamSamples.Size() == count, "Upstream samples size mismatch");
    Assert(!outputDepositedDepth || outputs.depositedDepth.Size() == count, "Deposited depth size mismatch");

    for (size_t i = 0; i < outputs.upstreamSamples.Size(); i++)
    {
        outputs.upstreamSamples[i] = 1.0f;
    }

    for (size_t i = 0; i < outputs.depositedDepth.Size(); i++)
    {
        outputs.depositedDepth[i] = 0.0f;
    }

    if (size < 3 || settings.iterations == 0)
    {
        return;
    }

    const int32 sizeSigned = int32(size);
    const float cellArea = settings.spacing * settings.spacing;
    const uint32 routingInterval = MathUtil::Max(settings.routingInterval, 1u);

    Array<float> filled;
    filled.Resize(count);

    Array<int32> receivers;
    receivers.Resize(count);

    // erodibility * sqrt(drainage area) / receiver distance, cached between routing updates
    Array<float> erosionFactors;
    erosionFactors.Resize(count);

    Array<float> drainageArea;
    drainageArea.Resize(count);

    Array<int32> floodOrder;
    floodOrder.Resize(count);

    Array<uint8> visited;
    visited.Resize(count);

    Array<float> previousHeights;
    previousHeights.Resize(count);

    Array<FloodEntry> heap;
    heap.Reserve(size_t(size) * 8u);

    const auto isBorder = [sizeSigned](int32 x, int32 z)
    {
        return x == 0 || z == 0 || x == sizeSigned - 1 || z == sizeSigned - 1;
    };

    for (uint32 iteration = 0; iteration < settings.iterations; iteration++)
    {
        if (iteration % routingInterval == 0)
        {
            // priority flood from the border fills pits with a tiny gradient, so water can route through them;
            // the fill only drives routing - the actual heights keep their basins
            Memory::Copy(filled.Data(), heights.Data(), count * sizeof(float));
            Memory::Zero(visited.Data(), count);

            heap.Clear();

            for (int32 z = 0; z < sizeSigned; z++)
            {
                for (int32 x = 0; x < sizeSigned; x++)
                {
                    if (!isBorder(x, z))
                    {
                        continue;
                    }

                    const int32 index = z * sizeSigned + x;

                    visited[index] = 1;
                    heap.PushBack(FloodEntry { filled[index], index });
                }
            }

            std::make_heap(heap.Begin(), heap.End(), FloodEntryGreater);

            size_t orderCount = 0;

            while (heap.Any())
            {
                std::pop_heap(heap.Begin(), heap.End(), FloodEntryGreater);
                const FloodEntry entry = heap.PopBack();

                floodOrder[orderCount++] = entry.index;

                const int32 entryX = entry.index % sizeSigned;
                const int32 entryZ = entry.index / sizeSigned;

                for (uint32 n = 0; n < 8; n++)
                {
                    const int32 neighborX = entryX + NeighborOffsetX[n];
                    const int32 neighborZ = entryZ + NeighborOffsetZ[n];

                    if (neighborX < 0 || neighborZ < 0 || neighborX >= sizeSigned || neighborZ >= sizeSigned)
                    {
                        continue;
                    }

                    const int32 neighborIndex = neighborZ * sizeSigned + neighborX;

                    if (visited[neighborIndex])
                    {
                        continue;
                    }

                    visited[neighborIndex] = 1;
                    filled[neighborIndex] = MathUtil::Max(filled[neighborIndex], entry.height + 1e-4f);

                    heap.PushBack(FloodEntry { filled[neighborIndex], neighborIndex });
                    std::push_heap(heap.Begin(), heap.End(), FloodEntryGreater);
                }
            }

            // steepest descent receivers; the slope jitter breaks up the grid-aligned channels plain D8 produces
            for (int32 z = 0; z < sizeSigned; z++)
            {
                for (int32 x = 0; x < sizeSigned; x++)
                {
                    const int32 index = z * sizeSigned + x;

                    receivers[index] = -1;
                    erosionFactors[index] = 0.0f;

                    if (isBorder(x, z))
                    {
                        continue;
                    }

                    const uint32 hash = TerrainHashCoord(settings.seed ^ (0x6C8E9CF5u + iteration * 0x9E3779B9u), originX + x, originZ + z);

                    float steepestSlope = 0.0f;

                    for (uint32 n = 0; n < 8; n++)
                    {
                        const int32 neighborIndex = (z + NeighborOffsetZ[n]) * sizeSigned + (x + NeighborOffsetX[n]);

                        const float jitter = 0.6f + 0.4f * float((hash >> (n * 4u)) & 0xFu) / 15.0f;
                        const float slope = (filled[index] - filled[neighborIndex]) / NeighborDistance[n] * jitter;

                        if (slope > steepestSlope)
                        {
                            steepestSlope = slope;
                            receivers[index] = neighborIndex;
                            erosionFactors[index] = NeighborDistance[n] * settings.spacing;
                        }
                    }
                }
            }

            for (size_t i = 0; i < count; i++)
            {
                drainageArea[i] = cellArea;
            }

            // the flood visits nodes in ascending filled height, so walking it backwards passes area downstream in one sweep
            for (size_t i = count; i-- > 0;)
            {
                const int32 index = floodOrder[i];

                if (receivers[index] >= 0)
                {
                    drainageArea[receivers[index]] += drainageArea[index];
                }
            }

            for (size_t i = 0; i < count; i++)
            {
                if (receivers[i] >= 0)
                {
                    erosionFactors[i] = settings.erodibility * std::sqrt(drainageArea[i]) / erosionFactors[i];
                }
            }
        }

        // implicit stream power update, downstream first so every receiver is already eroded
        for (size_t i = 0; i < count; i++)
        {
            const int32 index = floodOrder[i];
            const int32 receiver = receivers[index];

            if (receiver < 0 || heights[index] <= heights[receiver])
            {
                continue;
            }

            const float factor = erosionFactors[index];
            const float eroded = (heights[index] + factor * heights[receiver]) / (1.0f + factor);

            heights[index] = MathUtil::Max(eroded, heights[receiver]);
        }

        // hillslope diffusion plus talus collapse, symmetric per neighbor pair so it only writes the center
        Memory::Copy(previousHeights.Data(), heights.Data(), count * sizeof(float));

        for (int32 z = 1; z < sizeSigned - 1; z++)
        {
            for (int32 x = 1; x < sizeSigned - 1; x++)
            {
                const int32 index = z * sizeSigned + x;
                const float height = previousHeights[index];

                const float laplacian = previousHeights[index - 1]
                    + previousHeights[index + 1]
                    + previousHeights[index - sizeSigned]
                    + previousHeights[index + sizeSigned]
                    - 4.0f * height;

                float change = settings.diffusion * laplacian * 0.25f;

                for (uint32 n = 0; n < 8; n++)
                {
                    const int32 neighborIndex = (z + NeighborOffsetZ[n]) * sizeSigned + (x + NeighborOffsetX[n]);

                    const float difference = height - previousHeights[neighborIndex];
                    const float excess = MathUtil::Abs(difference) - settings.talusSlope * NeighborDistance[n] * settings.spacing;

                    if (excess > 0.0f)
                    {
                        change -= MathUtil::Sign(difference) * excess * 0.0625f;
                    }
                }

                heights[index] = height + change;

                if (outputDepositedDepth && change > 0.0f)
                {
                    outputs.depositedDepth[index] += change;
                }
            }
        }
    }

    if (outputUpstreamSamples)
    {
        for (size_t i = 0; i < count; i++)
        {
            outputs.upstreamSamples[i] = drainageArea[i] / cellArea;
        }
    }
}

// mean of values within radius (clamped at the edges), from a summed area table
static void BoxBlurHeightfield(Span<const float> values, uint32 size, int32 radius, Array<float>& outBlurred)
{
    const int32 sizeSigned = int32(size);
    const size_t tablePitch = size_t(size) + 1;

    Array<double> summedArea;
    summedArea.Resize(tablePitch * tablePitch);
    Memory::Zero(summedArea.Data(), summedArea.Size() * sizeof(double));

    for (int32 z = 0; z < sizeSigned; z++)
    {
        double rowSum = 0.0;

        for (int32 x = 0; x < sizeSigned; x++)
        {
            rowSum += double(values[size_t(z) * size + size_t(x)]);

            summedArea[size_t(z + 1) * tablePitch + size_t(x + 1)] = summedArea[size_t(z) * tablePitch + size_t(x + 1)] + rowSum;
        }
    }

    outBlurred.Resize(size_t(size) * size_t(size));

    for (int32 z = 0; z < sizeSigned; z++)
    {
        const int32 minZ = MathUtil::Max(z - radius, 0);
        const int32 maxZ = MathUtil::Min(z + radius, sizeSigned - 1);

        for (int32 x = 0; x < sizeSigned; x++)
        {
            const int32 minX = MathUtil::Max(x - radius, 0);
            const int32 maxX = MathUtil::Min(x + radius, sizeSigned - 1);

            const double sum = summedArea[size_t(maxZ + 1) * tablePitch + size_t(maxX + 1)]
                - summedArea[size_t(minZ) * tablePitch + size_t(maxX + 1)]
                - summedArea[size_t(maxZ + 1) * tablePitch + size_t(minX)]
                + summedArea[size_t(minZ) * tablePitch + size_t(minX)];

            const double sampleCount = double((maxX - minX + 1) * (maxZ - minZ + 1));

            outBlurred[size_t(z) * size + size_t(x)] = float(sum / sampleCount);
        }
    }
}

void TerrainBuildErosionMasks(
    Span<const float> baseHeights,
    Span<const float> erodedHeights,
    Span<const float> upstreamSamples,
    Span<const float> depositedDepth,
    uint32 size,
    float spacing,
    Span<ubyte> outMasks)
{
    HYP_SCOPE;

    const size_t count = size_t(size) * size_t(size);

    Assert(baseHeights.Size() == count && erodedHeights.Size() == count, "Heightfield size mismatch");
    Assert(upstreamSamples.Size() == count && depositedDepth.Size() == count, "Erosion output size mismatch");
    Assert(outMasks.Size() == count * TerrainErosionMasks::NumChannels, "Erosion masks size mismatch");

    const int32 sizeSigned = int32(size);

    // stream power lowers the whole region toward its fixed border, so only erosion deeper than its surroundings marks a gully
    Array<float> carvedDepth;
    carvedDepth.Resize(count);

    for (size_t i = 0; i < count; i++)
    {
        carvedDepth[i] = MathUtil::Max(baseHeights[i] - erodedHeights[i], 0.0f);
    }

    Array<float> surroundingCarvedDepth;
    BoxBlurHeightfield(carvedDepth, size, TerrainErosionMasks::IncisionRadius, surroundingCarvedDepth);

    const auto erodedHeightAt = [&](int32 x, int32 z) -> float
    {
        return erodedHeights[size_t(MathUtil::Clamp(z, 0, sizeSigned - 1)) * size + size_t(MathUtil::Clamp(x, 0, sizeSigned - 1))];
    };

    const int32 ringRadius = TerrainErosionMasks::ConcavityRadius;
    const float ringDistance = float(ringRadius) * spacing;

    const auto toByte = [](float value) -> ubyte
    {
        return ubyte(value * 255.0f + 0.5f);
    };

    for (int32 z = 0; z < sizeSigned; z++)
    {
        for (int32 x = 0; x < sizeSigned; x++)
        {
            const size_t index = size_t(z) * size + size_t(x);

            const float ringMean = (erodedHeightAt(x - ringRadius, z - ringRadius)
                + erodedHeightAt(x, z - ringRadius)
                + erodedHeightAt(x + ringRadius, z - ringRadius)
                + erodedHeightAt(x - ringRadius, z)
                + erodedHeightAt(x + ringRadius, z)
                + erodedHeightAt(x - ringRadius, z + ringRadius)
                + erodedHeightAt(x, z + ringRadius)
                + erodedHeightAt(x + ringRadius, z + ringRadius))
                / 8.0f;

            const float concavity = (ringMean - erodedHeights[index]) / ringDistance;
            const float incision = carvedDepth[index] - surroundingCarvedDepth[index];

            ubyte* masks = outMasks.Data() + index * TerrainErosionMasks::NumChannels;

            masks[TerrainErosionMasks::FlowChannel] = toByte(TerrainErosionMasks::EncodeFlowLog2(std::log2(MathUtil::Max(upstreamSamples[index], 1.0f))));
            masks[TerrainErosionMasks::IncisionChannel] = toByte(TerrainErosionMasks::EncodeDepth(incision));
            masks[TerrainErosionMasks::DepositionChannel] = toByte(TerrainErosionMasks::EncodeDepth(depositedDepth[index]));
            masks[TerrainErosionMasks::ConcavityChannel] = toByte(TerrainErosionMasks::EncodeConcavity(concavity));
        }
    }
}

} // namespace Hyperion
