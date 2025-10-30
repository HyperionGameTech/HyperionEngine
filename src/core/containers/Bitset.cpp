/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#include <core/containers/Bitset.hpp>
#include <core/utilities/Span.hpp>
#include <core/Util.hpp>

#include <core/utilities/ByteUtil.hpp>

#ifdef HYP_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace hyperion {
namespace containers {

static Array<Bitset::BlockType, InlineAllocator<2>> CreateBlocks_Internal(uint64 value)
{
    return { Bitset::BlockType(value & 0xFFFFFFFFu), Bitset::BlockType((value & (0xFFFFFFFFull << 32ull)) >> 32ull) };
}

template <uint64 InitialValue>
static Span<const Bitset::BlockType> CreateBlocks_Static_Internal()
{
    static const Bitset::BlockType s_blocks[] = {
        Bitset::BlockType(InitialValue & 0xFFFFFFFFu),
        Bitset::BlockType((InitialValue & (0xFFFFFFFFull << 32ull)) >> 32ull)
    };

    return Span<const Bitset::BlockType>(&s_blocks[0], &s_blocks[0] + HYP_ARRAY_SIZE(s_blocks));
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
    result.m_blocks.ResizeUninitialized(m_blocks.Size());

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

    const SizeType totalBitSize = NumBits();

    for (uint32 combinedBitIndex = 0; combinedBitIndex < totalBitSize; ++combinedBitIndex)
    {
        result.Set(combinedBitIndex + pos, Get(combinedBitIndex));
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
    const uint32 blockIndex = GetBlockIndex(index);

    if (blockIndex >= m_blocks.Size())
    {
        if (!value)
        {
            return; // not point setting if it's already unset.
        }

        m_blocks.Resize(blockIndex + 1);
    }

    if (value)
    {
        m_blocks[blockIndex] |= GetBitMask(index);
    }
    else
    {
        m_blocks[blockIndex] &= ~GetBitMask(index);

        // RemoveLeadingZeros();
    }
}

void Bitset::Clear()
{
    m_blocks = CreateBlocks_Static_Internal<0>();
}

Bitset& Bitset::SetNumBits(SizeType numBits)
{
    const SizeType previousNumBlocks = m_blocks.Size();
    SizeType newNumBlocks = (numBits + (NumBitsPerBlock - 1)) / NumBitsPerBlock;

    if (newNumBlocks < NumPreallocatedBlocks)
    {
        newNumBlocks = NumPreallocatedBlocks;
    }

    if (newNumBlocks == m_blocks.Size())
    {
        // no need to resize if it would be the same
        return *this;
    }

    m_blocks.Resize(newNumBlocks);

    return *this;
}

} // namespace containers
} // namespace hyperion