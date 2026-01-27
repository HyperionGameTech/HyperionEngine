/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/ConstantsAllocator.hpp>
#include <rendering/RenderInterface.hpp>

#include <rendering/util/SafeDeleter.hpp>

namespace Hyperion {

static constexpr SizeType BlockSize = 65535;

ConstantsAllocator::ConstantsAllocator()
    : m_transactionOffset(0)
{
}

ConstantsAllocator::~ConstantsAllocator()
{
    for (Block* block : m_blocks)
    {
        SafeDelete(std::move(block->buffer));
        delete block;
    }

    m_blocks.Clear();

    for (Block* block : m_currentFrameBlocks)
    {
        SafeDelete(std::move(block->buffer));
        delete block;
    }

    m_currentFrameBlocks.Clear();
}

void ConstantsAllocator::OnFrameStart()
{
    m_transactionOffset = 0;

    RecycleBlocks(GetFrameCounter());
}

void ConstantsAllocator::OnFrameEnd()
{
    for (Block* block : m_currentFrameBlocks)
    {
        block->offset = 0;

        m_blocks.PushBack(block);
    }

    m_currentFrameBlocks.Clear();
}

void ConstantsAllocator::Write(const void* src, SizeType size)
{
    if (size == 0)
        return;

    AssertDebug(src != nullptr);

    void* dst = Allocate(size);
    Memory::Copy(dst, src, size);
}

void* ConstantsAllocator::Allocate(SizeType size)
{
    for (Block* block : m_currentFrameBlocks)
    {
        const SizeType offset = block->offset;

        if (offset + size <= block->size)
        {
            void* ptr = (void*)(reinterpret_cast<uintptr_t>(block->buffer->Map()) + offset);

            block->offset = offset + size;

            return ptr;
        }
    }

    // allocate a new block
    Block* newBlock = NewBlock();
    Assert(size <= newBlock->size);

    void* ptr = newBlock->buffer->Map();

    newBlock->offset += size;

    return ptr;
}

ConstantsAllocator::Block* ConstantsAllocator::NewBlock()
{
    Block* newBlock = new Block;

    GpuBufferRef buffer = g_renderInterface->MakeGpuBuffer(GpuBufferType::CONSTANT_BUFFER, BlockSize, 256);
    Assert(buffer != nullptr);

    Assert(buffer->Create());

    newBlock->buffer = std::move(buffer);
    newBlock->frameCounter = GetFrameCounter();
    newBlock->size = BlockSize;
    newBlock->offset = 0;

    m_currentFrameBlocks.PushBack(newBlock);

    m_transactionOffset = 0;

    return newBlock;
}

void ConstantsAllocator::RecycleBlocks(uint32 currentFrameCounter)
{
    constexpr uint32 MinDiff = NumFramesInFlight;

    for (auto it = m_blocks.Begin(); it != m_blocks.End();)
    {
        Block* block = *it;

        if (currentFrameCounter - block->frameCounter >= MinDiff)
        {
            m_currentFrameBlocks.PushBack(block);

            it = m_blocks.Erase(it);

            continue;
        }

        ++it;
    }
}

} // namespace Hyperion
