#ifndef HYPERION_V2_RUNTIME_DOTNET_MANAGED_HANDLE_HPP
#define HYPERION_V2_RUNTIME_DOTNET_MANAGED_HANDLE_HPP

#include <core/lib/TypeID.hpp>
#include <core/Handle.hpp>
#include <core/ID.hpp>

#include <type_traits>

namespace hyperion::v2 {

extern "C" struct ManagedHandle
{
    UInt32  id;

    // Called from C# to increment the reference count
    // when an object is constructed with the handle
    void IncRef(UInt32 type_id);

    // Called from C# to release the handle
    // and decrement the reference count
    void DecRef(UInt32 type_id);
};

static_assert(sizeof(ManagedHandle) == 4, "ManagedHandle must be 4 bytes to match C# struct size");
static_assert(std::is_trivial_v<ManagedHandle>, "ManagedHandle must be a trivial type to be used in C#");

template <class T>
static inline ManagedHandle CreateManagedHandleFromHandle(Handle<T> handle)
{
    ManagedHandle result { handle.GetID().Value() };

    // Take ownership of the handle,
    // but do not increment the reference count
    handle.index = 0;

    return result;
}

template <class T>
static inline Handle<T> CreateHandleFromManagedHandle(ManagedHandle handle)
{
    return Handle<T>(ID<T> { handle.id });
}

} // namespace hyperion::v2

#endif