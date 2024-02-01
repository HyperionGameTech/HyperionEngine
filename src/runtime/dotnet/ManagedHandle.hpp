#ifndef HYPERION_V2_RUNTIME_DOTNET_MANAGED_HANDLE_HPP
#define HYPERION_V2_RUNTIME_DOTNET_MANAGED_HANDLE_HPP

#include <core/lib/TypeID.hpp>
#include <core/Handle.hpp>
#include <core/ID.hpp>

#include <type_traits>

namespace hyperion::v2 {

extern "C" struct ManagedHandle
{
    UInt32  type_id;
    UInt32  id;

    // Called from C# to release the handle
    // and decrement the reference count
    void Dispose();
};

static_assert(std::is_trivial_v<ManagedHandle>, "ManagedHandle must be a trivial type to be used in C#");

template <class T>
static inline ManagedHandle CreateManagedHandleFromHandle(Handle<T> &&handle)
{
    ManagedHandle result { TypeID::ForType<T>().Value(), handle.GetID().Value() };

    // Take ownership of the handle,
    // but do not increment the reference count
    handle.index = 0;

    return result;
}

template <class T>
static inline Handle<T> CreateHandleFromManagedHandle(ManagedHandle handle)
{
    AssertThrowMsg(handle.type_id == TypeID::ForType<T>().Value(), "Type mismatch");

    return Handle<T>(ID<T> { handle.id });
}

} // namespace hyperion::v2

#endif