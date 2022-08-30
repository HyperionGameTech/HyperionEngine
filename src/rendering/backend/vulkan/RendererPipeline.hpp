#ifndef HYPERION_RENDERER_PIPELINE_H
#define HYPERION_RENDERER_PIPELINE_H

#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/RendererDescriptorSet.hpp>
#include <core/lib/DynArray.hpp>
#include <core/lib/Optional.hpp>

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
    struct alignas(128) PushConstantData {
        union {
            struct  // NOLINT(clang-diagnostic-nested-anon-types)
            {
                UInt32 grid_size,
                    count_mode;
            } voxelizer_data;

            struct // NOLINT(clang-diagnostic-nested-anon-types)
            {
                UInt32 num_fragments,
                    voxel_grid_size,
                    mipmap_level;
            } octree_data;

            struct // NOLINT(clang-diagnostic-nested-anon-types)
            {
                UInt32 x, y;
            } counter;

            struct // NOLINT(clang-diagnostic-nested-anon-types)
            {
                float matrix[16];
                UInt32 time;
            } probe_data;

            struct // NOLINT(clang-diagnostic-nested-anon-types)
            {
                UInt32 extent[4];
                float aabb_max[4];
                float aabb_min[4];
            } vct_data;

            struct // NOLINT(clang-diagnostic-nested-anon-types)
            {
                float view_proj[16];
            } shadow_map_data;
            
            struct // NOLINT(clang-diagnostic-nested-anon-types)
            {
                UInt32 current_effect_index_stage; // 31bits for index, 1 bit for stage
            } post_fx_data;

            struct // NOLINT(clang-diagnostic-nested-anon-types)
            {
                UInt32 index;
            } render_component_data;

            struct // NOLINT(clang-diagnostic-nested-anon-types)
            {
                UInt32 width, height;
                float ray_step,
                    num_iterations,
                    max_ray_distance,
                    distance_bias,
                    offset,
                    eye_fade_start,
                    eye_fade_end,
                    screen_edge_fade_start,
                    screen_edge_fade_end;
            } ssr_data;

            struct // NOLINT(clang-diagnostic-nested-anon-types)
            {
                UInt32 flags;
            } deferred_data;

            struct // NOLINT(clang-diagnostic-nested-anon-types)
            {
                ShaderVec2<UInt32> mip_dimensions;
                ShaderVec2<UInt32> prev_mip_dimensions;
                UInt32 mip_level;
            } depth_pyramid_data;
            
            struct // NOLINT(clang-diagnostic-nested-anon-types)
            {
                UInt32 batch_offset;
                UInt32 num_draw_proxies;
                UInt32 scene_id;
                ShaderVec2<UInt32> depth_pyramid_dimensions;
            } object_visibility_data;

            struct // NOLINT(clang-diagnostic-nested-anon-types)
            {
                ShaderVec4<UInt32> mip_dimensions;
                ShaderVec4<UInt32> prev_mip_dimensions;
                UInt32 mip_level;
            } voxel_mip_data;
        };
    } push_constants;

    static_assert(sizeof(PushConstantData) <= 128);

    Pipeline();
    /*! \brief Construct a pipeline using the given \ref used_descriptor_set as the descriptor sets to be
        used with this pipeline.  */
    Pipeline(const DynArray<const DescriptorSet *> &used_descriptor_sets);
    Pipeline(const Pipeline &other) = delete;
    Pipeline &operator=(const Pipeline &other) = delete;
    ~Pipeline();

    void SetPushConstants(const PushConstantData &push_constants) { this->push_constants = push_constants; }

    VkPipelineLayout layout;

protected:
    std::vector<VkDescriptorSetLayout> GetDescriptorSetLayouts(Device *device, DescriptorPool *descriptor_pool) const;

    VkPipeline pipeline;

private:
    Optional<DynArray<const DescriptorSet *>> m_used_descriptor_sets;
};

} // namespace renderer
} // namespace hyperion

#endif
