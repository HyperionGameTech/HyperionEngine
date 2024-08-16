/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYP_CORE_CORE_HPP
#define HYP_CORE_CORE_HPP

#include <core/Defines.hpp>
#include <core/object/HypClassRegistry.hpp>
#include <core/ID.hpp>

#include <core/utilities/TypeID.hpp>

#include <rendering/backend/Platform.hpp>

namespace hyperion::renderer {

namespace platform {

template <PlatformType PLATFORM>
class Device;
} // namespace platform

using Device = platform::Device<Platform::CURRENT>;

} // namespace hyperion::renderer

namespace hyperion {

class Engine;
class HypClass;
class ObjectPool;

template <class T>
struct Handle;

HYP_API Engine *GetEngine();

template <class T>
HYP_NODISCARD HYP_FORCE_INLINE inline Handle<T> CreateObject()
{
    auto &container = Handle<T>::GetContainer();

    const uint index = container.NextIndex();
    container.ConstructAtIndex(index);

    return Handle<T>(ID<T>::FromIndex(index));
}

template <class T, class... Args>
HYP_NODISCARD HYP_FORCE_INLINE inline Handle<T> CreateObject(Args &&... args)
{
    auto &container = Handle<T>::GetContainer();

    const uint index = container.NextIndex();

    container.ConstructAtIndex(
        index,
        std::forward<Args>(args)...
    );

    return Handle<T>(ID<T>::FromIndex(index));
}

template <class T>
HYP_FORCE_INLINE inline bool InitObject(Handle<T> &handle)
{
    if (!handle) {
        return false;
    }

    if (!handle->GetID()) {
        return false;
    }

    handle->Init();

    return true;
}

template <class T>
HYP_FORCE_INLINE const HypClass *GetClass()
{
    return HypClassRegistry::GetInstance().template GetClass<T>();
}

template <class T>
HYP_FORCE_INLINE const HypClass *GetClass(const T *ptr)
{
    return HypClassRegistry::GetInstance().template GetClass<T>();
}

template <class T>
HYP_FORCE_INLINE const HypClass *GetClass(const Handle<T> &handle)
{
    return HypClassRegistry::GetInstance().template GetClass<T>();
}

HYP_API const HypClass *GetClass(TypeID type_id);

HYP_API const HypClass *GetClass(WeakName type_name);

} // namespace hyperion

#endif