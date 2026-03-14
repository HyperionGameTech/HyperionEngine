#include <RenderingPch.hpp>

#include <rendering/InstancedMeshData.hpp>

#include <Core/math/MathUtil.hpp>

#include <asset/AssetRegistry.hpp>
#include <asset/Assets.hpp>
#include <asset/BlobStorage.hpp>

#include <engine/EngineGlobals.hpp>

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
    bool needSaveBlobData = false;

    for (uint32 i = 0; i < uint32(buffers.Size()); i++)
    {
        BlobDataReference& ref = buffers[i];

        if (ref.raw == nullptr && ref.key && ref.size != 0)
        {
            BlobStorage& blobStorage = g_assetManager->GetAssetRegistry()->GetBlobStorage();

            if (!blobStorage.GetData(ref.key, ref.size, ref.raw))
            {
#if HYP_EDITOR || HYP_ALLOW_INLINE_BLOBS
                Handle<AssetPackage> package = GetPackage();
                Assert(package.IsValid());
                Assert(package->IsSaved());

                FileByteReader stream { package->GetSavedDirectory() / (String(*GetName()) + "." + BufferNames[i] + ".raw.blob") };
                if (!stream.Eof())
                {
                    ByteBuffer buffer = stream.Read(stream.Max());

                    AllocateBlobData(ref, buffer.Data(), buffer.Size(), 1);

                    needSaveBlobData = true;

                    continue;
                }
#endif
                HYP_FAIL("Failed to page blob data for InstancedMeshData {}", GetName());
            }
            else
            {
                ref.readOnly = true;
            }
        }
    }
    
#if HYP_EDITOR || HYP_ALLOW_INLINE_BLOBS
    if (needSaveBlobData)
    {    
        BlobStorage& blobStorage = g_assetManager->GetAssetRegistry()->GetBlobStorage();

        Result saveBlobDataResult = SaveBlobData(blobStorage);

        if (saveBlobDataResult.HasError())
        {
            HYP_LOG(Assets, Error, "Failed to save local blob data for InstancedMeshData: {}", saveBlobDataResult.GetError().GetMessage());
        }
    }
#endif
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
