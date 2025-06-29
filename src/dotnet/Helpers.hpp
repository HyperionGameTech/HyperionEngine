#ifndef HYPERION_DOTNET_HELPERS_HPP
#define HYPERION_DOTNET_HELPERS_HPP

#include <core/Defines.hpp>

#include <core/containers/String.hpp>

#include <core/object/HypObjectFwd.hpp>
#include <core/object/HypData.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <core/Handle.hpp>
#include <core/object/ObjId.hpp>

#include <type_traits>

namespace hyperion {
namespace filesystem {

class FilePath;

} // namespace filesystem

using filesystem::FilePath;

} // namespace hyperion

namespace hyperion::dotnet {

class Object;

template <class T, class T2 = void>
struct TransformArgument;

template <class T>
struct DefaultTransformArgument
{
    HYP_FORCE_INLINE T operator()(T value) const
    {
        return value;
    }
};

template <class T>
struct TransformArgument<T, std::enable_if_t<std::is_arithmetic_v<T>>> : DefaultTransformArgument<T>
{
};

template <class T>
struct TransformArgument<T, std::enable_if_t<std::is_class_v<T> && is_pod_type<T>>> : DefaultTransformArgument<T>
{
};

template <class T>
struct TransformArgument<T, std::enable_if_t<std::is_enum_v<T>>> : DefaultTransformArgument<T>
{
};

template <>
struct TransformArgument<void*> : DefaultTransformArgument<void*>
{
};

template <>
struct TransformArgument<char*> : DefaultTransformArgument<char*>
{
};

template <>
struct TransformArgument<const char*> : DefaultTransformArgument<const char*>
{
};

template <class T>
struct TransformArgument<ObjId<T>>
{
    HYP_FORCE_INLINE typename ObjId<T>::ValueType operator()(ObjId<T> id) const
    {
        return id.Value();
    }
};

// template <>
// struct TransformArgument<Object *>
// {
//     HYP_API void *operator()(Object *value) const;
// };

// template <class T>
// struct TransformArgument<T *, std::enable_if_t<IsHypObject<T>::value>>
// {
//     HYP_FORCE_INLINE void *operator()(T *value) const
//     {
//         if (!value) {
//             return nullptr;
//         }

//         return TransformArgument<Object *>{}(value->GetManagedObject());
//     }
// };

// template <class T>
// struct TransformArgument<RC<T>, std::enable_if_t<IsHypObject<T>::value>>
// {
//     HYP_FORCE_INLINE void *operator()(const RC<T> &value) const
//     {
//         if (!value) {
//             return nullptr;
//         }

//         return TransformArgument<Object *>{}(value->GetManagedObject());
//     }
// };

// template <class T>
// struct TransformArgument<Handle<T>, std::enable_if_t<IsHypObject<T>::value>>
// {
//     HYP_FORCE_INLINE void *operator()(const Handle<T> &value) const
//     {
//         if (!value) {
//             return nullptr;
//         }

//         return TransformArgument<Object *>{}(value->GetManagedObject());
//     }
// };

template <>
struct TransformArgument<String>
{
    HYP_API const char* operator()(const String& value) const
    {
        return value.Data();
    }
};

template <>
struct TransformArgument<UTF8StringView>
{
    HYP_API const char* operator()(UTF8StringView value) const
    {
        return value.Data();
    }
};

template <>
struct TransformArgument<ANSIString>
{
    HYP_API const char* operator()(const ANSIString& value) const
    {
        return value.Data();
    }
};

template <>
struct TransformArgument<ANSIStringView>
{
    HYP_API const char* operator()(ANSIStringView value) const
    {
        return value.Data();
    }
};

template <>
struct TransformArgument<FilePath>
{
    HYP_API const char* operator()(const FilePath& value) const;
};

// Conditionally construct or reference existing HypData
template <class T>
static inline const HypData* SetArg_HypData(HypData* arr, SizeType index, T&& arg)
{
    if constexpr (is_hypdata_v<T>)
    {
        return &arg;
    }
    else
    {
        new (&arr[index]) HypData(std::forward<T>(arg));
        return &arr[index];
    }
}

// NOLINTBEGIN
// ^^^ clang-lint wants to treat this as a global variable?
// Expand over each argument to fill args_array and args_array_ptr
template <class... Args, SizeType... Indices>
static inline void SetArgs_HypData(std::index_sequence<Indices...>, HypData* arr, const HypData* (&array_ptr)[sizeof...(Args) + 1], Args&&... args)
{
    ((array_ptr[Indices] = SetArg_HypData(arr, Indices, std::forward<Args>(args))), ...);
    array_ptr[sizeof...(Args)] = nullptr;
}

// NOLINTEND

} // namespace hyperion::dotnet

#endif