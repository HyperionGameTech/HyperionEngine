#include "FxObjectManager.hpp"

#include <FxEngine.hpp>
#include <FxObject.hpp>
#include <Math/FxMat4.hpp>
#include <Renderer/Backend/RxDsLayoutBuilder.hpp>
#include <Renderer/RxGlobals.hpp>
#include <Renderer/RxRenderBackend.hpp>

void FxObjectManager::Create()
{
    if (!mDescriptorPool.Pool) {
        mDescriptorPool.AddPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1);
        mDescriptorPool.AddPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1);
        mDescriptorPool.Create(gRenderer->GetDevice(), 2);
    }

    RxDsLayoutBuilder builder {};
    builder.AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, RxShaderType::eVertex);
    DsLayoutObjectBuffer = builder.Build();

    uint32 buffer_size = (sizeof(FxObjectGpuEntry) * scMaxObjects) * RxFramesInFlight;

    mObjectGpuBuffer.Create(RxGpuBufferType::eStorageWithOffset, buffer_size, VMA_MEMORY_USAGE_CPU_ONLY,
                            RxGpuBufferFlags::ePersistentMapped);


    mObjectSlotsInUse.InitZero(scMaxObjects);

    if (!mObjectBufferDS.IsInited()) {
        FxAssert(DsLayoutObjectBuffer != nullptr);
        mObjectBufferDS.Create(mDescriptorPool, DsLayoutObjectBuffer, true);
    }

    static constexpr uint32 bound_size = scMaxObjects * sizeof(FxObjectGpuEntry);
    mObjectBufferDS.AddBuffer(0, &mObjectGpuBuffer, 0, bound_size);
    mObjectBufferDS.Build();
}


uint32 FxObjectManager::GetBaseOffset() const
{
    return (gRenderer->GetFrameNumber() * scMaxObjects * sizeof(FxObjectGpuEntry));
}

uint32 FxObjectManager::GetOffsetObjectIndex(uint32 object_id) const
{
    return GetBaseOffset() + (object_id * sizeof(FxObjectGpuEntry));
}

FxObjectGpuEntry* FxObjectManager::GetBufferAtFrame(uint32 object_id)
{
    uint8* entry_buffer = reinterpret_cast<uint8*>(mObjectGpuBuffer.pMappedBuffer);

    return reinterpret_cast<FxObjectGpuEntry*>(entry_buffer + GetOffsetObjectIndex(object_id));
}


FxObjectId FxObjectManager::GenerateObjectId()
{
    std::lock_guard<std::mutex> guard(mInUse);

    FxObjectId free_object_id = mObjectSlotsInUse.FindNextFreeBit();
    mObjectSlotsInUse.Set(free_object_id);

    return free_object_id;
}

void FxObjectManager::Submit(FxObjectId object_id, FxObjectGpuEntry& entry)
{
    FxAssert(object_id < scMaxObjects);

    memcpy(GetBufferAtFrame(object_id), &entry, sizeof(FxObjectGpuEntry));

    // mObjectGpuBuffer.FlushToGpu(GetOffsetObjectIndex(object_id), sizeof(FxObjectGpuEntry));
}

void FxObjectManager::Submit(FxObjectId object_id, const FxMat4f& model_matrix)
{
    static_assert(offsetof(FxObjectGpuEntry, ModelMatrix) == 0);

    FxAssert(object_id < scMaxObjects);

    memcpy(GetBufferAtFrame(object_id), model_matrix.RawData, sizeof(FxMat4f));

    // mObjectGpuBuffer.FlushToGpu(GetOffsetObjectIndex(object_id), sizeof(FxObjectGpuEntry));
}


void FxObjectManager::FreeObjectId(FxObjectId id)
{
    if (id == UINT32_MAX) {
        return;
    }

    std::lock_guard<std::mutex> guard(mInUse);

    FxLogInfo("Freeing object {} from object manager", id);

    mObjectSlotsInUse.Unset(id);
}

void FxObjectManager::PrintActive(int limit)
{
    FxObjectGpuEntry* buffer = reinterpret_cast<FxObjectGpuEntry*>(mObjectGpuBuffer.pMappedBuffer);

    for (int i = 0; i < limit; i++) {
        if (mObjectSlotsInUse.Get(i)) {
            FxLogInfo("Object [{}]", i);

            float* model1 = buffer[i].ModelMatrix;

            for (int j = 0; j < 4; j++) {
                FxLogInfo("[{}, {}, {}, {}]", model1[j * 4 + 0], model1[j * 4 + 1], model1[j * 4 + 2],
                          model1[j * 4 + 3]);
            }
        }
    }
}

FxObjectId FxObjectManager::ReserveInstances(FxObjectId object_id, uint32 num_instances)
{
    // Unset the current object to determine if the current span of slots can contain the instances
    mObjectSlotsInUse.Unset(object_id);

    // Find the new bit group (+1 for the current object)
    uint32 start_index = mObjectSlotsInUse.FindNextFreeBitGroup(num_instances + 1);
    FxAssert(start_index != FxBit::scBitNotFound);

    // There is not enough room after our current object, move it
    if (start_index != object_id) {
        Submit(start_index, *GetBufferAtFrame(object_id));

        // The object id is already 'freed', so we do not need to call FreeObjectId.
    }

    // Mark the following bits as 'in use' to prevent other future objects from snatching them.
    for (uint32 i = start_index; i < start_index + num_instances; i++) {
        mObjectSlotsInUse.Set(i);
    }

    // Return the new slot block
    return start_index;
}


void FxObjectManager::Destroy()
{
    mObjectBufferDS.Destroy();
    mDescriptorPool.Destroy();
    mObjectGpuBuffer.Destroy();

    if (DsLayoutObjectBuffer) {
        vkDestroyDescriptorSetLayout(gRenderer->GetDevice()->Device, DsLayoutObjectBuffer, nullptr);
        DsLayoutObjectBuffer = nullptr;
    }
}
