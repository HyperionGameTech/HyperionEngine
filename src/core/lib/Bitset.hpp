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
    static constexpr SizeType GetBlockIndex(SizeType bit)
        { return bit / CHAR_BIT / sizeof(BlockType); }

    static constexpr SizeType GetBitMask(SizeType bit)
        { return 1ull << (bit & (num_bits_per_block - 1)); }

public:
    friend std::ostream &operator<<(std::ostream &os, const DynBitset &bitset);

    DynBitset() = default;

    explicit DynBitset(UInt64 value);

    DynBitset(const DynBitset &other) = default;
    DynBitset &operator=(const DynBitset &other) = default;
    DynBitset(DynBitset &&other) noexcept = default;
    DynBitset &operator=(DynBitset &&other) noexcept = default;
    ~DynBitset() = default;

    bool operator==(const DynBitset &other) const
        { return m_blocks == other.m_blocks; }

    bool operator!=(const DynBitset &other) const
        { return m_blocks != other.m_blocks; }

    // DynBitset operator~() const;

    DynBitset operator<<(SizeType pos) const;
    DynBitset &operator<<=(SizeType pos);

    DynBitset operator&(const DynBitset &other) const;
    DynBitset &operator&=(const DynBitset &other);

    DynBitset operator|(const DynBitset &other) const;
    DynBitset &operator|=(const DynBitset &other);

    DynBitset operator^(const DynBitset &other) const;
    DynBitset &operator^=(const DynBitset &other);

    bool Get(SizeType index) const;

    bool Test(SizeType index) const
        { return Get(index); }

    void Set(SizeType index, bool value);

    SizeType Count() const;

    /*! \brief Sets out to the Uint32 representation of the bitset.
        If more bits are included in the bitset than can be converted to
        a Uint32, false is returned. Otherwise, true is returned. */
    bool ToUInt32(UInt32 *out) const;

    /*! \brief Sets out to the Uint64 representation of the bitset.
        If more bits are included in the bitset than can be converted to
        a Uint64, false is returned. Otherwise, true is returned. */
    bool ToUInt64(UInt64 *out) const;

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

    SizeType TotalBitCount() const
        { return m_blocks.Size() * num_bits_per_block; }

    Array<BlockType> m_blocks;
};

} // namespace detail
} // namespace containers

using Bitset = containers::detail::DynBitset;

} // namespace hyperion

#endif