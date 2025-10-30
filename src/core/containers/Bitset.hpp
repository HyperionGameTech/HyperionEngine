/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/Array.hpp>
#include <core/utilities/FormatFwd.hpp>
#include <core/utilities/ByteUtil.hpp>

#include <core/math/MathUtil.hpp>

#include <core/Types.hpp>

#include <ostream>

namespace hyperion {
namespace containers {

/*! \brief A dynamic bitset implementation that allows for efficient storage and manipulation of bits.
 *  It supports operations such as setting, clearing, flipping bits, and iterating over set bits.
 *  The bitset can be resized dynamically, and it provides a range of bitwise operations.
 */
class HYP_API Bitset
{
public:
    using BlockType = uint32;

    using BitIndex = uint64;

    static constexpr uint32 NumPreallocatedBlocks = 2;
    static constexpr uint32 NumBitsPerBlock = sizeof(BlockType) * CHAR_BIT;
    static constexpr uint32 NumBitsPerBlockLog2 = MathUtil::FastLog2(NumBitsPerBlock);

    static constexpr BitIndex NotFound = BitIndex(-1);

    HYP_FORCE_INLINE static constexpr uint64 GetBitMask(BitIndex bit)
    {
        return 1ull << (bit & (NumBitsPerBlock - 1));
    }

    HYP_FORCE_INLINE static constexpr uint64 GetBlockIndex(BitIndex bit)
    {
        return bit / CHAR_BIT / sizeof(BlockType);
        // + 3 is log2(CHAR_BIT)
        // return bit >> (NumBitsPerBlockLog2 + 3);
    }

private:
    template <bool IsConst>
    struct IteratorBase
    {
        std::conditional_t<IsConst, const Bitset*, Bitset*> ptr;
        BitIndex bitIndex;

        HYP_FORCE_INLINE BitIndex operator*() const
        {
            return bitIndex;
        }

        HYP_FORCE_INLINE IteratorBase<IsConst>& operator++()
        {
            bitIndex = ptr->NextSetBitIndex(bitIndex + 1);

            return *this;
        }

        HYP_FORCE_INLINE IteratorBase<IsConst> operator++(int) const
        {
            return { ptr, ptr->NextSetBitIndex(bitIndex + 1) };
        }

        HYP_FORCE_INLINE bool operator==(const IteratorBase<IsConst>& other) const
        {
            return ptr == other.ptr && bitIndex == other.bitIndex;
        }

        HYP_FORCE_INLINE bool operator!=(const IteratorBase<IsConst>& other) const
        {
            return ptr != other.ptr || bitIndex != other.bitIndex;
        }
    };

public:
    struct ConstIterator : IteratorBase<true>
    {
    };

    struct Iterator : IteratorBase<false>
    {
        HYP_FORCE_INLINE operator ConstIterator() const
        {
            return { ptr, bitIndex };
        }
    };

    Bitset();

    /*! \brief Constructs a bitset from a 64-bit unsigned integer. */
    explicit Bitset(uint64 value);

    Bitset(const Bitset& other) = default;
    Bitset& operator=(const Bitset& other) = default;
    
    Bitset(Bitset&& other) noexcept;
    Bitset& operator=(Bitset&& other) noexcept;
    
    ~Bitset() = default;

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return AnyBitsSet();
    }

    HYP_FORCE_INLINE bool operator!() const
    {
        return !AnyBitsSet();
    }

    HYP_FORCE_INLINE bool operator==(const Bitset& other) const
    {
        if (m_blocks.Size() == other.m_blocks.Size())
        {
            return m_blocks.CompareBitwise(other.m_blocks);
        }

        return (*this | other).m_blocks.CompareBitwise(m_blocks.Size() > other.m_blocks.Size() ? other.m_blocks : m_blocks);
    }

    HYP_FORCE_INLINE bool operator!=(const Bitset& other) const
    {
        return !operator==(other);
    }

    /*! \brief Returns a Bitset with all bits flipped.
        Note, that the number of bits in the returned bitset is the same as
        the number of bits in the original bitset. */
    Bitset operator~() const;

    Bitset operator<<(uint32 pos) const;
    Bitset& operator<<=(uint32 pos);

    Bitset operator&(const Bitset& other) const;
    Bitset& operator&=(const Bitset& other);

    Bitset operator|(const Bitset& other) const;
    Bitset& operator|=(const Bitset& other);

    Bitset operator^(const Bitset& other) const;
    Bitset& operator^=(const Bitset& other);

    HYP_FORCE_INLINE const ubyte* Data() const
    {
        return reinterpret_cast<const ubyte*>(m_blocks.Data());
    }

    /*! \brief Returns the index of the first set bit. If no bit is set, -1 is returned.
        \returns The index of the first set bit. */
    inline BitIndex FirstSetBitIndex() const
    {
        const BlockType* blocksBegin = m_blocks.Begin();
        const BlockType* blocksEnd = m_blocks.End();

        const BlockType* blocksIter = blocksBegin;

        while (blocksIter < blocksEnd)
        {
            if (*blocksIter != 0)
            {
#ifdef HYP_CLANG_OR_GCC
                const uint32 bitIndex = __builtin_ffs(*blocksIter) - 1;
#elif defined(HYP_MSVC)
                unsigned long bitIndex = 0;
                _BitScanForward(&bitIndex, *blocksIter);
#endif

                return (uint64(blocksIter - blocksBegin) << NumBitsPerBlockLog2) + uint64(bitIndex);
            }

            ++blocksIter;
        }

        return NotFound;
    }

    /*! \brief Returns the index of the last set bit. If no bit is set, -1 is returned.
        \returns The index of the last set bit. */
    inline BitIndex LastSetBitIndex() const
    {
        const BlockType* blocksBegin = m_blocks.Begin();
        const BlockType* blocksEnd = m_blocks.End();

        const BlockType* blocksIter = blocksEnd;

        while (blocksIter != blocksBegin)
        {
            if (*(--blocksIter) != 0)
            {
#ifdef HYP_CLANG_OR_GCC
                const uint32 bitIndex = NumBitsPerBlock - __builtin_clz(*blocksIter) - 1;
#elif defined(HYP_MSVC)
                unsigned long bitIndex = 0;
                _BitScanReverse(&bitIndex, *blocksIter);
#endif

                return (uint64(blocksIter - blocksBegin) << NumBitsPerBlockLog2) + uint64(bitIndex);
            }
        }

        return NotFound;
    }

    /*! \brief Returns the index of the next set bit after or including the given index.
        \param offset The index to start searching from.
        \returns The index of the next set bit after the given index. */
    inline BitIndex NextSetBitIndex(BitIndex offset) const
    {
        const BlockType* blocksBegin = m_blocks.Begin();
        const BlockType* blocksEnd = m_blocks.End();

        const BlockType* blocksIter = blocksBegin + GetBlockIndex(offset);

        uint32 mask = ~(GetBitMask(offset) - 1);

        while (blocksIter < blocksEnd)
        {
            if ((*blocksIter & mask))
            {
#ifdef HYP_CLANG_OR_GCC
                const uint32 bitIndex = __builtin_ffs(*blocksIter & mask) - 1;
#elif defined(HYP_MSVC)
                unsigned long bitIndex = 0;
                _BitScanForward(&bitIndex, *blocksIter & mask);
#endif

                return (uint64(blocksIter - blocksBegin) << NumBitsPerBlockLog2) + uint64(bitIndex);
            }

            // use all bits in next iteration of loop
            mask = ~0u;
            ++blocksIter;
        }

        return NotFound;
    }

    inline BitIndex FirstZeroBitIndex() const
    {
        const uint32 numBlocks = uint32(m_blocks.Size());

        for (uint32 blockIndex = 0; blockIndex < numBlocks; blockIndex++)
        {
            const BlockType inverted = ~m_blocks[blockIndex];

            if (inverted != 0)
            {
#ifdef HYP_CLANG_OR_GCC
                const uint32 bitIndex = __builtin_ffs(inverted) - 1;
#elif defined(HYP_MSVC)
                unsigned long bitIndex = 0;
                _BitScanForward(&bitIndex, inverted);
#endif

                return (blockIndex << NumBitsPerBlockLog2) + bitIndex;
            }
        }

        // No free bit currently; return the first bit of the next block to be added
        return BitIndex(numBlocks) << NumBitsPerBlockLog2;
    }

    inline BitIndex LastZeroBitIndex() const
    {
        const uint32 numBlocks = uint32(m_blocks.Size());

        for (uint32 blockIndex = numBlocks; blockIndex != 0; blockIndex--)
        {
            const BlockType inverted = ~m_blocks[blockIndex - 1];

            if (inverted != 0)
            {
#ifdef HYP_CLANG_OR_GCC
                const uint32 bitIndex = NumBitsPerBlock - __builtin_clz(inverted) - 1;
#elif defined(HYP_MSVC)
                unsigned long bitIndex = 0;
                _BitScanReverse(&bitIndex, inverted);
#endif

                return ((blockIndex - 1) << NumBitsPerBlockLog2) + bitIndex;
            }
        }

        // No free bit currently; return the first bit of the next block to be added
        return BitIndex(numBlocks) << NumBitsPerBlockLog2;
    }

    /*! \brief Get the value of the bit at the given index.
        \param index The index of the bit to get.
        \returns True if the bit is set, false otherwise. */
    HYP_FORCE_INLINE bool Get(BitIndex index) const
    {
        const uint64 blockIndex = GetBlockIndex(index);

        return blockIndex < m_blocks.Size()
            && (m_blocks[blockIndex] & GetBitMask(index));
    }

    /*! \brief Test the value of the bit at the given index.
        \param index The index of the bit to test.
        \returns True if the bit is set, false otherwise. */
    HYP_FORCE_INLINE bool Test(BitIndex index) const
    {
        return Get(index);
    }

    /*! \brief Set the value of the bit at the given index.
        \param index The index of the bit to set.
        \param value True to set the bit, false to unset the bit. */
    void Set(BitIndex index, bool value);

    /*! \brief Clear the entire bitset.
     *  \details The bitset is cleared by setting it to its default state,
     *  deallocating any memory that was allocated. */
    void Clear();

    /*! \brief Returns the total number of bits in the bitset.
        \returns The total number of bits in the bitset. */
    HYP_FORCE_INLINE SizeType NumBits() const
    {
        return m_blocks.Size() << NumBitsPerBlockLog2;
    }

    /*! \brief Resizes the bitset to the given number of bits.
        \param numBits The new number of bits in the bitset.*/
    Bitset& SetNumBits(SizeType numBits);

    /*! \brief Returns the number of ones in the bitset.
        \returns The number of ones in the bitset. */
    HYP_FORCE_INLINE uint64 Count() const
    {
        uint64 count = 0;

        for (const BlockType value : m_blocks)
        {
            count += ByteUtil::BitCount(value);
        }

        return count;
    }

    HYP_FORCE_INLINE bool AnyBitsSet() const
    {
        for (auto it = m_blocks.Begin(); it != m_blocks.End(); ++it)
        {
            if (*it)
            {
                return true;
            }
        }

        return false;
    }

    /*! \brief Returns the uint32 representation of the bitset.
        If more bits are included in the bitset than can be converted to
        a uint32, the result is truncated.
        \returns The uint32 representation of the bitset. The result is truncated if it would not fit in a uint32.
    */
    HYP_FORCE_INLINE uint32 ToUInt32() const
    {
        if (m_blocks.Empty())
        {
            return 0;
        }
        else
        {
            return m_blocks[0];
        }
    }

    /*! \brief Sets out to the uint32 representation of the bitset.
        If more bits are included in the bitset than can be converted to
        a uint32, false is returned. Otherwise, true is returned.
        \param out The uint32 to set to the bitset.
        \returns True if the bitset can be represented as a uint32, false otherwise. */
    HYP_FORCE_INLINE bool ToUInt32(uint32* out) const
    {
        if (m_blocks.Empty())
        {
            *out = 0;

            return true;
        }
        else
        {
            *out = m_blocks[0];

            return m_blocks.Size() == 1;
        }
    }

    /*! \brief Returns the uint64 representation of the bitset.
        If more bits are included in the bitset than can be converted to
        a uint64, the result is truncated.
        \returns The uint64 representation of the bitset. The result is truncated if it would not fit in a uint64.
    */
    HYP_FORCE_INLINE uint64 ToUInt64() const
    {
        if (m_blocks.Empty())
        {
            return 0;
        }
        else if (m_blocks.Size() == 1)
        {
            return uint64(m_blocks[0]);
        }
        else
        {
            return uint64(m_blocks[0]) | (uint64(m_blocks[1]) << 32);
        }
    }

    /*! \brief Sets out to the uint64 representation of the bitset.
        If more bits are included in the bitset than can be converted to
        a uint64, false is returned. Otherwise, true is returned.
        \param out The uint64 to set to the bitset.
        \returns True if the bitset can be represented as a uint64, false otherwise. */
    HYP_FORCE_INLINE bool ToUInt64(uint64* out) const
    {
        if (m_blocks.Empty())
        {
            *out = 0;

            return true;
        }
        else if (m_blocks.Size() == 1)
        {
            *out = uint64(m_blocks[0]);

            return true;
        }
        else
        {
            *out = uint64(m_blocks[0]) | (uint64(m_blocks[1]) << 32);

            return m_blocks.Size() == 2;
        }
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;

        for (const auto value : m_blocks)
        {
            hc.Add(value);
        }

        return hc;
    }

    HYP_FORCE_INLINE Iterator Begin()
    {
        return { this, FirstSetBitIndex() };
    }

    HYP_FORCE_INLINE Iterator End()
    {
        return { this, NotFound };
    }

    HYP_FORCE_INLINE ConstIterator Begin() const
    {
        return { this, FirstSetBitIndex() };
    }

    HYP_FORCE_INLINE ConstIterator End() const
    {
        return { this, NotFound };
    }

    HYP_FORCE_INLINE Iterator begin()
    {
        return Begin();
    }

    HYP_FORCE_INLINE Iterator end()
    {
        return End();
    }

    HYP_FORCE_INLINE ConstIterator begin() const
    {
        return Begin();
    }

    HYP_FORCE_INLINE ConstIterator end() const
    {
        return End();
    }

private:
    HYP_FORCE_INLINE void RemoveLeadingZeros()
    {
        while (m_blocks.Size() > NumPreallocatedBlocks && m_blocks.Back() == 0)
        {
            m_blocks.PopBack();
        }
    }

    Array<BlockType, InlineAllocator<16>> m_blocks;
};

} // namespace containers

using Bitset = containers::Bitset;

namespace utilities {

template <class StringType>
struct Formatter<StringType, Bitset>
{
    auto operator()(const Bitset& bitset) const
    {
        StringType result;

        for (uint32 blockIndex = bitset.NumBits() / bitset.NumBitsPerBlock; blockIndex != 0; --blockIndex)
        {
            for (uint32 bitIndex = Bitset::NumBitsPerBlock; bitIndex != 0; --bitIndex)
            {
                const uint32 combinedBitIndex = ((blockIndex - 1) * Bitset::NumBitsPerBlock) + (bitIndex - 1);

                result.Append(bitset.Get(combinedBitIndex) ? '1' : '0');

                if (((bitIndex - 1) % CHAR_BIT) == 0)
                {
                    result.Append(' ');
                }
            }
        }

        return result;
    }
};

} // namespace utilities

} // namespace hyperion
