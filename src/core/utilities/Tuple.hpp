#ifndef HYPERION_TUPLE_HPP
#define HYPERION_TUPLE_HPP

#include <Constants.hpp>
#include <Types.hpp>

namespace hyperion {
namespace utilities {

template <class... Types>
class Tuple;

// see: https://mitchnull.blogspot.com/2012/06/c11-tuple-implementation-details-part-1.html?m=1

#pragma region TupleIndices

template <SizeType... Index>
struct TupleIndices
{
};

template <SizeType Start, class TupleIndicesType, SizeType End>
struct MakeTupleIndices_Impl;

template <SizeType Start, SizeType... Indices, SizeType End>
struct MakeTupleIndices_Impl<Start, TupleIndices<Indices...>, End>
{
    using Type = typename MakeTupleIndices_Impl<Start + 1, TupleIndices<Indices..., Start>, End>::Type;
};

template <SizeType End, SizeType... Indices>
struct MakeTupleIndices_Impl<End, TupleIndices<Indices...>, End>
{
    using Type = TupleIndices<Indices...>;
};

template <SizeType End, SizeType Start = 0>
struct MakeTupleIndices
{
    static_assert(Start <= End);

    using Type = typename MakeTupleIndices_Impl<Start, TupleIndices<>, End>::Type;
};

#pragma endregion TupleIndices

#pragma region TupleSize

template <class TupleType>
struct TupleSize;

template <class... Types>
struct TupleSize<Tuple<Types...>>
{
    static constexpr SizeType value = sizeof...(Types);
};

#pragma endregion TupleSize

#pragma region TupleLeaf

template <SizeType Index, class T>
struct TupleLeaf
{
    using ElementType = T;

    ElementType value;

    constexpr TupleLeaf() = default;

    // vvvvv Fix for rvalue refs vvvvv
    template <class T_>
    constexpr TupleLeaf(T_&& value)
        : value(std::forward<T_>(value))
    {
    }

    template <SizeType Index_, class T_>
    constexpr TupleLeaf(const TupleLeaf<Index_, T_>& other)
        : value(other.value)
    {
    }

    template <SizeType Index_, class T_>
    TupleLeaf& operator=(const TupleLeaf<Index_, T_>& other)
    {
        value = other.value;

        return *this;
    }

    template <SizeType Index_, class T_>
    constexpr TupleLeaf(TupleLeaf<Index_, T_>&& other) noexcept
        : value(std::move(other.value))
    {
    }

    template <SizeType Index_, class T_>
    TupleLeaf& operator=(TupleLeaf<Index_, T_>&& other) noexcept
    {
        value = std::move(other.value);

        return *this;
    }

    ~TupleLeaf() = default;

    HYP_FORCE_INLINE constexpr bool operator==(const TupleLeaf& other) const
    {
        return value == other.value;
    }

    HYP_FORCE_INLINE constexpr bool operator!=(const TupleLeaf& other) const
    {
        return value != other.value;
    }
};

// Specialization to hold references in a TupleLeaf
template <SizeType Index, class T>
struct TupleLeaf<Index, T&>
{
    using ElementType = T&;

    ElementType& value;

    constexpr TupleLeaf() = delete;

    template <class T_>
    constexpr TupleLeaf(T_&& value)
        : value(value)
    {
    }

    template <SizeType Index_, class T_>
    constexpr TupleLeaf(const TupleLeaf<Index_, T_>& other)
        : value(other.value)
    {
    }

    template <SizeType Index_, class T_>
    TupleLeaf& operator=(const TupleLeaf<Index_, T_>& other)
    {
        value = other.value;

        return *this;
    }

    template <SizeType Index_, class T_>
    constexpr TupleLeaf(TupleLeaf<Index_, T_>&& other) noexcept
        : value(other.value)
    {
    }

    template <SizeType Index_, class T_>
    TupleLeaf& operator=(TupleLeaf<Index_, T_>&& other) noexcept
    {
        value = other.value;

        return *this;
    }

    ~TupleLeaf() = default;

    HYP_FORCE_INLINE constexpr bool operator==(const TupleLeaf& other) const
    {
        return value == other.value;
    }

    HYP_FORCE_INLINE constexpr bool operator!=(const TupleLeaf& other) const
    {
        return value != other.value;
    }
};

#pragma endregion TupleLeaf

#pragma region TupleElement Helpers

template <SizeType Index, SizeType Depth, class FirstType, class... Types>
struct TupleElement_SelectType_Impl;

template <SizeType Depth, class FirstType, class... Types>
struct TupleElement_SelectType_Impl<Depth, Depth, FirstType, Types...>
{
    using Type = FirstType;
};

template <SizeType Index, SizeType Depth, class FirstType, class... Types>
struct TupleElement_SelectType_Impl
{
    using Type = typename TupleElement_SelectType_Impl<Index, Depth + 1, Types...>::Type;
};

#pragma endregion TupleElement Helpers

#pragma region TupleElement

template <SizeType Index, class... Types>
struct TupleElement
{
    using Type = typename TupleElement_SelectType_Impl<Index, 0, Types...>::Type;
};

#pragma endregion TupleElement

#pragma region FindTypeElementIndex

template <class T, SizeType Depth, class FirstType, class... Types>
struct FindTypeElementIndex_Impl;

template <class T, SizeType Depth, class... Types>
struct FindTypeElementIndex_Impl<T, Depth, T, Types...>
{
    static constexpr SizeType value = Depth;
};

template <class T, SizeType Depth, class FirstType, class... Types>
struct FindTypeElementIndex_Impl
{
    static constexpr SizeType value = FindTypeElementIndex_Impl<T, Depth + 1, Types...>::value;
};

template <class T, class... Types>
struct FindTypeElementIndex
{
    static constexpr SizeType value = FindTypeElementIndex_Impl<T, 0, Types...>::value;
};

#pragma endregion FindTypeElementIndex

#pragma region FindTypeElementIndex_Tuple

template <class T, class Tup>
struct FindTypeElementIndex_Tuple;

template <class T, class... Types>
struct FindTypeElementIndex_Tuple<T, Tuple<Types...>>
{
    static constexpr SizeType value = FindTypeElementIndex_Impl<T, 0, Types...>::value;
};

#pragma endregion FindTypeElementIndex_Tuple

#pragma region Tuple_Compare

// fwd decl of Tuple_Impl
template <class TupleIndicesType, class... Types>
struct Tuple_Impl;

template <class Comparator, SizeType Index, class... Types>
constexpr bool Tuple_Compare(
    const Tuple_Impl<typename MakeTupleIndices<sizeof...(Types)>::Type, Types...>& tup0,
    const Tuple_Impl<typename MakeTupleIndices<sizeof...(Types)>::Type, Types...>& tup1);

template <class Comparator, SizeType Index, class... Types>
constexpr bool Tuple_Compare(
    const Tuple_Impl<typename MakeTupleIndices<sizeof...(Types)>::Type, Types...>& tup0,
    const Tuple_Impl<typename MakeTupleIndices<sizeof...(Types)>::Type, Types...>& tup1)
{
    if constexpr (Index == sizeof...(Types))
    {
        return false;
    }
    else
    {
        const auto& lhs = static_cast<const TupleLeaf<Index, typename TupleElement<Index, Types...>::Type>&>(tup0).value;
        const auto& rhs = static_cast<const TupleLeaf<Index, typename TupleElement<Index, Types...>::Type>&>(tup1).value;

        if (Comparator {}(lhs, rhs))
        {
            return true;
        }

        if (Comparator {}(rhs, lhs))
        {
            return false;
        }

        return Tuple_Compare<Comparator, Index + 1, Types...>(tup0, tup1);
    }
}

#pragma endregion Tuple_Compare

#pragma region Tuple_Impl

template <class TupleIndicesType, class... Types>
struct Tuple_Impl;

template <SizeType... Indices, class... Types>
struct Tuple_Impl<TupleIndices<Indices...>, Types...> : TupleLeaf<Indices, Types>...
{
    constexpr Tuple_Impl() = default;

    template <SizeType... Indices_, class... Types_>
    constexpr Tuple_Impl(std::integer_sequence<SizeType, Indices_...>, Types_&&... values)
        : TupleLeaf<Indices, Types>(std::forward<Types_>(values))...
    {
    }

    template <SizeType... Indices_, class... Types_>
    constexpr Tuple_Impl(const Tuple_Impl<TupleIndices<Indices_...>, Types_...>& other)
        : TupleLeaf<Indices, Types>(static_cast<const TupleLeaf<Indices_, Types_>&>(other))...
    {
    }

    template <SizeType... Indices_, class... Types_>
    Tuple_Impl& operator=(const Tuple_Impl<TupleIndices<Indices_...>, Types_...>& other)
    {
        (TupleLeaf<Indices, Types>::operator=(static_cast<const TupleLeaf<Indices_, Types_>&>(other)), ...);

        return *this;
    }

    template <SizeType... Indices_, class... Types_>
    constexpr Tuple_Impl(Tuple_Impl<TupleIndices<Indices_...>, Types_...>&& other) noexcept
        : TupleLeaf<Indices, Types>(static_cast<TupleLeaf<Indices_, Types_>&&>(std::move(other)))...
    {
    }

    template <SizeType... Indices_, class... Types_>
    Tuple_Impl& operator=(Tuple_Impl<TupleIndices<Indices_...>, Types_...>&& other) noexcept
    {
        (TupleLeaf<Indices, Types>::operator=(static_cast<TupleLeaf<Indices_, Types_>&&>(std::move(other))), ...);

        return *this;
    }

    ~Tuple_Impl() = default;

    template <SizeType... Indices_, class... Types_>
    HYP_FORCE_INLINE constexpr bool operator==(const Tuple_Impl<TupleIndices<Indices_...>, Types_...>& other) const
    {
        return (TupleLeaf<Indices, Types>::operator==(static_cast<const TupleLeaf<Indices_, Types_>&>(other)) && ...);
    }

    template <SizeType... Indices_, class... Types_>
    HYP_FORCE_INLINE constexpr bool operator!=(const Tuple_Impl<TupleIndices<Indices_...>, Types_...>& other) const
    {
        return (TupleLeaf<Indices, Types>::operator!=(static_cast<const TupleLeaf<Indices_, Types_>&>(other)) || ...);
    }

    HYP_FORCE_INLINE constexpr SizeType Size() const
    {
        return sizeof...(Types);
    }
};

#pragma endregion Tuple_Impl

namespace helpers {

#pragma region ConcatTuples

template <class... FirstTypes, SizeType... FirstIndices, class... SecondTypes, SizeType... SecondIndices>
constexpr auto ConcatTuples_Impl(
    const Tuple<FirstTypes...>& first_tuple,
    hyperion::utilities::TupleIndices<FirstIndices...>,
    const Tuple<SecondTypes...>& second_tuple,
    hyperion::utilities::TupleIndices<SecondIndices...>)
{
    return Tuple<FirstTypes..., SecondTypes...> {
        first_tuple.template GetElement<FirstIndices>()...,
        second_tuple.template GetElement<SecondIndices>()...
    };
}

template <class... FirstTupleTypes, class... SecondTupleTypes>
constexpr auto ConcatTuples(const Tuple<FirstTupleTypes...>& first, const Tuple<SecondTupleTypes...>& second) -> Tuple<FirstTupleTypes..., SecondTupleTypes...>
{
    return ConcatTuples_Impl(
        first,
        typename hyperion::utilities::MakeTupleIndices<sizeof...(FirstTupleTypes)>::Type(),
        second,
        typename hyperion::utilities::MakeTupleIndices<sizeof...(SecondTupleTypes)>::Type());
}

#pragma endregion ConcatTuples

#pragma region MakeTuple

template <class... Types>
constexpr Tuple<NormalizedType<Types>...> MakeTuple(Types&&... values)
{
    return Tuple<NormalizedType<Types>...>(std::forward<Types>(values)...);
}

#pragma endregion MakeTuple

#pragma region Tie

template <class... Types>
constexpr Tuple<Types&...> Tie(Types&... values)
{
    return Tuple<Types&...>(values...);
}

#pragma endregion Tie

#pragma region ForwardAsTuple

template <class... Types>
constexpr Tuple<Types&&...> ForwardAsTuple(Types&&... values)
{
    return Tuple<Types&&...>(std::forward<Types>(values)...);
}

#pragma endregion ForwardAsTuple

#pragma region Apply

template <class FunctionType, class... Types, SizeType... Indices>
constexpr auto Apply_Impl(FunctionType&& function, Tuple<Types...>& args, hyperion::utilities::TupleIndices<Indices...>)
{
    return function(args.template GetElement<Indices>()...);
}

template <class FunctionType, class... Types, SizeType... Indices>
constexpr auto Apply_Impl(FunctionType&& function, const Tuple<Types...>& args, hyperion::utilities::TupleIndices<Indices...>)
{
    return function(args.template GetElement<Indices>()...);
}

template <class FunctionType, class... Types, SizeType... Indices>
constexpr auto Apply_Impl(FunctionType&& function, Tuple<Types...>&& args, hyperion::utilities::TupleIndices<Indices...>)
{
    return function(args.template GetElement<Indices>()...);
}

template <class FunctionType, class TupleType>
constexpr auto Apply(FunctionType&& function, TupleType&& args_tuple)
{
    return Apply_Impl(
        std::forward<FunctionType>(function),
        std::forward<TupleType>(args_tuple),
        typename NormalizedType<TupleType>::Indices {});
}

#pragma endregion Apply

} // namespace helpers

template <std::size_t Index, class... Types>
constexpr typename TupleElement<Index, Types...>::Type& get(Tuple<Types...>& tup) noexcept
{
    return static_cast<TupleLeaf<Index, typename TupleElement<Index, Types...>::Type>&>(tup.m_impl).value;
}

template <std::size_t Index, class... Types>
constexpr const typename TupleElement<Index, Types...>::Type& get(const Tuple<Types...>& tup) noexcept
{
    return static_cast<const TupleLeaf<Index, typename TupleElement<Index, Types...>::Type>&>(tup.m_impl).value;
}

template <std::size_t Index, class... Types>
constexpr typename TupleElement<Index, Types...>::Type&& get(Tuple<Types...>&& tup) noexcept
{
    return static_cast<TupleLeaf<Index, typename TupleElement<Index, Types...>::Type>&&>(tup.m_impl).value;
}

template <std::size_t Index, class... Types>
constexpr const typename TupleElement<Index, Types...>::Type&& get(const Tuple<Types...>&& tup) noexcept
{
    return static_cast<const TupleLeaf<Index, typename TupleElement<Index, Types...>::Type>&&>(tup.m_impl).value;
}

template <class... Types>
class Tuple
{
    template <std::size_t Index_, class... Types_>
    friend constexpr typename TupleElement<Index_, Types_...>::Type& get(Tuple<Types_...>&) noexcept;

    template <std::size_t Index_, class... Types_>
    friend constexpr const typename TupleElement<Index_, Types_...>::Type& get(const Tuple<Types_...>&) noexcept;

    template <std::size_t Index_, class... Types_>
    friend constexpr typename TupleElement<Index_, Types_...>::Type&& get(Tuple<Types_...>&&) noexcept;

    template <std::size_t Index_, class... Types_>
    friend constexpr const typename TupleElement<Index_, Types_...>::Type&& get(const Tuple<Types_...>&&) noexcept;

    Tuple_Impl<typename MakeTupleIndices<sizeof...(Types)>::Type, Types...> m_impl;

public:
    static constexpr SizeType size = sizeof...(Types);

    using Indices = typename hyperion::utilities::MakeTupleIndices<sizeof...(Types)>::Type;

    constexpr Tuple() = default;

    template <class... Types_>
    constexpr Tuple(Types_&&... values)
        : m_impl(std::index_sequence_for<Types_...>(), std::forward<Types_>(values)...)
    {
        static_assert(sizeof...(Types_) == sizeof...(Types), "Incorrect number of tuple args provided!");
    }

    constexpr Tuple(const Tuple& other)
        : m_impl(other.m_impl)
    {
    }

    HYP_FORCE_INLINE Tuple& operator=(const Tuple& other)
    {
        m_impl = other.m_impl;

        return *this;
    }

    constexpr Tuple(Tuple&& other) noexcept
        : m_impl(std::move(other.m_impl))
    {
    }

    HYP_FORCE_INLINE Tuple& operator=(Tuple&& other) noexcept
    {
        m_impl = std::move(other.m_impl);

        return *this;
    }

    ~Tuple() = default;

    HYP_FORCE_INLINE constexpr bool operator==(const Tuple& other) const
    {
        return m_impl == other.m_impl;
    }

    HYP_FORCE_INLINE constexpr bool operator!=(const Tuple& other) const
    {
        return m_impl != other.m_impl;
    }

    HYP_FORCE_INLINE constexpr bool operator<(const Tuple& other) const
    {
        return Tuple_Compare<std::less<>, 0>(m_impl, other.m_impl);
    }

    HYP_FORCE_INLINE constexpr bool operator<=(const Tuple& other) const
    {
        return Tuple_Compare<std::less_equal<>, 0>(m_impl, other.m_impl);
    }

    HYP_FORCE_INLINE constexpr bool operator>(const Tuple& other) const
    {
        return Tuple_Compare<std::greater<>, 0>(m_impl, other.m_impl);
    }

    HYP_FORCE_INLINE constexpr bool operator>=(const Tuple& other) const
    {
        return Tuple_Compare<std::greater_equal<>, 0>(m_impl, other.m_impl);
    }

    template <SizeType Index>
    constexpr typename TupleElement<Index, Types...>::Type& GetElement()
    {
        static_assert(Index < sizeof...(Types), "Index out of range of tuple");

        return static_cast<TupleLeaf<Index, typename TupleElement<Index, Types...>::Type>&>(m_impl).value;
    }

    template <SizeType Index>
    constexpr const typename TupleElement<Index, Types...>::Type& GetElement() const
    {
        static_assert(Index < sizeof...(Types), "Index out of range of tuple");

        return static_cast<const TupleLeaf<Index, typename TupleElement<Index, Types...>::Type>&>(m_impl).value;
    }

    template <class T>
    constexpr T& GetElement()
    {
        constexpr SizeType index = FindTypeElementIndex<T, Types...>::value;

        if constexpr (index == SizeType(-1))
        {
            static_assert(resolution_failure<T>, "Tuple does not contain element of given type");

            // to silence errors for wrong type being returned
            return *((T*)(nullptr));
        }
        else
        {
            return static_cast<TupleLeaf<index, T>&>(m_impl).value;
        }
    }

    template <class T>
    constexpr const T& GetElement() const
    {
        constexpr SizeType index = FindTypeElementIndex<T, Types...>::value;

        if constexpr (index == SizeType(-1))
        {
            static_assert(resolution_failure<T>, "Tuple does not contain element of given type");

            // to silence errors for wrong type being returned
            return *((const T*)(nullptr));
        }
        else
        {
            return static_cast<const TupleLeaf<index, T>&>(m_impl).value;
        }
    }

    HYP_FORCE_INLINE constexpr SizeType Size() const
    {
        return sizeof...(Types);
    }
};

} // namespace utilities

using utilities::Tuple;
using utilities::TupleElement;
using utilities::helpers::Apply;
using utilities::helpers::ConcatTuples;
using utilities::helpers::ForwardAsTuple;
using utilities::helpers::MakeTuple;
using utilities::helpers::Tie;

} // namespace hyperion

// std::tuple-like behavior
namespace std {

template <class... Types>
struct tuple_size<hyperion::utilities::Tuple<Types...>>
{
    static constexpr size_t value = sizeof...(Types);
};

template <size_t Index, class... Types>
struct tuple_element<Index, hyperion::utilities::Tuple<Types...>>
{
    using type = typename hyperion::utilities::TupleElement<Index, Types...>::Type;
};

} // namespace std

#endif