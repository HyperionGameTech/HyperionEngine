/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/Types.hpp>
#include <Core/HashCode.hpp>

#include <Core/reflection/ObjectMacros.hpp>

namespace Hyperion {

/*! \brief Represents an octant in an octree
 *  \details The bits are ordered as follows:
 *  - 0-2: index of topmost parent octant
 *  - 3-5: index of second parent octant
 *  - 6-8: index of third parent octant
 * ... and so on until the index of the octant itself.
 *
 * The maximum depth of an octree using this scheme is 64 bits / 3 bits for index = 21 octants.
 */
HYP_STRUCT()
struct OctantId
{
    HYP_STRUCT_BODY(OctantId);

    //! This bit is reserved for invalid octants -- We use 3 bits for each index, leaving 1 bit left on a 64-bit integer
    static constexpr uint64 InvalidBits = 1ull << 63;
    static constexpr SizeType MaxDepth = 64 / 3;

    uint64 indexBits { 0 };
    uint8 depth { 0 };

    OctantId() = default;

    explicit OctantId(uint64 indexBits, uint8 depth)
        : indexBits(indexBits),
          depth(depth)
    {
    }

    explicit OctantId(uint8 childIndex, OctantId parentId)
        : indexBits(!parentId.IsInvalid()
                ? parentId.indexBits | (uint64(childIndex) << (uint64(parentId.GetDepth() + uint8(1)) * 3ull))
                : childIndex),
          depth(parentId.GetDepth() + uint8(1))
    {
    }

    OctantId(const OctantId& other) = default;
    OctantId& operator=(const OctantId& other) = default;
    OctantId(OctantId&& other) noexcept = default;
    OctantId& operator=(OctantId&& other) noexcept = default;
    ~OctantId() = default;

    HYP_FORCE_INLINE bool IsInvalid() const
    {
        return indexBits & InvalidBits;
    }

    HYP_FORCE_INLINE bool operator==(const OctantId& other) const
    {
        return indexBits == other.indexBits && depth == other.depth;
    }

    HYP_FORCE_INLINE bool operator!=(const OctantId& other) const
    {
        return !(*this == other);
    }

    HYP_FORCE_INLINE uint8 GetIndex(uint8 depth) const
    {
        return (indexBits >> (uint64(depth) * 3ull)) & 0x7;
    }

    HYP_FORCE_INLINE uint8 GetIndex() const
    {
        return GetIndex(depth);
    }

    HYP_FORCE_INLINE uint8 GetDepth() const
    {
        return depth;
    }

    HYP_FORCE_INLINE bool IsSiblingOf(OctantId other) const
    {
        return depth == other.depth && (indexBits & ~(~0ull << (uint64(depth) * 3ull))) == (other.indexBits & ~(~0ull << (uint64(depth) * 3ull)));
    }

    HYP_FORCE_INLINE bool IsChildOf(OctantId other) const
    {
        return depth > other.depth && (indexBits & ~(~0ull << (uint64(other.depth) * 3ull))) == other.indexBits;
    }

    HYP_FORCE_INLINE bool IsParentOf(OctantId other) const
    {
        return depth < other.depth && indexBits == (other.indexBits & ~(~0ull << (uint64(depth) * 3ull)));
    }

    HYP_FORCE_INLINE OctantId GetParent() const
    {
        if (depth == 0)
        {
            return OctantId::Invalid();
        }

        return OctantId(indexBits & ~(~0ull << (uint64(depth) * 3ull)), depth - 1);
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(indexBits);
        hc.Add(depth);

        return hc;
    }

    /*! \brief Get the special invalid OctantId. */
    static OctantId Invalid()
    {
        // 0x80 For index bit because we reserve the highest bit for invalid octants
        // 0xff for depth because +1 (used for child octant id) will cause it to overflow to 0
        return OctantId(InvalidBits, 0xff);
    }
};

} // namespace Hyperion