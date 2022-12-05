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

#endif