#ifndef HYPERION_RENDERER_PIPELINE_H
#define HYPERION_RENDERER_PIPELINE_H

#include "RendererStructs.hpp"

#include <math/Vector4.hpp>
#include <Types.hpp>

#include <vulkan/vulkan.h>

#include <cstdint>

namespace hyperion {
namespace renderer {
    
class Device;
class DescriptorPool;

class Pipeline {
    friend class DescriptorPool;

public:
    struct alignas(16) PushConstantData {
        union {
            struct {  // NOLINT(clang-diagnostic-nested-anon-types)
                UInt32 grid_size,
                       count_mode;
            } voxelizer_data;

            struct {  // NOLINT(clang-diagnostic-nested-anon-types)
                UInt32 num_fragments,
                       voxel_grid_size,
                       mipmap_level;
            } octree_data;

            struct {  // NOLINT(clang-diagnostic-nested-anon-types)
                UInt32 x, y;
            } counter;

            struct {  // NOLINT(clang-diagnostic-nested-anon-types)
                float  matrix[16];
                UInt32 time;
            } probe_data;

            struct {  // NOLINT(clang-diagnostic-nested-anon-types)
                UInt32 extent[4];
                float  aabb_max[4];
                float  aabb_min[4];
            } vct_data;

            struct {  // NOLINT(clang-diagnostic-nested-anon-types)
                float view_proj[16];
            } shadow_map_data;
            
            struct {  // NOLINT(clang-diagnostic-nested-anon-types)
                UInt32 current_effect_index_stage; // 31bits for index, 1 bit for stage
            } post_fx_data;

            struct {  // NOLINT(clang-diagnostic-nested-anon-types)
                UInt32 index;
            } render_component_data;

            struct {  // NOLINT(clang-diagnostic-nested-anon-types)
                UInt32 width, height;
                float ray_step,
                      num_iterations,
                      max_ray_distance;
            } ssr_data;
        };
    } push_constants;

    static_assert(sizeof(PushConstantData) <= 128);

    Pipeline();
    Pipeline(const Pipeline &other) = delete;
    Pipeline &operator=(const Pipeline &other) = delete;
    ~Pipeline();

    VkPipelineLayout layout;

protected:
    std::vector<VkDescriptorSetLayout> GetDescriptorSetLayouts(Device *device, DescriptorPool *descriptor_pool) const;

    VkPipeline pipeline;
};

} // namespace renderer
} // namespace hyperion

#endif
