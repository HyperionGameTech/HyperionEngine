/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Defines.hpp>
#include <Core/Constants.hpp>
#include <Core/Types.hpp>

#include <Core/Containers/List.hpp>

#include <Core/Memory/Allocator/Allocator.hpp>
#include <Core/Memory/Allocator/ArenaAllocator.hpp>

#include <Core/Threading/SharedMutex.hpp>

#include <Rendering/RenderTypes.hpp>
#include <Rendering/RenderMemory.hpp>

namespace Hyperion {

struct CBufferAllocatorBlock;

extern uint32 CurrentRenderThreadIndex();

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

    HYP_NODISCARD void* Allocate(size_t count, size_t alignment);
    HYP_NODISCARD void* Allocate(size_t count, size_t alignment, GpuBuffer*& outBuffer, size_t& outStartOffset);

    template <class T>
    HYP_NODISCARD T* Allocate(size_t count = 1)
    {
        static_assert(is_pod_type_v<T> && !std::is_pointer_v<T>, "T must be plain old data to allocate in constant buffers");

        return static_cast<T*>(Allocate(sizeof(T) * count, alignof(T)));
    }

    void Write(const void* src, size_t count, size_t alignment)
    {
        if (count == 0 || !src)
        {
            return;
        }

        const uint32 idx = CurrentRenderThreadIndex();

        auto& scratch = m_scratch[idx];

        const size_t alignedCount = alignment > 0 ? ByteUtil::AlignAs(count, alignment) : count;
        const size_t scratchOffset = ByteUtil::AlignAs(scratch.Size(), alignment);

        scratch.SetSize(scratchOffset + alignedCount);

        m_scratchAlignment[idx] = MathUtil::Max(m_scratchAlignment[idx], alignment);

        Memory::Copy(scratch.Data() + scratchOffset, reinterpret_cast<const ubyte*>(src), count);
    }

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

    TList<Block, RenderAllocator> m_blocks;
    size_t m_minAllocationAlignment;

    TList<Block, RenderAllocator> m_currentFrameBlocks[NumRendererWorkerThreads + 1];

    TByteBuffer<RenderAllocator> m_scratch[NumRendererWorkerThreads + 1];
    size_t m_scratchAlignment[NumRendererWorkerThreads + 1];

    SharedMutex m_mutex;
};

} // namespace Hyperion
