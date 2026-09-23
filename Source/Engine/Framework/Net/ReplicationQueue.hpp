/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Defines.hpp>

#include <Core/Containers/Array.hpp>

#include <Core/Memory/ByteBuffer.hpp>

#include <Core/Threading/Mutex.hpp>

#include <Core/Utilities/ByteUtil.hpp>

#include <Net/NetMemory.hpp>

#include <type_traits>
#include <utility>

namespace Hyperion {

// Single producer / single consumer. The producer Push()es into a private buffer and PublishBatch() appends it
// to the shared pending buffer, so batches are never reordered or lost no matter how often each side runs.
template <class BaseType>
class ReplicationQueue
{
    // matches ByteBuffer's default storage alignment
    static constexpr size_t ItemAlignment = 16;

public:
    ReplicationQueue() = default;

    void PublishBatch()
    {
        if (m_writeBuffer.startOffsets.Empty())
        {
            return;
        }

        Mutex::Guard guard(m_pendingMutex);

        if (m_pendingBuffer.startOffsets.Empty())
        {
            std::swap(m_writeBuffer, m_pendingBuffer);
        }
        else
        {
            const size_t baseOffset = ByteUtil::AlignAs(m_pendingBuffer.writeOffset, ItemAlignment);
            const size_t requiredSize = baseOffset + m_writeBuffer.writeOffset;

            if (m_pendingBuffer.storage.Size() < requiredSize)
            {
                m_pendingBuffer.storage.SetSize(MathUtil::NextPowerOf2(requiredSize));
            }

            Memory::Copy(m_pendingBuffer.storage.Data() + baseOffset, m_writeBuffer.storage.Data(), m_writeBuffer.writeOffset);

            m_pendingBuffer.startOffsets.Reserve(m_pendingBuffer.startOffsets.Size() + m_writeBuffer.startOffsets.Size());

            for (size_t startOffset : m_writeBuffer.startOffsets)
            {
                m_pendingBuffer.startOffsets.PushBack(baseOffset + startOffset);
            }

            m_pendingBuffer.writeOffset = requiredSize;
        }

        ClearBuffer(m_writeBuffer);
    }

    /// Drains everything published so far. Returned pointers stay valid until the next DrainPending() or Reset().
    template <class AllocatorType>
    void DrainPending(Array<BaseType*, AllocatorType>& outItems)
    {
        ClearBuffer(m_readBuffer);

        {
            Mutex::Guard guard(m_pendingMutex);

            std::swap(m_readBuffer, m_pendingBuffer);
        }

        outItems.Reserve(outItems.Size() + m_readBuffer.startOffsets.Size());

        for (size_t startOffset : m_readBuffer.startOffsets)
        {
            outItems.PushBack(reinterpret_cast<BaseType*>(m_readBuffer.storage.Data() + startOffset));
        }
    }

    /// Discards everything queued. Only valid while neither the producer nor the consumer is running.
    void Reset()
    {
        Mutex::Guard guard(m_pendingMutex);

        ClearBuffer(m_writeBuffer);
        ClearBuffer(m_pendingBuffer);
        ClearBuffer(m_readBuffer);
    }

    /// Pushes an element to the queue
    template <class T>
    void Push(const T& item)
    {
        static_assert(std::is_base_of_v<BaseType, T> && std::is_trivially_destructible_v<T>);
        static_assert(alignof(T) <= ItemAlignment, "PublishBatch() only preserves alignment up to ItemAlignment");

        Buffer& buffer = m_writeBuffer;

        const size_t alignedOffset = ByteUtil::AlignAs(buffer.writeOffset, alignof(T));

        if (buffer.storage.Size() < alignedOffset + sizeof(T))
        {
            buffer.storage.SetSize(MathUtil::NextPowerOf2(alignedOffset + sizeof(T)));
        }

        T* newItem = reinterpret_cast<T*>(buffer.storage.Data() + alignedOffset);
        new (newItem) T(item);

        buffer.writeOffset = alignedOffset + sizeof(T);
        buffer.startOffsets.PushBack(alignedOffset);
    }

private:
    using StorageBuffer = memory::ByteBuffer<net::NetAllocator>;

    struct Buffer
    {
        StorageBuffer storage;
        Array<size_t, net::NetAllocator> startOffsets;
        size_t writeOffset = 0;
    };

    static void ClearBuffer(Buffer& buffer)
    {
        buffer.storage.SetSize(0);
        buffer.startOffsets.Resize(0);
        buffer.writeOffset = 0;
    }

    Buffer m_writeBuffer;   // producer-owned
    Buffer m_readBuffer;    // consumer-owned
    Buffer m_pendingBuffer; // guarded by m_pendingMutex
    Mutex m_pendingMutex;
};

} // namespace Hyperion
