#ifndef HYPERION_V2_BACKEND_RENDERER_DESCRIPTOR_SET_H
#define HYPERION_V2_BACKEND_RENDERER_DESCRIPTOR_SET_H

#include <util/Defines.hpp>


namespace hyperion {
namespace renderer {

class DescriptorSet;
class DescriptorPool;

enum class DescriptorSetState
{
    DESCRIPTOR_CLEAN = 0,
    DESCRIPTOR_DIRTY = 1
};

enum class DescriptorType
{
    UNSET,
    UNIFORM_BUFFER,
    UNIFORM_BUFFER_DYNAMIC,
    STORAGE_BUFFER,
    STORAGE_BUFFER_DYNAMIC,
    IMAGE,
    SAMPLER,
    IMAGE_SAMPLER,
    IMAGE_STORAGE,
    ACCELERATION_STRUCTURE
};

enum class DescriptorKey
{
    UNUSED = 0,

    GBUFFER_TEXTURES,
    GBUFFER_DEPTH,
    GBUFFER_MIP_CHAIN,
    GBUFFER_DEPTH_SAMPLER,
    GBUFFER_SAMPLER,
    DEFERRED_RESULT,
    POST_FX_PRE_STACK,
    POST_FX_POST_STACK,
    POST_FX_UNIFORMS,
    SSR_UV_IMAGE,
    SSR_SAMPLE_IMAGE,
    SSR_RADIUS_IMAGE,
    SSR_BLUR_HOR_IMAGE,
    SSR_BLUR_VERT_IMAGE,
    SSR_UV_TEXTURE,
    SSR_SAMPLE_TEXTURE,
    SSR_RADIUS_TEXTURE,
    SSR_BLUR_HOR_TEXTURE,
    SSR_BLUR_VERT_TEXTURE,
    SSR_FINAL_TEXTURE,
    ENV_PROBE_TEXTURES,
    ENV_PROBES,
    POINT_SHADOW_MAPS,
    VOXEL_IMAGE,
    DEPTH_PYRAMID_RESULT,
    SVO_BUFFER,
    SSR_RESULT,
    SSAO_GI_RESULT,
    UI_TEXTURE,
    MOTION_VECTORS_RESULT,
    RT_RADIANCE_RESULT,
    RT_PROBE_UNIFORMS,
    RT_IRRADIANCE_GRID,
    RT_DEPTH_GRID,
    TEMPORAL_AA_RESULT,
    IMMEDIATE_DRAWS,
    DEFERRED_LIGHTING_AMBIENT,
    DEFERRED_LIGHTING_DIRECT,
    DEFERRED_IRRADIANCE_ACCUM,
    DEFERRED_REFLECTION_PROBE,
    SH_VOLUMES,
    SH_CLIPMAPS,
    VCT_VOXEL_UAV,
    VCT_VOXEL_UNIFORMS,
    VCT_SVO_BUFFER,
    VCT_SVO_FRAGMENT_LIST,
    BLUE_NOISE_BUFFER,
    DOF_BLUR_RESULT,

    SCENE_BUFFER,
    LIGHTS_BUFFER,
    ENV_GRID_BUFFER,
    CURRENT_ENV_PROBE,
    CAMERA_BUFFER,
    SHADOW_MAPS,
    SHADOW_MATRICES,
    CUBEMAP_UNIFORMS,

    MATERIAL_BUFFER,
    OBJECT_BUFFER,
    SKELETON_BUFFER,
    ENTITY_INSTANCES,

    SAMPLER,
    TEXTURES

    // ... 

};

static inline bool IsDescriptorTypeBuffer(DescriptorType type)
{
    return type == DescriptorType::UNIFORM_BUFFER
        || type == DescriptorType::UNIFORM_BUFFER_DYNAMIC
        || type == DescriptorType::STORAGE_BUFFER
        || type == DescriptorType::STORAGE_BUFFER_DYNAMIC;
}

static inline bool IsDescriptorTypeDynamicBuffer(DescriptorType type)
{
    return type == DescriptorType::UNIFORM_BUFFER_DYNAMIC
        || type == DescriptorType::STORAGE_BUFFER_DYNAMIC;
}

} // namespace renderer
} // namespace hyperion

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererDescriptorSet.hpp>
#else
#error Unsupported rendering backend
#endif

namespace hyperion {
namespace renderer {

/* Convenience descriptor classes */

#define HYP_DEFINE_DESCRIPTOR(class_name, descriptor_type) \
    class class_name : public Descriptor { \
    public: \
        class_name(\
            UInt binding \
        ) : Descriptor(binding, descriptor_type) {} \
    }

HYP_DEFINE_DESCRIPTOR(UniformBufferDescriptor,        DescriptorType::UNIFORM_BUFFER);
HYP_DEFINE_DESCRIPTOR(DynamicUniformBufferDescriptor, DescriptorType::UNIFORM_BUFFER_DYNAMIC);
HYP_DEFINE_DESCRIPTOR(StorageBufferDescriptor,        DescriptorType::STORAGE_BUFFER);
HYP_DEFINE_DESCRIPTOR(DynamicStorageBufferDescriptor, DescriptorType::STORAGE_BUFFER_DYNAMIC);
HYP_DEFINE_DESCRIPTOR(ImageDescriptor,                DescriptorType::IMAGE);
HYP_DEFINE_DESCRIPTOR(SamplerDescriptor,              DescriptorType::SAMPLER);
HYP_DEFINE_DESCRIPTOR(ImageSamplerDescriptor,         DescriptorType::IMAGE_SAMPLER);
HYP_DEFINE_DESCRIPTOR(StorageImageDescriptor,         DescriptorType::IMAGE_STORAGE);
HYP_DEFINE_DESCRIPTOR(TlasDescriptor,                 DescriptorType::ACCELERATION_STRUCTURE);

} // namespace renderer
} // namespace hyperion

#undef HYP_DEFINE_DESCRIPTOR

#endif