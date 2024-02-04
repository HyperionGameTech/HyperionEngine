#ifndef HYPERION_STATIC_STRING_H
#define HYPERION_STATIC_STRING_H

#include <Types.hpp>
#include <HashCode.hpp>

#include <utility>
#include <string_view>

namespace hyperion {
    
namespace detail {

template <SizeType N, class F, SizeType... Indices>
constexpr auto make_seq_helper(F f, std::index_sequence<Indices...>)
{
    return std::integer_sequence<char, (f()[Indices])...>{};
}

template <class F>
constexpr auto make_seq(F f)
{
    constexpr SizeType size = f().size();
    using SequenceType = std::make_index_sequence<size>;

    return make_seq_helper<size>(f, SequenceType { });
}

template <SizeType Offset, SizeType ... Indices>
constexpr std::index_sequence<(Offset + Indices)...> make_offset_index_sequence(std::index_sequence<Indices...>)
{
    return {};
}

template <SizeType N, SizeType Offset>
using make_offset_index_sequence_t = decltype(make_offset_index_sequence<Offset>(std::make_index_sequence<N>{}));

} // namespace detail

template <SizeType Sz>
struct StaticString
{
    static constexpr SizeType size = Sz;

    char data[Sz];

    constexpr StaticString(const char (&str)[Sz])
    {
        for (SizeType i = 0; i < Sz; ++i) {
            data[i] = str[i];
        }
    }

    template <typename IntegerSequence, Int Index = 0>
    constexpr SizeType FindFirst() const
    {
        constexpr auto other_size = IntegerSequence::Size() - 1; // -1 to account for null terminator

        if constexpr (Index > Sz - other_size) {
            return -1;
        } else {
            Bool found = true;

            for (SizeType j = 0; j < other_size && j < Sz; ++j) {
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

    template <typename IntegerSequence, Int Index = Int(Sz) - Int(IntegerSequence::Size())>
    constexpr SizeType FindLast() const
    {
        constexpr auto other_size = IntegerSequence::Size() - 1; // -1 to account for null terminator

        if constexpr (Index < 0) {
            return -1;
        } else {
            Bool found = true;

            for (SizeType j = 0; j < other_size && j < Sz; ++j) {
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

    template <SizeType Start, SizeType End>
    constexpr StaticString<End - Start + 1> Substr() const
    {
        static_assert(Start <= End, "Start must be less or equal to End");
        static_assert(End <= Sz, "End must be less than or equal to Size");

        if constexpr (Start == End) {
            return StaticString<1> { { '\0' } };
        } else {
            return MakeSubString(detail::make_offset_index_sequence_t<End - Start, Start> { });
        }
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
            return ConcatImpl<OtherStaticString>(std::make_index_sequence<Sz + decltype(OtherStaticString)::size - 1> { });
        }
    }

    constexpr const char *Data() const
        { return &data[0]; }

    constexpr SizeType Size() const
        { return Sz; }

    template <SizeType ... Indices>
    constexpr StaticString<sizeof...(Indices) + 1> MakeSubString(std::index_sequence<Indices...>) const
    {
        return { { data[Indices]..., '\0' } };
    }

    template <auto OtherStaticString, SizeType ... Indices>
    constexpr auto ConcatImpl(std::index_sequence<Indices...>) const -> StaticString<Sz + decltype(OtherStaticString)::size - 1 /* remove extra null terminator */>
    {
        return {
            {
                (Indices < (Sz - 1) ? data[Indices] : OtherStaticString.data[Indices - Sz + 1])...
            }
        };
    }

    constexpr HashCode GetHashCode() const
    {
        return HashCode::GetHashCode(&data[0]);
    }
};

template <auto StaticString>
struct IntegerSequenceFromString
{
private:
    constexpr static auto value = detail::make_seq([] { return std::string_view { StaticString.data }; });

public:
    using Type = decltype(value);

    static constexpr const char *Data()
        { return &StaticString.data[0]; }

    static constexpr SizeType Size()
        { return StaticString.size; }
};

template <auto StaticString>
using IntegerSequenceFromStringType = typename IntegerSequenceFromString<StaticString>::Type;

} // namespace hyperion

#endif