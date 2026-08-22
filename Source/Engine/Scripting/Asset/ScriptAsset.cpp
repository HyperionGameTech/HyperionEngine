/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <HyperionPch.hpp>

#include <Scripting/Asset/ScriptAsset.hpp>

#include <Asset/AssetRegistry.hpp>
#include <Asset/BlobStorage.hpp>

#include <Framework/EngineGlobals.hpp>

#include <Core/Logging/Logger.hpp>

#include <ScriptAsset.generated.inl>

namespace Hyperion {

ScriptAsset::~ScriptAsset()
{
    FreeBlobData(m_data);
}

void ScriptAsset::Init()
{
    AssetObject::Init();
}

void ScriptAsset::SetBytecode(ConstByteView view)
{
    if (view.Size() == 0 && m_data.size == 0)
    {
        // no change
        return;
    }

    FreeBlobData(m_data);

    if (view.Size() != 0)
    {
        AllocateBlobData(m_data, view.Data(), view.Size(), 1);
    }

    MarkDirty();
}

ConstByteView ScriptAsset::GetBytecode() const
{
    if (m_data.raw == nullptr || m_data.size == 0)
    {
        return ConstByteView();
    }

    return ConstByteView(reinterpret_cast<const ubyte*>(m_data.raw), m_data.size);
}

void ScriptAsset::PageBlobData()
{
    if (IsTransient() || !IsRegistered())
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

        Handle<AssetRegistry> registry = GetAssetRegistry();
        AssertDebug(registry.IsValid());

        if (registry.IsValid())
        {
            const Name blobKey = m_data.key;
            const uint64 expectedSize = m_data.size;

            FileByteReader stream { registry->GetRootPath() / AssetBuckets::Scripts.GetName() / (String(*GetName()) + ".BC.raw.blob") };

            if (!stream.Eof())
            {
                if (stream.Max() != expectedSize)
                {
                    HYP_LOG(Assets, Error, "Local blob data for script asset '{}' is {} bytes but the manifest expects {}, ignoring it",
                            GetName(), stream.Max(), expectedSize);

                    return;
                }

                ByteBuffer buffer = stream.Read(stream.Max());

                AllocateBlobData(m_data, buffer.Data(), buffer.Size(), 1);
                m_data.key = blobKey;

                return;
            }
        }

        m_data.readOnly = true;
    }
}

void ScriptAsset::UnpageBlobData()
{
    if (!m_data.readOnly)
    {
        FreeBlobData(m_data);
    }

    m_data.raw = nullptr;
}

} // namespace Hyperion
