/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_HANDLE_HPP
#define HYPERION_CORE_HANDLE_HPP

#include <core/ID.hpp>
#include <core/ObjectPool.hpp>

#include <core/object/HypObject.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <core/Util.hpp>

namespace hyperion {

template <class T>
struct Handle;

template <class T>
struct WeakHandle;

struct AnyHandle;

struct HandleBase
{
};

/*! \brief Definition of a handle for a type.
 *  \details In Hyperion, a handle is a reference to an instance of a specific core engine object type.
 *  These objects are use memory pools for each type used, and are reference counted.
 *  \tparam T The type of object that the handle is referencing.
 */
template <class T>
struct Handle final : HandleBase
{
    using IDType = ID<T>;

    friend struct AnyHandle;
    friend struct WeakHandle<T>;

    static const Handle empty;

    HypObjectBase* ptr;

    Handle()
        : ptr(nullptr)
    {
    }

    Handle(std::nullptr_t)
        : Handle()
    {
    }

    explicit Handle(HypObjectBase* ptr)
        : ptr(ptr)
    {
        if (ptr)
        {
            ptr->m_header->IncRefStrong();
        }
    }

    /*! \brief Construct a handle from the given ID. Use only if you have an ID for an object that is guaranteed to exist.
     *  \param id The ID of the object to reference. */
    explicit Handle(IDType id)
        : ptr(nullptr)
    {
        if (id.IsValid())
        {
            IObjectContainer* container = ObjectPool::GetObjectContainerHolder().TryGet(id.GetTypeID());

            // This really shouldn't happen unless we're doing something wrong.
            // We shouldn't have an ID for a type that doesn't have a container.
            AssertThrowMsg(container != nullptr, "Container is not initialized for type!");

            HypObjectMemory<T>* header = static_cast<HypObjectMemory<T>*>(container->GetObjectHeader(id.ToIndex()));

            ptr = static_cast<HypObjectBase*>(header->GetPointer());
            AssertThrow(ptr != nullptr);

            // If strong count == 1 after incrementing, the object has already been destructed and it is invalid to create a strong reference
            const uint32 strong_count = header->IncRefStrong();
            AssertThrowMsg(strong_count > 0, "Object is no longer alive!");
        }
    }

    template <class TPointerType, typename = std::enable_if_t<IsHypObject<TPointerType>::value && std::is_convertible_v<TPointerType*, T*>>>
    explicit Handle(TPointerType* value)
        : ptr(value ? static_cast<HypObjectBase*>(static_cast<T*>(value)) : nullptr)
    {
        if (IsValid())
        {
            ptr->m_header->IncRefStrong();
        }
    }

    explicit Handle(HypObjectMemory<T>* value)
        : ptr(value ? static_cast<HypObjectBase*>(value->GetPointer()) : nullptr)
    {
        if (IsValid())
        {
            ptr->m_header->IncRefStrong();
        }
    }

    Handle(const Handle& other)
        : ptr(other.ptr)
    {
        if (IsValid())
        {
            ptr->m_header->IncRefStrong();
        }
    }

    Handle& operator=(const Handle& other)
    {
        if (this == &other || ptr == other.ptr)
        {
            return *this;
        }

        if (ptr)
        {
            ptr->m_header->DecRefStrong();
        }

        ptr = other.ptr;

        if (ptr)
        {
            ptr->m_header->IncRefStrong();
        }

        return *this;
    }

    Handle(Handle&& other) noexcept
        : ptr(other.ptr)
    {
        other.ptr = nullptr;
    }

    Handle& operator=(Handle&& other) noexcept
    {
        if (this == &other || ptr == other.ptr)
        {
            return *this;
        }

        if (ptr)
        {
            ptr->m_header->DecRefStrong();
        }

        ptr = other.ptr;
        other.ptr = nullptr;

        return *this;
    }

    ~Handle()
    {
        if (ptr)
        {
            ptr->m_header->DecRefStrong();
        }
    }

    HYP_FORCE_INLINE T* operator->() const
    {
        return Get();
    }

    HYP_FORCE_INLINE T& operator*() const
    {
        return *Get();
    }

    HYP_FORCE_INLINE bool operator!() const
    {
        return !IsValid();
    }

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return IsValid();
    }

    HYP_FORCE_INLINE operator IDType() const
    {
        return ptr != nullptr ? IDType(IDBase { ptr->m_header->container->GetObjectTypeID(), ptr->m_header->index + 1 }) : IDType();
    }

    HYP_FORCE_INLINE bool operator==(std::nullptr_t) const
    {
        return !IsValid();
    }

    HYP_FORCE_INLINE bool operator!=(std::nullptr_t) const
    {
        return IsValid();
    }

    HYP_FORCE_INLINE bool operator==(const Handle& other) const
    {
        return ptr == other.ptr;
    }

    HYP_FORCE_INLINE bool operator!=(const Handle& other) const
    {
        return ptr != other.ptr;
    }

    /*! \brief Compare two handles by their index.
     *  \param other The handle to compare to.
     *  \return True if the handle is less than the other handle. */
    HYP_FORCE_INLINE bool operator<(const Handle& other) const
    {
        return IDType(*this) < IDType(other);
    }

    HYP_FORCE_INLINE bool operator==(const IDType& id) const
    {
        return IDType(*this) == id;
    }

    HYP_FORCE_INLINE bool operator!=(const IDType& id) const
    {
        return IDType(*this) != id;
    }

    HYP_FORCE_INLINE bool operator<(const IDType& id) const
    {
        return IDType(*this) < id;
    }

    /*! \brief Check if the handle is valid. A handle is valid if its index is greater than 0.
     *  \return True if the handle is valid. */
    HYP_FORCE_INLINE bool IsValid() const
    {
        return ptr != nullptr;
    }

    /*! \brief Get a referenceable ID for the object that the handle is referencing.
     *  \return The ID of the object. */
    HYP_FORCE_INLINE IDType GetID() const
    {
        return IDType(*this);
    }

    /*! \brief Get a pointer to the object that the handle is referencing.
     *  \return A pointer to the object. */
    HYP_FORCE_INLINE T* Get() const
    {
        return static_cast<T*>(ptr);
    }

    HYP_FORCE_INLINE operator T*() const
    {
        return Get();
    }

    /*! \brief Reset the handle to an empty state.
     *  \details This will decrement the strong reference count of the object that the handle is referencing.
     *  The index is set to 0. */
    HYP_FORCE_INLINE void Reset()
    {
        if (ptr)
        {
            ptr->m_header->DecRefStrong();
        }

        ptr = nullptr;
    }

    template <class Ty, std::enable_if_t<!std::is_same_v<Ty, T> && std::is_convertible_v<std::add_pointer_t<T>, std::add_pointer_t<Ty>>, int> = 0>
    HYP_FORCE_INLINE operator Handle<Ty>() const&
    {
        return reinterpret_cast<const Handle<Ty>&>(*this);
    }

    template <class Ty, std::enable_if_t<!std::is_same_v<Ty, T> && std::is_convertible_v<std::add_pointer_t<T>, std::add_pointer_t<Ty>>, int> = 0>
    HYP_FORCE_INLINE operator Handle<Ty>() &&
    {
        return reinterpret_cast<Handle<Ty>&&>(*this);
    }

    template <class Ty, std::enable_if_t<!std::is_same_v<Ty, T> && (!std::is_convertible_v<std::add_pointer_t<T>, std::add_pointer_t<Ty>> && std::is_base_of_v<T, Ty>), int> = 0>
    HYP_FORCE_INLINE explicit operator Handle<Ty>() const&
    {
        if (ptr)
        {
            const bool instance_of_check = IsInstanceOfHypClass(GetClass(TypeID::ForType<Ty>()), ptr, ptr->m_header->container->GetObjectTypeID());
            AssertThrowMsg(instance_of_check, "Cannot cast Handle<T> to Handle<Ty> because T is not a base class of Ty!");
        }

        return reinterpret_cast<const Handle<Ty>&>(*this);
    }

    template <class Ty, std::enable_if_t<!std::is_same_v<Ty, T> && (!std::is_convertible_v<std::add_pointer_t<T>, std::add_pointer_t<Ty>> && std::is_base_of_v<T, Ty>), int> = 0>
    HYP_FORCE_INLINE explicit operator Handle<Ty>() &&
    {
        if (ptr)
        {
            const bool instance_of_check = IsInstanceOfHypClass(GetClass(TypeID::ForType<Ty>()), ptr, ptr->m_header->container->GetObjectTypeID());
            AssertThrowMsg(instance_of_check, "Cannot cast Handle<T> to Handle<Ty> because T is not a base class of Ty!");
        }

        return reinterpret_cast<Handle<Ty>&&>(*this);
    }

    HYP_FORCE_INLINE WeakHandle<T> ToWeak() const
    {
        return WeakHandle<T>(*this);
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return GetID().GetHashCode();
    }
};

template <class T>
struct WeakHandle final
{
    using IDType = ID<T>;

    static const WeakHandle empty;

    HypObjectBase* ptr;

    WeakHandle()
        : ptr(nullptr)
    {
    }

    WeakHandle(std::nullptr_t)
        : ptr(nullptr)
    {
    }

    explicit WeakHandle(HypObjectBase* ptr)
        : ptr(ptr)
    {
        if (ptr)
        {
            ptr->m_header->IncRefWeak();
        }
    }

    /*! \brief Construct a WeakHandle from the given ID.
     *  \param id The ID of the object to reference. */
    explicit WeakHandle(IDType id)
        : ptr(nullptr)
    {
        if (id.IsValid())
        {
            IObjectContainer* container = ObjectPool::GetObjectContainerHolder().TryGet(id.GetTypeID());
            AssertThrowMsg(container != nullptr, "Container is not initialized for type!");

            HypObjectMemory<T>* header = static_cast<HypObjectMemory<T>*>(container->GetObjectHeader(id.ToIndex()));

            ptr = static_cast<HypObjectBase*>(header->GetPointer());
            AssertThrow(ptr != nullptr);

            // All HypObjectBase types have an initial weak count that gets decremented on destruction.
            // If we hit 1, it means the object is not only no longer alive - but that the ID is totally invalid and would sometimes point to the wrong object!
            const uint32 weak_count = header->IncRefWeak();
            AssertThrowMsg(weak_count > 0, "Object overwriting detected! This is likely due to attempting to create a WeakHandle from an ID that is no longer valid or has been reused for another object.");
        }
    }

    WeakHandle(const Handle<T>& other)
        : ptr(other.ptr)
    {
        if (ptr)
        {
            ptr->m_header->IncRefWeak();
        }
    }

    WeakHandle& operator=(const Handle<T>& other)
    {
        if (ptr == other.ptr)
        {
            return *this;
        }

        if (ptr)
        {
            ptr->m_header->DecRefWeak();
        }

        ptr = other.ptr;

        if (ptr)
        {
            ptr->m_header->IncRefWeak();
        }

        return *this;
    }

    WeakHandle(const WeakHandle& other)
        : ptr(other.ptr)
    {
        if (ptr)
        {
            ptr->m_header->IncRefWeak();
        }
    }

    WeakHandle& operator=(const WeakHandle& other)
    {
        if (ptr == other.ptr)
        {
            return *this;
        }

        if (ptr)
        {
            ptr->m_header->DecRefWeak();
        }

        ptr = other.ptr;

        if (ptr)
        {
            ptr->m_header->IncRefWeak();
        }

        return *this;
    }

    WeakHandle(WeakHandle&& other) noexcept
        : ptr(other.ptr)
    {
        other.ptr = nullptr;
    }

    WeakHandle& operator=(WeakHandle&& other) noexcept
    {
        if (ptr == other.ptr)
        {
            return *this;
        }

        if (ptr)
        {
            ptr->m_header->DecRefWeak();
        }

        ptr = other.ptr;
        other.ptr = nullptr;

        return *this;
    }

    ~WeakHandle()
    {
        if (ptr)
        {
            ptr->m_header->DecRefWeak();
        }
    }

    /*! \brief Lock the weak handle to get a strong reference to the object.
     *  \details If the object is still alive, a strong reference is returned. Otherwise, an empty handle is returned.
     *  \return A strong reference to the object. */
    HYP_NODISCARD HYP_FORCE_INLINE Handle<T> Lock() const
    {
        if (!ptr)
        {
            return Handle<T>();
        }

        /// \todo: Fix this potential race condition. What if the object is destroyed while we are locking it?
        /// we should instead increment the strong reference count and then check if it is still alive. (we'll need to update the semantics around HypObject_OnIncRefCount_Strong first)
        return ptr->m_header->ref_count_strong.Get(MemoryOrder::ACQUIRE) != 0
            ? Handle<T>(static_cast<T*>(ptr))
            : Handle<T>();
    }

    HYP_FORCE_INLINE T* GetUnsafe() const
    {
        return static_cast<T*>(ptr);
    }

    HYP_FORCE_INLINE bool operator!() const
    {
        return !IsValid();
    }

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return IsValid();
    }

    HYP_FORCE_INLINE operator IDType() const
    {
        return ptr != nullptr ? IDType(IDBase { ptr->m_header->container->GetObjectTypeID(), ptr->m_header->index + 1 }) : IDType();
    }

    HYP_FORCE_INLINE bool operator==(std::nullptr_t) const
    {
        return !IsValid();
    }

    HYP_FORCE_INLINE bool operator!=(std::nullptr_t) const
    {
        return IsValid();
    }

    HYP_FORCE_INLINE bool operator==(const WeakHandle& other) const
    {
        return ptr == other.ptr;
    }

    HYP_FORCE_INLINE bool operator!=(const WeakHandle& other) const
    {
        return ptr != other.ptr;
    }

    HYP_FORCE_INLINE bool operator<(const WeakHandle& other) const
    {
        return IDType(*this) < IDType(other);
    }

    HYP_FORCE_INLINE bool operator==(const Handle<T>& other) const
    {
        return ptr == other.ptr;
    }

    HYP_FORCE_INLINE bool operator!=(const Handle<T>& other) const
    {
        return ptr != other.ptr;
    }

    HYP_FORCE_INLINE bool operator<(const Handle<T>& other) const
    {
        return IDType(*this) < IDType(other);
    }

    HYP_FORCE_INLINE bool operator==(const IDType& id) const
    {
        return IDType(*this) == id;
    }

    HYP_FORCE_INLINE bool operator!=(const IDType& id) const
    {
        return IDType(*this) != id;
    }

    HYP_FORCE_INLINE bool operator<(const IDType& id) const
    {
        return IDType(*this) < id;
    }

    HYP_FORCE_INLINE bool IsValid() const
    {
        return ptr != nullptr;
    }

    HYP_FORCE_INLINE IDType GetID() const
    {
        return IDType(*this);
    }

    void Reset()
    {
        if (ptr)
        {
            ptr->m_header->DecRefWeak();
        }

        ptr = nullptr;
    }

    template <class Ty, std::enable_if_t<!std::is_same_v<Ty, T> && std::is_convertible_v<std::add_pointer_t<T>, std::add_pointer_t<Ty>>, int> = 0>
    HYP_FORCE_INLINE operator WeakHandle<Ty>() const&
    {
        return reinterpret_cast<const WeakHandle<Ty>&>(*this);
    }

    template <class Ty, std::enable_if_t<!std::is_same_v<Ty, T> && std::is_convertible_v<std::add_pointer_t<T>, std::add_pointer_t<Ty>>, int> = 0>
    HYP_FORCE_INLINE operator WeakHandle<Ty>() &&
    {
        return reinterpret_cast<WeakHandle<Ty>&&>(*this);
    }

    template <class Ty, std::enable_if_t<!std::is_same_v<Ty, T> && (!std::is_convertible_v<std::add_pointer_t<T>, std::add_pointer_t<Ty>> && std::is_base_of_v<T, Ty>), int> = 0>
    HYP_FORCE_INLINE explicit operator WeakHandle<Ty>() const&
    {
        if (ptr)
        {
            const bool instance_of_check = IsInstanceOfHypClass(GetClass(TypeID::ForType<Ty>()), ptr, ptr->m_header->container->GetObjectTypeID());
            AssertThrowMsg(instance_of_check, "Cannot cast WeakHandle<T> to WeakHandle<Ty> because T is not a base class of Ty!");
        }

        return reinterpret_cast<const WeakHandle<Ty>&>(*this);
    }

    template <class Ty, std::enable_if_t<!std::is_same_v<Ty, T> && (!std::is_convertible_v<std::add_pointer_t<T>, std::add_pointer_t<Ty>> && std::is_base_of_v<T, Ty>), int> = 0>
    HYP_FORCE_INLINE explicit operator WeakHandle<Ty>() &&
    {
        if (ptr)
        {
            const bool instance_of_check = IsInstanceOfHypClass(GetClass(TypeID::ForType<Ty>()), ptr, ptr->m_header->container->GetObjectTypeID());
            AssertThrowMsg(instance_of_check, "Cannot cast WeakHandle<T> to WeakHandle<Ty> because T is not a base class of Ty!");
        }

        return reinterpret_cast<WeakHandle<Ty>&&>(*this);
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return GetID().GetHashCode();
    }
};

/*! \brief A dynamic Handle type. Type is stored at runtime instead of compile time.
 *  An AnyHandle is able to be punned to a Handle<T> permitted that T is the actual type of the held object */
struct AnyHandle final
{
    using IDType = IDBase;

    HypObjectBase* ptr;
    TypeID type_id;

public:
    static const AnyHandle empty;

    AnyHandle()
        : ptr(nullptr),
          type_id(TypeID::Void())
    {
    }

    HYP_API explicit AnyHandle(HypObjectBase* hyp_object_ptr);
    HYP_API AnyHandle(const HypClass* hyp_class, HypObjectBase* ptr);

    template <class T, typename = std::enable_if_t<std::is_base_of_v<HypObjectBase, T> && !std::is_same_v<HypObjectBase, T>>>
    explicit AnyHandle(T* ptr)
        : ptr(static_cast<HypObjectBase*>(ptr)),
          type_id(TypeID::ForType<T>())
    {
        if (IsValid())
        {
            this->ptr->m_header->IncRefStrong();
        }
    }

    template <class T>
    AnyHandle(const Handle<T>& handle)
        : ptr(handle.ptr),
          type_id(TypeID::ForType<T>())
    {
        if (handle.IsValid())
        {
            ptr->m_header->IncRefStrong();
        }
    }

    template <class T>
    AnyHandle(Handle<T>&& handle)
        : ptr(handle.ptr),
          type_id(TypeID::ForType<T>()) // type_id(ptr != nullptr ? ptr->m_header->container->GetObjectTypeID() : TypeID::ForType<T>())
    {
        handle.ptr = nullptr;
    }

    template <class T>
    explicit AnyHandle(ID<T> id)
        : AnyHandle(Handle<T>(id))
    {
    }

    HYP_API AnyHandle(const AnyHandle& other);
    HYP_API AnyHandle& operator=(const AnyHandle& other);

    HYP_API AnyHandle(AnyHandle&& other) noexcept;
    HYP_API AnyHandle& operator=(AnyHandle&& other) noexcept;

    HYP_API ~AnyHandle();

    HYP_FORCE_INLINE bool operator!() const
    {
        return !IsValid();
    }

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return IsValid();
    }

    HYP_FORCE_INLINE bool operator==(std::nullptr_t) const
    {
        return !IsValid();
    }

    HYP_FORCE_INLINE bool operator!=(std::nullptr_t) const
    {
        return IsValid();
    }

    HYP_FORCE_INLINE bool operator==(const AnyHandle& other) const
    {
        return ptr == other.ptr;
    }

    HYP_FORCE_INLINE bool operator!=(const AnyHandle& other) const
    {
        return ptr != other.ptr;
    }

    HYP_FORCE_INLINE bool operator<(const AnyHandle& other) const
    {
        return GetID() < other.GetID();
    }

    template <class T>
    HYP_FORCE_INLINE bool operator==(const Handle<T>& other) const
    {
        return ptr == other.ptr;
    }

    template <class T>
    HYP_FORCE_INLINE bool operator!=(const Handle<T>& other) const
    {
        return ptr != other.ptr;
    }

    HYP_FORCE_INLINE bool operator==(const IDBase& id) const
    {
        return GetID() == id;
    }

    HYP_FORCE_INLINE bool operator!=(const IDBase& id) const
    {
        return GetID() != id;
    }

    HYP_FORCE_INLINE bool operator<(const IDBase& id) const
    {
        return GetID() < id;
    }

    HYP_FORCE_INLINE bool IsValid() const
    {
        return ptr != nullptr;
    }

    HYP_FORCE_INLINE HypObjectBase* Get() const
    {
        return ptr;
    }

    /*! \brief Get a referenceable ID for the object that the handle is referencing.
     *  \return The ID of the object. */
    HYP_API IDType GetID() const;

    /*! \brief Get the TypeID for this handle type
     *  \return The TypeID for the handle */
    HYP_API TypeID GetTypeID() const;

    template <class T>
    HYP_FORCE_INLINE bool Is() const
    {
        constexpr TypeID other_type_id = TypeID::ForType<T>();
        return type_id == other_type_id
            || IsInstanceOfHypClass(GetClass(other_type_id), ptr, type_id);
    }

    template <class T>
    HYP_NODISCARD Handle<T> Cast() const
    {
        if (!Is<T>())
        {
            return {};
        }

        return Handle<T>(static_cast<T*>(ptr));
    }

    template <class T>
    HYP_NODISCARD HYP_FORCE_INLINE explicit operator const Handle<T>&() const&
    {
        static_assert(offsetof(Handle<T>, ptr) == 0, "Handle<T> must have ptr as first member");

        if (ptr != nullptr)
        {
            AssertThrowMsg(Is<T>(), "Cannot cast AnyHandle to Handle<T> because the type does not match or is not a base class of T!");
        }

        // This is "safe" because the type_id is checked above
        // It is hacky, but we ensure that the structs are the same size and the pointer to HypObjectHeader is the first member
        return reinterpret_cast<const Handle<T>&>(*this);
    }

    template <class T>
    HYP_NODISCARD HYP_FORCE_INLINE explicit operator Handle<T>() &&
    {
        static_assert(offsetof(Handle<T>, ptr) == 0, "Handle<T> must have ptr as first member");

        if (ptr != nullptr)
        {
            AssertThrowMsg(Is<T>(), "Cannot cast AnyHandle to Handle<T> because the type does not match or is not a base class of T!");
        }

        // This is "safe" because the type_id is checked above
        // It is hacky, but we ensure that the structs are the same size and the pointer to HypObjectHeader is the first member
        return reinterpret_cast<Handle<T>&&>(std::move(*this));
    }

    HYP_API AnyRef ToRef() const;

    template <class T>
    HYP_FORCE_INLINE T* TryGet() const
    {
        return ToRef().TryGet<T>();
    }

    HYP_API void Reset();

    /*! \brief Sets this handle to null and returns the pointer to the object that was being referenced.
     *  \note The reference count will not be decremented, so the object will still be alive. Thus, creating a new handle from the returned pointer will cause a memory leak as the ref count will be doubly incremented.
     *  \internal For internal use only, used for marshalling objects between C++ and C#.
     *  \return The pointer to the object that was being referenced. */
    HYP_NODISCARD HYP_API void* Release();
};

template <class T>
const Handle<T> Handle<T>::empty = {};

template <class T>
const WeakHandle<T> WeakHandle<T>::empty = {};

template <class T, class... Args>
inline Handle<T> CreateObject(Args&&... args)
{
    ObjectContainer<T>& container = ObjectPool::GetObjectContainerHolder().GetOrCreate<T>();

    HypObjectMemory<T>* header = container.Allocate();

    // Add initial strong reference for the Handle<T> - if the object is alive, it will have at least 1 strong ref.
    header->ref_count_strong.Increment(1, MemoryOrder::RELEASE);

    T* ptr = header->storage.GetPointer();

    Memory::ConstructWithContext<T, HypObjectInitializerGuard<T>>(ptr, std::forward<Args>(args)...);

    Handle<T> handle;
    handle.ptr = static_cast<HypObjectBase*>(ptr);

    return handle;
}

template <class T>
inline bool InitObject(const Handle<T>& handle)
{
    if (!handle)
    {
        return false;
    }

    HypObjectBase* base_ptr = static_cast<HypObjectBase*>(handle.Get());

    if (base_ptr->m_init_state.BitOr(HypObjectBase::INIT_STATE_INIT_CALLED, MemoryOrder::ACQUIRE_RELEASE) & HypObjectBase::INIT_STATE_INIT_CALLED)
    {
        // Already initialized
        return true;
    }

    AssertDebug(!base_ptr->IsReady());

    base_ptr->Init_Internal();

    return true;
}

#define DEF_HANDLE(T)                                                   \
    class T;                                                            \
                                                                        \
    extern IObjectContainer* g_container_ptr_##T;                       \
                                                                        \
    template <>                                                         \
    struct HandleDefinition<T>                                          \
    {                                                                   \
        static constexpr const char* class_name = HYP_STR(T);           \
                                                                        \
        HYP_API static IObjectContainer* GetAllottedContainerPointer(); \
    };

#define DEF_HANDLE_NS(ns, T)                                                   \
    namespace ns {                                                             \
    class T;                                                                   \
    }                                                                          \
                                                                               \
    extern IObjectContainer* g_container_ptr_##T;                              \
                                                                               \
    template <>                                                                \
    struct HandleDefinition<ns::T>                                             \
    {                                                                          \
        static constexpr const char* class_name = HYP_STR(ns) "::" HYP_STR(T); \
                                                                               \
        HYP_API static IObjectContainer* GetAllottedContainerPointer();        \
    };

#include <core/inl/HandleDefinitions.inl>

#undef DEF_HANDLE
#undef DEF_HANDLE_NS

} // namespace hyperion

#endif