/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BACKEND_RENDERER_HELPERS_HPP
#define HYPERION_BACKEND_RENDERER_HELPERS_HPP

#include <core/Defines.hpp>

#include <core/functional/Proc.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <Types.hpp>

namespace hyperion {

class RHICommandList;
namespace helpers {

uint32 MipmapSize(uint32 src_size, int lod);

} // namespace helpers

namespace platform {

template <PlatformType PLATFORM>
class Device;

template <PlatformType PLATFORM>
struct SingleTimeCommandsPlatformImpl;

template <PlatformType PLATFORM>
class SingleTimeCommands
{
public:
    HYP_API SingleTimeCommands();
    SingleTimeCommands(const SingleTimeCommands& other) = delete;
    SingleTimeCommands& operator=(const SingleTimeCommands& other) = delete;
    SingleTimeCommands(SingleTimeCommands&& other) noexcept = delete;
    SingleTimeCommands& operator=(SingleTimeCommands&& other) noexcept = delete;
    HYP_API ~SingleTimeCommands();

    void Push(Proc<void(RHICommandList& command_list)>&& fn)
    {
        m_functions.PushBack(std::move(fn));
    }

    RendererResult Execute();

private:
    SingleTimeCommandsPlatformImpl<PLATFORM> m_platform_impl;

    Device<PLATFORM>* m_device;
    Array<Proc<void(RHICommandList& command_list)>> m_functions;
};

} // namespace platform

} // namespace hyperion

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererHelpers.hpp>
#else
#error Unsupported rendering backend
#endif

namespace hyperion {
using SingleTimeCommands = platform::SingleTimeCommands<Platform::current>;

} // namespace hyperion

#endif