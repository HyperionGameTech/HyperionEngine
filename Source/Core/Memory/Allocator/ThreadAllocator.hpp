/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Memory/Allocator/ArenaAllocator.hpp>

namespace Hyperion {

namespace memory {

class ThreadAllocator : public TArena<DynamicAllocator>
{
public:
    static constexpr size_t BlockSize = 1 * 1024 * 1024; // 1 MB

    ThreadAllocator()
        : TArena(BlockSize)
    {
    }
};

// ========== SPECIALIZATION ==========

template <class T, class T2>
struct DefaultAllocatorInstanceHelper;

template <>
struct DefaultAllocatorInstanceHelper<ThreadAllocator, void>
{
    CORE_API ThreadAllocator& operator()() const;
};

// ====================================

} // namespace memory

using memory::ThreadAllocator;

} // namespace Hyperion
