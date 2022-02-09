#include "noise_terrain_control.h"

namespace hyperion {

NoiseTerrainControl::NoiseTerrainControl(Camera *camera, int seed)
    : TerrainControl(fbom::FBOMObjectType("NOISE_TERRAIN_CONTROL"), camera), 
      seed(seed)
{
}

std::shared_ptr<TerrainChunk> NoiseTerrainControl::NewChunk(const ChunkInfo &chunk_info)
{
    const std::vector<double> heights = NoiseTerrainChunk::GenerateHeights(seed, chunk_info);

    return std::make_shared<NoiseTerrainChunk>(heights, chunk_info);
}

std::shared_ptr<EntityControl> NoiseTerrainControl::CloneImpl()
{
    return std::make_shared<NoiseTerrainControl>(nullptr, seed); // TODO
}

} // namespace hyperion
