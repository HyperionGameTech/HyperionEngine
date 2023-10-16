#ifndef HYPERION_RENDERER_BACKEND_VULKAN_PIPELINE_HPP
#define HYPERION_RENDERER_BACKEND_VULKAN_PIPELINE_HPP

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
    
namespace platform {
template <PlatformType PLATFORM>
class Device;
} // namespace platform

using Device = platform::Device<Platform::VULKAN>;

class DescriptorPool;

class Pipeline
{
    friend class DescriptorPool;

public:
    struct alignas(128) PushConstantData
    {
        union
        {
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
                UInt32 num_instances;
                UInt32 scene_id;
                ShaderVec2<UInt32> depth_pyramid_dimensions;
            } object_visibility_data;

            struct // NOLINT(clang-diagnostic-nested-anon-types)
            {
                ShaderVec4<UInt32> mip_dimensions;
                ShaderVec4<UInt32> prev_mip_dimensions;
                UInt32 mip_level;
            } voxel_mip_data;

            struct // NOLINT(clang-diagnostic-nested-anon-types)
            {
                ShaderVec2<UInt32> image_dimensions;
            } blur_shadow_map_data;

            struct // NOLINT(clang-diagnostic-nested-anon-types)
            {
                ShaderVec2<UInt32> image_dimensions;
            } deferred_combine_data;

            struct // NOLINT(clang-diagnostic-nested-anon-types)
            {
                ShaderVec4<Float32> origin;
                Float32 spawn_radius;
                Float32 randomness;
                Float32 avg_lifespan;
                UInt32 max_particles;
                Float32 max_particles_sqrt;
                Float32 delta_time;
                UInt32 global_counter;
            } particle_spawner_data;
        };
    } push_constants;

    static_assert(sizeof(PushConstantData) <= 128);

    Pipeline();
    /*! \brief Construct a pipeline using the given \ref used_descriptor_set as the descriptor sets to be
        used with this pipeline.  */
    Pipeline(const Array<DescriptorSetRef> &used_descriptor_sets);
    Pipeline(const Pipeline &other) = delete;
    Pipeline &operator=(const Pipeline &other) = delete;
    ~Pipeline();

    bool HasCustomDescriptorSets() const
        { return m_has_custom_descriptor_sets; }

    const Optional<Array<DescriptorSetRef>> &GetUsedDescriptorSets() const
        { return m_used_descriptor_sets; }

    void SetUsedDescriptorSets(const Array<DescriptorSetRef> &used_descriptor_sets)
    {
        if (used_descriptor_sets.Any()) {
            m_used_descriptor_sets = used_descriptor_sets;
            m_has_custom_descriptor_sets = true;
        } else {
            m_used_descriptor_sets.Unset();
            m_has_custom_descriptor_sets = false;
        }
    }

    void SetPushConstants(const PushConstantData &push_constants)
        { this->push_constants = push_constants; }
    
    void SetPushConstants(const void *data, SizeType size);

    VkPipelineLayout layout;

protected:
    void AssignDefaultDescriptorSets(DescriptorPool *descriptor_pool);
    std::vector<VkDescriptorSetLayout> GetDescriptorSetLayouts(Device *device, DescriptorPool *descriptor_pool);

    VkPipeline                          pipeline;
    bool                                m_has_custom_descriptor_sets;
    Optional<Array<DescriptorSetRef>>   m_used_descriptor_sets;
};

} // namespace renderer
} // namespace hyperion

#endif
