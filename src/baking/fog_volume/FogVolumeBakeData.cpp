/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <baking/fog_volume/FogVolumeBakeData.hpp>

#include <scene/FogVolume.hpp>
#include <scene/util/VoxelOctree.hpp>

#include <util/NoiseFactory.hpp>

namespace Hyperion {

namespace Baking {

static struct FogVolumeNoiseCombinator
{
    NoiseCombinator noiseCombinator;

    FogVolumeNoiseCombinator()
    {
        noiseCombinator
            // Base Density
            .Use<SimplexNoiseGenerator>(0, NoiseCombinator::Mode::ADDITIVE, 0.4f, 0.0f, Vec3f(15.0f))
            // Structure (Mid-Frequency)
            .Use<SimplexNoiseGenerator>(1, NoiseCombinator::Mode::ADDITIVE, 0.3f, 0.0f, Vec3f(60.0f))
            // Grain (High-Frequency)
            .Use<SimplexNoiseGenerator>(2, NoiseCombinator::Mode::ADDITIVE, 0.2f, 0.0f, Vec3f(250.0f))
            // Eraser (Subtractive Worley)
            .Use<WorleyNoiseGenerator>(3, NoiseCombinator::Mode::SUBTRACTIVE, 0.2f, 0.0f, Vec3f(300.0f));
    }
} s_initializer;

static void GenerateNoiseBitmap(typename BakeData<FogVolume>::NoiseBitmap& noiseBitmap)
{
    for (uint32 z = 0; z < noiseBitmap.GetDepth(); z++)
    {
        for (uint32 y = 0; y < noiseBitmap.GetHeight(); y++)
        {
            for (uint32 x = 0; x < noiseBitmap.GetWidth(); x++)
            {
                const float noiseValue = s_initializer.noiseCombinator.GetNoise(
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
    octreeParams.maxDepth = 5;

    m_voxelOctree = MakeUnique<VoxelOctree>();

    auto buildResult = m_voxelOctree->Build(octreeParams, m_fogVolume->GetEntityManager());

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
