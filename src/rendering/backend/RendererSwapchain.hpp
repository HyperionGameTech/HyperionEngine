/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BACKEND_RENDERER_SWAPCHAIN_HPP
#define HYPERION_BACKEND_RENDERER_SWAPCHAIN_HPP

#include <rendering/backend/Platform.hpp>
#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererImage.hpp>

#include <core/math/Vector2.hpp>

#include <core/Defines.hpp>

namespace hyperion {
namespace renderer {
namespace platform {

template <PlatformType PLATFORM>
class Device;

template <PlatformType PLATFORM>
struct SwapchainPlatformImpl;

template <PlatformType PLATFORM>
class Swapchain
{
public:
    friend struct SwapchainPlatformImpl<PLATFORM>;

    static constexpr PlatformType platform = PLATFORM;
    
    HYP_API Swapchain();
    Swapchain(const Swapchain &other)               = delete;
    Swapchain &operator=(const Swapchain &other)    = delete;
    Swapchain(Swapchain &&other) noexcept           = delete;
    Swapchain &operator=(Swapchain &other) noexcept = delete;
    HYP_API ~Swapchain();
    
    HYP_FORCE_INLINE SwapchainPlatformImpl<PLATFORM> &GetPlatformImpl()
        { return m_platform_impl; }

    HYP_FORCE_INLINE const SwapchainPlatformImpl<PLATFORM> &GetPlatformImpl() const
        { return m_platform_impl; }

    HYP_FORCE_INLINE const FrameHandlerRef<PLATFORM> &GetFrameHandler() const
        { return m_frame_handler; }

    HYP_FORCE_INLINE SizeType NumImages() const
        { return m_images.Size(); }

    HYP_FORCE_INLINE const Array<ImageRef<PLATFORM>> &GetImages() const
        { return m_images; }

    HYP_API bool IsCreated() const;

    HYP_API RendererResult Create(Device<PLATFORM> *device);
    HYP_API RendererResult Destroy(Device<PLATFORM> *device);

    Vec2u                           extent;
    InternalFormat                  image_format;

private:
    Array<ImageRef<PLATFORM>>       m_images;

    FrameHandlerRef<PLATFORM>       m_frame_handler;

    SwapchainPlatformImpl<PLATFORM> m_platform_impl;
};

} // namespace platform
} // namespace renderer
} // namespace hyperion

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererSwapchain.hpp>
#else
#error Unsupported rendering backend
#endif

namespace hyperion {
namespace renderer {

using Swapchain = platform::Swapchain<Platform::CURRENT>;

} // namespace renderer
} // namespace hyperion

#endif