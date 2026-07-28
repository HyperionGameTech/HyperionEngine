/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Baking/BakeData.hpp>
#include <Baking/BakerMemory.hpp>

namespace Hyperion {

class LightmapVolume;

namespace Baking {

template <>
class BakeData<LightmapVolume> : public BakeDataBase
{
public:
    using BitmapType = Bitmap_RGBA8;

    using MeshFloatDataArray = Array<float, BakerAllocator>;
    using MeshIndexArray = Array<uint32, BakerAllocator>;

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

    HYP_FORCE_INLINE Span<BakeMeshData> GetMeshData()
    {
        return m_meshData;
    }

    HYP_FORCE_INLINE Span<const BakeMeshData> GetMeshData() const
    {
        return m_meshData;
    }

    virtual Result Build() override;

    HYP_FORCE_INLINE uint32 GetAtlasCount() const
    {
        return atlasCount;
    }

    void Blur();
    void Dilate();

    BitmapType ToBitmapIrradiance(uint32 atlasIndex) const;
    BitmapType ToBitmapRadiance(uint32 atlasIndex) const;
    BitmapType ToBitmapBentNormal(uint32 atlasIndex) const;

private:
    LightmapVolume* m_volume;

    Array<BakeMeshData, BakerAllocator> m_meshData;

    Array<LightmapRay, BakerAllocator> m_rays;

    // Per element mesh data used for building the UV map
    Array<MeshFloatDataArray, BakerAllocator> m_meshVertexPositions;
    Array<MeshFloatDataArray, BakerAllocator> m_meshVertexNormals;
    Array<MeshFloatDataArray, BakerAllocator> m_meshVertexUvs;
    Array<Array<uint32>, BakerAllocator> m_meshIndices;

    uint32 atlasCount = 1;
};

} // namespace Baking

} // namespace Hyperion
