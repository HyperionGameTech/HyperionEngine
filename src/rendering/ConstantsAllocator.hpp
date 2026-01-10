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
        SizeType size;
        SizeType offset;
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

    void Write(const void* src, SizeType size);

    template <class T>
    void Write(const T* src)
    {
        static_assert(sizeof(T) % 16 == 0);
        static_assert(alignof(T) <= 16);

        Write(src, sizeof(T));
    }

private:
    void* Allocate(SizeType size);

    Block* NewBlock();

    void RecycleBlocks(uint32 currentFrameCounter);

    LinkedList<Block*> m_blocks;
    Array<Block*> m_currentFrameBlocks;
    SizeType m_transactionOffset;
    bool m_inTransaction;
};

} // namespace Hyperion