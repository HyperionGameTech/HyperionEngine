/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/containers/StaticString.hpp>

#include <Core/Defines.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

// tuple forward declaration
namespace utilities {

template <class... Types>
class Tuple;

} // namespace utilities

using utilities::Tuple;

template <auto Str, bool ShouldStripNamespace>
constexpr auto ParseTypeName();

// strip "class " or "struct " from beginning StaticString
template <auto Str>
constexpr auto StripClassOrStruct()
{
    constexpr auto classIndex = Str.template FindFirst<containers::IntegerSequenceFromString<StaticString("class ")>>();
    constexpr auto structIndex = Str.template FindFirst<containers::IntegerSequenceFromString<StaticString("struct ")>>();

    if constexpr (classIndex != -1 && (structIndex == -1 || classIndex <= structIndex))
    {
        return containers::helpers::Substr<Str, classIndex + 6, Str.Size()>::value; // 6 = length of "class "
    }
    else if constexpr (structIndex != -1 && (classIndex == -1 || structIndex <= classIndex))
    {
        return containers::helpers::Substr<Str, structIndex + 7, Str.Size()>::value; // 7 = length of "struct "
    }
    else
    {
        return Str;
    }
}

#pragma region TypeNameStringTransformer

// constexpr functions to strip namespaces from StaticString

template <bool ShouldStripNamespace>
struct TypeNameStringTransformer
{
    static constexpr char delimiter = ',';

    static constexpr uint32 balanceBracketOptions = containers::helpers::BALANCE_BRACKETS_ANGLE;

    template <auto String>
    static constexpr auto Transform()
    {
        constexpr size_t lastIndex = ShouldStripNamespace
            ? containers::helpers::Trim<String>::value.template FindLast<containers::IntegerSequenceFromString<StaticString("::")>>()
            : size_t(-1);

        if constexpr (lastIndex == -1)
        {
            return StripClassOrStruct<containers::helpers::Trim<String>::value>();
        }
        else
        {
            return StripClassOrStruct<containers::helpers::Substr<containers::helpers::Trim<String>::value, lastIndex + 2, size_t(-1)>::value>();
        }
    }
};

#pragma endregion TypeNameStringTransformer

#pragma region TypeNameStringTransformer2

template <bool ShouldStripNamespace>
struct TypeNameStringTransformer2
{
    static constexpr char delimiter = ',';

    static constexpr uint32 balanceBracketOptions = containers::helpers::BALANCE_BRACKETS_ANGLE;

    template <auto String>
    static constexpr auto Transform()
    {
        return ParseTypeName<String, ShouldStripNamespace>();
    }
};

#pragma endregion TypeNameStringTransformer2

#pragma region ParseTypeName

template <auto Str, bool ShouldStripNamespace>
constexpr auto ParseTypeName()
{
    constexpr auto leftArrowIndex = Str.template FindFirst<containers::IntegerSequenceFromString<StaticString("<")>>();
    constexpr auto rightArrowIndex = Str.template FindLast<containers::IntegerSequenceFromString<StaticString(">")>>();

    if constexpr (leftArrowIndex != size_t(-1) && rightArrowIndex != size_t(-1))
    {
        static_assert(leftArrowIndex < rightArrowIndex, "Left arrow index must be less than right arrow index or parsing will fail!");

        return containers::helpers::Concat<
            containers::helpers::TransformSplit<TypeNameStringTransformer2<ShouldStripNamespace>, containers::helpers::Substr<Str, 0, leftArrowIndex>::value>::value,
            StaticString("<"),
            containers::helpers::TransformSplit<TypeNameStringTransformer2<ShouldStripNamespace>, containers::helpers::Substr<Str, leftArrowIndex + 1, rightArrowIndex>::value>::value,
            StaticString(">")>::value;
    }
    else
    {
        return containers::helpers::TransformSplit<TypeNameStringTransformer<ShouldStripNamespace>, Str>::value;
    }
}

#pragma endregion ParseTypeName

#pragma region TypeName

/*! \brief Returns the name of the type T as a StaticString.
 *
 *  \tparam T The type to get the name of.
 *
 *  \return The name of the type T as a StaticString.
 */
template <class T>
constexpr auto TypeName()
{
    constexpr StaticString<sizeof(HYP_FUNCTION_NAME_LIT)> name(HYP_FUNCTION_NAME_LIT);

#ifdef HYP_CLANG_OR_GCC
#ifdef HYP_CLANG
    // auto Hyperion::TypeName() [T = Hyperion::Task<int, int>]
    constexpr auto substr = containers::helpers::Substr<name, 31, sizeof(HYP_FUNCTION_NAME_LIT) - 2>::value;
#elif defined(HYP_GCC)
    // constexpr auto Hyperion::TypeName() [with T = Hyperion::Task<int, int>]
    constexpr auto substr = containers::helpers::Substr<name, 47, sizeof(HYP_FUNCTION_NAME_LIT) - 2>::value;
#endif
#elif defined(HYP_MSVC)
    //  auto __cdecl Hyperion::TypeName<class Hyperion::Task<int,int>>(void)
    constexpr auto substr = containers::helpers::Substr<name, 32, sizeof(HYP_FUNCTION_NAME_LIT) - 8>::value;

#else
    static_assert(false, "Unsupported compiler for TypeName()");
#endif

    return ParseTypeName<substr, false>();
}

/*! \brief Returns the name of the type T as a StaticString. Removes the namespace from the name (e.g. Hyperion::Task<int, int> -> Task<int, int>).
 *
 *  \tparam T The type to get the name of.
 *
 *  \return The name of the type T as a StaticString.
 */
template <class T>
constexpr auto TypeNameWithoutNamespace()
{
    constexpr StaticString<sizeof(HYP_FUNCTION_NAME_LIT)> name(HYP_FUNCTION_NAME_LIT);
#ifdef HYP_CLANG_OR_GCC
#ifdef HYP_CLANG

    // auto Hyperion::TypeNameWithoutNamespace() [T = Hyperion::Task<int, int>]
    constexpr auto substr = containers::helpers::Substr<name, 47, sizeof(HYP_FUNCTION_NAME_LIT) - 2>::value;
#elif defined(HYP_GCC)
    // constexpr auto Hyperion::TypeNameWithoutNamespace() [with T = Hyperion::Task<int, int>]
    constexpr auto substr = containers::helpers::Substr<name, 63, sizeof(HYP_FUNCTION_NAME_LIT) - 2>::value;
#endif
#elif defined(HYP_MSVC)
    //  auto __cdecl Hyperion::TypeNameWithoutNamespace<class Hyperion::Task<int,int>>(void)
    constexpr auto substr = containers::helpers::Substr<name, 48, sizeof(HYP_FUNCTION_NAME_LIT) - 8>::value;
#else
    static_assert(false, "Unsupported compiler");
#endif

    return ParseTypeName<substr, true>();
}

#pragma endregion TypeName

#pragma region TypeNameHelper

template <class T, bool ShouldStripNamespace = false>
struct TypeNameHelper;

template <class T>
struct TypeNameHelper<T, false>
{
    static constexpr decltype(TypeName<T>()) value = TypeName<T>();
};

template <class T>
struct TypeNameHelper<T, true>
{
    static constexpr decltype(TypeNameWithoutNamespace<T>()) value = TypeNameWithoutNamespace<T>();
};

#pragma endregion TypeNameHelper

#pragma region StripReturnType

template <auto Str>
constexpr auto StripReturnType()
{
    constexpr auto firstSpaceIndex = Str.template FindFirst<containers::IntegerSequenceFromString<StaticString(" ")>>();

    if constexpr (firstSpaceIndex == size_t(-1))
    {
        return Str;
    }
    else
    {
        constexpr auto withoutReturnType = containers::helpers::Substr<Str, firstSpaceIndex + 1, Str.Size()>::value;

        constexpr auto leftAngleBracketIndex = withoutReturnType.template FindFirst<containers::IntegerSequenceFromString<StaticString("<")>>();
        constexpr auto leftParenthesisIndex = withoutReturnType.template FindFirst<containers::IntegerSequenceFromString<StaticString("(")>>();

        constexpr auto firstTokenIndex = leftAngleBracketIndex != size_t(-1) && (leftParenthesisIndex == size_t(-1) || leftAngleBracketIndex < leftParenthesisIndex)
            ? leftAngleBracketIndex
            : leftParenthesisIndex;

        constexpr auto secondSpaceIndex = withoutReturnType.template FindFirst<containers::IntegerSequenceFromString<StaticString(" ")>>();

        if constexpr (secondSpaceIndex != size_t(-1) && firstTokenIndex != size_t(-1) && secondSpaceIndex < firstTokenIndex)
        {
            return containers::helpers::Substr<withoutReturnType, secondSpaceIndex + 1, withoutReturnType.Size()>::value;
        }
        else
        {
            return withoutReturnType;
        }
    }
}

#pragma endregion StripReturnType

#pragma region StripNamespaceFromFunctionName

template <auto Str>
constexpr auto StripNamespaceFromFunctionName()
{
    if constexpr (Str.Size() == 0)
    {
        return Str;
    }
    else if constexpr (Str.data[0] >= 'A' && Str.data[0] <= 'Z')
    {
        // function and class names start with uppercase letters in hyperion
        return Str;
    }
    else
    {
        constexpr auto firstColonIndex = Str.template FindFirst<containers::IntegerSequenceFromString<StaticString("::")>>();

        if constexpr (firstColonIndex != size_t(-1))
        {
            constexpr auto substr = containers::helpers::Substr<Str, firstColonIndex + 2, Str.Size()>::value;

            return StripNamespaceFromFunctionName<substr>();
        }
        else
        {
            return Str;
        }
    }
}

#pragma endregion StripNamespaceFromFunctionName

#pragma region PrettyFunctionName

/*! \brief Normalizes the input string, removing the return type and parameters from the function signature. */
template <auto Str>
constexpr auto PrettyFunctionName()
{
    constexpr auto withoutReturnType = StripReturnType<Str>();

    constexpr auto leftAngleBracketIndex = withoutReturnType.template FindFirst<containers::IntegerSequenceFromString<StaticString("<")>>();
    constexpr auto leftParenthesisIndex = withoutReturnType.template FindFirst<containers::IntegerSequenceFromString<StaticString("(")>>();

    if constexpr (leftParenthesisIndex != size_t(-1))
    {
        if constexpr (leftAngleBracketIndex != size_t(-1) && leftAngleBracketIndex < leftParenthesisIndex)
        {
            return StripNamespaceFromFunctionName<containers::helpers::Substr<withoutReturnType, 0, leftAngleBracketIndex>::value>();
        }
        else
        {
            return StripNamespaceFromFunctionName<containers::helpers::Substr<withoutReturnType, 0, leftParenthesisIndex>::value>();
        }
    }
    else
    {
        return withoutReturnType;
    }
}

//#define HYP_PRETTY_FUNCTION_NAME Hyperion::PrettyFunctionName<HYP_STATIC_STRING(HYP_FUNCTION_NAME_LIT)>()
#define HYP_PRETTY_FUNCTION_NAME HYP_STATIC_STRING(HYP_FUNCTION_NAME_LIT)

#pragma endregion PrettyFunctionName

#pragma region Template helpers

template <class T>
struct TypeWrapper
{
    using Type = T;
};

template <auto Value>
struct ValueWrapper
{
    static constexpr auto value = Value;
};

#pragma endregion Template helpers

#pragma region Misc utilities

template <class T>
struct NoOpFunction
{
    template <class... Args>
    HYP_FORCE_INLINE constexpr T operator()(Args&&...) const
    {
        return {};
    }
};

#pragma endregion Misc utilities

/*! \brief Size of an array literal (Hyperion equivalent of std::size) */
template <class T, uint32 N>
constexpr uint32 ArraySize(const T (&)[N])
{
    return N;
}

/*! \brief Convert the value to an rvalue reference. If it cannot be converted, a compile time error will be generated.
 *  \tparam T The type of the value being passed
 *  \param value The value to convert to an rvalue reference.
 *  \returns The value as an rvalue reference. */
template <class T>
HYP_FORCE_INLINE constexpr std::remove_reference_t<T>&& Move(T&& value) noexcept
{
    static_assert(std::is_lvalue_reference_v<T>, "T must be an lvalue reference to use Move()");
    static_assert(!std::is_same_v<const typename std::remove_reference_t<T>&, typename std::remove_reference_t<T>&>, "T must not be const to use Move()");

    return static_cast<std::remove_reference_t<T>&&>(value);
}

/*! \brief Attempts to move the object if possible, will use copy otherwise.
 *  \tparam T The type of the value being passed
 *  \param value The value to convert to an rvalue reference.
 *  \returns The value as an rvalue reference. */
template <class T>
HYP_FORCE_INLINE constexpr std::remove_reference_t<T>&& TryMove(T&& value) noexcept
{
    return static_cast<std::remove_reference_t<T>&&>(value);
}

template <class T>
HYP_FORCE_INLINE void Swap(T& a, T& b)
{
    static_assert(std::is_move_constructible_v<T> && std::is_move_assignable_v<T>, "Swap requires T to be move constructible and move assignable");

    T temp = TryMove(a);
    a = TryMove(b);
    b = TryMove(temp);
}

template <class T>
struct CheckedPointer
{
    T* ptr;

    CheckedPointer(T* ptr = nullptr)
        : ptr(ptr)
    {
    }

    HYP_FORCE_INLINE T& operator*() const
    {
        HYP_CORE_ASSERT(ptr != nullptr, "Dereferencing a null pointer");
        return *ptr;
    }

    HYP_FORCE_INLINE T* operator->() const
    {
        HYP_CORE_ASSERT(ptr != nullptr, "Dereferencing a null pointer");
        return ptr;
    }

    HYP_FORCE_INLINE bool IsValid() const
    {
        return ptr != nullptr;
    }

    HYP_FORCE_INLINE void Reset()
    {
        ptr = nullptr;
    }

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return IsValid();
    }

    HYP_FORCE_INLINE explicit operator T*() const
    {
        return ptr;
    }

    HYP_FORCE_INLINE bool operator==(std::nullptr_t) const
    {
        return ptr == nullptr;
    }

    HYP_FORCE_INLINE bool operator!=(std::nullptr_t) const
    {
        return ptr != nullptr;
    }

    HYP_FORCE_INLINE bool operator==(const CheckedPointer& other) const
    {
        return ptr == other.ptr;
    }

    HYP_FORCE_INLINE bool operator!=(const CheckedPointer& other) const
    {
        return ptr != other.ptr;
    }

    HYP_FORCE_INLINE bool operator<(const CheckedPointer& other) const
    {
        return ptr < other.ptr;
    }

    HYP_FORCE_INLINE bool operator>(const CheckedPointer& other) const
    {
        return ptr > other.ptr;
    }

    HYP_FORCE_INLINE bool operator<=(const CheckedPointer& other) const
    {
        return ptr <= other.ptr;
    }

    HYP_FORCE_INLINE bool operator>=(const CheckedPointer& other) const
    {
        return ptr >= other.ptr;
    }

    HYP_FORCE_INLINE bool operator==(const T* other) const
    {
        return ptr == other;
    }

    HYP_FORCE_INLINE bool operator!=(const T* other) const
    {
        return ptr != other;
    }

    HYP_FORCE_INLINE bool operator<(const T* other) const
    {
        return ptr < other;
    }

    HYP_FORCE_INLINE bool operator>(const T* other) const
    {
        return ptr > other;
    }

    HYP_FORCE_INLINE bool operator<=(const T* other) const
    {
        return ptr <= other;
    }

    HYP_FORCE_INLINE bool operator>=(const T* other) const
    {
        return ptr >= other;
    }
};

template <class T, class... Ts>
struct FirstOf
{
    using Type = T;
};

#pragma region StaticForEach

// Helper for static foreach over tuple types - no instance version
template <class FunctionType, class... Types, size_t... Indices>
constexpr void StaticForEach_TypesOnly_Impl(FunctionType&& function, Hyperion::utilities::TupleIndices<Indices...>)
{
    (function(TypeWrapper<Types> {}), ...);
}

// Helper for static foreach over tuple types - with instance version
template <class FunctionType, class... Types, size_t... Indices>
constexpr void StaticForEach_WithInstance_Impl(FunctionType&& function, Tuple<Types...>& tuple, Hyperion::utilities::TupleIndices<Indices...>)
{
    (function(TypeWrapper<Types> {}, tuple.template GetElement<Indices>()), ...);
}

template <class FunctionType, class... Types, size_t... Indices>
constexpr void StaticForEach_WithInstance_Impl(FunctionType&& function, const Tuple<Types...>& tuple, Hyperion::utilities::TupleIndices<Indices...>)
{
    (function(TypeWrapper<Types> {}, tuple.template GetElement<Indices>()), ...);
}

// Helper struct to enable specialization for tuple types
template <class TupleType>
struct StaticForEach_Helper;

// Empty tuple specialization - does nothing (prevents empty fold errors)
template <>
struct StaticForEach_Helper<Tuple<>>
{
    template <class FunctionType>
    static constexpr void Call(FunctionType&&)
    { /* no-op */
    }
};

template <class... Types>
struct StaticForEach_Helper<Tuple<Types...>>
{
    template <class FunctionType>
    static constexpr void Call(FunctionType&& function)
    {
        StaticForEach_TypesOnly_Impl<FunctionType, Types...>(
            std::forward<FunctionType>(function),
            typename Hyperion::utilities::MakeTupleIndices<sizeof...(Types)>::Type {});
    }
};

// Static foreach over tuple types without an instance
// Calls function with TypeWrapper<T> for each type in the tuple
// Usage: StaticForEach<Tuple<int, float, double>>(function)
template <class TupleType, class FunctionType>
constexpr void StaticForEach(FunctionType&& function)
{
    StaticForEach_Helper<TupleType>::Call(std::forward<FunctionType>(function));
}

// Static foreach over tuple types with an instance
// Calls function with TypeWrapper<T> and the element at index N for each type in the tuple
template <class FunctionType, class... Types>
constexpr void StaticForEach(FunctionType&& function, Tuple<Types...>& tuple)
{
    return StaticForEach_WithInstance_Impl(
        std::forward<FunctionType>(function),
        tuple,
        typename Hyperion::utilities::MakeTupleIndices<sizeof...(Types)>::Type {});
}

template <class FunctionType, class... Types>
constexpr void StaticForEach(FunctionType&& function, const Tuple<Types...>& tuple)
{
    return StaticForEach_WithInstance_Impl(
        std::forward<FunctionType>(function),
        tuple,
        typename Hyperion::utilities::MakeTupleIndices<sizeof...(Types)>::Type {});
}

#pragma endregion StaticForEach

#pragma region WithTupleElementAt

/*! \brief Invoke the given functor object for the element at the provided \p index */
template <class... Types, class Functor>
constexpr void WithTupleElement(Tuple<Types...>& tuple, size_t index, Functor&& fn)
{
    size_t currIndex = 0;

    auto Doer = [&]<size_t... Indices>(std::index_sequence<Indices...>)
    {
        ((currIndex++ == index && (fn(tuple.template GetElement<Indices>()), true)) || ...);
    };

    Doer(std::make_index_sequence<sizeof...(Types)> {});
}

/*! \brief Invoke the given functor object for the element at the provided \p index */
template <class... Types, class Functor>
constexpr void WithTupleElement(const Tuple<Types...>& tuple, size_t index, Functor&& fn)
{
    size_t currIndex = 0;

    auto Doer = [&]<size_t... Indices>(std::index_sequence<Indices...>)
    {
        ((currIndex++ == index && (fn(tuple.template GetElement<Indices>()), true)) || ...);
    };

    Doer(std::make_index_sequence<sizeof...(Types)> {});
}

#pragma endregion WithTupleElementAt

#pragma region OffsetOf

template <typename T>
struct MemberClass;

template <typename M, typename C>
struct MemberClass<M C::*>
{
    using Type = C;
};

template <typename Base, typename Derived, typename = void>
struct IsVirtualBaseOf : std::true_type
{
};

template <typename Base, typename Derived>
struct IsVirtualBaseOf<
    Base, Derived,
    std::void_t<decltype(static_cast<Derived*>((Base*)nullptr))>> : std::false_type
{
};

template <typename T1, typename T2>
HYP_FORCE_INLINE uint32 OffsetOf(T1 T2::* member)
{
    using MemberDeclaringClass = typename MemberClass<decltype(member)>::Type;

    static_assert(
        !IsVirtualBaseOf<MemberDeclaringClass, T2>::value,
        "Cannot calculate offset of a member in a Virtual Base.");

    const T2* obj = nullptr;
    return uint32(IntPtr(&(obj->*member)));
}

// helper macro for same usage as offsetof
#define HYP_OFFSET_OF(type, memberName) ::Hyperion::OffsetOf(&type::memberName)

#pragma endregion OffsetOf

} // namespace Hyperion
