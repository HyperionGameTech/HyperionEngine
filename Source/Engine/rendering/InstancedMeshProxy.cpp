#include <RenderingPch.hpp>

#include <rendering/InstancedMeshProxy.hpp>

#include <Core/math/MathUtil.hpp>

#include <asset/AssetRegistry.hpp>
#include <asset/Assets.hpp>

#include <engine/EngineGlobals.hpp>

#include <InstancedMeshProxy.generated.inl>

namespace Hyperion {

#if HYP_EDITOR
HYP_DECLARE_LOG_CHANNEL(Editor);
#endif

void InstancedMeshProxy_OnPostLoad(InstancedMeshProxy& imp)
{
    //// Ensure at least one instance
    //imp.numInstances = MathUtil::Max(imp.numInstances, 1u);

    //if (imp.buffers.Any())
    //{
    //    for (uint32 bufferIndex = 0; bufferIndex < MathUtil::Min(InstancedMeshProxy::MaxBuffers, uint32(imp.buffers.Size())); bufferIndex++)
    //    {
    //        if (imp.buffers[bufferIndex].Size() / imp.bufferStructSizes[bufferIndex] != imp.numInstances)
    //        {
    //            HYP_LOG(Rendering, Warning, "Expected mesh instance data to have a buffer size that is equal to (buffer struct size / number of instances). Buffer size: {}, Buffer struct size: {}, Num instances: {}",
    //                imp.buffers[bufferIndex].Size(), imp.bufferStructSizes[bufferIndex],
    //                imp.numInstances);
    //        }
    //    }

    //    if (imp.buffers.Size() > InstancedMeshProxy::MaxBuffers)
    //    {
    //        HYP_LOG(Rendering, Warning, "InstancedMeshProxy has more buffers than the maximum allowed: {} > {}", imp.buffers.Size(), InstancedMeshProxy::MaxBuffers);
    //    }
    //}
}

void InstancedMeshProxy::PageBlobData()
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
#if HYP_EDITOR
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
    
#if HYP_EDITOR
    if (needSaveBlobData)
    {    
        BlobStorage& blobStorage = g_assetManager->GetAssetRegistry()->GetBlobStorage();

        Result saveBlobDataResult = SaveBlobData(blobStorage);

        if (saveBlobDataResult.HasError())
        {
            HYP_LOG(Editor, Error, "Failed to save local blob data for InstancedMeshData: {}", saveBlobDataResult.GetError().GetMessage());
        }
    }
#endif
}

void InstancedMeshProxy::UnpageBlobData()
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
