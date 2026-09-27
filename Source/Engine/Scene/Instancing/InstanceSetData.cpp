/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/Instancing/InstanceSetData.hpp>

#include <Core/Memory/Memory.hpp>

#include <InstanceSetData.generated.inl>

namespace Hyperion {

InstanceSetData::InstanceSetData()
    : InstanceSetData(Name::Invalid())
{
}

InstanceSetData::InstanceSetData(Name name)
    : AssetObject(name),
      m_nextId(0)
{
}

InstanceSetData::~InstanceSetData()
{
    if (!m_records.readOnly)
    {
        FreeBlobData(m_records);
    }
}

void InstanceSetData::GetRecords(Array<InstanceRecord>& outRecords) const
{
    auto readScope = GetReadScope();

    outRecords.Clear();

    if (m_records.raw == nullptr || m_records.size == 0)
    {
        return;
    }

    AssertDebug(m_records.size % sizeof(InstanceRecord) == 0,
        "InstanceSetData '{}' blob is {} bytes, not a whole number of records", GetName(), m_records.size);

    const uint32 numRecords = uint32(m_records.size / sizeof(InstanceRecord));

    outRecords.Resize(numRecords);
    Memory::Copy(outRecords.Data(), m_records.raw, numRecords * sizeof(InstanceRecord));
}

void InstanceSetData::SetRecords(Span<const InstanceRecord> records, uint32 nextId)
{
    auto writeScope = GetWriteScope();

    if (!m_records.readOnly && m_records.raw != nullptr)
    {
        FreeBlobData(m_records);
    }

    m_records = {};

    if (records.Size() != 0)
    {
        AllocateBlobData(m_records, records.Data(), records.Size() * sizeof(InstanceRecord), alignof(InstanceRecord));
    }

    m_nextId = nextId;

    MarkDirty();
}

Handle<AssetObject> InstanceSetData::CloneAsset() const
{
    Array<InstanceRecord> records;
    GetRecords(records);

    Handle<InstanceSetData> clone = MakeHandle<InstanceSetData>(GetName());
    clone->SetRecords(Span<const InstanceRecord>(records.Data(), records.Size()), m_nextId);

    return clone;
}

void InstanceSetData::PageBlobData()
{
    if (IsTransient() || !IsRegistered())
    {
        return;
    }

    if (m_records.raw != nullptr || !m_records.key || m_records.size == 0)
    {
        return;
    }

    if (!PageBlobDataFromStorage(m_records))
    {
        (void)PageBlobDataFromLocalFile(m_records, RecordsBlobMagic, alignof(InstanceRecord));
    }
}

void InstanceSetData::UnpageBlobData()
{
    AssetObject::UnpageBlobData();

    AssertBlobDataPersisted(m_records);

    if (!m_records.readOnly)
    {
        FreeBlobData(m_records);
    }

    m_records.raw = nullptr;
}

} // namespace Hyperion
