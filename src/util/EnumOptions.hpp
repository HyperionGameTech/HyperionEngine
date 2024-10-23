/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#ifndef ENUM_OPTIONS_HPP
#define ENUM_OPTIONS_HPP

#include <core/containers/FixedArray.hpp>
#include <core/containers/Array.hpp>
#include <core/utilities/Pair.hpp>
#include <HashCode.hpp>
#include <math/MathUtil.hpp>

#include <Types.hpp>

#include <initializer_list>
#include <limits>

namespace hyperion {

namespace containers {
namespace detail {

// convert from attachment (2^x) into ordinal (0-5) for use as an array index
template <class EnumType, class OrdinalType = std::underlying_type_t<EnumType>>
constexpr OrdinalType EnumToOrdinal(EnumType option)
    { return OrdinalType(MathUtil::FastLog2_Pow2(uint64(option))); }

// convert from ordinal (0-5) into power-of-two for use as bit flags
template <class EnumType, class OrdinalType = std::underlying_type_t<EnumType>>
constexpr EnumType OrdinalToEnum(OrdinalType ordinal)
    { return EnumType(1ull << ordinal); }

template <class ContainerType, typename EnumType, typename ValueType, SizeType Sz>
struct EnumMapIterator
{
    using OrdinalType = std::underlying_type_t<EnumType>;

    ContainerType   &ref;
    SizeType        index;

    EnumMapIterator() = delete;

    EnumMapIterator(ContainerType &ref, SizeType index)
        : ref(ref), index(index)
    {
    }

    EnumMapIterator(const EnumMapIterator &other)
        : ref(other.ref),
          index(other.index)
    {
    }

    EnumMapIterator &operator=(const EnumMapIterator &other)
    {
        ref = other.ref;
        index = other.index;

        return *this;
    }

    EnumMapIterator(EnumMapIterator &&other) noexcept
        : ref(other.ref), index(other.index)
    {
    }

    EnumMapIterator &operator=(EnumMapIterator &&other) noexcept
    {
        ref = other.ref;
        index = other.index;

        return *this;
    }

    ~EnumMapIterator() = default;

    HYP_FORCE_INLINE EnumMapIterator &operator++()
    {
        ++index;

        return *this;
    }

    HYP_FORCE_INLINE EnumMapIterator operator++(int)
    {
        EnumMapIterator temp { *this };

        ++index;

        return temp;
    }

    HYP_FORCE_INLINE bool operator==(const EnumMapIterator &other) const
    {
        return index == other.index
            && std::addressof(ref) == std::addressof(other.ref);
    }

    HYP_FORCE_INLINE bool operator!=(const EnumMapIterator &other) const
        { return !(*this == other); }

    HYP_FORCE_INLINE Pair<EnumType, ValueType &> operator*()
        { return { OrdinalToEnum<EnumType, OrdinalType>(OrdinalType(index)), ref[index] }; }

    HYP_FORCE_INLINE Pair<EnumType, const ValueType &> operator*() const
        { return { OrdinalToEnum<EnumType, OrdinalType>(OrdinalType(index)), ref[index] }; }

    HYP_FORCE_INLINE ValueType *operator->() const
        { return &ref[index]; }
};

} // namespace detail
} // namespace containers

template <typename EnumType, typename ValueType, SizeType Sz>
class EnumOptions : public FixedArray<ValueType, Sz>
{
public:
    using Base = FixedArray<ValueType, Sz>;

    using OrdinalType = std::underlying_type_t<EnumType>;
    using KeyValuePairType  = KeyValuePair<EnumType, ValueType>;

    using Iterator = containers::detail::EnumMapIterator<Base, EnumType, ValueType, Sz>;
    using ConstIterator = containers::detail::EnumMapIterator<const Base, EnumType, const ValueType, Sz>;

    static constexpr OrdinalType max_value = MathUtil::MaxSafeValue<EnumType>();

    // convert from attachment (2^x) into ordinal (0-5) for use as an array index
    static constexpr OrdinalType EnumToOrdinal(EnumType value)
        { return containers::detail::EnumToOrdinal<EnumType, OrdinalType>(value); }

    // convert from ordinal (0-5) into power-of-two for use as bit flags
    static constexpr EnumType OrdinalToEnum(OrdinalType ordinal)
        { return containers::detail::OrdinalToEnum<EnumType, OrdinalType>(ordinal); }

    static_assert(Sz != 0, "EnumOptions cannot have size of zero");
    static_assert(
        OrdinalType(OrdinalToEnum(Sz - 1)) < max_value,
        "Size too large; enum conversion would cause overflow. "
        "Try changing the enum's underlying type to a larger sized data type?"
    );

    EnumOptions() : Base { } {}

    EnumOptions(std::initializer_list<KeyValuePairType> initializer_list)
        : Base { }
    {
        for (const auto &item : initializer_list) {
            Set(item.first, item.second);
        }
    }

    EnumOptions(const EnumOptions &other)
        : Base(static_cast<const Base &>(other))
    {
    }

    EnumOptions &operator=(const EnumOptions &other)
    {
        Base::operator=(static_cast<const Base &>(other));

        return *this;
    }

    EnumOptions(EnumOptions &&other)
        : Base(static_cast<Base &&>(other))
    {
    }

    EnumOptions &operator=(EnumOptions &&other) noexcept
    {
        Base::operator=(static_cast<Base &&>(other));

        return *this;
    }

    ~EnumOptions() = default;

    constexpr KeyValuePairType KeyValueAt(SizeType index) const
        { return KeyValuePairType { EnumType(OrdinalToEnum(index)), Base::m_values[index] }; }

    constexpr EnumType KeyAt(SizeType index) const
        { return EnumType(OrdinalToEnum(index)); }

    constexpr ValueType &ValueAt(SizeType index)
        { return Base::operator[](index); }

    constexpr const ValueType &ValueAt(SizeType index) const
        { return Base::operator[](index); }

    constexpr ValueType &Get(EnumType enum_key)
        { return Base::m_values[EnumToOrdinal(enum_key)]; }

    constexpr const ValueType &Get(EnumType enum_key) const
        { return Base::m_values[EnumToOrdinal(enum_key)]; }

    constexpr ValueType &operator[](EnumType enum_key)
        { return Base::m_values[EnumToOrdinal(enum_key)]; }

    constexpr const ValueType &operator[](EnumType enum_key) const
        { return Base::m_values[EnumToOrdinal(enum_key)]; }

    EnumOptions &Set(EnumType enum_key, ValueType &&value)
    {
        OrdinalType ord = EnumToOrdinal(enum_key);
        AssertThrow(ord < Sz);

        Base::m_values[ord] = std::move(value);

        return *this;
    }

    EnumOptions &Set(EnumType enum_key, const ValueType &value)
    {
        OrdinalType ord = EnumToOrdinal(enum_key);
        AssertThrow(ord < Size());

        Base::m_values[ord] = value;

        return *this;
    }

    EnumOptions &Unset(EnumType enum_key)
    {
        OrdinalType ord = EnumToOrdinal(enum_key);
        AssertThrow(ord < Sz);

        Base::m_values[ord] = { };

        return *this;
    }

    constexpr SizeType Size() const
        { return Base::Size(); }

    ValueType *Data()
        { return Base::Data(); }

    const ValueType *Data() const
        { return Base::Data(); }

    void Clear()
    {
        for (auto &value : Base::m_values) {
            value = ValueType { };
        }
    }
    
    // HYP_DEF_STL_BEGIN_END(
    //     Iterator(static_cast<std::conditional_t<std::is_const_v<std::remove_reference_t<decltype(*this)>>, const Base &, Base &>>(*this), 0),
    //     Iterator(static_cast<std::conditional_t<std::is_const_v<std::remove_reference_t<decltype(*this)>>, const Base &, Base &>>(*this), Size())
    // )

    Iterator Begin()
        { return Iterator(static_cast<Base &>(*this), 0); }

    Iterator End()
        { return Iterator(static_cast<Base &>(*this), Size()); }

    Iterator begin()
        { return Begin(); }

    Iterator end()
        { return End(); }

    ConstIterator Begin() const
        { return ConstIterator(static_cast<const Base &>(*this), 0); }

    ConstIterator End() const
        { return ConstIterator(static_cast<const Base &>(*this), Size()); }

    ConstIterator begin() const
        { return Begin(); }

    ConstIterator end() const
        { return End(); }
};

} // namespace hyperion

#endif