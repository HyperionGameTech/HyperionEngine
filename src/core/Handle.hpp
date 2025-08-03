/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/object/ObjId.hpp>
#include <core/object/ObjectPool.hpp>

#include <core/object/HypObjectBase.hpp>

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

/*! \brief A Handle is a strong reference to an object allocated in the Object Pool. All Handles are reference counted and will automatically
 *  release the object when the last reference is destroyed.
 *  \tparam T The type of object that the handle is referencing. Must be a subclass of HypObjectBase.
 */
template <class T>
struct Handle final : HandleBase
{
    using IdType = ObjId<T>;

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

    /*! \brief Construct a handle from the given Id. Use only if you have an Id for an object that is guaranteed to exist.
     *  \param id The Id of the object to reference. */
    explicit Handle(IdType id)
        : ptr(nullptr)
    {
        if (id.IsValid())
        {
            ObjectContainerBase* container = ObjectPool::GetObjectContainerMap().TryGet(id.GetTypeId());

            // This really shouldn't happen unless we're doing something wrong.
            // We shouldn't have an Id for a type that doesn't have a container.
            HYP_CORE_ASSERT(container != nullptr,
                "Container is not initialized for type! Possibly using an Id created without pointing to a valid object with TypeId %u?",
                id.GetTypeId().Value());

            HypObjectHeader* header = container->GetObjectHeader(id.ToIndex());
            HYP_CORE_ASSERT(header != nullptr);

            ptr = container->GetObjectPointer(header);
            HYP_CORE_ASSERT(ptr != nullptr);

            HYP_CORE_ASSERT(ptr->m_header->GetRefCountStrong() > 0, "Object is no longer alive!");

            // If strong count == 1 after incrementing, the object has already been destructed and it is invalid to create a strong reference
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

    /*! \see Id() */
    HYP_FORCE_INLINE operator IdType() const
    {
        return ptr != nullptr ? IdType(ObjIdBase { ptr->m_header->container->GetObjectTypeId(), ptr->m_header->index + 1 }) : IdType();
    }

    /*! \brief Get the runtime ID of the object that the handle is referencing.
     *  \return The runtime ID of the object. */
    HYP_FORCE_INLINE IdType Id() const
    {
        return IdType(*this);
    }

    /*! \brief Get the TypeId of the object that the handle is referencing. If the handle is null, it returns the TypeId of T.
     *  otherwise, it returns the TypeId of the object that the handle is referencing, which can be different from T if the object is a derived type.
     *  \return The TypeId of the object. */
    HYP_FORCE_INLINE const TypeId& GetTypeId() const
    {
        static const TypeId typeId = TypeId::ForType<T>();

        return ptr ? ptr->m_header->container->GetObjectTypeId() : typeId;
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
        return IdType(*this) < IdType(other);
    }

    HYP_FORCE_INLINE bool operator==(const IdType& id) const
    {
        return IdType(*this) == id;
    }

    HYP_FORCE_INLINE bool operator!=(const IdType& id) const
    {
        return IdType(*this) != id;
    }

    HYP_FORCE_INLINE bool operator<(const IdType& id) const
    {
        return IdType(*this) < id;
    }

    /*! \brief Check if the handle is valid. A handle is valid if its index is greater than 0.
     *  \return True if the handle is valid. */
    HYP_FORCE_INLINE bool IsValid() const
    {
        return ptr != nullptr;
    }

    /*! \brief Get a pointer to the object that the handle is referencing.
     *  \return A pointer to the object. */
    HYP_FORCE_INLINE T* Get() const &
    {
        return static_cast<T*>(ptr);
    }

    HYP_FORCE_INLINE operator T* const() const &
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
    HYP_FORCE_INLINE Handle<Ty> Cast() const&
    {
        return reinterpret_cast<const Handle<Ty>&>(*this);
    }

    template <class Ty, std::enable_if_t<!std::is_same_v<Ty, T> && std::is_convertible_v<std::add_pointer_t<T>, std::add_pointer_t<Ty>>, int> = 0>
    HYP_FORCE_INLINE Handle<Ty> Cast() &&
    {
        return reinterpret_cast<Handle<Ty>&&>(*this);
    }

    template <class Ty, std::enable_if_t<!std::is_same_v<Ty, T> && (!std::is_convertible_v<std::add_pointer_t<T>, std::add_pointer_t<Ty>> && std::is_base_of_v<T, Ty>), int> = 0>
    HYP_FORCE_INLINE Handle<Ty> Cast() const&
    {
        if (ptr)
        {
            const bool instanceOfCheck = IsA(Ty::Class(), ptr, ptr->m_header->container->GetObjectTypeId());
            HYP_CORE_ASSERT(instanceOfCheck, "Cannot cast Handle<T> to Handle<Ty> because T is not a base class of Ty!");
        }

        return reinterpret_cast<const Handle<Ty>&>(*this);
    }

    template <class Ty, std::enable_if_t<!std::is_same_v<Ty, T> && (!std::is_convertible_v<std::add_pointer_t<T>, std::add_pointer_t<Ty>> && std::is_base_of_v<T, Ty>), int> = 0>
    HYP_FORCE_INLINE Handle<Ty> Cast() &&
    {
        if (ptr)
        {
            const bool instanceOfCheck = IsA(Ty::Class(), ptr, ptr->m_header->container->GetObjectTypeId());
            HYP_CORE_ASSERT(instanceOfCheck, "Cannot cast Handle<T> to Handle<Ty> because T is not a base class of Ty!");
        }

        return reinterpret_cast<Handle<Ty>&&>(*this);
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
            const bool instanceOfCheck = IsA(Ty::Class(), ptr, ptr->m_header->container->GetObjectTypeId());
            HYP_CORE_ASSERT(instanceOfCheck, "Cannot cast Handle<T> to Handle<Ty> because T is not a base class of Ty!");
        }

        return reinterpret_cast<const Handle<Ty>&>(*this);
    }

    template <class Ty, std::enable_if_t<!std::is_same_v<Ty, T> && (!std::is_convertible_v<std::add_pointer_t<T>, std::add_pointer_t<Ty>> && std::is_base_of_v<T, Ty>), int> = 0>
    HYP_FORCE_INLINE explicit operator Handle<Ty>() &&
    {
        if (ptr)
        {
            const bool instanceOfCheck = IsA(Ty::Class(), ptr, ptr->m_header->container->GetObjectTypeId());
            HYP_CORE_ASSERT(instanceOfCheck, "Cannot cast Handle<T> to Handle<Ty> because T is not a base class of Ty!");
        }

        return reinterpret_cast<Handle<Ty>&&>(*this);
    }

    HYP_FORCE_INLINE WeakHandle<T> ToWeak() const
    {
        return WeakHandle<T>(*this);
    }

    HYP_FORCE_INLINE AnyRef ToRef() const
    {
        return AnyRef { GetTypeId(), ptr };
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return Id().GetHashCode();
    }

    static Handle FromPointer(HypObjectBase* ptr)
    {
        Handle<T> handle;
        handle.ptr = ptr;

        if (ptr)
        {
            if (!IsA(GetClass(TypeId::ForType<T>()), ptr, ptr->m_header->container->GetObjectTypeId()))
            {
                HYP_FAIL("Cannot create WeakHandle because it is not a base class of T!");
            }

            ptr->m_header->IncRefStrong();
        }

        return handle;
    }
};

template <class T>
struct WeakHandle final
{
    using IdType = ObjId<T>;

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

    /*! \brief Construct a WeakHandle from the given Id.
     *  \param id The Id of the object to reference. */
    explicit WeakHandle(IdType id)
        : ptr(nullptr)
    {
        if (id.IsValid())
        {
            ObjectContainerBase* container = ObjectPool::GetObjectContainerMap().TryGet(id.GetTypeId());

            // This really shouldn't happen unless we're doing something wrong.
            // We shouldn't have an Id for a type that doesn't have a container.
            HYP_CORE_ASSERT(container != nullptr,
                "Container is not initialized for type! Possibly using an Id created without pointing to a valid object with TypeId %u?",
                id.GetTypeId().Value());

            HypObjectHeader* header = container->GetObjectHeader(id.ToIndex());
            HYP_CORE_ASSERT(header != nullptr);

            ptr = container->GetObjectPointer(header);
            HYP_CORE_ASSERT(ptr != nullptr);

            // All HypObjectBase types have an initial weak count of 1 which gets incremented when the object is created and decremented in the destructor of HypObjectBase.
            // If it is zero, it means the object is not only no longer alive - but that the Id is totally invalid and would sometimes point to the wrong object!
            HYP_CORE_ASSERT(header->GetRefCountWeak() > 0, "Object overwriting detected! This is likely due to attempting to create a WeakHandle from an Id that is no longer valid or has been reused for another object.");

            header->IncRefWeak();
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

        /// \FIXME: Potential race condition. What if the object is destroyed while we are locking it?
        /// we should instead increment the strong reference count and then check if it is still alive. (we'll need to update the semantics around HypObject_OnIncRefCount_Strong first)
        return ptr->m_header->refCountStrong.Get(MemoryOrder::ACQUIRE) != 0
            ? Handle<T>::FromPointer(ptr)
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

    /*! \see Id() */
    HYP_FORCE_INLINE operator IdType() const
    {
        return ptr != nullptr ? IdType(ObjIdBase { ptr->m_header->container->GetObjectTypeId(), ptr->m_header->index + 1 }) : IdType();
    }

    /*! \brief Get a referenceable Id for the object that the weak handle is referencing.
     *  \return The Id of the object. */
    HYP_FORCE_INLINE IdType Id() const
    {
        return IdType(*this);
    }

    /*! \brief Get the TypeId of the object that the handle is referencing. If the handle is null, it returns the TypeId of T.
     *  otherwise, it returns the TypeId of the object that the handle is referencing, which can be different from T if the object is a derived type.
     *  \return The TypeId of the object. */
    HYP_FORCE_INLINE const TypeId& GetTypeId() const
    {
        static const TypeId typeId = TypeId::ForType<T>();

        return ptr ? ptr->m_header->container->GetObjectTypeId() : typeId;
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
        return IdType(*this) < IdType(other);
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
        return IdType(*this) < IdType(other);
    }

    HYP_FORCE_INLINE bool operator==(const IdType& id) const
    {
        return IdType(*this) == id;
    }

    HYP_FORCE_INLINE bool operator!=(const IdType& id) const
    {
        return IdType(*this) != id;
    }

    HYP_FORCE_INLINE bool operator<(const IdType& id) const
    {
        return IdType(*this) < id;
    }

    HYP_FORCE_INLINE bool IsValid() const
    {
        return ptr != nullptr;
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
            const bool instanceOfCheck = IsA(Ty::Class(), ptr, ptr->m_header->container->GetObjectTypeId());
            HYP_CORE_ASSERT(instanceOfCheck, "Cannot cast WeakHandle<T> to WeakHandle<Ty> because T is not a base class of Ty!");
        }

        return reinterpret_cast<const WeakHandle<Ty>&>(*this);
    }

    template <class Ty, std::enable_if_t<!std::is_same_v<Ty, T> && (!std::is_convertible_v<std::add_pointer_t<T>, std::add_pointer_t<Ty>> && std::is_base_of_v<T, Ty>), int> = 0>
    HYP_FORCE_INLINE explicit operator WeakHandle<Ty>() &&
    {
        if (ptr)
        {
            const bool instanceOfCheck = IsA(Ty::Class(), ptr, ptr->m_header->container->GetObjectTypeId());
            HYP_CORE_ASSERT(instanceOfCheck, "Cannot cast WeakHandle<T> to WeakHandle<Ty> because T is not a base class of Ty!");
        }

        return reinterpret_cast<WeakHandle<Ty>&&>(*this);
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return Id().GetHashCode();
    }

    static WeakHandle FromPointer(T* ptr)
    {
        WeakHandle<T> handle;
        handle.ptr = ptr;

        if (ptr)
        {
            if (!IsA(GetClass(TypeId::ForType<T>()), ptr, ptr->m_header->container->GetObjectTypeId()))
            {
                HYP_FAIL("Cannot create WeakHandle because it is not a base class of T!");
            }

            ptr->m_header->IncRefWeak();
        }

        return handle;
    }
};

/*! \brief A dynamic Handle type. Type is stored at runtime instead of compile time.
 *  An AnyHandle is able to be punned to a Handle<T> permitted that T is the actual type of the held object
 *  \todo Deprecate in favour of Handle<HypObjectBase>. */
struct AnyHandle final
{
    using IdType = ObjIdBase;

    HypObjectBase* ptr;
    TypeId typeId;

public:
    static const AnyHandle empty;

    AnyHandle()
        : ptr(nullptr),
          typeId(TypeId::Void())
    {
    }

    HYP_API explicit AnyHandle(HypObjectBase* hypObjectPtr);
    HYP_API AnyHandle(const HypClass* hypClass, HypObjectBase* ptr);

    template <class T, typename = std::enable_if_t<std::is_base_of_v<HypObjectBase, T> && !std::is_same_v<HypObjectBase, T>>>
    explicit AnyHandle(T* ptr)
        : ptr(static_cast<HypObjectBase*>(ptr)),
          typeId(this->ptr != nullptr ? this->ptr->m_header->container->GetObjectTypeId() : TypeId::ForType<T>())
    {
        if (IsValid())
        {
            this->ptr->m_header->IncRefStrong();
        }
    }

    template <class T>
    AnyHandle(const Handle<T>& handle)
        : ptr(handle.ptr),
          typeId(this->ptr != nullptr ? this->ptr->m_header->container->GetObjectTypeId() : TypeId::ForType<T>())
    {
        if (handle.IsValid())
        {
            ptr->m_header->IncRefStrong();
        }
    }

    template <class T>
    AnyHandle(Handle<T>&& handle)
        : ptr(handle.ptr),
          typeId(this->ptr != nullptr ? this->ptr->m_header->container->GetObjectTypeId() : TypeId::ForType<T>())
    {
        handle.ptr = nullptr;
    }

    template <class T>
    explicit AnyHandle(ObjId<T> id)
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
        return Id() < other.Id();
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

    HYP_FORCE_INLINE bool operator==(const ObjIdBase& id) const
    {
        return Id() == id;
    }

    HYP_FORCE_INLINE bool operator!=(const ObjIdBase& id) const
    {
        return Id() != id;
    }

    HYP_FORCE_INLINE bool operator<(const ObjIdBase& id) const
    {
        return Id() < id;
    }

    HYP_FORCE_INLINE bool IsValid() const
    {
        return ptr != nullptr;
    }

    HYP_FORCE_INLINE HypObjectBase* Get() const
    {
        return ptr;
    }

    /*! \brief Get a referenceable Id for the object that the handle is referencing.
     *  \return The Id of the object. */
    HYP_API IdType Id() const;

    /*! \brief Get the TypeId for this handle type
     *  \return The TypeId for the handle */
    HYP_API TypeId GetTypeId() const;

    template <class T>
    HYP_FORCE_INLINE bool Is() const
    {
        constexpr TypeId otherTypeId = TypeId::ForType<T>();

        return typeId == otherTypeId
            || IsA(GetClass(otherTypeId), ptr, typeId);
    }

    template <class T>
    HYP_NODISCARD Handle<T> Cast() const
    {
        if (!Is<T>())
        {
            return {};
        }

        return Handle<T>::FromPointer(ptr);
    }

    template <class T>
    HYP_NODISCARD HYP_FORCE_INLINE explicit operator const Handle<T>&() const&
    {
        static_assert(offsetof(Handle<T>, ptr) == 0, "Handle<T> must have ptr as first member");

        if (ptr != nullptr)
        {
            HYP_CORE_ASSERT(Is<T>(), "Cannot cast AnyHandle to Handle<T> because the type does not match or is not a base class of T!");
        }

        // This is "safe" because the typeId is checked above
        // It is hacky, but we ensure that the structs are the same size and the pointer to HypObjectHeader is the first member
        return reinterpret_cast<const Handle<T>&>(*this);
    }

    template <class T>
    HYP_NODISCARD HYP_FORCE_INLINE explicit operator Handle<T>() &&
    {
        static_assert(offsetof(Handle<T>, ptr) == 0, "Handle<T> must have ptr as first member");

        if (ptr != nullptr)
        {
            HYP_CORE_ASSERT(Is<T>(), "Cannot cast AnyHandle to Handle<T> because the type does not match or is not a base class of T!");
        }

        // This is "safe" because the typeId is checked above
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
    ObjectContainer<T>& container = ObjectPool::GetObjectContainerMap().GetOrCreate<T>();

    HypObjectMemory<T>* header = container.Allocate();

    T* ptr = header->storage.GetPointer();

    Memory::ConstructWithContext<T, HypObjectInitializerGuard<T>>(ptr, std::forward<Args>(args)...);

    Handle<T> handle;
    handle.ptr = static_cast<HypObjectBase*>(ptr);

    return handle;
}

template <class T>
inline bool InitObject(const Handle<T>& handle)
{
    // provide a better error message for attempting to initialize incomplete types.
    if constexpr (!implementationExists<T>)
    {
        static_assert(implementationExists<T>, "Cannot initialize an incomplete type. Make sure the type is fully defined before calling InitObject.");
    }
    else if constexpr (!std::is_base_of_v<HypObjectBase, T>)
    {
        static_assert(std::is_base_of_v<HypObjectBase, T>, "Cannot initialize a type that does not derive from HypObjectBase.");
    }

    if (!handle)
    {
        return false;
    }

    HypObjectBase* basePtr = static_cast<HypObjectBase*>(handle.Get());

    if (basePtr->m_initState.BitOr(HypObjectBase::INIT_STATE_INIT_CALLED, MemoryOrder::ACQUIRE_RELEASE) & HypObjectBase::INIT_STATE_INIT_CALLED)
    {
        // Already initialized
        return true;
    }

    HYP_CORE_ASSERT(!basePtr->IsReady());

    basePtr->Init_Internal();

    return true;
}

} // namespace hyperion
