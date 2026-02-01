/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>
#include <core/Constants.hpp>

#include <rendering/RenderObject.hpp>
#include <rendering/RenderMemory.hpp>

#include <core/Types.hpp>

namespace Hyperion {

class ConstantsAllocator
{
    struct Block
    {
        HYP_DEF_POOL_NEW_DELETE(g_renderPool);

        GpuBufferRef buffer;
        uint32 frameCounter; // last used frame
        uint32 size;
        uint32 offset;
    };

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

    Array<Block*, RenderAllocator> m_blocks;
    Array<Block*, RenderAllocator> m_currentFrameBlocks;
    TByteBuffer<RenderTempAllocator> m_scratch;
};

} // namespace Hyperion