/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BACKEND_RENDERER_HELPERS_HPP
#define HYPERION_BACKEND_RENDERER_HELPERS_HPP

#include <core/Defines.hpp>

#include <core/functional/Proc.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <Types.hpp>

namespace hyperion {
namespace renderer {
namespace helpers {

uint32 MipmapSize(uint32 src_size, int lod);

} // namespace helpers

namespace platform {

template <PlatformType PLATFORM>
class Device;

template <PlatformType PLATFORM>
class CommandBuffer;

template <PlatformType PLATFORM>
class Fence;

template <PlatformType PLATFORM>
struct SingleTimeCommandsPlatformImpl;

template <PlatformType PLATFORM>
class SingleTimeCommands
{
public:
    HYP_API SingleTimeCommands(Device<PLATFORM> *device);
    SingleTimeCommands(const SingleTimeCommands &other)                 = delete;
    SingleTimeCommands &operator=(const SingleTimeCommands &other)      = delete;
    SingleTimeCommands(SingleTimeCommands &&other) noexcept             = delete;
    SingleTimeCommands &operator=(SingleTimeCommands &&other) noexcept  = delete;
    HYP_API ~SingleTimeCommands();

    void Push(Proc<RendererResult(const platform::CommandBufferRef<PLATFORM> &)> &&fn)
    {
        m_functions.PushBack(std::move(fn));
    }

    RendererResult Execute()
    {
        HYPERION_BUBBLE_ERRORS(Begin());

        RendererResult result;

        for (auto &fn : m_functions) {
            HYPERION_PASS_ERRORS(fn(m_command_buffer), result);

            if (!result) {
                break;
            }
        }

        m_functions.Clear();

        HYPERION_PASS_ERRORS(End(), result);

        return result;
    }

private:
    SingleTimeCommandsPlatformImpl<PLATFORM>                                    m_platform_impl;

    HYP_API RendererResult Begin();
    HYP_API RendererResult End();

    Device<PLATFORM>                                                            *m_device;
    Array<Proc<RendererResult(const platform::CommandBufferRef<PLATFORM> &)>>   m_functions;
    FenceRef<PLATFORM>                                                          m_fence;

    CommandBufferRef<PLATFORM>                                                  m_command_buffer;
};

} // namespace platform

} // namespace renderer
} // namespace hyperion

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererHelpers.hpp>
#else
#error Unsupported rendering backend
#endif

namespace hyperion {
namespace renderer {

using SingleTimeCommands = platform::SingleTimeCommands<Platform::CURRENT>;

} // namespace renderer
} // namespace hyperion

#endif