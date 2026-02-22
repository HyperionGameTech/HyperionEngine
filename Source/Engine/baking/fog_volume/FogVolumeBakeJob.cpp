/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <baking/fog_volume/FogVolumeBakeJob.hpp>

#include <scene/Scene.hpp>
#include <scene/FogVolume.hpp>

#include <scene/util/VoxelOctree.hpp>

namespace Hyperion {
namespace Baking {

BakeJob<FogVolume>::~BakeJob()
{
}

void BakeJob<FogVolume>::Start_Internal()
{
    const typename BakeData<FogVolume>::VolumeBitmap& volumeBitmap = m_bakeData->GetVolumeBitmap();

    const Vec3u volumeExtent = Vec3u {
        volumeBitmap.GetWidth(),
        volumeBitmap.GetHeight(),
        volumeBitmap.GetDepth()
    };

    // Flatten texel indices for processing
    m_texelIndices.Resize(volumeExtent.x * volumeExtent.y * volumeExtent.z);

    for (uint32 z = 0; z < volumeExtent.z; ++z)
    {
        for (uint32 y = 0; y < volumeExtent.y; ++y)
        {
            for (uint32 x = 0; x < volumeExtent.x; ++x)
            {
                const uint32 texelIndex = z * (volumeExtent.x * volumeExtent.y) + y * volumeExtent.x + x;

                m_texelIndices[texelIndex] = texelIndex;
            }
        }
    }
}

void BakeJob<FogVolume>::Process_Internal(bool* outIsReadyToProcess)
{
    if (outIsReadyToProcess)
    {
        *outIsReadyToProcess = true;
    }
}

uint32 BakeJob<FogVolume>::ProcessTexels(Span<LightmapTexel*> texels, uint32 texelOffset)
{
    const BoundingBox worldAabb = m_fogVolume->GetWorldBounds();
    const Vec3f extentWS = worldAabb.GetExtent();

    const Vec3u bitmapExtent = Vec3u {
        m_bakeData->GetVolumeBitmap().GetWidth(),
        m_bakeData->GetVolumeBitmap().GetHeight(),
        m_bakeData->GetVolumeBitmap().GetDepth()
    };

    const Vec3f texelHalfSizeWS = extentWS * (Vec3f(0.5f) / Vec3f(bitmapExtent));

    for (uint32 txlIdx = 0; txlIdx < uint32(texels.Size()); ++txlIdx)
    {
        LightmapTexel* texel = texels[txlIdx];
        const uint32 realTexelIndex = texelOffset + txlIdx;

        const Vec3u texelCoord = Vec3u {
            realTexelIndex % bitmapExtent.x,
            (realTexelIndex / bitmapExtent.x) % bitmapExtent.y,
            realTexelIndex / (bitmapExtent.x * bitmapExtent.y)
        };

        const Vec3f posWS = worldAabb.GetMin() + (extentWS * (Vec3f(texelCoord) / Vec3f(bitmapExtent))) + texelHalfSizeWS;

        const double dist = m_bakeData->GetVoxelOctree()->GetSignedDistanceAtPoint(posWS);

        texel->color0.x = float(dist);
        texel->color0.w = 1.0f;
    }

    return uint32(texels.Size());
}

} // namespace Baking
} // namespace Hyperion
