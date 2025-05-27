/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYP_CORE_CORE_HPP
#define HYP_CORE_CORE_HPP

#include <core/object/HypClassRegistry.hpp>

#include <rendering/backend/Platform.hpp>

namespace hyperion::renderer {

namespace platform {

template <PlatformType PLATFORM>
class Device;
} // namespace platform

using Device = platform::Device<Platform::current>;

} // namespace hyperion::renderer

namespace hyperion {

class Engine;

} // namespace hyperion

#endif