#ifndef HEAP_VALUE_HPP
#define HEAP_VALUE_HPP

#include <core/lib/Any.hpp>
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

class HeapValue
{
public:
    HeapValue();
    HeapValue(const HeapValue &other) = delete;
    HeapValue &operator=(const HeapValue &other) = delete;
    ~HeapValue();

    const TypeID &GetTypeID() const
        { return m_any.GetTypeID(); }

    int &GetFlags()
        { return m_flags; }

    int GetFlags() const
        { return m_flags; }

    template <class T>
    bool TypeCompatible() const 
        { return m_any.GetTypeID() == TypeID::ForType<NormalizedType<T>>(); }

    bool HasValue() const
        { return m_any.HasValue(); }

    template <class T>
    void Assign(const T &value)
        { m_any = value; }

    template <typename T>
    void Assign(T &&value)
        { m_any = std::move(value);  }

    template <typename T>
    T &Get()
        { return m_any.Get<T>(); }

    template <typename T>
    const T &Get() const
        { return m_any.Get<T>(); }

    template <class T>
    auto GetRawPointer() -> NormalizedType<T> *
        { return static_cast<NormalizedType<T> *>(m_any.GetPointer()); }

    template <class T>
    auto GetPointer() -> NormalizedType<T> *
        { return TypeCompatible<T>() ? GetRawPointer<T>() : nullptr; }

    void Mark();

private:
    Any m_any;
    int m_flags;
};

} // namespace vm
} // namespace hyperion

#endif
