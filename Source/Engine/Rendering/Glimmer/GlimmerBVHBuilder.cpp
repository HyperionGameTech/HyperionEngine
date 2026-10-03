/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Glimmer/GlimmerBVHBuilder.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Profiling/ProfileScope.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace Hyperion {

static constexpr uint32 NumSplitBins = 16;
static constexpr uint32 MaxSahSplitDepth = 32;

static HYP_FORCE_INLINE uint32 GetSplitBinIndex(float centroid, float binMin, float binScale)
{
    return MathUtil::Min(uint32((centroid - binMin) * binScale), NumSplitBins - 1);
}

class GlimmerBVHBuildContext
{
public:
    GlimmerBVHBuildContext(Span<const BoundingBox> primitiveBounds, const GlimmerBVHBuildParams& params)
        : m_primitiveBounds(primitiveBounds),
          m_params(params)
    {
        const uint32 numPrimitives = uint32(primitiveBounds.Size());

        m_primitiveCentroids.Resize(numPrimitives);
        m_primitiveOrder.Resize(numPrimitives);

        BoundingBox sceneBounds;

        for (uint32 primitiveIndex = 0; primitiveIndex < numPrimitives; primitiveIndex++)
        {
            const BoundingBox& bounds = primitiveBounds[primitiveIndex];

            m_primitiveCentroids[primitiveIndex] = (bounds.min + bounds.max) * 0.5f;
            m_primitiveOrder[primitiveIndex] = primitiveIndex;

            sceneBounds = sceneBounds.Union(bounds);
        }

        const float sceneMagnitude = MathUtil::Max(
            MathUtil::Max(MathUtil::Abs(sceneBounds.min.x), MathUtil::Abs(sceneBounds.min.y), MathUtil::Abs(sceneBounds.min.z)),
            MathUtil::Max(MathUtil::Abs(sceneBounds.max.x), MathUtil::Abs(sceneBounds.max.y), MathUtil::Abs(sceneBounds.max.z)),
            1.0f);

        m_boundsPadding = sceneMagnitude * 1e-5f;

        m_params.minLeafSize = MathUtil::Max(m_params.minLeafSize, 1u);
        m_params.maxLeafSize = MathUtil::Max(m_params.maxLeafSize, m_params.minLeafSize);
    }

    void Build(Array<GlimmerBVHNode>& outNodes, Array<uint32>& outPrimitiveOrder)
    {
        const uint32 numPrimitives = uint32(m_primitiveOrder.Size());

        m_nodes.Clear();

        if (numPrimitives == 1)
        {
            const ChildReference leaf { m_primitiveBounds[0], 0, 1 };

            GlimmerBVHNode& root = m_nodes.EmplaceBack();
            WriteChild(leaf, root.leftMin, root.leftMax, root.leftIndex, root.leftCount);
            WriteChild(leaf, root.rightMin, root.rightMax, root.rightIndex, root.rightCount);
        }
        else if (numPrimitives > 1)
        {
            uint32 middle = 0;

            if (!FindSplit(0, numPrimitives, ComputeBounds(0, numPrimitives), 0, true, middle))
            {
                middle = SplitAtMedian(0, numPrimitives, ComputeCentroidBounds(0, numPrimitives));
            }

            BuildNode(0, middle, numPrimitives, 0);
        }

        outNodes = std::move(m_nodes);
        outPrimitiveOrder = std::move(m_primitiveOrder);
    }

private:
    struct ChildReference
    {
        BoundingBox bounds;
        uint32 index;
        uint32 count;
    };

    struct SplitBin
    {
        BoundingBox bounds;
        uint32 count = 0;
    };

    BoundingBox ComputeBounds(uint32 begin, uint32 end) const
    {
        BoundingBox bounds;

        for (uint32 orderIndex = begin; orderIndex < end; orderIndex++)
        {
            bounds = bounds.Union(m_primitiveBounds[m_primitiveOrder[orderIndex]]);
        }

        return bounds;
    }

    BoundingBox ComputeCentroidBounds(uint32 begin, uint32 end) const
    {
        BoundingBox centroidBounds;

        for (uint32 orderIndex = begin; orderIndex < end; orderIndex++)
        {
            centroidBounds = centroidBounds.Union(m_primitiveCentroids[m_primitiveOrder[orderIndex]]);
        }

        return centroidBounds;
    }

    bool FindSplit(uint32 begin, uint32 end, const BoundingBox& bounds, uint32 depth, bool mustSplit, uint32& outMiddle)
    {
        const uint32 count = end - begin;
        const BoundingBox centroidBounds = ComputeCentroidBounds(begin, end);

        if (depth < MaxSahSplitDepth)
        {
            const float parentArea = MathUtil::Max(bounds.GetSurfaceArea(), 1e-20f);

            uint32 bestAxis = ~0u;
            uint32 bestBin = 0;
            float bestCost = MathUtil::MaxSafeValue<float>();

            for (uint32 axis = 0; axis < 3; axis++)
            {
                const float centroidExtent = centroidBounds.max[axis] - centroidBounds.min[axis];

                if (!(centroidExtent > 0.0f))
                {
                    continue;
                }

                const float binScale = float(NumSplitBins) / centroidExtent;

                for (SplitBin& bin : m_bins)
                {
                    bin = SplitBin {};
                }

                for (uint32 orderIndex = begin; orderIndex < end; orderIndex++)
                {
                    const uint32 primitiveIndex = m_primitiveOrder[orderIndex];

                    SplitBin& bin = m_bins[GetSplitBinIndex(m_primitiveCentroids[primitiveIndex][axis], centroidBounds.min[axis], binScale)];
                    bin.bounds = bin.bounds.Union(m_primitiveBounds[primitiveIndex]);
                    bin.count++;
                }

                // Split i puts bins [0, i] on the left and [i + 1, NumSplitBins) on the right
                float rightAreas[NumSplitBins - 1];
                uint32 rightCounts[NumSplitBins - 1];

                BoundingBox rightBounds;
                uint32 rightCount = 0;

                for (uint32 binIndex = NumSplitBins - 1; binIndex > 0; binIndex--)
                {
                    rightBounds = rightBounds.Union(m_bins[binIndex].bounds);
                    rightCount += m_bins[binIndex].count;

                    rightAreas[binIndex - 1] = rightBounds.GetSurfaceArea();
                    rightCounts[binIndex - 1] = rightCount;
                }

                BoundingBox leftBounds;
                uint32 leftCount = 0;

                for (uint32 splitIndex = 0; splitIndex < NumSplitBins - 1; splitIndex++)
                {
                    leftBounds = leftBounds.Union(m_bins[splitIndex].bounds);
                    leftCount += m_bins[splitIndex].count;

                    if (leftCount == 0 || rightCounts[splitIndex] == 0)
                    {
                        continue;
                    }

                    const float cost = m_params.traversalCost
                        + (leftBounds.GetSurfaceArea() * float(leftCount) + rightAreas[splitIndex] * float(rightCounts[splitIndex])) / parentArea;

                    if (cost < bestCost)
                    {
                        bestCost = cost;
                        bestAxis = axis;
                        bestBin = splitIndex;
                    }
                }
            }

            if (bestAxis != ~0u)
            {
                if (!mustSplit && count <= m_params.maxLeafSize && bestCost >= float(count))
                {
                    return false;
                }

                const float binMin = centroidBounds.min[bestAxis];
                const float binScale = float(NumSplitBins) / (centroidBounds.max[bestAxis] - binMin);

                uint32* partitionPoint = std::partition(
                    m_primitiveOrder.Data() + begin,
                    m_primitiveOrder.Data() + end,
                    [this, bestAxis, bestBin, binMin, binScale](uint32 primitiveIndex)
                    {
                        return GetSplitBinIndex(m_primitiveCentroids[primitiveIndex][bestAxis], binMin, binScale) <= bestBin;
                    });

                outMiddle = uint32(partitionPoint - m_primitiveOrder.Data());

                if (outMiddle > begin && outMiddle < end)
                {
                    return true;
                }
            }
        }

        if (!mustSplit && count <= m_params.maxLeafSize)
        {
            return false;
        }

        outMiddle = SplitAtMedian(begin, end, centroidBounds);

        return true;
    }

    uint32 SplitAtMedian(uint32 begin, uint32 end, const BoundingBox& centroidBounds)
    {
        const uint32 middle = begin + (end - begin) / 2;

        const Vec3f centroidExtent = centroidBounds.max - centroidBounds.min;

        const uint32 axis = (centroidExtent.x >= centroidExtent.y && centroidExtent.x >= centroidExtent.z)
            ? 0
            : (centroidExtent.y >= centroidExtent.z ? 1 : 2);

        if (centroidExtent[axis] > 0.0f)
        {
            std::nth_element(
                m_primitiveOrder.Data() + begin,
                m_primitiveOrder.Data() + middle,
                m_primitiveOrder.Data() + end,
                [this, axis](uint32 lhs, uint32 rhs)
                {
                    return m_primitiveCentroids[lhs][axis] < m_primitiveCentroids[rhs][axis];
                });
        }

        return middle;
    }

    ChildReference BuildChild(uint32 begin, uint32 end, uint32 depth)
    {
        ChildReference child;
        child.bounds = ComputeBounds(begin, end);

        uint32 middle = 0;

        if (end - begin <= m_params.minLeafSize || depth + 1 >= m_params.maxDepth || !FindSplit(begin, end, child.bounds, depth, false, middle))
        {
            child.index = begin;
            child.count = end - begin;

            return child;
        }

        child.index = BuildNode(begin, middle, end, depth);
        child.count = 0;

        return child;
    }

    uint32 BuildNode(uint32 begin, uint32 middle, uint32 end, uint32 depth)
    {
        AssertDebug(begin < middle && middle < end);

        const uint32 nodeIndex = uint32(m_nodes.Size());
        m_nodes.EmplaceBack();

        const ChildReference left = BuildChild(begin, middle, depth + 1);
        const ChildReference right = BuildChild(middle, end, depth + 1);

        // m_nodes may have grown while building the children
        GlimmerBVHNode& node = m_nodes[nodeIndex];
        WriteChild(left, node.leftMin, node.leftMax, node.leftIndex, node.leftCount);
        WriteChild(right, node.rightMin, node.rightMax, node.rightIndex, node.rightCount);

        return nodeIndex;
    }

    void WriteChild(const ChildReference& child, float (&outMin)[3], float (&outMax)[3], uint32& outIndex, uint32& outCount) const
    {
        for (int axis = 0; axis < 3; axis++)
        {
            outMin[axis] = child.bounds.min[axis] - m_boundsPadding;
            outMax[axis] = child.bounds.max[axis] + m_boundsPadding;
        }

        outIndex = child.index;
        outCount = child.count;
    }

    Span<const BoundingBox> m_primitiveBounds;
    GlimmerBVHBuildParams m_params;

    Array<Vec3f> m_primitiveCentroids;
    Array<uint32> m_primitiveOrder;

    Array<GlimmerBVHNode> m_nodes;

    SplitBin m_bins[NumSplitBins];

    float m_boundsPadding = 0.0f;
};

void GlimmerBVHBuilder::Build(
    Span<const BoundingBox> primitiveBounds,
    const GlimmerBVHBuildParams& params,
    Array<GlimmerBVHNode>& outNodes,
    Array<uint32>& outPrimitiveOrder)
{
    HYP_SCOPE;

    GlimmerBVHBuildParams clampedParams = params;
    clampedParams.maxDepth = MathUtil::Max(clampedParams.maxDepth, 2u);

    GlimmerBVHBuildContext context { primitiveBounds, clampedParams };
    context.Build(outNodes, outPrimitiveOrder);

    AssertDebug(outNodes.Empty() || GlimmerBVHBuilder::CalculateDepth(outNodes.ToSpan()) <= clampedParams.maxDepth);
}

uint32 GlimmerBVHBuilder::CalculateDepth(Span<const GlimmerBVHNode> nodes)
{
    if (nodes.Size() == 0)
    {
        return 0;
    }

    struct PendingNode
    {
        uint32 index;
        uint32 depth;
    };

    Array<PendingNode> pending;
    pending.PushBack(PendingNode { 0, 1 });

    uint32 maxDepth = 0;

    while (pending.Any())
    {
        const PendingNode current = pending.PopBack();
        maxDepth = MathUtil::Max(maxDepth, current.depth);

        const GlimmerBVHNode& node = nodes[current.index];

        if (node.leftCount == 0 && node.leftIndex < nodes.Size())
        {
            pending.PushBack(PendingNode { node.leftIndex, current.depth + 1 });
        }

        if (node.rightCount == 0 && node.rightIndex < nodes.Size())
        {
            pending.PushBack(PendingNode { node.rightIndex, current.depth + 1 });
        }
    }

    return maxDepth;
}

bool GlimmerBVHBuilder::PackBLAS(Span<const GlimmerBVHNode> nodes, Array<GlimmerBLASNode>& outNodes, Array<uint32>& outLeafEnds)
{
    HYP_SCOPE;

    constexpr int MinExponent = -32;
    constexpr int MaxExponent = 31;

    outNodes.Resize(nodes.Size());
    outLeafEnds.Clear();

    for (size_t nodeIndex = 0; nodeIndex < nodes.Size(); nodeIndex++)
    {
        const GlimmerBVHNode& node = nodes[nodeIndex];
        GlimmerBLASNode& packed = outNodes[nodeIndex];

        const float* const childMin[2] = { node.leftMin, node.rightMin };
        const float* const childMax[2] = { node.leftMax, node.rightMax };

        float origin[3];
        double scale[3];
        uint32 exponents = 0;

        for (int axis = 0; axis < 3; axis++)
        {
            origin[axis] = MathUtil::Min(node.leftMin[axis], node.rightMin[axis]);

            const double extent = double(MathUtil::Max(node.leftMax[axis], node.rightMax[axis])) - double(origin[axis]);

            int exponent = extent > 0.0 ? int(std::ceil(std::log2(extent / 255.0))) : MinExponent;
            exponent = MathUtil::Clamp(exponent, MinExponent, MaxExponent);

            while (exponent < MaxExponent && 255.0 * std::ldexp(1.0, exponent) < extent)
            {
                exponent++;
            }

            scale[axis] = std::ldexp(1.0, exponent);
            exponents |= uint32(exponent - MinExponent) << (axis * 6);
        }

        // rounding outward keeps the quantized bounds around the real ones
        ubyte childBytes[12];

        for (uint32 child = 0; child < 2; child++)
        {
            for (int axis = 0; axis < 3; axis++)
            {
                const double quantizedMin = std::floor((double(childMin[child][axis]) - double(origin[axis])) / scale[axis]);
                const double quantizedMax = std::ceil((double(childMax[child][axis]) - double(origin[axis])) / scale[axis]);

                childBytes[child * 6 + axis] = ubyte(MathUtil::Clamp(quantizedMin, 0.0, 255.0));
                childBytes[child * 6 + 3 + axis] = ubyte(MathUtil::Clamp(quantizedMax, 0.0, 255.0));
            }
        }

        const uint32 childIndices[2] = { node.leftIndex, node.rightIndex };
        const uint32 childCounts[2] = { node.leftCount, node.rightCount };

        uint32 refs[2];

        for (uint32 child = 0; child < 2; child++)
        {
            if (childIndices[child] > GlimmerBLASNode::MaxIndex)
            {
                return false;
            }

            refs[child] = childIndices[child];

            if (childCounts[child] != 0)
            {
                if (childIndices[child] + childCounts[child] - 1 > GlimmerBLASNode::MaxIndex)
                {
                    return false;
                }

                refs[child] |= GlimmerBLASNode::RefLeafBit;
                outLeafEnds.PushBack(childIndices[child] + childCounts[child] - 1);
            }
        }

        Memory::Copy(&packed.words[0], origin, sizeof(origin));
        Memory::Copy(&packed.words[4], childBytes, sizeof(childBytes));

        packed.words[3] = exponents | ((refs[1] >> 9) << 18);
        packed.words[7] = refs[0] | ((refs[1] & 0x1FFu) << 23);
    }

    return true;
}

} // namespace Hyperion
