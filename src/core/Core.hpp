/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#ifndef HYP_CORE_CORE_HPP
#define HYP_CORE_CORE_HPP

#include <core/lib/Memory.hpp>
#include <system/Debug.hpp>
#include <core/Defines.hpp>
#include <Types.hpp>

#include <rendering/backend/Platform.hpp>

namespace hyperion::renderer {

namespace platform {

template <PlatformType PLATFORM>
class Device;
} // namespace platform

using Device = platform::Device<Platform::CURRENT>;

} // namespace hyperion::renderer

namespace hyperion::v2 {

class Engine;
class ObjectPool;

template <class T>
struct Handle;

HYP_API Engine *GetEngine();
HYP_API renderer::Device *GetEngineDevice();

template <class T, class EngineImpl>
static HYP_FORCE_INLINE bool InitObjectIntern(EngineImpl *engine, Handle<T> &handle)
{
    return engine->template InitObject<T>(handle);
}

/**
 * \brief Initializes an object with the engine. This is a wrapper around the engine's InitObject function.
 */
template <class T>
static HYP_FORCE_INLINE bool InitObject(Handle<T> &handle)
{
    return InitObjectIntern(GetEngine(), handle);
}

} // namespace hyperion::v2

#endif