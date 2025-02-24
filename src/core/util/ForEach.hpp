/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_UTIL_FOREACH_HPP
#define HYPERION_UTIL_FOREACH_HPP

#include <core/math/MathUtil.hpp>
#include <Types.hpp>
#include <core/threading/TaskSystem.hpp>

#include <algorithm>
#include <utility>
#include <type_traits>

namespace hyperion {

enum class IterationResult : uint8
{
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
static inline void ForEach(Container &&container, Callback &&callback)
{
    for (auto it = container.Begin(); it != container.End(); ++it) {
        IterationResult result = callback(*it);

        if (result == IterationResult::STOP) {
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
static inline void ForEach(Container &&container, Mutex &mutex, Callback &&callback)
{
    typename Mutex::Guard guard(mutex);

    for (auto it = container.Begin(); it != container.End(); ++it) {
        IterationResult result = callback(*it);

        if (result == IterationResult::STOP) {
            break;
        }
    }
}

/*! \brief Execute a lambda for each item in the container, in \ref{num_batches} batches.
 *  Container must be a contiguous container.
 *  Callback is called with the item, the index of the item, and the batch index and should return a IterationResult.
 *
 *  \param container The container to iterate over.
 *  \param num_batches The number of batches to split the container into.
 *  \param callback The function to call for each item.
 */
template <class Container, class Callback>
static inline void ForEachInBatches(Container &&container, uint32 num_batches, Callback &&callback)
{
    static_assert(NormalizedType<Container>::is_contiguous, "Container must be contiguous to use ForEachInBatches");

    const uint32 num_items = container.Size();
    const uint32 items_per_batch = (num_items + num_batches - 1) / num_batches;

    auto *data_ptr = container.Data();

    for (uint32 batch_index = 0; batch_index < num_batches; batch_index++) {
        const uint32 offset_index = batch_index * items_per_batch;
        const uint32 max_index = MathUtil::Min(offset_index + items_per_batch, num_items);

        for (uint32 i = offset_index; i < max_index; ++i) {
            IterationResult result = callback(*(data_ptr + i), i, batch_index);

            if (result == IterationResult::STOP) {
                break;
            }
        }
    }
}

} // namespace hyperion

#endif