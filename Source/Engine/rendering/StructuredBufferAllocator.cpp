/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <RenderingPch.hpp>

#include <rendering/StructuredBufferAllocator.hpp>
#include <rendering/StructuredBuffer.hpp>
#include <rendering/RenderInterface.hpp>

#include <Core/threading/SharedMutex.hpp>

namespace Hyperion {

/// Should we align numElements to the next power of two to increase buffer reuse?
static constexpr bool UseNextPowerOfTwo = true;
static constexpr uint32 MaxFramesBeforeDiscard = NumFramesInFlight;

#pragma region StructuredBufferAllocator

struct StructuredBufferAllocatorImpl
{
    struct CachedStructuredBuffer
    {
        size_t numElements = 0;
        size_t elementSize = 0;
        uint32 lastUsedFrame = 0;
        StructuredBuffer buffer;
    };

    LinkedList<CachedStructuredBuffer, RenderAllocator> cachedBuffers;
    LinkedList<CachedStructuredBuffer, RenderAllocator> usedBuffers;

    SharedMutex mutex;

    ~StructuredBufferAllocatorImpl() = default;

    void OnFrameStart()
    {
    }

    void OnFrameEnd()
    {
        const uint32 frameCounter = GetFrameCounter();

        for (auto it = cachedBuffers.Begin(); it != cachedBuffers.End();)
        {
            CachedStructuredBuffer& cachedBuffer = *it;

            if (int64(frameCounter) - int64(cachedBuffer.lastUsedFrame) >= MaxFramesBeforeDiscard)
            {
                it = cachedBuffers.Erase(it);

                continue;
            }

            ++it;
        }

        // Recycle buffers that were used in the frame
        for (auto it = usedBuffers.Begin(); it != usedBuffers.End();)
        {
            CachedStructuredBuffer& usedBuffer = *it;
            usedBuffer.buffer.Flush();

            cachedBuffers.PushBack(std::move(usedBuffer));

            it = usedBuffers.Erase(it);
        }
    }

    template <class AllocatorType>
    void UpdateAllUsedInFrame(TCommandRecorder<AllocatorType>& cr)
    {
        // for (CachedStructuredBuffer& usedBuffer : usedBuffers)
        // {
        //     AssertDebug(usedBuffer.buffer.gpuBuffer != nullptr && usedBuffer.buffer.gpuBuffer->IsCreated());

        //     usedBuffer.buffer.Update(cr);
        // }
    }

    StructuredBuffer& AcquireBuffer(size_t numElements, size_t elementSize)
    {
        AssertDebug(elementSize != 0 && numElements != 0);

        const size_t numElementsAligned = UseNextPowerOfTwo ? MathUtil::NextPowerOf2(numElements) : numElements;

        // only part that can be used across renderer worker threads simultaneously
        TUniqueLock lock(mutex);

        for (auto it = cachedBuffers.Begin(); it != cachedBuffers.End(); ++it)
        {
            if (it->numElements == numElementsAligned && it->elementSize == elementSize)
            {
                CachedStructuredBuffer& entry = usedBuffers.PushBack(std::move(*it));
                entry.lastUsedFrame = GetFrameCounter();

                cachedBuffers.Erase(it);

                return entry.buffer;
            }
        }

        // Create new one
        CachedStructuredBuffer& newEntry = usedBuffers.EmplaceBack();

        newEntry.lastUsedFrame = GetFrameCounter();

        newEntry.numElements = numElementsAligned;
        newEntry.elementSize = elementSize;

        newEntry.buffer = StructuredBuffer(numElementsAligned, elementSize);
        newEntry.buffer.Initialize();

        return newEntry.buffer;
    }

    void Shutdown()
    {
        for (auto& cached : cachedBuffers)
            cached.buffer.Shutdown();

        cachedBuffers.Clear();

        for (auto& used : usedBuffers)
            used.buffer.Shutdown();

        usedBuffers.Clear();
    }
};

StructuredBufferAllocator::StructuredBufferAllocator()
    : m_impl(MakePimpl<StructuredBufferAllocatorImpl>())
{
}

StructuredBufferAllocator::~StructuredBufferAllocator()
{
    Shutdown();
}

void StructuredBufferAllocator::OnFrameStart()
{
    m_impl->OnFrameStart();
}

void StructuredBufferAllocator::OnFrameEnd()
{
    m_impl->OnFrameEnd();
}

template <>
void StructuredBufferAllocator::UpdateAllUsedInFrame<RenderAllocator>(TCommandRecorder<RenderAllocator>& cr)
{
    return m_impl->UpdateAllUsedInFrame(cr);
}

StructuredBuffer& StructuredBufferAllocator::AcquireBuffer(size_t numElements, size_t elementSize)
{
    return m_impl->AcquireBuffer(numElements, elementSize);
}

void StructuredBufferAllocator::Shutdown()
{
    m_impl->Shutdown();
}

#pragma endregion StructuredBufferAllocator

} // namespace Hyperion
