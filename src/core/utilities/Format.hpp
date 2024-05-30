#ifndef HYPERION_FORMAT_HPP
#define HYPERION_FORMAT_HPP

#include <core/utilities/FormatFwd.hpp>
#include <core/utilities/StringView.hpp>
#include <core/utilities/Tuple.hpp>
#include <core/containers/String.hpp>
#include <core/containers/StaticString.hpp>
#include <core/memory/ByteBuffer.hpp>

namespace hyperion {

// fwd decl for math types
namespace math {
namespace detail {

template <class T>
struct Vec2;

template <class T>
struct Vec3;

template <class T>
struct Vec4;

} // namespace detail
} // namespace math

namespace utilities {

// Int types
template <class StringType, class T>
struct Formatter< StringType, T, std::enable_if_t< std::is_integral_v< T > > >
{
    auto operator()(T value) const
    {
        return StringType::ToString(value);
    }
};

template <class StringType>
struct Formatter< StringType, float >
{
    auto operator()(float value) const
    {
        memory::ByteBuffer byte_buffer;
        byte_buffer.SetSize(10);

        int result_size = std::snprintf(reinterpret_cast<char *>(byte_buffer.Data()), byte_buffer.Size(), "%f", value);

        if (result_size > byte_buffer.Size()) {
            byte_buffer.SetSize(result_size);

            result_size = std::snprintf(reinterpret_cast<char *>(byte_buffer.Data()), byte_buffer.Size(), "%f", value);
        }

        return StringType(byte_buffer);
    }
};

// Enum specialization
template <class StringType, class T>
struct Formatter< StringType, T, std::enable_if_t< std::is_enum_v< T > > >
{
    auto operator()(T value) const
    {
        return Formatter< StringType, std::underlying_type_t< T > >{}(static_cast<std::underlying_type_t< T > >(value));
    }
};

// StaticString< Size >
template <class StringType, SizeType Size>
struct Formatter< StringType, StaticString< Size > >
{
    auto operator()(const StaticString< Size > &value) const
    {
        return StringType(value.Data());
    }
};
template <class StringType, int OtherStringType>
struct Formatter< StringType, containers::detail::String < OtherStringType > >
{
    auto operator()(const containers::detail::String< OtherStringType > &value) const
    {
        return value.ToUTF8();
    }
};

template <class StringType>
struct Formatter<StringType, char *>
{
    auto operator()(const char *value) const
    {
        return StringType(value);
    }
};

template <class StringType, SizeType Size>
struct Formatter<StringType, char[Size]>
{
    auto operator()(const char (&value)[Size]) const
    {
        return StringType(static_cast<const char *>(value));
    }
};

template <class StringType>
struct Formatter<StringType, wchar_t *>
{
    auto operator()(const wchar_t *value) const
    {
        return StringType(WideStringView(value));
    }
};

template <class StringType, class T>
struct Formatter< StringType, math::detail::Vec2< T > >
{
    HYP_FORCE_INLINE
    static const char *GetFormatString()
    {
        if constexpr (std::is_floating_point_v< T >) {
            static const char *format_string = "[%f %f]";

            return format_string;
        } else if constexpr (std::is_integral_v< T > && std::is_signed_v< T > && sizeof(T) <= 4) {
            static const char *format_string = "[%d %d]";

            return format_string;
        } else if constexpr (std::is_integral_v< T > && std::is_signed_v< T > && sizeof(T) <= 8) {
            static const char *format_string = "[%lld %lld]";

            return format_string;
        } else if constexpr (std::is_integral_v< T > && std::is_unsigned_v< T > && sizeof(T) <= 4) {
            static const char *format_string = "[%u %u]";

            return format_string;
        } else if constexpr (std::is_integral_v< T > && std::is_unsigned_v< T > && sizeof(T) <= 8) {
            static const char *format_string = "[%llu %llu]";

            return format_string;
        } else {
            static_assert(resolution_failure< T >, "Cannot format Vec2 type: unknown inner type");
        }
    }

    auto operator()(const math::detail::Vec2< T > &value) const
    {
        memory::ByteBuffer byte_buffer;
        byte_buffer.SetSize(32);

        int result_size = std::snprintf(reinterpret_cast<char *>(byte_buffer.Data()), byte_buffer.Size(), GetFormatString(), value.x, value.y);

        if (result_size > byte_buffer.Size()) {
            byte_buffer.SetSize(result_size);

            result_size = std::snprintf(reinterpret_cast<char *>(byte_buffer.Data()), byte_buffer.Size(), GetFormatString(), value.x, value.y);
        }

        return StringType(byte_buffer);
    }
};

template <class StringType, class T>
struct Formatter< StringType, math::detail::Vec3< T > >
{
    HYP_FORCE_INLINE
    static const char *GetFormatString()
    {
        if constexpr (std::is_floating_point_v< T >) {
            static const char *format_string = "[%f %f %f]";

            return format_string;
        } else if constexpr (std::is_integral_v< T > && std::is_signed_v< T > && sizeof(T) <= 4) {
            static const char *format_string = "[%d %d %d]";

            return format_string;
        } else if constexpr (std::is_integral_v< T > && std::is_signed_v< T > && sizeof(T) <= 8) {
            static const char *format_string = "[%lld %lld %lld]";

            return format_string;
        } else if constexpr (std::is_integral_v< T > && std::is_unsigned_v< T > && sizeof(T) <= 4) {
            static const char *format_string = "[%u %u %u]";

            return format_string;
        } else if constexpr (std::is_integral_v< T > && std::is_unsigned_v< T > && sizeof(T) <= 8) {
            static const char *format_string = "[%llu %llu %llu]";

            return format_string;
        } else {
            static_assert(resolution_failure< T >, "Cannot format Vec3 type: unknown inner type");
        }
    }

    auto operator()(const math::detail::Vec3< T > &value) const
    {
        memory::ByteBuffer byte_buffer;
        byte_buffer.SetSize(48);

        int result_size = std::snprintf(reinterpret_cast<char *>(byte_buffer.Data()), byte_buffer.Size(), GetFormatString(), value.x, value.y, value.z);

        if (result_size > byte_buffer.Size()) {
            byte_buffer.SetSize(result_size);

            result_size = std::snprintf(reinterpret_cast<char *>(byte_buffer.Data()), byte_buffer.Size(), GetFormatString(), value.x, value.y, value.z);
        }

        return StringType(byte_buffer);
    }
};

template <class StringType, class T>
struct Formatter< StringType, math::detail::Vec4< T > >
{
    HYP_FORCE_INLINE
    static const char *GetFormatString()
    {
        if constexpr (std::is_floating_point_v< T >) {
            static const char *format_string = "[%f %f %f %f]";

            return format_string;
        } else if constexpr (std::is_integral_v< T > && std::is_signed_v< T > && sizeof(T) <= 4) {
            static const char *format_string = "[%d %d %d %d]";

            return format_string;
        } else if constexpr (std::is_integral_v< T > && std::is_signed_v< T > && sizeof(T) <= 8) {
            static const char *format_string = "[%lld %lld %lld %lld]";

            return format_string;
        } else if constexpr (std::is_integral_v< T > && std::is_unsigned_v< T > && sizeof(T) <= 4) {
            static const char *format_string = "[%u %u %u %u]";

            return format_string;
        } else if constexpr (std::is_integral_v< T > && std::is_unsigned_v< T > && sizeof(T) <= 8) {
            static const char *format_string = "[%llu %llu %llu %llu]";

            return format_string;
        } else {
            static_assert(resolution_failure<T>, "Cannot format Vec4 type: unknown inner type");
        }
    }

    auto operator()(const math::detail::Vec4< T > &value) const
    {
        memory::ByteBuffer byte_buffer;
        byte_buffer.SetSize(64);

        int result_size = std::snprintf(reinterpret_cast<char *>(byte_buffer.Data()), byte_buffer.Size(), GetFormatString(), value.x, value.y, value.z, value.w);

        if (result_size > byte_buffer.Size()) {
            byte_buffer.SetSize(result_size);

            result_size = std::snprintf(reinterpret_cast<char *>(byte_buffer.Data()), byte_buffer.Size(), GetFormatString(), value.x, value.y, value.z, value.w);
        }

        return StringType(byte_buffer);
    }
};

namespace detail {

#pragma region ConcatRuntimeStrings

template <int StringType>
struct ConcatRuntimeStrings_Impl;

template <int StringType>
struct ConcatRuntimeStrings_Impl
{
    template <class SecondStringType>
    constexpr auto Impl(const containers::detail::String< StringType > &str0, SecondStringType &&str1) const
    {
        return str0 + str1;
    }

    constexpr auto operator()(const containers::detail::String< StringType > &str0) const
    {
        return str0;
    }

    template <class SecondStringType>
    constexpr auto operator()(const containers::detail::String< StringType > &str0, SecondStringType &&str1) const
    {
        if (str0.Empty() && str1.Empty()) {
            return containers::detail::String<StringType>::empty;
        } else if (str0.Empty()) {
            return str1;
        } else if (str1.Empty()) {
            return str0;
        } else {
            return Impl(str0, str1);
        }
    }

    template <class SecondStringType, class ...OtherStringType>
    constexpr auto operator()(const containers::detail::String< StringType > &str0, SecondStringType &&str1, OtherStringType &&... other) const
    {
        if (str0.Empty() && str1.Empty()) {
            return ConcatRuntimeStrings_Impl()(containers::detail::String< StringType >::empty, other...);
        } else if (str0.Empty()) {
            return ConcatRuntimeStrings_Impl()(str1, other...);
        } else if (str1.Empty()) {
            return ConcatRuntimeStrings_Impl()(str0, other...);
        } else {
            return ConcatRuntimeStrings_Impl()(Impl(str0, str1), std::forward< OtherStringType >(other)...);
        }
    }
};

template <int StringType, class... Args>
constexpr auto ConcatRuntimeStrings(Args &&... strings)
{
    return ConcatRuntimeStrings_Impl< StringType >()(std::forward< Args >(strings)...);
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
containers::detail::String< StringType > FormatString_FormatElement_Runtime(const T &element)
{
    using FormatterSpecializationType = std::conditional_t<
        std::is_pointer_v< std::remove_cvref_t< T > >,
        std::add_pointer_t< std::remove_cvref_t< std::remove_pointer_t< T > > >,
        std::remove_cvref_t< T >
    >;

    static_assert(implementation_exists< Formatter< containers::detail::String< StringType >, FormatterSpecializationType > >, "No Formatter specialization exists for type");

    // if-constexpr is to prevent a huge swath of errors preventing the user from seeing the assertion failure.
    if constexpr (implementation_exists< Formatter< containers::detail::String< StringType >, FormatterSpecializationType > >) {
        return Formatter< containers::detail::String< StringType >, FormatterSpecializationType >{}(element);
    } else {
        return containers::detail::String< StringType >::empty;
    }
}

#pragma endregion FormatString_FormatElement

#pragma region FormatString_BuildTuple

template <auto Str, class Transformer, SizeType SubIndex = 0>
struct FormatString_BuildTuple;

template <auto Str, class Transformer, SizeType StringIndexStart = 0, SizeType StringIndexEnd = 0, SizeType SubIndex = 0>
struct FormatString_BuildTuple_Impl;

template <auto Str, class Transformer, SizeType SubIndex>
struct FormatString_BuildTuple_Impl< Str, Transformer, SizeType(-1), SizeType(-1), SubIndex >
{
    template <class ... Args>
    constexpr auto operator()(Args &&... args) const
    {
        return MakeTuple(Str);
    }
};

template <auto Str, class Transformer, SizeType StringIndexStart, SizeType StringIndexEnd, SizeType SubIndex>
struct FormatString_BuildTuple_Impl
{
    template <class ... Args>
    constexpr auto operator()(Args &&... args) const
    {
        constexpr auto inner_value = containers::helpers::Substr< Str, StringIndexStart + 1, StringIndexEnd >::value;

        if constexpr (inner_value.Size() > 1 /* NUL */) {
            // Parse string to integer, use it as SubIndex.
            constexpr SizeType parsed_integer = SizeType(containers::helpers::ParseInteger< inner_value >::value);

            if constexpr (parsed_integer < sizeof...(Args)) {
                return ConcatTuples(
                    MakeTuple(containers::helpers::Substr< Str, 0, StringIndexStart >::value),
                    ConcatTuples(
                        ForwardAsTuple(ForwardAsTuple(std::forward< Args >(args)...).template GetElement< parsed_integer >()),
                        FormatString_BuildTuple< containers::helpers::Substr< Str, StringIndexEnd + 1, Str.Size() - 1 >::value, Transformer, parsed_integer >{}(std::forward< Args >(args)...)
                    )
                );
            } else {
                static_assert(FormatString_BadFormat_IndexOutOfBounds< Str, inner_value >{}, "String interpolation attempted to access out of range element. Explicitly provided index value was outside of the number of arguments.");
            }
        } else if constexpr (SubIndex < sizeof...(Args)) {
            return ConcatTuples(
                MakeTuple(containers::helpers::Substr< Str, 0, StringIndexStart >::value),
                ConcatTuples(
                    ForwardAsTuple(ForwardAsTuple(std::forward< Args >(args)...).template GetElement< SubIndex >()),
                    FormatString_BuildTuple< containers::helpers::Substr< Str, StringIndexEnd + 1, Str.Size() - 1 >::value, Transformer, SubIndex + 1 >{}(std::forward< Args >(args)...)
                )
            );
        } else {
            static_assert(FormatString_BadFormat_IndexOutOfBounds< Str, SubIndex >{}, "String interpolation attempted to access out of range element. Does the number of arguments match the number of replacement tokens?");

            return MakeTuple(Str);
        }
    }
};

template <auto Str, class Transformer, SizeType SubIndex>
struct FormatString_BuildTuple
{
    template <class ... Args>
    constexpr auto operator()(Args &&... args) const
    {
        return FormatString_BuildTuple_Impl<
            Str,
            Transformer,
            Str.template FindFirst< containers::detail::IntegerSequenceFromString< StaticString { { '{', '\0' } } > >(),
            Str.template FindFirst< containers::detail::IntegerSequenceFromString< StaticString { { '}', '\0' } } > >(),
            SubIndex
        >{}(std::forward< Args >(args)...);
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
containers::detail::String<StringType> FormatString_ProcessTuple_ProcessElements_Runtime(const Tuple< Ts... > &args, std::index_sequence< Indices... >)
{
    return ConcatRuntimeStrings< StringType >(FormatString_FormatElement_Runtime< StringType >(args.template GetElement< Indices >())...);
}

template <int StringType, class ... Ts>
struct FormatString_ProcessTuple_Impl
{
    using Types = Tuple< Ts... >;

    Tuple< Ts... >  args;

    constexpr auto operator()() const
    {
#if 0
        if constexpr (std::is_constant_evaluated())
            return FormatString_ProcessTuple_ProcessElements_CompileTime(args, std::make_index_sequence<sizeof...(Ts)>());
        else
#endif
            return FormatString_ProcessTuple_ProcessElements_Runtime< StringType >(args, std::make_index_sequence< sizeof...(Ts) >());
    }
};

template <int StringType, class ... Ts>
constexpr auto FormatString_ProcessTuple(Tuple< Ts... > &&tup)
{
    return FormatString_ProcessTuple_Impl< StringType, Ts... > { std::move(tup) };
}

#pragma endregion FormatString_ProcessTuple

struct FormatTransformer
{
    static constexpr char delimiter = '%';
    static constexpr uint32 balance_bracket_options = containers::helpers::BALANCE_BRACKETS_NONE;

    template <auto Str>
    static constexpr auto Transform()
    {
        return Str;
    }
};

template <auto Str, class ... Args>
constexpr auto Format_Impl(Args &&... args)
{
    return (FormatString_ProcessTuple< containers::detail::StringType::UTF8 >(
        FormatString_BuildTuple< Str, FormatTransformer >{}(std::forward< Args >(args)...)
    ))();
}

} // namespace detail

template <auto Str, class ... Args>
constexpr auto Format(Args &&... args)
{
    return detail::Format_Impl< Str >(std::forward< Args >(args)...);
}

} // namespace utilities

using utilities::Format;

} // namespace hyperion

// Helper macro for utilities::Format< FormatString >(...)
#define HYP_FORMAT(fmt, ...) Format< StaticString(fmt) >(__VA_ARGS__)

#endif