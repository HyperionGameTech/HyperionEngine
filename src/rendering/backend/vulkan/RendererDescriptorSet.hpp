/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BACKEND_RENDERER_VULKAN_DESCRIPTOR_SET_HPP
#define HYPERION_BACKEND_RENDERER_VULKAN_DESCRIPTOR_SET_HPP

#include <core/Name.hpp>
#include <core/utilities/Optional.hpp>
#include <core/memory/RefCountedPtr.hpp>
#include <core/containers/ArrayMap.hpp>
#include <core/threading/Mutex.hpp>
#include <core/Defines.hpp>

#include <rendering/backend/Platform.hpp>
#include <rendering/backend/RenderObject.hpp>

#include <Types.hpp>
#include <Constants.hpp>

#include <vulkan/vulkan.h>

namespace hyperion {
namespace renderer {

struct VulkanDescriptorSetLayoutWrapper;

struct VulkanDescriptorElementInfo
{
    uint32              binding;
    uint32              index;
    VkDescriptorType    descriptor_type;

    union {
        VkDescriptorBufferInfo                          buffer_info;
        VkDescriptorImageInfo                           image_info;
        VkWriteDescriptorSetAccelerationStructureKHR    acceleration_structure_info;
    };
};

namespace platform {

template <PlatformType PLATFORM>
class Device;

template <PlatformType PLATFORM>
class GPUBuffer;

template <PlatformType PLATFORM>
class ImageView;

template <PlatformType PLATFORM>
class Sampler;

template <PlatformType PLATFORM>
class TopLevelAccelerationStructure;

template <PlatformType PLATFORM>
class GraphicsPipeline;

template <PlatformType PLATFORM>
class ComputePipeline;

template <PlatformType PLATFORM>
class RaytracingPipeline;

template <PlatformType PLATFORM>
class CommandBuffer;

template <>
struct DescriptorSetElementTypeInfo<Platform::VULKAN, GPUBuffer<Platform::VULKAN>>
{
    static constexpr uint32 mask = (1u << uint32(DescriptorSetElementType::UNIFORM_BUFFER))
        | (1u << uint32(DescriptorSetElementType::UNIFORM_BUFFER_DYNAMIC))
        | (1u << uint32(DescriptorSetElementType::STORAGE_BUFFER))
        | (1u << uint32(DescriptorSetElementType::STORAGE_BUFFER_DYNAMIC));
};

template <>
struct DescriptorSetElementTypeInfo<Platform::VULKAN, ImageView<Platform::VULKAN>>
{
    static constexpr uint32 mask = (1u << uint32(DescriptorSetElementType::IMAGE))
        | (1u << uint32(DescriptorSetElementType::IMAGE_STORAGE));
};

template <>
struct DescriptorSetElementTypeInfo<Platform::VULKAN, Sampler<Platform::VULKAN>>
{
    static constexpr uint32 mask = (1u << uint32(DescriptorSetElementType::SAMPLER));
};

template <>
struct DescriptorSetElementTypeInfo<Platform::VULKAN, TopLevelAccelerationStructure<Platform::VULKAN>>
{
    static constexpr uint32 mask = (1u << uint32(DescriptorSetElementType::TLAS));
};

struct DescriptorSetElementCachedValue
{
    VulkanDescriptorElementInfo info;
};

template <>
struct DescriptorSetPlatformImpl<Platform::VULKAN>
{
    DescriptorSet<Platform::VULKAN>                         *self = nullptr;
    VkDescriptorSet                                         handle = VK_NULL_HANDLE;
    HashMap<Name, Array<DescriptorSetElementCachedValue>>   cached_elements;
    RC<VulkanDescriptorSetLayoutWrapper>                    vk_layout_wrapper;
};

} // namespace platform
} // namespace renderer
} // namespace hyperion

#endif