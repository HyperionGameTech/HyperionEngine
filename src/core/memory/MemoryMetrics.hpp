/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>
#include <core/Types.hpp>
#include <core/math/MathUtil.hpp>

namespace hyperion {
namespace memory {

/*! \brief Standardized memory metrics structure that can be used across different memory allocators. */
struct MemoryMetrics
{
    enum
    {
        MM_BYTES_COMMITTED = 0,
        MM_BYTES_USED,
        MM_BYTES_FREE,
        MM_ALLOCATIONS_ACTIVE,
        MM_BLOCKS_TOTAL,
        MM_BYTES_PEAK,

        MM_MAX_ // windows defines MM_MAX lol
    };

    SizeType values[8]; // reserve some space for future use
    static_assert(HYP_ARRAY_SIZE(values) >= MM_MAX_, "Not enough space in MemoryMetrics values array.");

    MemoryMetrics()
        : values()
    {
    }

    HYP_FORCE_INLINE bool HasMemory() const
    {
        return values[MM_BYTES_COMMITTED] > 0;
    }

    /*! \brief Returns the utilization ratio (0.0 to 1.0) representing how much of the committed memory is in use.
     *  Returns 0.0 if no memory has been committed. */
    HYP_FORCE_INLINE float GetUtilization() const
    {
        if (values[MM_BYTES_COMMITTED] == 0)
        {
            return 0.0f;
        }

        return MathUtil::Clamp(float(values[MM_BYTES_USED]) / MathUtil::Max(float(values[MM_BYTES_COMMITTED]), FLT_EPSILON), 0.0f, 1.0f);
    }

    /*! \brief Returns the fragmentation ratio (0.0 to 1.0) representing the proportion of free memory that is fragmented.
     *  This is an estimate based on the number of blocks vs free memory. */
    HYP_FORCE_INLINE float GetFragmentation() const
    {
        if (values[MM_BYTES_FREE] == 0 || values[MM_BLOCKS_TOTAL] <= 1)
        {
            return 0.0f;
        }

        // Simple heuristic: more blocks with the same free memory suggests more fragmentation
        return MathUtil::Min(1.0f, float(values[MM_BLOCKS_TOTAL]) / MathUtil::Max(float(values[MM_BYTES_FREE] / 1024), FLT_EPSILON));
    }

    MemoryMetrics& operator+=(const MemoryMetrics& other)
    {
        values[MM_BYTES_COMMITTED] += other.values[MM_BYTES_COMMITTED];
        values[MM_BYTES_USED] += other.values[MM_BYTES_USED];
        values[MM_BYTES_FREE] += other.values[MM_BYTES_FREE];
        values[MM_ALLOCATIONS_ACTIVE] += other.values[MM_ALLOCATIONS_ACTIVE];
        values[MM_BLOCKS_TOTAL] += other.values[MM_BLOCKS_TOTAL];
        values[MM_BYTES_PEAK] = MathUtil::Max(values[MM_BYTES_PEAK], other.values[MM_BYTES_PEAK]);

        return *this;
    }

    HYP_FORCE_INLINE MemoryMetrics operator+(const MemoryMetrics& other) const
    {
        MemoryMetrics result = *this;
        result += other;
        return result;
    }

    HYP_FORCE_INLINE SizeType& operator[](int i)
    {
        return values[i];
    }

    HYP_FORCE_INLINE SizeType operator[](int i) const
    {
        return values[i];
    }
};

} // namespace memory

using memory::MemoryMetrics;

} // namespace hyperion
