
/*! Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/Types.hpp>
#include <Core/Constants.hpp>

#include <Core/containers/Array.hpp>

#include <Core/threading/SharedMutex.hpp>

#include <Core/math/MathUtil.hpp>

#include <rendering/GpuBuffer.hpp>

#include <engine/EngineMemory.hpp>

namespace Hyperion {

extern uint32 GetFrameCounter();

template <GpuBufferType BufferType>
class TBufferAllocator
{
protected:
    struct Entry
    {
        uint32 size = 0;
        uint32 lastUsedFrame = uint32(-1);

        GpuBufferRef buffer;

        HYP_FORCE_INLINE bool operator==(const Entry& other) const
        {
            return buffer == other.buffer;
        }

        HYP_FORCE_INLINE bool operator<(const Entry& other) const
        {
            return size < other.size;
        }
    };

    Array<Entry, RenderAllocator> cachedBuffers;
    Array<Entry, RenderAllocator> usedBuffers[NumFramesInFlight];
    
    SharedMutex mutex;

    TBufferAllocator() = default;

public:
    ~TBufferAllocator() = default;

    void OnFrameStart()
    {
        auto& used = usedBuffers[GetFrameCounter() % NumFramesInFlight];

        // recycle it
        for (Entry& usedBuffer : used)
        {
            auto lowerBoundIt = cachedBuffers.LowerBound(usedBuffer);
            cachedBuffers.Insert(lowerBoundIt, std::move(usedBuffer));
        }

        used.Clear();
    }

    void OnFrameEnd()
    {
        const uint32 currFrame = GetFrameCounter();

        for (auto it = cachedBuffers.Begin(); it != cachedBuffers.End();)
        {
            const int64 frameDiff = int64(currFrame) - int64(it->lastUsedFrame);

            if (frameDiff > NumFramesInFlight)
            {
                GpuBufferRef& gpuBuffer = it->buffer;
                gpuBuffer.Reset(); // delete immediately

                it = cachedBuffers.Erase(it);

                continue;
            }

            ++it;
        }
    }

    GpuBuffer* GetBuffer(uint32 bufferSize)
    {
        TUniqueLock lock(mutex);

        const uint32 currFrame = GetFrameCounter();

        auto lowerBoundIt = cachedBuffers.LowerBound(Entry { bufferSize });

        // unused one (different frame)
        for (auto it = lowerBoundIt != cachedBuffers.End() ? lowerBoundIt : cachedBuffers.Begin();
            it != cachedBuffers.End();)
        {
            auto& cachedBuffer = *it;

            // find first that fits to reuse and is not too big (don't want to waste space creating more larger buffers for those that need it after us)
            if (cachedBuffer.size >= bufferSize && cachedBuffer.size < bufferSize * 2)
            {
                cachedBuffer.size = bufferSize;
                cachedBuffer.lastUsedFrame = currFrame;

                GpuBuffer* buffer = cachedBuffer.buffer.Get();

                Assert(buffer != nullptr
                    && buffer->IsCreated()
                    && buffer->Size() >= bufferSize);
            
                auto& used = usedBuffers[currFrame % NumFramesInFlight];
                auto cached = std::move(cachedBuffer);

                cachedBuffers.Erase(it);

                return used.PushBack(std::move(cached)).buffer.Get();
            }

            ++it;
        }

        // bump it up a bit
        bufferSize = MathUtil::NextMultiple(bufferSize, 256);

        // create new one if none found
        Entry newBuffer;
        newBuffer.size = bufferSize;
        newBuffer.lastUsedFrame = currFrame;

        newBuffer.buffer = g_renderInterface->MakeGpuBuffer(BufferType, bufferSize);
        newBuffer.buffer->SetIsCpuAccessible(true);

#if HYP_DEBUG_MODE
        newBuffer.buffer->SetDebugName(NAME("BufferAllocator_Buffer"));
#endif

        Assert(newBuffer.buffer->Create());
            
        auto& used = usedBuffers[currFrame % NumFramesInFlight];
        return used.PushBack(std::move(newBuffer)).buffer.Get();
    }
};

} // namespace Hyperion