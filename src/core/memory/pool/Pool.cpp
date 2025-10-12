#include <core/memory/pool/Pool.hpp>

#include <thirdparty/tlsf/tlsf.h>

namespace hyperion {
namespace memory {

struct Block
{
    ByteBuffer buffer;

    tlsf_t tlsf;

    explicit Block(SizeType capacity)
    {
        buffer.SetSize(capacity + tlsf_size());

        tlsf = tlsf_create_with_pool(buffer.Data(), buffer.Size());

        if (!tlsf)
        {
            HYP_FAIL("Failed to create TLSF allocator!");
        }
    }

    ~Block()
    {
        if (tlsf)
        {
            int status = tlsf_check(tlsf);

            if (status != 0)
            {
                HYP_FAIL("TLSF allocator is corrupt! Status: {}", status);
            }

            tlsf_destroy(tlsf);
            tlsf = nullptr;
        }
    }

    void* Allocate(SizeType size, SizeType alignment)
    {
        return tlsf_memalign(tlsf, alignment, size);
    }

    void Free(void* ptr)
    {
        tlsf_free(tlsf, ptr);
    }
};

struct BlockStorage : LinkedList<Block>
{
};

static constexpr SizeType DefaultBlockSize = 1024 * 1024; // 1 MB

Pool::Pool()
    : Pool(DefaultBlockSize)
{
}

Pool::Pool(SizeType blockSize)
    : m_blockSize(blockSize)
{
    HYP_CORE_ASSERT(blockSize > sizeof(void*), "Block size must be greater than pointer size");

    m_blocks = new BlockStorage();
}

Pool::~Pool()
{
    delete m_blocks;
}

HYP_NODISCARD void* Pool::Alloc(SizeType size, SizeType alignment)
{
    HYP_CORE_ASSERT(alignment <= 16);

    size = ByteUtil::AlignAs(size, alignment);

    for (Block& block : *m_blocks)
    {
        void* p = block.Allocate(size, alignment);

        if (p)
        {
            return p;
        }
    }

    // allocate new block
    Block& newBlock = m_blocks->EmplaceBack(m_blockSize);
    void* p = newBlock.Allocate(size, alignment);

    if (HYP_UNLIKELY(!p))
    {
        HYP_FAIL("Failed to allocate from new block!");
    }

    return p;

#if 0
    // alignment must be at least alignof(AllocHeader)
    const SizeType headerAlign = alignof(AllocHeader);
    const SizeType headerSize = sizeof(AllocHeader);

    alignment = MathUtil::Max(alignment, headerAlign);

    // add space for header + potential padding
    const SizeType totalSize = ByteUtil::AlignAs(headerSize + size, alignment);

    for (auto& block : *m_blocks)
    {
        void* raw = block.Allocate(totalSize, alignment);

        if (raw)
        {
            UIntPtr base = reinterpret_cast<UIntPtr>(raw);
            UIntPtr addr = base + headerSize;

            AllocHeader* hdr = reinterpret_cast<AllocHeader*>(addr);
            hdr->owner = &block;

            return reinterpret_cast<void*>(addr + headerSize);
        }
    }

    // allocate new block if needed
    Block& newBlock = m_blocks->EmplaceBack(m_blockSize);

    void* raw = newBlock.Allocate(totalSize, alignment);

    if (!raw)
    {
        HYP_FAIL("Failed to allocate from new block!");
    }
    
    UIntPtr base = reinterpret_cast<UIntPtr>(raw);
    UIntPtr addr = base + headerSize;

    AllocHeader* hdr = reinterpret_cast<AllocHeader*>(addr);
    hdr->owner = &newBlock;

    return reinterpret_cast<void*>(addr + headerSize);
#endif
}

void Pool::Free(void* ptr)
{
    if (!ptr)
    {
        return;
    }

    for (Block& block : *m_blocks)
    {
        ubyte* base = reinterpret_cast<ubyte*>(block.buffer.Data());
        ubyte* bptr = reinterpret_cast<ubyte*>(ptr);
        if (bptr > base && bptr < base + block.buffer.Size())
        {
            block.Free(ptr);
            return;
        }
    }

    HYP_FAIL("Pointer {} not found in any pool block!", ptr);

#if 0
    // Fast path: read header to find owner
    AllocHeader* hdr = reinterpret_cast<AllocHeader*>(reinterpret_cast<UIntPtr>(ptr) - sizeof(AllocHeader));
    Block* owner = hdr->owner;

    if (owner != nullptr)
    {
        UIntPtr base = reinterpret_cast<UIntPtr>(owner->buffer.Data());
        UIntPtr bptr = reinterpret_cast<UIntPtr>(ptr);

        if (bptr > base && bptr <= base + owner->buffer.GetCapacity())
        {
            owner->Free(ptr);
            return;
        }
    }

    // Fallback: search blocks
    for (auto& block : *m_blocks)
    {
        UIntPtr base = reinterpret_cast<UIntPtr>(block.buffer.Data());
        UIntPtr bptr = reinterpret_cast<UIntPtr>(ptr);

        if (bptr > base && bptr <= base + block.buffer.GetCapacity())
        {
            block.Free(ptr);
            return;
        }
    }

    // not found
    HYP_FAIL("Pointer {} not found in any pool block!", ptr);
#endif
}

void Pool::Reset()
{
    m_blocks->Clear();
}

} // namespace memory
} // namespace hyperion