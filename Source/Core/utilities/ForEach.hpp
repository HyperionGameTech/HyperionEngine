/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/math/MathUtil.hpp>

#include <Core/utilities/Span.hpp>

#include <Core/Types.hpp>

#include <algorithm>
#include <utility>
#include <type_traits>

namespace Hyperion {

enum class IterationResult : uint8
{
    INVALID = uint8(-1),

    CONTINUE = 0,
    STOP
};

/*! \brief Execute a callback for each item in the container.
 *  Callback is called with the item as an argument and should return a IterationResult.
 *
 *  \param container The container to iterate over.
 *  \param callback The function to call for each item.
 */
template <class Container, class Callback>
static inline void ForEach(Container&& container, Callback&& callback)
{
    for (auto it = container.Begin(); it != container.End(); ++it)
    {
        IterationResult result = callback(*it);

        if (result == IterationResult::STOP)
        {
            break;
        }
    }
}

/*! \brief Execute a callback for each item in the container, locking the mutex before iterating.
 *  Callback is called with the item as an argument and should return a IterationResult.
 *
 *  \param container The container to iterate over.
 *  \param mutex The mutex to lock before iterating.
 *  \param callback The function to call for each item.
 */
template <class Container, class Mutex, class Callback>
static inline void ForEach(Container&& container, Mutex& mutex, Callback&& callback)
{
    typename Mutex::Guard guard(mutex);

    for (auto it = container.Begin(); it != container.End(); ++it)
    {
        IterationResult result = callback(*it);

        if (result == IterationResult::STOP)
        {
            break;
        }
    }
}

/*! \brief Execute a lambda for each item in the container, in \p numBatches batches.
 *  Container must be a contiguous container.
 *  Callback is called with a Span of items for each batch, and should return a IterationResult.
 *
 *  \param container The container to iterate over.
 *  \param numBatches The number of batches to split the container into.
 *  \param callback The function to call with a Span of items for the batch
 */
template <class Container, class Callback>
static inline void ForEachInBatches(Container&& container, uint32 numBatches, Callback&& callback)
{
    const uint32 numItems = container.Size();
    const uint32 itemsPerBatch = (numItems + numBatches - 1) / numBatches;

    auto* dataPtr = container.Data();

    for (uint32 batchIndex = 0; batchIndex < numBatches; batchIndex++)
    {
        const uint32 offsetIndex = batchIndex * itemsPerBatch;
        const uint32 maxIndex = MathUtil::Min(offsetIndex + itemsPerBatch, numItems);

        IterationResult result = callback(container.ToSpan().Slice(offsetIndex, maxIndex - offsetIndex));

        if (result == IterationResult::STOP)
        {
            break;
        }
    }
}

} // namespace Hyperion
