#ifndef HYPERION_RENDERER_PIPELINE_H
#define HYPERION_RENDERER_PIPELINE_H

#include "renderer_structs.h"

#include <math/vector4.h>

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
                uint32_t grid_size,
                         count_mode;
            } voxelizer_data;

            struct {  // NOLINT(clang-diagnostic-nested-anon-types)
                uint32_t num_fragments,
                         voxel_grid_size,
                         mipmap_level;
            } octree_data;

            struct {  // NOLINT(clang-diagnostic-nested-anon-types)
                uint32_t x, y;
            } counter;

            struct {  // NOLINT(clang-diagnostic-nested-anon-types)
                float    matrix[16];
                uint32_t time;
            } probe_data;

            struct {  // NOLINT(clang-diagnostic-nested-anon-types)
                uint32_t extent[4];
                float    aabb_max[4];
                float    aabb_min[4];
            } vct_data;

            struct {  // NOLINT(clang-diagnostic-nested-anon-types)
                float view_proj[16];
            } shadow_map_data;
        };
    } push_constants;

    static_assert(sizeof(PushConstantData) <= 128);

    Pipeline();
    Pipeline(const Pipeline &other) = delete;
    Pipeline &operator=(const Pipeline &other) = delete;
    ~Pipeline();

protected:
    std::vector<VkDescriptorSetLayout> GetDescriptorSetLayouts(Device *device, DescriptorPool *descriptor_pool) const;

    VkPipeline pipeline;
    VkPipelineLayout layout;
};

} // namespace renderer
} // namespace hyperion

#endif
