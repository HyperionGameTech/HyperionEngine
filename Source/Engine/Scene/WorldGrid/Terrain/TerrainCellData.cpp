/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/WorldGrid/Terrain/TerrainCellData.hpp>

#include <Asset/AssetRegistry.hpp>
#include <Asset/BlobStorage.hpp>

#include <Framework/EngineGlobals.hpp>

#include <Core/Logging/Logger.hpp>

#include <Core/Memory/Memory.hpp>
#include <Core/Memory/ByteBuffer.hpp>

#include <TerrainCellData.generated.inl>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(WorldGrid);

#pragma region TerrainCellData

TerrainCellData::TerrainCellData()
    : AssetObject()
{
}

TerrainCellData::TerrainCellData(Name name, const Vec2i& coord, const Vec3u& extent)
    : AssetObject(name),
      coord(coord),
      extent(extent)
{
}

TerrainCellData::~TerrainCellData()
{
    FreeBlobData(m_heights);
    FreeBlobData(m_splatMap);
    FreeBlobData(m_erosionMasks);
    FreeBlobData(m_groundCoverPaint);
}

void TerrainCellData::SetHeights(Span<const float> paddedHeights)
{
    FreeBlobData(m_heights);

    m_heights = BlobDataReference {};

    if (paddedHeights.Size() != 0)
    {
        AllocateBlobData(m_heights, paddedHeights.Data(), paddedHeights.Size() * sizeof(float), alignof(float));
    }

    MarkDirty();
}

void TerrainCellData::ClearHeights()
{
    FreeBlobData(m_heights);

    m_heights = BlobDataReference {};

    FreeBlobData(m_erosionMasks);

    m_erosionMasks = BlobDataReference {};

    generatorFingerprint = 0;
    isSculpted = false;

    MarkDirty();
}

Span<const float> TerrainCellData::GetHeights() const
{
    if (m_heights.raw == nullptr || m_heights.size == 0 || m_heights.size % sizeof(float) != 0)
    {
        return Span<const float>();
    }

    return Span<const float>((const float*)m_heights.raw, m_heights.size / sizeof(float));
}

Span<float> TerrainCellData::GetHeights()
{
    if (m_heights.raw == nullptr || m_heights.readOnly || m_heights.size == 0 || m_heights.size % sizeof(float) != 0)
    {
        return Span<float>();
    }

    return Span<float>((float*)m_heights.raw, m_heights.size / sizeof(float));
}

bool TerrainCellData::EnsureWritableHeights()
{
    if (m_heights.size == 0)
    {
        return false;
    }

    const auto makeWritable = [this]()
    {
        if (m_heights.raw != nullptr && m_heights.readOnly)
        {
            SetBlobDataResident(true);
        }

        return m_heights.raw != nullptr && !m_heights.readOnly;
    };

    if (makeWritable())
    {
        return true;
    }

    auto readScope = GetReadScope();

    return makeWritable();
}

bool TerrainCellData::HasSplatMap() const
{
    return m_splatMap.size != 0;
}

void TerrainCellData::ClearSplatMap()
{
    FreeBlobData(m_splatMap);

    m_splatMap = BlobDataReference {};

    MarkDirty();
}

ByteView TerrainCellData::GetSplatMap()
{
    if (m_splatMap.raw == nullptr || m_splatMap.readOnly || m_splatMap.size == 0)
    {
        return ByteView();
    }

    return ByteView((ubyte*)m_splatMap.raw, m_splatMap.size);
}

ConstByteView TerrainCellData::GetSplatMap() const
{
    if (m_splatMap.raw == nullptr || m_splatMap.size == 0)
    {
        return ConstByteView();
    }

    return ConstByteView((const ubyte*)m_splatMap.raw, m_splatMap.size);
}

void TerrainCellData::SetErosionMasks(ConstByteView erosionMasks)
{
    FreeBlobData(m_erosionMasks);

    m_erosionMasks = BlobDataReference {};

    if (erosionMasks.Size() != 0)
    {
        AllocateBlobData(m_erosionMasks, erosionMasks.Data(), erosionMasks.Size(), 1);
    }

    MarkDirty();
}

ConstByteView TerrainCellData::GetErosionMasks() const
{
    if (m_erosionMasks.raw == nullptr || m_erosionMasks.size == 0)
    {
        return ConstByteView();
    }

    return ConstByteView((const ubyte*)m_erosionMasks.raw, m_erosionMasks.size);
}

ByteView TerrainCellData::GetGroundCoverPaint()
{
    if (m_groundCoverPaint.raw == nullptr || m_groundCoverPaint.readOnly || m_groundCoverPaint.size == 0)
    {
        return ByteView();
    }

    return ByteView((ubyte*)m_groundCoverPaint.raw, m_groundCoverPaint.size);
}

ConstByteView TerrainCellData::GetGroundCoverPaint() const
{
    if (m_groundCoverPaint.raw == nullptr || m_groundCoverPaint.size == 0)
    {
        return ConstByteView();
    }

    return ConstByteView((const ubyte*)m_groundCoverPaint.raw, m_groundCoverPaint.size);
}

void TerrainCellData::SetGroundCoverPaint(const Array<Name>& layers, ConstByteView paint)
{
    FreeBlobData(m_groundCoverPaint);
    m_groundCoverPaint = BlobDataReference {};

    if (paint.Size() != 0 && layers.Any())
    {
        AllocateBlobData(m_groundCoverPaint, paint.Data(), paint.Size(), 1);

        m_groundCoverPaintLayers = layers;
    }
    else
    {
        m_groundCoverPaintLayers.Clear();
    }

    MarkDirty();
}

int32 TerrainCellData::FindGroundCoverPaintLayer(Name layerName) const
{
    for (uint32 layerIndex = 0; layerIndex < uint32(m_groundCoverPaintLayers.Size()); layerIndex++)
    {
        if (m_groundCoverPaintLayers[layerIndex] == layerName)
        {
            return int32(layerIndex);
        }
    }

    return -1;
}

bool TerrainCellData::EnsureWritableGroundCoverPaint()
{
    if (m_groundCoverPaint.size == 0)
    {
        return false;
    }

    const auto makeWritable = [this]()
    {
        if (m_groundCoverPaint.raw != nullptr && m_groundCoverPaint.readOnly)
        {
            SetBlobDataResident(true, m_groundCoverPaint);
        }

        return m_groundCoverPaint.raw != nullptr && !m_groundCoverPaint.readOnly;
    };

    if (makeWritable())
    {
        return true;
    }

    auto readScope = GetReadScope();

    return makeWritable();
}

int32 TerrainCellData::EnsureGroundCoverPaintLayer(Name layerName, uint32 numVertices)
{
    const size_t planeSize = size_t(numVertices);
    const size_t existingSize = planeSize * m_groundCoverPaintLayers.Size();

    if (m_groundCoverPaintLayers.Any() && (!EnsureWritableGroundCoverPaint() || m_groundCoverPaint.size != existingSize))
    {
        HYP_LOG(WorldGrid, Warning, "Ground cover paint of terrain cell data '{}' is {} bytes but {} layers of {} vertices need {} - clearing it",
            GetName(), m_groundCoverPaint.size, m_groundCoverPaintLayers.Size(), numVertices, existingSize);

        auto writeScope = GetWriteScope();

        FreeBlobData(m_groundCoverPaint);
        m_groundCoverPaint = BlobDataReference {};
        m_groundCoverPaintLayers.Clear();

        MarkDirty();
    }

    if (const int32 existingLayer = FindGroundCoverPaintLayer(layerName); existingLayer >= 0)
    {
        return existingLayer;
    }

    auto writeScope = GetWriteScope();

    ByteBuffer oldData;

    if (m_groundCoverPaint.raw != nullptr && m_groundCoverPaint.size != 0)
    {
        oldData = ByteBuffer(ConstByteView((const ubyte*)m_groundCoverPaint.raw, m_groundCoverPaint.size));
    }

    const size_t requiredSize = planeSize * (m_groundCoverPaintLayers.Size() + 1);

    FreeBlobData(m_groundCoverPaint);
    m_groundCoverPaint = BlobDataReference {};

    AllocateBlobData(m_groundCoverPaint, nullptr, requiredSize, 1);

    if (m_groundCoverPaint.raw == nullptr || m_groundCoverPaint.size < requiredSize)
    {
        m_groundCoverPaintLayers.Clear();

        return -1;
    }

    Memory::Zero(m_groundCoverPaint.raw, requiredSize);

    if (oldData.Size() != 0)
    {
        Memory::Copy(m_groundCoverPaint.raw, oldData.Data(), oldData.Size());
    }

    m_groundCoverPaintLayers.PushBack(layerName);

    MarkDirty();

    return int32(m_groundCoverPaintLayers.Size() - 1);
}

bool TerrainCellData::EnsureSplatMapAllocated(uint32 numVertices)
{
    const size_t requiredSize = size_t(numVertices) * NumSplatLayers;

    const auto checkIsResident = [this, requiredSize]()
    {
        return m_splatMap.raw != nullptr && m_splatMap.size >= requiredSize;
    };

    if (checkIsResident())
    {
        if (m_splatMap.readOnly)
        {
            SetBlobDataResident(true);
        }

        return true;
    }

    {
        auto readScope = GetReadScope();

        if (checkIsResident())
        {
            if (m_splatMap.readOnly)
            {
                SetBlobDataResident(true);
            }

            MarkDirty();

            return true;
        }
    }

    // Mutating the asset from here on, so writers scope.
    auto writeScope = GetWriteScope();

    if (checkIsResident())
    {
        MarkDirty();

        return true;
    }

    if (m_splatMap.raw != nullptr)
    {
        // Resident, but smaller than needed - grow it, keeping the painted weights.
        ByteBuffer oldData(ConstByteView((const ubyte*)m_splatMap.raw, m_splatMap.size));
        const size_t oldSize = oldData.Size();
        const size_t oldNumVertices = oldSize / NumSplatLayers;

        FreeBlobData(m_splatMap);
        AllocateBlobData(m_splatMap, nullptr, requiredSize, 1);

        if (m_splatMap.raw == nullptr || m_splatMap.size < requiredSize)
        {
            return false;
        }

        Memory::Copy(m_splatMap.raw, oldData.Data(), oldSize);

        // Default new vertices to layer 0 fully painted.
        ubyte* splatData = (ubyte*)m_splatMap.raw;

        for (size_t i = oldNumVertices; i < size_t(numVertices); i++)
        {
            splatData[i * NumSplatLayers] = 255;
        }

        MarkDirty();

        return true;
    }

    FreeBlobData(m_splatMap);
    AllocateBlobData(m_splatMap, nullptr, requiredSize, 1);

    if (m_splatMap.raw == nullptr || m_splatMap.size < requiredSize)
    {
        return false;
    }

    // Default to layer 0 fully painted.
    Memory::Zero(m_splatMap.raw, requiredSize);

    ubyte* splatData = (ubyte*)m_splatMap.raw;

    for (size_t i = 0; i < size_t(numVertices); i++)
    {
        splatData[i * NumSplatLayers] = 255;
    }

    MarkDirty();

    return true;
}

void TerrainCellData::PageBlobData()
{
    if (IsTransient() || !IsRegistered())
    {
        return;
    }

    Handle<AssetRegistry> registry = GetAssetRegistry();
    AssertDebug(registry.IsValid());

    if (!registry.IsValid())
    {
        return;
    }

    const FilePath blobDirectory = registry->GetRootPath() / AssetBuckets::Terrain.GetName();

    if (m_heights.raw == nullptr
        && m_heights.key
        && m_heights.size != 0)
    {
        if (!PageBlobDataFromStorage(m_heights))
        {
            PageBlobDataFromFile(blobDirectory, HeightsBlobMagic, m_heights);
        }
    }

    if (m_splatMap.raw == nullptr
        && m_splatMap.key
        && m_splatMap.size != 0)
    {
        if (!PageBlobDataFromStorage(m_splatMap))
        {
            PageBlobDataFromFile(blobDirectory, SplatMapBlobMagic, m_splatMap);
        }
    }

    if (m_erosionMasks.raw == nullptr
        && m_erosionMasks.key
        && m_erosionMasks.size != 0)
    {
        if (!PageBlobDataFromStorage(m_erosionMasks))
        {
            PageBlobDataFromFile(blobDirectory, ErosionMasksBlobMagic, m_erosionMasks);
        }
    }

    if (m_groundCoverPaint.raw == nullptr
        && m_groundCoverPaint.key
        && m_groundCoverPaint.size != 0)
    {
        if (!PageBlobDataFromStorage(m_groundCoverPaint))
        {
            PageBlobDataFromFile(blobDirectory, GroundCoverPaintBlobMagic, m_groundCoverPaint);
        }
    }
}

bool TerrainCellData::PageBlobDataFromFile(const FilePath& directory, const char* magic, BlobDataReference& reference)
{
    const Name blobKey = reference.key;
    const uint64 expectedSize = reference.size;

    FileByteReader stream { directory / (String(*GetName()) + "." + magic + ".raw.blob") };

    if (stream.Eof())
    {
        // No local blob data on disk - mark read-only so we don't retry until persisted.
        reference.readOnly = true;

        return false;
    }

    if (stream.Max() != expectedSize)
    {
        HYP_LOG(WorldGrid, Error, "Local blob data for terrain cell data asset '{}' is {} bytes but the manifest expects {}, ignoring it",
                GetName(), stream.Max(), expectedSize);

        return false;
    }

    ByteBuffer buffer = stream.Read(stream.Max());

    AllocateBlobData(reference, buffer.Data(), buffer.Size(), 1);
    reference.key = blobKey;

    return true;
}

void TerrainCellData::UnpageBlobData()
{
    AssetObject::UnpageBlobData();

    AssertBlobDataPersisted(m_heights);

    if (!m_heights.readOnly)
    {
        FreeBlobData(m_heights);
    }

    m_heights.raw = nullptr;

    AssertBlobDataPersisted(m_splatMap);

    if (!m_splatMap.readOnly)
    {
        FreeBlobData(m_splatMap);
    }

    m_splatMap.raw = nullptr;

    AssertBlobDataPersisted(m_erosionMasks);

    if (!m_erosionMasks.readOnly)
    {
        FreeBlobData(m_erosionMasks);
    }

    m_erosionMasks.raw = nullptr;

    AssertBlobDataPersisted(m_groundCoverPaint);

    if (!m_groundCoverPaint.readOnly)
    {
        FreeBlobData(m_groundCoverPaint);
    }

    m_groundCoverPaint.raw = nullptr;
}

#pragma endregion TerrainCellData

} // namespace Hyperion
