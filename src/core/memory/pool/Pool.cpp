#include <core/memory/pool/Pool.hpp>

namespace hyperion {
namespace memory {

Pool::~Pool() = default;

HYP_NODISCARD void* Pool::Alloc(SizeType size, SizeType alignment)
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
    // Fast path: read header to find owner
    using AllocHeader = Block::AllocHeader;

    AllocHeader* hdr = reinterpret_cast<AllocHeader*>(reinterpret_cast<ubyte*>(ptr) - sizeof(AllocHeader));
    Block* owner = hdr->owner;

    if (owner != nullptr)
    {
        ubyte* base = reinterpret_cast<ubyte*>(owner->buffer.Data());
        ubyte* bptr = reinterpret_cast<ubyte*>(ptr);
        if (bptr > base && bptr <= base + owner->buffer.GetCapacity())
        {
            owner->Free(ptr);
            return;
        }
    }

    // Fallback: search blocks
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

} // namespace memory
} // namespace hyperion