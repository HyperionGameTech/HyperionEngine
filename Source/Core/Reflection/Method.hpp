/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Reflection/BoxedValue.hpp>
#include <Core/Reflection/ClassAttribute.hpp>
#include <Core/Reflection/Member.hpp>

#include <Core/Functional/Proc.hpp>

#include <Core/Reflection/TypeId.hpp>
#include <Core/Reflection/TypeInfoFwd.hpp>
#include <Core/Utilities/EnumFlags.hpp>

#include <Core/Defines.hpp>
#include <Core/Name/Name.hpp>
#include <Core/Utilities/FunctionTraits.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

class Class;

CORE_API extern const char* LookupTypeName(const TypeId& typeId);

#ifdef HYP_SCRIPT
enum class BytecodeAddress : uint32;
#ifndef INVALID_FUNCTION_ADDRESS
#define INVALID_FUNCTION_ADDRESS BytecodeAddress(~0u)
#endif

#endif

struct MethodParameter
{
    const TypeInfo* typeInfo = &TypeInfo_Void();
};

#pragma region CallMethod

template <class FunctionType, class ReturnType, class... ArgTypes, size_t... Indices>
static inline decltype(auto) CallMethod_Impl(FunctionType fn, BoxedValue** args, std::index_sequence<Indices...>)
{
    auto assertArgType = [args]<size_t Index>(std::integral_constant<size_t, Index>) -> bool
    {
        BoxedValue& arg = *args[Index];

        const bool condition = arg.Is<NormalizedType<typename TupleElement<Index, ArgTypes...>::Type>>(/* strict */ false);

        if (!condition)
        {
            HYP_FAIL("Unexpected argument of type {} at index {} (expected {})",
                TypeInfo_GetName(*arg.GetTypeInfo()),
                Index,
                TypeNameHelper<NormalizedType<typename TupleElement<Index, ArgTypes...>::Type>>::value.Data());
        }

        return condition;
    };

    (void)(assertArgType(std::integral_constant<size_t, Indices> {}) && ...);

    return fn(args[Indices]->template Get<NormalizedType<ArgTypes>>()...);
}

template <class FunctionType, class ReturnType, class... ArgTypes>
static inline decltype(auto) CallMethod(FunctionType fn, BoxedValue** args)
{
    return CallMethod_Impl<FunctionType, ReturnType, ArgTypes...>(fn, args, std::make_index_sequence<sizeof...(ArgTypes)> {});
}

#pragma endregion CallMethod

#pragma region InitMethodParams

template <class ReturnType, class ThisType, class... ArgTypes, size_t... Indices>
void InitMethodParams_Impl(Array<MethodParameter>& outParams, std::index_sequence<Indices...>)
{
    auto addParameter = [&outParams]<size_t Index>(std::integral_constant<size_t, Index>) -> bool
    {
        outParams.PushBack(MethodParameter { &TypeOf<NormalizedType<typename TupleElement<Index, ArgTypes...>::Type>>() });

        return true;
    };

    (void)(addParameter(std::integral_constant<size_t, Indices> {}) && ...);

    if constexpr (!std::is_void_v<ThisType>)
    {
        outParams.PushBack(MethodParameter { &TypeOf<NormalizedType<ThisType>>() });
    }
}

template <class ReturnType, class ThisType, class... ArgTypes>
void InitMethodParams(Array<MethodParameter>& outParams)
{
    InitMethodParams_Impl<ReturnType, ThisType, ArgTypes...>(outParams, std::make_index_sequence<sizeof...(ArgTypes)> {});
}

template <class ReturnType, class ThisType, class ArgsTupleType>
struct InitMethodParams_Tuple;

template <class ReturnType, class ThisType, class... ArgTypes>
struct InitMethodParams_Tuple<ReturnType, ThisType, Tuple<ArgTypes...>>
{
    void operator()(Array<MethodParameter>& outParams) const
    {
        InitMethodParams<ReturnType, ThisType, ArgTypes...>(outParams);
    }
};

#pragma endregion InitMethodParams

enum class MethodFlags : uint8
{
    NONE = 0x0,
    STATIC = 0x1,
    MEMBER = 0x2,
    VARIADIC = 0x4
};

HYP_MAKE_ENUM_FLAGS(MethodFlags)

template <class FunctionType, class EnableIf = void>
struct MethodHelper;

template <class FunctionType>
struct MethodHelper<FunctionType, std::enable_if_t<FunctionTraits<FunctionType>::isMemberFunction && !FunctionTraits<FunctionType>::isFunctor>>
{
    using ThisType = typename FunctionTraits<FunctionType>::ThisType;
    using ReturnType = typename FunctionTraits<FunctionType>::ReturnType;
    using ArgTypes = typename FunctionTraits<FunctionType>::ArgTypes;

    static constexpr EnumFlags<MethodFlags> flags = MethodFlags::MEMBER;
    static constexpr uint32 numArgs = FunctionTraits<FunctionType>::numArgs + 1;
};

template <class FunctionType>
struct MethodHelper<FunctionType, std::enable_if_t<!FunctionTraits<FunctionType>::isMemberFunction || FunctionTraits<FunctionType>::isFunctor>>
{
    using ThisType = void;
    using ReturnType = typename FunctionTraits<FunctionType>::ReturnType;
    using ArgTypes = typename FunctionTraits<FunctionType>::ArgTypes;

    static constexpr EnumFlags<MethodFlags> flags = MethodFlags::STATIC;
    static constexpr uint32 numArgs = FunctionTraits<FunctionType>::numArgs;
};

#define HYP_METHOD_MEMBER_FN_WRAPPER(_mem_fn)                                                    \
    [_mem_fn]<class... InnerArgTypes>(TargetType& target, InnerArgTypes&&... args) -> ReturnType \
    {                                                                                            \
        return (target.*_mem_fn)(std::forward<InnerArgTypes>(args)...);                          \
    }

class Method final : public IMember
{
public:
    Method(Span<const ClassAttribute> attributes = {})
        : m_name(Name::Invalid()),
          m_returnTypeInfo(&TypeInfo_Void()),
          m_targetTypeInfo(&TypeInfo_Void()),
          m_flags(MethodFlags::NONE),
          m_attributes(attributes)
    {
#ifdef HYP_SCRIPT
        m_scriptAddress = INVALID_FUNCTION_ADDRESS;
#endif
    }

#ifdef HYP_SCRIPT
    Method(Name name, const TypeInfo* returnTypeInfo, const TypeInfo* targetTypeInfo, BytecodeAddress scriptAddress, EnumFlags<MethodFlags> flags, Span<const ClassAttribute> attributes = {})
        : m_name(name),
          m_returnTypeInfo(returnTypeInfo),
          m_targetTypeInfo(targetTypeInfo),
          m_scriptAddress(scriptAddress),
          m_flags(flags),
          m_attributes(attributes)
    {
        HYP_CORE_ASSERT(m_returnTypeInfo != nullptr, "Return TypeInfo cannot be null");
        HYP_CORE_ASSERT(m_targetTypeInfo != nullptr, "Target TypeInfo cannot be null");
    }
#endif

    template <class ReturnType, class TargetType, class... ArgTypes>
    Method(Name name, ReturnType (TargetType::*memFn)(ArgTypes...), Span<const ClassAttribute> attributes = {})
        : m_name(name),
          m_flags(MethodFlags::MEMBER),
          m_attributes(attributes),
          m_proc([memFn](BoxedValue** args, size_t numArgs) -> BoxedValue
              {
                  HYP_CORE_ASSERT(numArgs == sizeof...(ArgTypes) + 1);

                  const auto fn = HYP_METHOD_MEMBER_FN_WRAPPER(memFn);

                  if constexpr (std::is_void_v<ReturnType>)
                  {
                      CallMethod<decltype(fn), ReturnType, TargetType, ArgTypes...>(fn, args);

                      return BoxedValue();
                  }
                  else
                  {
                      return BoxedValue(CallMethod<decltype(fn), ReturnType, TargetType, ArgTypes...>(fn, args));
                  }
              })
    {
#ifdef HYP_SCRIPT
        m_scriptAddress = INVALID_FUNCTION_ADDRESS;
#endif

        m_returnTypeInfo = &TypeOf<NormalizedType<ReturnType>>();
        m_targetTypeInfo = &TypeOf<NormalizedType<TargetType>>();

        m_params.Reserve(sizeof...(ArgTypes) + 1);
        InitMethodParams_Tuple<ReturnType, TargetType, Tuple<ArgTypes...>> {}(m_params);
    }

    template <class ReturnType, class TargetType, class... ArgTypes>
    Method(Name name, ReturnType (TargetType::*memFn)(ArgTypes...) const, Span<const ClassAttribute> attributes = {})
        : m_name(name),
          m_flags(MethodFlags::MEMBER),
          m_attributes(attributes),
          m_proc([memFn](BoxedValue** args, size_t numArgs) -> BoxedValue
              {
                  HYP_CORE_ASSERT(numArgs == sizeof...(ArgTypes) + 1);

                  // replace member function with free function using target pointer as first arg
                  const auto fn = HYP_METHOD_MEMBER_FN_WRAPPER(memFn);

                  if constexpr (std::is_void_v<ReturnType>)
                  {
                      CallMethod<decltype(fn), ReturnType, TargetType, ArgTypes...>(fn, args);

                      return BoxedValue();
                  }
                  else
                  {
                      return BoxedValue(CallMethod<decltype(fn), ReturnType, TargetType, ArgTypes...>(fn, args));
                  }
              })
    {
#ifdef HYP_SCRIPT
        m_scriptAddress = INVALID_FUNCTION_ADDRESS;
#endif

        m_returnTypeInfo = &TypeOf<NormalizedType<ReturnType>>();
        m_targetTypeInfo = &TypeOf<NormalizedType<TargetType>>();

        m_params.Reserve(sizeof...(ArgTypes) + 1);
        InitMethodParams_Tuple<ReturnType, TargetType, Tuple<ArgTypes...>> {}(m_params);
    }

    // Static method or free function
    template <class ReturnType, class... ArgTypes>
    Method(Name name, ReturnType (*fn)(ArgTypes...), Span<const ClassAttribute> attributes = {})
        : m_name(name),
          m_flags(MethodFlags::STATIC),
          m_attributes(attributes),
          m_proc([fn](BoxedValue** args, size_t numArgs) -> BoxedValue
              {
                  HYP_CORE_ASSERT(numArgs == sizeof...(ArgTypes));

                  if constexpr (std::is_void_v<ReturnType>)
                  {
                      CallMethod<decltype(fn), ReturnType, ArgTypes...>(fn, args);

                      return BoxedValue();
                  }
                  else
                  {
                      return BoxedValue(CallMethod<decltype(fn), ReturnType, ArgTypes...>(fn, args));
                  }
              })
    {
#ifdef HYP_SCRIPT
        m_scriptAddress = INVALID_FUNCTION_ADDRESS;
#endif

        m_returnTypeInfo = &TypeOf<NormalizedType<ReturnType>>();
        m_targetTypeInfo = &TypeInfo_Void();

        m_params.Reserve(sizeof...(ArgTypes));
        InitMethodParams_Tuple<ReturnType, void, Tuple<ArgTypes...>> {}(m_params);
    }

    Method(const Method& other) = delete;
    Method& operator=(const Method& other) = delete;

    Method(Method&& other) noexcept = default;
    Method& operator=(Method&& other) noexcept = default;

    virtual ~Method() override = default;

    virtual MemberType GetMemberType() const override
    {
        return MemberType::Method;
    }

    virtual Name GetName() const override
    {
        return m_name;
    }

    virtual const TypeInfo& GetTypeInfo() const override
    {
        return *m_returnTypeInfo;
    }

    virtual const TypeInfo& GetTargetTypeInfo() const override
    {
        return *m_targetTypeInfo;
    }

    virtual const ClassAttributeSet& GetAttributes() const override
    {
        return m_attributes;
    }

    virtual const ClassAttributeValue& GetAttribute(StringHash key) const override
    {
        return m_attributes.Get(key);
    }

    virtual const ClassAttributeValue& GetAttribute(StringHash key, const ClassAttributeValue& defaultValue) const override
    {
        return m_attributes.Get(key, defaultValue);
    }

    HYP_FORCE_INLINE Array<MethodParameter>& GetParameters()
    {
        return m_params;
    }

    HYP_FORCE_INLINE const Array<MethodParameter>& GetParameters() const
    {
        return m_params;
    }

    HYP_FORCE_INLINE EnumFlags<MethodFlags> GetFlags() const
    {
        return m_flags;
    }

    HYP_FORCE_INLINE BoxedValue Invoke(Span<BoxedValue> args) const
    {
        BoxedValue** argPtrs = (BoxedValue**)StackAlloc(args.Size() * sizeof(BoxedValue*));
        for (size_t i = 0; i < args.Size(); ++i)
        {
            argPtrs[i] = &args[i];
        }

        return m_proc(argPtrs, args.Size());
    }

    HYP_FORCE_INLINE BoxedValue Invoke(Span<BoxedValue*> args) const
    {
        return m_proc(args.Data(), args.Size());
    }

    HYP_FORCE_INLINE BoxedValue Invoke(const Array<BoxedValue*>& args) const
    {
        return m_proc(const_cast<BoxedValue**>(args.Data()), args.Size());
    }

#ifdef HYP_SCRIPT
    HYP_FORCE_INLINE BytecodeAddress GetScriptAddress() const
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
    const TypeInfo* m_returnTypeInfo;
    const TypeInfo* m_targetTypeInfo;
    Array<MethodParameter> m_params;
    EnumFlags<MethodFlags> m_flags;
    ClassAttributeSet m_attributes;

    Proc<BoxedValue(BoxedValue**, size_t)> m_proc;

#ifdef HYP_SCRIPT
    BytecodeAddress m_scriptAddress;
#endif
};

#undef HYP_METHOD_MEMBER_FN_WRAPPER

} // namespace Hyperion
