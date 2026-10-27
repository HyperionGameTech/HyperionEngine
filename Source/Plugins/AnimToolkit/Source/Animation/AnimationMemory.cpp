/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>
#include <Animation/AnimationMemory.hpp>

#include <Core/Memory/Allocator/Allocator.hpp>

#include <Core/Memory/Pool/Pool.hpp>

namespace Hyperion {

static constexpr size_t AnimPoolBlockSize = 8 * 1024 * 1024; // 8 MiB

static Pool s_animPool { AnimPoolBlockSize, PF_THREAD_SAFE | PF_FALLBACK };
ANIMATION_API Pool* g_animPool = &s_animPool;

} // namespace Hyperion
