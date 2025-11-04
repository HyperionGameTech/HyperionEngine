/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/utilities/EnumFlags.hpp>
#include <core/utilities/FormatFwd.hpp>

#include <core/reflection/HypObjectMacros.hpp>

#include <core/Defines.hpp>
#include <core/Constants.hpp>

#include <type_traits>

namespace hyperion {

namespace dotnet {
class ManagedObject;
class ManagedClass;
} // namespace dotnet

class ScriptObjectResource;

namespace utilities {
struct TypeId;
} // namespace utilities

using utilities::TypeId;

enum class ClassFlags : uint8;
enum class ClassAllocationMethod : uint8;

class HypObjectContainerBase;
class HypObjectBase;
struct HypObjectHeader;
class IHypMember;
class Field;
class Method;
class StaticField;
class Property;

template <class T>
struct Handle;

template <class T>
struct WeakHandle;

template <class T>
struct HypObjectMemory;

template <class T>
struct ClassDecl;

template <class T, class T2 = void>
struct IsHypObject;

template <class T, class T2>
struct IsHypObject
{
    static_assert(ImplementationExistsV<T>, "Cannot use IsHypObject with undefined type!");

    static constexpr bool value = false;
};

template <class T>
struct IsHypObject<T, std::enable_if_t<ImplementationExistsV<typename T::ClassInfo::Type>>>
{
    static constexpr bool value = true;

    using Type = typename T::ClassInfo::Type;
};

template <class T>
constexpr bool IsHypObjectV = IsHypObject<T>::value;

template <class T>
using IsHypObject_t = typename IsHypObject<T>::Type;

enum class HypObjectInitializerFlags : uint32
{
    NONE = 0x0,
    SUPPRESS_MANAGED_OBJECT_CREATION = 0x1
};

HYP_MAKE_ENUM_FLAGS(HypObjectInitializerFlags)

struct HypObjectInitializerContext
{
    const Class* cls = nullptr;
    EnumFlags<HypObjectInitializerFlags> flags = HypObjectInitializerFlags::NONE;
};

class HypObjectPtr
{
public:
    HypObjectPtr()
        : m_ptr(nullptr),
          m_class(nullptr)
    {
    }

    explicit HypObjectPtr(std::nullptr_t)
        : m_ptr(nullptr),
          m_class(nullptr)
    {
    }

    HypObjectPtr(const Class* cls, void* ptr)
        : m_ptr(ptr),
          m_class(cls)
    {
    }

    template <class T, typename = std::enable_if_t<IsHypObjectV<T>>>
    explicit HypObjectPtr(T* ptr)
        : m_ptr(ptr),
          m_class(T::StaticClass())
    {
    }

    HypObjectPtr(const HypObjectPtr& other) = default;
    HypObjectPtr& operator=(const HypObjectPtr& other) = default;
    HypObjectPtr(HypObjectPtr&& other) noexcept = default;
    HypObjectPtr& operator=(HypObjectPtr&& other) noexcept = default;
    ~HypObjectPtr() = default;

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return m_ptr != nullptr && m_class != nullptr;
    }

    HYP_FORCE_INLINE bool operator!() const
    {
        return !m_ptr || !m_class;
    }

    HYP_FORCE_INLINE bool operator==(std::nullptr_t) const
    {
        return m_ptr == nullptr;
    }

    HYP_FORCE_INLINE bool operator!=(std::nullptr_t) const
    {
        return m_ptr != nullptr;
    }

    HYP_FORCE_INLINE bool operator==(const HypObjectPtr& other) const
    {
        return m_ptr == other.m_ptr;
    }

    HYP_FORCE_INLINE bool operator!=(const HypObjectPtr& other) const
    {
        return m_ptr != other.m_ptr;
    }

    HYP_FORCE_INLINE bool IsValid() const
    {
        return m_ptr != nullptr && m_class != nullptr;
    }

    HYP_FORCE_INLINE const Class* GetClass() const
    {
        return m_class;
    }

    HYP_FORCE_INLINE void* GetPointer() const
    {
        return m_ptr;
    }

    HYP_API uint32 GetRefCountStrong() const;
    HYP_API uint32 GetRefCountWeak() const;

    HYP_API void IncRef(bool weak = false);
    HYP_API void DecRef(bool weak = false);

private:
    void* m_ptr;
    const Class* m_class;
};

#ifdef HYP_DOTNET
HYP_API void HypObject_IncScriptObjectRef(HypObjectBase* ptr);
HYP_API void HypObject_DecScriptObjectRef(HypObjectBase* ptr);
#endif

struct HypObjectInitializerGuardBase
{
    HYP_API HypObjectInitializerGuardBase(HypObjectPtr ptr);
    HYP_API ~HypObjectInitializerGuardBase();

    HypObjectPtr ptr;
    uint32 count;
};

template <class T>
struct HypObjectInitializerGuard : HypObjectInitializerGuardBase
{
    HypObjectInitializerGuard(void* ptr)
        : HypObjectInitializerGuardBase(HypObjectPtr(T::StaticClass(), ptr))
    {
    }
};

/// ClassRef - A strong reference to a Class.
/// Reference counting only applies to dynamically created / destroyed Class objects (used with scripts).
struct ClassRef
{
    const Class* cls;

    HYP_FORCE_INLINE ClassRef()
        : cls(nullptr)
    {
    }

    HYP_API ClassRef(const Class* cls, int initialRefCount = 1);

    HYP_API ClassRef(const ClassRef& other);
    HYP_API ClassRef& operator=(const ClassRef& other);

    HYP_API ClassRef(ClassRef&& other) noexcept;
    HYP_API ClassRef& operator=(ClassRef&& other) noexcept;

    HYP_API ~ClassRef();

    HYP_FORCE_INLINE bool IsValid() const
    {
        return cls != nullptr;
    }

    HYP_FORCE_INLINE const Class* operator->() const
    {
        return cls;
    }

    HYP_FORCE_INLINE const Class& operator*() const
    {
        return *cls;
    }

    HYP_FORCE_INLINE operator const Class*() const
    {
        return cls;
    }

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return IsValid();
    }

    HYP_FORCE_INLINE bool operator==(const ClassRef& other) const
    {
        return cls == other.cls;
    }

    HYP_FORCE_INLINE bool operator!=(const ClassRef& other) const
    {
        return cls != other.cls;
    }

    HYP_FORCE_INLINE bool operator<(const ClassRef& other) const
    {
        return cls < other.cls;
    }
};

/// Helpers ///

HYP_API extern const Class* GetClass(const TypeId& typeId);

template <class T>
const Class* GetClass()
{
    // If you get an unresolved external for GetClassHelper<T>::Get(),
    // it means that T does not have Class info generated for it. Ensure that
    // the build tool was run and that the generated files are included in the build.
    return GetClassHelper<T>::Get();
}

/// Casts ///

template <class Other, class T>
static inline Other* ObjCast(T* objectPtr)
{
    static_assert(std::is_class_v<Other>, "Other must be a class type to use with ObjCast");

    if constexpr (std::is_same_v<T, Other> || std::is_base_of_v<Other, T>)
    {
        return static_cast<Other*>(objectPtr);
    }

    if (objectPtr && objectPtr->template IsA<Other>())
    {
        return static_cast<Other*>(objectPtr);
    }

    return nullptr;
}

template <class Other, class T>
static inline const Other* ObjCast(const T* objectPtr)
{
    static_assert(std::is_class_v<Other>, "Other must be a class type to use with ObjCast");

    if constexpr (std::is_same_v<T, Other> || std::is_base_of_v<Other, T>)
    {
        return static_cast<const Other*>(objectPtr);
    }

    if (objectPtr && objectPtr->template IsA<Other>())
    {
        return static_cast<const Other*>(const_cast<T*>(objectPtr));
    }

    return nullptr;
}

template <class Other, class T>
static inline const Handle<Other>& ObjCast(const Handle<T>& handle)
{
    static_assert(std::is_class_v<Other>, "Other must be a class type to use with ObjCast");

    if (!handle.IsValid())
    {
        return Handle<Other>::empty;
    }

    if (handle->template IsA<Other>())
    {
        return reinterpret_cast<const Handle<Other>&>(handle);
    }

    return Handle<Other>::empty;
}

template <class Other, class T>
static inline Handle<Other> ObjCast(Handle<T>&& handle)
{
    static_assert(std::is_class_v<Other>, "Other must be a class type to use with ObjCast");

    if (!handle.IsValid())
    {
        return Handle<Other>::empty;
    }

    if (handle->template IsA<Other>())
    {
        return reinterpret_cast<Handle<Other>&&>(handle);
    }

    return Handle<Other>::empty;
}

template <class Other, class T>
static inline const WeakHandle<Other>& ObjCast(const WeakHandle<T>& handle)
{
    static_assert(std::is_class_v<Other>, "Other must be a class type to use with ObjCast");

    if (!handle.IsValid())
    {
        return WeakHandle<Other>::empty;
    }

    if (IsA(TypeInfo_GetClass(*handle.GetTypeInfo()), Other::StaticClass()))
    {
        return reinterpret_cast<const WeakHandle<Other>&>(handle);
    }

    return WeakHandle<Other>::empty;
}

template <class Other, class T>
static inline WeakHandle<Other> ObjCast(WeakHandle<T>&& handle)
{
    static_assert(std::is_class_v<Other>, "Other must be a class type to use with ObjCast");

    if (!handle.IsValid())
    {
        return WeakHandle<Other>::empty;
    }

    if (IsA(TypeInfo_GetClass(*handle.GetTypeInfo()), Other::StaticClass()))
    {
        return reinterpret_cast<WeakHandle<Other>&&>(handle);
    }

    return WeakHandle<Other>::empty;
}

/// IsA() checks ///

// NOTE: These overloads are implemented in Class.cpp
HYP_API extern bool IsA(const Class* cls, const void* ptr, const TypeId& typeId);
HYP_API extern bool IsA(const Class* cls, const Class* instanceClass);

template <class ExpectedType, class InstanceType>
static inline bool IsA()
{
    static_assert(ImplementationExistsV<ExpectedType>, "Implementation does not exist for the expected type! Ensure proper headers are included.");
    static_assert(ImplementationExistsV<InstanceType>, "Implementation does not exist for the instance type! Ensure proper headers are included.");

    static const Class* s_instanceClass = InstanceType::StaticClass();

    if (!s_instanceClass)
    {
        return false;
    }

    static const Class* s_expectedClass = ExpectedType::StaticClass();

    if (!s_expectedClass)
    {
        return false;
    }

    // Short-circuit with compile time checking
    if constexpr (std::is_same_v<ExpectedType, InstanceType> || std::is_base_of_v<ExpectedType, InstanceType>)
    {
        return true;
    }

    static const bool cachedCheck = ::hyperion::IsA(s_expectedClass, s_instanceClass);

    return cachedCheck || IsA(s_expectedClass, s_instanceClass);
}

template <class ExpectedType>
static inline bool IsA(const Class* instanceClass)
{
    if (!instanceClass)
    {
        return false;
    }

    const Class* expectedClass = ExpectedType::StaticClass();

    if (!expectedClass)
    {
        return false;
    }

    return IsA(expectedClass, instanceClass);
}

template <class ExpectedType, class InstanceType>
static inline bool IsA(const InstanceType* instance)
{
    if (!instance)
    {
        return false;
    }

    // first check caches w/ static
    // second uses the actual instance type (can be more derived)
    return ::hyperion::IsA<ExpectedType, InstanceType>() || instance->template IsA<ExpectedType>();
}

template <class ExpectedType, class InstanceType, typename = std::enable_if_t<!std::is_pointer_v<InstanceType>>>
static inline bool IsA(const InstanceType& instance)
{
    // first check caches w/ static
    // second uses the actual instance type (can be more derived)
    return ::hyperion::IsA<ExpectedType, InstanceType>() || instance.template IsA<ExpectedType>();
}

} // namespace hyperion
