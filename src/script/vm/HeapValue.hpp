#ifndef HEAP_VALUE_HPP
#define HEAP_VALUE_HPP

#include <core/lib/Any.hpp>
#include <core/lib/Variant.hpp>
#include <script/vm/VMString.hpp>
#include <script/vm/VMArray.hpp>
#include <script/vm/VMObject.hpp>
#include <script/vm/VMStruct.hpp>
#include <Constants.hpp>
#include <util/Defines.hpp>

#include <type_traits>
#include <typeinfo>
#include <cstdint>
#include <cstdlib>
#include <stdio.h>
#include <utility>

namespace hyperion {
namespace vm {

enum HeapValueFlags
{
    GC_MARKED = 0x01,
    GC_DESTROYED = 0x02, // for debug
    GC_ALWAYS_ALIVE = 0x04, // for internal objects -- keep them alive without needing to be marked again

    GC_ALIVE = GC_MARKED | GC_ALWAYS_ALIVE
};

class HeapValue_Impl
{
    Variant<VMString, VMObject, VMArray, VMStruct, Any> m_variant;

#define IS_INLINE_TYPE(T) ( \
        std::is_same_v<VMString, NormalizedType<T>> \
        || std::is_same_v<VMObject, NormalizedType<T>> \
        || std::is_same_v<VMArray, NormalizedType<T>> \
        || std::is_same_v<VMStruct, NormalizedType<T>> \
    )

public:
    HeapValue_Impl() = default;
    HeapValue_Impl(const HeapValue_Impl &other) = delete;
    HeapValue_Impl &operator=(const HeapValue_Impl &other) = delete;
    ~HeapValue_Impl() = default;
    
    TypeID GetTypeID() const
    {
        const TypeID type_id = m_variant.GetTypeID();

        if (type_id == TypeID::ForType<Any>()) {
            return m_variant.Get<Any>().GetTypeID();
        }

        return type_id;
    }

    template <class T>
    bool Is() const
        { return GetTypeID() == TypeID::ForType<T>(); }

    bool HasValue() const
        { return m_variant.IsValid(); }

    template <typename T>
    T &Get()
    {
        if constexpr (IS_INLINE_TYPE(T)) {
            return m_variant.Get<T>();
        } else {
            return m_variant.Get<Any>().Get<T>();
        }
    }

    template <typename T>
    const T &Get() const
    {
        if constexpr (IS_INLINE_TYPE(T)) {
            return m_variant.Get<T>();
        } else {
            return m_variant.Get<Any>().Get<T>();
        }
    }

    void *GetRawPointer()
    {
        const TypeID type_id = m_variant.GetTypeID();

        if (type_id == TypeID::ForType<Any>()) {
            return m_variant.Get<Any>().GetPointer();
        }

        return m_variant.GetPointer();
    }

    const void *GetRawPointer() const
    {
        const TypeID type_id = m_variant.GetTypeID();

        if (type_id == TypeID::ForType<Any>()) {
            return m_variant.Get<Any>().GetPointer();
        }

        return m_variant.GetPointer();
    }

    template <class T>
    T *GetPointer()
    {
        if constexpr (IS_INLINE_TYPE(T)) {
            return m_variant.TryGet<T>();
        } else {
            if (auto *any_ptr = m_variant.TryGet<Any>()) {
                return any_ptr->TryGet<T>();
            }

            return nullptr;
        }
    }

    template <class T>
    void Assign(const T &value)
    {
        if constexpr (IS_INLINE_TYPE(T)) {
            m_variant.Set(value);
        } else {
            m_variant.Set(Any::Construct<T>(value));
        }
    }

    template <typename T>
    void Assign(T &&value)
    {
        if constexpr (IS_INLINE_TYPE(T)) {
            m_variant.Set(std::move(value));
        } else {
            m_variant.Set(Any(std::move(value)));
        }
    }

#undef IS_INLINE_TYPE
};

class HeapValue
{
public:
    HeapValue();
    HeapValue(const HeapValue &other) = delete;
    HeapValue &operator=(const HeapValue &other) = delete;
    ~HeapValue();

    TypeID GetTypeID() const
        { return m_impl.GetTypeID(); }

    int &GetFlags()
        { return m_flags; }

    int GetFlags() const
        { return m_flags; }

    template <class T>
    bool Is() const
        { return m_impl.Is<T>(); }

    template <class T>
    bool TypeCompatible() const 
        { return m_impl.Is<T>(); }

    bool HasValue() const
        { return m_impl.HasValue(); }

    template <class T>
    void Assign(const T &value)
        { m_impl.Assign(value); debug_name = typeid(T).name(); }

    template <typename T>
    void Assign(T &&value)
        { m_impl.Assign(std::forward<T>(value)); debug_name = typeid(T).name(); }

    template <typename T>
    T &Get()
        { return m_impl.Get<T>(); }

    template <typename T>
    const T &Get() const
        { return m_impl.Get<T>(); }

    void *GetRawPointer()
        { return m_impl.GetRawPointer(); }

    const void *GetRawPointer() const
        { return m_impl.GetRawPointer(); }

    template <class T>
    T *GetPointer()
        { return m_impl.GetPointer<T>(); }

    template <class T>
    const T *GetPointer() const
        { return m_impl.GetPointer<T>(); }

    void Mark();

private:
    HeapValue_Impl m_impl;
    int m_flags;

    const char *debug_name = "";
};

} // namespace vm
} // namespace hyperion

#endif
