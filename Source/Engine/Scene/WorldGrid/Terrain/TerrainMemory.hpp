/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Defines.hpp>

#include <Core/Memory/Pool/Pool.hpp>

namespace Hyperion {

static constexpr size_t TerrainPoolBlockSize = 16 * 1024 * 1024; // 16 MiB

ENGINE_API extern Pool* g_terrainPool; // thread safe pool. cells generate and build on the streaming and background threads

using TerrainAllocator = AllocatorInstance<Pool, &g_terrainPool>;

void InitTerrainMemory();
void ShutdownTerrainMemory();

} // namespace Hyperion
