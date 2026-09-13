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
            PageBlobDataFromFile(blobDirectory, "TCD", m_heights);
        }
    }

    if (m_splatMap.raw == nullptr
        && m_splatMap.key
        && m_splatMap.size != 0)
    {
        if (!PageBlobDataFromStorage(m_splatMap))
        {
            PageBlobDataFromFile(blobDirectory, "SPLT", m_splatMap);
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
}

#pragma endregion TerrainCellData

} // namespace Hyperion
