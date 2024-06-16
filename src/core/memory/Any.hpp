/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ANY_HPP
#define HYPERION_ANY_HPP

#include <core/utilities/TypeID.hpp>
#include <core/memory/Memory.hpp>
#include <core/system/Debug.hpp>
#include <core/Defines.hpp>

#include <Types.hpp>

#include <type_traits>

namespace hyperion {

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

} // namespace detail

class Any
{
    using CopyConstructor = std::add_pointer_t<void *(void *)>;
    using DeleteFunction = std::add_pointer_t<void(void *)>;

public:
    // UniquePtr is a friend class
    template <class T>
    friend class memory::UniquePtr;

    Any()
        : m_type_id(TypeID::ForType<void>()),
          m_ptr(nullptr),
          m_copy_ctor(nullptr),
          m_dtor(nullptr)
    {
    }

    static Any Empty()
        { return Any(); }

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

    template <class T>
    Any(const T &value)
        : m_type_id(TypeID::ForType<NormalizedType<T>>()),
          m_ptr(new NormalizedType<T>(value)),
          m_copy_ctor(&detail::Any_CopyConstruct<NormalizedType<T>>),
          m_dtor(&Memory::Delete<NormalizedType<T>>)
    {
    }

    template <class T>
    Any &operator=(const T &value)
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

    template <class T>
    Any(T &&value) noexcept
        : m_type_id(TypeID::ForType<NormalizedType<T>>()),
          m_ptr(new NormalizedType<T>(std::forward<NormalizedType<T>>(value))),
          m_copy_ctor(&detail::Any_CopyConstruct<NormalizedType<T>>),
          m_dtor(&Memory::Delete<NormalizedType<T>>)
    {
    }

    template <class T>
    Any &operator=(T &&value) noexcept
    {
        const auto new_type_id = TypeID::ForType<NormalizedType<T>>();

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

    Any(const Any &other)
        : m_type_id(other.m_type_id),
          m_ptr(other.HasValue() ? other.m_copy_ctor(other.m_ptr) : nullptr),
          m_copy_ctor(other.m_copy_ctor),
          m_dtor(other.m_dtor)
    {
    }

    Any &operator=(const Any &other)
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

    Any(Any &&other) noexcept
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
        m_copy_ctor = other.m_copy_ctor;
        m_dtor = other.m_dtor;

        other.m_type_id = TypeID::ForType<void>();
        other.m_ptr = nullptr;
        other.m_copy_ctor = nullptr;
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

    /*! \brief Returns true if the held object is of type T. */
    template <class T>
    HYP_FORCE_INLINE bool Is() const
        { return m_type_id == TypeID::ForType<NormalizedType<T>>(); }

    /*! \brief Returns true if the held object is of type \ref{type_id}. */
    HYP_FORCE_INLINE
    bool Is(TypeID type_id) const
        { return m_type_id == type_id; }

    /*! \brief Returns the held object as a reference to type T. If the held object is not of type T, an assertion will fail. */
    template <class T>
    HYP_NODISCARD HYP_FORCE_INLINE
    T &Get()
    {
        const TypeID requested_type_id = TypeID::ForType<NormalizedType<T>>();
        AssertThrowMsg(m_type_id == requested_type_id, "Held type not equal to requested type!");

        return *static_cast<NormalizedType<T> *>(m_ptr);
    }
    
    /*! \brief Returns the held object as a const reference to type T. If the held object is not of type T, an assertion will fail. */
    template <class T>
    HYP_NODISCARD HYP_FORCE_INLINE
    const T &Get() const
    {
        const TypeID requested_type_id = TypeID::ForType<NormalizedType<T>>();
        AssertThrowMsg(m_type_id == requested_type_id, "Held type not equal to requested type!");

        return *static_cast<const NormalizedType<T> *>(m_ptr);
    }

    /*! \brief Attempts to get the held object as a pointer to type T. If the held object is not of type T, nullptr is returned. */
    template <class T>
    HYP_NODISCARD HYP_FORCE_INLINE
    T *TryGet()
    {
        const TypeID requested_type_id = TypeID::ForType<NormalizedType<T>>();

        if (m_type_id == requested_type_id) {
            return static_cast<NormalizedType<T> *>(m_ptr);
        }

        return nullptr;
    }
    
    /*! \brief Attempts to get the held object as a const pointer to type T. If the held object is not of type T, nullptr is returned. */
    template <class T>
    HYP_NODISCARD HYP_FORCE_INLINE
    const T *TryGet() const
    {
        const TypeID requested_type_id = TypeID::ForType<NormalizedType<T>>();
        if (m_type_id == requested_type_id) {
            return static_cast<const NormalizedType<T> *>(m_ptr);
        }

        return nullptr;
    }

    template <class T>
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

    template <class T>
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
    void Emplace(Args &&... args)
    {
        if (HasValue()) {
            m_dtor(m_ptr);
        }

        m_type_id = TypeID::ForType<NormalizedType<T>>();
        m_ptr = new NormalizedType<T>(std::forward<Args>(args)...);
        m_copy_ctor = &detail::Any_CopyConstruct<NormalizedType<T>>;
        m_dtor = &Memory::Delete<NormalizedType<T>>;
    }

    /*! \brief Drop ownership of the object, giving it to the caller.
        Make sure to use delete! */
    template <class T>
    HYP_NODISCARD HYP_FORCE_INLINE
    T *Release()
    {
        const TypeID requested_type_id = TypeID::ForType<NormalizedType<T>>();

        AssertThrowMsg(m_type_id == requested_type_id, "Held type not equal to requested type!");

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
    HYP_FORCE_INLINE
    void Reset(T *ptr)
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
    HYP_FORCE_INLINE
    void Reset()
    {
        if (HasValue()) {
            m_dtor(m_ptr);
        }

        m_type_id = TypeID::ForType<void>();
        m_ptr = nullptr;
        m_copy_ctor = nullptr;
        m_dtor = nullptr;
    }

private:
    TypeID          m_type_id;
    void            *m_ptr;
    CopyConstructor m_copy_ctor;
    DeleteFunction  m_dtor;
};
} // namespace memory

using memory::Any;

} // namespace hyperion

#endif