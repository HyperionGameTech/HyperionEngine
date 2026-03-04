/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/ConstantsAllocator.hpp>
#include <rendering/RenderInterface.hpp>

#include <rendering/util/DeletionQueue.hpp>

namespace Hyperion {

static constexpr size_t ConstantBufferSize = 65536;


struct ConstantAllocatorBlock
{
    HYP_DEF_POOL_NEW_DELETE(g_renderPool);

    GpuBuffer buffer { GpuBufferType::CONSTANT_BUFFER, ConstantBufferSize, 256 };
    uint32 frameCounter = uint32(-1); // last used frame
    uint32 offset = 0;
};

ConstantsAllocator::ConstantsAllocator()
{
}

ConstantsAllocator::~ConstantsAllocator()
{
    m_scratch.Clear();
    
    for (Block* block : m_blocks)
    {
        delete block;
    }

    m_blocks.Clear();

    for (Block* block : m_currentFrameBlocks)
    {
        delete block;
    }

    m_currentFrameBlocks.Clear();
}

void ConstantsAllocator::OnFrameStart()
{
    m_scratch.SetCapacity(2048);
}

void ConstantsAllocator::OnFrameEnd()
{
    m_scratch.Clear();

    for (Block* block : m_currentFrameBlocks)
    {
        block->offset = 0;

        m_blocks.PushBack(block);
    }

    m_currentFrameBlocks.Clear();
}

void ConstantsAllocator::Write(const void* src, uint32 size)
{
    if (size == 0)
        return;

    AssertDebug(src != nullptr);

    const uint32 scratchOffset = uint32(m_scratch.Size());

    m_scratch.SetSize(scratchOffset + size);

    Memory::Copy(m_scratch.Data() + scratchOffset, reinterpret_cast<const ubyte*>(src), size);
}

void ConstantsAllocator::Commit(GpuBuffer*& outBuffer, uint32& outOffset, uint32& outSize)
{
    if (m_scratch.Empty())
    {
        outBuffer = nullptr;
        outOffset = 0;
        outSize = 0;
        return;
    }

    void* ptr = Allocate(m_scratch.Size(), outBuffer, outOffset);
    AssertDebug(ptr != nullptr);

    Memory::Copy(ptr, m_scratch.Data(), m_scratch.Size());

    outSize = uint32(m_scratch.Size());

    // keep memory around
    m_scratch.SetSize(0);
}

void* ConstantsAllocator::Allocate(uint32 size, GpuBuffer*& outBuffer, uint32& outStartOffset)
{
    const uint32 currentFrameCounter = GetFrameCounter();
    
    outBuffer = nullptr;
    outStartOffset = 0;

    if (m_currentFrameBlocks.Any())
    {
        Block* lastBlock = m_currentFrameBlocks.Back();

        const size_t offset = lastBlock->offset;

        if (offset + size <= ConstantBufferSize)
        {
            void* ptr = (void*)(reinterpret_cast<UIntPtr>(lastBlock->buffer.Map()) + offset);

            outBuffer = &lastBlock->buffer;
            outStartOffset = lastBlock->offset;

            lastBlock->offset = offset + size;

            return ptr;
        }
    }

    // allocate a new block
    Block* newBlock = TryGetRecycledBlock(currentFrameCounter);
    
    if (!newBlock)
        newBlock = NewBlock(currentFrameCounter);

    Assert(size <= ConstantBufferSize);
    
    outBuffer = &newBlock->buffer;
    outStartOffset = newBlock->offset;

    void* ptr = newBlock->buffer.Map();

    newBlock->offset += size;

    return ptr;
}

ConstantsAllocator::Block* ConstantsAllocator::NewBlock(uint32 currentFrameCounter)
{
    Block* newBlock = new Block;
    Assert(newBlock->buffer.Create());

    newBlock->frameCounter = currentFrameCounter;
    newBlock->offset = 0;

    m_currentFrameBlocks.PushBack(newBlock);

    return newBlock;
}

ConstantsAllocator::Block* ConstantsAllocator::TryGetRecycledBlock(uint32 currentFrameCounter)
{
    constexpr uint32 MinDiff = NumFramesInFlight;

    for (auto it = m_blocks.Begin(); it != m_blocks.End();)
    {
        Block* block = *it;

        if (currentFrameCounter - block->frameCounter >= MinDiff)
        {
            block->offset = 0;
            block->frameCounter = currentFrameCounter;

            m_currentFrameBlocks.PushBack(block);

            it = m_blocks.Erase(it);

            continue;
        }

        ++it;
    }

    return nullptr;
}

} // namespace Hyperion
