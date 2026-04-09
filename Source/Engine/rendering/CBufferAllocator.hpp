/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>
#include <Core/Constants.hpp>
#include <Core/Types.hpp>

#include <Core/containers/LinkedList.hpp>

#include <Core/memory/allocator/Allocator.hpp>
#include <Core/memory/allocator/ArenaAllocator.hpp>

#include <Core/threading/SharedMutex.hpp>

#include <rendering/RenderObject.hpp>
#include <rendering/RenderMemory.hpp>

namespace Hyperion {

struct CBufferAllocatorBlock;

class CBufferAllocator
{
    using Block = CBufferAllocatorBlock;

public:
    CBufferAllocator();

    CBufferAllocator(const CBufferAllocator& other) = delete;
    CBufferAllocator& operator=(const CBufferAllocator& other) = delete;

    CBufferAllocator(CBufferAllocator&& other) noexcept = delete;
    CBufferAllocator& operator=(CBufferAllocator&& other) noexcept = delete;

    ~CBufferAllocator();

    void Initialize(size_t minAllocationAlignment);

    void OnFrameStart();
    void OnFrameEnd();

    void* Allocate(size_t count, size_t alignment, GpuBuffer*& outBuffer, size_t& outStartOffset);

    void Write(const void* src, size_t count, size_t alignment);

    template <class T>
    void Write(const T* src)
    {
        static_assert(is_pod_type_v<T> && !std::is_pointer_v<T>, "T must be plain old data to write to constant buffers");

        Write(src, sizeof(T), alignof(T));
    }

    void Commit(GpuBuffer*& outBuffer, size_t& outOffset, size_t& outSize);

private:
    Block* NewBlock(uint32 currentFrameCounter);
    Block* TryGetRecycledBlock(uint32 currentFrameCounter);

    LinkedList<Block, RenderAllocator> m_blocks;
    size_t m_minAllocationAlignment;

    LinkedList<Block, RenderAllocator> m_currentFrameBlocks[NumRendererWorkerThreads + 1];

    TByteBuffer<RenderAllocator> m_scratch[NumRendererWorkerThreads + 1];
    size_t m_scratchAlignment[NumRendererWorkerThreads + 1];

    SharedMutex m_mutex;
};

} // namespace Hyperion