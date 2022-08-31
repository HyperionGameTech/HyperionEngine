#include <rendering/backend/RendererPipeline.hpp>
#include <core/lib/FlatSet.hpp>

#include <system/Debug.hpp>

namespace hyperion {
namespace renderer {

Pipeline::Pipeline()
    : pipeline(VK_NULL_HANDLE),
      layout(VK_NULL_HANDLE),
      push_constants{}
{
}

Pipeline::Pipeline(const DynArray<const DescriptorSet *> &used_descriptor_sets)
    : m_used_descriptor_sets(used_descriptor_sets),
      pipeline(VK_NULL_HANDLE),
      layout(VK_NULL_HANDLE),
      push_constants{}
{

}

Pipeline::~Pipeline()
{
    AssertThrowMsg(pipeline == VK_NULL_HANDLE, "Expected pipeline to have been destroyed");
    AssertThrowMsg(layout == VK_NULL_HANDLE, "Expected layout to have been destroyed");
}

std::vector<VkDescriptorSetLayout> Pipeline::GetDescriptorSetLayouts(Device *device, DescriptorPool *descriptor_pool) const
{
    // TODO: set specialization constants based on these values so the indices all match up

    std::vector<VkDescriptorSetLayout> used_layouts;

    if (m_used_descriptor_sets.Any()) {
        for (auto *descriptor_set : m_used_descriptor_sets.Get()) {
            used_layouts.push_back(descriptor_set->m_layout);
        }
    } else {
        DynArray<DescriptorSet::Index> all_layouts;
        const auto &pool_layouts = descriptor_pool->GetDescriptorSetLayouts();

#if HYP_FEATURES_BINDLESS_TEXTURES
        all_layouts = DynArray<DescriptorSet::Index> {
            DescriptorSet::Index::DESCRIPTOR_SET_INDEX_UNUSED,
            DescriptorSet::Index::DESCRIPTOR_SET_INDEX_GLOBAL,
            DescriptorSet::Index::DESCRIPTOR_SET_INDEX_SCENE,
            DescriptorSet::Index::DESCRIPTOR_SET_INDEX_VOXELIZER,
            DescriptorSet::Index::DESCRIPTOR_SET_INDEX_OBJECT,
            DescriptorSet::Index::DESCRIPTOR_SET_INDEX_BINDLESS,
            DescriptorSet::Index::DESCRIPTOR_SET_INDEX_RAYTRACING
        };
#else
        all_layouts = DynArray<DescriptorSet::Index> {
            DescriptorSet::Index::DESCRIPTOR_SET_INDEX_UNUSED,
            DescriptorSet::Index::DESCRIPTOR_SET_INDEX_GLOBAL,
            DescriptorSet::Index::DESCRIPTOR_SET_INDEX_SCENE,
            DescriptorSet::Index::DESCRIPTOR_SET_INDEX_VOXELIZER,
            DescriptorSet::Index::DESCRIPTOR_SET_INDEX_OBJECT,
            DescriptorSet::Index::DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES
        };
#endif

        used_layouts.reserve(all_layouts.Size());

        for (auto item : all_layouts) {
            used_layouts.push_back(pool_layouts.At(item));
        }
    }

    return used_layouts;
}

} // namespace renderer
} // namespace hyperion
