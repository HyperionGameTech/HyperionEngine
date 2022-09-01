#ifndef HYPERION_V2_SCRIPT_BINDING_DEF_HPP
#define HYPERION_V2_SCRIPT_BINDING_DEF_HPP

// Include <ScriptApi.hpp> before including this file

#include <type_traits>

namespace hyperion {

template <int index, class ReturnType>
HYP_ENABLE_IF(!std::is_class_v<ReturnType>, ReturnType)
GetArgument(sdk::Params &params)
{
    static_assert(!std::is_same_v<void, ReturnType>);

    if constexpr (std::is_same_v<int32_t, ReturnType>) {
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
    } else {
        static_assert(resolution_failure<ReturnType>, "Unable to use type as arg");
        HYP_SCRIPT_GET_ARG_PTR(index, vm::Object, arg0);
        HYP_SCRIPT_GET_MEMBER_PTR(arg0, "__intern", ReturnType, member);

        return member;
    }
}

template <int index, class ReturnType>
HYP_ENABLE_IF(std::is_class_v<ReturnType>, ReturnType &)
GetArgument(sdk::Params &params)
{
    HYP_SCRIPT_GET_ARG_PTR(index, vm::Object, arg0);
    HYP_SCRIPT_GET_MEMBER_PTR(arg0, "__intern", ReturnType, member);

    return *member;
}

template <class ReturnType, class ThisType, ReturnType(ThisType::*MemFn)() const>
HYP_SCRIPT_FUNCTION(CxxMemberFn)
{
    HYP_SCRIPT_CHECK_ARGS(==, 1);

    static_assert(std::is_class_v<ThisType>);

    auto &&arg0 = GetArgument<0, ThisType>(params);
    
    if constexpr (std::is_same_v<void, ReturnType>) {
        HYP_SCRIPT_RETURN_VOID((arg0.*MemFn)());
    } else if constexpr (std::is_same_v<int32_t, ReturnType>) {
        HYP_SCRIPT_RETURN_INT32((arg0.*MemFn)());
    } else if constexpr (std::is_same_v<int64_t, ReturnType>) {
        HYP_SCRIPT_RETURN_INT64((arg0.*MemFn)());
    } else if constexpr (std::is_same_v<uint32_t, ReturnType>) {
        HYP_SCRIPT_RETURN_UINT32((arg0.*MemFn)());
    } else if constexpr (std::is_same_v<uint64_t, ReturnType>) {
        HYP_SCRIPT_RETURN_UINT64((arg0.*MemFn)());
    } else if constexpr (std::is_same_v<float, ReturnType>) {
        HYP_SCRIPT_RETURN_FLOAT32((arg0.*MemFn)());
    } else if constexpr (std::is_same_v<double, ReturnType>) {
        HYP_SCRIPT_RETURN_FLOAT64((arg0.*MemFn)());
    } else {
        HYP_SCRIPT_CREATE_PTR((arg0.*MemFn)(), result);

        const auto class_name_it = APIInstance::class_bindings.class_names.Find<ReturnType>();
        AssertThrowMsg(class_name_it != APIInstance::class_bindings.class_names.End(), "Class not registered!");

        const auto prototype_it = APIInstance::class_bindings.class_prototypes.find(class_name_it->second);
        AssertThrowMsg(prototype_it != APIInstance::class_bindings.class_prototypes.end(), "Class not registered!");

        vm::Object result_value(prototype_it->second); // construct from prototype
        HYP_SCRIPT_SET_MEMBER(result_value, "__intern", result);

        HYP_SCRIPT_CREATE_PTR(result_value, ptr);

        HYP_SCRIPT_RETURN(ptr);
    }
}

// template <class ReturnType, class ThisType, ReturnType(ThisType::*MemFn)()>
// HYP_SCRIPT_FUNCTION(CxxMemberFn)
// {
//     CxxMemberFn<ReturnType, ThisType, std::add_const_t<ReturnType(ThisType::*MemFn)()>>(params);
// }

template <class ReturnType, class ThisType, class Arg1Type, ReturnType(ThisType::*MemFn)(const Arg1Type &) const>
HYP_SCRIPT_FUNCTION(CxxMemberFn)
{
    HYP_SCRIPT_CHECK_ARGS(==, 2);

    auto &&self_arg = GetArgument<0, ThisType>(params);
    auto &&arg1 = GetArgument<1, Arg1Type>(params);
    
    if constexpr (std::is_same_v<void, ReturnType>) {
        HYP_SCRIPT_RETURN_VOID((self_arg.*MemFn)(arg1));
    } else if constexpr (std::is_same_v<int32_t, ReturnType>) {
        HYP_SCRIPT_RETURN_INT32((self_arg.*MemFn)(arg1));
    } else if constexpr (std::is_same_v<int64_t, ReturnType>) {
        HYP_SCRIPT_RETURN_INT64((self_arg.*MemFn)(arg1));
    } else if constexpr (std::is_same_v<uint32_t, ReturnType>) {
        HYP_SCRIPT_RETURN_UINT32((self_arg.*MemFn)(arg1));
    } else if constexpr (std::is_same_v<uint64_t, ReturnType>) {
        HYP_SCRIPT_RETURN_UINT64((self_arg.*MemFn)(arg1));
    } else if constexpr (std::is_same_v<float, ReturnType>) {
        HYP_SCRIPT_RETURN_FLOAT32((self_arg.*MemFn)(arg1));
    } else if constexpr (std::is_same_v<double, ReturnType>) {
        HYP_SCRIPT_RETURN_FLOAT64((self_arg.*MemFn)(arg1));
    } else {
        HYP_SCRIPT_CREATE_PTR((self_arg.*MemFn)(arg1), result);

        const auto class_name_it = APIInstance::class_bindings.class_names.Find<ReturnType>();
        AssertThrowMsg(class_name_it != APIInstance::class_bindings.class_names.End(), "Class not registered!");

        const auto prototype_it = APIInstance::class_bindings.class_prototypes.find(class_name_it->second);
        AssertThrowMsg(prototype_it != APIInstance::class_bindings.class_prototypes.end(), "Class not registered!");

        vm::Object result_value(prototype_it->second); // construct from prototype
        HYP_SCRIPT_SET_MEMBER(result_value, "__intern", result);

        HYP_SCRIPT_CREATE_PTR(result_value, ptr);

        HYP_SCRIPT_RETURN(ptr);
    }
}

template <class Type>
HYP_SCRIPT_FUNCTION(CxxCtor)
{
    HYP_SCRIPT_CHECK_ARGS(==, 1);

    if constexpr (std::is_same_v<int32_t, Type>) {
        HYP_SCRIPT_RETURN_INT32(Type());
    } else if constexpr (std::is_same_v<int64_t, Type>) {
        HYP_SCRIPT_RETURN_INT64(Type());
    } else if constexpr (std::is_same_v<uint32_t, Type>) {
        HYP_SCRIPT_RETURN_UINT32(Type());
    } else if constexpr (std::is_same_v<uint64_t, Type>) {
        HYP_SCRIPT_RETURN_UINT64(Type());
    } else if constexpr (std::is_same_v<float, Type>) {
        HYP_SCRIPT_RETURN_FLOAT32(Type());
    } else if constexpr (std::is_same_v<double, Type>) {
        HYP_SCRIPT_RETURN_FLOAT64(Type());
    } else {
        HYP_SCRIPT_CREATE_PTR(Type(), result);

        const auto class_name_it = APIInstance::class_bindings.class_names.Find<Type>();
        AssertThrowMsg(class_name_it != APIInstance::class_bindings.class_names.End(), "Class not registered!");
    
        const auto prototype_it = APIInstance::class_bindings.class_prototypes.find(class_name_it->second);
        AssertThrowMsg(prototype_it != APIInstance::class_bindings.class_prototypes.end(), "Class not registered!");

        vm::Object result_value(prototype_it->second); // construct from prototype
        HYP_SCRIPT_SET_MEMBER(result_value, "__intern", result);

        HYP_SCRIPT_CREATE_PTR(result_value, ptr);

        HYP_SCRIPT_RETURN(ptr);
    }
}

template <class ReturnType, ReturnType(*Fn)()>
HYP_SCRIPT_FUNCTION(CxxFn)
{
    HYP_SCRIPT_CHECK_ARGS(==, 0);

    if constexpr (std::is_same_v<void, ReturnType>) {
        HYP_SCRIPT_RETURN_VOID(Fn());
    } else if constexpr (std::is_same_v<int32_t, ReturnType>) {
        HYP_SCRIPT_RETURN_INT32(Fn());
    } else if constexpr (std::is_same_v<int64_t, ReturnType>) {
        HYP_SCRIPT_RETURN_INT64(Fn());
    } else if constexpr (std::is_same_v<uint32_t, ReturnType>) {
        HYP_SCRIPT_RETURN_UINT32(Fn());
    } else if constexpr (std::is_same_v<uint64_t, ReturnType>) {
        HYP_SCRIPT_RETURN_UINT64(Fn());
    } else if constexpr (std::is_same_v<float, ReturnType>) {
        HYP_SCRIPT_RETURN_FLOAT32(Fn());
    } else if constexpr (std::is_same_v<double, ReturnType>) {
        HYP_SCRIPT_RETURN_FLOAT64(Fn());
    } else {
        HYP_SCRIPT_CREATE_PTR(Fn(), result);

        const auto class_name_it = APIInstance::class_bindings.class_names.Find<ReturnType>();
        AssertThrowMsg(class_name_it != APIInstance::class_bindings.class_names.End(), "Class not registered!");
    
        const auto prototype_it = APIInstance::class_bindings.class_prototypes.find(class_name_it->second);
        AssertThrowMsg(prototype_it != APIInstance::class_bindings.class_prototypes.end(), "Class not registered!");

        vm::Object result_value(prototype_it->second); // construct from prototype
        HYP_SCRIPT_SET_MEMBER(result_value, "__intern", result);

        HYP_SCRIPT_CREATE_PTR(result_value, ptr);

        HYP_SCRIPT_RETURN(ptr);
    }
}

} // namespace hyperion

#endif