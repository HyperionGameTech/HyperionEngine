/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/HashCode.hpp>
#include <Core/Defines.hpp>

namespace Hyperion {

class Scene;
class LightmapVolume;
class EnvProbe;
class Light;

namespace Baking {

struct BakeLayer;
struct BakeLayerHashes;

namespace BakeEpoch {

/// Combined hash of the static geometry and lights that affect a bake for \p scene
ENGINE_API void ComputeSceneHashes(const Scene& scene, BakeLayerHashes& inOutResult);

ENGINE_API uint64 ComputeEpoch(const LightmapVolume& volume, BakeLayer& bakeLayer);
ENGINE_API uint64 ComputeEpoch(const EnvProbe& probe, BakeLayer& bakeLayer);
ENGINE_API uint64 ComputeEpoch(const Light& light, BakeLayer& bakeLayer);

} // namespace BakeEpoch
} // namespace Baking

} // namespace Hyperion
