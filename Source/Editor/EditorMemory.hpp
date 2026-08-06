/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <Core/Memory/Allocator/AllocatorFwd.hpp>

namespace Hyperion {

#ifdef HYP_EDITOR

EDITOR_API extern Pool* g_editorPool;

using EditorAllocator = AllocatorInstance<Pool, &g_editorPool>;

#endif // HYP_EDITOR

} // namespace Hyperion
