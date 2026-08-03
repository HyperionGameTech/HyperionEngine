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

    BlobStorage* blobStorage = EngineGlobals::GetBlobStorage();

    if (m_data.raw == nullptr
        && m_data.key
        && m_data.size != 0)
    {
        if (!blobStorage || !blobStorage->GetData(m_data.key, m_data.size, m_data.raw))
        {
#if defined(HYP_EDITOR) || defined(HYP_ALLOW_INLINE_BLOBS)
            const Name blobKey = m_data.key;
            const uint64 expectedSize = m_data.size;

            FileByteReader stream { registry->GetRootPath() / AssetBuckets::RawData.GetName() / (String(*GetName()) + ".RAW.raw.blob") };

            if (!stream.Eof())
            {
                if (stream.Max() != expectedSize)
                {
                    HYP_LOG(Assets, Error, "Local blob data for raw data asset '{}' is {} bytes but the manifest expects {}, ignoring it",
                            GetName(), stream.Max(), expectedSize);

                    return;
                }

                ByteBuffer buffer = stream.Read(stream.Max());

                AllocateBlobData(m_data, buffer.Data(), buffer.Size(), 1);
                m_data.key = blobKey;

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
