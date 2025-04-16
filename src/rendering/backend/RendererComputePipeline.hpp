/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BACKEND_RENDERER_COMPUTE_PIPELINE_HPP
#define HYPERION_BACKEND_RENDERER_COMPUTE_PIPELINE_HPP

#include <rendering/backend/Platform.hpp>
#include <rendering/backend/RendererPipeline.hpp>
#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererSwapchain.hpp>
#include <rendering/backend/RendererBuffer.hpp>
#include <rendering/backend/RendererShader.hpp>
#include <rendering/backend/RenderObject.hpp>

#include <core/Defines.hpp>

namespace hyperion {
namespace renderer {
namespace platform {

template <PlatformType PLATFORM>
class ComputePipeline : public Pipeline<PLATFORM>
{
public:
    static constexpr PlatformType platform = PLATFORM;

    ComputePipeline();
    ComputePipeline(const ShaderRef<PLATFORM> &shader, const DescriptorTableRef<PLATFORM> &descriptor_table);
    ComputePipeline(const ComputePipeline &other)               = delete;
    ComputePipeline &operator=(const ComputePipeline &other)    = delete;
    ~ComputePipeline();

    RendererResult Create();
    RendererResult Destroy();

    void Bind(CommandBuffer<PLATFORM> *command_buffer) const;

    void Dispatch(CommandBuffer<PLATFORM> *command_buffer, const Vec3u &group_size) const;
    void DispatchIndirect(
        CommandBuffer<PLATFORM> *command_buffer,
        const IndirectBuffer<PLATFORM> *indirect,
        SizeType offset = 0
    ) const;
};

} // namespace platform
} // namespace renderer
} // namespace hyperion

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererComputePipeline.hpp>
#else
#error Unsupported rendering backend
#endif

namespace hyperion {
namespace renderer {

using ComputePipeline = platform::ComputePipeline<Platform::CURRENT>;

} // namespace renderer
} // namespace hyperion

#endif