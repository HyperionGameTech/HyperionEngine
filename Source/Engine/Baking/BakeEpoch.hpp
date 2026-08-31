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

namespace Baking {

struct BakerScene;

namespace BakeEpoch {

/// Combined hash of the static geometry and lights that affect a bake for \p scene
ENGINE_API HashCode GetSceneHash(const Scene& scene);

ENGINE_API uint64 ComputeEpoch(const LightmapVolume& volume, const BakerScene& bakerScene);
ENGINE_API uint64 ComputeEpoch(const EnvProbe& probe, const BakerScene& bakerScene);

} // namespace BakeEpoch
} // namespace Baking

} // namespace Hyperion
