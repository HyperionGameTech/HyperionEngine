/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_HYP_METHOD_HPP
#define HYPERION_CORE_HYP_METHOD_HPP

#include <core/object/HypData.hpp>
#include <core/object/HypClassAttribute.hpp>
#include <core/object/HypMemberFwd.hpp>

#include <core/functional/Proc.hpp>

#include <core/utilities/TypeID.hpp>
#include <core/utilities/EnumFlags.hpp>

#include <core/Defines.hpp>
#include <core/Name.hpp>
#include <core/Traits.hpp>

#include <core/serialization/Serialization.hpp>

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
        const bool condition = args[Index]->Is< NormalizedType< typename TupleElement< Index, ArgTypes... >::Type > >(/* strict */ false);

        if (!condition) {
            HYP_FAIL("Invalid argument at index %u: Expected %s, Got TypeID %u",
                Index, TypeName< NormalizedType< typename TupleElement< Index, ArgTypes... >::Type > >().Data(),
                args[Index]->GetTypeID().Value());
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

#define HYP_METHOD_MEMBER_FN_WRAPPER(_mem_fn) \
    [_mem_fn](TargetType *target, ArgTypes... args) -> ReturnType \
    { \
        AssertThrow(target != nullptr); \
        \
        return (target->*_mem_fn)(args...); \
    }

class HypMethod : public IHypMember
{
public:
    HypMethod(Span<const HypClassAttribute> attributes = {})
        : m_name(Name::Invalid()),
          m_return_type_id(TypeID::Void()),
          m_target_type_id(TypeID::Void()),
          m_flags(HypMethodFlags::NONE),
          m_attributes(attributes)
    {
    }

    template <class ReturnType, class TargetType, class... ArgTypes>
    HypMethod(Name name, ReturnType(TargetType::*mem_fn)(ArgTypes...), Span<const HypClassAttribute> attributes = {})
        : m_name(name),
          m_flags(HypMethodFlags::MEMBER),
          m_attributes(attributes),
          m_proc([mem_fn](HypData **args, SizeType num_args) -> HypData
          {
              AssertThrow(num_args == sizeof...(ArgTypes) + 1);

              const auto fn = HYP_METHOD_MEMBER_FN_WRAPPER(mem_fn);

              if constexpr (std::is_void_v<ReturnType>) {
                  detail::CallHypMethod<decltype(fn), ReturnType, TargetType *, ArgTypes...>(fn, args);

                  return HypData();
              } else {
                  return HypData(detail::CallHypMethod<decltype(fn), ReturnType, TargetType *, ArgTypes...>(fn, args));
              }
          })
    {
        m_return_type_id = TypeID::ForType<NormalizedType<ReturnType>>();
        m_target_type_id = TypeID::ForType<NormalizedType<TargetType>>();

        m_params.Reserve(sizeof...(ArgTypes) + 1);
        detail::InitHypMethodParams_Tuple< ReturnType, TargetType, Tuple<ArgTypes...> >{}(m_params);

        if (m_attributes["serialize"] || m_attributes["xmlattribute"]) {
            m_serialize_proc = [mem_fn](Span<HypData> args) -> fbom::FBOMData
            {
                AssertThrow(args.Size() == sizeof...(ArgTypes) + 1);

                const auto fn = HYP_METHOD_MEMBER_FN_WRAPPER(mem_fn);

                HypData **arg_ptrs = (HypData **)StackAlloc(args.Size() * sizeof(HypData *));
                for (SizeType i = 0; i < args.Size(); ++i) {
                    arg_ptrs[i] = &args[i];
                }

                if constexpr (std::is_void_v<ReturnType> || sizeof...(ArgTypes) != 0) {
                    HYP_FAIL("Cannot serialize void return type or non-zero number of arguments");
                } else {
                    fbom::FBOMData out;

                    if (fbom::FBOMResult err = HypDataHelper<NormalizedType<ReturnType>>::Serialize(detail::CallHypMethod<decltype(fn), ReturnType, TargetType *, ArgTypes...>(fn, arg_ptrs), out)) {
                        HYP_FAIL("Failed to serialize data: %s", err.message.Data());
                    }

                    return out;
                }
            };

            m_deserialize_proc = [mem_fn](fbom::FBOMLoadContext &context, Span<HypData> args, const fbom::FBOMData &data) -> void
            {
                AssertThrow(args.Size() == sizeof...(ArgTypes));

                if constexpr (sizeof...(ArgTypes) >= 1) {
                    const auto fn = HYP_METHOD_MEMBER_FN_WRAPPER(mem_fn);

                    HypData value;

                    if (fbom::FBOMResult err = HypDataHelper<NormalizedType<typename TupleElement<sizeof...(ArgTypes) - 1, ArgTypes...>::Type>>::Deserialize(context, data, value)) {
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
    HypMethod(Name name, ReturnType(TargetType::*mem_fn)(ArgTypes...) const, Span<const HypClassAttribute> attributes = {})
        : m_name(name),
          m_flags(HypMethodFlags::MEMBER),
          m_attributes(attributes),
          m_proc([mem_fn](HypData **args, SizeType num_args) -> HypData
          {
              AssertThrow(num_args == sizeof...(ArgTypes) + 1);

              // replace member function with free function using target pointer as first arg
              const auto fn = HYP_METHOD_MEMBER_FN_WRAPPER(mem_fn);

              if constexpr (std::is_void_v<ReturnType>) {
                  detail::CallHypMethod<decltype(fn), ReturnType, TargetType *, ArgTypes...>(fn, args);

                  return HypData();
              } else {
                  return HypData(detail::CallHypMethod<decltype(fn), ReturnType, TargetType *, ArgTypes...>(fn, args));
              }
          })
    {
        m_return_type_id = TypeID::ForType<NormalizedType<ReturnType>>();
        m_target_type_id = TypeID::ForType<NormalizedType<TargetType>>();

        m_params.Reserve(sizeof...(ArgTypes) + 1);
        detail::InitHypMethodParams_Tuple< ReturnType, TargetType, Tuple<ArgTypes...> >{}(m_params);

        if (m_attributes["serialize"] || m_attributes["xmlattribute"]) {
            m_serialize_proc = [mem_fn](Span<HypData> args) -> fbom::FBOMData
            {
                AssertThrow(args.Size() == sizeof...(ArgTypes) + 1);

                const auto fn = HYP_METHOD_MEMBER_FN_WRAPPER(mem_fn);

                HypData **arg_ptrs = (HypData **)StackAlloc(args.Size() * sizeof(HypData *));
                for (SizeType i = 0; i < args.Size(); ++i) {
                    arg_ptrs[i] = &args[i];
                }

                if constexpr (std::is_void_v<ReturnType> || sizeof...(ArgTypes) != 0) {
                    HYP_FAIL("Cannot serialize void return type or non-zero number of arguments");
                } else {
                    fbom::FBOMData out;

                    if (fbom::FBOMResult err = HypDataHelper<NormalizedType<ReturnType>>::Serialize(detail::CallHypMethod<decltype(fn), ReturnType, TargetType *, ArgTypes...>(fn, arg_ptrs), out)) {
                        HYP_FAIL("Failed to serialize data: %s", err.message.Data());
                    }

                    return out;
                }
            };

            m_deserialize_proc = [mem_fn](fbom::FBOMLoadContext &context, Span<HypData> args, const fbom::FBOMData &data) -> void
            {
                AssertThrow(args.Size() == sizeof...(ArgTypes));

                if constexpr (sizeof...(ArgTypes) >= 1) {
                    const auto fn = HYP_METHOD_MEMBER_FN_WRAPPER(mem_fn);

                    HypData value;

                    if (fbom::FBOMResult err = HypDataHelper<NormalizedType<typename TupleElement<sizeof...(ArgTypes) - 1, ArgTypes...>::Type>>::Deserialize(context, data, value)) {
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
    
    // Static method or free function
    template <class ReturnType, class... ArgTypes>
    HypMethod(Name name, ReturnType(*fn)(ArgTypes...), Span<const HypClassAttribute> attributes = {})
        : m_name(name),
          m_flags(HypMethodFlags::STATIC),
          m_attributes(attributes),
          m_proc([fn](HypData **args, SizeType num_args) -> HypData
          {
              AssertThrow(num_args == sizeof...(ArgTypes));

              if constexpr (std::is_void_v<ReturnType>) {
                  detail::CallHypMethod<decltype(fn), ReturnType, ArgTypes...>(fn, args);

                  return HypData();
              } else {
                  return HypData(detail::CallHypMethod<decltype(fn), ReturnType, ArgTypes...>(fn, args));
              }
          })
    {
        m_return_type_id = TypeID::ForType<NormalizedType<ReturnType>>();
        m_target_type_id = TypeID::Void();

        m_params.Reserve(sizeof...(ArgTypes));
        detail::InitHypMethodParams_Tuple< ReturnType, void, Tuple<ArgTypes...> >{}(m_params);
    }

    HypMethod(const HypMethod &other)                   = delete;
    HypMethod &operator=(const HypMethod &other)        = delete;
    HypMethod(HypMethod &&other) noexcept               = default;
    HypMethod &operator=(HypMethod &&other) noexcept    = default;
    virtual ~HypMethod() override                       = default;

    virtual HypMemberType GetMemberType() const override
    {
        return HypMemberType::TYPE_METHOD;
    }

    virtual Name GetName() const override
    {
        return m_name;
    }

    virtual TypeID GetTypeID() const override
    {
        return m_return_type_id;
    }

    virtual TypeID GetTargetTypeID() const override
    {
        return m_target_type_id;
    }

    virtual bool CanSerialize() const override
    {
        return m_serialize_proc.IsValid();
    }

    virtual bool CanDeserialize() const override
    {
        return m_deserialize_proc.IsValid();
    }

    virtual bool Serialize(Span<HypData> args, fbom::FBOMData &out) const override
    {
        if (!CanSerialize()) {
            return false;
        }

        out = m_serialize_proc(args);

        return true;
    }

    virtual bool Deserialize(fbom::FBOMLoadContext &context, HypData &target, const fbom::FBOMData &data) const override
    {
        if (!m_deserialize_proc.IsValid()) {
            return false;
        }

        m_deserialize_proc(context, Span<HypData>(&target, 1), data);

        return true;
    }
    
    virtual const HypClassAttributeSet &GetAttributes() const override
    {
        return m_attributes;
    }

    virtual const HypClassAttributeValue &GetAttribute(ANSIStringView key) const override
    {
        return m_attributes.Get(key);
    }

    virtual const HypClassAttributeValue &GetAttribute(ANSIStringView key, const HypClassAttributeValue &default_value) const override
    {
        return m_attributes.Get(key, default_value);
    }

    HYP_FORCE_INLINE const Array<HypMethodParameter> &GetParameters() const
        { return m_params; }

    HYP_FORCE_INLINE EnumFlags<HypMethodFlags> GetFlags() const
        { return m_flags; }

    HYP_FORCE_INLINE HypData Invoke(Span<HypData> args) const
    {
        HypData **arg_ptrs = (HypData **)StackAlloc(args.Size() * sizeof(HypData *));
        for (SizeType i = 0; i < args.Size(); ++i) {
            arg_ptrs[i] = &args[i];
        }

        return m_proc(arg_ptrs, args.Size());
    }

    HYP_FORCE_INLINE HypData Invoke(const Array<HypData *> &args) const
    {
        return m_proc(const_cast<HypData **>(args.Data()), args.Size());
    }

private:
    Name                                                                        m_name;
    TypeID                                                                      m_return_type_id;
    TypeID                                                                      m_target_type_id;
    Array<HypMethodParameter>                                                   m_params;
    EnumFlags<HypMethodFlags>                                                   m_flags;
    HypClassAttributeSet                                                        m_attributes;

    Proc<HypData(HypData **, SizeType)>                                         m_proc;
    Proc<HypData(const Array<HypData *> &)>                                     m_proc_array;
    Proc<fbom::FBOMData(Span<HypData>)>                                         m_serialize_proc;
    Proc<void(fbom::FBOMLoadContext &, Span<HypData>, const fbom::FBOMData &)>  m_deserialize_proc;
};

#undef HYP_METHOD_MEMBER_FN_WRAPPER

} // namespace hyperion

#endif