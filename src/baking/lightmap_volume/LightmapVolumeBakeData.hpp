/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <baking/BakeData.hpp>

namespace Hyperion {

class LightmapVolume;

namespace Baking {

template <>
class BakeData<LightmapVolume> : public BakeDataBase
{
public:
    using BitmapType = Bitmap_RGBA32F;

    using MeshFloatDataArray = Array<float, DynamicAllocator>;
    using MeshIndexArray = Array<uint32, DynamicAllocator>;

    BakeData()
        : m_volume(nullptr)
    {
    }

    BakeData(Span<const BakeEntity> bakeEntities, LightmapVolume* volume);

    BakeData(const BakeData& other) = default;
    BakeData(BakeData&& other) noexcept = default;

    BakeData& operator=(const BakeData& other) = default;
    BakeData& operator=(BakeData&& other) noexcept = default;

    ~BakeData() override = default;

    HYP_FORCE_INLINE const Array<BakeMesh>& GetMeshData() const
    {
        return m_meshData;
    }

    virtual Result Build() override;

    BitmapType ToBitmapIrradiance() const;
    BitmapType ToBitmapRadiance() const;

private:
    LightmapVolume* m_volume;

    Array<BakeMesh> m_meshData;

    Array<LightmapRay> m_rays;

    // Per element mesh data used for building the UV map
    Array<MeshFloatDataArray, DynamicAllocator> m_meshVertexPositions;
    Array<MeshFloatDataArray, DynamicAllocator> m_meshVertexNormals;
    Array<MeshFloatDataArray, DynamicAllocator> m_meshVertexUvs;
    Array<Array<uint32>, DynamicAllocator> m_meshIndices;
};

} // namespace Baking

} // namespace Hyperion
