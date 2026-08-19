/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Defines.hpp>

#include <Core/Containers/Array.hpp>

#include <Core/Memory/ByteBuffer.hpp>

#include <Core/Threading/AtomicVar.hpp>

#include <Core/Utilities/ByteUtil.hpp>

#include <Net/NetMemory.hpp>

#include <type_traits>

namespace Hyperion {

template <class BaseType>
class ReplicationQueue
{
public:
    ReplicationQueue()
        : m_writeIndex(0),
          m_readIndex(1),
          m_readyIndex(2)
    {
    }

    void PublishBatch()
    {
        m_writeIndex = m_readyIndex.Exchange(m_writeIndex, MemoryOrder::ACQUIRE_RELEASE);
    }

    /// Drains all elements and flips buffer
    template <class AllocatorType>
    void DrainPending(Array<BaseType*, AllocatorType>& outItems)
    {
        Buffer& previous = m_buffers[m_readIndex];
        previous.storage.SetSize(0);
        previous.startOffsets.Resize(0);
        previous.writeOffset = 0;

        m_readIndex = m_readyIndex.Exchange(m_readIndex, MemoryOrder::ACQUIRE_RELEASE);

        Buffer& buffer = m_buffers[m_readIndex];

        outItems.Reserve(outItems.Size() + buffer.startOffsets.Size());

        for (size_t startOffset : buffer.startOffsets)
        {
            outItems.PushBack(reinterpret_cast<BaseType*>(buffer.storage.Data() + startOffset));
        }
    }

    /// Pushes an element to the queue
    template <class T>
    void Push(const T& item)
    {
        static_assert(std::is_base_of_v<BaseType, T> && std::is_trivially_destructible_v<T>);

        Buffer& buffer = m_buffers[m_writeIndex];

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

    Buffer m_buffers[3];

    uint32 m_writeIndex; // producer-owned
    uint32 m_readIndex;  // consumer-owned
    AtomicVar<uint32> m_readyIndex; // shared
};

} // namespace Hyperion
