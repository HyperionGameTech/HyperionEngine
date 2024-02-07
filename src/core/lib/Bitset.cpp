#include <core/lib/Bitset.hpp>

#ifdef HYP_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <bitset> // for output

namespace hyperion {

namespace containers {
namespace detail {

DynBitset::DynBitset(UInt64 value)
    : m_blocks { BlockType(value & 0xFFFFFFFF), BlockType((value & (0xFFFFFFFFull << 32ull)) >> 32ull) }
{
    RemoveLeadingZeros();
}

DynBitset DynBitset::operator~() const
{
    DynBitset result;
    result.m_blocks.Resize(m_blocks.Size());

    for (SizeType index = 0; index < result.m_blocks.Size(); index++) {
        result.m_blocks[index] = ~m_blocks[index];
    }

    result.RemoveLeadingZeros();

    return result;
}

DynBitset DynBitset::operator<<(SizeType pos) const
{
    DynBitset result;

    const SizeType total_bit_size = NumBits();

    for (SizeType combined_bit_index = 0; combined_bit_index < total_bit_size; ++combined_bit_index) {
        result.Set(combined_bit_index + pos, Get(combined_bit_index));
    }

    return result;
}

DynBitset &DynBitset::operator<<=(SizeType pos)
    { return *this = (*this << pos); }

DynBitset DynBitset::operator&(const DynBitset &other) const
{
    DynBitset result;
    result.m_blocks.Resize(MathUtil::Min(m_blocks.Size(), other.m_blocks.Size()));

    for (SizeType index = 0; index < result.m_blocks.Size(); index++) {
        result.m_blocks[index] = m_blocks[index] & other.m_blocks[index];
    }

    result.RemoveLeadingZeros();

    return result;
}

DynBitset &DynBitset::operator&=(const DynBitset &other)
    { return *this = (*this & other); }

DynBitset DynBitset::operator|(const DynBitset &other) const
{
    DynBitset result;
    result.m_blocks.Resize(MathUtil::Max(m_blocks.Size(), other.m_blocks.Size()));

    for (SizeType index = 0; index < result.m_blocks.Size(); index++) {
        result.m_blocks[index] = (index < m_blocks.Size() ? m_blocks[index] : 0)
            | (index < other.m_blocks.Size() ? other.m_blocks[index] : 0);
    }

    result.RemoveLeadingZeros();

    return result;
}

DynBitset &DynBitset::operator|=(const DynBitset &other)
    { return *this = (*this | other); }

DynBitset DynBitset::operator^(const DynBitset &other) const
{
    DynBitset result;
    result.m_blocks.Resize(MathUtil::Max(m_blocks.Size(), other.m_blocks.Size()));

    for (SizeType index = 0; index < result.m_blocks.Size(); index++) {
        result.m_blocks[index] = (index < m_blocks.Size() ? m_blocks[index] : 0)
            ^ (index < other.m_blocks.Size() ? other.m_blocks[index] : 0);
    }

    result.RemoveLeadingZeros();

    return result;
}

DynBitset &DynBitset::operator^=(const DynBitset &other)
    { return *this = (*this ^ other); }

void DynBitset::Set(SizeType index, Bool value)
{
    const SizeType bit_index = GetBlockIndex(index);

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

SizeType DynBitset::Count() const
{
    SizeType count = 0;

    for (const auto value : m_blocks) {
        count += MathUtil::BitCount(value);
    }

    return count;
}

UInt32 DynBitset::ToUInt32() const
{
    if (m_blocks.Empty()) {
        return 0;
    } else {
        return m_blocks[0];
    }
}

Bool DynBitset::ToUInt32(UInt32 *out) const
{
    if (m_blocks.Empty()) {
        *out = 0;

        return true;
    } else {
        *out = m_blocks[0];

        return m_blocks.Size() == 1;
    }
}

UInt64 DynBitset::ToUInt64() const
{
    if (m_blocks.Empty()) {
        return 0;
    } else if (m_blocks.Size() == 1) {
        return UInt64(m_blocks[0]);
    } else {
        return UInt64(m_blocks[0]) | (UInt64(m_blocks[1]) << 32);
    }
}

Bool DynBitset::ToUInt64(UInt64 *out) const
{
    if (m_blocks.Empty()) {
        *out = 0;

        return true;
    } else if (m_blocks.Size() == 1) {
        *out = UInt64(m_blocks[0]);

        return true;
    } else {
        *out = UInt64(m_blocks[0]) | (UInt64(m_blocks[1]) << 32);

        return m_blocks.Size() == 2;
    }
}

DynBitset &DynBitset::Resize(SizeType num_bits, bool value)
{
    const SizeType previous_num_blocks = m_blocks.Size();
    const SizeType new_num_blocks = (num_bits + (num_bits_per_block - 1)) / num_bits_per_block;

    if (new_num_blocks < 2) {
        return *this;
    }

    m_blocks.Resize(new_num_blocks);

    // if the new number of blocks is greater than the previous number of blocks, set the new blocks to the value
    if (new_num_blocks > previous_num_blocks) {
        // if value has been set to true, set the new blocks to all 1 bits
        if (value) {
            for (SizeType index = previous_num_blocks; index < new_num_blocks; index++) {
                m_blocks[index] = ~BlockType(0);
            }
        }
    }

    return *this;
}

SizeType DynBitset::FirstSetBitIndex() const
{
    for (SizeType block_index = 0; block_index < m_blocks.Size(); block_index++) {
        if (m_blocks[block_index] != 0) {
#ifdef HYP_CLANG_OR_GCC
            const SizeType bit_index = __builtin_ffsll(m_blocks[block_index]) - 1;
#elif defined(HYP_MSVC)
            unsigned long bit_index = 0;
            _BitScanForward64(&bit_index, m_blocks[block_index]);
#endif

            return (block_index * DynBitset::num_bits_per_block) + bit_index;
        }
    }

    return -1; // No set bit found
}

void DynBitset::RemoveLeadingZeros()
{
    while (!m_blocks.Empty() && m_blocks.Back() == 0) {
        m_blocks.PopBack();
    }
}

std::ostream &operator<<(std::ostream &os, const DynBitset &bitset)
{
    for (SizeType block_index = bitset.m_blocks.Size(); block_index != 0; --block_index) {
        for (SizeType bit_index = DynBitset::num_bits_per_block; bit_index != 0; --bit_index) {
            const SizeType combined_bit_index = ((block_index - 1) * DynBitset::num_bits_per_block) + (bit_index - 1);

            os << (bitset.Get(combined_bit_index) ? '1' : '0');

            if (((bit_index - 1) % CHAR_BIT) == 0) {
                os << ' ';
            }
        }

    }

    return os;
}

} // namespace detail
} // namespace containers

} // namespace hyperion