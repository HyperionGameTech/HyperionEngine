/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/containers/Bitset.hpp>
#include <core/utilities/Span.hpp>
#include <core/Util.hpp>

#include <core/utilities/ByteUtil.hpp>

#ifdef HYP_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <bitset> // for output

namespace hyperion {

namespace containers {

static Array<Bitset::BlockType, InlineAllocator<16>> CreateBlocks_Internal(uint64 value)
{
    return { Bitset::BlockType(value & 0xFFFFFFFF), Bitset::BlockType((value & (0xFFFFFFFFull << 32ull)) >> 32ull) };
}

template <uint64 InitialValue>
static Span<const Bitset::BlockType> CreateBlocks_Static_Internal()
{
    static const Bitset::BlockType blocks[2] = { Bitset::BlockType(InitialValue & 0xFFFFFFFF), Bitset::BlockType((InitialValue & (0xFFFFFFFFull << 32ull)) >> 32ull) };

    return Span<const Bitset::BlockType>(&blocks[0], &blocks[0] + ArraySize(blocks));
}

Bitset::Bitset()
    : m_blocks(CreateBlocks_Static_Internal<0>())
{
}

Bitset::Bitset(uint64 value)
    : m_blocks(CreateBlocks_Internal(value))
{
}

Bitset::Bitset(Bitset&& other) noexcept
    : m_blocks(std::move(other.m_blocks))
{
    other.m_blocks = CreateBlocks_Static_Internal<0>();
}

Bitset& Bitset::operator=(Bitset&& other) noexcept
{
    m_blocks = std::move(other.m_blocks);
    other.m_blocks = CreateBlocks_Static_Internal<0>();

    return *this;
}

Bitset Bitset::operator~() const
{
    Bitset result;
    result.m_blocks.Resize(m_blocks.Size());

    for (uint32 index = 0; index < result.m_blocks.Size(); index++)
    {
        result.m_blocks[index] = ~m_blocks[index];
    }

    result.RemoveLeadingZeros();

    return result;
}

Bitset Bitset::operator<<(uint32 pos) const
{
    Bitset result;

    const SizeType total_bit_size = NumBits();

    for (uint32 combined_bit_index = 0; combined_bit_index < total_bit_size; ++combined_bit_index)
    {
        result.Set(combined_bit_index + pos, Get(combined_bit_index));
    }

    return result;
}

Bitset& Bitset::operator<<=(uint32 pos)
{
    return *this = (*this << pos);
}

Bitset Bitset::operator&(const Bitset& other) const
{
    Bitset result;
    result.m_blocks.Resize(MathUtil::Min(m_blocks.Size(), other.m_blocks.Size()));

    for (uint32 index = 0; index < result.m_blocks.Size(); index++)
    {
        result.m_blocks[index] = m_blocks[index] & other.m_blocks[index];
    }

    result.RemoveLeadingZeros();

    return result;
}

Bitset& Bitset::operator&=(const Bitset& other)
{
    m_blocks.Resize(MathUtil::Min(m_blocks.Size(), other.m_blocks.Size()));

    for (uint32 index = 0; index < m_blocks.Size(); index++)
    {
        m_blocks[index] = m_blocks[index] & other.m_blocks[index];
    }

    RemoveLeadingZeros();

    return *this;
}

Bitset Bitset::operator|(const Bitset& other) const
{
    Bitset result;
    result.m_blocks.Resize(MathUtil::Max(m_blocks.Size(), other.m_blocks.Size()));

    for (uint32 index = 0; index < result.m_blocks.Size(); index++)
    {
        result.m_blocks[index] = (index < m_blocks.Size() ? m_blocks[index] : 0)
            | (index < other.m_blocks.Size() ? other.m_blocks[index] : 0);
    }

    result.RemoveLeadingZeros();

    return result;
}

Bitset& Bitset::operator|=(const Bitset& other)
{
    m_blocks.Resize(MathUtil::Max(m_blocks.Size(), other.m_blocks.Size()));

    for (uint32 index = 0; index < m_blocks.Size(); index++)
    {
        m_blocks[index] = m_blocks[index] | (index < other.m_blocks.Size() ? other.m_blocks[index] : 0);
    }

    RemoveLeadingZeros();

    return *this;
}

Bitset Bitset::operator^(const Bitset& other) const
{
    Bitset result;
    result.m_blocks.Resize(MathUtil::Max(m_blocks.Size(), other.m_blocks.Size()));

    for (uint32 index = 0; index < result.m_blocks.Size(); index++)
    {
        result.m_blocks[index] = (index < m_blocks.Size() ? m_blocks[index] : 0)
            ^ (index < other.m_blocks.Size() ? other.m_blocks[index] : 0);
    }

    result.RemoveLeadingZeros();

    return result;
}

Bitset& Bitset::operator^=(const Bitset& other)
{
    m_blocks.Resize(MathUtil::Max(m_blocks.Size(), other.m_blocks.Size()));

    for (uint32 index = 0; index < m_blocks.Size(); index++)
    {
        m_blocks[index] = m_blocks[index] ^ (index < other.m_blocks.Size() ? other.m_blocks[index] : 0);
    }

    RemoveLeadingZeros();

    return *this;
}

void Bitset::Set(BitIndex index, bool value)
{
    const uint32 block_index = GetBlockIndex(index);

    if (block_index >= m_blocks.Size())
    {
        if (!value)
        {
            return; // not point setting if it's already unset.
        }

        m_blocks.Resize(block_index + 1);
    }

    if (value)
    {
        m_blocks[block_index] |= GetBitMask(index);
    }
    else
    {
        m_blocks[block_index] &= ~GetBitMask(index);
    }
}

void Bitset::Clear()
{
    m_blocks = CreateBlocks_Static_Internal<0>();
}

uint64 Bitset::Count() const
{
    uint64 count = 0;

    for (const BlockType value : m_blocks)
    {
        count += ByteUtil::BitCount(value);
    }

    return count;
}

Bitset& Bitset::Resize(SizeType num_bits)
{
    const SizeType previous_num_blocks = m_blocks.Size();
    const SizeType new_num_blocks = (num_bits + (num_bits_per_block - 1)) / num_bits_per_block;

    m_blocks.Resize(new_num_blocks);

    const SizeType current_num_bits = NumBits();

    // if (current_num_bits > num_bits && !m_blocks.Empty()) {
    //     // @FIXME: Use bitmask
    //     for (uint32 index = num_bits; index < current_num_bits; ++index) {
    //         Set(index, false);
    //     }
    // }

    if (m_blocks.Size() < num_preallocated_blocks)
    {
        m_blocks.Resize(num_preallocated_blocks);
    }

    return *this;
}

Bitset::BitIndex Bitset::FirstSetBitIndex() const
{
    for (uint32 block_index = 0; block_index < m_blocks.Size(); block_index++)
    {
        if (m_blocks[block_index] != 0)
        {
#ifdef HYP_CLANG_OR_GCC
            const uint32 bit_index = __builtin_ffs(m_blocks[block_index]) - 1;
#elif defined(HYP_MSVC)
            unsigned long bit_index = 0;
            _BitScanForward(&bit_index, m_blocks[block_index]);
#endif

            return (block_index * num_bits_per_block) + bit_index;
        }
    }

    return not_found;
}

Bitset::BitIndex Bitset::LastSetBitIndex() const
{
    for (uint32 block_index = m_blocks.Size(); block_index != 0; --block_index)
    {
        if (m_blocks[block_index - 1] != 0)
        {
#ifdef HYP_CLANG_OR_GCC
            const uint32 bit_index = Bitset::num_bits_per_block - __builtin_clz(m_blocks[block_index - 1]) - 1;
#elif defined(HYP_MSVC)
            unsigned long bit_index = 0;
            _BitScanReverse(&bit_index, m_blocks[block_index - 1]);
#endif

            return ((block_index - 1) * num_bits_per_block) + bit_index;
        }
    }

    return not_found;
}

Bitset::BitIndex Bitset::NextSetBitIndex(BitIndex offset) const
{
    const uint32 block_index = GetBlockIndex(offset);

    uint32 mask = ~(GetBitMask(offset) - 1);

    for (uint32 i = block_index; i < m_blocks.Size(); i++)
    {
        if ((m_blocks[i] & mask) != 0)
        {
#ifdef HYP_CLANG_OR_GCC
            const uint32 bit_index = __builtin_ffs(m_blocks[i] & mask) - 1;
#elif defined(HYP_MSVC)
            unsigned long bit_index = 0;
            _BitScanForward(&bit_index, m_blocks[i] & mask);
#endif

            return (i * num_bits_per_block) + bit_index;
        }

        // use all bits in next iteration of loop
        mask = ~0u;
    }

    return not_found;
}

} // namespace containers
} // namespace hyperion