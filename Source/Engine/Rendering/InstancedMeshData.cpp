#include <RenderingPch.hpp>

#include <Rendering/InstancedMeshData.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Asset/AssetRegistry.hpp>
#include <Asset/Assets.hpp>
#include <Asset/BlobStorage.hpp>

#include <Framework/EngineGlobals.hpp>

#include <InstancedMeshData.generated.inl>

namespace Hyperion {

void InstancedMeshData::PageBlobData()
{
    for (uint32 i = 0; i < uint32(buffers.Size()); i++)
    {
        BlobDataReference& ref = buffers[i];

        // For editor, we allow inline blobs, so we try to read from the file system first. If that fails, we fall back to the blob storage.

        if (ref.raw == nullptr && ref.key && ref.size != 0)
        {
            if (PageBlobDataFromStorage(ref))
            {
                continue;
            }

            Handle<AssetRegistry> registry = GetAssetRegistry();
            AssertDebug(registry.IsValid());

            if (registry.IsValid())
            {
                const uint64 expectedSize = ref.size;

                FileByteReader stream { registry->GetRootPath() / AssetBuckets::InstancedMeshData.GetName() / (String(*GetName()) + "." + BufferNames[i] + ".raw.blob") };
                if (!stream.Eof())
                {
                    if (stream.Max() != expectedSize)
                    {
                        HYP_LOG(Engine, Error, "Local blob data for InstancedMeshData '{}' buffer {} is {} bytes but the manifest expects {}. Data corruption detected.",
                                GetName(), BufferNames[i], stream.Max(), expectedSize);

                        continue;
                    }

                    ByteBuffer buffer = stream.Read(stream.Max());

                    AllocateBlobData(ref, buffer.Data(), buffer.Size(), 1);

                    continue;
                }
            }

            HYP_LOG(Engine, Error, "Failed to page blob data for InstancedMeshData {}", GetName());

            ref.readOnly = true;
        }
    }
}

void InstancedMeshData::UnpageBlobData()
{
    AssetObject::UnpageBlobData();
    
    for (BlobDataReference& ref : buffers)
    {
        AssertBlobDataPersisted(ref);

        if (!ref.readOnly)
        {
            FreeBlobData(ref);
        }

        ref.raw = nullptr;
    }
}

} // namespace Hyperion
