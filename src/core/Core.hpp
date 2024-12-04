/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYP_CORE_CORE_HPP
#define HYP_CORE_CORE_HPP

#include <core/Defines.hpp>
#include <core/ID.hpp>

#include <core/object/HypClassRegistry.hpp>

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
class HypEnum;
class HypObjectPtr;

template <class T>
struct Handle;

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

template <class T>
HYP_FORCE_INLINE const HypEnum *GetEnum()
{
    return HypClassRegistry::GetInstance().template GetEnum<T>();
}

HYP_API const HypEnum *GetEnum(TypeID type_id);
HYP_API const HypEnum *GetEnum(WeakName type_name);

HYP_API bool IsInstanceOfHypClass(const HypClass *hyp_class, const void *ptr, TypeID type_id);
HYP_API bool IsInstanceOfHypClass(const HypClass *hyp_class, const HypClass *instance_hyp_class);

} // namespace hyperion

#endif