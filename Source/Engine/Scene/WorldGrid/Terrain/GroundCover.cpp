/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/WorldGrid/Terrain/GroundCover.hpp>

#include <Scene/Prefab.hpp>

#include <GroundCover.generated.inl>

namespace Hyperion {

GroundCover::GroundCover()
    : GroundCover(Name::Invalid())
{
}

GroundCover::GroundCover(Name name)
    : AssetObject(name)
{
}

} // namespace Hyperion
