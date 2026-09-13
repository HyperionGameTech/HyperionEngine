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
    FreeBlobData(m_sculptDelta);
    FreeBlobData(m_splatMap);
}

void TerrainCellData::SetSculptDelta(ConstByteView view)
{
    if (view.Size() == 0 && m_sculptDelta.size == 0)
    {
        return;
    }

    FreeBlobData(m_sculptDelta);

    if (view.Size() != 0)
    {
        AllocateBlobData(m_sculptDelta, view.Data(), view.Size(), 1);
    }

    MarkDirty();
}

ByteView TerrainCellData::GetSculptDelta()
{
    if (m_sculptDelta.raw == nullptr || m_sculptDelta.readOnly || m_sculptDelta.size == 0)
    {
        return ByteView();
    }

    return ByteView((ubyte*)m_sculptDelta.raw, m_sculptDelta.size);
}

ConstByteView TerrainCellData::GetSculptDelta() const
{
    if (m_sculptDelta.raw == nullptr || m_sculptDelta.size == 0)
    {
        return ConstByteView();
    }

    return ConstByteView((const ubyte*)m_sculptDelta.raw, m_sculptDelta.size);
}

Span<const float> TerrainCellData::GetSculptDeltaFloat() const
{
    ConstByteView blob = GetSculptDelta();

    if (blob.Size() == 0 || blob.Size() % sizeof(float) != 0)
    {
        return Span<const float>();
    }

    return Span<const float>((const float*)blob.Data(), blob.Size() / sizeof(float));
}

bool TerrainCellData::EnsureWritableSculptDelta(uint32 numVertices)
{
    const size_t requiredSize = size_t(numVertices) * sizeof(float);

    const auto checkIsResident = [this, requiredSize]()
    {
        return m_sculptDelta.raw != nullptr && m_sculptDelta.size >= requiredSize;
    };

    if (checkIsResident())
    {
        if (m_sculptDelta.readOnly)
        {
            // make prviate
            SetBlobDataResident(true);
        }

        return true;
    }

    {
        auto readScope = GetReadScope();

        if (checkIsResident())
        {
            if (m_sculptDelta.readOnly)
            {
                SetBlobDataResident(true);
            }

            MarkDirty();

            return true;
        }
    }

    auto writeScope = GetWriteScope();

    if (checkIsResident())
    {
        // loaded by another thread
        MarkDirty();

        return true;
    }

    if (m_sculptDelta.raw != nullptr)
    {
        // Resident, but smaller than needed (e.g. cell size changed) - grow the buffer,
        // keeping the existing sculpted heights.
        ByteBuffer oldData(ConstByteView((const ubyte*)m_sculptDelta.raw, m_sculptDelta.size));
        const size_t oldSize = oldData.Size();

        FreeBlobData(m_sculptDelta);
        AllocateBlobData(m_sculptDelta, nullptr, requiredSize, alignof(float));

        if (m_sculptDelta.raw == nullptr || m_sculptDelta.size < requiredSize)
        {
            return false;
        }

        Memory::Copy(m_sculptDelta.raw, oldData.Data(), oldSize);
        Memory::Zero((ubyte*)m_sculptDelta.raw + oldSize, requiredSize - oldSize);

        MarkDirty();

        return true;
    }

    FreeBlobData(m_sculptDelta);
    AllocateBlobData(m_sculptDelta, nullptr, requiredSize, alignof(float));

    if (m_sculptDelta.raw == nullptr || m_sculptDelta.size < requiredSize)
    {
        return false;
    }

    Memory::Zero(m_sculptDelta.raw, requiredSize);

    MarkDirty();

    return true;
}

bool TerrainCellData::HasSplatMap() const
{
    return m_splatMap.size != 0;
}

void TerrainCellData::ClearSculptDelta()
{
    FreeBlobData(m_sculptDelta);

    m_sculptDelta = BlobDataReference {};

    MarkDirty();
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

    if (m_sculptDelta.raw == nullptr
        && m_sculptDelta.key
        && m_sculptDelta.size != 0)
    {
        if (!PageBlobDataFromStorage(m_sculptDelta))
        {
            PageBlobDataFromFile(blobDirectory, "TERA", m_sculptDelta);
        }
    }

    if (m_splatMap.raw == nullptr
        && m_splatMap.key
        && m_splatMap.size != 0)
    {
        if (!PageBlobDataFromStorage(m_splatMap))
        {
            PageBlobDataFromFile(blobDirectory, "TSM", m_splatMap);
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

    AssertBlobDataPersisted(m_sculptDelta);

    if (!m_sculptDelta.readOnly)
    {
        FreeBlobData(m_sculptDelta);
    }

    m_sculptDelta.raw = nullptr;

    AssertBlobDataPersisted(m_splatMap);

    if (!m_splatMap.readOnly)
    {
        FreeBlobData(m_splatMap);
    }

    m_splatMap.raw = nullptr;
}

#pragma endregion TerrainCellData

} // namespace Hyperion
