/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERER_BACKEND_VULKAN_STRUCTS_HPP
#define HYPERION_RENDERER_BACKEND_VULKAN_STRUCTS_HPP

#include <core/utilities/Optional.hpp>
#include <core/containers/Array.hpp>
#include <Types.hpp>

#include <vulkan/vulkan.h>

#include <memory>

namespace hyperion {
namespace renderer {

struct MeshBindingDescription
{
    uint32              binding;
    uint32              stride;
    VkVertexInputRate   input_rate;

    MeshBindingDescription()
        : binding(0), stride(0), input_rate(VK_VERTEX_INPUT_RATE_VERTEX)
    {
    }

    MeshBindingDescription(uint32 binding, uint32 stride, VkVertexInputRate input_rate)
        : binding(binding), stride(stride), input_rate(input_rate)
    {
    }

    VkVertexInputBindingDescription GetBindingDescription()
    {
        VkVertexInputBindingDescription bind;
        bind.binding = this->binding;
        bind.stride = this->stride;
        bind.inputRate = this->input_rate;
        return bind;
    }
};


struct QueueFamilyIndices
{
    Optional<uint32> graphics_family;
    Optional<uint32> transfer_family;
    Optional<uint32> present_family;
    Optional<uint32> compute_family;

    bool IsComplete() const {
        return graphics_family.HasValue()
            && transfer_family.HasValue()
            && present_family.HasValue()
            && compute_family.HasValue();
    }
};

struct SwapchainSupportDetails
{
    VkSurfaceCapabilitiesKHR        capabilities;
    Array<VkQueueFamilyProperties>  queue_family_properties;
    Array<VkSurfaceFormatKHR>       formats;
    Array<VkPresentModeKHR>         present_modes;
};

struct IndirectDrawCommand
{
    // native vk object
    VkDrawIndexedIndirectCommand command;
};

static_assert(std::is_standard_layout_v<IndirectDrawCommand>, "IndirectDrawCommand must be POD");
static_assert(sizeof(IndirectDrawCommand) == 20, "Verify size of struct in shader");

} // namespace renderer
} // namespace hyperion

#endif //HYPERION_RENDERER_BACKEND_VULKAN_STRUCTS_HPP
