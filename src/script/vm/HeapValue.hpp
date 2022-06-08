#ifndef HEAP_VALUE_HPP
#define HEAP_VALUE_HPP

#include <type_traits>
#include <typeinfo>
#include <cstdint>
#include <cstdlib>
#include <stdio.h>

namespace hyperion {
namespace vm {

enum HeapValueFlags {
    GC_MARKED = 0x01,
};

class HeapValue {
public:
    HeapValue();
    HeapValue(const HeapValue &other) = delete;
    HeapValue &operator=(const HeapValue &other) = delete;
    ~HeapValue();

    inline bool operator==(const HeapValue &other) const
    {
        if (m_holder == other.m_holder) {
            return true;
        }

        if (m_holder != NULL && other.m_holder != NULL) {
            return (*m_holder) == (*other.m_holder);
        }

        return false;
    }

    inline size_t GetTypeId() const { return m_holder != nullptr ? m_holder->m_type_id : 0; }
    inline intptr_t GetId() const { return (intptr_t)m_ptr; }
    inline bool IsNull() const { return m_holder == nullptr; }
    inline int &GetFlags() { return m_flags; }
    inline int GetFlags() const { return m_flags; }

    template <typename T>
    inline bool TypeCompatible() const 
        { return GetTypeId() == GetTypeId<typename std::decay<T>::type>(); }

    template <typename T>
    inline void Assign(const T &value)
    {
        if (m_holder != nullptr) { delete m_holder; }
        auto holder = new DerivedHolder<typename std::decay<T>::type>(value);
        m_ptr = reinterpret_cast<void*>(&holder->m_value);
        m_holder = holder;
    }

    template <typename T>
    inline T &Get()
    {
        if (!TypeCompatible<T>()) { throw std::bad_cast(); }
        return *reinterpret_cast<typename std::decay<T>::type*>(m_ptr);
    }

    template <typename T>
    inline const T &Get() const
    {
        if (!TypeCompatible<T>()) { throw std::bad_cast(); }
        return *reinterpret_cast<const typename std::decay<T>::type*>(m_ptr);
    }

    template <typename T>
    inline auto GetRawPointer() -> typename std::decay<T>::type*
        { return reinterpret_cast<typename std::decay<T>::type*>(m_ptr); }

    template <typename T>
    inline auto GetPointer() -> typename std::decay<T>::type*
        { return TypeCompatible<T>() ? GetRawPointer<T>() : nullptr; }

private:
    // base class for an 'any' holder with pure virtual functions
    struct BaseHolder {
        BaseHolder(size_t type_id) : m_type_id(type_id) {}
        virtual ~BaseHolder() = default;
        virtual bool operator==(const BaseHolder &other) const = 0;
        size_t m_type_id;
    };

    // derived class that can hold any type
    template <typename T> struct DerivedHolder : public BaseHolder {
        explicit DerivedHolder(const T &value) : BaseHolder(GetTypeId<T>()), m_value(value) {}

        virtual bool operator==(const BaseHolder &other) const override
        {
            const DerivedHolder<T> *other_casted = dynamic_cast<const DerivedHolder<T>*>(&other);
            return (other_casted != nullptr && other_casted->m_value == m_value);
        }

        T m_value;
    };

    BaseHolder *m_holder;
    void *m_ptr;
    int m_flags;

    template <typename T> struct Type { static void id() {} };
    template <typename T> static inline size_t GetTypeId() { /*return reinterpret_cast<size_t>(&Type<T>::id);*/ return typeid(T).hash_code(); }
};

} // namespace vm
} // namespace hyperion

#endif
