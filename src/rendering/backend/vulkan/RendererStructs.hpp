#ifndef HYPERION_RENDERER_BACKEND_VULKAN_STRUCTS_HPP
#define HYPERION_RENDERER_BACKEND_VULKAN_STRUCTS_HPP

#include <core/lib/Optional.hpp>
#include <core/lib/DynArray.hpp>
#include <core/lib/HashMap.hpp>
#include <HashCode.hpp>
#include <Types.hpp>

#include <math/Vector2.hpp>
#include <math/Vector3.hpp>
#include <math/Vector4.hpp>
#include <math/Matrix4.hpp>

#include <vulkan/vulkan.h>

#include <cstdint>
#include <algorithm>
#include <set>
#include <memory>
#include <utility>

namespace hyperion {
namespace renderer {

struct MeshBindingDescription
{
    UInt32              binding;
    UInt32              stride;
    VkVertexInputRate   input_rate;

    MeshBindingDescription()
        : binding(0), stride(0), input_rate(VK_VERTEX_INPUT_RATE_VERTEX)
    {
    }

    MeshBindingDescription(UInt32 binding, UInt32 stride, VkVertexInputRate input_rate)
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
    Optional<UInt32> graphics_family;
    Optional<UInt32> transfer_family;
    Optional<UInt32> present_family;
    Optional<UInt32> compute_family;

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
