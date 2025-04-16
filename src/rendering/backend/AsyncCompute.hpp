/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BACKEND_RENDERER_ASYNC_COMPUTE_HPP
#define HYPERION_BACKEND_RENDERER_ASYNC_COMPUTE_HPP

#include <core/Defines.hpp>

#include <core/containers/FixedArray.hpp>
#include <core/containers/ArrayMap.hpp>

#include <rendering/rhi/RHICommandList.hpp>

#include <rendering/backend/Platform.hpp>
#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererResult.hpp>
#include <rendering/backend/RendererBuffer.hpp>
#include <rendering/backend/RendererCommandBuffer.hpp>

#include <core/math/MathUtil.hpp>
#include <core/math/Extent.hpp>

#include <Types.hpp>

namespace hyperion::renderer {

class IAsyncCompute
{
public:
    virtual ~IAsyncCompute() = default;

    virtual bool IsSupported() const = 0;

    virtual RHICommandList &GetCommandList(uint32 frame_index) = 0;
    virtual const RHICommandList &GetCommandList(uint32 frame_index) const = 0;
};

namespace platform {

template <PlatformType PLATFORM>
class Device;

template <PlatformType PLATFORM>
class Frame;

template <PlatformType PLATFORM>
class DescriptorTable;

template <PlatformType PLATFORM>
class AsyncCompute final : public IAsyncCompute
{
public:
    HYP_API AsyncCompute();
    AsyncCompute(const AsyncCompute &)                  = delete;
    AsyncCompute &operator=(const AsyncCompute &)       = delete;
    AsyncCompute(AsyncCompute &&) noexcept              = delete;
    AsyncCompute &operator=(AsyncCompute &&) noexcept   = delete;
    HYP_API ~AsyncCompute();

    virtual bool IsSupported() const override
        { return m_is_supported; }

    virtual RHICommandList &GetCommandList(uint32 frame_index) override
        { return m_command_lists[frame_index]; }

    virtual const RHICommandList &GetCommandList(uint32 frame_index) const override
        { return m_command_lists[frame_index]; }

    HYP_API RendererResult Create();
    HYP_API RendererResult Submit(Frame<PLATFORM> *frame);

    HYP_API RendererResult PrepareForFrame(Frame<PLATFORM> *frame);
    HYP_API RendererResult WaitForFence(Frame<PLATFORM> *frame);

private:
    FixedArray<RHICommandList, max_frames_in_flight>                m_command_lists;
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