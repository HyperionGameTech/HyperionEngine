/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <RenderingPch.hpp>

#include <Rendering/CBufferAllocator.hpp>
#include <Rendering/RenderInterface.hpp>

#include <Rendering/util/DeletionQueue.hpp>

namespace Hyperion {

static constexpr size_t CBufferSize = 65536;

struct CBufferAllocatorBlock
{
    HYP_DEF_POOL_NEW_DELETE(g_renderPool);

    GpuBuffer* buffer;
    size_t offset;
    uint32 lastUsedFrame;

    CBufferAllocatorBlock()
        : buffer(new GpuBuffer{ GpuBufferType::ConstantBuffer, CBufferSize, 256 }),
          offset(0),
          lastUsedFrame(UINT32_MAX)
    {
    }

    CBufferAllocatorBlock(const CBufferAllocatorBlock&) = delete;
    CBufferAllocatorBlock& operator=(const CBufferAllocatorBlock&) = delete;

    CBufferAllocatorBlock(CBufferAllocatorBlock&& other) noexcept
        : buffer(other.buffer),
          offset(0),
          lastUsedFrame(other.lastUsedFrame)
    {
        other.buffer = nullptr;
        other.lastUsedFrame = UINT32_MAX;
        other.offset = 0;
    }

    CBufferAllocatorBlock& operator=(CBufferAllocatorBlock&& other) noexcept
    {
        delete buffer;

        buffer = other.buffer;
        other.buffer = nullptr;

        offset = other.offset;
        other.offset = 0;

        lastUsedFrame = other.lastUsedFrame;
        other.lastUsedFrame = UINT32_MAX;

        return *this;
    }

    ~CBufferAllocatorBlock()
    {
        delete buffer;
    }
};

CBufferAllocator::CBufferAllocator()
    : m_minAllocationAlignment(0),
      m_scratchAlignment {}
{
}

CBufferAllocator::~CBufferAllocator()
{
    for (auto& scratch : m_scratch)
    {
        scratch.Clear();
    }

    m_blocks.Clear();

    for (auto& frameBlocks : m_currentFrameBlocks)
    {
        frameBlocks.Clear();
    }
}

void CBufferAllocator::Initialize(size_t minAllocationAlignment)
{
    m_minAllocationAlignment = minAllocationAlignment;
}

void CBufferAllocator::OnFrameStart()
{
    for (auto& scratch : m_scratch)
    {
        scratch.SetCapacity(2048);
    }
}

// only ever called after all workers are done.
void CBufferAllocator::OnFrameEnd()
{
    AssertOnThread(g_renderThread);

    for (uint32 idx = 0; idx < NumRendererWorkerThreads + 1; idx++)
    {
        m_scratch[idx].Clear();

        for (Block& block : m_currentFrameBlocks[idx])
        {
            size_t flushSize = MathUtil::Min(block.offset, CBufferSize);

            if (flushSize != 0)
            {
                block.buffer->Flush(0, flushSize);
            }

            block.offset = 0;

            m_blocks.PushBack(std::move(block));
        }

        m_currentFrameBlocks[idx].Clear();
    }
}

void CBufferAllocator::Write(const void* src, size_t count, size_t alignment)
{
    if (count == 0)
        return;

    AssertDebug(src != nullptr);

    const uint32 idx = CurrentRenderThreadIndex();

    auto& scratch = m_scratch[idx];

    const size_t alignedCount = alignment > 0 ? ByteUtil::AlignAs(count, alignment) : count;
    const size_t scratchOffset = ByteUtil::AlignAs(scratch.Size(), alignment);

    scratch.SetSize(scratchOffset + alignedCount);

    m_scratchAlignment[idx] = MathUtil::Max(m_scratchAlignment[idx], alignment);

    Memory::Copy(scratch.Data() + scratchOffset, reinterpret_cast<const ubyte*>(src), count);
}

void CBufferAllocator::Commit(GpuBuffer*& outBuffer, size_t& outOffset, size_t& outSize)
{
    const uint32 idx = CurrentRenderThreadIndex();

    auto& scratch = m_scratch[idx];
    size_t& scratchAlignment = m_scratchAlignment[idx];

    if (scratch.Empty())
    {
        outBuffer = nullptr;
        outOffset = 0;
        outSize = 0;
        return;
    }

    void* ptr = Allocate(scratch.Size(), scratchAlignment, outBuffer, outOffset);
    AssertDebug(ptr != nullptr && outBuffer != nullptr);
    AssertDebug(outOffset % m_minAllocationAlignment == 0);

    Memory::Copy(ptr, scratch.Data(), scratch.Size());

    outSize = scratch.Size();

    // keep memory around
    scratch.SetSize(0);
    scratchAlignment = 0;
}

void* CBufferAllocator::Allocate(size_t count, size_t alignment, GpuBuffer*& outBuffer, size_t& outStartOffset)
{
    if (alignment < m_minAllocationAlignment)
        alignment = m_minAllocationAlignment;

    const uint32 idx = CurrentRenderThreadIndex();
    const uint32 currentFrameCounter = GetFrameCounter();

    outBuffer = nullptr;
    outStartOffset = 0;

    if (m_currentFrameBlocks[idx].Any())
    {
        Block& lastBlock = m_currentFrameBlocks[idx].Back();

        const size_t offset = ByteUtil::AlignAs(lastBlock.offset, alignment);

        if (offset + count <= CBufferSize)
        {
            void* ptr = (void*)(reinterpret_cast<UIntPtr>(lastBlock.buffer->Map()) + offset);

            outBuffer = lastBlock.buffer;
            outStartOffset = offset;

            lastBlock.offset = offset + count;

            return ptr;
        }
    }

    // allocate a new block
    Block* newBlock = TryGetRecycledBlock(currentFrameCounter);

    if (!newBlock)
        newBlock = NewBlock(currentFrameCounter);

    Assert(count <= CBufferSize && newBlock->offset == 0);

    outBuffer = newBlock->buffer;
    outStartOffset = 0;

    void* ptr = newBlock->buffer->Map();
    Assert(ptr != nullptr);

    newBlock->offset += count;

    return ptr;
}

CBufferAllocator::Block* CBufferAllocator::NewBlock(uint32 currentFrameCounter)
{
    const uint32 idx = CurrentRenderThreadIndex();

    Block& newBlock = m_currentFrameBlocks[idx].EmplaceBack();
    newBlock.lastUsedFrame = currentFrameCounter;
    newBlock.offset = 0;

    CheckResult(newBlock.buffer->Create());

    return &newBlock;
}

CBufferAllocator::Block* CBufferAllocator::TryGetRecycledBlock(uint32 currentFrameCounter)
{
    const uint32 idx = CurrentRenderThreadIndex();

    TUniqueLock lock(m_mutex);

    constexpr uint32 MinDiff = NumFramesInFlight;

    for (auto it = m_blocks.Begin(); it != m_blocks.End();)
    {
        Block& block = *it;

        if (currentFrameCounter - block.lastUsedFrame >= MinDiff)
        {
            block.offset = 0;
            block.lastUsedFrame = currentFrameCounter;

            Block& newBlock = m_currentFrameBlocks[idx].PushBack(std::move(block));

            it = m_blocks.Erase(it);

            return &newBlock;
        }

        ++it;
    }

    return nullptr;
}

} // namespace Hyperion
