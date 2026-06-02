/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <RenderingPch.hpp>

#include <Rendering/RawBufferAllocator.hpp>
#include <Rendering/RawBuffer.hpp>
#include <Rendering/RenderInterface.hpp>

#include <Core/Threading/SharedMutex.hpp>

namespace Hyperion {

/// Should we align numElements to the next power of two to increase buffer reuse?
static constexpr bool UseNextPowerOfTwo = true;
static constexpr uint32 MaxFramesBeforeDiscard = NumFramesInFlight;

#pragma region BufferAllocator

struct BufferAllocatorImpl
{
    struct CachedStructuredBuffer
    {
        size_t numElements = 0;
        size_t elementSize = 0;
        uint32 lastUsedFrame = 0;
        RawBuffer buffer;
    };

    TList<CachedStructuredBuffer, RenderAllocator> cachedBuffers;
    TList<CachedStructuredBuffer, RenderAllocator> usedBuffers;

    SharedMutex mutex;

    ~BufferAllocatorImpl() = default;

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
            cachedBuffers.PushBack(std::move(usedBuffer));

            it = usedBuffers.Erase(it);
        }
    }

    template <class TBuffer, class TInitializeFunction>
    TBuffer& AllocateBuffer(size_t numElements, size_t elementSize, TInitializeFunction&& initialize)
    {
        AssertDebug(numElements != 0);

        const size_t numElementsAligned = UseNextPowerOfTwo ? MathUtil::NextPowerOf2(numElements) : numElements;

        TUniqueLock lock(mutex);

        for (auto it = cachedBuffers.Begin(); it != cachedBuffers.End(); ++it)
        {
            if (it->buffer.gpuBuffer->GetBufferType() == TBuffer::BufferType
                && it->numElements == numElementsAligned
                && it->elementSize == elementSize)
            {
                CachedStructuredBuffer& entry = usedBuffers.PushBack(std::move(*it));
                entry.lastUsedFrame = GetFrameCounter();

                cachedBuffers.Erase(it);

                return reinterpret_cast<TBuffer&>(entry.buffer);
            }
        }

        // Create new one
        CachedStructuredBuffer& newEntry = usedBuffers.EmplaceBack();

        newEntry.lastUsedFrame = GetFrameCounter();

        newEntry.numElements = numElementsAligned;
        newEntry.elementSize = elementSize;

        initialize(newEntry.buffer, numElementsAligned, elementSize);

        AssertDebug(newEntry.buffer.gpuBuffer != nullptr);
        AssertDebug(newEntry.buffer.gpuBuffer->GetBufferType() == TBuffer::BufferType);

        return reinterpret_cast<TBuffer&>(newEntry.buffer);
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

BufferAllocator::BufferAllocator()
    : m_impl(MakePimplWithAllocator<BufferAllocatorImpl, RenderAllocator>())
{
}

BufferAllocator::~BufferAllocator()
{
    Shutdown();
}

void BufferAllocator::OnFrameStart()
{
    m_impl->OnFrameStart();
}

void BufferAllocator::OnFrameEnd()
{
    m_impl->OnFrameEnd();
}

StructuredBuffer& BufferAllocator::AcquireStructuredBuffer(size_t numElements, size_t elementSize)
{
    return m_impl->AllocateBuffer<StructuredBuffer>(numElements, elementSize, [](RawBuffer& buffer, size_t numElements, size_t elementSize)
    {
        buffer = StructuredBuffer(numElements, elementSize);
        buffer.Initialize();
    });
}

RWStructuredBuffer& BufferAllocator::AcquireRWStructuredBuffer(size_t numElements, size_t elementSize)
{
    return m_impl->AllocateBuffer<RWStructuredBuffer>(numElements, elementSize, [](RawBuffer& buffer, size_t numElements, size_t elementSize)
    {
        buffer = RWStructuredBuffer(numElements, elementSize);
        buffer.Initialize();
    });
}

ByteAddressBuffer& BufferAllocator::AcquireByteAddressBuffer(size_t totalSizeBytes)
{
    return m_impl->AllocateBuffer<ByteAddressBuffer>(totalSizeBytes, 0, [](RawBuffer& buffer, size_t totalSizeBytes, size_t)
    {
        buffer = ByteAddressBuffer(totalSizeBytes);
        buffer.Initialize();
    });
}

void BufferAllocator::Shutdown()
{
    m_impl->Shutdown();
}

#pragma endregion BufferAllocator

// Sizes of all these must be the same as we cast the RawBuffer to the specific type directly.
static_assert(sizeof(RawBuffer) == sizeof(StructuredBuffer));
static_assert(sizeof(RawBuffer) == sizeof(RWStructuredBuffer));
static_assert(sizeof(RawBuffer) == sizeof(ByteAddressBuffer));

} // namespace Hyperion
