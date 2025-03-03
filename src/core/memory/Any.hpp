/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ANY_HPP
#define HYPERION_ANY_HPP

#include <core/utilities/TypeID.hpp>
#include <core/memory/AnyRef.hpp>
#include <core/memory/Memory.hpp>
#include <core/debug/Debug.hpp>
#include <core/Defines.hpp>

#include <Types.hpp>

#include <type_traits>

namespace hyperion {

class HypClass;

extern HYP_API const HypClass *GetClass(TypeID type_id);
extern HYP_API bool IsInstanceOfHypClass(const HypClass *hyp_class, const void *ptr, TypeID type_id);

namespace memory {

template <class T>
class UniquePtr;

namespace detail {

template <class T>
static inline void *Any_CopyConstruct(void *src)
{
    static_assert(std::is_copy_constructible_v<T>, "T must be copy constructible to use copyable Any type");

    return new T(*static_cast<T *>(src));
}

class AnyBase
{
};

} // namespace detail

class CopyableAny;

class Any : public detail::AnyBase
{
    using DeleteFunction = std::add_pointer_t<void(void *)>;

public:
    // UniquePtr is a friend class
    template <class T>
    friend class memory::UniquePtr;

    Any()
        : m_type_id(TypeID::ForType<void>()),
          m_ptr(nullptr),
          m_dtor(nullptr)
    {
    }

    /*! \brief Construct an Any by taking ownership of \ref{ptr}. The object pointed to will be deleted when the Any is destructed. */
    template <class T>
    explicit Any(T *ptr)
        : m_type_id(TypeID::ForType<NormalizedType<T>>()),
          m_ptr(ptr),
          m_dtor(&Memory::Delete<NormalizedType<T>>)
    {
    }

    /*! \brief Construct a new T into the Any, without needing to use any move or copy constructors. */
    template <class T, class ...Args>
    static Any Construct(Args &&... args)
    {
        Any any;

        if constexpr (!std::is_void_v<T>) {
            any.Emplace<NormalizedType<T>>(std::forward<Args>(args)...);
        }

        return any;
    }

    template <class T, typename = std::enable_if_t< !std::is_base_of_v< detail::AnyBase, T > > >
    Any(T &&value) noexcept
        : m_type_id(TypeID::ForType<NormalizedType<T>>()),
          m_ptr(new NormalizedType<T>(std::forward<NormalizedType<T>>(value))),
          m_dtor(&Memory::Delete<NormalizedType<T>>)
    {
    }

    template <class T, typename = std::enable_if_t< !std::is_base_of_v< detail::AnyBase, T > > >
    Any &operator=(T &&value) noexcept
    {
        const TypeID new_type_id = TypeID::ForType<NormalizedType<T>>();

        if constexpr (std::is_move_assignable_v<NormalizedType<T>>) {
            if (m_type_id == new_type_id) {
                // types are same, call move assignment operator
                *static_cast<NormalizedType<T> *>(m_ptr) = std::forward<T>(value);

                return *this;
            }
        }

        if (HasValue()) {
            m_dtor(m_ptr);
        }

        m_type_id = new_type_id;
        m_ptr = new NormalizedType<T>(std::forward<T>(value));
        m_dtor = &Memory::Delete<NormalizedType<T>>;

        return *this;
    }

    Any(const Any &other)               = delete;
    Any &operator=(const Any &other)    = delete;

    Any(Any &&other) noexcept
        : m_type_id(std::move(other.m_type_id)),
          m_ptr(other.m_ptr),
          m_dtor(other.m_dtor)
    {
        other.m_type_id = TypeID::ForType<void>();
        other.m_ptr = nullptr;
        other.m_dtor = nullptr;
    }

    Any &operator=(Any &&other) noexcept
    {
        if (this == &other) {
            return *this;
        }

        if (HasValue()) {
            m_dtor(m_ptr);
        }

        m_type_id = other.m_type_id;
        m_ptr = other.m_ptr;
        m_dtor = other.m_dtor;

        other.m_type_id = TypeID::ForType<void>();
        other.m_ptr = nullptr;
        other.m_dtor = nullptr;
        
        return *this;
    }

    ~Any()
    {
        if (HasValue()) {
            m_dtor(m_ptr);
        }
    }

    /*! \brief Get a raw pointer to the held object. */
    HYP_FORCE_INLINE void *GetPointer()
        { return m_ptr; }

    /*! \brief Get a raw pointer to the held object. */
    HYP_FORCE_INLINE const void *GetPointer() const
        { return m_ptr; }

    /*! \brief Returns true if the Any has a value. */
    HYP_FORCE_INLINE bool HasValue() const
        { return m_ptr != nullptr; }

    /*! \brief Returns the TypeID of the held object. */
    HYP_FORCE_INLINE TypeID GetTypeID() const
        { return m_type_id; }

    /*! \brief Returns true if the held object is of type T.
     *  If T has a HypClass registered, this function will also return true if the held object is a subclass of T. */
    template <class T>
    HYP_FORCE_INLINE bool Is() const
    {
        constexpr TypeID type_id = TypeID::ForType<NormalizedType<T>>();

        return m_type_id == type_id
            || IsInstanceOfHypClass(GetClass(type_id), m_ptr, m_type_id);
    }

    /*! \brief Returns true if the held object is of type \ref{type_id}.
     *  If the type with the given ID has a HypClass registered, this function will also return true if the held object is a subclass of the type. */
    HYP_FORCE_INLINE bool Is(TypeID type_id) const
    {
        return m_type_id == type_id
            || IsInstanceOfHypClass(GetClass(type_id), m_ptr, m_type_id);
    }

    /*! \brief Returns the held object as a reference to type T. If the held object is not of type T, an assertion will fail. */
    template <class T>
    HYP_NODISCARD HYP_FORCE_INLINE T &Get() const
    {
        const TypeID requested_type_id = TypeID::ForType<NormalizedType<T>>();
        AssertThrowMsg(m_type_id == requested_type_id, "Held type not equal to requested type!");

        return *static_cast<NormalizedType<T> *>(m_ptr);
    }

    /*! \brief Attempts to get the held object as a pointer to type T. If the held object is not of type T, nullptr is returned. */
    template <class T>
    HYP_FORCE_INLINE T *TryGet() const
    {
        const TypeID requested_type_id = TypeID::ForType<NormalizedType<T>>();

        if (m_type_id == requested_type_id) {
            return static_cast<NormalizedType<T> *>(m_ptr);
        }

        return nullptr;
    }

    template <class T, typename = std::enable_if_t< !std::is_base_of_v< detail::AnyBase, T > > >
    void Set(const T &value)
    {
        if (HasValue()) {
            m_dtor(m_ptr);
        }

        m_type_id = TypeID::ForType<NormalizedType<T>>();
        m_ptr = new NormalizedType<T>(value);
        m_dtor = &Memory::Delete<NormalizedType<T>>;
    }

    template <class T, typename = std::enable_if_t< !std::is_base_of_v< detail::AnyBase, T > > >
    void Set(T &&value)
    {
        if (HasValue()) {
            m_dtor(m_ptr);
        }

        m_type_id = TypeID::ForType<NormalizedType<T>>();
        m_ptr = new NormalizedType<T>(std::move(value));
        m_dtor = &Memory::Delete<NormalizedType<T>>;
    }

    /*! \brief Construct a new pointer into the Any. Any current value will be destroyed. */
    template <class T, class ...Args>
    T &Emplace(Args &&... args)
    {
        if (HasValue()) {
            m_dtor(m_ptr);
        }

        m_type_id = TypeID::ForType<NormalizedType<T>>();
        m_ptr = new NormalizedType<T>(std::forward<Args>(args)...);
        m_dtor = &Memory::Delete<NormalizedType<T>>;

        return *static_cast<NormalizedType<T> *>(m_ptr);
    }

    /*! \brief Drop ownership of the object, giving it to the caller.
        Make sure to use delete! */
    template <class T>
    HYP_NODISCARD HYP_FORCE_INLINE T *Release()
    {
        if constexpr (!std::is_void_v<T>) {
            const TypeID requested_type_id = TypeID::ForType<NormalizedType<T>>();
            AssertThrowMsg(m_type_id == requested_type_id, "Held type not equal to requested type!");
        }

        T *ptr = static_cast<T *>(m_ptr);

        m_type_id = TypeID::ForType<void>();
        m_ptr = nullptr;
        m_dtor = nullptr;

        return ptr;
    }

    /*! \brief Takes ownership of {ptr}, resetting the current value held in the Any.
        Do NOT delete the value passed to this function, as it is deleted by the Any.
    */
    template <class T>
    HYP_FORCE_INLINE void Reset(T *ptr)
    {
        if (HasValue()) {
            m_dtor(m_ptr);
        }

        m_type_id = TypeID::ForType<NormalizedType<T>>();
        m_ptr = ptr;

        if (ptr) {
            m_dtor = &Memory::Delete<NormalizedType<T>>;
        } else {
            m_dtor = nullptr;
        }
    }

    /*! \brief Resets the current value held in the Any. */
    HYP_FORCE_INLINE void Reset()
    {
        if (HasValue()) {
            m_dtor(m_ptr);
        }

        m_type_id = TypeID::ForType<void>();
        m_ptr = nullptr;
        m_dtor = nullptr;
    }

    HYP_FORCE_INLINE explicit operator bool() const
        { return HasValue(); }

    /*! \brief Returns the held object as a reference to type T */
    HYP_NODISCARD HYP_FORCE_INLINE AnyRef ToRef()
        { return AnyRef(m_type_id, m_ptr); }

    /*! \brief Returns the held object as a const reference to type T */
    HYP_NODISCARD HYP_FORCE_INLINE ConstAnyRef ToRef() const
        { return ConstAnyRef(m_type_id, m_ptr); }

    HYP_NODISCARD HYP_FORCE_INLINE explicit operator AnyRef()
        { return AnyRef(m_type_id, m_ptr); }

    HYP_NODISCARD HYP_FORCE_INLINE explicit operator ConstAnyRef() const
        { return ConstAnyRef(m_type_id, m_ptr); }

    static Any FromVoidPointer(TypeID type_id, void *ptr, DeleteFunction dtor)
    {
        Any result;
        result.m_type_id = type_id;
        result.m_ptr = ptr;
        result.m_dtor = dtor;

        return result;
    }

protected:
    TypeID          m_type_id;
    void            *m_ptr;
    DeleteFunction  m_dtor;
};

class CopyableAny : public detail::AnyBase
{
    using CopyConstructor = std::add_pointer_t<void *(void *)>;
    using DeleteFunction = std::add_pointer_t<void(void *)>;

public:
    // UniquePtr is a friend class
    template <class T>
    friend class memory::UniquePtr;

    CopyableAny()
        : m_type_id(TypeID::ForType<void>()),
          m_ptr(nullptr),
          m_copy_ctor(nullptr),
          m_dtor(nullptr)
    {
    }

    /*! \brief Construct a new T into the Any, without needing to use any move or copy constructors. */
    template <class T, class ...Args>
    static CopyableAny Construct(Args &&... args)
    {
        CopyableAny any;

        if constexpr (!std::is_void_v<T>) {
            any.Emplace<NormalizedType<T>>(std::forward<Args>(args)...);
        }

        return any;
    }

    template <class T, typename = std::enable_if_t< !std::is_base_of_v< detail::AnyBase, T > > >
    CopyableAny(const T &value)
        : m_type_id(TypeID::ForType<NormalizedType<T>>()),
          m_ptr(new NormalizedType<T>(value)),
          m_copy_ctor(&detail::Any_CopyConstruct<NormalizedType<T>>),
          m_dtor(&Memory::Delete<NormalizedType<T>>)
    {
    }

    template <class T, typename = std::enable_if_t< !std::is_base_of_v< detail::AnyBase, T > > >
    CopyableAny &operator=(const T &value)
    {
        const TypeID new_type_id = TypeID::ForType<NormalizedType<T>>();
        
        if constexpr (std::is_copy_assignable_v<NormalizedType<T>>) {
            if (m_type_id == new_type_id) {
                // types are same, call copy assignment operator.
                *static_cast<T *>(m_ptr) = value;

                return *this;
            }
        }

        if (HasValue()) {
            m_dtor(m_ptr);
        }

        m_type_id = new_type_id;
        m_ptr = new NormalizedType<T>(value);
        m_copy_ctor = &detail::Any_CopyConstruct<NormalizedType<T>>;
        m_dtor = &Memory::Delete<NormalizedType<T>>;

        return *this;
    }

    template <class T, typename = std::enable_if_t< !std::is_base_of_v< detail::AnyBase, T > > >
    CopyableAny(T &&value) noexcept
        : m_type_id(TypeID::ForType<NormalizedType<T>>()),
          m_ptr(new NormalizedType<T>(std::forward<NormalizedType<T>>(value))),
          m_copy_ctor(&detail::Any_CopyConstruct<NormalizedType<T>>),
          m_dtor(&Memory::Delete<NormalizedType<T>>)
    {
    }

    template <class T, typename = std::enable_if_t< !std::is_base_of_v< detail::AnyBase, T > > >
    CopyableAny &operator=(T &&value) noexcept
    {
        const TypeID new_type_id = TypeID::ForType<NormalizedType<T>>();

        if constexpr (std::is_move_assignable_v<NormalizedType<T>>) {
            if (m_type_id == new_type_id) {
                // types are same, call move assignment operator
                *static_cast<NormalizedType<T> *>(m_ptr) = std::move(value);

                return *this;
            }
        }

        if (HasValue()) {
            m_dtor(m_ptr);
        }

        m_type_id = new_type_id;
        m_ptr = new NormalizedType<T>(std::move(value));
        m_copy_ctor = &detail::Any_CopyConstruct<NormalizedType<NormalizedType<T>>>;
        m_dtor = &Memory::Delete<NormalizedType<T>>;

        return *this;
    }

    CopyableAny(const CopyableAny &other)
        : m_type_id(other.m_type_id),
          m_ptr(other.HasValue() ? other.m_copy_ctor(other.m_ptr) : nullptr),
          m_copy_ctor(other.m_copy_ctor),
          m_dtor(other.m_dtor)
    {
    }

    CopyableAny &operator=(const CopyableAny &other)
    {
        if (this == &other) {
            return *this;
        }

        if (HasValue()) {
            m_dtor(m_ptr);
        }

        m_type_id = other.m_type_id;
        m_ptr = other.HasValue() ? other.m_copy_ctor(other.m_ptr) : nullptr;
        m_copy_ctor = other.m_copy_ctor;
        m_dtor = other.m_dtor;

        return *this;
    }

    CopyableAny(CopyableAny &&other) noexcept
        : m_type_id(std::move(other.m_type_id)),
          m_ptr(other.m_ptr),
          m_copy_ctor(other.m_copy_ctor),
          m_dtor(other.m_dtor)
    {
        other.m_type_id = TypeID::ForType<void>();
        other.m_ptr = nullptr;
        other.m_copy_ctor = nullptr;
        other.m_dtor = nullptr;
    }

    CopyableAny &operator=(CopyableAny &&other) noexcept
    {
        if (this == &other) {
            return *this;
        }

        if (HasValue()) {
            m_dtor(m_ptr);
        }

        m_type_id = other.m_type_id;
        m_ptr = other.m_ptr;
        m_copy_ctor = other.m_copy_ctor;
        m_dtor = other.m_dtor;

        other.m_type_id = TypeID::ForType<void>();
        other.m_ptr = nullptr;
        other.m_copy_ctor = nullptr;
        other.m_dtor = nullptr;
        
        return *this;
    }

    ~CopyableAny()
    {
        if (HasValue()) {
            m_dtor(m_ptr);
        }
    }

    /*! \brief Get a raw pointer to the held object. */
    HYP_FORCE_INLINE void *GetPointer()
        { return m_ptr; }

    /*! \brief Get a raw pointer to the held object. */
    HYP_FORCE_INLINE const void *GetPointer() const
        { return m_ptr; }

    /*! \brief Returns true if the Any has a value. */
    HYP_FORCE_INLINE bool HasValue() const
        { return m_ptr != nullptr; }

    /*! \brief Returns the TypeID of the held object. */
    HYP_FORCE_INLINE TypeID GetTypeID() const
        { return m_type_id; }

    /*! \brief Returns true if the held object is of type T.
     *  If T has a HypClass registered, this function will also return true if the held object is a subclass of T. */
    template <class T>
    HYP_FORCE_INLINE bool Is() const
    {
        constexpr TypeID type_id = TypeID::ForType<NormalizedType<T>>();

        return m_type_id == type_id
            || IsInstanceOfHypClass(GetClass(type_id), m_ptr, m_type_id);
    }

    /*! \brief Returns true if the held object is of type \ref{type_id}.
     *  If the type with the given ID has a HypClass registered, this function will also return true if the held object is a subclass of the type. */
    HYP_FORCE_INLINE bool Is(TypeID type_id) const
    {
        return m_type_id == type_id
            || IsInstanceOfHypClass(GetClass(type_id), m_ptr, m_type_id);
    }

    /*! \brief Returns the held object as a reference to type T. If the held object is not of type T, an assertion will fail. */
    template <class T>
    HYP_FORCE_INLINE T &Get() const
    {
        const TypeID requested_type_id = TypeID::ForType<NormalizedType<T>>();
        AssertThrowMsg(m_type_id == requested_type_id, "Held type not equal to requested type!");

        return *static_cast<NormalizedType<T> *>(m_ptr);
    }

    /*! \brief Attempts to get the held object as a pointer to type T. If the held object is not of type T, nullptr is returned. */
    template <class T>
    HYP_FORCE_INLINE T *TryGet() const
    {
        const TypeID requested_type_id = TypeID::ForType<NormalizedType<T>>();

        if (m_type_id == requested_type_id) {
            return static_cast<NormalizedType<T> *>(m_ptr);
        }

        return nullptr;
    }

    template <class T, typename = std::enable_if_t< !std::is_base_of_v< detail::AnyBase, T > > >
    void Set(const T &value)
    {
        if (HasValue()) {
            m_dtor(m_ptr);
        }

        m_type_id = TypeID::ForType<NormalizedType<T>>();
        m_ptr = new NormalizedType<T>(value);
        m_copy_ctor = &detail::Any_CopyConstruct<NormalizedType<T>>;
        m_dtor = &Memory::Delete<NormalizedType<T>>;
    }

    template <class T, typename = std::enable_if_t< !std::is_base_of_v< detail::AnyBase, T > > >
    void Set(T &&value)
    {
        if (HasValue()) {
            m_dtor(m_ptr);
        }

        m_type_id = TypeID::ForType<NormalizedType<T>>();
        m_ptr = new NormalizedType<T>(std::move(value));
        m_copy_ctor = &detail::Any_CopyConstruct<NormalizedType<T>>;
        m_dtor = &Memory::Delete<NormalizedType<T>>;
    }

    /*! \brief Construct a new pointer into the Any. Any current value will be destroyed. */
    template <class T, class ...Args>
    T &Emplace(Args &&... args)
    {
        if (HasValue()) {
            m_dtor(m_ptr);
        }

        m_type_id = TypeID::ForType<NormalizedType<T>>();
        m_ptr = new NormalizedType<T>(std::forward<Args>(args)...);
        m_copy_ctor = &detail::Any_CopyConstruct<NormalizedType<T>>;
        m_dtor = &Memory::Delete<NormalizedType<T>>;

        return *static_cast<NormalizedType<T> *>(m_ptr);
    }

    /*! \brief Drop ownership of the object, giving it to the caller.
        Make sure to use delete! */
    template <class T>
    HYP_NODISCARD HYP_FORCE_INLINE T *Release()
    {
        if constexpr (!std::is_void_v<T>) {
            const TypeID requested_type_id = TypeID::ForType<NormalizedType<T>>();
            AssertThrowMsg(m_type_id == requested_type_id, "Held type not equal to requested type!");
        }

        T *ptr = static_cast<T *>(m_ptr);

        m_type_id = TypeID::ForType<void>();
        m_ptr = nullptr;
        m_copy_ctor = nullptr;
        m_dtor = nullptr;

        return ptr;
    }

    /*! \brief Takes ownership of {ptr}, resetting the current value held in the Any.
        Do NOT delete the value passed to this function, as it is deleted by the Any.
    */
    template <class T>
    HYP_FORCE_INLINE void Reset(T *ptr)
    {
        if (HasValue()) {
            m_dtor(m_ptr);
        }

        m_type_id = TypeID::ForType<NormalizedType<T>>();
        m_ptr = ptr;

        if (ptr) {
            m_copy_ctor = &detail::Any_CopyConstruct<NormalizedType<T>>;
            m_dtor = &Memory::Delete<NormalizedType<T>>;
        } else {
            m_copy_ctor = nullptr;
            m_dtor = nullptr;
        }
    }

    /*! \brief Resets the current value held in the Any. */
    HYP_FORCE_INLINE void Reset()
    {
        if (HasValue()) {
            m_dtor(m_ptr);
        }

        m_type_id = TypeID::ForType<void>();
        m_ptr = nullptr;
        m_copy_ctor = nullptr;
        m_dtor = nullptr;
    }

    HYP_FORCE_INLINE explicit operator bool() const
        { return HasValue(); }

    /*! \brief Returns the held object as a reference to type T */
    HYP_NODISCARD HYP_FORCE_INLINE AnyRef ToRef()
        { return AnyRef(m_type_id, m_ptr); }

    /*! \brief Returns the held object as a const reference to type T */
    HYP_NODISCARD HYP_FORCE_INLINE ConstAnyRef ToRef() const
        { return ConstAnyRef(m_type_id, m_ptr); }

    HYP_NODISCARD HYP_FORCE_INLINE explicit operator AnyRef()
        { return AnyRef(m_type_id, m_ptr); }

    HYP_NODISCARD HYP_FORCE_INLINE explicit operator ConstAnyRef() const
        { return ConstAnyRef(m_type_id, m_ptr); }

protected:
    TypeID          m_type_id;
    void            *m_ptr;
    CopyConstructor m_copy_ctor;
    DeleteFunction  m_dtor;
};

} // namespace memory

using memory::Any;
using memory::CopyableAny;

} // namespace hyperion

#endif