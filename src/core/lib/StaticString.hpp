#ifndef HYPERION_STATIC_STRING_H
#define HYPERION_STATIC_STRING_H

#include <Types.hpp>

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
    char data[Sz];

    constexpr StaticString(const char (&str)[Sz])
    {
        for (SizeType i = 0; i < Sz; ++i) {
            data[i] = str[i];
        }
    }

    template <SizeType Start, SizeType End>
    constexpr StaticString<End - Start> Substr() const
    {
        static_assert(Start < End, "Start must be less than End");
        static_assert(End <= Sz, "End must be less than or equal to Size");

        // return [this]<SizeType ... Indices>(std::index_sequence<Indices...>)
        // {
        //     return StaticString<End - Start> { { data[Indices + Start]... } };
        // }(detail::make_offset_index_sequence_t<End - Start, Start> { });

        return MakeSubString(detail::make_offset_index_sequence_t<End - Start, Start> { });
    }

    constexpr SizeType Size() const
        { return Sz; }

    template <SizeType ... Indices>
    constexpr StaticString<sizeof...(Indices)> MakeSubString(std::index_sequence<Indices...>) const
    {
        return { { data[Indices]... } };
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
};

template <auto StaticString>
using IntegerSequenceFromStringType = typename IntegerSequenceFromString<StaticString>::Type;

} // namespace hyperion

#endif