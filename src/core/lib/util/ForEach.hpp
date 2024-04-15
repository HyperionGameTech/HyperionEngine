/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_V2_LIB_UTIL_FOREACH_HPP
#define HYPERION_V2_LIB_UTIL_FOREACH_HPP

#include <core/lib/DynArray.hpp>
#include <math/MathUtil.hpp>
#include <Types.hpp>
#include <TaskSystem.hpp>

#include <algorithm>
#include <utility>
#include <type_traits>

namespace hyperion {


/*! \brief Execute a lambda for each item in the container, in \ref{num_batches} batches.
 *  Container must be a contiguous container.
 *  Lambda is called with the item, the index of the item, and the batch index.
 *
 *  \param container The container to iterate over.
 *  \param num_batches The number of batches to split the container into.
 *  \param lambda The lambda to call for each item.
 */
template <class Container, class Lambda>
HYP_FORCE_INLINE
static inline void ForEachInBatches(Container &container, uint num_batches, Lambda &&lambda)
{
    static_assert(Container::is_contiguous, "Container must be contiguous to use ForEachInBatches");

    const uint num_items = container.Size();
    const uint items_per_batch = (num_items + num_batches - 1) / num_batches;

    auto *data_ptr = container.Data();

    for (uint batch_index = 0; batch_index < num_batches; batch_index++) {
        const uint offset_index = batch_index * items_per_batch;
        const uint max_index = MathUtil::Min(offset_index + items_per_batch, num_items);

        for (uint i = offset_index; i < max_index; ++i) {
            lambda(*(data_ptr + i), i, batch_index);
        }
    }
}

/*! \brief Call a lambda for each permutation of items in the container.
 *  Container must be a contiguous container.
 *  Lambda is called with an Array of indices into the container.
 *
 *  \param container The container to iterate over.
 *  \param lambda The lambda to call for each permutation.
 */
template <class Container, class Lambda>
static inline void ForEachPermutation(Container &container, Lambda &&lambda)
{
    Array<uint> indices;

    for (uint i = 0; i < uint(container.Size()); i++) {
        const uint num_combinations = (1u << i);

        for (uint k = 0; k < num_combinations; k++) {
            indices.Clear();

            for (uint j = 0; j < i; j++) {
                if ((k & (1u << j)) != (1u << j)) {
                    continue;
                }
                
                indices.PushBack(j);
            }
            
            indices.PushBack(i);

            lambda(indices);
        }
    }
}

/*! \brief Perform a parallel foreach with in the default pool (THREAD_POOL_GENERIC). */
template <class Container, class Lambda>
HYP_FORCE_INLINE
static inline void ParallelForEach(Container &container, Lambda &&lambda)
{
    TaskSystem::GetInstance().ParallelForEach(
        container,
        std::forward<Lambda>(lambda)
    );
}

/*! \brief Perform a parallel foreach within the given task thread pool \ref{pool}.
 *  Number of batches will depend upon the thread pool selected's number of workers. */
template <class Container, class Lambda>
HYP_FORCE_INLINE
static inline void ParallelForEach(Container &container, TaskThreadPoolName pool, Lambda &&lambda)
{
    TaskSystem::GetInstance().ParallelForEach(
        pool,
        container,
        std::forward<Lambda>(lambda)
    );
}

/*! \brief Perform a parallel foreach within the given task thread pool \ref{pool} and using \ref{num_batches} batches. */
template <class Container, class Lambda>
HYP_FORCE_INLINE
static inline void ParallelForEach(Container &container, uint num_batches, TaskThreadPoolName pool, Lambda &&lambda)
{
    TaskSystem::GetInstance().ParallelForEach(
        pool,
        num_batches,
        container,
        std::forward<Lambda>(lambda)
    );
}

} // namespace hyperion

#endif