/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BITSET_HPP
#define HYPERION_BITSET_HPP

#include <core/containers/Array.hpp>
#include <math/MathUtil.hpp>

#include <Types.hpp>

#include <ostream>

namespace hyperion {

namespace containers {

class Bitset;

std::ostream &operator<<(std::ostream &os, const Bitset &bitset);

class Bitset
{
public:
    using BlockType = uint32;

    using BitIndex = uint64;

    static constexpr uint32 num_preallocated_blocks = 2;
    static constexpr uint32 num_bits_per_block = sizeof(BlockType) * CHAR_BIT;

    static constexpr BitIndex not_found = BitIndex(-1);

private:
    [[nodiscard]]
    HYP_FORCE_INLINE
    static constexpr uint32 GetBlockIndex(BitIndex bit)
        { return bit / CHAR_BIT / sizeof(BlockType); }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    static constexpr uint32 GetBitMask(uint32 bit)
        { return 1ull << (bit & (num_bits_per_block - 1)); }

    template <bool IsConst>
    struct IteratorBase
    {
        std::conditional_t<IsConst, const Bitset *, Bitset *>   ptr;
        BitIndex                                                bit_index;

        [[nodiscard]]
        HYP_FORCE_INLINE
        BitIndex operator*() const
            { return bit_index; }

        HYP_FORCE_INLINE
        IteratorBase<IsConst> &operator++()
        {
            bit_index = ptr->NextSetBitIndex(bit_index + 1);

            return *this;
        }

        HYP_FORCE_INLINE
        IteratorBase<IsConst> operator++(int) const
        {
            return { ptr, ptr->NextSetBitIndex(bit_index + 1) };
        }

        [[nodiscard]]
        HYP_FORCE_INLINE
        bool operator==(const IteratorBase<IsConst> &other) const
            { return ptr == other.ptr && bit_index == other.bit_index; }

        [[nodiscard]]
        HYP_FORCE_INLINE
        bool operator!=(const IteratorBase<IsConst> &other) const
            { return ptr != other.ptr || bit_index != other.bit_index; }
    };
    
public:
    friend std::ostream &operator<<(std::ostream &os, const Bitset &bitset);

    struct ConstIterator : IteratorBase<true>
    {
    };

    struct Iterator : IteratorBase<false>
    {
        operator ConstIterator() const
            { return { ptr, bit_index }; }
    };

    HYP_API Bitset();

    /*! \brief Constructs a bitset from a 64-bit unsigned integer. */
    HYP_API explicit Bitset(uint64 value);

    Bitset(const Bitset &other)                         = default;
    Bitset &operator=(const Bitset &other)              = default;
    HYP_API Bitset(Bitset &&other) noexcept;
    HYP_API Bitset &operator=(Bitset &&other) noexcept;
    ~Bitset()                                           = default;

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator==(const Bitset &other) const
    {
        if (m_blocks.Size() == other.m_blocks.Size()) {
            return m_blocks.CompareBitwise(other.m_blocks);
        }

        return (*this | other).m_blocks.CompareBitwise(m_blocks.Size() > other.m_blocks.Size() ? other.m_blocks : m_blocks);
    }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator!=(const Bitset &other) const
        { return !operator==(other); }

    /*! \brief Returns a Bitset with all bits flipped. 
        Note, that the number of bits in the returned bitset is the same as
        the number of bits in the original bitset. */
    HYP_API Bitset operator~() const;

    HYP_API Bitset operator<<(uint32 pos) const;
    HYP_API Bitset &operator<<=(uint32 pos);

    HYP_API Bitset operator&(const Bitset &other) const;
    HYP_API Bitset &operator&=(const Bitset &other);

    HYP_API Bitset operator|(const Bitset &other) const;
    HYP_API Bitset &operator|=(const Bitset &other);

    HYP_API Bitset operator^(const Bitset &other) const;
    HYP_API Bitset &operator^=(const Bitset &other);

    [[nodiscard]]
    HYP_FORCE_INLINE
    const ubyte *Data() const
        { return reinterpret_cast<const ubyte *>(m_blocks.Data()); }

    /*! \brief Resizes the bitset to the given number of bits.
        \param num_bits The new number of bits in the bitset. */
    HYP_API Bitset &Resize(uint32 num_bits);

    /*! \brief Returns the index of the first set bit. If no bit is set, -1 is returned.
        \returns The index of the first set bit. */
    HYP_API BitIndex FirstSetBitIndex() const;

    /*! \brief Returns the index of the last set bit. If no bit is set, -1 is returned.
        \returns The index of the last set bit. */
    HYP_API BitIndex LastSetBitIndex() const;

    /*! \brief Returns the index of the next set bit after or including the given index.
        \param offset The index to start searching from.
        \returns The index of the next set bit after the given index. */
    HYP_API BitIndex NextSetBitIndex(BitIndex offset) const;

    /*! \brief Get the value of the bit at the given index.
        \param index The index of the bit to get.
        \returns True if the bit is set, false otherwise. */
    [[nodiscard]]
    HYP_FORCE_INLINE
    bool Get(BitIndex index) const
    {
        const uint32 block_index = GetBlockIndex(index);

        return block_index < m_blocks.Size()
            && (m_blocks[block_index] & GetBitMask(index));
    }

    /*! \brief Test the value of the bit at the given index.
        \param index The index of the bit to test.
        \returns True if the bit is set, false otherwise. */
    [[nodiscard]]
    HYP_FORCE_INLINE
    bool Test(BitIndex index) const
        { return Get(index); }

    /*! \brief Set the value of the bit at the given index.
        \param index The index of the bit to set.
        \param value True to set the bit, false to unset the bit. */
    HYP_API void Set(BitIndex index, bool value);

    /*! \brief Clear the entire bitset.
     *  \details The bitset is cleared by setting it to its default state,
     *  deallocating any memory that was allocated. */
    HYP_API void Clear();

    /*! \brief Returns the total number of bits in the bitset.
        \returns The total number of bits in the bitset. */
    [[nodiscard]]
    HYP_FORCE_INLINE
    uint32 NumBits() const
        { return m_blocks.Size() * num_bits_per_block; }

    /*! \brief Returns the number of ones in the bitset.
        \returns The number of ones in the bitset. */
    [[nodiscard]]
    HYP_API uint64 Count() const;

    /*! \brief Returns the uint32 representation of the bitset.
        If more bits are included in the bitset than can be converted to
        a uint32, the result is truncated.
        \returns The uint32 representation of the bitset. The result is truncated if it would not fit in a uint32.
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

    /*! \brief Sets out to the uint32 representation of the bitset.
        If more bits are included in the bitset than can be converted to
        a uint32, false is returned. Otherwise, true is returned.
        \param out The uint32 to set to the bitset.
        \returns True if the bitset can be represented as a uint32, false otherwise. */
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

    /*! \brief Returns the uint64 representation of the bitset.
        If more bits are included in the bitset than can be converted to
        a uint64, the result is truncated.
        \returns The uint64 representation of the bitset. The result is truncated if it would not fit in a uint64.
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

    /*! \brief Sets out to the uint64 representation of the bitset.
        If more bits are included in the bitset than can be converted to
        a uint64, false is returned. Otherwise, true is returned.
        \param out The uint64 to set to the bitset.
        \returns True if the bitset can be represented as a uint64, false otherwise. */
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

    [[nodiscard]]
    HYP_FORCE_INLINE
    Iterator Begin()
        { return { this, FirstSetBitIndex() }; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    Iterator End()
        { return { this, not_found }; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    ConstIterator Begin() const
        { return { this, FirstSetBitIndex() }; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    ConstIterator End() const
        { return { this, not_found }; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    Iterator begin()
        { return Begin(); }

    [[nodiscard]]
    HYP_FORCE_INLINE
    Iterator end()
        { return End(); }

    [[nodiscard]]
    HYP_FORCE_INLINE
    ConstIterator begin() const
        { return Begin(); }

    [[nodiscard]]
    HYP_FORCE_INLINE
    ConstIterator end() const
        { return End(); }

private:
    HYP_FORCE_INLINE
    void RemoveLeadingZeros()
    {
        while (m_blocks.Size() > num_preallocated_blocks && m_blocks.Back() == 0) {
            m_blocks.PopBack();
        }
    }

    Array<BlockType, 64>    m_blocks;
};

} // namespace containers

using Bitset = containers::Bitset;

} // namespace hyperion

#endif