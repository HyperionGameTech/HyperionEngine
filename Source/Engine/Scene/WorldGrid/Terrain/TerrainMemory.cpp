/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/WorldGrid/Terrain/TerrainMemory.hpp>

namespace Hyperion {

ENGINE_API Pool* g_terrainPool;

void InitTerrainMemory()
{
    Assert(g_terrainPool == nullptr);

    // heightmaps and erosion regions don't fit a block
    g_terrainPool = new Pool(TerrainPoolBlockSize, PF_THREAD_SAFE | PF_FALLBACK);
}

void ShutdownTerrainMemory()
{
    delete g_terrainPool;
    g_terrainPool = nullptr;
}

} // namespace Hyperion
