/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

#include <core/utilities/TypeId.hpp>
#include <core/utilities/EnumFlags.hpp>

#include <core/object/ObjId.hpp>

#ifdef HYP_DEBUG_MODE
#include <core/threading/Threads.hpp>
#endif

#include <core/Constants.hpp>

#include <type_traits>

namespace hyperion {

namespace dotnet {
class Object;
class Class;
} // namespace dotnet

enum class HypClassAllocationMethod : uint8;

class ObjectContainerBase;
class HypClass;
class HypObjectBase;

template <class T>
class HypObject;

template <class T>
struct Handle;

template <class T>
struct WeakHandle;

class IHypObjectInitializer;

struct HypObjectHeader;

template <class T>
struct HypObjectMemory;

class ManagedObjectResource;

enum class HypClassFlags : uint32;

extern HYP_API const HypClass* GetClass(TypeId typeId);

HYP_API extern void FixupObjectInitializerPointer(void* target, IHypObjectInitializer* initializer);

class IHypObjectInitializer
{
public:
    virtual ~IHypObjectInitializer() = default;

    virtual TypeId GetTypeId() const = 0;

    virtual const HypClass* GetClass() const = 0;

    virtual void IncRef(void* _this, bool weak) const = 0;
    virtual void DecRef(void* _this, bool weak) const = 0;

    virtual uint32 GetRefCount_Strong(void* _this) const = 0;
    virtual uint32 GetRefCount_Weak(void* _this) const = 0;
};

template <class T, class T2 = void>
struct IsHypObject
{
    static constexpr bool value = false;
};

/*! \brief A type trait to determine if a type is a HypObject (has a HypClass associated with it).
 *  \note A type is considered a HypObject if it is derived from HypObjectBase or if it has HYP_OBJECT_BODY(...) in the class body.
 *  \tparam T The type to check. */
template <class T>
struct IsHypObject<T, std::enable_if_t<std::is_base_of_v<HypObjectBase, T> || (T::HypObjectData::isHypObject)>>
{
    static constexpr bool value = true;

    using Type = typename T::HypObjectData::Type;
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

    HypObjectPtr(std::nullptr_t)
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
    HypObjectPtr(T* ptr)
        : m_ptr(ptr),
          m_hypClass(GetHypClass(TypeId::ForType<typename IsHypObject<T>::Type>()))
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

    HYP_API IHypObjectInitializer* GetObjectInitializer() const;

    HYP_API uint32 GetRefCount_Strong() const;
    HYP_API uint32 GetRefCount_Weak() const;

    HYP_API void IncRef(bool weak = false);
    HYP_API void DecRef(bool weak = false);

private:
    HYP_API const HypClass* GetHypClass(TypeId typeId) const;

    void* m_ptr;
    const HypClass* m_hypClass;
};

HYP_API void HypObject_OnIncRefCount_Strong(HypObjectPtr ptr, uint32 count);
HYP_API void HypObject_OnDecRefCount_Strong(HypObjectPtr ptr, uint32 count);
HYP_API void HypObject_OnIncRefCount_Weak(HypObjectPtr ptr, uint32 count);
HYP_API void HypObject_OnDecRefCount_Weak(HypObjectPtr ptr, uint32 count);

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
        : HypObjectInitializerGuardBase(HypObjectPtr(GetClassAndEnsureValid(), ptr))
    {
    }

    static const HypClass* GetClassAndEnsureValid()
    {
        using HypObjectType = typename IsHypObject<T>::Type;
        static const HypClass* hypClass = GetClass(TypeId::ForType<HypObjectType>());

        HYP_CORE_ASSERT(hypClass != nullptr, "HypClass not registered for type %s", TypeNameWithoutNamespace<T>().Data());

        return hypClass;
    }
};

template <class T>
struct HypClassRegistration;

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
