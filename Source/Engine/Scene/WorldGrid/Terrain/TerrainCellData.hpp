/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Asset/AssetObject.hpp>

#include <Core/Math/Vector2.hpp>
#include <Core/Math/Vector3.hpp>
#include <Core/Utilities/Span.hpp>

namespace Hyperion {

HYP_CLASS(AssetBucket = "Terrain")
class ENGINE_API TerrainCellData : public AssetObject
{
    HYP_OBJECT_BODY(TerrainCellData);

public:
    TerrainCellData();
    explicit TerrainCellData(Name name, const Vec2i& coord = Vec2i::Zero(), const Vec3u& extent = Vec3u::Zero());

    TerrainCellData(const TerrainCellData& other) = delete;
    TerrainCellData& operator=(const TerrainCellData& other) = delete;

    TerrainCellData(TerrainCellData&& other) noexcept = delete;
    TerrainCellData& operator=(TerrainCellData&& other) noexcept = delete;

    ~TerrainCellData();

    HYP_FIELD(Property = "Coord", Serialize)
    Vec2i coord;

    HYP_FIELD(Property = "Extent", Serialize)
    Vec3u extent;

    ///fingerprint of the generator state the heights were produced with; see TerrainGenerator::ComputeFingerprint
    HYP_FIELD(Property = "GeneratorFingerprint", Serialize)
    uint64 generatorFingerprint = 0;

    ///sculpted heights are kept even when the fingerprint no longer matches
    HYP_FIELD(Property = "IsSculpted", Serialize)
    bool isSculpted = false;

    ///(cellSize + 2 * TerrainGenerator::CellPadding)^2 heights. True when the manifest has heights, without paging them in.
    bool HasHeights() const
    {
        return m_heights.size != 0;
    }

    void SetHeights(Span<const float> paddedHeights);
    void ClearHeights();

    Span<const float> GetHeights() const;
    Span<float> GetHeights();

    ///pages the heights in and makes them a private writable copy; false if there are none to load
    bool EnsureWritableHeights();

    void ClearSplatMap();

    static constexpr uint32 NumSplatLayers = 4;

    bool HasSplatMap() const;

    ByteView GetSplatMap();
    ConstByteView GetSplatMap() const;

    bool EnsureSplatMapAllocated(uint32 numVertices);

protected:
    virtual void PageBlobData() override;
    virtual void UnpageBlobData() override;

    bool PageBlobDataFromFile(const FilePath& directory, const char* magic, BlobDataReference& reference);

    virtual void CollectBlobDataReferences(Array<Tuple<const char*, uint16, BlobDataReference*>>& outReferences) override
    {
        // terrain heights
        outReferences.EmplaceBack("TERH", 1, &m_heights);

        // terrain splat map
        outReferences.EmplaceBack("TSM", 1, &m_splatMap);
    }

private:
    HYP_FIELD(Property = "Heights", Serialize)
    BlobDataReference m_heights;

    HYP_FIELD(Property = "SplatMap", Serialize)
    BlobDataReference m_splatMap;
};

} // namespace Hyperion
