#pragma once

#include "RxDevice.hpp"
#include "RxGpuBuffer.hpp"
#include "RxPipeline.hpp"
#include "RxSampler.hpp"

#include <vulkan/vulkan.h>

#include <Core/FxPanic.hpp>
#include <Core/FxSizedArray.hpp>
#include <Renderer/RxConstants.hpp>

#include "vulkan/vulkan_core.h"

struct RxTarget;

class RxDescriptorPool
{
public:
    void Create(RxGpuDevice* device, uint32 max_sets = 10);

    bool IsInited() const { return (Pool != nullptr); }
    FX_FORCE_INLINE VkDescriptorPool Get() const { return Pool; }
    void AddPoolSize(VkDescriptorType type, uint32_t count) { RemainingDescriptorCounts[type] = count; }

    void Destroy();
    ~RxDescriptorPool() { Destroy(); }

public:
    VkDescriptorPool Pool = nullptr;

    uint16 SetCapacity = 0;
    uint16 SetsUsed = 0;

    std::unordered_map<VkDescriptorType, uint32> RemainingDescriptorCounts;

private:
    friend class RxDescriptorSet;
};

enum class RxDescriptorBufferType
{
    eStorage,
    eUniform,
};


class RxDescriptorSet
{
    struct DescriptorEntry
    {
        uint32 BindIndex = 0;
        RxImage* pImage = nullptr;
        RxSampler* pSampler = nullptr;
        RxRawGpuBuffer* pBuffer = nullptr;

        uint64 BufferOffset = 0;
        uint64 BufferRange = 0;
    };


    static constexpr uint32 scMaxBuffers = 2;
    static constexpr uint32 scMaxImages = 6;

    static constexpr uint32 scMaxDescriptorEntries = scMaxBuffers + scMaxImages;

public:
    void Create(RxDescriptorPool& pool, VkDescriptorSetLayout layout, bool has_dynamic_offsets, uint32 count = 1);
    bool IsInited() const { return Set != nullptr; }

    static void BindMultiple(uint32 first_set_index, const RxCommandBuffer& cmd, VkPipelineBindPoint bind_point,
                             const RxPipeline& pipeline, VkDescriptorSet* sets, uint32 sets_count);

    static void BindMultiple(uint32 first_set_index, const RxCommandBuffer& cmd, VkPipelineBindPoint bind_point,
                             const RxPipeline& pipeline, const FxSlice<VkDescriptorSet>& sets);

    static void BindMultipleOffset(uint32 first_set_index, const RxCommandBuffer& cmd, VkPipelineBindPoint bind_point,
                                   const RxPipeline& pipeline, const FxSlice<VkDescriptorSet>& sets,
                                   const FxSlice<uint32>& offsets);

    void BindWithOffset(uint32 first_set_index, const RxCommandBuffer& cmd, VkPipelineBindPoint bind_point,
                        const RxPipeline& pipeline, uint32 offset) const;

    void Bind(uint32 first_set_index, const RxCommandBuffer& cmd, VkPipelineBindPoint bind_point,
              const RxPipeline& pipeline) const;

    void AddBuffer(uint32 bind_index, RxRawGpuBuffer* buffer, uint64 offset, uint64 range);
    void AddImage(uint32 bind_index, RxImage* image, RxSampler* sampler);
    void AddImageFromTarget(uint32 bind_index, RxTarget* target, RxSampler* sampler);

    void Build();

    VkDescriptorSet Get()
    {
        if (!mbIsBuilt) {
            Build();
        }

        return Set;
    }

    bool HasDynamicOffsets() const { return mbHasDynamicOffsets; }

    VkDescriptorSetLayout GetLayout() { return Layout; }
    void DestroyLayout();

    bool operator!() const { return Set == nullptr; }

    void Destroy();

    ~RxDescriptorSet() { Destroy(); }

private:
    VkDescriptorSet Set = nullptr;
    VkDescriptorSetLayout Layout = nullptr;

    bool mbHasDynamicOffsets : 1 = false;
    bool mbIsBuilt : 1 = false;

    FxSizedArray<DescriptorEntry> mDescriptorEntries;
};
