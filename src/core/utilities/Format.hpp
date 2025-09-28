#pragma once
#include <core/utilities/FormatFwd.hpp>
#include <core/utilities/StringView.hpp>
#include <core/utilities/Tuple.hpp>

#include <core/containers/String.hpp>
#include <core/containers/StaticString.hpp>

#include <core/memory/ByteBuffer.hpp>

namespace hyperion {
namespace utilities {

// int types
template <class StringType, class T>
struct Formatter<StringType, T, std::enable_if_t<std::is_integral_v<T>>>
{
    auto operator()(T value) const
    {
        return StringType::ToString(value);
    }
};

template <class StringType>
struct Formatter<StringType, char>
{
    auto operator()(char value) const
    {
        const char charArray[2] = { value, '\0' };

        return StringType(&charArray[0]);
    }
};

template <class StringType>
struct Formatter<StringType, bool>
{
    static const inline StringType trueString = "true";
    static const inline StringType falseString = "false";
    
    static const inline StringType* const boolStrings[2] = { &falseString, &trueString };
    
    const auto& operator()(bool value) const
    {
        return *boolStrings[value];
    }
};

template <class StringType>
struct Formatter<StringType, float>
{
    auto operator()(float value) const
    {
        Array<ubyte, InlineAllocator<16>> buf;
        buf.Resize(16);

        int resultSize = std::snprintf(reinterpret_cast<char*>(buf.Data()), buf.Size(), "%g", value) + 1;

        if (resultSize > buf.Size())
        {
            buf.Resize(resultSize);

            resultSize = std::snprintf(reinterpret_cast<char*>(buf.Data()), buf.Size(), "%g", value) + 1;
        }

        return StringType(buf.ToByteView());
    }
};

template <class StringType>
struct Formatter<StringType, double>
{
    auto operator()(double value) const
    {
        Array<ubyte, InlineAllocator<16>> buf;
        buf.Resize(16);

        int resultSize = std::snprintf(reinterpret_cast<char*>(buf.Data()), buf.Size(), "%g", value) + 1;

        if (resultSize > buf.Size())
        {
            buf.Resize(resultSize);

            resultSize = std::snprintf(reinterpret_cast<char*>(buf.Data()), buf.Size(), "%g", value) + 1;
        }

        return StringType(buf.ToByteView());
    }
};

// Enum specialization
template <class StringType, class T>
struct Formatter<StringType, T, std::enable_if_t<std::is_enum_v<T>>>
{
    auto operator()(T value) const
    {
        return Formatter<StringType, std::underlying_type_t<T>> {}(static_cast<std::underlying_type_t<T>>(value));
    }
};

// StaticString< Size >
template <class StringType, SizeType Size>
struct Formatter<StringType, StaticString<Size>>
{
    auto operator()(const StaticString<Size>& value) const
    {
        return StringType(value.Data());
    }
};

template <class StringType, int OtherStringType>
struct Formatter<StringType, containers::String<OtherStringType>>
{
    auto operator()(const containers::String<OtherStringType>& value) const
    {
        return value.ToUTF8();
    }
};

template <class StringType, int OtherStringType>
struct Formatter<StringType, utilities::StringView<OtherStringType>>
{
    auto operator()(const utilities::StringView<OtherStringType>& value) const
    {
        return StringType(value);
    }
};

template <class StringType, class T, auto FormatString>
struct PrintfFormatter
{
    auto operator()(T value) const
    {
        Array<ubyte, InlineAllocator<64>> buf;
        buf.Resize(64);

        int resultSize = std::snprintf(reinterpret_cast<char*>(buf.Data()), buf.Size(), FormatString.data, value) + 1;

        if (resultSize > buf.Size())
        {
            buf.Resize(resultSize);

            resultSize = std::snprintf(reinterpret_cast<char*>(buf.Data()), buf.Size(), FormatString.data, value) + 1;
        }

        return StringType(buf.ToByteView());
    }
};

template <class StringType>
struct Formatter<StringType, void*> : PrintfFormatter<StringType, const void*, StaticString("%p")>
{
};

template <class StringType>
struct Formatter<StringType, char*>
{
    auto operator()(const char* value) const
    {
        return StringType(value);
    }
};

template <class StringType, SizeType Size>
struct Formatter<StringType, char[Size]>
{
    auto operator()(const char (&value)[Size]) const
    {
        return StringType(static_cast<const char*>(value));
    }
};

template <class StringType>
struct Formatter<StringType, wchar_t*>
{
    auto operator()(const wchar_t* value) const
    {
        return StringType(WideStringView(value));
    }
};

#pragma region ConcatRuntimeStrings

template <int StringType>
struct ConcatRuntimeStrings_Impl;

template <int StringType>
struct ConcatRuntimeStrings_Impl
{
    template <class SecondStringType>
    constexpr auto Impl(const containers::String<StringType>& str0, SecondStringType&& str1) const
    {
        return str0 + str1;
    }

    constexpr auto operator()(const containers::String<StringType>& str0) const
    {
        return str0;
    }

    template <class SecondStringType>
    constexpr auto operator()(const containers::String<StringType>& str0, SecondStringType&& str1) const
    {
        if (str0.Empty() && str1.Empty())
        {
            return containers::String<StringType>::empty;
        }
        else if (str0.Empty())
        {
            return str1;
        }
        else if (str1.Empty())
        {
            return str0;
        }
        else
        {
            return Impl(str0, str1);
        }
    }

    template <class SecondStringType, class... OtherStringType>
    constexpr auto operator()(const containers::String<StringType>& str0, SecondStringType&& str1, OtherStringType&&... other) const
    {
        if (str0.Empty() && str1.Empty())
        {
            return ConcatRuntimeStrings_Impl()(containers::String<StringType>::empty, other...);
        }
        else if (str0.Empty())
        {
            return ConcatRuntimeStrings_Impl()(str1, other...);
        }
        else if (str1.Empty())
        {
            return ConcatRuntimeStrings_Impl()(str0, other...);
        }
        else
        {
            return ConcatRuntimeStrings_Impl()(Impl(str0, str1), std::forward<OtherStringType>(other)...);
        }
    }
};

template <int StringType, class... Args>
constexpr auto ConcatRuntimeStrings(Args&&... strings)
{
    return ConcatRuntimeStrings_Impl<StringType>()(std::forward<Args>(strings)...);
}

#pragma endregion ConcatRuntimeStrings

#pragma region FormatString_BadFormat

template <auto Str>
struct FormatString_BadFormat : std::false_type
{
};

template <auto Str, auto Index>
struct FormatString_BadFormat_IndexOutOfBounds : std::false_type
{
};

#pragma endregion FormatString_BadFormat

#pragma region FormatString_FormatElement

template <int StringType, class T>
containers::String<StringType> FormatString_FormatElement_Runtime(const T& element)
{
    using FormatterSpecializationType = std::conditional_t<
        std::is_pointer_v<std::remove_cvref_t<T>>,
        std::add_pointer_t<std::remove_cvref_t<std::remove_pointer_t<T>>>,
        std::remove_cvref_t<T>>;

    static_assert(ImplementationExistsV<Formatter<containers::String<StringType>, FormatterSpecializationType>>, "No Formatter specialization exists for type");

    // if-constexpr is to prevent a huge swath of errors preventing the user from seeing the assertion failure.
    if constexpr (ImplementationExistsV<Formatter<containers::String<StringType>, FormatterSpecializationType>>)
    {
        return Formatter<containers::String<StringType>, FormatterSpecializationType> {}(element);
    }
    else
    {
        return containers::String<StringType>::empty;
    }
}

#pragma endregion FormatString_FormatElement

#pragma region FormatString_BuildTuple

template <auto Str, class Transformer, SizeType SubIndex = 0>
struct FormatString_BuildTuple;

template <auto Str, class Transformer, SizeType StringIndexStart = 0, SizeType StringIndexEnd = 0, SizeType SubIndex = 0>
struct FormatString_BuildTuple_Impl;

template <auto Str, class Transformer, SizeType SubIndex>
struct FormatString_BuildTuple_Impl<Str, Transformer, SizeType(-1), SizeType(-1), SubIndex>
{
    template <class... Args>
    constexpr auto operator()(Args&&... args) const
    {
        return MakeTuple(Str);
    }
};

template <auto Str, class Transformer, SizeType StringIndexStart, SizeType StringIndexEnd, SizeType SubIndex>
struct FormatString_BuildTuple_Impl
{
    template <class... Args>
    constexpr auto operator()(Args&&... args) const
    {
        constexpr auto innerValue = containers::helpers::Substr<Str, StringIndexStart + 1, StringIndexEnd>::value;

        if constexpr (innerValue.Size() > 1 /* NUL */)
        {
            // Parse string to integer, use it as SubIndex.
            constexpr SizeType parsedInteger = SizeType(containers::helpers::ParseInteger<innerValue>::value);

            if constexpr (parsedInteger < sizeof...(Args))
            {
                return ConcatTuples(
                    MakeTuple(containers::helpers::Substr<Str, 0, StringIndexStart>::value),
                    ConcatTuples(
                        ForwardAsTuple(ForwardAsTuple(std::forward<Args>(args)...).template GetElement<parsedInteger>()),
                        FormatString_BuildTuple<containers::helpers::Substr<Str, StringIndexEnd + 1, Str.Size() - 1>::value, Transformer, parsedInteger> {}(std::forward<Args>(args)...)));
            }
            else
            {
                static_assert(FormatString_BadFormat_IndexOutOfBounds<Str, innerValue> {}, "String interpolation attempted to access out of range element. Explicitly provided index value was outside of the number of arguments.");
            }
        }
        else if constexpr (SubIndex < sizeof...(Args))
        {
            return ConcatTuples(
                MakeTuple(containers::helpers::Substr<Str, 0, StringIndexStart>::value),
                ConcatTuples(
                    ForwardAsTuple(ForwardAsTuple(std::forward<Args>(args)...).template GetElement<SubIndex>()),
                    FormatString_BuildTuple<containers::helpers::Substr<Str, StringIndexEnd + 1, Str.Size() - 1>::value, Transformer, SubIndex + 1> {}(std::forward<Args>(args)...)));
        }
        else
        {
            static_assert(FormatString_BadFormat_IndexOutOfBounds<Str, SubIndex> {}, "String interpolation attempted to access out of range element. Does the number of arguments match the number of replacement tokens?");

            return MakeTuple(Str);
        }
    }
};

template <auto Str, class Transformer, SizeType SubIndex>
struct FormatString_BuildTuple
{
    template <class... Args>
    constexpr auto operator()(Args&&... args) const
    {
        return FormatString_BuildTuple_Impl<
            Str,
            Transformer,
            Str.template FindFirst<containers::IntegerSequenceFromString<StaticString { { '{', '\0' } }>>(),
            Str.template FindFirst<containers::IntegerSequenceFromString<StaticString { { '}', '\0' } }>>(),
            SubIndex> {}(std::forward<Args>(args)...);
    }
};

#pragma endregion FormatString_BuildTuple

#pragma region FormatString_ProcessTuple

#if 0
template <class... Ts, SizeType... Indices>
constexpr auto FormatString_ProcessTuple_ProcessElements_CompileTime(const Tuple< Ts... > &args, std::index_sequence<Indices...>)
{
    return containers::helpers::ConcatStrings(FormatString_FormatElement_CompileTime(args.template GetElement< Indices >())...);
}
#endif

template <int StringType, class... Ts, SizeType... Indices>
containers::String<StringType> FormatString_ProcessTuple_ProcessElements_Runtime(const Tuple<Ts...>& args, std::index_sequence<Indices...>)
{
    return ConcatRuntimeStrings<StringType>(FormatString_FormatElement_Runtime<StringType>(args.template GetElement<Indices>())...);
}

template <int StringType, class... Ts>
struct FormatString_ProcessTuple_Impl
{
    using Types = Tuple<Ts...>;

    Tuple<Ts...> args;

    constexpr auto operator()() const
    {
#if 0
        if constexpr (std::is_constant_evaluated())
            return FormatString_ProcessTuple_ProcessElements_CompileTime(args, std::make_index_sequence<sizeof...(Ts)>());
        else
#endif
        return FormatString_ProcessTuple_ProcessElements_Runtime<StringType>(args, std::make_index_sequence<sizeof...(Ts)>());
    }
};

template <int StringType, class... Ts>
constexpr auto FormatString_ProcessTuple(Tuple<Ts...>&& tup)
{
    return FormatString_ProcessTuple_Impl<StringType, Ts...> { std::move(tup) };
}

#pragma endregion FormatString_ProcessTuple

struct FormatTransformer
{
    template <auto Str>
    static constexpr auto Transform()
    {
        return Str;
    }
};

template <auto Str, class... Args>
constexpr auto Format_Impl(Args&&... args)
{
    return (FormatString_ProcessTuple<containers::StringType::UTF8>(
        FormatString_BuildTuple<Str, FormatTransformer> {}(std::forward<Args>(args)...)))();
}

template <auto Str, class... Args>
constexpr auto Format(Args&&... args)
{
    return Format_Impl<Str>(std::forward<Args>(args)...);
}

} // namespace utilities

using utilities::Format;

} // namespace hyperion

// Helper macro for utilities::Format< FormatString >(...)

// To override FormatFwd.hpp
#ifdef HYP_FORMAT
#undef HYP_FORMAT
#endif

#define HYP_FORMAT(fmt, ...) Format<HYP_STATIC_STRING(fmt)>(__VA_ARGS__)
