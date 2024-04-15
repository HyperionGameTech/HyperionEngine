/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_V2_LIB_BITSET_HPP
#define HYPERION_V2_LIB_BITSET_HPP

#include <core/lib/DynArray.hpp>
#include <math/MathUtil.hpp>
#include <Types.hpp>

#include <ostream>

namespace hyperion {

namespace containers {
namespace detail {

class DynBitset;

std::ostream &operator<<(std::ostream &os, const DynBitset &bitset);

class DynBitset
{
public:
    using BlockType = uint32;

    static constexpr SizeType num_bits_per_block = sizeof(BlockType) * CHAR_BIT;

private:
    [[nodiscard]]
    HYP_FORCE_INLINE
    static constexpr SizeType GetBlockIndex(SizeType bit)
        { return bit / CHAR_BIT / sizeof(BlockType); }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    static constexpr SizeType GetBitMask(SizeType bit)
        { return 1ull << (bit & (num_bits_per_block - 1)); }

public:
    friend std::ostream &operator<<(std::ostream &os, const DynBitset &bitset);

    DynBitset()                                         = default;

    HYP_API explicit DynBitset(uint64 value);

    DynBitset(const DynBitset &other)                   = default;
    DynBitset &operator=(const DynBitset &other)        = default;
    DynBitset(DynBitset &&other) noexcept               = default;
    DynBitset &operator=(DynBitset &&other) noexcept    = default;
    ~DynBitset()                                        = default;

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator==(const DynBitset &other) const
        { return m_blocks.CompareBitwise(other.m_blocks); }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator!=(const DynBitset &other) const
        { return m_blocks.CompareBitwise(other.m_blocks); }

    /*! \brief Returns a DynBitset with all bits flipped. 
        Note, that the number of bits in the returned bitset is the same as
        the number of bits in the original bitset. */
    HYP_API DynBitset operator~() const;

    HYP_API DynBitset operator<<(SizeType pos) const;
    HYP_API DynBitset &operator<<=(SizeType pos);

    HYP_API DynBitset operator&(const DynBitset &other) const;
    HYP_API DynBitset &operator&=(const DynBitset &other);

    HYP_API DynBitset operator|(const DynBitset &other) const;
    HYP_API DynBitset &operator|=(const DynBitset &other);

    HYP_API DynBitset operator^(const DynBitset &other) const;
    HYP_API DynBitset &operator^=(const DynBitset &other);

    /*! \brief Resizes the bitset to the given number of bits. 
        If the new number of bits is greater than the current number of bits,
        the new bits are set to the given value.
        Setting num_bits to a value < 64 has no effect */
    HYP_API DynBitset &Resize(SizeType num_bits, bool value = false);

    /*! \brief Returns the index of the first set bit. If no bit is set, -1 is returned. */
    HYP_API SizeType FirstSetBitIndex() const;

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool Get(SizeType index) const
    {
        return GetBlockIndex(index) < m_blocks.Size()
            && (m_blocks[GetBlockIndex(index)] & GetBitMask(index));
    }

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool Test(SizeType index) const
        { return Get(index); }

    HYP_API void Set(SizeType index, bool value);

    /*! \brief Returns the total number of bits in the bitset. */
    [[nodiscard]]
    HYP_FORCE_INLINE
    SizeType NumBits() const
        { return m_blocks.Size() * num_bits_per_block; }

    /*! \brief Returns the number of ones in the bitset. */
    [[nodiscard]]
    HYP_API SizeType Count() const;

    /*! \brief Returns the Uint32 representation of the bitset.
        If more bits are included in the bitset than can be converted to
        a Uint32, the result is truncated.
    */
    [[nodiscard]]
    HYP_FORCE_INLINE
    uint32 ToUInt32() const
    {
        if (m_blocks.Empty()) {
            return 0;
        } else {
            return m_blocks[0];
        }
    }

    /*! \brief Sets out to the Uint32 representation of the bitset.
        If more bits are included in the bitset than can be converted to
        a Uint32, false is returned. Otherwise, true is returned. */
    HYP_FORCE_INLINE
    bool ToUInt32(uint32 *out) const
    {
        if (m_blocks.Empty()) {
            *out = 0;

            return true;
        } else {
            *out = m_blocks[0];

            return m_blocks.Size() == 1;
        }
    }

    /*! \brief Returns the Uint64 representation of the bitset.
        If more bits are included in the bitset than can be converted to
        a Uint64, the result is truncated.
    */
    [[nodiscard]]
    HYP_FORCE_INLINE
    uint64 ToUInt64() const
    {
        if (m_blocks.Empty()) {
            return 0;
        } else if (m_blocks.Size() == 1) {
            return uint64(m_blocks[0]);
        } else {
            return uint64(m_blocks[0]) | (uint64(m_blocks[1]) << 32);
        }
    }

    /*! \brief Sets out to the Uint64 representation of the bitset.
        If more bits are included in the bitset than can be converted to
        a Uint64, false is returned. Otherwise, true is returned. */
    HYP_FORCE_INLINE
    bool ToUInt64(uint64 *out) const
    {
        if (m_blocks.Empty()) {
            *out = 0;

            return true;
        } else if (m_blocks.Size() == 1) {
            *out = uint64(m_blocks[0]);

            return true;
        } else {
            *out = uint64(m_blocks[0]) | (uint64(m_blocks[1]) << 32);

            return m_blocks.Size() == 2;
        }
    }

    [[nodiscard]]
    HYP_FORCE_INLINE
    HashCode GetHashCode() const
    {
        HashCode hc;

        for (const auto value : m_blocks) {
            hc.Add(value);
        }

        return hc;
    }

private:
    void RemoveLeadingZeros();

    Array<BlockType, 64> m_blocks;
};

} // namespace detail
} // namespace containers

using Bitset = containers::detail::DynBitset;

} // namespace hyperion

#endif