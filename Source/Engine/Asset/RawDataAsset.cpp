#include <HyperionPch.hpp>

#include <Asset/RawDataAsset.hpp>

#include <Asset/AssetRegistry.hpp>
#include <Asset/BlobStorage.hpp>

#include <Framework/EngineGlobals.hpp>

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

    if (m_data.raw == nullptr
        && m_data.key
        && m_data.size != 0)
    {
        if (PageBlobDataFromStorage(m_data))
        {
            return;
        }

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

        m_data.readOnly = true;
    }
}

void RawDataAsset::UnpageBlobData()
{
    AssetObject::UnpageBlobData();
    
    AssertBlobDataPersisted(m_data);

    if (!m_data.readOnly)
    {
        FreeBlobData(m_data);
    }
    
    m_data.raw = nullptr;
}

} // namespace Hyperion
