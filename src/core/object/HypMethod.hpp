/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/object/HypData.hpp>
#include <core/object/HypClassAttribute.hpp>
#include <core/object/HypMemberFwd.hpp>

#include <core/functional/Proc.hpp>

#include <core/utilities/TypeId.hpp>
#include <core/utilities/EnumFlags.hpp>

#include <core/Defines.hpp>
#include <core/Name.hpp>
#include <core/Traits.hpp>

#include <core/serialization/Serialization.hpp>

#include <core/Types.hpp>

namespace hyperion {

class HypClass;

extern HYP_API const char* LookupTypeName(TypeId typeId);

#ifdef HYP_SCRIPT
enum class Script_FunctionAddress : uint32;
#ifndef INVALID_FUNCTION_ADDRESS
#define INVALID_FUNCTION_ADDRESS Script_FunctionAddress(~0u)
#endif

#endif

struct HypMethodParameter
{
    TypeId typeId;
};

#pragma region CallHypMethod

template <class FunctionType, class ReturnType, class... ArgTypes, SizeType... Indices>
HYP_FORCE_INLINE decltype(auto) CallHypMethod_Impl(FunctionType fn, HypData** args, std::index_sequence<Indices...>)
{
    auto assertArgType = [args]<SizeType Index>(std::integral_constant<SizeType, Index>) -> bool
    {
        const bool condition = args[Index]->Is<NormalizedType<typename TupleElement<Index, ArgTypes...>::Type>>(/* strict */ false);

        if (!condition)
        {
            HYP_FAIL("Invalid argument at index {}: Expected {}, Got {}",
                Index,
                TypeName<NormalizedType<typename TupleElement<Index, ArgTypes...>::Type>>().Data(),
                LookupTypeName(args[Index]->GetTypeId()));
        }

        return condition;
    };

    (void)(assertArgType(std::integral_constant<SizeType, Indices> {}) && ...);

    return fn(args[Indices]->template Get<NormalizedType<ArgTypes>>()...);
}

template <class FunctionType, class ReturnType, class... ArgTypes>
decltype(auto) CallHypMethod(FunctionType fn, HypData** args)
{
    return CallHypMethod_Impl<FunctionType, ReturnType, ArgTypes...>(fn, args, std::make_index_sequence<sizeof...(ArgTypes)> {});
}

#pragma endregion CallHypMethod

#pragma region InitHypMethodParams

template <class ReturnType, class ThisType, class... ArgTypes, SizeType... Indices>
void InitHypMethodParams_Impl(Array<HypMethodParameter>& outParams, std::index_sequence<Indices...>)
{
    auto addParameter = [&outParams]<SizeType Index>(std::integral_constant<SizeType, Index>) -> bool
    {
        outParams.PushBack(HypMethodParameter {
            TypeId::ForType<NormalizedType<typename TupleElement<Index, ArgTypes...>::Type>>() });

        return true;
    };

    (void)(addParameter(std::integral_constant<SizeType, Indices> {}) && ...);

    if constexpr (!std::is_void_v<ThisType>)
    {
        outParams.PushBack(HypMethodParameter {
            TypeId::ForType<NormalizedType<ThisType>>() });
    }
}

template <class ReturnType, class ThisType, class... ArgTypes>
void InitHypMethodParams(Array<HypMethodParameter>& outParams)
{
    InitHypMethodParams_Impl<ReturnType, ThisType, ArgTypes...>(outParams, std::make_index_sequence<sizeof...(ArgTypes)> {});
}

template <class ReturnType, class ThisType, class ArgsTupleType>
struct InitHypMethodParams_Tuple;

template <class ReturnType, class ThisType, class... ArgTypes>
struct InitHypMethodParams_Tuple<ReturnType, ThisType, Tuple<ArgTypes...>>
{
    void operator()(Array<HypMethodParameter>& outParams) const
    {
        InitHypMethodParams<ReturnType, ThisType, ArgTypes...>(outParams);
    }
};

#pragma endregion InitHypMethodParams

enum class HypMethodFlags : uint8
{
    NONE = 0x0,
    STATIC = 0x1,
    MEMBER = 0x2,
    VARIADIC = 0x4
};

HYP_MAKE_ENUM_FLAGS(HypMethodFlags)

template <class FunctionType, class EnableIf = void>
struct HypMethodHelper;

template <class FunctionType>
struct HypMethodHelper<FunctionType, std::enable_if_t<FunctionTraits<FunctionType>::isMemberFunction && !FunctionTraits<FunctionType>::isFunctor>>
{
    using ThisType = typename FunctionTraits<FunctionType>::ThisType;
    using ReturnType = typename FunctionTraits<FunctionType>::ReturnType;
    using ArgTypes = typename FunctionTraits<FunctionType>::ArgTypes;

    static constexpr EnumFlags<HypMethodFlags> flags = HypMethodFlags::MEMBER;
    static constexpr uint32 numArgs = FunctionTraits<FunctionType>::numArgs + 1;
};

template <class FunctionType>
struct HypMethodHelper<FunctionType, std::enable_if_t<!FunctionTraits<FunctionType>::isMemberFunction || FunctionTraits<FunctionType>::isFunctor>>
{
    using ThisType = void;
    using ReturnType = typename FunctionTraits<FunctionType>::ReturnType;
    using ArgTypes = typename FunctionTraits<FunctionType>::ArgTypes;

    static constexpr EnumFlags<HypMethodFlags> flags = HypMethodFlags::STATIC;
    static constexpr uint32 numArgs = FunctionTraits<FunctionType>::numArgs;
};

#define HYP_METHOD_MEMBER_FN_WRAPPER(_mem_fn)                                                    \
    [_mem_fn]<class... InnerArgTypes>(TargetType& target, InnerArgTypes&&... args) -> ReturnType \
    {                                                                                            \
        return (target.*_mem_fn)(std::forward<InnerArgTypes>(args)...);                          \
    }

class HypMethod : public IHypMember
{
public:
    HypMethod(Span<const HypClassAttribute> attributes = {})
        : m_name(Name::Invalid()),
          m_returnTypeId(TypeId::Void()),
          m_targetTypeId(TypeId::Void()),
          m_flags(HypMethodFlags::NONE),
          m_attributes(attributes)
    {
#ifdef HYP_SCRIPT
        m_scriptAddress = INVALID_FUNCTION_ADDRESS;
#endif
    }

#ifdef HYP_SCRIPT
    HypMethod(Name name, TypeId returnTypeId, TypeId targetTypeId, Script_FunctionAddress scriptAddress, EnumFlags<HypMethodFlags> flags, Span<const HypClassAttribute> attributes = {})
        : m_name(name),
          m_returnTypeId(returnTypeId),
          m_targetTypeId(targetTypeId),
          m_scriptAddress(scriptAddress),
          m_flags(flags),
          m_attributes(attributes)
    {
    }
#endif

    template <class ReturnType, class TargetType, class... ArgTypes>
    HypMethod(Name name, ReturnType (TargetType::*memFn)(ArgTypes...), Span<const HypClassAttribute> attributes = {})
        : m_name(name),
          m_flags(HypMethodFlags::MEMBER),
          m_attributes(attributes),
          m_proc([memFn](HypData** args, SizeType numArgs) -> HypData
              {
                  HYP_CORE_ASSERT(numArgs == sizeof...(ArgTypes) + 1);

                  const auto fn = HYP_METHOD_MEMBER_FN_WRAPPER(memFn);

                  if constexpr (std::is_void_v<ReturnType>)
                  {
                      CallHypMethod<decltype(fn), ReturnType, TargetType, ArgTypes...>(fn, args);

                      return HypData();
                  }
                  else
                  {
                      return HypData(CallHypMethod<decltype(fn), ReturnType, TargetType, ArgTypes...>(fn, args));
                  }
              })
    {
#ifdef HYP_SCRIPT
        m_scriptAddress = INVALID_FUNCTION_ADDRESS;
#endif

        m_returnTypeId = TypeId::ForType<NormalizedType<ReturnType>>();
        m_targetTypeId = TypeId::ForType<NormalizedType<TargetType>>();

        m_params.Reserve(sizeof...(ArgTypes) + 1);
        InitHypMethodParams_Tuple<ReturnType, TargetType, Tuple<ArgTypes...>> {}(m_params);

        if (m_attributes["serialize"] || m_attributes["xmlattribute"])
        {
            m_serializeProc = [memFn](Span<HypData> args, EnumFlags<FBOMDataFlags> flags) -> FBOMData
            {
                HYP_CORE_ASSERT(args.Size() == sizeof...(ArgTypes) + 1);

                const auto fn = HYP_METHOD_MEMBER_FN_WRAPPER(memFn);

                HypData** argPtrs = (HypData**)StackAlloc(args.Size() * sizeof(HypData*));
                for (SizeType i = 0; i < args.Size(); ++i)
                {
                    argPtrs[i] = &args[i];
                }

                if constexpr (std::is_void_v<ReturnType> || sizeof...(ArgTypes) != 0)
                {
                    HYP_FAIL("Cannot serialize void return type or non-zero number of arguments");
                }
                else
                {
                    FBOMData out;

                    if (FBOMResult err = HypDataHelper<NormalizedType<ReturnType>>::Serialize(CallHypMethod<decltype(fn), ReturnType, TargetType, ArgTypes...>(fn, argPtrs), out, flags))
                    {
                        HYP_FAIL("Failed to serialize data: %s", err.message.Data());
                    }

                    return out;
                }
            };

            m_deserializeProc = [memFn](FBOMLoadContext& context, Span<HypData> args, const FBOMData& data) -> void
            {
                HYP_CORE_ASSERT(args.Size() == sizeof...(ArgTypes));

                if constexpr (sizeof...(ArgTypes) >= 1)
                {
                    const auto fn = HYP_METHOD_MEMBER_FN_WRAPPER(memFn);

                    HypData value;

                    if (FBOMResult err = HypDataHelper<NormalizedType<typename TupleElement<sizeof...(ArgTypes) - 1, ArgTypes...>::Type>>::Deserialize(context, data, value))
                    {
                        HYP_FAIL("Failed to deserialize data: %s", err.message.Data());
                    }

                    HypData** argPtrs = (HypData**)StackAlloc((args.Size() + 1) * sizeof(HypData*));
                    for (SizeType i = 0; i < args.Size(); ++i)
                    {
                        argPtrs[i] = &args[i];
                    }

                    argPtrs[args.Size()] = &value;

                    CallHypMethod<decltype(fn), ReturnType, TargetType, ArgTypes...>(fn, argPtrs);
                }
                else
                {
                    HYP_FAIL("Cannot deserialize using non-setter method");
                }
            };
        }
    }

    template <class ReturnType, class TargetType, class... ArgTypes>
    HypMethod(Name name, ReturnType (TargetType::*memFn)(ArgTypes...) const, Span<const HypClassAttribute> attributes = {})
        : m_name(name),
          m_flags(HypMethodFlags::MEMBER),
          m_attributes(attributes),
          m_proc([memFn](HypData** args, SizeType numArgs) -> HypData
              {
                  HYP_CORE_ASSERT(numArgs == sizeof...(ArgTypes) + 1);

                  // replace member function with free function using target pointer as first arg
                  const auto fn = HYP_METHOD_MEMBER_FN_WRAPPER(memFn);

                  if constexpr (std::is_void_v<ReturnType>)
                  {
                      CallHypMethod<decltype(fn), ReturnType, TargetType, ArgTypes...>(fn, args);

                      return HypData();
                  }
                  else
                  {
                      return HypData(CallHypMethod<decltype(fn), ReturnType, TargetType, ArgTypes...>(fn, args));
                  }
              })
    {
#ifdef HYP_SCRIPT
        m_scriptAddress = INVALID_FUNCTION_ADDRESS;
#endif

        m_returnTypeId = TypeId::ForType<NormalizedType<ReturnType>>();
        m_targetTypeId = TypeId::ForType<NormalizedType<TargetType>>();

        m_params.Reserve(sizeof...(ArgTypes) + 1);
        InitHypMethodParams_Tuple<ReturnType, TargetType, Tuple<ArgTypes...>> {}(m_params);

        if (m_attributes["serialize"] || m_attributes["xmlattribute"])
        {
            m_serializeProc = [memFn](Span<HypData> args, EnumFlags<FBOMDataFlags> flags) -> FBOMData
            {
                HYP_CORE_ASSERT(args.Size() == sizeof...(ArgTypes) + 1);

                const auto fn = HYP_METHOD_MEMBER_FN_WRAPPER(memFn);

                HypData** argPtrs = (HypData**)StackAlloc(args.Size() * sizeof(HypData*));
                for (SizeType i = 0; i < args.Size(); ++i)
                {
                    argPtrs[i] = &args[i];
                }

                if constexpr (std::is_void_v<ReturnType> || sizeof...(ArgTypes) != 0)
                {
                    HYP_FAIL("Cannot serialize void return type or non-zero number of arguments");
                }
                else
                {
                    FBOMData out;

                    if (FBOMResult err = HypDataHelper<NormalizedType<ReturnType>>::Serialize(CallHypMethod<decltype(fn), ReturnType, TargetType, ArgTypes...>(fn, argPtrs), out, flags))
                    {
                        HYP_FAIL("Failed to serialize data: %s", err.message.Data());
                    }

                    return out;
                }
            };

            m_deserializeProc = [memFn](FBOMLoadContext& context, Span<HypData> args, const FBOMData& data) -> void
            {
                HYP_CORE_ASSERT(args.Size() == sizeof...(ArgTypes));

                if constexpr (sizeof...(ArgTypes) >= 1)
                {
                    const auto fn = HYP_METHOD_MEMBER_FN_WRAPPER(memFn);

                    HypData value;

                    if (FBOMResult err = HypDataHelper<NormalizedType<typename TupleElement<sizeof...(ArgTypes) - 1, ArgTypes...>::Type>>::Deserialize(context, data, value))
                    {
                        HYP_FAIL("Failed to deserialize data: %s", err.message.Data());
                    }

                    HypData** argPtrs = (HypData**)StackAlloc((args.Size() + 1) * sizeof(HypData*));
                    for (SizeType i = 0; i < args.Size(); ++i)
                    {
                        argPtrs[i] = &args[i];
                    }

                    argPtrs[args.Size()] = &value;

                    CallHypMethod<decltype(fn), ReturnType, TargetType, ArgTypes...>(fn, argPtrs);
                }
                else
                {
                    HYP_FAIL("Cannot deserialize using non-setter method");
                }
            };
        }
    }

    // Static method or free function
    template <class ReturnType, class... ArgTypes>
    HypMethod(Name name, ReturnType (*fn)(ArgTypes...), Span<const HypClassAttribute> attributes = {})
        : m_name(name),
          m_flags(HypMethodFlags::STATIC),
          m_attributes(attributes),
          m_proc([fn](HypData** args, SizeType numArgs) -> HypData
              {
                  HYP_CORE_ASSERT(numArgs == sizeof...(ArgTypes));

                  if constexpr (std::is_void_v<ReturnType>)
                  {
                      CallHypMethod<decltype(fn), ReturnType, ArgTypes...>(fn, args);

                      return HypData();
                  }
                  else
                  {
                      return HypData(CallHypMethod<decltype(fn), ReturnType, ArgTypes...>(fn, args));
                  }
              })
    {
#ifdef HYP_SCRIPT
        m_scriptAddress = INVALID_FUNCTION_ADDRESS;
#endif

        m_returnTypeId = TypeId::ForType<NormalizedType<ReturnType>>();
        m_targetTypeId = TypeId::Void();

        m_params.Reserve(sizeof...(ArgTypes));
        InitHypMethodParams_Tuple<ReturnType, void, Tuple<ArgTypes...>> {}(m_params);
    }

    HypMethod(const HypMethod& other) = delete;
    HypMethod& operator=(const HypMethod& other) = delete;
    HypMethod(HypMethod&& other) noexcept = default;
    HypMethod& operator=(HypMethod&& other) noexcept = default;
    virtual ~HypMethod() override = default;

    virtual HypMemberType GetMemberType() const override
    {
        return HypMemberType::TYPE_METHOD;
    }

    virtual Name GetName() const override
    {
        return m_name;
    }

    virtual TypeId GetTypeId() const override
    {
        return m_returnTypeId;
    }

    virtual TypeId GetTargetTypeId() const override
    {
        return m_targetTypeId;
    }

    virtual bool CanSerialize() const override
    {
        return m_serializeProc.IsValid();
    }

    virtual bool CanDeserialize() const override
    {
        return m_deserializeProc.IsValid();
    }

    virtual bool Serialize(Span<HypData> args, FBOMData& out, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags(0)) const override
    {
        if (!CanSerialize())
        {
            return false;
        }

        out = m_serializeProc(args, flags);

        return true;
    }

    virtual bool Deserialize(FBOMLoadContext& context, HypData& target, const FBOMData& data) const override
    {
        if (!m_deserializeProc.IsValid())
        {
            return false;
        }

        m_deserializeProc(context, Span<HypData>(&target, 1), data);

        return true;
    }

    virtual const HypClassAttributeSet& GetAttributes() const override
    {
        return m_attributes;
    }

    virtual const HypClassAttributeValue& GetAttribute(WeakName key) const override
    {
        return m_attributes.Get(key);
    }

    virtual const HypClassAttributeValue& GetAttribute(WeakName key, const HypClassAttributeValue& defaultValue) const override
    {
        return m_attributes.Get(key, defaultValue);
    }

    HYP_FORCE_INLINE Array<HypMethodParameter>& GetParameters()
    {
        return m_params;
    }

    HYP_FORCE_INLINE const Array<HypMethodParameter>& GetParameters() const
    {
        return m_params;
    }

    HYP_FORCE_INLINE EnumFlags<HypMethodFlags> GetFlags() const
    {
        return m_flags;
    }

    HYP_FORCE_INLINE HypData Invoke(Span<HypData> args) const
    {
        HypData** argPtrs = (HypData**)StackAlloc(args.Size() * sizeof(HypData*));
        for (SizeType i = 0; i < args.Size(); ++i)
        {
            argPtrs[i] = &args[i];
        }

        return m_proc(argPtrs, args.Size());
    }

    HYP_FORCE_INLINE HypData Invoke(Span<HypData*> args) const
    {
        return m_proc(args.Data(), args.Size());
    }

    HYP_FORCE_INLINE HypData Invoke(const Array<HypData*>& args) const
    {
        return m_proc(const_cast<HypData**>(args.Data()), args.Size());
    }

#ifdef HYP_SCRIPT
    HYP_FORCE_INLINE Script_FunctionAddress GetScriptAddress() const
    {
        return m_scriptAddress;
    }

    HYP_FORCE_INLINE bool IsScriptFunction() const
    {
        return m_scriptAddress != INVALID_FUNCTION_ADDRESS;
    }
#endif

private:
    Name m_name;
    TypeId m_returnTypeId;
    TypeId m_targetTypeId;
    Array<HypMethodParameter> m_params;
    EnumFlags<HypMethodFlags> m_flags;
    HypClassAttributeSet m_attributes;

    Proc<HypData(HypData**, SizeType)> m_proc;

    Proc<FBOMData(Span<HypData>, EnumFlags<FBOMDataFlags> flags)> m_serializeProc;
    Proc<void(FBOMLoadContext&, Span<HypData>, const FBOMData&)> m_deserializeProc;

#ifdef HYP_SCRIPT
    Script_FunctionAddress m_scriptAddress;
#endif
};

#undef HYP_METHOD_MEMBER_FN_WRAPPER

} // namespace hyperion
