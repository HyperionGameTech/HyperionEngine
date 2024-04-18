/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/containers/Bitset.hpp>

#include <util/ByteUtil.hpp>

#ifdef HYP_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <bitset> // for output

namespace hyperion {

namespace containers {

Bitset::Bitset(uint64 value)
    : m_blocks { BlockType(value & 0xFFFFFFFF), BlockType((value & (0xFFFFFFFFull << 32ull)) >> 32ull) }
{
    RemoveLeadingZeros();
}

Bitset Bitset::operator~() const
{
    Bitset result;
    result.m_blocks.Resize(m_blocks.Size());

    for (uint32 index = 0; index < result.m_blocks.Size(); index++) {
        result.m_blocks[index] = ~m_blocks[index];
    }

    result.RemoveLeadingZeros();

    return result;
}

Bitset Bitset::operator<<(uint32 pos) const
{
    Bitset result;

    const uint32 total_bit_size = NumBits();

    for (uint32 combined_bit_index = 0; combined_bit_index < total_bit_size; ++combined_bit_index) {
        result.Set(combined_bit_index + pos, Get(combined_bit_index));
    }

    return result;
}

Bitset &Bitset::operator<<=(uint32 pos)
    { return *this = (*this << pos); }

Bitset Bitset::operator&(const Bitset &other) const
{
    Bitset result;
    result.m_blocks.Resize(MathUtil::Min(m_blocks.Size(), other.m_blocks.Size()));

    for (uint32 index = 0; index < result.m_blocks.Size(); index++) {
        result.m_blocks[index] = m_blocks[index] & other.m_blocks[index];
    }

    result.RemoveLeadingZeros();

    return result;
}

Bitset &Bitset::operator&=(const Bitset &other)
    { return *this = (*this & other); }

Bitset Bitset::operator|(const Bitset &other) const
{
    Bitset result;
    result.m_blocks.Resize(MathUtil::Max(m_blocks.Size(), other.m_blocks.Size()));

    for (uint32 index = 0; index < result.m_blocks.Size(); index++) {
        result.m_blocks[index] = (index < m_blocks.Size() ? m_blocks[index] : 0)
            | (index < other.m_blocks.Size() ? other.m_blocks[index] : 0);
    }

    result.RemoveLeadingZeros();

    return result;
}

Bitset &Bitset::operator|=(const Bitset &other)
    { return *this = (*this | other); }

Bitset Bitset::operator^(const Bitset &other) const
{
    Bitset result;
    result.m_blocks.Resize(MathUtil::Max(m_blocks.Size(), other.m_blocks.Size()));

    for (uint32 index = 0; index < result.m_blocks.Size(); index++) {
        result.m_blocks[index] = (index < m_blocks.Size() ? m_blocks[index] : 0)
            ^ (index < other.m_blocks.Size() ? other.m_blocks[index] : 0);
    }

    result.RemoveLeadingZeros();

    return result;
}

Bitset &Bitset::operator^=(const Bitset &other)
    { return *this = (*this ^ other); }

void Bitset::Set(uint32 index, bool value)
{
    const uint32 bit_index = GetBlockIndex(index);

    if (bit_index >= m_blocks.Size()) {
        if (!value) {
            return; // not point setting if it's already unset.
        }

        m_blocks.Resize(bit_index + 1);
    }

    if (value) {
        m_blocks[bit_index] |= GetBitMask(index);
    } else {
        m_blocks[bit_index] &= ~GetBitMask(index);
    }
}

uint64 Bitset::Count() const
{
    uint64 count = 0;

    for (const auto value : m_blocks) {
        count += ByteUtil::BitCount(value);
    }

    return count;
}

Bitset &Bitset::Resize(uint32 num_bits, bool value)
{
    const uint32 previous_num_blocks = m_blocks.Size();
    const uint32 new_num_blocks = (num_bits + (num_bits_per_block - 1)) / num_bits_per_block;

    if (new_num_blocks < 2) {
        return *this;
    }

    m_blocks.Resize(new_num_blocks);

    // if the new number of blocks is greater than the previous number of blocks, set the new blocks to the value
    if (new_num_blocks > previous_num_blocks) {
        // if value has been set to true, set the new blocks to all 1 bits
        if (value) {
            for (uint32 index = previous_num_blocks; index < new_num_blocks; index++) {
                m_blocks[index] = ~BlockType(0);
            }
        }
    }

    return *this;
}

uint64 Bitset::FirstSetBitIndex() const
{
    for (uint32 block_index = 0; block_index < m_blocks.Size(); block_index++) {
        if (m_blocks[block_index] != 0) {
#ifdef HYP_CLANG_OR_GCC
            const uint32 bit_index = __builtin_ffsll(m_blocks[block_index]) - 1;
#elif defined(HYP_MSVC)
            unsigned long bit_index = 0;
            _BitScanForward64(&bit_index, m_blocks[block_index]);
#endif

            return (block_index * Bitset::num_bits_per_block) + bit_index;
        }
    }

    return -1; // No set bit found
}

void Bitset::RemoveLeadingZeros()
{
    while (!m_blocks.Empty() && m_blocks.Back() == 0) {
        m_blocks.PopBack();
    }
}

std::ostream &operator<<(std::ostream &os, const Bitset &bitset)
{
    for (uint32 block_index = bitset.m_blocks.Size(); block_index != 0; --block_index) {
        for (uint32 bit_index = Bitset::num_bits_per_block; bit_index != 0; --bit_index) {
            const uint32 combined_bit_index = ((block_index - 1) * Bitset::num_bits_per_block) + (bit_index - 1);

            os << (bitset.Get(combined_bit_index) ? '1' : '0');

            if (((bit_index - 1) % CHAR_BIT) == 0) {
                os << ' ';
            }
        }

    }

    return os;
}

} // namespace containers
} // namespace hyperion