/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/utilities/EnumFlags.hpp>
#include <Core/utilities/FormatFwd.hpp>

#include <Core/reflection/ObjectMacros.hpp>

#include <Core/Defines.hpp>
#include <Core/Constants.hpp>

#include <type_traits>

namespace Hyperion {

namespace dotnet {
class ManagedObject;
class ManagedClass;
} // namespace dotnet

class ScriptObjectResource;

namespace utilities {

struct TypeId;

template <class T>
extern const TypeId& TypeIdOf();

} // namespace utilities

using utilities::TypeId;
using utilities::TypeIdOf;

enum class ClassFlags : uint8;
enum class ClassAllocationMethod : uint8;

class ObjectContainerBase;
class ObjectBase;
struct ObjectHeader;
class IMember;
class Field;
class Method;
class StaticField;
class Property;

template <class T>
struct Handle;

template <class T>
struct WeakHandle;

template <class T>
struct ClassDecl;

template <class T, class T2 = void>
struct IsObject;

template <class T, class T2>
struct IsObject
{
    static_assert(implementation_exists_v<T>, "Cannot use IsObject with undefined type!");

    static constexpr bool value = false;
};

template <class T>
struct IsObject<T, std::enable_if_t<implementation_exists_v<typename T::ClassInfo::Type>>>
{
    static constexpr bool value = true;

    using Type = typename T::ClassInfo::Type;
};

template <class T>
constexpr bool IsObjectV = IsObject<T>::value;

enum class ObjectInitializerFlags : uint32
{
    NONE = 0x0,
    SUPPRESS_MANAGED_OBJECT_CREATION = 0x1
};

HYP_MAKE_ENUM_FLAGS(ObjectInitializerFlags)

struct ObjectInitializerContext
{
    const Class* cls = nullptr;
    EnumFlags<ObjectInitializerFlags> flags = ObjectInitializerFlags::NONE;
};

class TypedObjPtr
{
public:
    TypedObjPtr()
        : m_ptr(nullptr),
          m_class(nullptr)
    {
    }

    explicit TypedObjPtr(std::nullptr_t)
        : m_ptr(nullptr),
          m_class(nullptr)
    {
    }

    TypedObjPtr(const Class* cls, void* ptr)
        : m_ptr(ptr),
          m_class(cls)
    {
    }

    template <class T, typename = std::enable_if_t<IsObjectV<T>>>
    explicit TypedObjPtr(T* ptr)
        : m_ptr(ptr),
          m_class(T::StaticClass())
    {
    }

    TypedObjPtr(const TypedObjPtr& other) = default;
    TypedObjPtr& operator=(const TypedObjPtr& other) = default;
    TypedObjPtr(TypedObjPtr&& other) noexcept = default;
    TypedObjPtr& operator=(TypedObjPtr&& other) noexcept = default;
    ~TypedObjPtr() = default;

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

    HYP_FORCE_INLINE bool operator==(const TypedObjPtr& other) const
    {
        return m_ptr == other.m_ptr;
    }

    HYP_FORCE_INLINE bool operator!=(const TypedObjPtr& other) const
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

struct ObjectInitializerGuardBase
{
    HYP_API ObjectInitializerGuardBase(TypedObjPtr ptr);
    HYP_API ~ObjectInitializerGuardBase();

    TypedObjPtr ptr;
    uint32 count;
};

template <class T>
struct ObjectInitializerGuard : ObjectInitializerGuardBase
{
    ObjectInitializerGuard(void* ptr)
        : ObjectInitializerGuardBase(TypedObjPtr(T::StaticClass(), ptr))
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
#if HYP_MSVC
    // If you get an unresolved external,
    // it means that T does not have Class info generated for it. Ensure that
    // the build tool was run and that the generated files are included in the build.
    return GetClassHelper<T>::Get();
#else
    static const Class* s_cls = GetClass(TypeIdOf<T>());
    return s_cls;
#endif
}


/// Casts ///

template <class Other, class T>
static inline Other* StaticCast(T* objectPtr)
{
    static_assert(std::is_class_v<Other>, "Other must be a class type to use with StaticCast");

    if constexpr (std::is_same_v<T, Other> || std::is_base_of_v<Other, T> || std::is_base_of_v<T, Other>)
    {
        return static_cast<Other*>(objectPtr);
    }
    else
    {
        static_assert(always_fail_v<T>, "Invalid StaticCast");
    }
}

template <class Other, class T>
static inline const Other* StaticCast(const T* objectPtr)
{
    static_assert(std::is_class_v<Other>, "Other must be a class type to use with StaticCast");

    if constexpr (std::is_same_v<T, Other> || std::is_base_of_v<Other, T> || std::is_base_of_v<T, Other>)
    {
        return static_cast<const Other*>(objectPtr);
    }
    else
    {
        static_assert(always_fail_v<T>, "Invalid StaticCast");
    }
}

template <class Other, class T>
static inline const Handle<Other>& StaticCast(const Handle<T>& handle)
{
    static_assert(std::is_class_v<Other>, "Other must be a class type to use with StaticCast");

    if constexpr (std::is_same_v<T, Other> || std::is_base_of_v<Other, T> || std::is_base_of_v<T, Other>)
    {
        return reinterpret_cast<const Handle<Other>&>(handle);
    }
    else
    {
        static_assert(always_fail_v<T>, "Invalid StaticCast");
    }
}

template <class Other, class T>
static inline Handle<Other> StaticCast(Handle<T>&& handle)
{
    static_assert(std::is_class_v<Other>, "Other must be a class type to use with StaticCast");

    if constexpr (std::is_same_v<T, Other> || std::is_base_of_v<Other, T> || std::is_base_of_v<T, Other>)
    {
        return reinterpret_cast<Handle<Other>&&>(handle);
    }
    else
    {
        static_assert(always_fail_v<T>, "Invalid StaticCast");
    }
}

template <class Other, class T>
static inline const WeakHandle<Other>& StaticCast(const WeakHandle<T>& handle)
{
    static_assert(std::is_class_v<Other>, "Other must be a class type to use with StaticCast");

    if constexpr (std::is_same_v<T, Other> || std::is_base_of_v<Other, T> || std::is_base_of_v<T, Other>)
    {
        return reinterpret_cast<const WeakHandle<Other>&>(handle);
    }
    else
    {
        static_assert(always_fail_v<T>, "Invalid StaticCast");
    }
}

template <class Other, class T>
static inline WeakHandle<Other> StaticCast(WeakHandle<T>&& handle)
{
    static_assert(std::is_class_v<Other>, "Other must be a class type to use with StaticCast");

    if constexpr (std::is_same_v<T, Other> || std::is_base_of_v<Other, T> || std::is_base_of_v<T, Other>)
    {
        return reinterpret_cast<WeakHandle<Other>&&>(handle);
    }
    else
    {
        static_assert(always_fail_v<T>, "Invalid StaticCast");
    }
}

template <class Other, class T>
static inline Other* DynamicCast(T* objectPtr)
{
    static_assert(std::is_class_v<Other>, "Other must be a class type to use with DynamicCast");

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
static inline const Other* DynamicCast(const T* objectPtr)
{
    static_assert(std::is_class_v<Other>, "Other must be a class type to use with DynamicCast");

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
static inline const Handle<Other>& DynamicCast(const Handle<T>& handle)
{
    static_assert(std::is_class_v<Other>, "Other must be a class type to use with DynamicCast");

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
static inline Handle<Other> DynamicCast(Handle<T>&& handle)
{
    static_assert(std::is_class_v<Other>, "Other must be a class type to use with DynamicCast");

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
static inline const WeakHandle<Other>& DynamicCast(const WeakHandle<T>& handle)
{
    static_assert(std::is_class_v<Other>, "Other must be a class type to use with DynamicCast");

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
static inline WeakHandle<Other> DynamicCast(WeakHandle<T>&& handle)
{
    static_assert(std::is_class_v<Other>, "Other must be a class type to use with DynamicCast");

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
    static_assert(implementation_exists_v<ExpectedType>, "Implementation does not exist for the expected type! Ensure proper headers are included.");
    static_assert(implementation_exists_v<InstanceType>, "Implementation does not exist for the instance type! Ensure proper headers are included.");

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

    static const bool cachedCheck = ::Hyperion::IsA(s_expectedClass, s_instanceClass);

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
    return ::Hyperion::IsA<ExpectedType, InstanceType>() || instance->template IsA<ExpectedType>();
}

template <class ExpectedType, class InstanceType, typename = std::enable_if_t<!std::is_pointer_v<InstanceType>>>
static inline bool IsA(const InstanceType& instance)
{
    // first check caches w/ static
    // second uses the actual instance type (can be more derived)
    return ::Hyperion::IsA<ExpectedType, InstanceType>() || instance.template IsA<ExpectedType>();
}

} // namespace Hyperion
