/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_HANDLE_HPP
#define HYPERION_CORE_HANDLE_HPP

#include <core/ID.hpp>
#include <core/ObjectPool.hpp>

#include <core/object/HypObjectFwd.hpp>

#include <core/memory/UniquePtr.hpp>
#include <core/memory/NotNullPtr.hpp>

namespace hyperion {

template <class T>
struct Handle;

template <class T>
struct WeakHandle;

struct AnyHandle;

template <class T, bool AllowIncomplete = true>
HYP_FORCE_INLINE static auto GetContainer() -> std::conditional_t< !AllowIncomplete || implementation_exists<T>, ObjectContainer<T> *, IObjectContainer * >
{
    if constexpr (!AllowIncomplete) {
        static_assert(implementation_exists<T>, "Cannot use incomplete type for Handle here");
    }

    // If T is a defined type, we can use ObjectContainer<T> methods (ObjectContainer<T> is final, virtual calls can be optimized)
    if constexpr (!AllowIncomplete || implementation_exists<T>) {
        return static_cast<ObjectContainer<T> *>(&ObjectPool::GetObjectContainerHolder().GetOrCreate<T>());
    } else {
        static IObjectContainer *container = ObjectPool::GetObjectContainerHolder().TryGet(TypeID::ForType<T>());
        AssertThrowMsg(container != nullptr, "Container is not initialized for type");

        return container;
    }
}

struct HandleBase
{
};

/*! \brief Definition of a handle for a type.
 *  \details In Hyperion, a handle is a reference to an instance of a specific core engine object type.
 *  These objects are managed by the ObjectPool and are reference counted.
 *  \note Objects that are managed by the ObjectPool must have a HandleDefinition defined for them.
 *  Check the HandleDefinitions.inl file for list of defined handles.
 *  \tparam T The type of object that the handle is referencing. 
*/
template <class T>
struct Handle final : HandleBase
{
    using IDType = ID<T>;

    static_assert(implementation_exists<HandleDefinition<T>>, "Type does not support handles");

public:
    friend struct AnyHandle;
    friend struct WeakHandle<T>;

    static const Handle empty;

    HypObjectHeader *header;
    T               *ptr;

    Handle()
        : header(nullptr),
          ptr(nullptr)
    {
    }
    
    explicit Handle(HypObjectHeader *header)
        : header(header)
    {
        if (header) {
            header->IncRefStrong();

            ptr = header->container->GetObjectPointer(header);
        } else {
            ptr = nullptr;
        }
    }

    /*! \brief Construct a handle from the given ID.
     *  \param id The ID of the object to reference. */
    explicit Handle(IDType id)
        : Handle(id.IsValid() ? GetContainer<T, false>()->GetObjectHeader(id.Value() - 1) : nullptr)
    {
    }

    template <class TPointerType, typename = std::enable_if_t<IsHypObject<TPointerType>::value && std::is_convertible_v<TPointerType *, T *>>>
    explicit Handle(TPointerType *ptr)
        : header(ptr != nullptr ? ptr->GetObjectHeader_Internal() : nullptr),
          ptr(static_cast<T *>(ptr))
    {
    }

    explicit Handle(HypObjectMemory<T> *header)
        : header(static_cast<HypObjectHeader *>(header)),
          ptr(header != nullptr ? header->GetPointer() : nullptr)
    {
    }

    Handle(const Handle &other)
        : header(other.header),
          ptr(other.ptr)
    {
        if (header != nullptr) {
            header->IncRefStrong();
        }
    }

    Handle &operator=(const Handle &other)
    {
        if (this == &other || header == other.header) {
            return *this;
        }

        if (header != nullptr) {
            header->DecRefStrong();
        }

        header = other.header;
        ptr = other.ptr;

        if (header != nullptr) {
            header->IncRefStrong();
        }

        return *this;
    }

    Handle(Handle &&other) noexcept
        : header(other.header),
          ptr(other.ptr)
    {
        other.header = nullptr;
        other.ptr = nullptr;
    }

    Handle &operator=(Handle &&other) noexcept
    {
        if (this == &other || header == other.header) {
            return *this;
        }

        if (header != nullptr) {
            header->DecRefStrong();
        }

        header = other.header;
        other.header = nullptr;

        ptr = other.ptr;
        other.ptr = nullptr;

        return *this;
    }

    ~Handle()
    {
        if (header != nullptr) {
            header->DecRefStrong();
        }
    }
    
    HYP_FORCE_INLINE T *operator->() const
        { return ptr; }
    
    HYP_FORCE_INLINE T &operator*()
        { return *ptr; }
    
    HYP_FORCE_INLINE const T &operator*() const
        { return *ptr; }
    
    HYP_FORCE_INLINE bool operator!() const
        { return !IsValid(); }
    
    HYP_FORCE_INLINE explicit operator bool() const
        { return IsValid(); }

    HYP_FORCE_INLINE operator IDType() const
        { return header != nullptr ? IDType(header->index + 1) : IDType(); }
    
    HYP_FORCE_INLINE bool operator==(std::nullptr_t) const
        { return ptr == nullptr; }
    
    HYP_FORCE_INLINE bool operator!=(std::nullptr_t) const
        { return ptr != nullptr; }
    
    HYP_FORCE_INLINE bool operator==(const Handle &other) const
        { return header == other.header; }
    
    HYP_FORCE_INLINE bool operator!=(const Handle &other) const
        { return header != other.header; }
    
    /*! \brief Compare two handles by their index.
     *  \param other The handle to compare to.
     *  \return True if the handle is less than the other handle. */
    HYP_FORCE_INLINE bool operator<(const Handle &other) const
        { return IDType(*this) < IDType(other); }
    
    HYP_FORCE_INLINE bool operator==(const IDType &id) const
        { return IDType(*this) == id; }
    
    HYP_FORCE_INLINE bool operator!=(const IDType &id) const
        { return IDType(*this) != id; }

    HYP_FORCE_INLINE bool operator<(const IDType &id) const
        { return IDType(*this) < id; }
    
    /*! \brief Check if the handle is valid. A handle is valid if its index is greater than 0.
     *  \return True if the handle is valid. */
    HYP_FORCE_INLINE bool IsValid() const
        { return header != nullptr; }
    
    /*! \brief Get a referenceable ID for the object that the handle is referencing.
     *  \return The ID of the object. */
    HYP_FORCE_INLINE IDType GetID() const
        { return IDType(*this); }
    
    /*! \brief Get the TypeID for this handle type
     *  \return The TypeID for the handle */
    HYP_FORCE_INLINE TypeID GetTypeID() const
        { return header ? header->container->GetObjectTypeID() : TypeID::ForType<T>(); }
    
    /*! \brief Get a pointer to the object that the handle is referencing.
     *  \return A pointer to the object. */
    HYP_FORCE_INLINE T *Get() const
        { return ptr; }

    /*! \brief Cast to a different handle type - if the type is not convertible, this will return an empty handle.
     *  \tparam Ty The type to cast to.
     *  \return A handle to the new type. */
    template <class Ty, std::enable_if_t<!std::is_same_v<Ty, T> && std::is_convertible_v<std::add_pointer_t<T>, std::add_pointer_t<Ty>>, int> = 0>
    HYP_FORCE_INLINE operator Handle<Ty>() const
    {
        Handle<Ty> handle;

        if (header != nullptr) {
            handle.header = header;
            handle.header->IncRefStrong();

            handle.ptr = static_cast<Ty *>(ptr);
        }

        return handle;
    }
    
    /*! \brief Reset the handle to an empty state.
     *  \details This will decrement the strong reference count of the object that the handle is referencing.
     *  The index is set to 0. */
    HYP_FORCE_INLINE void Reset()
    {
        if (header != nullptr) {
            header->DecRefStrong();
        }

        header = nullptr;
        ptr = nullptr;
    }

    HYP_FORCE_INLINE WeakHandle<T> ToWeak() const
    {
        return WeakHandle<T>(*this);
    }
    
    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        // Hashcode must be same as ID<T> hashcode
        HashCode hc;
        hc.Add(IDType(*this));

        return hc;
    }
};

template <class T>
struct WeakHandle final
{
    using IDType = ID<T>;

    static_assert(implementation_exists<HandleDefinition<T>>, "Type does not support handles");

    static const WeakHandle empty;

    HypObjectHeader *header;
    T               *ptr;

    WeakHandle()
        : header(nullptr),
          ptr(nullptr)
    {
    }
    
    explicit WeakHandle(HypObjectHeader *header)
        : header(header)
    {
        if (header != nullptr) {
            header->IncRefWeak();

            ptr = header->container->GetObjectPointer(header);
        } else {
            ptr = nullptr;
        }
    }

    /*! \brief Construct a WeakHandle from the given ID.
     *  \param id The ID of the object to reference. */
    explicit WeakHandle(IDType id)
        : header(id.IsValid() ? GetContainer<T, false>()->GetObjectHeader(id.Value() - 1) : nullptr)
    {
        if (header != nullptr) {
            header->IncRefWeak();

            ptr = header->container->GetObjectPointer(header);
        } else {
            ptr = nullptr;
        }
    }

    WeakHandle(const Handle<T> &other)
        : header(other.header),
          ptr(other.ptr)
    {
        if (header != nullptr) {
            header->IncRefWeak();
        }
    }

    WeakHandle &operator=(const Handle<T> &other)
    {
        if (header == other.header) {
            return *this;
        }

        if (header != nullptr) {
            header->DecRefWeak();
        }

        header = other.header;
        ptr = other.ptr;

        if (header != nullptr) {
            header->IncRefWeak();
        }

        return *this;
    }

    WeakHandle(const WeakHandle &other)
        : header(other.header),
          ptr(other.ptr)
    {
        if (header != nullptr) {
            header->IncRefWeak();
        }
    }

    WeakHandle &operator=(const WeakHandle &other)
    {
        if (header == other.header) {
            return *this;
        }

        if (header != nullptr) {
            header->DecRefWeak();
        }

        header = other.header;
        ptr = other.ptr;

        if (header != nullptr) {
            header->IncRefWeak();
        }

        return *this;
    }

    WeakHandle(WeakHandle &&other) noexcept
        : header(other.header),
          ptr(other.ptr)
    {
        other.header = nullptr;
        other.ptr = nullptr;
    }

    WeakHandle &operator=(WeakHandle &&other) noexcept
    {
        if (header == other.header) {
            return *this;
        }

        if (header != nullptr) {
            header->DecRefWeak();
        }

        header = other.header;
        other.header = nullptr;

        ptr = other.ptr;
        other.ptr = nullptr;

        return *this;
    }

    ~WeakHandle()
    {
        if (header != nullptr) {
            header->DecRefWeak();
        }
    }

    /*! \brief Lock the weak handle to get a strong reference to the object.
     *  \details If the object is still alive, a strong reference is returned. Otherwise, an empty handle is returned.
     *  \return A strong reference to the object. */
    HYP_NODISCARD HYP_FORCE_INLINE Handle<T> Lock() const
    {
        if (!header) {
            return Handle<T>();
        }

        if (header->ref_count_strong.Get(MemoryOrder::ACQUIRE) == 0) {
            return Handle<T>();
        }
        
        Handle<T> handle;

        handle.header = header;
        handle.header->IncRefStrong();

        handle.ptr = ptr;

        return handle;
    }

    HYP_FORCE_INLINE T *GetUnsafe() const
        { return ptr; }
    
    HYP_FORCE_INLINE bool operator!() const
        { return !IsValid(); }
    
    HYP_FORCE_INLINE explicit operator bool() const
        { return IsValid(); }

    HYP_FORCE_INLINE operator IDType() const
        { return header != nullptr ? IDType(header->index + 1) : IDType(); }
    
    HYP_FORCE_INLINE bool operator==(std::nullptr_t) const
        { return ptr == nullptr; }
    
    HYP_FORCE_INLINE bool operator!=(std::nullptr_t) const
        { return ptr != nullptr; }
    
    HYP_FORCE_INLINE bool operator==(const WeakHandle &other) const
        { return header == other.header; }
    
    HYP_FORCE_INLINE bool operator!=(const WeakHandle &other) const
        { return header != other.header; }

    HYP_FORCE_INLINE bool operator<(const WeakHandle &other) const
        { return IDType(*this) < IDType(other); }
    
    HYP_FORCE_INLINE bool operator==(const Handle<T> &other) const
        { return header == other.header; }
    
    HYP_FORCE_INLINE bool operator!=(const Handle<T> &other) const
        { return header != other.header; }
    
    HYP_FORCE_INLINE bool operator<(const Handle<T> &other) const
        { return IDType(*this) < IDType(other); }
    
    HYP_FORCE_INLINE bool operator==(const IDType &id) const
        { return IDType(*this) == id; }
    
    HYP_FORCE_INLINE bool operator!=(const IDType &id) const
        { return IDType(*this) != id; }
    
    HYP_FORCE_INLINE bool operator<(const IDType &id) const
        { return IDType(*this) < id; }
    
    HYP_FORCE_INLINE bool IsValid() const
        { return header != nullptr; }
    
    HYP_FORCE_INLINE IDType GetID() const
        { return IDType(*this); }

    void Reset()
    {
        if (header != nullptr) {
            header->DecRefWeak();
        }

        header = nullptr;
        ptr = nullptr;
    }
    
    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        // hashcode must be same as ID<T>
        HashCode hc;
        hc.Add(IDType(*this));

        return hc;
    }
};

/*! \brief A dynamic Handle type. Type is stored at runtime instead of compile time.
 *  An AnyHandle is able to be punned to a Handle<T> permitted that T is the actual type of the held object */
struct AnyHandle final
{
    using IDType = IDBase;

    HypObjectHeader *header;
    void            *ptr;
    TypeID          type_id;

public:
    static const AnyHandle empty;

    AnyHandle()
        : header(nullptr),
          ptr(nullptr),
          type_id(TypeID::Void())
    {
    }

    template <class T, typename = std::enable_if_t<std::is_base_of_v<HypObjectBase, T> && !std::is_same_v<HypObjectBase, T>>>
    explicit AnyHandle(T *ptr)
        : header(ptr ? ptr->GetObjectHeader_Internal() : nullptr),
          ptr(ptr),
          type_id(ptr ? ptr->GetObjectHeader_Internal()->container->GetObjectTypeID() : TypeID::ForType<T>())
    {
        if (IsValid()) {
            this->header->IncRefStrong();
        }
    }

    template <class T>
    AnyHandle(const Handle<T> &handle)
        : header(handle.IsValid() ? handle.header : nullptr),
          ptr(handle.ptr),
          type_id(handle.IsValid() ? handle.GetTypeID() : TypeID::ForType<T>())
    {
        if (handle.IsValid()) {
            header->IncRefStrong();
        }
    }

    template <class T>
    AnyHandle(Handle<T> &&handle)
        : header(handle.IsValid() ? handle.header : nullptr),
          ptr(handle.ptr),
          type_id(handle.IsValid() ? handle->GetTypeID() : TypeID::ForType<T>())
    {
        handle.header = nullptr;
        handle.ptr = nullptr;
    }

    template <class T>
    explicit AnyHandle(ID<T> id)
        : AnyHandle(Handle<T>(id))
    {
    }

    HYP_API AnyHandle(const AnyHandle &other);
    HYP_API AnyHandle &operator=(const AnyHandle &other);

    HYP_API AnyHandle(AnyHandle &&other) noexcept;
    HYP_API AnyHandle &operator=(AnyHandle &&other) noexcept;

    HYP_API ~AnyHandle();
    
    HYP_FORCE_INLINE bool operator!() const
        { return !IsValid(); }
    
    HYP_FORCE_INLINE explicit operator bool() const
        { return IsValid(); }
    
    HYP_FORCE_INLINE bool operator==(std::nullptr_t) const
        { return !IsValid(); }
    
    HYP_FORCE_INLINE bool operator!=(std::nullptr_t) const
        { return IsValid(); }
    
    HYP_FORCE_INLINE bool operator==(const AnyHandle &other) const
        { return header == other.header; }
    
    HYP_FORCE_INLINE bool operator!=(const AnyHandle &other) const
        { return header != other.header; }
    
    HYP_FORCE_INLINE bool operator<(const AnyHandle &other) const
        { return GetID() < other.GetID(); }
    
    template <class T>
    HYP_FORCE_INLINE bool operator==(const Handle<T> &other) const
        { return header == other.header; }
    
    template <class T>
    HYP_FORCE_INLINE bool operator!=(const Handle<T> &other) const
        { return header != other.header; }
    
    HYP_FORCE_INLINE bool operator==(const IDBase &id) const
        { return GetID() == id; }

    HYP_FORCE_INLINE bool operator!=(const IDBase &id) const
        { return GetID() != id; }

    HYP_FORCE_INLINE bool operator<(const IDBase &id) const
        { return GetID() < id; }
    
    HYP_FORCE_INLINE bool IsValid() const
        { return header != nullptr; }
    
    /*! \brief Get a referenceable ID for the object that the handle is referencing.
     *  \return The ID of the object. */
    HYP_API IDType GetID() const;
    
    /*! \brief Get the TypeID for this handle type
     *  \return The TypeID for the handle */
    HYP_FORCE_INLINE TypeID GetTypeID() const
        { return type_id; }

    template <class T>
    HYP_FORCE_INLINE bool Is() const
    {
        if (!header) {
            return false;
        }

        return type_id == TypeID::ForType<T>()
            || IsInstanceOfHypClass(GetClass(type_id), ptr, type_id);
    }

    template <class T>
    HYP_NODISCARD Handle<T> Cast() const &
    {
        if (!Is<T>()) {
            return Handle<T>::empty;
        }

        Handle<T> handle;
        handle.header = header;
        handle.ptr = ptr;

        handle.header->IncRefStrong();

        return handle;
    }

    template <class T>
    HYP_NODISCARD Handle<T> Cast() &&
    {
        if (!Is<T>()) {
            return Handle<T>::empty;
        }

        Handle<T> handle;
        handle.header = header;
        handle.ptr = ptr;

        handle.header->IncRefStrong();

        header = nullptr;
        ptr = nullptr;

        return handle;
    }

    // Hack conversion operator to allow punning AnyHandle to Handle<T> - used by HypData
    // Ensure that the type is actually the correct Handle type before using!
    template <class T>
    HYP_NODISCARD HYP_FORCE_INLINE explicit operator Handle<T> &() &
    {
        if (!Is<T>()) {
            return const_cast<Handle<T> &>(Handle<T>::empty);
        }
        
        // Hack to allow punning AnyHandle to Handle<T> - used by HypData
        return *reinterpret_cast<Handle<T> *>(this);
    }

    // Hack conversion operator to allow punning AnyHandle to Handle<T> - used by HypData
    // Ensure that the type is actually the correct Handle type before using!
    template <class T>
    HYP_NODISCARD HYP_FORCE_INLINE explicit operator const Handle<T> &() const &
    {
        if (!Is<T>()) {
            return Handle<T>::empty;
        }

        // Hack to allow punning AnyHandle to Handle<T> - used by HypData
        return *reinterpret_cast<const Handle<T> *>(this);
    }

    HYP_API AnyRef ToRef() const &;

    template <class T>
    HYP_FORCE_INLINE T *TryGet() const
        { return ToRef().TryGet<T>(); }

    HYP_API void Reset();

    /*! \brief Sets this handle to null and returns the pointer to the object that was being referenced.
     *  \note The reference count will not be decremented, so the object will still be alive. Thus, creating a new handle from the returned pointer will cause a memory leak as the ref count will be doubly incremented.
     *  \internal For internal use only, used for marshalling objects between C++ and C#.
     *  \return The pointer to the object that was being referenced. */
    HYP_NODISCARD HYP_API void *Release();
};

template <class T>
const Handle<T> Handle<T>::empty = { };

template <class T>
const WeakHandle<T> WeakHandle<T>::empty = { };

template <class T>
HYP_NODISCARD HYP_FORCE_INLINE inline Handle<T> CreateObject()
{
    ObjectContainer<T> &container = ObjectPool::GetObjectContainerHolder().GetOrCreate<T>();

    HypObjectMemory<T> *element = container.Allocate();
    element->Construct();

    return Handle<T>(element);
}

template <class T, class... Args>
HYP_NODISCARD HYP_FORCE_INLINE inline Handle<T> CreateObject(Args &&... args)
{
    ObjectContainer<T> &container = ObjectPool::GetObjectContainerHolder().GetOrCreate<T>();

    HypObjectMemory<T> *element = container.Allocate();
    element->Construct(std::forward<Args>(args)...);

    return Handle<T>(element);
}

template <class T>
HYP_FORCE_INLINE inline bool InitObject(const Handle<T> &handle)
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

#define DEF_HANDLE(T) \
    class T; \
    \
    extern IObjectContainer *g_container_ptr_##T; \
    \
    template <> \
    struct HandleDefinition< T > \
    { \
        HYP_API static IObjectContainer *GetAllottedContainerPointer(); \
    };

#define DEF_HANDLE_NS(ns, T) \
    namespace ns { \
    class T; \
    } \
    \
    extern IObjectContainer *g_container_ptr_##T; \
    \
    template <> \
    struct HandleDefinition< ns::T > \
    { \
        HYP_API static IObjectContainer *GetAllottedContainerPointer(); \
    };

#include <core/inl/HandleDefinitions.inl>

#undef DEF_HANDLE
#undef DEF_HANDLE_NS

} // namespace hyperion

#endif