/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_STRING_FWD_HPP
#define HYPERION_STRING_FWD_HPP

#include <util/UTF8.hpp>

#include <Types.hpp>

namespace hyperion {
namespace containers {

enum StringType : int
{
    NONE = 0,

    ANSI = 1,
    UTF8 = 2,
    UTF16 = 3,
    UTF32 = 4,
    WIDE_CHAR = 5,

    MAX
};

using namespace utf;

template <int StringType>
struct StringTypeImpl
{
};

template <>
struct StringTypeImpl<ANSI>
{
    using CharType = char;
    using WidestCharType = char;
};

template <>
struct StringTypeImpl<UTF8>
{
    using CharType = char;
    using WidestCharType = utf::u32char;
};

template <>
struct StringTypeImpl<UTF16>
{
    using CharType = utf::u16char;
    using WidestCharType = utf::u16char;
};

template <>
struct StringTypeImpl<UTF32>
{
    using CharType = utf::u32char;
    using WidestCharType = utf::u32char;
};

template <>
struct StringTypeImpl<WIDE_CHAR>
{
    using CharType = wchar_t;
    using WidestCharType = wchar_t;
};

template <int TStringType>
class String;

} // namespace containers

namespace utilities {
template <int TStringType>
class StringView;

} // namespace utilities

using StringType = containers::StringType;

using String = containers::String<StringType::UTF8>;
using ANSIString = containers::String<StringType::ANSI>;
using WideString = containers::String<StringType::WIDE_CHAR>;
using UTF32String = containers::String<StringType::UTF32>;
using UTF16String = containers::String<StringType::UTF16>;
using PlatformString = containers::String<std::is_same_v<TChar, wchar_t> ? StringType::WIDE_CHAR : StringType::UTF8>;

template <int TStringType>
using StringView = utilities::StringView<TStringType>;

using ANSIStringView = StringView<StringType::ANSI>;
using UTF8StringView = StringView<StringType::UTF8>;
using UTF16StringView = StringView<StringType::UTF16>;
using UTF32StringView = StringView<StringType::UTF32>;
using WideStringView = StringView<StringType::WIDE_CHAR>;

// traits
template <class T>
struct IsString
{
    static constexpr bool value = false;
};

template <int TStringType>
struct IsString<containers::String<TStringType>>
{
    static constexpr bool value = true;
};

} // namespace hyperion

#endif