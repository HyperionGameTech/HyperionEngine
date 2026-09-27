/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

#include <Core/Containers/Array.hpp>

namespace Hyperion {

template <class AllocatorType = DynamicAllocator>
class RangeAllocator
{
    struct FreeRange
    {
        uint32 start;
        uint32 count;
    };

public:
    static constexpr uint32 InvalidStart = ~0u;

    RangeAllocator(uint32 start, uint32 count)
        : m_numAllocated(0)
    {
        if (count != 0)
        {
            m_freeRanges.PushBack(FreeRange { start, count });
        }
    }

    RangeAllocator(const RangeAllocator& other) = delete;
    RangeAllocator& operator=(const RangeAllocator& other) = delete;

    RangeAllocator(RangeAllocator&& other) noexcept = default;
    RangeAllocator& operator=(RangeAllocator&& other) noexcept = default;

    ~RangeAllocator() = default;

    HYP_FORCE_INLINE uint32 NumAllocated() const
    {
        return m_numAllocated;
    }

    /// Returns InvalidStart when no free range is large enough
    HYP_NODISCARD uint32 Allocate(uint32 count)
    {
        if (count == 0)
        {
            return InvalidStart;
        }

        for (uint32 rangeIndex = 0; rangeIndex < uint32(m_freeRanges.Size()); rangeIndex++)
        {
            FreeRange& range = m_freeRanges[rangeIndex];

            if (range.count < count)
            {
                continue;
            }

            const uint32 start = range.start;

            range.start += count;
            range.count -= count;

            if (range.count == 0)
            {
                m_freeRanges.EraseAt(rangeIndex);
            }

            m_numAllocated += count;

            return start;
        }

        return InvalidStart;
    }

    void Free(uint32 start, uint32 count)
    {
        if (start == InvalidStart || count == 0)
        {
            return;
        }

        m_numAllocated -= count;

        uint32 insertIndex = 0;

        while (insertIndex < uint32(m_freeRanges.Size()) && m_freeRanges[insertIndex].start < start)
        {
            insertIndex++;
        }

        m_freeRanges.Insert(m_freeRanges.Begin() + insertIndex, FreeRange { start, count });

        // merge with the next range, then with the previous one
        if (insertIndex + 1 < uint32(m_freeRanges.Size())
            && m_freeRanges[insertIndex].start + m_freeRanges[insertIndex].count == m_freeRanges[insertIndex + 1].start)
        {
            m_freeRanges[insertIndex].count += m_freeRanges[insertIndex + 1].count;
            m_freeRanges.EraseAt(insertIndex + 1);
        }

        if (insertIndex > 0
            && m_freeRanges[insertIndex - 1].start + m_freeRanges[insertIndex - 1].count == m_freeRanges[insertIndex].start)
        {
            m_freeRanges[insertIndex - 1].count += m_freeRanges[insertIndex].count;
            m_freeRanges.EraseAt(insertIndex);
        }
    }

private:
    // sorted by start
    Array<FreeRange, AllocatorType> m_freeRanges;
    uint32 m_numAllocated;
};

} // namespace Hyperion
