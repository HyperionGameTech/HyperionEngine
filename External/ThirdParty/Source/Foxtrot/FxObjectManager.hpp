#pragma once

#include <Core/FxBitset.hpp>
#include <Renderer/Backend/RxDescriptors.hpp>
#include <Renderer/Backend/RxGpuBuffer.hpp>

struct alignas(16) FxObjectGpuEntry
{
    float ModelMatrix[16];
};

class FxMat4f;

using FxObjectId = uint32;

class FxObjectManager
{
public:
    static constexpr uint32 scMaxObjects = 128;


public:
    void Create();

    FxObjectId GenerateObjectId();
    void Submit(FxObjectId id, FxObjectGpuEntry& entry);
    void Submit(FxObjectId id, const FxMat4f& model_matrix);
    void FreeObjectId(FxObjectId id);

    void PrintActive(int limit = 20);

    uint32 GetOffsetObjectIndex(uint32 object_id) const;
    uint32 GetBaseOffset() const;

    /**
     * @brief Finds an object slot with `num_instances` free slots following.
     *
     * @note This does not update the object's object id or set the reserved instances counter, use
     * FxObject::ReserveInstances() for that!
     *
     * @param object_id An object id of the current object if it needs to be moved.
     * @returns The object id for the first slot of the block.
     */
    FxObjectId ReserveInstances(FxObjectId object_id, uint32 num_instances);

    void Destroy();

    ~FxObjectManager() { Destroy(); }

private:
    FxObjectGpuEntry* GetBufferAtFrame(uint32 object_id);

public:
    RxDescriptorPool mDescriptorPool {};

public:
    RxRawGpuBuffer mObjectGpuBuffer {};
    FxBitset mObjectSlotsInUse;

    RxDescriptorSet mObjectBufferDS {};
    VkDescriptorSetLayout DsLayoutObjectBuffer = nullptr;

    std::mutex mInUse;
};
