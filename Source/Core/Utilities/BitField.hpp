/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Utilities/ByteUtil.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Types.hpp>

#include <climits>

namespace Hyperion {
namespace utilities {

/// Like Bitset, but statically sized, inline storage, and designed for minimal overhead at the cost of features
/// Still allows iteration through only '1' bits like Bitset and unlike std::bitset.

template <size_t N>
struct BitField
{
    using WordType = uint64;

    static constexpr size_t NumBitsPerWord = sizeof(WordType) * CHAR_BIT;
    static constexpr size_t NumBitsPerWordLog2 = MathUtil::FastLog2(NumBitsPerWord);
    static constexpr size_t NumWords = (N + NumBitsPerWord - 1) / NumBitsPerWord;

    template <bool IsConst>
    struct IteratorBase
    {
        std::conditional_t<IsConst, const BitField&, BitField&> ref;
        size_t index;

        HYP_FORCE_INLINE size_t operator*() const
        {
            return index;
        }

        HYP_FORCE_INLINE IteratorBase<IsConst>& operator++()
        {
            index = ref.NextOneBit(index + 1);

            return *this;
        }

        HYP_FORCE_INLINE IteratorBase<IsConst> operator++(int) const
        {
            return { ref, ref.NextOneBit(index + 1) };
        }

        HYP_FORCE_INLINE bool operator==(const IteratorBase<IsConst>& other) const
        {
            return &ref == &other.ref && index == other.index;
        }

        HYP_FORCE_INLINE bool operator!=(const IteratorBase<IsConst>& other) const
        {
            return &ref != &other.ref || index != other.index;
        }
    };

    struct ConstIterator : IteratorBase<true>
    {
    };

    struct Iterator : IteratorBase<false>
    {
        HYP_FORCE_INLINE operator ConstIterator() const
        {
            return { this->ref, this->index };
        }
    };

    WordType words[NumWords];

    constexpr BitField() = default;
    
    constexpr BitField(const BitField&) = default;
    BitField& operator=(const BitField&) = default;

    constexpr BitField(BitField&&) noexcept = default;
    BitField& operator=(BitField&&) noexcept = default;

    static constexpr HYP_FORCE_INLINE WordType GetBitMask(size_t bitIndex)
    {
        return static_cast<WordType>(1ull << (bitIndex & (NumBitsPerWord - 1)));
    }

    static constexpr HYP_FORCE_INLINE size_t GetWordIndex(size_t bitIndex)
    {
        // same as bitIndex / 64
        return bitIndex >> 6;
    }

    size_t NextOneBit(size_t startOffset) const
    {
        const WordType* wordsBegin = words;
        const WordType* wordsEnd = words + NumWords;

        const size_t startWordIndex = GetWordIndex(startOffset);

        if (startWordIndex >= NumWords)
        {
            return SIZE_MAX;
        }

        const WordType* wordsIter = wordsBegin + startWordIndex;

        WordType currWordMasked = (*wordsIter) & (~WordType(0) << (startOffset & (NumBitsPerWord - 1)));

        if (currWordMasked)
        {
#ifdef HYP_CLANG_OR_GCC
            const uint32 bitIndex = __builtin_ctzll(currWordMasked);
#elif defined(HYP_MSVC)
            unsigned long bitIndex = 0;
            _BitScanForward64(&bitIndex, currWordMasked);
#endif

            return (static_cast<size_t>(wordsIter - wordsBegin) << NumBitsPerWordLog2) + static_cast<size_t>(bitIndex);
        }

        // no more mask after first iteration.
        
        while (++wordsIter != wordsEnd)
        {
            currWordMasked = *wordsIter;

            if (currWordMasked)
            {
    #ifdef HYP_CLANG_OR_GCC
                const uint32 bitIndex = __builtin_ctzll(currWordMasked);
    #elif defined(HYP_MSVC)
                unsigned long bitIndex = 0;
                _BitScanForward64(&bitIndex, currWordMasked);
    #endif

                return (static_cast<size_t>(wordsIter - wordsBegin) << NumBitsPerWordLog2) + static_cast<size_t>(bitIndex);
            }
        }

        // not found
        return SIZE_MAX;
    }

    size_t FirstOneBit() const
    {
        const WordType* wordsBegin = words;
        const WordType* wordsEnd = words + NumWords;

        const WordType* wordsIter = wordsBegin;

        do
        {
            WordType currWord = *wordsIter;
            if (currWord)
            {
#ifdef HYP_CLANG_OR_GCC
                const uint32 bitIndex = __builtin_ctzll(currWord);
#elif defined(HYP_MSVC)
                unsigned long bitIndex = 0;
                _BitScanForward64(&bitIndex, currWord);
#endif

                return (static_cast<size_t>(wordsIter - wordsBegin) << NumBitsPerWordLog2) + static_cast<size_t>(bitIndex);
            }

            ++wordsIter;
        } while (wordsIter != wordsEnd);

        return SIZE_MAX;
    }

    size_t LastOneBit() const
    {
        const WordType* wordsBegin = words;
        const WordType* wordsEnd = words + NumWords;

        const WordType* wordsIter = wordsEnd;

        do
        {
            if (*(--wordsIter) != 0)
            {
#ifdef HYP_CLANG_OR_GCC
                const uint32 bitIndex = NumBitsPerWord - __builtin_clzll(*wordsIter) - 1;
#elif defined(HYP_MSVC)
                unsigned long bitIndex = 0;
                _BitScanReverse64(&bitIndex, *wordsIter);
#endif

                return (static_cast<size_t>(wordsIter - wordsBegin) << NumBitsPerWordLog2) + static_cast<size_t>(bitIndex);
            }
        } while (wordsIter != wordsBegin);

        return SIZE_MAX;
    }

    HYP_FORCE_INLINE size_t CountOnes() const
    {
        size_t count = 0;

        #pragma unroll
        for (WordType value : words)
        {
            count += ByteUtil::BitCount(value);
        }

        return count;
    }

    constexpr HYP_FORCE_INLINE BitField operator&(const BitField &other) const
    {
        BitField result;

        for (size_t i = 0; i < NumWords; i++)
        {
            result.words[i] = words[i] & other.words[i];
        }

        return result;
    }
    
    constexpr HYP_FORCE_INLINE BitField operator&(uint64 other) const
    {
        BitField result;
        result.words[0] = words[0] & other;

        for (size_t i = 1; i < NumWords; i++)
        {
            result.words[i] = WordType(0);
        }

        return result;
    }

    constexpr HYP_FORCE_INLINE BitField& operator&=(const BitField &other)
    {
        for (size_t i = 0; i < NumWords; i++)
        {
            words[i] &= other.words[i];
        }

        return *this;
    }

    constexpr HYP_FORCE_INLINE BitField operator|(const BitField &other) const
    {
        BitField result;

        for (size_t i = 0; i < NumWords; i++)
        {
            result.words[i] = words[i] | other.words[i];
        }

        return result;
    }

    constexpr HYP_FORCE_INLINE BitField operator|(uint64 other) const
    {
        BitField result;
        result.words[0] = words[0] | other;

        for (size_t i = 1; i < NumWords; i++)
        {
            result.words[i] = words[i];
        }

        return result;
    }

    HYP_FORCE_INLINE BitField& operator|=(const BitField &other)
    {
        for (size_t i = 0; i < NumWords; i++)
        {
            words[i] |= other.words[i];
        }

        return *this;
    }

    constexpr HYP_FORCE_INLINE BitField operator^(const BitField &other) const
    {
        BitField result;

        for (size_t i = 0; i < NumWords; i++)
        {
            result.words[i] = words[i] ^ other.words[i];
        }

        return result;
    }

    constexpr HYP_FORCE_INLINE BitField operator^(uint64 other) const
    {
        BitField result;
        result.words[0] = words[0] ^ other;

        for (size_t i = 1; i < NumWords; i++)
        {
            result.words[i] = words[i];
        }
    }

    constexpr HYP_FORCE_INLINE BitField& operator^=(const BitField &other)
    {
        for (size_t i = 0; i < NumWords; i++)
        {
            words[i] ^= other.words[i];
        }

        return *this;
    }

    constexpr HYP_FORCE_INLINE BitField operator~() const
    {
        BitField result;

        for (size_t i = 0; i < NumWords; i++)
        {
            result.words[i] = ~words[i];
        }

        return result;
    }

    constexpr HYP_FORCE_INLINE bool Test(size_t bitIndex) const
    {
        return (words[GetWordIndex(bitIndex)] & GetBitMask(bitIndex));
    }

    constexpr HYP_FORCE_INLINE void Set(size_t bitIndex, bool value)
    {
        const size_t wordIndex = GetWordIndex(bitIndex);
        const size_t mask = GetBitMask(bitIndex);

        const WordType currValue = words[wordIndex];

        const WordType possibleValues[2] = {
            (currValue & ~mask),
            (currValue | mask)
        };

        words[wordIndex] = possibleValues[value];
    }

    HYP_FORCE_INLINE Iterator Begin()
    {
        return { *this, FirstOneBit() };
    }

    HYP_FORCE_INLINE Iterator End()
    {
        return { *this, SIZE_MAX };
    }

    HYP_FORCE_INLINE ConstIterator Begin() const
    {
        return { *this, FirstOneBit() };
    }

    HYP_FORCE_INLINE ConstIterator End() const
    {
        return { *this, SIZE_MAX };
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
};

} // namespace utilities

using utilities::BitField;

// #pragma region Bitset format helper

// namespace utilities {

// template <class StringType, class AllocatorType>
// struct Formatter<StringType, TBitset<AllocatorType>>
// {
//     auto operator()(const TBitset<AllocatorType>& bitset) const
//     {
//         StringType result;

//         for (uint32 blockIndex = bitset.NumBits() / bitset.NumBitsPerWord; blockIndex != 0; --blockIndex)
//         {
//             for (uint32 bitIndex = TBitset<AllocatorType>::NumBitsPerWord; bitIndex != 0; --bitIndex)
//             {
//                 const uint32 combinedBitIndex = ((blockIndex - 1) * TBitset<AllocatorType>::NumBitsPerWord) + (bitIndex - 1);

//                 result.Append(bitset.Get(combinedBitIndex) ? '1' : '0');

//                 if (((bitIndex - 1) % CHAR_BIT) == 0)
//                 {
//                     result.Append(' ');
//                 }
//             }
//         }

//         return result;
//     }
// };

// } // namespace utilities

// #pragma endregion Bitset format helper

} // namespace Hyperion
