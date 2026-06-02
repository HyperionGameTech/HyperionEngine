
/*! Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/Types.hpp>
#include <Core/Constants.hpp>

#include <Core/Containers/Array.hpp>

#include <Core/Threading/SharedMutex.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Framework/EngineMemory.hpp>

namespace Hyperion {

extern uint32 GetFrameCounter();

/*! \brief Utility class for managing a cache of reusable buffers with intelligent size-based matching.
 *  Provides tiered waste tolerance to balance memory reuse vs waste:
 *  - Small buffers (< 1KB): Allow up to 4x size difference for better reuse
 *  - Medium buffers (1KB - 64KB): Allow up to 2x size difference
 *  - Large buffers (> 64KB): Allow up to 1.5x size difference to prevent waste
 */
template <class EntryType, class BufferRefType>
class TBufferCache
{
public:
    using Entry = EntryType;
    using BufferRef = BufferRefType;

    /*! \brief Get the maximum waste ratio allowed based on buffer size.
     *  Smaller buffers can tolerate more waste percentage-wise to improve reuse.
     *  Larger buffers use stricter limits to prevent excessive memory waste.
     */
    HYP_FORCE_INLINE static float GetMaxWasteRatio(size_t bufferSize)
    {
        // For very small buffers (< 1KB), allow up to 4x size (e.g., 256B buffer for 64B request)
        // For medium buffers (1KB - 64KB), allow up to 2x size
        // For large buffers (> 64KB), allow up to 1.5x size to prevent waste
        if (bufferSize < 1024)
        {
            return 4.0f;
        }
        else if (bufferSize < 64 * 1024)
        {
            return 2.0f;
        }
        else
        {
            return 1.5f;
        }
    }

    /*! \brief Find the best matching buffer from cache using tiered waste tolerance.
     *  Prioritizes:
     *  1. Exact size match (best)
     *  2. Small buffers that fit within waste tolerance
     *  3. Slightly larger buffers with minimal waste
     *
     *  \param cachedBuffers The sorted array of cached buffers to search
     *  \param bufferSize The requested buffer size (will be aligned to 256)
     *  \param outBestMatchEntry Output parameter for the best matching entry found
     *  \return Iterator to the best match in cachedBuffers, or cachedBuffers.End() if no match found
     */
    static typename Array<Entry, RenderAllocator>::Iterator FindBestMatch(
        Array<Entry, RenderAllocator>& cachedBuffers,
        size_t bufferSize,
        Entry& outBestMatchEntry)
    {
        // Round up to minimum alignment first
        const size_t requestedSize = bufferSize;
        bufferSize = MathUtil::NextMultiple(bufferSize, 256);

        auto lowerBoundIt = cachedBuffers.LowerBound(Entry { bufferSize });

        Entry* bestMatch = nullptr;

        float bestWasteRatio = MathUtil::MaxSafeValue<float>();
        auto bestMatchIt = cachedBuffers.End();

        // Search for best match among buffers that can satisfy the request
        for (auto it = lowerBoundIt != cachedBuffers.End() ? lowerBoundIt : cachedBuffers.Begin();
            it != cachedBuffers.End();
            ++it)
        {
            auto& cachedBuffer = *it;

            // Buffer must be large enough
            if (cachedBuffer.size < bufferSize)
            {
                continue;
            }

            const float wasteRatio = float(cachedBuffer.size) / float(bufferSize);
            const float maxWasteRatio = GetMaxWasteRatio(requestedSize);

            // Check if this buffer is within acceptable waste limits
            if (wasteRatio <= maxWasteRatio)
            {
                // Prefer smaller waste ratio (better fit)
                if (wasteRatio < bestWasteRatio)
                {
                    bestWasteRatio = wasteRatio;
                    bestMatch = &cachedBuffer;
                    bestMatchIt = it;

                    // Exact match is ideal, take it immediately
                    if (wasteRatio == 1.0f)
                    {
                        break;
                    }
                }
            }
        }

        if (bestMatch != nullptr)
        {
            outBestMatchEntry = *bestMatch;

            return bestMatchIt;
        }

        return cachedBuffers.End();
    }

    /*! \brief Move an entry from the cache to the used list for the current frame.
     *  \param cachedBuffers The cache array to remove from
     *  \param usedBuffers The per-frame used buffers array
     *  \param it Iterator to the entry in cachedBuffers
     *  \param entry The entry to move (with updated size)
     *  \return Pointer to the GpuBuffer that was moved
     */
    static GpuBuffer* MoveToUsed(
        Array<Entry, RenderAllocator>& cachedBuffers,
        TList<Entry, RenderAllocator>& usedBuffers,
        typename Array<Entry, RenderAllocator>::Iterator it,
        Entry& entry)
    {
        Assert(it != cachedBuffers.End());

        auto& newElem = usedBuffers.PushBack(std::move(entry)).buffer;
        cachedBuffers.Erase(it);
        return newElem.Get();
    }

    /*! \brief Recycle used buffers from a previous frame back into the cache. Assumes usedBuffers is sorted by last used frame index.
     *  \param usedBuffers The used buffers array to recycle
     *  \param cachedBuffers The cache array to add to
     *  \param maxFrameIndex Frame index to stop iterating at (no buffers with last use count >= \p{maxFrameIndex} will be recycled this call)
     */
    static void RecycleUsedBuffers(
        TList<Entry, RenderAllocator>& usedBuffers,
        Array<Entry, RenderAllocator>& cachedBuffers,
        uint32 maxFrameIndex)
    {
        while (!usedBuffers.Empty())
        {
            Entry& currEntry = usedBuffers.Front();

            if (currEntry.lastUsedFrame >= maxFrameIndex)
            {
                break;
            }

            auto lowerBoundIt = cachedBuffers.LowerBound(currEntry);
            cachedBuffers.Insert(lowerBoundIt, std::move(currEntry));
            usedBuffers.PopFront();
        }
    }
};

} // namespace Hyperion
