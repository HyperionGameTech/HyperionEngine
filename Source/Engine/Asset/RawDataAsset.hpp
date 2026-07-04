#pragma once

#include <Asset/AssetObject.hpp>

namespace Hyperion {

HYP_CLASS(AssetBucket = "RawData")
class ENGINE_API RawDataAsset : public AssetObject
{
    HYP_OBJECT_BODY(RawDataAsset);

public:
    RawDataAsset()
        : AssetObject()
    {
    }

    explicit RawDataAsset(Name name)
        : AssetObject(name)
    {
    }

    RawDataAsset(Name name, ConstByteView data)
        : AssetObject(name)
    {
        SetData(data);
    }

    RawDataAsset(const RawDataAsset& other) = delete;
    RawDataAsset& operator=(const RawDataAsset& other) = delete;

    RawDataAsset(RawDataAsset&& other) noexcept = delete;
    RawDataAsset& operator=(RawDataAsset&& other) noexcept = delete;

    ~RawDataAsset();

    void SetData(ConstByteView view);
    ConstByteView GetData() const;

protected:
    void Init() override;

    void PageBlobData() override;
    void UnpageBlobData() override;

    void CollectBlobDataReferences(Array<Tuple<const char*, uint16, BlobDataReference*>>& outReferences) override
    {
        outReferences.EmplaceBack("RAW", 1, &m_data);
    }

private:
    HYP_FIELD(Property = "Data", Serialize)
    BlobDataReference m_data;
};

} // namespace Hyperion
