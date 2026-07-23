/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <EditorPch.hpp>

#include <Editor/EditorMemory.hpp>

#include <Core/Memory/Allocator/Allocator.hpp>

#include <Core/Memory/Pool/Pool.hpp>

namespace Hyperion {

static constexpr size_t EditorPoolBlockSize = 16 * 1024 * 1024; // 16 MB

// Use system memory allocator for fallback
static Pool s_editorPool { EditorPoolBlockSize, PF_FALLBACK };

EDITOR_API Pool* g_editorPool = &s_editorPool;

} // namespace Hyperion
