#include <HyperionPch.hpp>

#include <Asset/RawDataAsset.hpp>

#include <Asset/AssetRegistry.hpp>
#include <Asset/BlobStorage.hpp>

#include <Core/Logging/Logger.hpp>

#include <RawDataAsset.generated.inl>

namespace Hyperion {

RawDataAsset::~RawDataAsset()
{
    FreeBlobData(m_data);
}

void RawDataAsset::Init()
{
    AssetObject::Init();
}

void RawDataAsset::SetData(ConstByteView view)
{
    if (view.Size() == 0 && m_data.size == 0)
    {
        return;
    }

    FreeBlobData(m_data);

    if (view.Size() != 0)
    {
        AllocateBlobData(m_data, view.Data(), view.Size(), 1);
    }

    MarkDirty();
}

ConstByteView RawDataAsset::GetData() const
{
    if (m_data.raw == nullptr || m_data.size == 0)
    {
        return ConstByteView();
    }

    return ConstByteView(reinterpret_cast<const ubyte*>(m_data.raw), m_data.size);
}

void RawDataAsset::PageBlobData()
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

    BlobStorage* blobStorage = registry->HasBlobStorage() ? &registry->GetBlobStorage() : nullptr;

    if (m_data.raw == nullptr
        && m_data.key
        && m_data.size != 0)
    {
        if (!blobStorage || !blobStorage->GetData(m_data.key, m_data.size, m_data.raw))
        {
#if HYP_EDITOR || HYP_ALLOW_INLINE_BLOBS
            FileByteReader stream { registry->GetRootPath() / AssetBuckets::RawData.GetName() / (String(*GetName()) + ".RAW.raw.blob") };

            if (!stream.Eof())
            {
                ByteBuffer buffer = stream.Read(stream.Max());

                AllocateBlobData(m_data, buffer.Data(), buffer.Size(), 1);

#if HYP_EDITOR
                if (blobStorage != nullptr)
                {
                    Result saveResult = SaveBlobData(blobStorage);

                    if (saveResult.HasError())
                    {
                        HYP_LOG(Assets, Error, "Failed to save raw data blob: {}", saveResult.GetError().GetMessage());
                    }

                    MarkDirty();
                }
#endif

                return;
            }
#endif
        }
        else
        {
            m_data.readOnly = true;
        }
    }
}

void RawDataAsset::UnpageBlobData()
{
    if (m_data.readOnly)
    {
        m_data.raw = nullptr;
    }
}

} // namespace Hyperion
