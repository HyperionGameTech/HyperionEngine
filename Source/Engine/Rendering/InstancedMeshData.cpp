#include <RenderingPch.hpp>

#include <Rendering/InstancedMeshData.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Asset/AssetRegistry.hpp>
#include <Asset/Assets.hpp>
#include <Asset/BlobStorage.hpp>

#include <Framework/EngineGlobals.hpp>

#include <InstancedMeshData.generated.inl>

namespace Hyperion {

void InstancedMeshProxy_OnPostLoad(InstancedMeshData& instancedMesh)
{
    //// Ensure at least one instance
    //instancedMesh.numInstances = MathUtil::Max(instancedMesh.numInstances, 1u);

    //if (instancedMesh.buffers.Any())
    //{
    //    for (uint32 bufferIndex = 0; bufferIndex < MathUtil::Min(InstancedMeshData::MaxBuffers, uint32(instancedMesh.buffers.Size())); bufferIndex++)
    //    {
    //        if (instancedMesh.buffers[bufferIndex].Size() / instancedMesh.bufferStructSizes[bufferIndex] != instancedMesh.numInstances)
    //        {
    //            HYP_LOG(Rendering, Warning, "Expected mesh instance data to have a buffer size that is equal to (buffer struct size / number of instances). Buffer size: {}, Buffer struct size: {}, Num instances: {}",
    //                instancedMesh.buffers[bufferIndex].Size(), instancedMesh.bufferStructSizes[bufferIndex],
    //                instancedMesh.numInstances);
    //        }
    //    }

    //    if (instancedMesh.buffers.Size() > InstancedMeshData::MaxBuffers)
    //    {
    //        HYP_LOG(Rendering, Warning, "InstancedMeshData has more buffers than the maximum allowed: {} > {}", instancedMesh.buffers.Size(), InstancedMeshData::MaxBuffers);
    //    }
    //}
}

void InstancedMeshData::PageBlobData()
{
    Handle<AssetRegistry> registry = GetAssetRegistry();
    AssertDebug(registry.IsValid());

    if (!registry.IsValid())
    {
        return;
    }

    BlobStorage* blobStorage = registry->HasBlobStorage() ? &registry->GetBlobStorage() : nullptr;

    for (uint32 i = 0; i < uint32(buffers.Size()); i++)
    {
        BlobDataReference& ref = buffers[i];

        // For editor, we allow inline blobs, so we try to read from the file system first. If that fails, we fall back to the blob storage.

        if (ref.raw == nullptr && ref.key && ref.size != 0)
        {
            if (!blobStorage || !blobStorage->GetData(ref.key, ref.size, ref.raw))
            {
#if defined(HYP_EDITOR) || defined(HYP_ALLOW_INLINE_BLOBS)
                const Name blobKey = ref.key;
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
                    ref.key = blobKey;

                    continue;
                }
#endif
                HYP_LOG(Engine, Error, "Failed to page blob data for InstancedMeshData {}", GetName());
            }
            else
            {
                ref.readOnly = true;
            }
        }
    }
}

void InstancedMeshData::UnpageBlobData()
{
    for (BlobDataReference& ref : buffers)
    {
        if (ref.readOnly)
        {
            ref.raw = nullptr;
        }
    }
}

} // namespace Hyperion
