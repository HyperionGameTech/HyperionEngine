/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Baking/FogVolume/FogVolumeBakeData.hpp>

#include <Scene/FogVolume.hpp>
#include <Scene/Util/VoxelOctree.hpp>

#include <Util/NoiseFactory.hpp>

namespace Hyperion {

namespace Baking {

static void GenerateNoiseBitmap(typename BakeData<FogVolume>::NoiseBitmap& noiseBitmap)
{
    class FogVolumeNoiseCombinator : public NoiseCombinator
    {
    public:
        FogVolumeNoiseCombinator()
        {
            Use<SimplexNoiseGenerator>(0, NoiseCombinator::Mode::ADDITIVE, 0.5f, 0.0f, Vec3f(20.0f));
            Use<SimplexNoiseGenerator>(1, NoiseCombinator::Mode::ADDITIVE, 0.3f, 0.0f, Vec3f(70.0f));
            Use<SimplexNoiseGenerator>(2, NoiseCombinator::Mode::ADDITIVE, 0.4f, 0.0f, Vec3f(80.0f));
            Use<WorleyNoiseGenerator>(3, NoiseCombinator::Mode::SUBTRACTIVE, 0.1f, 0.0f, Vec3f(80.0f));
        }
    };

    FogVolumeNoiseCombinator noiseCombinator;

    for (uint32 z = 0; z < noiseBitmap.GetDepth(); z++)
    {
        for (uint32 y = 0; y < noiseBitmap.GetHeight(); y++)
        {
            for (uint32 x = 0; x < noiseBitmap.GetWidth(); x++)
            {
                const float noiseValue = noiseCombinator.GetNoise(
                    Vec3f(
                        float(x) / float(noiseBitmap.GetWidth()),
                        float(y) / float(noiseBitmap.GetHeight()),
                        float(z) / float(noiseBitmap.GetDepth())));

                noiseBitmap.GetPixelReference(x, y, z).SetComponentFloat(0, noiseValue);
            }
        }
    }
}

Result BakeData<FogVolume>::Build()
{
    Assert(m_fogVolume != nullptr);

    const BoundingBox localBounds = m_fogVolume->GetLocalBounds();
    const Vec3f localBoundsExtent = localBounds.GetExtent();
    const float maxExtent = localBoundsExtent.Max();

    if (maxExtent < MathUtil::epsilonF)
    {
        dimensions = Vec3u::One();
    }
    else
    {
        const float scale = float(FogVolume::MaxVolumeTextureExtent) / maxExtent;
        dimensions = Vec3u(MathUtil::Max(Vec3f(1.0f), MathUtil::Ceil(localBoundsExtent * scale)));
    }

    if (dimensions.Volume() == 0)
    {
        dimensions = Vec3u::One();
    }

    m_volumeBitmap = VolumeBitmap(
        dimensions.x,
        dimensions.y,
        dimensions.z);

    const Vec3f extentWS = m_fogVolume->GetWorldBounds().GetExtent();
    const Vec3f texelSizeWS = extentWS / Vec3f(dimensions);

    texels.Resize(dimensions.Volume());

    BoundingBox voxelOctreeAabb = m_fogVolume->GetWorldBounds();

    if (!voxelOctreeAabb.IsValid() || !voxelOctreeAabb.IsFinite() || voxelOctreeAabb.IsZero())
    {
        return HYP_MAKE_ERROR(Error, "Invalid fog volume AABB for voxel octree build");
    }

    VoxelOctreeParams octreeParams;
    octreeParams.aabb = voxelOctreeAabb;
    octreeParams.allowResize = false;
    octreeParams.maxDepth = 3;

    m_voxelOctree = MakeUnique<VoxelOctree>();

    auto buildResult = m_voxelOctree->Build(octreeParams, *m_fogVolume->GetEntityManager());

    if (buildResult.HasError())
    {
        return buildResult.GetError();
    }

    m_noiseBitmap = NoiseBitmap(
        MaxNoiseBitmapExtent,
        MaxNoiseBitmapExtent,
        MaxNoiseBitmapExtent);

    GenerateNoiseBitmap(m_noiseBitmap);

    return {};
}

} // namespace Baking

} // namespace Hyperion
