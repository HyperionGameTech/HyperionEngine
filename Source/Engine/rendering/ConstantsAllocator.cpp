/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/ConstantsAllocator.hpp>
#include <rendering/RenderInterface.hpp>

#include <rendering/util/DeletionQueue.hpp>

namespace Hyperion {

static constexpr size_t ConstantBufferSize = 65536;

struct ConstantsAllocatorBlock
{
    HYP_DEF_POOL_NEW_DELETE(g_renderPool);

    GpuBuffer* buffer;
    size_t offset;
    uint32 lastUsedFrame;

    ConstantsAllocatorBlock()
        : buffer(new GpuBuffer{ GpuBufferType::CONSTANT_BUFFER, ConstantBufferSize, 256 }),
          offset(0),
          lastUsedFrame(size_t(-1))
    {
    }

    ConstantsAllocatorBlock(const ConstantsAllocatorBlock&) = delete;
    ConstantsAllocatorBlock& operator=(const ConstantsAllocatorBlock&) = delete;
    
    ConstantsAllocatorBlock(ConstantsAllocatorBlock&& other) noexcept
        : buffer(other.buffer),
          offset(0),
          lastUsedFrame(other.lastUsedFrame)
    {
        other.buffer = nullptr;
        other.lastUsedFrame = size_t(-1);
        other.offset = 0;
    }

    ConstantsAllocatorBlock& operator=(ConstantsAllocatorBlock&& other) noexcept
    {
        delete buffer;

        buffer = other.buffer;
        other.buffer = nullptr;

        offset = other.offset;
        other.offset = 0;

        lastUsedFrame = other.lastUsedFrame;
        other.lastUsedFrame = size_t(-1);

        return *this;
    }

    ~ConstantsAllocatorBlock()
    {
        delete buffer;
    }
};

ConstantsAllocator::ConstantsAllocator()
    : m_minAllocationAlignment(0),
      m_scratchAlignment(0)
{
}

ConstantsAllocator::~ConstantsAllocator()
{
    m_scratch.Clear();
    
    for (Block& block : m_blocks)
    {
        delete block.buffer;
    }

    m_blocks.Clear();

    for (Block& block : m_currentFrameBlocks)
    {
        delete block.buffer;
    }

    m_currentFrameBlocks.Clear();
}

void ConstantsAllocator::Initialize(size_t minAllocationAlignment)
{
    m_minAllocationAlignment = minAllocationAlignment;
}

void ConstantsAllocator::OnFrameStart()
{
    m_scratch.SetCapacity(2048);
}

void ConstantsAllocator::OnFrameEnd()
{
    m_scratch.Clear();

    for (Block& block : m_currentFrameBlocks)
    {
        block.offset = 0;

        m_blocks.PushBack(std::move(block));
    }

    m_currentFrameBlocks.Clear();
}

void ConstantsAllocator::Write(const void* src, size_t count, size_t alignment)
{
    if (count == 0)
        return;

    AssertDebug(src != nullptr);

    const size_t alignedCount = alignment > 0 ? ByteUtil::AlignAs(count, alignment) : count;
    const size_t scratchOffset = ByteUtil::AlignAs(m_scratch.Size(), alignment);

    m_scratch.SetSize(scratchOffset + alignedCount);

    m_scratchAlignment = MathUtil::Max(m_scratchAlignment, alignment);

    Memory::Copy(m_scratch.Data() + scratchOffset, reinterpret_cast<const ubyte*>(src), count);
}

void ConstantsAllocator::Commit(GpuBuffer*& outBuffer, size_t& outOffset, size_t& outSize)
{
    if (m_scratch.Empty())
    {
        outBuffer = nullptr;
        outOffset = 0;
        outSize = 0;
        return;
    }

    void* ptr = Allocate(m_scratch.Size(), m_scratchAlignment, outBuffer, outOffset);
    AssertDebug(ptr != nullptr && outBuffer != nullptr);

    Memory::Copy(ptr, m_scratch.Data(), m_scratch.Size());

    outSize = m_scratch.Size();

    // keep memory around
    m_scratch.SetSize(0);
    m_scratchAlignment = 0;
}

void* ConstantsAllocator::Allocate(size_t count, size_t alignment, GpuBuffer*& outBuffer, size_t& outStartOffset)
{
    if (alignment < m_minAllocationAlignment)
        alignment = m_minAllocationAlignment;

    const size_t alignedCount = ByteUtil::AlignAs(count, alignment);

    const uint32 currentFrameCounter = GetFrameCounter();
    
    outBuffer = nullptr;
    outStartOffset = 0;

    if (m_currentFrameBlocks.Any())
    {
        Block& lastBlock = m_currentFrameBlocks.Back();

        const size_t offset = ByteUtil::AlignAs(lastBlock.offset, alignment);

        if (offset + alignedCount <= ConstantBufferSize)
        {
            void* ptr = (void*)(reinterpret_cast<UIntPtr>(lastBlock.buffer->Map()) + offset);

            outBuffer = lastBlock.buffer;
            outStartOffset = lastBlock.offset;

            lastBlock.offset = offset + alignedCount;

            return ptr;
        }
    }

    // allocate a new block
    Block* newBlock = TryGetRecycledBlock(currentFrameCounter);
    
    if (!newBlock)
        newBlock = NewBlock(currentFrameCounter);

    Assert(alignedCount <= ConstantBufferSize);
    
    outBuffer = newBlock->buffer;
    outStartOffset = newBlock->offset;

    void* ptr = newBlock->buffer->Map();

    newBlock->offset += alignedCount;

    return ptr;
}

ConstantsAllocator::Block* ConstantsAllocator::NewBlock(uint32 currentFrameCounter)
{
    Block& newBlock = m_currentFrameBlocks.EmplaceBack();
    newBlock.lastUsedFrame = currentFrameCounter;
    newBlock.offset = 0;

    CheckResult(newBlock.buffer->Create());

    return &newBlock;
}

ConstantsAllocator::Block* ConstantsAllocator::TryGetRecycledBlock(uint32 currentFrameCounter)
{
    constexpr uint32 MinDiff = NumFramesInFlight;

    for (auto it = m_blocks.Begin(); it != m_blocks.End();)
    {
        Block& block = *it;

        if (currentFrameCounter - block.lastUsedFrame >= MinDiff)
        {
            block.offset = 0;
            block.lastUsedFrame = currentFrameCounter;

            Block& newBlock = m_currentFrameBlocks.PushBack(std::move(block));

            it = m_blocks.Erase(it);

            return &newBlock;
        }

        ++it;
    }

    return nullptr;
}

} // namespace Hyperion
