#ifndef HYPERION_V2_LIB_ANY_HPP
#define HYPERION_V2_LIB_ANY_HPP

#include <core/lib/TypeID.hpp>
#include <core/lib/CMemory.hpp>
#include <system/Debug.hpp>
#include <Types.hpp>

#include <util/Defines.hpp>

#include <type_traits>

namespace hyperion {

namespace detail {
template <class T>
class UniquePtr;
} // namespace detail

class Any
{
    using DeleteFunction = std::add_pointer_t<void(void *)>;

public:
    // UniquePtr is a friend class
    template <class T>
    friend class detail::UniquePtr;

    Any()
        : m_type_id(TypeID::ForType<void>()),
          m_ptr(nullptr),
          m_delete_function(nullptr)
    {
    }

    static Any Empty()
        { return Any(); }

    /*! \brief Construct a new T into the Any, without needing to use any move or copy constructors. */
    template <class T, class ...Args>
    static Any Construct(Args &&... args)
    {
        Any any;
        any.m_type_id = TypeID::ForType<T>();
        any.m_ptr = new T(std::forward<Args>(args)...);
        any.m_delete_function = &Memory::Delete<T>;

        return any;
    }

    template <class T>
    explicit Any(const T &value)
        : m_type_id(TypeID::ForType<T>()),
          m_ptr(new T(value)),
          m_delete_function(&Memory::Delete<T>)
    {
    }

    template <class T>
    Any &operator=(const T &value)
    {
        const auto new_type_id = TypeID::ForType<T>();


        if constexpr (std::is_copy_assignable_v<T>) {
            if (m_type_id == new_type_id) {
                // types are same, call copy assignment operator.
                *static_cast<T *>(m_ptr) = value;

                return *this;
            }
        }

        if (HasValue()) {
            m_delete_function(m_ptr);
        }

        m_type_id = new_type_id;
        m_ptr = new T(value);
        m_delete_function = &Memory::Delete<T>;

        return *this;
    }

    template <class T>
    explicit Any(T &&value) noexcept
        : m_type_id(TypeID::ForType<T>()),
          m_ptr(new T(std::forward<T>(value))),
          m_delete_function(&Memory::Delete<T>)
    {
    }

    template <class T>
    Any &operator=(T &&value) noexcept
    {
        const auto new_type_id = TypeID::ForType<T>();

        if constexpr (std::is_move_assignable_v<T>) {
            if (m_type_id == new_type_id) {
                // types are same, call move assignment operator
                *static_cast<T *>(m_ptr) = std::move(value);

                return *this;
            }
        }

        if (HasValue()) {
            m_delete_function(m_ptr);
        }

        m_type_id = new_type_id;
        m_ptr = new T(std::move(value));
        m_delete_function = &Memory::Delete<T>;

        return *this;
    }

    Any(const Any &other) = delete;
    Any &operator=(const Any &other) = delete;

    Any(Any &&other) noexcept
        : m_type_id(std::move(other.m_type_id)),
          m_ptr(other.m_ptr),
          m_delete_function(other.m_delete_function)
    {
        other.m_type_id = TypeID::ForType<void>();
        other.m_ptr = nullptr;
        other.m_delete_function = nullptr;
    }

    Any &operator=(Any &&other) noexcept
    {
        if (HasValue()) {
            m_delete_function(m_ptr);
        }

        m_type_id = other.m_type_id;
        m_ptr = other.m_ptr;
        m_delete_function = other.m_delete_function;

        other.m_type_id = TypeID::ForType<void>();
        other.m_ptr = nullptr;
        other.m_delete_function = nullptr;
        
        return *this;
    }

    ~Any()
    {
        if (HasValue()) {
            m_delete_function(m_ptr);
        }
    }

    /**
     * \brief Get a raw pointer to the held object.
     */
    HYP_FORCE_INLINE void *GetPointer()
        { return m_ptr; }

    /**
     * \brief Get a raw pointer to the held object.
     */
    HYP_FORCE_INLINE const void *GetPointer() const
        { return m_ptr; }

    /**
     * \brief Get the delete function for the held object.
    */
    HYP_FORCE_INLINE DeleteFunction GetDeleteFunction() const
        { return m_delete_function; }

    /**
     * \brief Returns true if the Any has a value.
     */
    HYP_FORCE_INLINE bool HasValue() const
        { return m_ptr != nullptr; }

    /**
     * \brief Returns the TypeID of the held object.
     */
    HYP_FORCE_INLINE TypeID GetTypeID() const
        { return m_type_id; }

    /**
     * \brief Returns true if the held object is of type T.
     */
    template <class T>
    HYP_FORCE_INLINE bool Is() const
        { return m_type_id == TypeID::ForType<T>(); }

    /**
     * \brief Returns true if the held object is of type {type_id}.
     */
    HYP_FORCE_INLINE bool Is(TypeID type_id) const
        { return m_type_id == type_id; }

    /**
     * \brief Returns the held object as a reference to type T. If the held object is not of type T, an exception is thrown.
     */
    template <class T>
    HYP_FORCE_INLINE T &Get()
    {
        const auto requested_type_id = TypeID::ForType<T>();
        AssertThrowMsg(m_type_id == requested_type_id, "Held type not equal to requested type!");

        return *static_cast<T *>(m_ptr);
    }
    
    /**
     * \brief Returns the held object as a const reference to type T. If the held object is not of type T, an exception is thrown.
     */
    template <class T>
    HYP_FORCE_INLINE const T &Get() const
    {
        const auto requested_type_id = TypeID::ForType<T>();
        AssertThrowMsg(m_type_id == requested_type_id, "Held type not equal to requested type!");

        return *static_cast<const T *>(m_ptr);
    }

    /**
     * \brief Attempts to get the held object as a pointer to type T. If the held object is not of type T, nullptr is returned.
     */
    template <class T>
    HYP_FORCE_INLINE T *TryGet()
    {
        const auto requested_type_id = TypeID::ForType<T>();
        if (m_type_id == requested_type_id) {
            return static_cast<T *>(m_ptr);
        }

        return nullptr;
    }
    
    /**
     * \brief Attempts to get the held object as a const pointer to type T. If the held object is not of type T, nullptr is returned.
     */
    template <class T>
    HYP_FORCE_INLINE const T *TryGet() const
    {
        const auto requested_type_id = TypeID::ForType<T>();
        if (m_type_id == requested_type_id) {
            return static_cast<const T *>(m_ptr);
        }

        return nullptr;
    }

    /*! \brief Construct a new pointer into the Any. Any current value will be destroyed. */
    template <class T, class ...Args>
    void Emplace(Args &&... args)
    {
        if (HasValue()) {
            m_delete_function(m_ptr);
        }

        m_type_id = TypeID::ForType<T>();
        m_ptr = new T(std::forward<Args>(args)...);
        m_delete_function = &Memory::Delete<T>;
    }

    /*! \brief Drop ownership of the object, giving it to the caller.
        Make sure to use delete! */
    template <class T>
    T *Release()
    {
        const auto requested_type_id = TypeID::ForType<T>();

        AssertThrowMsg(m_type_id == requested_type_id, "Held type not equal to requested type!");

        T *ptr = static_cast<T *>(m_ptr);

        m_type_id = TypeID::ForType<void>();
        m_ptr = nullptr;
        m_delete_function = nullptr;

        return ptr;
    }

    /*! \brief Takes ownership of {ptr}, resetting the current value held in the Any.
        Do NOT delete the value passed to this function, as it is deleted by the Any.
    */
    template <class T>
    void Reset(T *ptr)
    {
        if (HasValue()) {
            m_delete_function(m_ptr);
        }

        m_type_id = TypeID::ForType<T>();
        m_ptr = ptr;

        if (ptr) {
            m_delete_function = &Memory::Delete<T>;
        } else {
            m_delete_function = nullptr;
        }
    }

    /*! \brief Resets the current value held in the Any. */
    void Reset()
    {
        if (HasValue()) {
            m_delete_function(m_ptr);
        }

        m_type_id = TypeID::ForType<void>();
        m_ptr = nullptr;
        m_delete_function = nullptr;
    }

private:
    TypeID          m_type_id;
    void            *m_ptr;
    DeleteFunction  m_delete_function;
};

} // namespace hyperion

#endif