#include <core/memory/pool/Pool.hpp>

namespace hyperion {
namespace memory {

#pragma region Block

Pool::Block::Block(SizeType capacity)
{
    buffer.SetSize(capacity);
    allocator.AddPool(buffer.Data(), buffer.GetCapacity());
}

void* Pool::Block::Allocate(SizeType size, SizeType alignment)
{
    return allocator.Allocate(size, alignment);
}

void Pool::Block::Free(void* ptr)
{
    if (ptr == nullptr)
    {
        return;
    }

    allocator.Free(ptr);
}

#pragma endregion Block

Pool::~Pool()
{
    m_blocks.Clear();
}

HYP_NODISCARD void* Pool::Allocate(SizeType size, SizeType alignment)
{
    for (auto& block : m_blocks)
    {
        void* p = block.Allocate(size, alignment);
        if (p != nullptr)
        {
            return p;
        }
    }

    m_blocks.EmplaceBack(m_blockSize);

    Block& newBlock = m_blocks.Back();

    void* p = newBlock.Allocate(size, alignment);

    if (HYP_UNLIKELY(p == nullptr))
    {
        HYP_FAIL("Failed to allocate from new block!");
    }

    return p;
}

void Pool::Free(void* ptr)
{
    for (auto& block : m_blocks)
    {
        ubyte* base = reinterpret_cast<ubyte*>(block.buffer.Data());
        ubyte* bptr = reinterpret_cast<ubyte*>(ptr);
        if (bptr > base && bptr <= base + block.buffer.GetCapacity())
        {
            block.Free(ptr);
            return;
        }
    }

    // not found
    HYP_FAIL("Pointer {} not found in any pool block!", ptr);
}

void Pool::Reset()
{
    m_blocks.Clear();
}

MemoryMetrics Pool::GetMemoryMetrics() const
{
    MemoryMetrics metrics;

    for (const auto& block : m_blocks)
    {
        const SizeType blockCapacity = block.buffer.GetCapacity();

        // With TLSF allocator, we can get accurate statistics from the allocator itself
        MemoryMetrics blockMetrics = block.allocator.GetMemoryMetrics();
        metrics += blockMetrics;
    }

    return metrics;
}

} // namespace memory
} // namespace hyperion