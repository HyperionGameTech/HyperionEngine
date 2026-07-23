/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

namespace Hyperion {

#pragma region Fwd declarations

namespace memory {

class Pool;

template <class AllocatorType> class TArena;

using Arena = TArena<DynamicAllocator>;

} // namespace memory

using memory::Pool;
using memory::Arena;

#pragma endregion Fwd declarations

#ifdef HYP_EDITOR

EDITOR_API extern Pool* g_editorPool;

using EditorAllocator = AllocatorInstance<Pool, &g_editorPool>;

#endif // HYP_EDITOR

} // namespace Hyperion
