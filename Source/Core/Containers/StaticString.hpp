/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>
#include <Core/Utilities/Pair.hpp>
#include <Core/Utilities/Tuple.hpp>
#include <Core/Utilities/StringView.hpp>

#include <Core/Types.hpp>
#include <Core/HashCode.hpp>

#include <string_view>
#include <array>

namespace Hyperion {
namespace containers {

template <class Pair, Pair... Pairs>
struct PairSequence
{
    using Type = Pair;

    static constexpr size_t Size()
    {
        return sizeof...(Pairs);
    }
};

template <class T, size_t N, class F, size_t... Indices>
constexpr auto MakePairSequenceHelper(F f, std::index_sequence<Indices...>)
{
    return PairSequence<T, (f()[Indices])...> {};
}

template <class T, class F>
constexpr auto MakePairSequence(F f)
{
    constexpr size_t size = f().size();

    return MakePairSequenceHelper<T, size>(f, std::make_index_sequence<size>());
}

template <class T, size_t N, class F, size_t... Indices>
constexpr auto makeSeqHelper(F f, std::index_sequence<Indices...>)
{
    return std::integer_sequence<T, (f()[Indices])...> {};
}

template <class T, class F>
constexpr auto makeSeq(F f)
{
    constexpr size_t size = f().size();

    return makeSeqHelper<T, size>(f, std::make_index_sequence<size>());
}

template <size_t Offset, size_t... Indices>
constexpr std::index_sequence<(Offset + Indices)...> makeOffsetIndexSequence(std::index_sequence<Indices...>)
{
    return {};
}

template <size_t N, size_t Offset>
using OffsetSequence = decltype(makeOffsetIndexSequence<Offset>(std::make_index_sequence<N>()));

// Fwd decl of IntegerSequenceFromString template
template <auto StaticString>
struct IntegerSequenceFromString;

/*! \brief A compile-time string with a fixed size. Some useful operations are provided as helper template structs and as member functions.
 *  Params are provided as template
 *
 *  \tparam Sz The size of the string, including the null terminator. */
template <size_t Sz>
struct StaticString
{
    using CharType = char;

    using Iterator = const CharType*;
    using ConstIterator = const CharType*;

    static constexpr size_t size = Sz;

    CharType data[Sz];

    constexpr StaticString(const CharType (&str)[Sz])
        : data { '\0' }
    {
        for (size_t i = 0; i < Sz; ++i)
        {
            data[i] = str[i];
        }
    }

    constexpr StaticString(const CharType* begin, const CharType* end)
        : data { '\0' }
    {
        size_t index = 0;

        for (const CharType* ptr = begin; ptr != end && index < Sz; ++ptr, ++index)
        {
            data[index] = *ptr;
        }

        // null terminate
        if (index < Sz)
        {
            data[index] = '\0';
        }
    }

    constexpr operator StringView<StringType::ANSI>() const
    {
        return StringView<StringType::ANSI>(Begin(), Begin() + Sz - 1);
    }

    constexpr operator StringView<StringType::UTF8>() const
    {
        return StringView<StringType::UTF8>(Begin(), Begin() + Sz - 1);
    }

    template <typename IntegerSequence, int Index = 0>
    constexpr size_t FindFirst() const
    {
        constexpr auto thisSize = Sz - 1;                       // -1 to account for null terminator
        constexpr auto otherSize = IntegerSequence::Size() - 1; // -1 to account for null terminator

        if constexpr (thisSize < otherSize)
        {
            return -1;
        }
        else if constexpr (Index > thisSize - otherSize)
        {
            return -1;
        }
        else
        {
            bool found = true;

            for (size_t j = 0; j < otherSize && j < thisSize; ++j)
            {
                if (data[Index + j] != IntegerSequence {}.Data()[j])
                {
                    found = false;
                    break;
                }
            }

            if (found)
            {
                return Index;
            }
            else
            {
                return FindFirst<IntegerSequence, Index + 1>();
            }
        }
    }

    template <typename IntegerSequence, int Index = (int(Sz) - int(IntegerSequence::Size()))>
    constexpr size_t FindLast() const
    {
        constexpr auto thisSize = Sz - 1;                       // -1 to account for null terminator
        constexpr auto otherSize = IntegerSequence::Size() - 1; // -1 to account for null terminator

        if constexpr (thisSize < otherSize)
        {
            return -1;
        }
        else if constexpr (Index < 0)
        {
            return -1;
        }
        else
        {
            bool found = true;

            for (size_t j = 0; j < otherSize; ++j)
            {
                if (data[Index + j] != IntegerSequence {}.Data()[j])
                {
                    found = false;
                    break;
                }
            }

            if (found)
            {
                return Index;
            }
            else
            {
                return FindLast<IntegerSequence, Index - 1>();
            }
        }
    }

    template <size_t First, size_t Last>
    constexpr auto Substr() const
    {
        constexpr auto clampedEnd = Last >= Sz ? Sz - 1 : Last;

        StaticString<clampedEnd - First + 1> result = { { '\0' } };

        for (size_t index = First; index < clampedEnd; index++)
        {
            result.data[index] = data[index];
        }

        return result;
    }

    template <auto OtherStaticString>
    constexpr auto Concat() const
    {
        if constexpr (Sz <= 1 && decltype(OtherStaticString)::size <= 1)
        {
            return StaticString<1> { { '\0' } };
        }
        else if constexpr (Sz <= 1)
        {
            return OtherStaticString;
        }
        else if constexpr (decltype(OtherStaticString)::size <= 1)
        {
            return StaticString<Sz> { { data } };
        }
        else
        {
            return Concat_Impl<OtherStaticString>(std::make_index_sequence<Sz + decltype(OtherStaticString)::size - 1>());
        }
    }

    /*! \brief Count the number of occurrences of a character in the StaticString.
     *
     *  \tparam char The character to count.
     *
     *  \returns The number of occurrences of the character in the StaticString. */
    template <CharType Char>
    constexpr auto Count() const
    {
        size_t count = 0;

        for (size_t i = 0; i < Sz; i++)
        {
            if (data[i] == Char)
            {
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

    constexpr const CharType* Data() const
    {
        return &data[0];
    }

    constexpr size_t Size() const
    {
        return Sz;
    }

    constexpr HashCode GetHashCode() const
    {
        return HashCode::GetHashCode(data);
    }

    HYP_DEF_STL_BEGIN_END_CONSTEXPR(
        data,
        data + Sz)

    // Implementations

    template <auto OtherStaticString, size_t... Indices>
    constexpr auto Concat_Impl(std::index_sequence<Indices...>) const -> StaticString<Sz + decltype(OtherStaticString)::size - 1 /* remove extra null terminator */>
    {
        return {
            { (Indices < (Sz - 1) ? data[Indices] : OtherStaticString.data[Indices - Sz + 1])... }
        };
    }

    constexpr size_t FindTrimLastIndex_Left_Impl() const
    {
        constexpr char WhitespaceChars[] = { ' ', '\n', '\r', '\t', '\f', '\v' };

        size_t index = 0;

        for (; index < Sz; index++)
        {
            bool found = false;

            for (size_t j = 0; j < sizeof(WhitespaceChars); j++)
            {
                if (data[index] == WhitespaceChars[j])
                {
                    found = true;

                    break;
                }
            }

            if (!found)
            {
                break;
            }
        }

        return index;
    }

    constexpr size_t FindTrimLastIndex_Right_Impl() const
    {
        constexpr char WhitespaceChars[] = { ' ', '\n', '\r', '\t', '\f', '\v' };

        size_t index = Sz - 1 /* for NUL char*/;

        for (; index != 0; index--)
        {
            bool found = false;

            for (size_t j = 0; j < sizeof(WhitespaceChars); j++)
            {
                if (data[index - 1] == WhitespaceChars[j])
                {
                    found = true;

                    break;
                }
            }

            if (!found)
            {
                break;
            }
        }

        if (index == Sz - 1)
        {
            return -1;
        }

        return index;
    }
};

template <auto StaticString>
struct IntegerSequenceFromString
{
    using StaticStringType = decltype(StaticString);
    using CharType = typename StaticStringType::CharType;

private:
    constexpr static auto value = makeSeq<CharType>([]
        {
            return std::string_view { StaticString.data };
        });

public:
    using Type = decltype(value);

    static constexpr const CharType* Data()
    {
        return &StaticString.data[0];
    }

    static constexpr size_t Size()
    {
        return StaticString.size;
    }
};

namespace helpers {
struct BasicStaticStringTransformer
{
    static constexpr bool keepDelimiter = true;

    static constexpr uint32 balanceBracketOptions = 0;

    template <auto String>
    static constexpr auto Transform()
    {
        return String;
    }
};

#pragma region Substr

template <auto String, size_t Start, size_t End, bool PastEnd>
struct Substr_Impl;

template <auto String, size_t Start, size_t End>
struct Substr_Impl<String, Start, End, false>
{
    template <size_t... Indices>
    constexpr auto operator()(std::index_sequence<Indices...>) const
    {
        return StaticString { { String.data[Indices]..., '\0' } };
    }
};

template <auto String, size_t Start, size_t End>
struct Substr_Impl<String, Start, End, true>
{
    template <size_t... Indices>
    constexpr auto operator()(std::index_sequence<Indices...>) const
    {
        return StaticString { { '\0' } };
    }
};

template <auto String, size_t Start, size_t End>
struct Substr
{
    static constexpr auto value = Substr_Impl<String, Start, End, Start >= (End >= String.size ? String.size - 1 : End)>()(containers::OffsetSequence<(End >= String.size ? String.size - 1 : End) - Start, Start>());
};

/*template <auto String, size_t Start>
struct Substr<String, Start, size_t(-1)>
{
    static_assert(Start <= String.size - 1, "Start must be less than or equal to end");
    static_assert(Start < String.size, "Start must be less than string size");

    static constexpr auto value = Substr_Impl<String, Start, String.size - 1, Start >= String.size - 1>()(containers::OffsetSequence<(String.size - 1) - Start, Start>());
};*/

#pragma endregion Substr

#pragma region Trim

template <auto Str>
constexpr size_t FindTrimLastIndex_Left()
{
    return Str.FindTrimLastIndex_Left_Impl();
}

template <auto Str>
constexpr size_t FindTrimLastIndex_Right()
{
    return Str.FindTrimLastIndex_Right_Impl();
}

// TrimLeft

template <auto String, size_t LastIndex>
struct TrimLeft_Impl;

template <auto String>
struct TrimLeft_Impl<String, 0>
{
    // Trim left side
    static constexpr auto value = String;
};

template <auto String, size_t LastIndex>
struct TrimLeft_Impl
{
    // Trim left side
    static constexpr auto value = Substr<String, LastIndex, String.Size()>::value;
};

// TrimRight
template <auto String, size_t LastIndex>
struct TrimRight_Impl;

template <auto String>
struct TrimRight_Impl<String, size_t(-1)>
{
    // Trim right side
    static constexpr auto value = String;
};

template <auto String, size_t LastIndex>
struct TrimRight_Impl
{
    // Trim right side
    static constexpr auto value = Substr<String, 0, LastIndex>::value;
};

/*! \brief Trims the left side of a StaticString, removing whitespace characters at the front.
 *
 *  \tparam String The StaticString to trim.
 *
 *  \returns A StaticString with the left side trimmed. */
template <auto String>
struct TrimLeft
{
    // Trim left side
    static constexpr auto value = TrimLeft_Impl<String, FindTrimLastIndex_Left<String>()>::value;
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
    static constexpr auto value = TrimRight_Impl<String, FindTrimLastIndex_Right<String>()>::value;
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
    static constexpr auto value = TrimRight<TrimLeft<String>::value>::value;
};

#pragma endregion Trim

#pragma region Split

template <auto String, char Delimiter, class Transformer, size_t Index = 0>
struct Split_Impl;

template <auto String, char Delimiter, class Transformer>
struct Split_Impl<String, Delimiter, Transformer, size_t(-1)>
{
    static constexpr auto value = MakeTuple(Transformer::template Transform<String>());
};

template <auto String, char Delimiter, bool KeepDelimiter>
struct Split_ApplyDelimiter_Impl;

template <auto String, char Delimiter>
struct Split_ApplyDelimiter_Impl<String, Delimiter, true>
{
    static constexpr auto value = String.template Concat<StaticString { { Delimiter, '\0' } }>();
};

template <auto String, char Delimiter>
struct Split_ApplyDelimiter_Impl<String, Delimiter, false>
{
    static constexpr auto value = String;
};

/*! \brief Splits a StaticString by a delimiter.
 *
 *  \tparam String The StaticString to split.
 *  \tparam Delimiter The delimiter to split the StaticString by.
 *  \tparam Transformer The transformation to apply to each part.
 *
 *  \returns A tuple of StaticStrings, the result of splitting the StaticString by the delimiter. */
template <auto String, char Delimiter, class Transformer = BasicStaticStringTransformer>
struct Split;

template <auto String, char Delimiter, class Transformer, size_t Index>
struct Split_Impl
{
    static constexpr auto value = ConcatTuples(
        MakeTuple(Transformer::template Transform<Split_ApplyDelimiter_Impl<Substr<String, 0, Index>::value, Delimiter, Transformer::keepDelimiter>::value>()),
        MakeTuple(Split<Substr<String, Index + 1, String.Size()>::value, Delimiter, Transformer>::value));
};

template <auto String, char Delimiter, class Transformer>
struct Split
{
    static constexpr auto value = Split_Impl<String, Delimiter, Transformer, String.template FindFirst<containers::IntegerSequenceFromString<StaticString { { Delimiter, '\0' } }>>()>::value;
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

template <size_t FirstSize, size_t... OtherSizes>
struct ConcatStrings_Impl;

template <size_t FirstSize, size_t... OtherSizes>
struct ConcatStrings_Impl
{
    template <class SecondStaticStringType, size_t... Indices>
    constexpr auto Impl(StaticString<FirstSize> str0, SecondStaticStringType&& str1, std::index_sequence<Indices...>) const -> StaticString<FirstSize + SecondStaticStringType::size - 1 /* remove extra null terminator */>
    {
        return {
            { (Indices < (FirstSize - 1) ? str0.data[Indices] : str1.data[Indices - FirstSize + 1])... }
        };
    }

    constexpr auto operator()(StaticString<FirstSize> str0) const -> StaticString<FirstSize>
    {
        return str0;
    }

    template <class SecondStaticStringType>
    constexpr auto operator()(StaticString<FirstSize> str0, SecondStaticStringType&& str1) const -> StaticString<(FirstSize - 1) + (OtherSizes + ... + 0) - (sizeof...(OtherSizes)) + 1>
    {
        if constexpr (FirstSize <= 1 && SecondStaticStringType::size <= 1)
        {
            return StaticString<1> { { '\0' } };
        }
        else if constexpr (FirstSize <= 1)
        {
            return SecondStaticStringType { { str1.data } };
        }
        else if constexpr (SecondStaticStringType::size <= 1)
        {
            return StaticString<FirstSize> { { str0.data } };
        }
        else
        {
            return Impl(str0, std::forward<SecondStaticStringType>(str1), std::make_index_sequence<FirstSize + SecondStaticStringType::size - 1>());
        }
    }

    template <class SecondStaticStringType, class... OtherStaticStringTypes>
    constexpr auto operator()(StaticString<FirstSize> str0, SecondStaticStringType&& str1, OtherStaticStringTypes&&... other) const -> StaticString<(FirstSize - 1) + (OtherSizes + ... + 0) - (sizeof...(OtherSizes)) + 1>
    {
        if constexpr (FirstSize <= 1 && SecondStaticStringType::size <= 1)
        {
            return ConcatStrings_Impl<1, OtherStaticStringTypes::size...>()(StaticString<1> { { '\0' } }, std::forward<OtherStaticStringTypes>(other)...);
        }
        else if constexpr (FirstSize <= 1)
        {
            return ConcatStrings_Impl<SecondStaticStringType::size, OtherStaticStringTypes::size...>()(SecondStaticStringType { { str1.data } }, std::forward<OtherStaticStringTypes>(other)...);
        }
        else if constexpr (SecondStaticStringType::size <= 1)
        {
            return ConcatStrings_Impl<FirstSize, OtherStaticStringTypes::size...>()(StaticString<FirstSize> { { str0.data } }, std::forward<OtherStaticStringTypes>(other)...);
        }
        else
        {
            return ConcatStrings_Impl<FirstSize + SecondStaticStringType::size - 1, OtherStaticStringTypes::size...>()(Impl(str0, std::forward<SecondStaticStringType>(str1), std::make_index_sequence<FirstSize + SecondStaticStringType::size - 1>()), std::forward<OtherStaticStringTypes>(other)...);
        }
    }
};

template <class... StaticStringInstance>
constexpr auto ConcatStrings(StaticStringInstance&&... strings)
{
    return ConcatStrings_Impl<std::remove_cv_t<std::remove_reference_t<decltype(strings)>>::size...>()(std::forward<StaticStringInstance>(strings)...);
}

#pragma endregion Concat

#pragma region TransformParts

template <class Transformer, size_t FirstSize, size_t... OtherSizes>
struct TransformParts_Impl;

template <class Transformer, size_t FirstSize, size_t... OtherSizes>
struct TransformParts_Impl
{
    template <size_t... Indices>
    constexpr auto Impl(StaticString<FirstSize> str0, std::index_sequence<Indices...>) const
    {
        return str0;
    }

    template <class SecondStaticStringType, size_t... Indices>
    constexpr auto Impl(StaticString<FirstSize> str0, SecondStaticStringType&& str1, std::index_sequence<Indices...>) const
    {
        return ConcatStrings(str0, StaticString { { Transformer::delimiter, '\0' } }, std::forward<SecondStaticStringType>(str1));
    }

    constexpr auto operator()(StaticString<FirstSize> str0) const
    {
        return str0;
    }

    template <class SecondStaticStringType>
    constexpr auto operator()(StaticString<FirstSize> str0, SecondStaticStringType&& str1) const
    {
        if constexpr (FirstSize <= 1 && SecondStaticStringType::size <= 1)
        {
            return StaticString<1> { { '\0' } };
        }
        else if constexpr (FirstSize <= 1)
        {
            return SecondStaticStringType { { str1.data } };
        }
        else if constexpr (SecondStaticStringType::size <= 1)
        {
            return StaticString<FirstSize> { { str0.data } };
        }
        else
        {
            return Impl(str0, std::forward<SecondStaticStringType>(str1), std::make_index_sequence<FirstSize + SecondStaticStringType::size - 1>());
        }
    }

    template <class SecondStaticStringType, class... OtherStaticStringTypes>
    constexpr auto operator()(StaticString<FirstSize> str0, SecondStaticStringType&& str1, OtherStaticStringTypes&&... other) const
    {
        if constexpr (FirstSize <= 1 && SecondStaticStringType::size <= 1)
        {
            return TransformParts_Impl<Transformer, 1, OtherStaticStringTypes::size...>()(StaticString<1> { { '\0' } }, std::forward<OtherStaticStringTypes>(other)...);
        }
        else if constexpr (FirstSize <= 1)
        {
            return TransformParts_Impl<Transformer, SecondStaticStringType::size, OtherStaticStringTypes::size...>()(SecondStaticStringType { { str1.data } }, std::forward<OtherStaticStringTypes>(other)...);
        }
        else if constexpr (SecondStaticStringType::size <= 1)
        {
            return TransformParts_Impl<Transformer, FirstSize, OtherStaticStringTypes::size...>()(StaticString<FirstSize> { { str0.data } }, std::forward<OtherStaticStringTypes>(other)...);
        }
        else
        {
            return TransformParts_Impl<Transformer, FirstSize + SecondStaticStringType::size + 1 - 1, OtherStaticStringTypes::size...>()(Impl(str0, std::forward<SecondStaticStringType>(str1), std::make_index_sequence<FirstSize + SecondStaticStringType::size - 1>()), std::forward<OtherStaticStringTypes>(other)...);
        }
    }
};

/*! \brief Transforms a list of StaticStrings using a Transformer and concatenates them.
 *
 *  \tparam Transformer The transformer to apply to each StaticString. Must have a static constexpr char delimiter member, and a static constexpr auto Transform<auto String>() function.
 *  \tparam Strings The StaticStrings to transform.
 *
 *  \returns A StaticString, the result of transforming all the StaticStrings, and concatenating them. */
template <class Transformer, auto... Strings>
struct TransformParts
{
    static constexpr auto value = TransformParts_Impl<Transformer, Transformer::template Transform<Strings>().Size()...>()(Transformer::template Transform<Strings>()...);
};

#pragma endregion TransformParts

#pragma region FindCharCount

enum BalanceBracketsOptions : uint32
{
    BALANCE_BRACKETS_NONE = 0x0,
    BALANCE_BRACKETS_SQUARE = 0x1,
    BALANCE_BRACKETS_PARENTHESES = 0x2,
    BALANCE_BRACKETS_ANGLE = 0x4
};

template <auto String, char Delimiter, uint32 BracketOptions>
struct FindCharCount;

template <auto String, char Delimiter>
struct FindCharCount<String, Delimiter, BALANCE_BRACKETS_NONE>
{
    static constexpr size_t value = String.template Count<Delimiter>();
};

template <auto String, char Delimiter, uint32 BracketOptions>
struct FindCharCount_Impl
{
    constexpr size_t operator()() const
    {
        constexpr char Brackets[] = "[]()<>";

        size_t count = 0;

        int bracketCounts[(sizeof(Brackets) - 1) / 2] = { 0 };

        for (size_t i = 0; i < String.Size(); i++)
        {
            for (size_t j = 0; j < sizeof(Brackets) - 1; j++)
            {
                if (String.data[i] == Brackets[j])
                {
                    bracketCounts[j / 2] += (j % 2) ? -1 : 1;

                    break;
                }
            }

            if (String.data[i] == Delimiter)
            {
                if ((BracketOptions & BALANCE_BRACKETS_SQUARE) && bracketCounts[0] > 0)
                {
                    continue;
                }
                if ((BracketOptions & BALANCE_BRACKETS_PARENTHESES) && bracketCounts[1] > 0)
                {
                    continue;
                }
                if ((BracketOptions & BALANCE_BRACKETS_ANGLE) && bracketCounts[2] > 0)
                {
                    continue;
                }

                ++count;
            }
        }

        return count;
    }
};

template <auto String, char Delimiter, uint32 BracketOptions>
struct FindCharCount
{
    static constexpr size_t value = FindCharCount_Impl<String, Delimiter, BracketOptions> {}();
};

#pragma endregion FindCharCount

#pragma region GetSplitIndices

template <auto String, char Delimiter, uint32 BracketOptions, size_t Count>
struct GetSplitIndices_Impl
{
    constexpr auto operator()() const
    {
        return containers::MakePairSequence<Pair<size_t, size_t>>([]() -> std::array<Pair<size_t, size_t>, Count + 1>
            {
                std::array<Pair<size_t, size_t>, Count + 1> splitIndices = {};

                if constexpr (Count == 0)
                {
                    splitIndices[0] = Pair<size_t, size_t> { 0, String.Size() - 1 /* -1 for NUL char */ };
                }
                else
                {
                    size_t DelimiterIndices[Count] = {};

                    size_t index = 0;

                    constexpr char Brackets[] = "[]()<>";

                    int bracketCounts[(sizeof(Brackets) - 1) / 2] = { 0 };

                    for (size_t i = 0; i < String.size; i++)
                    {
                        for (size_t j = 0; j < sizeof(Brackets) - 1; j++)
                        {
                            if (String.data[i] == Brackets[j])
                            {
                                bracketCounts[j / 2] += (j % 2) ? -1 : 1;

                                break;
                            }
                        }

                        if (String.data[i] == Delimiter)
                        {
                            if ((BracketOptions & BALANCE_BRACKETS_SQUARE) && bracketCounts[0] > 0)
                            {
                                continue;
                            }
                            if ((BracketOptions & BALANCE_BRACKETS_PARENTHESES) && bracketCounts[1] > 0)
                            {
                                continue;
                            }
                            if ((BracketOptions & BALANCE_BRACKETS_ANGLE) && bracketCounts[2] > 0)
                            {
                                continue;
                            }
                        }

                        if (String.data[i] == Delimiter)
                        {
                            DelimiterIndices[index++] = i;
                        }
                    }

                    for (size_t i = 0; i < Count; i++)
                    {
                        size_t prev = i == 0 ? 0 : DelimiterIndices[i - 1] + 1;
                        size_t current = DelimiterIndices[i];

                        splitIndices[i] = { prev, current };
                    }

                    splitIndices[Count] = Pair<size_t, size_t> {
                        size_t(DelimiterIndices[Count - 1] + 1),
                        String.Size() - 1 /* -1 for NUL char */
                    };
                }

                return splitIndices;
            });
    }
};

/*! \brief Get the indices of the occurrences of a character in the StaticString.  */
template <auto String, char Delimiter, uint32 BracketOptions, size_t Count = size_t(-1)>
struct GetSplitIndices;

template <auto String, char Delimiter, uint32 BracketOptions>
struct GetSplitIndices<String, Delimiter, BracketOptions, size_t(-1)>
{
    static constexpr auto value = GetSplitIndices<String, Delimiter, BracketOptions, FindCharCount<String, Delimiter, BracketOptions>::value>::value;
};

template <auto String, char Delimiter, uint32 BracketOptions, size_t Count>
struct GetSplitIndices
{
    static constexpr auto value = GetSplitIndices_Impl<String, Delimiter, BracketOptions, Count> {}();
};

#pragma endregion GetSplitIndices

#pragma region TransformSplit

template <class Transformer, auto String, Pair<size_t, size_t>... SplitIndices>
constexpr auto TransformSplit_Impl(PairSequence<Pair<size_t, size_t>, SplitIndices...>)
{
    return TransformParts<Transformer, Substr<String, SplitIndices.first, SplitIndices.second>::value...>::value;
}

/*! \brief Splits a StaticString by delimiter, applies a transformation to each element, and rejoins it into a single StaticString.
 *
 *  \tparam Transformer The transformer to apply to each element in the split StaticString. Must have a static constexpr char delimiter member, and a static constexpr auto Transform<auto String>() function.
 *  \tparam Strings The StaticStrings to transform.
 *
 *  \returns A StaticString, the result of concatenating each of the transformed StaticStrings after splitting the input by a delimtier. */
template <class Transformer, auto String>
struct TransformSplit
{
    static constexpr auto value = TransformSplit_Impl<Transformer, String>(GetSplitIndices<String, Transformer::delimiter, Transformer::balanceBracketOptions>::value);
};

#pragma endregion TransformSplit

#pragma region ParseInteger

template <auto String, size_t CharIndex = 0, int Value = 0>
struct ParseInteger;

template <auto String, size_t CharIndex>
struct ParseInteger_ParseChar_Impl
{
    static constexpr auto value = String.data[CharIndex] - '0';
};

template <auto String, size_t CharIndex, int Value, bool AtEnd>
struct ParseInteger_Impl;

template <auto String, size_t CharIndex, int Value>
struct ParseInteger_Impl<String, CharIndex, Value, false>
{
    static constexpr auto value = ParseInteger<String, CharIndex + 1, Value * 10 + ParseInteger_ParseChar_Impl<String, CharIndex>::value>::value;
};

template <auto String, size_t CharIndex, int Value>
struct ParseInteger_Impl<String, CharIndex, Value, true>
{
    static constexpr auto value = Value;
};

template <auto String, size_t CharIndex, int Value>
struct ParseInteger
{
    static constexpr auto value = ParseInteger_Impl < String, CharIndex, Value, CharIndex >= String.size - 1 || String.data[CharIndex] == '\0' > ::value;
};

#pragma endregion ParseInteger

#pragma region MakeStaticString

template <class StringType>
constexpr auto MakeStaticString(StringType strArg)
{
    return StaticString(HYP_GET_CONST_ARG(strArg));
}

#define HYP_MAKE_CONST_ARG_STR(sz, str) \
    [=] {                               \
        return StaticString<(sz)>(str); \
    }

#pragma endregion MakeStaticString

} // namespace helpers

} // namespace containers

using containers::StaticString;

} // namespace Hyperion

#define HYP_STATIC_STRING(text) \
    ::Hyperion::StaticString<sizeof(text)>(text)
