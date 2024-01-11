#ifndef ENUM_OPTIONS_H
#define ENUM_OPTIONS_H

#include <core/lib/FixedArray.hpp>
#include <core/lib/DynArray.hpp>
#include <core/lib/Pair.hpp>
#include <HashCode.hpp>
#include <math/MathUtil.hpp>

#include <Types.hpp>

#include <initializer_list>

namespace hyperion {

namespace containers {
namespace detail {

// convert from attachment (2^x) into ordinal (0-5) for use as an array index
constexpr UInt64 EnumToOrdinal(UInt64 option)
    { return MathUtil::FastLog2_Pow2(option); }

// convert from ordinal (0-5) into power-of-two for use as bit flags
constexpr UInt64 OrdinalToEnum(UInt64 ordinal)
    { return 1ull << ordinal; }

template <class ContainerType, typename EnumType, typename ValueType, SizeType Sz>
struct EnumMapIterator
{
    ContainerType   &ref;
    SizeType        index;

    EnumMapIterator() = delete;

    EnumMapIterator(ContainerType &ref, SizeType index)
        : ref(ref), index(index)
    {
    }

    EnumMapIterator(const EnumMapIterator &other)
        : ref(other.ref), index(other.index)
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

    HYP_FORCE_INLINE
    EnumMapIterator &operator++()
    {
        ++index;

        return *this;
    }

    HYP_FORCE_INLINE
    EnumMapIterator operator++(int)
    {
        EnumMapIterator temp { *this };

        ++index;

        return temp;
    }

    HYP_FORCE_INLINE
    Bool operator==(const EnumMapIterator &other) const
    {
        return index == other.index
            && std::addressof(ref) == std::addressof(other.ref);
    }

    HYP_FORCE_INLINE
    Bool operator!=(const EnumMapIterator &other) const
        { return !(*this == other); }

    std::pair<EnumType, ValueType &> operator*()
    {
        return std::make_pair<EnumType, ValueType &>(
            EnumType(OrdinalToEnum(index)),
            ref[index]
        );
    }

    std::pair<EnumType, ValueType> operator*() const
    {
        return std::make_pair<EnumType, ValueType>(
            EnumType(OrdinalToEnum(index)),
            ref[index]
        );
    }

    std::pair<EnumType, ValueType &> operator->()
        { return (*this).operator*(); }

    std::pair<EnumType, ValueType> operator->() const
        { return (*this).operator*(); }
};

} // namespace detail
} // namespace containers

template <typename EnumType, typename ValueType, SizeType Sz>
class EnumOptions : public FixedArray<ValueType, Sz>
{
public:
    using Base              = FixedArray<ValueType, Sz>;

    using EnumOption_t      = EnumType;
    using Ordinal_t         = UInt64;
    using KeyValuePairType  = KeyValuePair<EnumType, ValueType>;

    using Iterator          = containers::detail::EnumMapIterator<Base, EnumType, ValueType, Sz>;
    using ConstIterator     = containers::detail::EnumMapIterator<const Base, EnumType, const ValueType, Sz>;

    // convert from attachment (2^x) into ordinal (0-5) for use as an array index
    static constexpr UInt64 EnumToOrdinal(UInt64 option)
        { return containers::detail::EnumToOrdinal(option); }

    // convert from ordinal (0-5) into power-of-two for use as bit flags
    static constexpr UInt64 OrdinalToEnum(UInt64 ordinal)
        { return containers::detail::OrdinalToEnum(ordinal); }

    static_assert(Sz != 0, "EnumOptions cannot have size of zero");
    static_assert(
        OrdinalToEnum(Sz - 1) < MathUtil::MaxSafeValue<EnumType>(),
        "Size too large; enum conversion would cause overflow. "
        "Try changing the enum's underlying type to a larger sized data type?"
    );

    EnumOptions() : Base { } {}

    EnumOptions(std::initializer_list<KeyValuePairType> initializer_list)
        : Base { }
    {
        Array<KeyValuePairType> temp(initializer_list);

        for (const auto &item : temp) {
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
        { return KeyValuePairType { EnumOption_t(OrdinalToEnum(index)), Base::m_values[index] }; }

    constexpr EnumType KeyAt(SizeType index) const
        { return EnumOption_t(OrdinalToEnum(index)); }

    constexpr ValueType &ValueAt(SizeType index)
        { return Base::operator[](index); }

    constexpr const ValueType &ValueAt(SizeType index) const
        { return Base::operator[](index); }

    constexpr ValueType &Get(EnumOption_t enum_key)
        { return Base::m_values[EnumToOrdinal(enum_key)]; }

    constexpr const ValueType &Get(EnumOption_t enum_key) const
        { return Base::m_values[EnumToOrdinal(enum_key)]; }

    constexpr ValueType &operator[](EnumOption_t enum_key)
        { return Base::m_values[EnumToOrdinal(enum_key)]; }

    constexpr const ValueType &operator[](EnumOption_t enum_key) const
        { return Base::m_values[EnumToOrdinal(enum_key)]; }

    EnumOptions &Set(EnumOption_t enum_key, ValueType &&value)
    {
        Ordinal_t ord = EnumToOrdinal(enum_key);
        AssertThrow(ord < Sz);

        Base::m_values[ord] = std::move(value);

        return *this;
    }

    EnumOptions &Set(EnumOption_t enum_key, const ValueType &value)
    {
        Ordinal_t ord = EnumToOrdinal(enum_key);
        AssertThrow(ord < Size());

        Base::m_values[ord] = value;

        return *this;
    }

    EnumOptions &Unset(EnumOption_t enum_key)
    {
        Ordinal_t ord = EnumToOrdinal(enum_key);
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