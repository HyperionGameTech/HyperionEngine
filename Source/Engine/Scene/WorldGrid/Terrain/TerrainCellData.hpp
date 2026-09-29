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
#include <Core/Containers/Array.hpp>

namespace Hyperion {

HYP_CLASS(AssetBucket = "Terrain")
class ENGINE_API TerrainCellData : public AssetObject
{
    HYP_OBJECT_BODY(TerrainCellData);

public:
    static constexpr const char* HeightsBlobMagic = "TCD";
    static constexpr const char* SplatMapBlobMagic = "SPLT";
    static constexpr const char* ErosionMasksBlobMagic = "EMSK";
    static constexpr const char* GroundCoverPaintBlobMagic = "GCPT";

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

    ///fingerprint of the generator state the heights were produced with
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

    ///cellSize^2 TerrainErosionMasks saved with generated heights. Kept as generated when the heights are sculpted
    bool HasErosionMasks() const
    {
        return m_erosionMasks.size != 0;
    }

    void SetErosionMasks(ConstByteView erosionMasks);

    ConstByteView GetErosionMasks() const;

    ///one cellSize^2 plane of painted weights per painted GroundCover layer, in the order of GetGroundCoverPaintLayers()
    bool HasGroundCoverPaint() const
    {
        return m_groundCoverPaint.size != 0;
    }

    const Array<Name>& GetGroundCoverPaintLayers() const
    {
        return m_groundCoverPaintLayers;
    }

    ByteView GetGroundCoverPaint();
    ConstByteView GetGroundCoverPaint() const;

    ///pages the planes in as a writable copy and appends a cleared plane for \p layerName if it has none. The plane's index, or -1
    int32 EnsureGroundCoverPaintLayer(Name layerName, uint32 numVertices);

    ///index of \p layerName's plane, or -1 if it has never been painted here
    int32 FindGroundCoverPaintLayer(Name layerName) const;

    ///pages the planes in as a writable copy; false if there are none
    bool EnsureWritableGroundCoverPaint();

    void SetGroundCoverPaint(const Array<Name>& layers, ConstByteView paint);

protected:
    virtual void PageBlobData() override;
    virtual void UnpageBlobData() override;

    bool PageBlobDataFromFile(const FilePath& directory, const char* magic, BlobDataReference& reference);

    virtual void CollectBlobDataReferences(Array<Tuple<const char*, uint16, BlobDataReference*>>& outReferences) override
    {
        outReferences.EmplaceBack(HeightsBlobMagic, 1, &m_heights);
        outReferences.EmplaceBack(SplatMapBlobMagic, 1, &m_splatMap);
        outReferences.EmplaceBack(ErosionMasksBlobMagic, 1, &m_erosionMasks);
        outReferences.EmplaceBack(GroundCoverPaintBlobMagic, 1, &m_groundCoverPaint);
    }

private:
    HYP_FIELD(Property = "Heights", Serialize)
    BlobDataReference m_heights;

    HYP_FIELD(Property = "SplatMap", Serialize)
    BlobDataReference m_splatMap;

    HYP_FIELD(Property = "ErosionMasks", Serialize)
    BlobDataReference m_erosionMasks;

    HYP_FIELD(Property = "GroundCoverPaintLayers", Serialize)
    Array<Name> m_groundCoverPaintLayers;

    HYP_FIELD(Property = "GroundCoverPaint", Serialize)
    BlobDataReference m_groundCoverPaint;
};

} // namespace Hyperion
