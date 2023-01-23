#include <core/lib/Bitset.hpp>

#include <bitset> // for output

namespace hyperion {

namespace containers {
namespace detail {

DynBitset::DynBitset(UInt64 value)
    : m_blocks { BlockType(value & 0xFFFFFFFF), BlockType((value & (0xFFFFFFFFull << 32ull)) >> 32ull) }
{
    RemoveLeadingZeros();
}

// DynBitset DynBitset::operator~() const
// {
//     DynBitset result = *this;

//     for (SizeType index = 0; index < result.m_blocks.Size(); ++index) {
//         result.m_blocks[index] = ~result.m_blocks[index];
//     }

//     result.RemoveLeadingZeros();

//     return result;
// }

DynBitset DynBitset::operator<<(SizeType pos) const
{
    DynBitset result;

    const SizeType total_bit_size = TotalBitCount();

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

bool DynBitset::Get(SizeType index) const
{
    return GetBlockIndex(index) < m_blocks.Size()
        && (m_blocks[GetBlockIndex(index)] & GetBitMask(index));
}

void DynBitset::Set(SizeType index, bool value)
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

bool DynBitset::ToUInt32(UInt32 *out) const
{
    if (m_blocks.Empty()) {
        *out = 0;

        return true;
    } else {
        *out = m_blocks[0];

        return m_blocks.Size() == 1;
    }
}

bool DynBitset::ToUInt64(UInt64 *out) const
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

void DynBitset::RemoveLeadingZeros()
{
    while (m_blocks.Any()) {
        if (m_blocks.Back() == 0) {
            m_blocks.PopBack();
        } else {
            break;
        }
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