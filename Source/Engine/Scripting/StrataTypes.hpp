/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/FileSystem/FilePath.hpp>

namespace Hyperion {
namespace Strata {

/// Registers the structs described by a Strata type metadata blob (strata/strata_types.h) as dynamic
/// Structs, and its `@component` structs as runtime component types. A struct whose layout is unchanged
/// since it was last registered keeps its Struct (only its defaults are refreshed); a changed layout
/// replaces it, and existing components migrate to the new layout.
bool RegisterTypeMetadata(const void* data, size_t size, const FilePath& sourcePath);

/// Registers the types described by the metadata AOT-linked into the executable, once. Called when the first
/// Strata script starts; saved components of these types wait as unresolved until then. No-op in JIT builds,
/// where each script registers its own types when it compiles.
void RegisterLinkedTypes();

/// Re-registers the types one Strata source defines (after an edit), without running it. JIT builds only.
void ReloadTypes(const FilePath& sourcePath, const FilePath& scriptsDirectory);

} // namespace Strata
} // namespace Hyperion
