/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BACKEND_RENDERER_FENCE_HPP
#define HYPERION_BACKEND_RENDERER_FENCE_HPP

#include <rendering/backend/Platform.hpp>
#include <rendering/backend/RendererResult.hpp>

#include <core/Defines.hpp>

namespace hyperion {
namespace renderer {
namespace platform {

template <PlatformType PLATFORM>
class Device;

template <PlatformType PLATFORM>
struct FencePlatformImpl;

template <PlatformType PLATFORM>
class Fence
{
public:
    HYP_API Fence();
    Fence(const Fence &other)               = delete;
    Fence &operator=(const Fence &other)    = delete;
    HYP_API ~Fence();

    [[nodiscard]]
    HYP_FORCE_INLINE
    FencePlatformImpl<PLATFORM> &GetPlatformImpl()
        { return m_platform_impl; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    const FencePlatformImpl<PLATFORM> &GetPlatformImpl() const
        { return m_platform_impl; }

    HYP_API Result Create(Device<PLATFORM> *device);
    HYP_API Result Destroy(Device<PLATFORM> *device);
    HYP_API Result WaitForGPU(Device<PLATFORM> *device, bool timeout_loop = false);
    HYP_API Result Reset(Device<PLATFORM> *device);

private:
    FencePlatformImpl<PLATFORM> m_platform_impl;
};

} // namespace platform
} // namespace renderer
} // namespace hyperion


#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererFence.hpp>
#else
#error Unsupported rendering backend
#endif

namespace hyperion {
namespace renderer {

using Fence = platform::Fence<Platform::CURRENT>;

} // namespace renderer
} // namespace hyperion

#endif