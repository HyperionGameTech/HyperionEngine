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
    using BlockType = UInt32;

    static constexpr SizeType num_bits_per_block = sizeof(BlockType) * CHAR_BIT;

private:
    HYP_FORCE_INLINE
    static constexpr SizeType GetBlockIndex(SizeType bit)
        { return bit / CHAR_BIT / sizeof(BlockType); }

    HYP_FORCE_INLINE
    static constexpr SizeType GetBitMask(SizeType bit)
        { return 1ull << (bit & (num_bits_per_block - 1)); }

public:
    friend std::ostream &operator<<(std::ostream &os, const DynBitset &bitset);

    DynBitset() = default;

    explicit DynBitset(UInt64 value);

    DynBitset(const DynBitset &other)                   = default;
    DynBitset &operator=(const DynBitset &other)        = default;
    DynBitset(DynBitset &&other) noexcept               = default;
    DynBitset &operator=(DynBitset &&other) noexcept    = default;
    ~DynBitset()                                        = default;

    HYP_FORCE_INLINE
    Bool operator==(const DynBitset &other) const
        { return m_blocks.CompareBitwise(other.m_blocks); }

    HYP_FORCE_INLINE
    Bool operator!=(const DynBitset &other) const
        { return m_blocks.CompareBitwise(other.m_blocks); }

    /*! \brief Returns a DynBitset with all bits flipped. 
        Note, that the number of bits in the returned bitset is the same as
        the number of bits in the original bitset. */
    DynBitset operator~() const;

    DynBitset operator<<(SizeType pos) const;
    DynBitset &operator<<=(SizeType pos);

    DynBitset operator&(const DynBitset &other) const;
    DynBitset &operator&=(const DynBitset &other);

    DynBitset operator|(const DynBitset &other) const;
    DynBitset &operator|=(const DynBitset &other);

    DynBitset operator^(const DynBitset &other) const;
    DynBitset &operator^=(const DynBitset &other);

    /*! \brief Resizes the bitset to the given number of bits. 
        If the new number of bits is greater than the current number of bits,
        the new bits are set to the given value.
        Setting num_bits to a value < 64 has no effect */
    DynBitset &Resize(SizeType num_bits, bool value = false);

    /*! \brief Returns the index of the first set bit. If no bit is set, -1 is returned. */
    SizeType FirstSetBitIndex() const;

    HYP_FORCE_INLINE
    Bool Get(SizeType index) const
    {
        return GetBlockIndex(index) < m_blocks.Size()
            && (m_blocks[GetBlockIndex(index)] & GetBitMask(index));
    }

    HYP_FORCE_INLINE
    Bool Test(SizeType index) const
        { return Get(index); }

    void Set(SizeType index, Bool value);

    /*! \brief Returns the total number of bits in the bitset. */
    SizeType NumBits() const
        { return m_blocks.Size() * num_bits_per_block; }

    /*! \brief Returns the number of ones in the bitset. */
    SizeType Count() const;

    /*! \brief Returns the Uint32 representation of the bitset.
        If more bits are included in the bitset than can be converted to
        a Uint32, the result is truncated.
    */
    UInt32 ToUInt32() const;

    /*! \brief Sets out to the Uint32 representation of the bitset.
        If more bits are included in the bitset than can be converted to
        a Uint32, false is returned. Otherwise, true is returned. */
    Bool ToUInt32(UInt32 *out) const;

    /*! \brief Returns the Uint64 representation of the bitset.
        If more bits are included in the bitset than can be converted to
        a Uint64, the result is truncated.
    */
    UInt64 ToUInt64() const;

    /*! \brief Sets out to the Uint64 representation of the bitset.
        If more bits are included in the bitset than can be converted to
        a Uint64, false is returned. Otherwise, true is returned. */
    Bool ToUInt64(UInt64 *out) const;

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