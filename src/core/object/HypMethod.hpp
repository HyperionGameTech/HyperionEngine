/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_HYP_METHOD_HPP
#define HYPERION_CORE_HYP_METHOD_HPP

#include <core/object/HypData.hpp>
#include <core/object/HypClassAttribute.hpp>

#include <core/Defines.hpp>
#include <core/Name.hpp>

#include <core/functional/Proc.hpp>

#include <core/utilities/TypeID.hpp>
#include <core/utilities/EnumFlags.hpp>

#include <core/memory/Any.hpp>
#include <core/memory/AnyRef.hpp>

#include <asset/serialization/Serialization.hpp>

#include <Types.hpp>

namespace hyperion {

class HypClass;

struct HypMethodParameter
{
    TypeID  type_id;
};

namespace detail {

#pragma region CallHypMethod

template <class FunctionType, class ReturnType, class... ArgTypes, SizeType... Indices>
decltype(auto) CallHypMethod_Impl(FunctionType fn, HypData **args, std::index_sequence<Indices...>)
{
    auto AssertArgType = [args]<SizeType Index>(std::integral_constant<SizeType, Index>) -> bool
    {
        const bool condition = args[Index]->Is< NormalizedType< typename TupleElement< Index, ArgTypes... >::Type > >();

        if (!condition) {
            HYP_FAIL("Invalid argument at index %u: Expected %s",
                Index, TypeName< NormalizedType< typename TupleElement< Index, ArgTypes... >::Type > >().Data());
        }

        return condition;
    };

    (void)(AssertArgType(std::integral_constant<SizeType, Indices>{}) && ...);

    return fn(args[Indices]->template Get<NormalizedType<ArgTypes>>()...);
}

template <class FunctionType, class ReturnType, class... ArgTypes>
decltype(auto) CallHypMethod(FunctionType fn, HypData **args)
{
    return CallHypMethod_Impl<FunctionType, ReturnType, ArgTypes...>(fn, args, std::make_index_sequence<sizeof...(ArgTypes)>{});
}

template <class FunctionType, class ReturnType, class ArgsTupleType>
struct CallHypMethod_Tuple;

template <class FunctionType, class ReturnType, class... ArgTypes>
struct CallHypMethod_Tuple<FunctionType, ReturnType, Tuple<ArgTypes...>>
{
    decltype(auto) operator()(FunctionType fn, Span<HypData> args) const
    {
        return CallHypMethod<FunctionType, ReturnType, ArgTypes...>(fn, args);
    }
};

#pragma endregion CallHypMethod

#pragma region InitHypMethodParams

template <class ReturnType, class ThisType, class... ArgTypes, SizeType... Indices>
void InitHypMethodParams_Impl(Array<HypMethodParameter> &out_params, std::index_sequence<Indices...>)
{
    auto AddParameter = [&out_params]<SizeType Index>(std::integral_constant<SizeType, Index>) -> bool
    {
        out_params.PushBack(HypMethodParameter {
            TypeID::ForType< NormalizedType< typename TupleElement< Index, ArgTypes... >::Type > >()
        });

        return true;
    };

    (void)(AddParameter(std::integral_constant<SizeType, Indices>{}) && ...);

    if constexpr (!std::is_void_v<ThisType>) {
        out_params.PushBack(HypMethodParameter {
            TypeID::ForType<NormalizedType<ThisType>>()
        });
    }
}

template <class ReturnType, class ThisType, class... ArgTypes>
void InitHypMethodParams(Array<HypMethodParameter> &out_params)
{
    InitHypMethodParams_Impl<ReturnType, ThisType, ArgTypes...>(out_params, std::make_index_sequence<sizeof...(ArgTypes)>{});
}

template <class ReturnType, class ThisType, class ArgsTupleType>
struct InitHypMethodParams_Tuple;

template <class ReturnType, class ThisType, class... ArgTypes>
struct InitHypMethodParams_Tuple<ReturnType, ThisType, Tuple<ArgTypes...>>
{
    void operator()(Array<HypMethodParameter> &out_params) const
    {
        InitHypMethodParams<ReturnType, ThisType, ArgTypes...>(out_params);
    }
};

#pragma endregion InitHypMethodParams

} // namespace detail

enum class HypMethodFlags : uint32
{
    NONE    = 0x0,
    STATIC  = 0x1,
    MEMBER  = 0x2
};

HYP_MAKE_ENUM_FLAGS(HypMethodFlags)

namespace detail {

template <class FunctionType, class EnableIf = void>
struct HypMethodHelper;

template <class FunctionType>
struct HypMethodHelper<FunctionType, std::enable_if_t< FunctionTraits<FunctionType>::is_member_function && !FunctionTraits<FunctionType>::is_functor > >
{
    using ThisType = typename FunctionTraits<FunctionType>::ThisType;
    using ReturnType = typename FunctionTraits<FunctionType>::ReturnType;
    using ArgTypes = typename FunctionTraits<FunctionType>::ArgTypes;

    static constexpr EnumFlags<HypMethodFlags> flags = HypMethodFlags::MEMBER;
    static constexpr uint32 num_args = FunctionTraits<FunctionType>::num_args + 1;
};

template <class FunctionType>
struct HypMethodHelper<FunctionType, std::enable_if_t< !FunctionTraits<FunctionType>::is_member_function || FunctionTraits<FunctionType>::is_functor > >
{
    using ThisType = void;
    using ReturnType = typename FunctionTraits<FunctionType>::ReturnType;
    using ArgTypes = typename FunctionTraits<FunctionType>::ArgTypes;

    static constexpr EnumFlags<HypMethodFlags> flags = HypMethodFlags::STATIC;
    static constexpr uint32 num_args = FunctionTraits<FunctionType>::num_args;
};

} // namespace detail

struct HypMethod
{
    Name                                                name;
    TypeID                                              return_type_id;
    Array<HypMethodParameter>                           params;
    EnumFlags<HypMethodFlags>                           flags;
    HashMap<String, String>                             attributes;

    Proc<HypData, Span<HypData>>                        proc;
    Proc<fbom::FBOMData, Span<HypData>>                 serialize_proc;
    Proc<void, Span<HypData>, const fbom::FBOMData &>   deserialize_proc;

    HypMethod(Span<HypClassAttribute> attributes = {})
        : name(Name::Invalid()),
          return_type_id(TypeID::Void()),
          flags(HypMethodFlags::NONE),
          attributes(HypClassAttribute::ToHashMap(attributes))
    {
    }

    // template <class FunctionType, class HelperType = typename detail::HypMethodHelper<FunctionType>, typename = std::enable_if_t< !std::is_member_function_pointer_v<FunctionType> > >
    // HypMethod(Name name, FunctionType &&fn, Span<HypClassAttribute> attributes = {})
    //     : name(name),
    //       flags(HelperType::flags),
    //       attributes(HypClassAttribute::ToHashMap(attributes)),
    //       proc([fn = std::forward<FunctionType>(fn)](Span<HypData> args) -> HypData
    //       {
    //           AssertThrow(args.Size() == HelperType::num_args);

    //           if constexpr (std::is_void_v<typename HelperType::ReturnType>) {
    //               detail::CallHypMethod_Tuple<decltype(fn), typename HelperType::ReturnType, typename HelperType::ArgTypes >{}(fn, args);

    //               return HypData();
    //           } else {
    //               return HypData(detail::CallHypMethod_Tuple<decltype(fn), typename HelperType::ReturnType, typename HelperType::ArgTypes >{}(fn, args));
    //           }
    //       }),
    //       serialize_proc([fn = std::forward<FunctionType>(fn)](Span<HypData> args) -> fbom::FBOMData
    //       {
    //           AssertThrow(args.Size() == HelperType::num_args);

    //           if constexpr (std::is_void_v<typename HelperType::ReturnType>) {
    //               HYP_FAIL("Cannot serialize void return type or non-zero number of arguments");
    //           } else {
    //               fbom::FBOMData out;

    //               if (fbom::FBOMResult err = HypDataHelper<NormalizedType<ReturnType>>::Serialize(HypData(detail::CallHypMethod_Tuple<decltype(fn), typename HelperType::ReturnType, typename HelperType::ArgTypes >{}(fn, args)), out)) {
    //                   HYP_FAIL("Failed to serialize data: %s", err.message.Data());
    //               }

    //               return out;
    //           }
    //       }),
    //       deserialize_proc([fn = std::forward<FunctionType>(fn)](Span<HypData> args, const fbom::FBOMData &data) -> void
    //       {
    //           HypData value;

    //           if (fbom::FBOMResult err = HypDataHelper<NormalizedType<typename HelperType::ReturnType>>::Deserialize(data, value)) {
    //               HYP_FAIL("Failed to deserialize data: %s", err.message.Data());
    //           }

    //           if constexpr (std::is_void_v<typename HelperType::ReturnType>) {
    //               detail::CallHypMethod_Tuple<decltype(fn), typename HelperType::ReturnType, typename HelperType::ArgTypes >{}(fn, value);
    //           } else {
    //               HYP_FAIL("Cannot deserialize non-void return type");
    //           }
    //       })

    // {
    //     return_type_id = TypeID::ForType<NormalizedType< typename HelperType::ReturnType >>();

    //     params.Reserve(HelperType::num_args);
    //     detail::InitHypMethodParams_Tuple< typename HelperType::ReturnType, typename HelperType::ThisType, typename HelperType::ArgTypes >{}(params);
    // }

    template <class ReturnType, class TargetType, class... ArgTypes>
    HypMethod(Name name, ReturnType(TargetType::*mem_fn)(ArgTypes...), Span<HypClassAttribute> attributes = {})
        : name(name),
          flags(HypMethodFlags::MEMBER),
          attributes(HypClassAttribute::ToHashMap(attributes)),
          proc([mem_fn](Span<HypData> args) -> HypData
          {
              AssertThrow(args.Size() == sizeof...(ArgTypes) + 1);

              const auto fn = [mem_fn](TargetType *target, ArgTypes... args) -> ReturnType
              {
                  return (target->*mem_fn)(args...);
              };

              HypData **arg_ptrs = (HypData **)StackAlloc(args.Size() * sizeof(HypData *));
              for (SizeType i = 0; i < args.Size(); ++i) {
                  arg_ptrs[i] = &args[i];
              }

              if constexpr (std::is_void_v<ReturnType>) {
                  detail::CallHypMethod<decltype(fn), ReturnType, TargetType *, ArgTypes...>(fn, arg_ptrs);

                  return HypData();
              } else {
                  return HypData(detail::CallHypMethod<decltype(fn), ReturnType, TargetType *, ArgTypes...>(fn, arg_ptrs));
              }
          })
    {
        return_type_id = TypeID::ForType<NormalizedType< ReturnType >>();

        params.Reserve(sizeof...(ArgTypes) + 1);
        detail::InitHypMethodParams_Tuple< ReturnType, TargetType, Tuple<ArgTypes...> >{}(params);

        if (this->attributes.Contains("serializeas")) {
            serialize_proc = [mem_fn](Span<HypData> args) -> fbom::FBOMData
            {
                AssertThrow(args.Size() == sizeof...(ArgTypes) + 1);

                const auto fn = [mem_fn](TargetType *target, ArgTypes... args) -> ReturnType
                {
                    return (target->*mem_fn)(args...);
                };

                HypData **arg_ptrs = (HypData **)StackAlloc(args.Size() * sizeof(HypData *));
                for (SizeType i = 0; i < args.Size(); ++i) {
                    arg_ptrs[i] = &args[i];
                }

                if constexpr (std::is_void_v<ReturnType> || sizeof...(ArgTypes) != 0) {
                    HYP_FAIL("Cannot serialize void return type or non-zero number of arguments");
                } else {
                    fbom::FBOMData out;

                    if (fbom::FBOMResult err = HypDataHelper<NormalizedType<ReturnType>>::Serialize(HypData(detail::CallHypMethod<decltype(fn), ReturnType, TargetType *, ArgTypes...>(fn, arg_ptrs)), out)) {
                        HYP_FAIL("Failed to serialize data: %s", err.message.Data());
                    }

                    return out;
                }
            };

            deserialize_proc = [mem_fn](Span<HypData> args, const fbom::FBOMData &data) -> void
            {
                AssertThrow(args.Size() == sizeof...(ArgTypes));

                if constexpr (sizeof...(ArgTypes) >= 1) {
                    const auto fn = [mem_fn](TargetType *target, ArgTypes... args) -> ReturnType
                    {
                        return (target->*mem_fn)(args...);
                    };

                    HypData value;

                    if (fbom::FBOMResult err = HypDataHelper<NormalizedType<typename TupleElement<sizeof...(ArgTypes) - 1, ArgTypes...>::Type>>::Deserialize(data, value)) {
                        HYP_FAIL("Failed to deserialize data: %s", err.message.Data());
                    }

                    HypData **arg_ptrs = (HypData **)StackAlloc((args.Size() + 1) * sizeof(HypData *));
                    for (SizeType i = 0; i < args.Size(); ++i) {
                        arg_ptrs[i] = &args[i];
                    }

                    arg_ptrs[args.Size()] = &value;

                    detail::CallHypMethod<decltype(fn), ReturnType, TargetType *, ArgTypes...>(fn, arg_ptrs);
                } else {
                    HYP_FAIL("Cannot deserialize using non-setter method");
                }
            };
        }
    }
    
    template <class ReturnType, class TargetType, class... ArgTypes>
    HypMethod(Name name, ReturnType(TargetType::*mem_fn)(ArgTypes...) const, Span<HypClassAttribute> attributes = {})
        : name(name),
          flags(HypMethodFlags::MEMBER),
          attributes(HypClassAttribute::ToHashMap(attributes)),
          proc([mem_fn](Span<HypData> args) -> HypData
          {
              AssertThrow(args.Size() == sizeof...(ArgTypes) + 1);

              // replace member function with free function using target pointer as first arg
              const auto fn = [mem_fn](TargetType *target, ArgTypes... args) -> ReturnType
              {
                  return (target->*mem_fn)(args...);
              };

              HypData **arg_ptrs = (HypData **)StackAlloc(args.Size() * sizeof(HypData *));
              for (SizeType i = 0; i < args.Size(); ++i) {
                  arg_ptrs[i] = &args[i];
              }

              if constexpr (std::is_void_v<ReturnType>) {
                  detail::CallHypMethod<decltype(fn), ReturnType, TargetType *, ArgTypes...>(fn, arg_ptrs);

                  return HypData();
              } else {
                  return HypData(detail::CallHypMethod<decltype(fn), ReturnType, TargetType *, ArgTypes...>(fn, arg_ptrs));
              }
          })
    {
        return_type_id = TypeID::ForType<NormalizedType< ReturnType >>();

        params.Reserve(sizeof...(ArgTypes) + 1);
        detail::InitHypMethodParams_Tuple< ReturnType, TargetType, Tuple<ArgTypes...> >{}(params);

        if (this->attributes.Contains("serializeas")) {
            serialize_proc = [mem_fn](Span<HypData> args) -> fbom::FBOMData
            {
                AssertThrow(args.Size() == sizeof...(ArgTypes) + 1);

                const auto fn = [mem_fn](TargetType *target, ArgTypes... args) -> ReturnType
                {
                    return (target->*mem_fn)(args...);
                };

                HypData **arg_ptrs = (HypData **)StackAlloc(args.Size() * sizeof(HypData *));
                for (SizeType i = 0; i < args.Size(); ++i) {
                    arg_ptrs[i] = &args[i];
                }

                if constexpr (std::is_void_v<ReturnType> || sizeof...(ArgTypes) != 0) {
                    HYP_FAIL("Cannot serialize void return type or non-zero number of arguments");
                } else {
                    fbom::FBOMData out;

                    if (fbom::FBOMResult err = HypDataHelper<NormalizedType<ReturnType>>::Serialize(HypData(detail::CallHypMethod<decltype(fn), ReturnType, TargetType *, ArgTypes...>(fn, arg_ptrs)), out)) {
                        HYP_FAIL("Failed to serialize data: %s", err.message.Data());
                    }

                    return out;
                }
            };

            deserialize_proc = [mem_fn](Span<HypData> args, const fbom::FBOMData &data) -> void
            {
                AssertThrow(args.Size() == sizeof...(ArgTypes));

                if constexpr (sizeof...(ArgTypes) >= 1) {
                    const auto fn = [mem_fn](TargetType *target, ArgTypes... args) -> ReturnType
                    {
                        return (target->*mem_fn)(args...);
                    };

                    HypData value;

                    if (fbom::FBOMResult err = HypDataHelper<NormalizedType<typename TupleElement<sizeof...(ArgTypes) - 1, ArgTypes...>::Type>>::Deserialize(data, value)) {
                        HYP_FAIL("Failed to deserialize data: %s", err.message.Data());
                    }

                    HypData **arg_ptrs = (HypData **)StackAlloc((args.Size() + 1) * sizeof(HypData *));
                    for (SizeType i = 0; i < args.Size(); ++i) {
                        arg_ptrs[i] = &args[i];
                    }

                    arg_ptrs[args.Size()] = &value;

                    detail::CallHypMethod<decltype(fn), ReturnType, TargetType *, ArgTypes...>(fn, arg_ptrs);
                } else {
                    HYP_FAIL("Cannot deserialize using non-setter method");
                }
            };
        }
    }

    HypMethod(const HypMethod &other)                   = delete;
    HypMethod &operator=(const HypMethod &other)        = delete;
    HypMethod(HypMethod &&other) noexcept               = default;
    HypMethod &operator=(HypMethod &&other) noexcept    = default;
    ~HypMethod()                                        = default;

    HYP_FORCE_INLINE const String *GetAttribute(UTF8StringView key) const
    {
        auto it = attributes.FindAs(key);

        if (it == attributes.End()) {
            return nullptr;
        }

        return &it->second;
    }

    HYP_FORCE_INLINE HypData Invoke(Span<HypData> args) const
        { return proc(args); }

    HYP_FORCE_INLINE fbom::FBOMData Invoke_Serialized(Span<HypData> args) const
    {
        AssertThrowMsg(serialize_proc.IsValid(), "Method %s does not support serialization", name.LookupString());

        return serialize_proc(args);
    }

    HYP_FORCE_INLINE void Invoke_Deserialized(Span<HypData> args, const fbom::FBOMData &data) const
    {
        AssertThrowMsg(deserialize_proc.IsValid(), "Method %s does not support deserialization", name.LookupString());

        deserialize_proc(args, data);
    }
};

} // namespace hyperion

#endif