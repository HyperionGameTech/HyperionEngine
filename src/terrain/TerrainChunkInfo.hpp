#ifndef HYPERION_V2_TERRAIN_CHUNK_INFO_H
#define HYPERION_V2_TERRAIN_CHUNK_INFO_H

#include <math/Vector2.hpp>
#include <math/Vector3.hpp>
#include <rendering/backend/RendererStructs.hpp>

#include <Types.hpp>

#include <memory>

namespace hyperion::v2 {

using renderer::Extent3D;

// struct TerrainChunkNeighborInfo {
//     Vector2 position;
//     bool    in_queue;
// };

// struct TerrainChunkInfo {
//     Extent3D extent;
//     Vector2  position;
//     Vector3  scale;
// };

} // namespace hyperion::v2

#endif