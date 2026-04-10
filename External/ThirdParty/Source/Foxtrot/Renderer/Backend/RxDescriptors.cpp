#include "RxDescriptors.hpp"

#include <Renderer/RxAttachment.hpp>
#include <Renderer/RxGlobals.hpp>
#include <Renderer/RxRenderBackend.hpp>

/////////////////////////////////////
// Descriptor Pool Functions
/////////////////////////////////////

void RxDescriptorPool::Create(RxGpuDevice* device, uint32 max_sets)
{
    const uint32 pool_sizes_count = RemainingDescriptorCounts.size();
    FxSizedArray<VkDescriptorPoolSize> pool_sizes(pool_sizes_count);

    for (const auto& desc_count : RemainingDescriptorCounts) {
        pool_sizes.Insert({ .type = desc_count.first, .descriptorCount = desc_count.second });
    }


    VkDescriptorPoolCreateInfo pool_info {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.maxSets = max_sets;
    pool_info.poolSizeCount = pool_sizes.Size;
    pool_info.pPoolSizes = pool_sizes.pData;

    SetCapacity = max_sets;

    VkResult status = vkCreateDescriptorPool(device->Device, &pool_info, nullptr, &Pool);

    if (status != VK_SUCCESS) {
        FxPanicVulkan("DescriptorPool", "Failed to create descriptor pool!", status);
    }
}

void RxDescriptorPool::Destroy()
{
    if (!Pool) {
        return;
    }

    vkDestroyDescriptorPool(gRenderer->GetDevice()->Device, Pool, nullptr);
    Pool = nullptr;
}

/////////////////////////////////////
// Descriptor Sets
/////////////////////////////////////

void RxDescriptorSet::Create(RxDescriptorPool& pool, VkDescriptorSetLayout layout, bool has_dynamic_offsets,
                             uint32 count)
{
    FxAssertMsg(pool.IsInited(), "Descriptor pool is not initialized!");

    Layout = layout;
    mbHasDynamicOffsets = has_dynamic_offsets;

    VkDescriptorSetAllocateInfo alloc_info {};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = pool.Get();
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &layout;

    pool.SetsUsed++;

    VkResult status = vkAllocateDescriptorSets(gRenderer->GetDevice()->Device, &alloc_info, &Set);

    if (status != VK_SUCCESS) {
        FxLogError("Pool has {} allocated sets, with {} currently in use.", pool.SetCapacity, pool.SetsUsed);
        FxPanicVulkan("DescriptorSet", "Failed to allocate descriptor set!", status);
    }
}

void RxDescriptorSet::BindMultiple(uint32 first_set_index, const RxCommandBuffer& cmd, VkPipelineBindPoint bind_point,
                                   const RxPipeline& pipeline, VkDescriptorSet* sets, uint32 sets_count)
{
    vkCmdBindDescriptorSets(cmd, bind_point, pipeline.Layout, first_set_index, sets_count, sets, 0, nullptr);
}

void RxDescriptorSet::BindMultiple(uint32 first_set_index, const RxCommandBuffer& cmd, VkPipelineBindPoint bind_point,
                                   const RxPipeline& pipeline, const FxSlice<VkDescriptorSet>& sets)
{
    vkCmdBindDescriptorSets(cmd, bind_point, pipeline.Layout, first_set_index, sets.Size, sets.pData, 0, nullptr);
}

void RxDescriptorSet::BindMultipleOffset(uint32 first_set_index, const RxCommandBuffer& cmd,
                                         VkPipelineBindPoint bind_point, const RxPipeline& pipeline,
                                         const FxSlice<VkDescriptorSet>& sets, const FxSlice<uint32>& offsets)
{
    vkCmdBindDescriptorSets(cmd, bind_point, pipeline.Layout, first_set_index, sets.Size, sets.pData, offsets.Size,
                            offsets.pData);
}

void RxDescriptorSet::BindWithOffset(uint32 first_set_index, const RxCommandBuffer& cmd, VkPipelineBindPoint bind_point,
                                     const RxPipeline& pipeline, uint32 offset) const
{
    vkCmdBindDescriptorSets(cmd, bind_point, pipeline.Layout, first_set_index, 1, &Set, 1, &offset);
}

void RxDescriptorSet::Bind(uint32 first_set_index, const RxCommandBuffer& cmd, VkPipelineBindPoint bind_point,
                           const RxPipeline& pipeline) const
{
    vkCmdBindDescriptorSets(cmd, bind_point, pipeline.Layout, first_set_index, 1, &Set, 0, nullptr);
}


void RxDescriptorSet::AddBuffer(uint32 bind_index, RxRawGpuBuffer* buffer, uint64 offset, uint64 range)
{
    if (!mDescriptorEntries.IsInited()) {
        mDescriptorEntries.InitCapacity(scMaxDescriptorEntries);
    }

    FxAssertMsg(buffer != nullptr, "Input buffer cannot be null!");

    DescriptorEntry input_buffer {
        .BindIndex = bind_index,
        .pImage = nullptr,
        .pSampler = nullptr,
        .pBuffer = buffer,
        .BufferOffset = offset,
        .BufferRange = range,
    };

    mDescriptorEntries.Insert(input_buffer);

    mbIsBuilt = false;
}

void RxDescriptorSet::AddImageFromTarget(uint32 bind_index, RxTarget* target, RxSampler* sampler)
{
    FxAssertMsg(target != nullptr, "Input target cannot be null!");
    AddImage(bind_index, &target->Image, sampler);
}

void RxDescriptorSet::AddImage(uint32 bind_index, RxImage* image, RxSampler* sampler)
{
    if (!mDescriptorEntries.IsInited()) {
        mDescriptorEntries.InitCapacity(scMaxDescriptorEntries);
    }

    FxAssertMsg(image != nullptr, "Input image cannot be null!");

    DescriptorEntry input_target {
        .BindIndex = bind_index,
        .pImage = image,
        .pSampler = sampler,
        .pBuffer = nullptr,
    };

    mDescriptorEntries.Insert(input_target);

    mbIsBuilt = false;
}

void RxDescriptorSet::Build()
{
    FxAssert(mbIsBuilt == false);
    FxAssertMsg(mDescriptorEntries.IsNotEmpty(), "Descriptor set is missing entries!");

    FxStackArray<VkDescriptorImageInfo, scMaxImages> image_infos;
    FxStackArray<VkDescriptorBufferInfo, scMaxBuffers> buffer_infos;
    FxStackArray<VkWriteDescriptorSet, scMaxDescriptorEntries> write_infos;

    for (const DescriptorEntry& entry : mDescriptorEntries) {
        if (entry.pImage) {
            VkDescriptorImageInfo image_info {
                .sampler = entry.pSampler->Sampler,
                .imageView = entry.pImage->View,
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            };

            const VkWriteDescriptorSet image_write {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = Set,
                .dstBinding = entry.BindIndex,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = image_infos.Insert(image_info),
            };

            write_infos.Insert(image_write);
        }
        else if (entry.pBuffer) {
            const VkDescriptorBufferInfo buffer_info {
                .buffer = entry.pBuffer->Buffer,
                .offset = entry.BufferOffset,
                .range = entry.BufferRange,
            };

            const VkWriteDescriptorSet buffer_write {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = Set,
                .dstBinding = entry.BindIndex,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = RxGpuBufferUtil::BufferTypeToDescriptorType(entry.pBuffer->Type),
                .pImageInfo = nullptr,
                .pBufferInfo = buffer_infos.Insert(buffer_info),
            };

            write_infos.Insert(buffer_write);
        }
    }

    vkUpdateDescriptorSets(gRenderer->GetDevice()->Device, write_infos.Size, write_infos.pData, 0, nullptr);

    mbIsBuilt = true;

    mDescriptorEntries.Free();
}

void RxDescriptorSet::DestroyLayout()
{
    if (Layout == nullptr) {
        return;
    }
    vkDestroyDescriptorSetLayout(gRenderer->GetDevice()->Device, Layout, nullptr);
    Layout = nullptr;
}

void RxDescriptorSet::Destroy()
{
    // if (Layout != nullptr) {
    //     vkDestroyDescriptorSetLayout(gRenderer->GetDevice()->Device, Layout, nullptr);
    //     Layout = nullptr;
    // }
}
