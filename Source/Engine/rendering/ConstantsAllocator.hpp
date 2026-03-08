/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/Defines.hpp>
#include <Core/Constants.hpp>
#include <Core/Types.hpp>

#include <Core/containers/LinkedList.hpp>

#include <Core/memory/allocator/Allocator.hpp>
#include <Core/memory/allocator/ArenaAllocator.hpp>

#include <rendering/RenderObject.hpp>
#include <rendering/RenderMemory.hpp>

namespace Hyperion {

struct ConstantsAllocatorBlock;

class ConstantsAllocator
{
    using Block = ConstantsAllocatorBlock;

public:
    ConstantsAllocator();

    ConstantsAllocator(const ConstantsAllocator& other) = delete;
    ConstantsAllocator& operator=(const ConstantsAllocator& other) = delete;

    ConstantsAllocator(ConstantsAllocator&& other) noexcept = delete;
    ConstantsAllocator& operator=(ConstantsAllocator&& other) noexcept = delete;

    ~ConstantsAllocator();

    void Initialize(size_t minAllocationAlignment);

    void OnFrameStart();
    void OnFrameEnd();

    void* Allocate(size_t count, size_t alignment, GpuBuffer*& outBuffer, size_t& outStartOffset);

    void Write(const void* src, size_t count, size_t alignment);

    template <class T>
    void Write(const T* src)
    {
        static_assert(is_pod_type_v<T>, "T must be plain old data to write to constant buffers");

        Write(src, sizeof(T), alignof(T));
    }

    void Commit(GpuBuffer*& outBuffer, size_t& outOffset, size_t& outSize);

private:
    Block* NewBlock(uint32 currentFrameCounter);
    Block* TryGetRecycledBlock(uint32 currentFrameCounter);

    LinkedList<Block, RHIAllocator> m_blocks;
    LinkedList<Block, RHIAllocator> m_currentFrameBlocks;
    TByteBuffer<RHITempAllocator> m_scratch;
    size_t m_scratchAlignment;
    size_t m_minAllocationAlignment;
};

} // namespace Hyperion