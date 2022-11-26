#ifndef HYPERION_V2_OPAQUE_HANDLE_HPP
#define HYPERION_V2_OPAQUE_HANDLE_HPP

#include <core/Core.hpp>
#include <core/Handle.hpp>
#include <Component.hpp>

namespace hyperion::v2 {

extern ComponentSystem &GetObjectSystem();

template <class T>
static inline ObjectContainer<T> &GetContainer()
{
    return GetObjectSystem().GetContainer<T>();
}

struct OpaqueHandleBase
{
    SizeType index;
};

template <class T>
struct OpaqueHandle : OpaqueHandleBase
{
    using ID = HandleID<T>;

    static_assert(has_opaque_handle_defined<T>, "Type does not support handles");

    static const OpaqueHandle empty;

    OpaqueHandle()
    {
        index = 0;
    }

    explicit OpaqueHandle(const ID &id)
    {
        index = id.Value();

        if (index != 0) {
            GetContainer<T>().IncRef(index - 1);
        }
    }

    OpaqueHandle(const OpaqueHandle &other)
    {
        index = other.index;

        if (index != 0) {
            GetContainer<T>().IncRef(index - 1);
        }
    }

    OpaqueHandle &operator=(const OpaqueHandle &other)
    {
        if (index != 0) {
            GetContainer<T>().DecRef(index - 1);
        }

        index = other.index;

        if (index != 0) {
            GetContainer<T>().IncRef(index - 1);
        }

        return *this;
    }

    OpaqueHandle(OpaqueHandle &&other) noexcept
    {
        index = other.index;
        other.index = 0;
    }

    OpaqueHandle &operator=(OpaqueHandle &&other) noexcept
    {
        if (index != 0) {
            GetContainer<T>().DecRef(index - 1);
        }

        index = other.index;
        other.index = 0;

        return *this;
    }

    ~OpaqueHandle()
    {
        if (index != 0) {
            GetContainer<T>().DecRef(index - 1);
        }
    }

    T *operator->() const
        { return Get(); }

    T &operator*()
        { return *Get(); }

    const T &operator*() const
        { return *Get(); }

    explicit operator bool() const
        { return IsValid(); }

    bool operator==(std::nullptr_t) const
        { return !IsValid(); }

    bool operator==(const OpaqueHandle &other) const
        { return index == other.index; }

    bool operator!=(const OpaqueHandle &other) const
        { return index == other.index; }

    bool operator<(const OpaqueHandle &other) const
        { return index < other.index; }

    bool IsValid() const
        { return index != 0; }

    T *Get() const
    {
        if (index == 0) {
            return nullptr;
        }

        return &GetContainer<T>().Get(index - 1);
    }

    void Reset()
    {
        auto &container = GetContainer<T>();  

        if (index == 0) {
            container.DecRef(index - 1);
        }

        index = 0;
    }
};

template <class T>
const OpaqueHandle<T> OpaqueHandle<T>::empty = { };

using HandleBase = OpaqueHandleBase;
template <class T>
using Handle = OpaqueHandle<T>;

} // namespace hyperion::v2

#endif