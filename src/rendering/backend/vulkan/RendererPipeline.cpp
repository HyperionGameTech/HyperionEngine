#include <rendering/backend/RendererPipeline.hpp>
#include <core/lib/FlatSet.hpp>
#include <core/lib/CMemory.hpp>

#include <system/Debug.hpp>

namespace hyperion {
namespace renderer {
namespace platform {

Pipeline<Platform::VULKAN>::Pipeline()
    : m_has_custom_descriptor_sets(false),
      pipeline(VK_NULL_HANDLE),
      layout(VK_NULL_HANDLE),
      push_constants { }
{
}

Pipeline<Platform::VULKAN>::Pipeline(const Array<DescriptorSetRef> &used_descriptor_sets)
    : m_used_descriptor_sets(used_descriptor_sets),
      m_has_custom_descriptor_sets(true),
      pipeline(VK_NULL_HANDLE),
      layout(VK_NULL_HANDLE),
      push_constants { }
{
}

Pipeline<Platform::VULKAN>::~Pipeline()
{
    AssertThrowMsg(pipeline == VK_NULL_HANDLE, "Expected pipeline to have been destroyed");
    AssertThrowMsg(layout == VK_NULL_HANDLE, "Expected layout to have been destroyed");
}

void Pipeline<Platform::VULKAN>::AssignDefaultDescriptorSets(DescriptorPool *descriptor_pool)
{
    m_has_custom_descriptor_sets = false;

#if HYP_FEATURES_BINDLESS_TEXTURES
    m_used_descriptor_sets.Set({
        descriptor_pool->GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_UNUSED),
        descriptor_pool->GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL),
        descriptor_pool->GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE),
        descriptor_pool->GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_VOXELIZER),
        descriptor_pool->GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_OBJECT),
        descriptor_pool->GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_BINDLESS),
        descriptor_pool->GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_RAYTRACING)
    });
#else
    m_used_descriptor_sets.Set({
        descriptor_pool->GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_UNUSED),
        descriptor_pool->GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL),
        descriptor_pool->GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE),
        descriptor_pool->GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_VOXELIZER),
        descriptor_pool->GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_OBJECT),
        descriptor_pool->GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES)
    });
#endif
}

std::vector<VkDescriptorSetLayout> Pipeline<Platform::VULKAN>::GetDescriptorSetLayouts(Device<Platform::VULKAN> *device, DescriptorPool *descriptor_pool)
{
    // TODO: set specialization constants based on these values so the indices all match up

    if (m_used_descriptor_sets.HasValue()) {
        m_has_custom_descriptor_sets = true;
    } else {
        AssignDefaultDescriptorSets(descriptor_pool);
    }
    
    std::vector<VkDescriptorSetLayout> used_layouts;
    used_layouts.reserve(m_used_descriptor_sets.Get().Size());

    for (DescriptorSetRef descriptor_set : m_used_descriptor_sets.Get()) {
        AssertThrow(descriptor_set != nullptr);
        AssertThrow(descriptor_set->IsCreated());

        //if (descriptor_set->GetIndex() == DescriptorSet::DESCRIPTOR_SET_INDEX_UNUSED) {
       //     continue;
        //}

        used_layouts.push_back(descriptor_set->m_layout);
    }

    return used_layouts;
}


void Pipeline<Platform::VULKAN>::SetPushConstants(const void *data, SizeType size)
{
    AssertThrow(size <= sizeof(push_constants));

    Memory::MemCpy(&push_constants, data, size);

    if (size < sizeof(push_constants)) {
        Memory::MemSet(&push_constants + size, 0, sizeof(push_constants) - size);
    }
}

} // namespace platform
} // namespace renderer
} // namespace hyperion
