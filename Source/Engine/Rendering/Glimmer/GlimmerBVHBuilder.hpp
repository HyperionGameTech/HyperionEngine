/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Containers/Array.hpp>
#include <Core/Utilities/Span.hpp>
#include <Core/Math/BoundingBox.hpp>

namespace Hyperion {

struct GlimmerBVHNode
{
    float leftMin[3];
    uint32 leftIndex;   //!< Child node index, or first primitive when the child is a leaf

    float leftMax[3];
    uint32 leftCount;   //!< Leaf primitive count, 0 when the child is an interior node

    float rightMin[3];
    uint32 rightIndex;

    float rightMax[3];
    uint32 rightCount;
};

static_assert(sizeof(GlimmerBVHNode) == 64);

/*  A compressed BLAS node.
 *
 *    words[0..2]  node bounds origin (float x3)
 *    words[3]     bits 0-17: per-axis scale exponents, 6 bits each, biased by 32 (scale = 2^(value - 32));
 *                 bits 18-31: bits 9-22 of the right child ref
 *    words[4..6]  child bounds, one byte per component: left min xyz, left max xyz, right min xyz, right max xyz
 *    words[7]     bits 0-22: left child ref, bits 23-31: bits 0-8 of the right child ref
 * 
 *  A child ref is bit 22 = child is a leaf, bits 0-21 = node index (interior) or first triangle (leaf).
 *  Leaves have no count: the last triangle of a leaf has GlimmerBLASNode::LeafEndFlag set in its position0 padding. */
struct GlimmerBLASNode
{
    static constexpr uint32 RefLeafBit = 1u << 22;
    static constexpr uint32 RefIndexMask = RefLeafBit - 1;
    static constexpr uint32 MaxIndex = RefIndexMask;
    static constexpr uint32 LeafEndFlag = 1u;

    uint32 words[8];
};

static_assert(sizeof(GlimmerBLASNode) == 32);

struct GlimmerBVHBuildParams
{
    uint32 minLeafSize = 2;
    uint32 maxLeafSize = 8;

    uint32 maxDepth = 32;

    float traversalCost = 1.0f;
};

/// @TODO Make namespace? since its only static functions
class ENGINE_API GlimmerBVHBuilder
{
public:
    static void Build(
        Span<const BoundingBox> primitiveBounds,
        const GlimmerBVHBuildParams& params,
        Array<GlimmerBVHNode>& outNodes,
        Array<uint32>& outPrimitiveOrder);

    /*! \brief Deepest root-to-leaf path, for sizing traversal stacks. */
    static uint32 CalculateDepth(Span<const GlimmerBVHNode> nodes);

    /*! \brief Quantizes nodes for the BLAS pool. \ref outLeafEnds gets the index of the last primitive of every leaf.
     *  Returns false if an index doesn't fit a child ref. */
    static bool PackBLAS(Span<const GlimmerBVHNode> nodes, Array<GlimmerBLASNode>& outNodes, Array<uint32>& outLeafEnds);
};

} // namespace Hyperion
