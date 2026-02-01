/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/ConstantsAllocator.hpp>
#include <rendering/RenderInterface.hpp>

#include <rendering/util/SafeDeleter.hpp>

namespace Hyperion {

static constexpr SizeType ConstantBufferSize = 65536;

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

        const SizeType offset = lastBlock->offset;

        if (offset + size <= lastBlock->size)
        {
            void* ptr = (void*)(reinterpret_cast<uintptr_t>(lastBlock->buffer->Map()) + offset);

            outBuffer = lastBlock->buffer;
            outStartOffset = lastBlock->offset;

            lastBlock->offset = offset + size;

            return ptr;
        }
    }

    // allocate a new block
    Block* newBlock = TryGetRecycledBlock(currentFrameCounter);
    
    if (!newBlock)
        newBlock = NewBlock(currentFrameCounter);

    Assert(size <= newBlock->size);
    
    outBuffer = newBlock->buffer;
    outStartOffset = newBlock->offset;

    void* ptr = newBlock->buffer->Map();

    newBlock->offset += size;

    return ptr;
}

ConstantsAllocator::Block* ConstantsAllocator::NewBlock(uint32 currentFrameCounter)
{
    Block* newBlock = new Block;

    GpuBufferRef buffer = g_renderInterface->MakeGpuBuffer(GpuBufferType::CONSTANT_BUFFER, ConstantBufferSize, 256);
    Assert(buffer != nullptr);

    Assert(buffer->Create());

    newBlock->buffer = std::move(buffer);
    newBlock->frameCounter = currentFrameCounter;
    newBlock->size = ConstantBufferSize;
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
