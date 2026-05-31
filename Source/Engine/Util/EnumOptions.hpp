/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Containers/FixedArray.hpp>
#include <Core/Containers/Array.hpp>
#include <Core/Utilities/Pair.hpp>
#include <Core/HashCode.hpp>
#include <Core/Math/MathUtil.hpp>

#include <Core/Types.hpp>

#include <initializer_list>
#include <limits>

namespace Hyperion {

namespace containers {
// convert from attachment (2^x) into ordinal (0-5) for use as an array index
template <class EnumType, class OrdinalType = std::underlying_type_t<EnumType>>
constexpr OrdinalType EnumToOrdinal(EnumType option)
{
    return OrdinalType(MathUtil::FastLog2_Pow2(uint64(option)));
}

// convert from ordinal (0-5) into power-of-two for use as bit flags
template <class EnumType, class OrdinalType = std::underlying_type_t<EnumType>>
constexpr EnumType OrdinalToEnum(OrdinalType ordinal)
{
    return EnumType(1ull << ordinal);
}

template <class ContainerType, typename EnumType, typename ValueType, size_t Sz>
struct EnumMapIterator
{
    using OrdinalType = std::underlying_type_t<EnumType>;

    ContainerType& ref;
    size_t index;

    EnumMapIterator() = delete;

    EnumMapIterator(ContainerType& ref, size_t index)
        : ref(ref),
          index(index)
    {
    }

    EnumMapIterator(const EnumMapIterator& other)
        : ref(other.ref),
          index(other.index)
    {
    }

    EnumMapIterator& operator=(const EnumMapIterator& other)
    {
        ref = other.ref;
        index = other.index;

        return *this;
    }

    EnumMapIterator(EnumMapIterator&& other) noexcept
        : ref(other.ref),
          index(other.index)
    {
    }

    EnumMapIterator& operator=(EnumMapIterator&& other) noexcept
    {
        ref = other.ref;
        index = other.index;

        return *this;
    }

    ~EnumMapIterator() = default;

    HYP_FORCE_INLINE EnumMapIterator& operator++()
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

    HYP_FORCE_INLINE bool operator==(const EnumMapIterator& other) const
    {
        return index == other.index && (&ref == &other.ref);
    }

    HYP_FORCE_INLINE bool operator!=(const EnumMapIterator& other) const
    {
        return !(*this == other);
    }

    HYP_FORCE_INLINE Pair<EnumType, ValueType&> operator*()
    {
        return { OrdinalToEnum<EnumType, OrdinalType>(OrdinalType(index)), ref[index] };
    }

    HYP_FORCE_INLINE Pair<EnumType, const ValueType&> operator*() const
    {
        return { OrdinalToEnum<EnumType, OrdinalType>(OrdinalType(index)), ref[index] };
    }

    HYP_FORCE_INLINE ValueType* operator->() const
    {
        return &ref[index];
    }
};

} // namespace containers

template <typename EnumType, typename ValueType, size_t Sz>
class EnumOptions : public FixedArray<ValueType, Sz>
{
public:
    using Base = FixedArray<ValueType, Sz>;

    using OrdinalType = std::underlying_type_t<EnumType>;
    using KeyValuePairType = KeyValuePair<EnumType, ValueType>;

    using Iterator = containers::EnumMapIterator<Base, EnumType, ValueType, Sz>;
    using ConstIterator = containers::EnumMapIterator<const Base, EnumType, const ValueType, Sz>;

    static constexpr OrdinalType maxValue = MathUtil::MaxSafeValue<EnumType>();

    // convert from attachment (2^x) into ordinal (0-5) for use as an array index
    static constexpr OrdinalType EnumToOrdinal(EnumType value)
    {
        return containers::EnumToOrdinal<EnumType, OrdinalType>(value);
    }

    // convert from ordinal (0-5) into power-of-two for use as bit flags
    static constexpr EnumType OrdinalToEnum(OrdinalType ordinal)
    {
        return containers::OrdinalToEnum<EnumType, OrdinalType>(ordinal);
    }

    static_assert(Sz != 0, "EnumOptions cannot have size of zero");
    static_assert(
        OrdinalType(OrdinalToEnum(Sz - 1)) < maxValue,
        "Size too large; enum conversion would cause overflow. "
        "Try changing the enum's underlying type to a larger sized data type?");

    EnumOptions()
        : Base {}
    {
    }

    EnumOptions(std::initializer_list<KeyValuePairType> initializerList)
        : Base {}
    {
        for (const auto& item : initializerList)
        {
            Set(item.first, item.second);
        }
    }

    EnumOptions(const EnumOptions& other)
        : Base(static_cast<const Base&>(other))
    {
    }

    EnumOptions& operator=(const EnumOptions& other)
    {
        Base::operator=(static_cast<const Base&>(other));

        return *this;
    }

    EnumOptions(EnumOptions&& other)
        : Base(static_cast<Base&&>(other))
    {
    }

    EnumOptions& operator=(EnumOptions&& other) noexcept
    {
        Base::operator=(static_cast<Base&&>(other));

        return *this;
    }

    ~EnumOptions() = default;

    constexpr KeyValuePairType KeyValueAt(size_t index) const
    {
        return KeyValuePairType { EnumType(OrdinalToEnum(index)), Base::m_values[index] };
    }

    constexpr EnumType KeyAt(size_t index) const
    {
        return EnumType(OrdinalToEnum(index));
    }

    constexpr ValueType& ValueAt(size_t index)
    {
        return Base::operator[](index);
    }

    constexpr const ValueType& ValueAt(size_t index) const
    {
        return Base::operator[](index);
    }

    constexpr ValueType& Get(EnumType enumKey)
    {
        return Base::m_values[EnumToOrdinal(enumKey)];
    }

    constexpr const ValueType& Get(EnumType enumKey) const
    {
        return Base::m_values[EnumToOrdinal(enumKey)];
    }

    constexpr ValueType& operator[](EnumType enumKey)
    {
        return Base::m_values[EnumToOrdinal(enumKey)];
    }

    constexpr const ValueType& operator[](EnumType enumKey) const
    {
        return Base::m_values[EnumToOrdinal(enumKey)];
    }

    EnumOptions& Set(EnumType enumKey, ValueType&& value)
    {
        OrdinalType ord = EnumToOrdinal(enumKey);
        Assert(ord < Sz);

        Base::m_values[ord] = std::move(value);

        return *this;
    }

    EnumOptions& Set(EnumType enumKey, const ValueType& value)
    {
        OrdinalType ord = EnumToOrdinal(enumKey);
        Assert(ord < Size());

        Base::m_values[ord] = value;

        return *this;
    }

    EnumOptions& Unset(EnumType enumKey)
    {
        OrdinalType ord = EnumToOrdinal(enumKey);
        Assert(ord < Sz);

        Base::m_values[ord] = {};

        return *this;
    }

    constexpr size_t Size() const
    {
        return Base::Size();
    }

    ValueType* Data()
    {
        return Base::Data();
    }

    const ValueType* Data() const
    {
        return Base::Data();
    }

    void Clear()
    {
        for (auto& value : Base::m_values)
        {
            value = ValueType {};
        }
    }

    HYP_DEF_STL_BEGIN_END(Iterator(static_cast<Base&>(*this), 0), Iterator(static_cast<Base&>(*this), Size()));
};

} // namespace Hyperion
