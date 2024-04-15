/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERER_BACKEND_VULKAN_COMPUTE_PIPELINE_HPP
#define HYPERION_RENDERER_BACKEND_VULKAN_COMPUTE_PIPELINE_HPP

#include <core/Containers.hpp>

#include <rendering/backend/RendererPipeline.hpp>
#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererSwapchain.hpp>
#include <rendering/backend/RendererBuffer.hpp>
#include <rendering/backend/RendererShader.hpp>

#include <Types.hpp>

namespace hyperion {
namespace renderer {
namespace platform {

template <PlatformType PLATFORM>
class ShaderProgram;

template <>
class ComputePipeline<Platform::VULKAN> : public Pipeline<Platform::VULKAN>
{
public:
    ComputePipeline();
    ComputePipeline(ShaderProgramRef<Platform::VULKAN> shader, DescriptorTableRef<Platform::VULKAN> descriptor_table);
    ComputePipeline(const ComputePipeline &other) = delete;
    ComputePipeline &operator=(const ComputePipeline &other) = delete;
    ~ComputePipeline();

    Result Create(Device<Platform::VULKAN> *device);
    Result Destroy(Device<Platform::VULKAN> *device);

    void Bind(CommandBuffer<Platform::VULKAN> *command_buffer) const;
    void Bind(CommandBuffer<Platform::VULKAN> *command_buffer, const void *push_constants_ptr, SizeType push_constants_size);
    void Bind(CommandBuffer<Platform::VULKAN> *command_buffer, const PushConstantData &push_constant_data);

    void SubmitPushConstants(CommandBuffer<Platform::VULKAN> *cmd) const;

    void Dispatch(CommandBuffer<Platform::VULKAN> *command_buffer, Extent3D group_size) const;
    void DispatchIndirect(
        CommandBuffer<Platform::VULKAN> *command_buffer,
        const IndirectBuffer<Platform::VULKAN> *indirect,
        SizeType offset = 0
    ) const;
};

} // namspace platform
} // namespace renderer
} // namespace hyperion

#endif //HYPERION_RENDERER_BACKEND_VULKAN_COMPUTE_PIPELINE_HPP
