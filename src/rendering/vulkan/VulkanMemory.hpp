/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>
#include <core/Types.hpp>

#include <core/memory/allocator/Allocator.hpp>
#include <core/memory/allocator/ArenaAllocator.hpp>

#include <rendering/RenderMemory.hpp>

namespace hyperion {

/*! \brief Temporary Vulkan memory arena for fast allocations during rendering. */
extern TArena<RenderAllocator>* g_vulkanArena;

using VulkanAllocator = AllocatorInstance<TArena<RenderAllocator>, &g_vulkanArena>;

} // namespace hyperion
