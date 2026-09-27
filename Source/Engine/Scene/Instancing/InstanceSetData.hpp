/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Containers/Array.hpp>
#include <Core/Utilities/Span.hpp>

#include <Asset/AssetObject.hpp>

#include <Scene/Instancing/InstanceTypes.hpp>

namespace Hyperion {

HYP_CLASS(AssetBucket = "InstanceData")
class ENGINE_API InstanceSetData final : public AssetObject
{
    HYP_OBJECT_BODY(InstanceSetData);

public:
    static constexpr const char* RecordsBlobMagic = "INST";

    InstanceSetData();
    explicit InstanceSetData(Name name);

    InstanceSetData(const InstanceSetData&) = delete;
    InstanceSetData& operator=(const InstanceSetData&) = delete;

    ~InstanceSetData() override;

    HYP_FORCE_INLINE uint32 GetNextId() const
    {
        return m_nextId;
    }

    void GetRecords(Array<InstanceRecord>& outRecords) const;
    void SetRecords(Span<const InstanceRecord> records, uint32 nextId);

    virtual Handle<AssetObject> CloneAsset() const override;

    void CollectBlobDataReferences(Array<Tuple<const char*, uint16, BlobDataReference*>>& outReferences) override
    {
        if (m_records.size != 0)
        {
            outReferences.EmplaceBack(RecordsBlobMagic, 1, &m_records);
        }
    }

protected:
    void PageBlobData() override;
    void UnpageBlobData() override;

private:
    HYP_FIELD(Property = "Records", Serialize, Editor = false)
    BlobDataReference m_records;

    HYP_FIELD(Property = "NextId", Serialize, Editor = false)
    uint32 m_nextId;
};

} // namespace Hyperion
