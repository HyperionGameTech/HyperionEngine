/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/Defines.hpp>
#include <Core/Constants.hpp>

#include <Core/memory/allocator/Allocator.hpp>
#include <Core/memory/allocator/ArenaAllocator.hpp>

#include <rendering/RenderObject.hpp>
#include <rendering/RenderMemory.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

struct ConstantAllocatorBlock;

class ConstantsAllocator
{
    using Block = ConstantAllocatorBlock;

public:
    ConstantsAllocator();

    ConstantsAllocator(const ConstantsAllocator& other) = delete;
    ConstantsAllocator& operator=(const ConstantsAllocator& other) = delete;

    ConstantsAllocator(ConstantsAllocator&& other) noexcept = delete;
    ConstantsAllocator& operator=(ConstantsAllocator&& other) noexcept = delete;

    ~ConstantsAllocator();

    void OnFrameStart();
    void OnFrameEnd();

    void Write(const void* src, uint32 size);

    template <class T>
    void Write(const T* src)
    {
        static_assert(sizeof(T) % 16 == 0);
        static_assert(alignof(T) <= 16);

        Write(src, sizeof(T));
    }

    void Commit(GpuBuffer*& outBuffer, uint32& outOffset, uint32& outSize);

private:
    void* Allocate(uint32 size, GpuBuffer*& outBuffer, uint32& outStartOffset);

    Block* NewBlock(uint32 currentFrameCounter);
    Block* TryGetRecycledBlock(uint32 currentFrameCounter);

    Array<Block*, RHIAllocator> m_blocks;
    Array<Block*, RHIAllocator> m_currentFrameBlocks;
    TByteBuffer<RHITempAllocator> m_scratch;
};

} // namespace Hyperion