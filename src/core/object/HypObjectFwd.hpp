/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

#include <core/utilities/TypeId.hpp>
#include <core/utilities/EnumFlags.hpp>

#include <core/object/ObjId.hpp>

#ifdef HYP_DEBUG_MODE
#include <core/threading/Threads.hpp>
#endif

#include <core/utilities/FormatFwd.hpp>

#include <core/Constants.hpp>

#include <type_traits>

namespace hyperion {

namespace dotnet {
class Object;
class Class;
} // namespace dotnet

class ScriptObjectResource;

enum class HypClassFlags : uint8;
enum class HypClassAllocationMethod : uint8;

class HypObjectContainerBase;
class HypClass;
class HypObjectBase;
struct HypObjectHeader;
class IHypMember;
class HypField;
class HypMethod;
class HypConstant;
class HypProperty;

template <class T>
struct Handle;

template <class T>
struct WeakHandle;

template <class T>
struct HypObjectMemory;

template <class T>
struct HypClassRegistration;

template <class T>
struct HypStructRegistration;

template <class T, class T2 = void>
struct IsHypObject;

template <class T>
struct IsHypObject<T, std::enable_if_t<!implementationExists<typename T::HypClassInfo::Type> && implementationExists<T>>>
{
    static constexpr bool value = false;
};

template <class T>
struct IsHypObject<T, std::enable_if_t<implementationExists<typename T::HypClassInfo::Type>>>
{
    static constexpr bool value = true;

    using Type = typename T::HypClassInfo::Type;
};

enum class HypObjectInitializerFlags : uint32
{
    NONE = 0x0,
    SUPPRESS_MANAGED_OBJECT_CREATION = 0x1
};

HYP_MAKE_ENUM_FLAGS(HypObjectInitializerFlags)

struct HypObjectInitializerContext
{
    const HypClass* hypClass = nullptr;
    EnumFlags<HypObjectInitializerFlags> flags = HypObjectInitializerFlags::NONE;
};

class HypObjectPtr
{
public:
    HypObjectPtr()
        : m_ptr(nullptr),
          m_hypClass(nullptr)
    {
    }

    explicit HypObjectPtr(std::nullptr_t)
        : m_ptr(nullptr),
          m_hypClass(nullptr)
    {
    }

    HypObjectPtr(const HypClass* hypClass, void* ptr)
        : m_ptr(ptr),
          m_hypClass(hypClass)
    {
    }

    template <class T, typename = std::enable_if_t<IsHypObject<T>::value>>
    explicit HypObjectPtr(T* ptr)
        : m_ptr(ptr),
          m_hypClass(T::Class())
    {
    }

    HypObjectPtr(const HypObjectPtr& other) = default;
    HypObjectPtr& operator=(const HypObjectPtr& other) = default;
    HypObjectPtr(HypObjectPtr&& other) noexcept = default;
    HypObjectPtr& operator=(HypObjectPtr&& other) noexcept = default;
    ~HypObjectPtr() = default;

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return m_ptr != nullptr && m_hypClass != nullptr;
    }

    HYP_FORCE_INLINE bool operator!() const
    {
        return !m_ptr || !m_hypClass;
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
        return m_ptr != nullptr && m_hypClass != nullptr;
    }

    HYP_FORCE_INLINE const HypClass* GetClass() const
    {
        return m_hypClass;
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
    const HypClass* m_hypClass;
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

#ifdef HYP_DEBUG_MODE
    ThreadId initializerThreadId;
#else
    uint32 count;
#endif
};

template <class T>
struct HypObjectInitializerGuard : HypObjectInitializerGuardBase
{
    HypObjectInitializerGuard(void* ptr)
        : HypObjectInitializerGuardBase(HypObjectPtr(T::Class(), ptr))
    {
    }
};

/// HypClassRef - A strong reference to a HypClass.
/// Reference counting only applies to dynamically created / destroyed HypClass objects (used with scripts).
struct HypClassRef
{
    const HypClass* hypClass;

    HYP_FORCE_INLINE HypClassRef()
        : hypClass(nullptr)
    {
    }

    HYP_API HypClassRef(const HypClass* hypClass, int initialRefCount = 1);

    HYP_API HypClassRef(const HypClassRef& other);
    HYP_API HypClassRef& operator=(const HypClassRef& other);

    HYP_API HypClassRef(HypClassRef&& other) noexcept;
    HYP_API HypClassRef& operator=(HypClassRef&& other) noexcept;

    HYP_API ~HypClassRef();

    HYP_FORCE_INLINE bool IsValid() const
    {
        return hypClass != nullptr;
    }

    HYP_FORCE_INLINE const HypClass* operator->() const
    {
        return hypClass;
    }

    HYP_FORCE_INLINE const HypClass& operator*() const
    {
        return *hypClass;
    }

    HYP_FORCE_INLINE operator const HypClass*() const
    {
        return hypClass;
    }

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return IsValid();
    }

    HYP_FORCE_INLINE bool operator==(const HypClassRef& other) const
    {
        return hypClass == other.hypClass;
    }

    HYP_FORCE_INLINE bool operator!=(const HypClassRef& other) const
    {
        return hypClass != other.hypClass;
    }

    HYP_FORCE_INLINE bool operator<(const HypClassRef& other) const
    {
        return hypClass < other.hypClass;
    }
};

extern HYP_API const HypClass* GetClass(TypeId typeId);

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

    if (IsA(GetClass(handle.GetTypeId()), Other::Class()))
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

    if (IsA(GetClass(handle.GetTypeId()), Other::Class()))
    {
        return reinterpret_cast<WeakHandle<Other>&&>(handle);
    }

    return WeakHandle<Other>::empty;
}

/// IsA() checks ///

// NOTE: These overloads are implemented in HypClass.cpp
extern HYP_API bool IsA(const HypClass* hypClass, const void* ptr, TypeId typeId);
extern HYP_API bool IsA(const HypClass* hypClass, const HypClass* instanceHypClass);

template <class ExpectedType, class InstanceType>
static inline bool IsA()
{
    static_assert(implementationExists<ExpectedType>, "Implementation does not exist for the expected type! Ensure proper headers are included.");
    static_assert(implementationExists<InstanceType>, "Implementation does not exist for the instance type! Ensure proper headers are included.");

    static const HypClass* instanceHypClass = GetClass(TypeId::ForType<InstanceType>());

    if (!instanceHypClass)
    {
        return false;
    }

    static const HypClass* hypClass = GetClass(TypeId::ForType<ExpectedType>());

    if (!hypClass)
    {
        return false;
    }

    // Short-circuit with compile time checking
    if constexpr (std::is_same_v<ExpectedType, InstanceType> || std::is_base_of_v<ExpectedType, InstanceType>)
    {
        return true;
    }

    static const bool cachedCheck = ::hyperion::IsA(hypClass, instanceHypClass);

    return cachedCheck || IsA(hypClass, instanceHypClass);
}

template <class ExpectedType>
static inline bool IsA(const HypClass* instanceHypClass)
{
    if (!instanceHypClass)
    {
        return false;
    }

    const HypClass* hypClass = GetClass(TypeId::ForType<ExpectedType>());

    if (!hypClass)
    {
        return false;
    }

    return IsA(hypClass, instanceHypClass);
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
