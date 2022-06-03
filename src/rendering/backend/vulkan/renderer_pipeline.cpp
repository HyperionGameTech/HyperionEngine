#include "renderer_pipeline.h"
#include "renderer_descriptor_set.h"

#include <system/debug.h>

namespace hyperion {
namespace renderer {

Pipeline::Pipeline()
    : pipeline{},
      layout{},
      push_constants{}
{
}

Pipeline::~Pipeline()
{
    AssertThrowMsg(pipeline == nullptr, "Expected pipeline to have been destroyed");
    AssertThrowMsg(layout == nullptr, "Expected layout to have been destroyed");
}

std::vector<VkDescriptorSetLayout> Pipeline::GetDescriptorSetLayouts(Device *device, DescriptorPool *descriptor_pool) const
{
    const auto &pool_layouts = descriptor_pool->GetDescriptorSetLayouts();

#if HYP_FEATURES_BINDLESS_TEXTURES
    std::vector<VkDescriptorSetLayout> used_layouts{ // tmp
        pool_layouts.At(DescriptorSet::Index::DESCRIPTOR_SET_INDEX_UNUSED),
        pool_layouts.At(DescriptorSet::Index::DESCRIPTOR_SET_INDEX_GLOBAL),
        pool_layouts.At(DescriptorSet::Index::DESCRIPTOR_SET_INDEX_SCENE),
        pool_layouts.At(DescriptorSet::Index::DESCRIPTOR_SET_INDEX_OBJECT),
        pool_layouts.At(DescriptorSet::Index::DESCRIPTOR_SET_INDEX_SCENE_FRAME_1),
        pool_layouts.At(DescriptorSet::Index::DESCRIPTOR_SET_INDEX_OBJECT_FRAME_1),
        pool_layouts.At(DescriptorSet::Index::DESCRIPTOR_SET_INDEX_BINDLESS),
        pool_layouts.At(DescriptorSet::Index::DESCRIPTOR_SET_INDEX_BINDLESS_FRAME_1),
        pool_layouts.At(DescriptorSet::Index::DESCRIPTOR_SET_INDEX_VOXELIZER),
        pool_layouts.At(DescriptorSet::Index::DESCRIPTOR_SET_INDEX_RAYTRACING)
    };
#else
    std::vector<VkDescriptorSetLayout> used_layouts{ // tmp
        pool_layouts.At(DescriptorSet::Index::DESCRIPTOR_SET_INDEX_UNUSED),
        pool_layouts.At(DescriptorSet::Index::DESCRIPTOR_SET_INDEX_GLOBAL),
        pool_layouts.At(DescriptorSet::Index::DESCRIPTOR_SET_INDEX_SCENE),
        pool_layouts.At(DescriptorSet::Index::DESCRIPTOR_SET_INDEX_OBJECT),
        pool_layouts.At(DescriptorSet::Index::DESCRIPTOR_SET_INDEX_SCENE_FRAME_1),
        pool_layouts.At(DescriptorSet::Index::DESCRIPTOR_SET_INDEX_OBJECT_FRAME_1),
        pool_layouts.At(DescriptorSet::Index::DESCRIPTOR_SET_INDEX_VOXELIZER),
        pool_layouts.At(DescriptorSet::Index::DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES)
    };
#endif
    
    return used_layouts;
}

} // namespace renderer
} // namespace hyperion
