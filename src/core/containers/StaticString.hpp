/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_STATIC_STRING_HPP
#define HYPERION_STATIC_STRING_HPP

#include <core/Defines.hpp>
#include <core/utilities/Pair.hpp>
#include <core/utilities/Tuple.hpp>

#include <Types.hpp>
#include <HashCode.hpp>

#include <string_view>
#include <array>

namespace hyperion {
namespace containers {

template <class Pair, Pair... Pairs>
struct PairSequence
{
    using Type = Pair;

    static constexpr SizeType Size()
    {
        return sizeof...(Pairs);
    }
};

namespace detail {
template <class T, SizeType N, class F, SizeType... Indices>
constexpr auto MakePairSequenceHelper(F f, std::index_sequence<Indices...>)
{
    return PairSequence<T, (f()[Indices])...>{};
}
} // namespace detail

template <class T, class F>
constexpr auto MakePairSequence(F f)
{
    constexpr SizeType size = f().size();

    return detail::MakePairSequenceHelper<T, size>(f, std::make_index_sequence<size>());
}


namespace detail {

template <class T, SizeType N, class F, SizeType... Indices>
constexpr auto make_seq_helper(F f, std::index_sequence<Indices...>)
{
    return std::integer_sequence<T, (f()[Indices])...>{};
}

template <class T, class F>
constexpr auto make_seq(F f)
{
    constexpr SizeType size = f().size();

    return make_seq_helper<T, size>(f, std::make_index_sequence<size>());
}

template <SizeType Offset, SizeType ... Indices>
constexpr std::index_sequence<(Offset + Indices)...> make_offset_index_sequence(std::index_sequence<Indices...>)
{
    return {};
}

template <SizeType N, SizeType Offset>
using make_offset_index_sequence_t = decltype(make_offset_index_sequence<Offset>(std::make_index_sequence<N>()));

// Fwd decl of IntegerSequenceFromString template
template <auto StaticString>
struct IntegerSequenceFromString;

} // namespace detail


/*! \brief A compile-time string with a fixed size. Some useful operations are provided as helper template structs and as member functions.
 *  Params are provided as template
 * 
 *  \tparam Sz The size of the string, including the null terminator. */
template <SizeType Sz>
struct StaticString
{
    using Iterator = const char *;
    using ConstIterator = const char *;

    static constexpr SizeType size = Sz;

    using CharType = char;

    CharType data[Sz];

    constexpr StaticString(const CharType (&str)[Sz])
    {
        for (SizeType i = 0; i < Sz; ++i) {
            data[i] = str[i];
        }
    }

    template <typename IntegerSequence, int Index = 0>
    constexpr SizeType FindFirst() const
    {
        constexpr auto this_size = Sz - 1; // -1 to account for null terminator
        constexpr auto other_size = IntegerSequence::Size() - 1; // -1 to account for null terminator

        if constexpr (this_size < other_size) {
            return -1;
        } else if constexpr (Index > this_size - other_size) {
            return -1;
        } else {
            bool found = true;

            for (SizeType j = 0; j < other_size && j < this_size; ++j) {
                if (data[Index + j] != IntegerSequence{}.Data()[j]) {
                    found = false;
                    break;
                }
            }

            if (found) {
                return Index;
            } else {
                return FindFirst<IntegerSequence, Index + 1>();
            }
        }
    }

    template <typename IntegerSequence, int Index = (int(Sz) - int(IntegerSequence::Size()))>
    constexpr SizeType FindLast() const
    {
        constexpr auto this_size = Sz - 1; // -1 to account for null terminator
        constexpr auto other_size = IntegerSequence::Size() - 1; // -1 to account for null terminator

        if constexpr (this_size < other_size) {
            return -1;
        } else if constexpr (Index < 0) {
            return -1;
        } else {
            bool found = true;

            for (SizeType j = 0; j < other_size; ++j) {
                if (data[Index + j] != IntegerSequence{}.Data()[j]) {
                    found = false;
                    break;
                }
            }

            if (found) {
                return Index;
            } else {
                return FindLast<IntegerSequence, Index - 1>();
            }
        }
    }

    template <SizeType First, SizeType Last>
    constexpr auto Substr() const
    {
        constexpr auto clamped_end = Last >= Sz ? Sz - 1 : Last;

        StaticString<clamped_end - First + 1> result = { { '\0' } };

        for (SizeType index = First; index < clamped_end; index++) {
            result.data[index] = data[index];
        }

        return result;
    }

    template <auto OtherStaticString>
    constexpr auto Concat() const
    {
        if constexpr (Sz <= 1 && decltype(OtherStaticString)::size <= 1) {
            return StaticString<1> { { '\0' } };
        } else if constexpr (Sz <= 1) {
            return OtherStaticString;
        } else if constexpr (decltype(OtherStaticString)::size <= 1) {
            return StaticString<Sz> { { data } };
        } else {
            return Concat_Impl<OtherStaticString>(std::make_index_sequence<Sz + decltype(OtherStaticString)::size - 1>());
        }
    }

    /*! \brief Count the number of occurrences of a character in the StaticString.
     * 
     *  \tparam Char The character to count.
     * 
     *  \returns The number of occurrences of the character in the StaticString. */
    template <CharType Char>
    constexpr auto Count() const
    {
        SizeType count = 0;

        for (SizeType i = 0; i < Sz; i++) {
            if (data[i] == Char) {
                count++;
            }
        }

        return count;
    }

    // constexpr auto TrimLeft() const
    // {
    //     return Substr(FindTrimLastIndex_Left_Impl(), Sz);
    // }

    // constexpr auto TrimRight() const
    // {
    //     return Substr(0, FindTrimLastIndex_Right_Impl());
    // }

    // constexpr auto Trim() const
    // {
    //     return TrimLeft().TrimRight();
    // }

    constexpr const CharType *Data() const
        { return &data[0]; }

    constexpr SizeType Size() const
        { return Sz; }


    constexpr HashCode GetHashCode() const
    {
        return HashCode::GetHashCode<Sz>(data);
    }

    HYP_DEF_STL_BEGIN_END(
        &data[0],
        &data[0] + Sz
    )

    // Implementations

    template <auto OtherStaticString, SizeType ... Indices>
    constexpr auto Concat_Impl(std::index_sequence<Indices...>) const -> StaticString<Sz + decltype(OtherStaticString)::size - 1 /* remove extra null terminator */>
    {
        return {
            {
                (Indices < (Sz - 1) ? data[Indices] : OtherStaticString.data[Indices - Sz + 1])...
            }
        };
    }

    constexpr SizeType FindTrimLastIndex_Left_Impl() const
    {
        constexpr char whitespace_chars[] = { ' ', '\n', '\r', '\t', '\f', '\v' };

        SizeType index = 0;

        for (; index < Sz; index++) {
            bool found = false;

            for (SizeType j = 0; j < std::size(whitespace_chars); j++) {
                if (data[index] == whitespace_chars[j]) {
                    found = true;

                    break;
                }
            }

            if (!found) {
                break;
            }
        }

        return index;
    }

    constexpr SizeType FindTrimLastIndex_Right_Impl() const
    {
        constexpr char whitespace_chars[] = { ' ', '\n', '\r', '\t', '\f', '\v' };

        SizeType index = Sz - 1/* for NUL char*/;

        for (; index != 0; index--) {
            bool found = false;

            for (SizeType j = 0; j < std::size(whitespace_chars); j++) {
                if (data[index - 1] == whitespace_chars[j]) {
                    found = true;

                    break;
                }
            }

            if (!found) {
                break;
            }
        }

        if (index == Sz - 1) {
            return -1;
        }

        return index;
    }
};

namespace detail {

template <auto StaticString>
struct IntegerSequenceFromString
{
private:
    constexpr static auto value = detail::make_seq<char>([] { return std::string_view { StaticString.data }; });

public:
    using Type = decltype(value);

    static constexpr const char *Data()
        { return &StaticString.data[0]; }

    static constexpr SizeType Size()
        { return StaticString.size; }
};

} // namespace detail

namespace helpers {
struct BasicStaticStringTransformer
{
    static constexpr bool keep_delimiter = true;

    static constexpr uint32 balance_bracket_options = 0;

    template <auto String>
    static constexpr auto Transform()
    {
        return String;
    }
};

#pragma region Substr

namespace detail {
template <auto String, SizeType Start, SizeType End, bool PastEnd>
struct Substr_Impl;

template <auto String, SizeType Start, SizeType End>
struct Substr_Impl<String, Start, End, false>
{
    template <SizeType ... Indices>
    constexpr auto operator()(std::index_sequence<Indices...>) const
    {
        return StaticString { { String.data[Indices]..., '\0' } };
    }
};

template <auto String, SizeType Start, SizeType End>
struct Substr_Impl<String, Start, End, true>
{
    template <SizeType ... Indices>
    constexpr auto operator()(std::index_sequence<Indices...>) const
    {
        return StaticString { { '\0' } };
    }
};

} // namespace detail

template <auto String, SizeType Start, SizeType End>
struct Substr
{
    static constexpr auto value = detail::Substr_Impl<String, Start, End, Start >= (End >= String.size ? String.size - 1 : End)>()(containers::detail::make_offset_index_sequence_t<(End >= String.size ? String.size - 1 : End) - Start, Start>());
};

/*template <auto String, SizeType Start>
struct Substr<String, Start, SizeType(-1)>
{
    static_assert(Start <= String.size - 1, "Start must be less than or equal to end");
    static_assert(Start < String.size, "Start must be less than string size");
    
    static constexpr auto value = detail::Substr_Impl<String, Start, String.size - 1, Start >= String.size - 1>()(containers::detail::make_offset_index_sequence_t<(String.size - 1) - Start, Start>());
};*/

#pragma endregion Substr

#pragma region Trim

namespace detail {

template <auto Str>
constexpr SizeType FindTrimLastIndex_Left()
{
    return Str.FindTrimLastIndex_Left_Impl();
}

template <auto Str>
constexpr SizeType FindTrimLastIndex_Right()
{
    return Str.FindTrimLastIndex_Right_Impl();
}

// TrimLeft

template <auto String, SizeType LastIndex>
struct TrimLeft_Impl;

template <auto String>
struct TrimLeft_Impl<String, 0>
{
    // Trim left side
    static constexpr auto value = String;
};

template <auto String, SizeType LastIndex>
struct TrimLeft_Impl
{
    // Trim left side
    static constexpr auto value = Substr< String, LastIndex, String.Size() >::value;
};

// TrimRight
template <auto String, SizeType LastIndex>
struct TrimRight_Impl;

template <auto String>
struct TrimRight_Impl<String, SizeType(-1)>
{
    // Trim right side
    static constexpr auto value = String;
};

template <auto String, SizeType LastIndex>
struct TrimRight_Impl
{
    // Trim right side
    static constexpr auto value = Substr< String, 0, LastIndex >::value;
};

} // namespace detail

/*! \brief Trims the left side of a StaticString, removing whitespace characters at the front.
 * 
 *  \tparam String The StaticString to trim.
 * 
 *  \returns A StaticString with the left side trimmed. */
template <auto String>
struct TrimLeft
{
    // Trim left side
    static constexpr auto value = detail::TrimLeft_Impl< String, detail::FindTrimLastIndex_Left<String>() >::value;
};

/*! \brief Trims the right side of a StaticString, removing whitespace characters at the end.
 * 
 *  \tparam String The StaticString to trim.
 * 
 *  \returns A StaticString with the right side trimmed. */
template <auto String>
struct TrimRight
{
    // Trim left side
    static constexpr auto value = detail::TrimRight_Impl< String, detail::FindTrimLastIndex_Right<String>() >::value;
};

/*! \brief Trims both sides of a StaticString, removing whitespace characters at the start and end of the string.
 * 
 *  \tparam String The StaticString to trim.
 * 
 *  \returns A StaticString with both sides trimmed. */
template <auto String>
struct Trim
{
    // Trim left side
    static constexpr auto value = TrimRight< TrimLeft< String >::value >::value;
};

#pragma endregion Trim

#pragma region Split

namespace detail {

template <auto String, char Delimiter, class Transformer, SizeType Index = 0>
struct Split_Impl;

template <auto String, char Delimiter, class Transformer>
struct Split_Impl<String, Delimiter, Transformer, SizeType(-1)>
{
    static constexpr auto value = MakeTuple(Transformer::template Transform< String >());
};

template <auto String, char Delimiter, bool KeepDelimiter>
struct Split_ApplyDelimiter_Impl;

template <auto String, char Delimiter>
struct Split_ApplyDelimiter_Impl<String, Delimiter, true>
{
    static constexpr auto value = String.template Concat< StaticString { { Delimiter, '\0' } } >();
};

template <auto String, char Delimiter>
struct Split_ApplyDelimiter_Impl<String, Delimiter, false>
{
    static constexpr auto value = String;
};

} // namespace detail

/*! \brief Splits a StaticString by a delimiter.
 * 
 *  \tparam String The StaticString to split.
 *  \tparam Delimiter The delimiter to split the StaticString by.
 *  \tparam Transformer The transformation to apply to each part.
 * 
 *  \returns A tuple of StaticStrings, the result of splitting the StaticString by the delimiter. */
template <auto String, char Delimiter, class Transformer = BasicStaticStringTransformer>
struct Split;

namespace detail {
template <auto String, char Delimiter, class Transformer, SizeType Index>
struct Split_Impl
{
    static constexpr auto value = ConcatTuples(
        MakeTuple(Transformer::template Transform< Split_ApplyDelimiter_Impl< Substr< String, 0, Index >::value, Delimiter, Transformer::keep_delimiter >::value >()),
        MakeTuple(Split< Substr< String, Index + 1, String.Size() >::value, Delimiter, Transformer >::value)
    );
};
} // namespace detail

template <auto String, char Delimiter, class Transformer>
struct Split
{
    static constexpr auto value = detail::Split_Impl< String, Delimiter, Transformer, String.template FindFirst< containers::detail::IntegerSequenceFromString< StaticString { { Delimiter, '\0' } } > >() >::value;
};

#pragma endregion Split

#pragma region Concat

/*! \brief Concatenates a list of StaticStrings.
 * 
 *  \tparam Strings The StaticStrings to concatenate.
 *  
 *  \returns A StaticString, the result of concatenating all the StaticStrings. */
template <auto... Strings>
struct Concat;

template <auto Last>
struct Concat<Last>
{
    static constexpr auto value = Last;
};

template <auto First, auto... Rest>
struct Concat<First, Rest...>
{
    static constexpr auto value = First.template Concat<Concat<Rest...>::value>();
};

namespace detail {

template <SizeType FirstSize, SizeType ...OtherSizes>
struct ConcatStrings_Impl;

template <SizeType FirstSize, SizeType ...OtherSizes>
struct ConcatStrings_Impl
{
    template <class SecondStaticStringType, SizeType ... Indices>
    constexpr auto Impl(StaticString<FirstSize> str0, SecondStaticStringType &&str1, std::index_sequence<Indices...>) const -> StaticString<FirstSize + SecondStaticStringType::size - 1 /* remove extra null terminator */>
    {
        return {
            {
                (Indices < (FirstSize - 1) ? str0.data[Indices] : str1.data[Indices - FirstSize + 1])...
            }
        };
    }

    constexpr auto operator()(StaticString<FirstSize> str0) const -> StaticString<FirstSize>
    {
        return str0;
    }

    template <class SecondStaticStringType>
    constexpr auto operator()(StaticString<FirstSize> str0, SecondStaticStringType &&str1) const -> StaticString<(FirstSize - 1) + (OtherSizes + ... + 0) - (sizeof...(OtherSizes)) + 1>
    {
        if constexpr (FirstSize <= 1 && SecondStaticStringType::size <= 1) {
            return StaticString<1> { { '\0' } };
        } else if constexpr (FirstSize <= 1) {
            return SecondStaticStringType { { str1.data } };
        } else if constexpr (SecondStaticStringType::size <= 1) {
            return StaticString<FirstSize> { { str0.data } };
        } else {
            return Impl(str0, std::forward<SecondStaticStringType>(str1), std::make_index_sequence<FirstSize + SecondStaticStringType::size - 1>());
        }
    }

    template <class SecondStaticStringType, class ...OtherStaticStringTypes>
    constexpr auto operator()(StaticString<FirstSize> str0, SecondStaticStringType &&str1, OtherStaticStringTypes &&... other) const -> StaticString<(FirstSize - 1) + (OtherSizes + ... + 0) - (sizeof...(OtherSizes)) + 1>
    {
        if constexpr (FirstSize <= 1 && SecondStaticStringType::size <= 1) {
            return ConcatStrings_Impl<1, OtherStaticStringTypes::size...>()(StaticString<1> { { '\0' } }, std::forward<OtherStaticStringTypes>(other)...);
        } else if constexpr (FirstSize <= 1) {
            return ConcatStrings_Impl<SecondStaticStringType::size, OtherStaticStringTypes::size...>()(SecondStaticStringType { { str1.data } }, std::forward<OtherStaticStringTypes>(other)...);
        } else if constexpr (SecondStaticStringType::size <= 1) {
            return ConcatStrings_Impl<FirstSize, OtherStaticStringTypes::size...>()(StaticString<FirstSize> { { str0.data } }, std::forward<OtherStaticStringTypes>(other)...);
        } else {
            return ConcatStrings_Impl<FirstSize + SecondStaticStringType::size - 1, OtherStaticStringTypes::size...>()(Impl(str0, std::forward<SecondStaticStringType>(str1), std::make_index_sequence<FirstSize + SecondStaticStringType::size - 1>()), std::forward<OtherStaticStringTypes>(other)...);
        }
    }
};

} // namespace detail

template <class ... StaticStringInstance>
constexpr auto ConcatStrings(StaticStringInstance &&... strings)
{
    return detail::ConcatStrings_Impl<std::remove_cvref_t<decltype(strings)>::size...>()(std::forward<StaticStringInstance>(strings)...);
}

#pragma endregion Concat

#pragma region TransformParts


namespace detail {

template <class Transformer, SizeType FirstSize, SizeType ...OtherSizes>
struct TransformParts_Impl;

template <class Transformer, SizeType FirstSize, SizeType ...OtherSizes>
struct TransformParts_Impl
{
    template <SizeType ... Indices>
    constexpr auto Impl(StaticString<FirstSize> str0, std::index_sequence<Indices...>) const
    {
        return str0;
    }

    template <class SecondStaticStringType, SizeType ... Indices>
    constexpr auto Impl(StaticString<FirstSize> str0, SecondStaticStringType &&str1, std::index_sequence<Indices...>) const
    {
        return ConcatStrings(str0, StaticString { { Transformer::delimiter, '\0' } }, std::forward<SecondStaticStringType>(str1));
    }

    constexpr auto operator()(StaticString<FirstSize> str0) const
    {
        return str0;
    }

    template <class SecondStaticStringType>
    constexpr auto operator()(StaticString<FirstSize> str0, SecondStaticStringType &&str1) const
    {
        if constexpr (FirstSize <= 1 && SecondStaticStringType::size <= 1) {
            return StaticString<1> { { '\0' } };
        } else if constexpr (FirstSize <= 1) {
            return SecondStaticStringType { { str1.data } };
        } else if constexpr (SecondStaticStringType::size <= 1) {
            return StaticString<FirstSize> { { str0.data } };
        } else {
            return Impl(str0, std::forward<SecondStaticStringType>(str1), std::make_index_sequence<FirstSize + SecondStaticStringType::size - 1>());
        }
    }

    template <class SecondStaticStringType, class ...OtherStaticStringTypes>
    constexpr auto operator()(StaticString<FirstSize> str0, SecondStaticStringType &&str1, OtherStaticStringTypes &&... other) const
    {
        if constexpr (FirstSize <= 1 && SecondStaticStringType::size <= 1) {
            return TransformParts_Impl<Transformer, 1, OtherStaticStringTypes::size...>()(StaticString<1> { { '\0' } }, std::forward<OtherStaticStringTypes>(other)...);
        } else if constexpr (FirstSize <= 1) {
            return TransformParts_Impl<Transformer, SecondStaticStringType::size, OtherStaticStringTypes::size...>()(SecondStaticStringType { { str1.data } }, std::forward<OtherStaticStringTypes>(other)...);
        } else if constexpr (SecondStaticStringType::size <= 1) {
            return TransformParts_Impl<Transformer, FirstSize, OtherStaticStringTypes::size...>()(StaticString<FirstSize> { { str0.data } }, std::forward<OtherStaticStringTypes>(other)...);
        } else {
            return TransformParts_Impl<Transformer, FirstSize + SecondStaticStringType::size + 1 - 1, OtherStaticStringTypes::size...>()(Impl(str0, std::forward<SecondStaticStringType>(str1), std::make_index_sequence<FirstSize + SecondStaticStringType::size - 1>()), std::forward<OtherStaticStringTypes>(other)...);
        }
    }
};

} // namespace detail

/*! \brief Transforms a list of StaticStrings using a Transformer and concatenates them.
 * 
 *  \tparam Transformer The transformer to apply to each StaticString. Must have a static constexpr char delimiter member, and a static constexpr auto Transform<auto String>() function.
 *  \tparam Strings The StaticStrings to transform.
 * 
 *  \returns A StaticString, the result of transforming all the StaticStrings, and concatenating them. */
template <class Transformer, auto ... Strings>
struct TransformParts
{
    static constexpr auto value = detail::TransformParts_Impl<Transformer, Transformer::template Transform< Strings >().Size()...>()(Transformer::template Transform< Strings >()...);
};

#pragma endregion TransformParts

#pragma region FindCharCount

enum BalanceBracketsOptions : uint32
{
    BALANCE_BRACKETS_NONE           = 0x0,
    BALANCE_BRACKETS_SQUARE         = 0x1,
    BALANCE_BRACKETS_PARENTHESES    = 0x2,
    BALANCE_BRACKETS_ANGLE          = 0x4
};

template <auto String, char Delimiter, uint32 BracketOptions>
struct FindCharCount;

template <auto String, char Delimiter>
struct FindCharCount<String, Delimiter, BALANCE_BRACKETS_NONE>
{
    static constexpr SizeType value = String.template Count< Delimiter >();
};

namespace detail {

template <auto String, char Delimiter, uint32 BracketOptions>
struct FindCharCount_Impl
{
    constexpr SizeType operator()() const
    {
        constexpr char brackets[] = "[]()<>";

        SizeType count = 0;

        int bracket_counts[(std::size(brackets) - 1) / 2] = { 0 };

        for (SizeType i = 0; i < String.Size(); i++) {
            for (SizeType j = 0; j < std::size(brackets) - 1; j++) {
                if (String.data[i] == brackets[j]) {
                    bracket_counts[j / 2] += (j % 2) ? -1 : 1;

                    break;
                }
            }

            if (String.data[i] == Delimiter) {
                if ((BracketOptions & BALANCE_BRACKETS_SQUARE) && bracket_counts[0] > 0) {
                    continue;
                }
                if ((BracketOptions & BALANCE_BRACKETS_PARENTHESES) && bracket_counts[1] > 0) {
                    continue;
                }
                if ((BracketOptions & BALANCE_BRACKETS_ANGLE) && bracket_counts[2] > 0) {
                    continue;
                }

                ++count;
            }
        }

        return count;
    }
};

} // namespace detail

template <auto String, char Delimiter, uint32 BracketOptions>
struct FindCharCount
{
    static constexpr SizeType value = detail::FindCharCount_Impl<String, Delimiter, BracketOptions>{}();
};

#pragma endregion FindCharCount

#pragma region GetSplitIndices

namespace detail {

template <auto String, char Delimiter, uint32 BracketOptions, SizeType Count>
struct GetSplitIndices_Impl
{
    constexpr auto operator()() const
    {
        return containers::MakePairSequence<Pair<SizeType, SizeType>>([]() -> std::array<Pair<SizeType, SizeType>, Count + 1>
        {
            std::array<Pair<SizeType, SizeType>, Count + 1> split_indices = { };

            if constexpr (Count == 0) {
                split_indices[0] = Pair<SizeType, SizeType> { 0, String.Size() - 1 /* -1 for NUL char */ };
            } else {
                std::array<char, Count> delimiter_indices = { };

                SizeType index = 0;

                constexpr char brackets[] = "[]()<>";

                int bracket_counts[(std::size(brackets) - 1) / 2] = { 0 };

                for (SizeType i = 0; i < String.size; i++) {
                    for (SizeType j = 0; j < std::size(brackets) - 1; j++) {
                        if (String.data[i] == brackets[j]) {
                            bracket_counts[j / 2] += (j % 2) ? -1 : 1;

                            break;
                        }
                    }

                    if (String.data[i] == Delimiter) {
                        if ((BracketOptions & BALANCE_BRACKETS_SQUARE) && bracket_counts[0] > 0) {
                            continue;
                        }
                        if ((BracketOptions & BALANCE_BRACKETS_PARENTHESES) && bracket_counts[1] > 0) {
                            continue;
                        }
                        if ((BracketOptions & BALANCE_BRACKETS_ANGLE) && bracket_counts[2] > 0) {
                            continue;
                        }
                    }

                    if (String.data[i] == Delimiter) {
                        delimiter_indices[index++] = i;
                    }
                }

                for (SizeType i = 0; i < std::size(delimiter_indices); i++) {
                    SizeType prev = i == 0 ? 0 : delimiter_indices[i - 1] + 1;
                    SizeType current = delimiter_indices[i];

                    split_indices[i] = { prev, current };
                }

                split_indices[Count] = Pair<SizeType, SizeType> {
                    SizeType(delimiter_indices[std::size(delimiter_indices) - 1] + 1),
                    String.Size() - 1 /* -1 for NUL char */
                };
            }

            return split_indices;
        });
    }
};

} // namespace detail

/*! \brief Get the indices of the occurrences of a character in the StaticString.  */
template <auto String, char Delimiter, uint32 BracketOptions, SizeType Count = SizeType(-1)>
struct GetSplitIndices;

template <auto String, char Delimiter, uint32 BracketOptions>
struct GetSplitIndices<String, Delimiter, BracketOptions, SizeType(-1)>
{
    static constexpr auto value = GetSplitIndices< String, Delimiter, BracketOptions, FindCharCount< String, Delimiter, BracketOptions >::value >::value;
};

template <auto String, char Delimiter, uint32 BracketOptions, SizeType Count>
struct GetSplitIndices
{
    static constexpr auto value = detail::GetSplitIndices_Impl< String, Delimiter, BracketOptions, Count >{}();
};

#pragma endregion GetSplitIndices

#pragma region TransformSplit

namespace detail {

template <class Transformer, auto String, Pair< SizeType, SizeType > ... SplitIndices>
constexpr auto TransformSplit_Impl(PairSequence< Pair< SizeType, SizeType >, SplitIndices... >)
{
    return TransformParts< Transformer, Substr< String, SplitIndices.first, SplitIndices.second >::value... >::value;
}

} // namespace detail

/*! \brief Splits a StaticString by delimiter, applies a transformation to each element, and rejoins it into a single StaticString.
 * 
 *  \tparam Transformer The transformer to apply to each element in the split StaticString. Must have a static constexpr char delimiter member, and a static constexpr auto Transform<auto String>() function.
 *  \tparam Strings The StaticStrings to transform.
 * 
 *  \returns A StaticString, the result of concatenating each of the transformed StaticStrings after splitting the input by a delimtier. */
template <class Transformer, auto String>
struct TransformSplit
{
    static constexpr auto value = detail::TransformSplit_Impl< Transformer, String >(GetSplitIndices< String, Transformer::delimiter, Transformer::balance_bracket_options >::value);
};

#pragma endregion TransformSplit

#pragma region ParseInteger

template <auto String, SizeType CharIndex = 0, int Value = 0>
struct ParseInteger;

namespace detail {

template <auto String, SizeType CharIndex>
struct ParseInteger_ParseChar_Impl
{
    static constexpr auto value = String.data[CharIndex] - '0';
};

template <auto String, SizeType CharIndex, int Value, bool AtEnd>
struct ParseInteger_Impl;

template <auto String, SizeType CharIndex, int Value>
struct ParseInteger_Impl<String, CharIndex, Value, false>
{
    static constexpr auto value = ParseInteger< String, CharIndex + 1, Value * 10 + ParseInteger_ParseChar_Impl< String, CharIndex >::value >::value;
};

template <auto String, SizeType CharIndex, int Value>
struct ParseInteger_Impl<String, CharIndex, Value, true>
{
    static constexpr auto value = Value;
};

} // namespace detail

template <auto String, SizeType CharIndex, int Value>
struct ParseInteger
{
    static constexpr auto value = detail::ParseInteger_Impl< String, CharIndex, Value, CharIndex >= String.size - 1 || String.data[CharIndex] == '\0' >::value;
};

#pragma endregion ParseInteger

} // namespace helpers

} // namespace containers

using containers::StaticString;

} // namespace hyperion

#endif