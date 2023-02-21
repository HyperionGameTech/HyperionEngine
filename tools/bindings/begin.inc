#ifndef HYPERION_V2_SCRIPT_BINDING_DEF_GENERATED_HPP
#define HYPERION_V2_SCRIPT_BINDING_DEF_GENERATED_HPP

// Include <ScriptApi.hpp> before including this file

#include <type_traits>

namespace hyperion {

template <class T>
struct GetArgumentImpl
{
    static_assert(resolution_failure<T>, "Unable to use type as arg");
};

template <>
struct GetArgumentImpl<Int32>
{
    auto operator()(Int index, sdk::Params &params)
    {
        HYP_SCRIPT_GET_ARG_INT(index, arg_value);

        return arg_value;
    }
};

template <>
struct GetArgumentImpl<Int64>
{
    auto operator()(Int index, sdk::Params &params)
    {
        HYP_SCRIPT_GET_ARG_INT(index, arg_value);

        return arg_value;
    }
};

template <>
struct GetArgumentImpl<UInt32>
{
    auto operator()(Int index, sdk::Params &params)
    {
        HYP_SCRIPT_GET_ARG_UINT(index, arg_value);

        return arg_value;
    }
};

template <>
struct GetArgumentImpl<UInt64>
{
    auto operator()(Int index, sdk::Params &params)
    {
        HYP_SCRIPT_GET_ARG_UINT(index, arg_value);

        return arg_value;
    }
};

template <>
struct GetArgumentImpl<Float>
{
    auto operator()(Int index, sdk::Params &params)
    {
        HYP_SCRIPT_GET_ARG_FLOAT(index, arg_value);

        return arg_value;
    }
};

template <>
struct GetArgumentImpl<Double>
{
    auto operator()(Int index, sdk::Params &params)
    {
        HYP_SCRIPT_GET_ARG_FLOAT(index, arg_value);

        return arg_value;
    }
};

template <>
struct GetArgumentImpl<bool>
{
    auto operator()(Int index, sdk::Params &params)
    {
        HYP_SCRIPT_GET_ARG_BOOLEAN(index, arg_value);

        return arg_value;
    }
};

template <>
struct GetArgumentImpl<void *>
{
    auto operator()(Int index, sdk::Params &params)
    {
        HYP_SCRIPT_GET_ARG_PTR_RAW(index, void, arg_value);

        return arg_value;
    }
};

template <>
struct GetArgumentImpl<vm::VMString>
{
    auto operator()(Int index, sdk::Params &params)
    {
        HYP_SCRIPT_GET_ARG_PTR(index, vm::VMString, arg_value);

        return arg_value;
    }
};

template <>
struct GetArgumentImpl<vm::VMObject>
{
    auto operator()(Int index, sdk::Params &params)
    {
        HYP_SCRIPT_GET_ARG_PTR(index, vm::VMObject, arg_value);

        return arg_value;
    }
};

template <>
struct GetArgumentImpl<vm::VMArray>
{
    auto operator()(Int index, sdk::Params &params)
    {
        HYP_SCRIPT_GET_ARG_PTR(index, vm::VMArray, arg_value);

        return arg_value;
    }
};

template <>
struct GetArgumentImpl<vm::VMStruct>
{
    auto operator()(Int index, sdk::Params &params)
    {
        HYP_SCRIPT_GET_ARG_PTR(index, vm::VMStruct, arg_value);

        return arg_value;
    }
};

template <class T>
constexpr bool is_vm_object_type = std::is_same_v<vm::VMString, T> || std::is_same_v<vm::VMObject, T> || std::is_same_v<vm::VMArray, T> || std::is_same_v<vm::VMStruct, T>;

template <int index, class ReturnType>
typename std::enable_if_t<
    !std::is_class_v<NormalizedType<ReturnType>> || is_vm_object_type<ReturnType>,
    std::conditional_t<std::is_class_v<ReturnType>, std::add_pointer_t<ReturnType>, ReturnType>
>
GetArgument(sdk::Params &params)
{
    static_assert(!std::is_same_v<void, ReturnType>);

    /*if constexpr (std::is_same_v<int32_t, ReturnType>) {
        HYP_SCRIPT_GET_ARG_INT(index, arg0);

        return arg0;
    } else if constexpr (std::is_same_v<int64_t, ReturnType>) {
        HYP_SCRIPT_GET_ARG_INT(index, arg0);

        return arg0;
    } else if constexpr (std::is_same_v<uint32_t, ReturnType>) {
        HYP_SCRIPT_GET_ARG_UINT(index, arg0);

        return arg0;
    } else if constexpr (std::is_same_v<uint64_t, ReturnType>) {
        HYP_SCRIPT_GET_ARG_UINT(index, arg0);

        return arg0;
    } else if constexpr (std::is_same_v<float, ReturnType>) {
        HYP_SCRIPT_GET_ARG_FLOAT(index, arg0);

        return arg0;
    } else if constexpr (std::is_same_v<double, ReturnType>) {
        HYP_SCRIPT_GET_ARG_FLOAT(index, arg0);

        return arg0;
    } else if constexpr (std::is_same_v<bool, ReturnType>) {
        HYP_SCRIPT_GET_ARG_BOOLEAN(index, arg0);

        return arg0;
    } else if constexpr (std::is_pointer_v<ReturnType>) {
        using ReturnTypeWithoutPtr = std::remove_pointer_t<ReturnType>;
        HYP_SCRIPT_GET_ARG_PTR_RAW(index, ReturnTypeWithoutPtr, arg0);

        return arg0;
    } else if constexpr (std::is_class_v<ReturnType>) {
        HYP_SCRIPT_GET_ARG_PTR(index, ReturnType, arg0);

        return arg0;
    } else {
        static_assert(resolution_failure<ReturnType>, "Unable to use type as arg");
    }*/

    return GetArgumentImpl<ReturnType>()(index, params);
}

template <int index, class ReturnType>
typename std::enable_if_t<
    std::is_class_v<NormalizedType<ReturnType>> && !(is_vm_object_type<NormalizedType<ReturnType>>),
    NormalizedType<ReturnType> &
>
GetArgument(sdk::Params &params)
{
    HYP_SCRIPT_GET_ARG_PTR(index, vm::VMObject, arg0);
    HYP_SCRIPT_GET_MEMBER_PTR(arg0, "__intern", NormalizedType<ReturnType>, member);

    return *member;
}