/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Baking/BakeData.hpp>
#include <Baking/BakerMemory.hpp>

#include <Scene/LightmapVolume.hpp>

namespace Hyperion {

namespace Baking {

template <>
class BakeData<LightmapVolume> : public BakeDataBase
{
public:
    using ColorBitmap = Bitmap_R11G11B10F;
    using BentNormalBitmap = Bitmap_RGBA8;

    struct MeshSnapshot
    {
        Handle<Mesh> mesh;

        VertexInputLayoutDesc layout;
        Array<float, BakerAllocator> vertices;
        Array<uint32, BakerAllocator> indices;

        bool needsUnwrap = false;
        bool unwrapped = false;
        bool valid = true;

        Array<uint32, BakerAllocator> triangleCharts;
        uint32 numCharts = 0;

        Vec2f uvExtent;
        float uvArea = 0.0f;
    };

    struct EntityRect
    {
        bool valid = false;

        uint16 atlasIndex = 0;
        uint16 elementIndex = 0;

        Vec2u offsetCoords;
        Vec2u dimensions;

        Vec2f offsetUV;
        Vec2f scale;
    };

    BakeData()
        : m_texelsPerUnit(0.0f)
    {
    }

    BakeData(Span<const BakeEntity> bakeEntities, float texelsPerUnit);

    BakeData(const BakeData& other) = default;
    BakeData(BakeData&& other) noexcept = default;

    BakeData& operator=(const BakeData& other) = default;
    BakeData& operator=(BakeData&& other) noexcept = default;

    ~BakeData() override = default;

    void UseExistingPacking(const LightmapVolume& volume, Array<EntityRect, BakerAllocator>&& entityRects);

    HYP_FORCE_INLINE bool IsReusingExistingPacking() const
    {
        return m_reuseExistingPacking;
    }

    bool AnyMeshNeedsUnwrap() const;

    virtual Result Build() override;

    HYP_FORCE_INLINE uint32 GetAtlasCount() const
    {
        return m_atlasCount;
    }

    HYP_FORCE_INLINE Span<const MeshSnapshot> GetMeshSnapshots() const
    {
        return m_meshes;
    }

    HYP_FORCE_INLINE const MeshSnapshot& GetMeshSnapshotForEntity(uint32 entityIndex) const
    {
        return m_meshes[m_entityMeshIndices[entityIndex]];
    }

    HYP_FORCE_INLINE Span<const EntityRect> GetEntityRects() const
    {
        return m_entityRects;
    }

    /*! \brief The packing built by Build() when not reusing the existing one. */
    HYP_FORCE_INLINE Array<LightmapVolumeAtlas>& GetPackedAtlases()
    {
        return m_packedAtlases;
    }

    void Blur();
    void Dilate();

    ColorBitmap ToBitmapIrradiance(uint32 atlasIndex) const;
    BentNormalBitmap ToBitmapBentNormal(uint32 atlasIndex) const;

private:
    Result UnwrapMesh(MeshSnapshot& meshSnapshot) const;
    void ComputeMeshCharts(MeshSnapshot& meshSnapshot) const;

    Result PackEntities();

    void RasterizeEntity(uint32 entityIndex, uint32 chartBase);

    Array<MeshSnapshot, BakerAllocator> m_meshes;
    Array<uint32, BakerAllocator> m_entityMeshIndices;

    Array<EntityRect, BakerAllocator> m_entityRects;
    Array<LightmapVolumeAtlas> m_packedAtlases;

    Array<LightmapRay, BakerAllocator> m_rays;

    float m_texelsPerUnit;

    bool m_reuseExistingPacking = false;

    Vec2u m_atlasDimensions = LightmapVolume::DefaultAtlasDimensions;
    uint32 m_atlasCount = 0;
};

} // namespace Baking

} // namespace Hyperion
