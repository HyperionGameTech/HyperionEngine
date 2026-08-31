/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Baking/BakeEpoch.hpp>
#include <Baking/BakerScene.hpp>

#include <Scene/Scene.hpp>
#include <Scene/EntityTag.hpp>

#include <Scene/LightmapVolume.hpp>
#include <Scene/EnvProbe.hpp>

namespace Hyperion {

namespace Baking {
namespace BakeEpoch {

HashCode GetSceneHash(const Scene& scene)
{
    if (!(scene.GetSceneFlags() & SceneFlags::HAS_OCTREE))
    {
        return HashCode();
    }

    const SceneOctree& octree = scene.GetOctree();

    // clang-format off
    return HashCode(0)
        .Add(octree.GetEntryListHash<EntityTag::MobStatic>())
        .Add(octree.GetEntryListHash<EntityTag::Light>());
    // clang-format on
}

uint64 ComputeEpoch(const LightmapVolume& volume, const BakerScene& bakerScene)
{
    Scene* scene = volume.GetScene();

    if (!scene)
    {
        return 0;
    }

    return GetSceneHash(*scene).Value();
}

uint64 ComputeEpoch(const EnvProbe& probe, const BakerScene& bakerScene)
{
    Scene* scene = probe.GetScene();

    if (!scene)
    {
        return 0;
    }

    return GetSceneHash(*scene)
        .Add(bakerScene.GetEpochRev(BakerSceneCategory::Lightmap))
        .Value();
}

} // namespace BakeEpoch

} // namespace Baking

} // namespace Hyperion
