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

    void SetSculptDelta(ConstByteView view);

    /*! True when the manifest says this cell has sculpt data - does not require the blob to be paged in. */
    bool HasSculptDelta() const
    {
        return m_sculptDelta.size != 0;
    }

    ByteView GetSculptDelta();
    ConstByteView GetSculptDelta() const;

    Span<const float> GetSculptDeltaFloat() const;

    bool EnsureWritableSculptDelta(uint32 numVertices);

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
        // terrain sculpt deltas
        outReferences.EmplaceBack("TERA", 1, &m_sculptDelta);

        // terrain splat map
        outReferences.EmplaceBack("TSM", 1, &m_splatMap);
    }

private:
    HYP_FIELD(Property = "SculptDelta", Serialize)
    BlobDataReference m_sculptDelta;

    HYP_FIELD(Property = "SplatMap", Serialize)
    BlobDataReference m_splatMap;
};

} // namespace Hyperion
