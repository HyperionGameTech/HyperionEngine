/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BACKEND_RENDERER_ASYNC_COMPUTE_HPP
#define HYPERION_BACKEND_RENDERER_ASYNC_COMPUTE_HPP

#include <core/Defines.hpp>

#include <core/containers/FixedArray.hpp>
#include <core/containers/ArrayMap.hpp>

#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/Platform.hpp>
#include <rendering/backend/RendererResult.hpp>
#include <rendering/backend/RendererBuffer.hpp>

#include <math/MathUtil.hpp>
#include <math/Extent.hpp>

#include <Types.hpp>

namespace hyperion::renderer {

namespace platform {

template <PlatformType PLATFORM>
class Device;

template <PlatformType PLATFORM>
class Frame;

template <PlatformType PLATFORM>
class DescriptorTable;

template <PlatformType PLATFORM>
class AsyncCompute
{
public:
    HYP_API AsyncCompute();
    AsyncCompute(const AsyncCompute &)                  = delete;
    AsyncCompute &operator=(const AsyncCompute &)       = delete;
    AsyncCompute(AsyncCompute &&) noexcept              = delete;
    AsyncCompute &operator=(AsyncCompute &&) noexcept   = delete;
    HYP_API ~AsyncCompute();

    HYP_FORCE_INLINE bool IsSupported() const
        { return m_is_supported; }

    HYP_API Result Create(Device<PLATFORM> *device);
    HYP_API Result Submit(Device<PLATFORM> *device, Frame<PLATFORM> *frame);

    HYP_API Result PrepareForFrame(Device<PLATFORM> *device, Frame<PLATFORM> *frame);
    HYP_API Result WaitForFence(Device<PLATFORM> *device, Frame<PLATFORM> *frame);

    HYP_API void InsertBarrier(
        Frame<PLATFORM> *frame,
        const GPUBufferRef<PLATFORM> &buffer,
        ResourceState resource_state
    ) const;

    HYP_API void Dispatch(
        Frame<PLATFORM> *frame,
        const ComputePipelineRef<PLATFORM> &ref,
        const Vec3u &extent
    ) const;

    HYP_API void Dispatch(
        Frame<PLATFORM> *frame,
        const ComputePipelineRef<PLATFORM> &ref,
        const Vec3u &extent,
        const DescriptorTableRef<PLATFORM> &descriptor_table,
        const ArrayMap<Name, ArrayMap<Name, uint>> &offsets = { }
    ) const;

private:
    FixedArray<CommandBufferRef<PLATFORM>, max_frames_in_flight>    m_command_buffers;
    FixedArray<FenceRef<PLATFORM>, max_frames_in_flight>            m_fences;
    bool                                                            m_is_supported;
    bool                                                            m_is_fallback;
};

} // namespace platform

} // namespace hyperion::renderer

// to reduce noise, pull the above enum classes into hyperion namespace
namespace hyperion {

namespace renderer {

using AsyncCompute = platform::AsyncCompute<Platform::CURRENT>;

} // namespace renderer

} // namespace hyperion

#endif