#ifndef HYPERION_V2_LIB_TYPE_ID_HPP
#define HYPERION_V2_LIB_TYPE_ID_HPP

#include <core/lib/Mutex.hpp>
#include <core/Name.hpp>
#include <core/Util.hpp>

#include <Types.hpp>
#include <HashCode.hpp>

#include <atomic>

namespace hyperion {

using TypeIDValue = uint32;

struct TypeID
{
    using ValueType = TypeIDValue;

private:
    /*! \brief The underlying value of the TypeID object.
        Note, do not rely on using this directly! This could easily
        change between implementations, or depending on the order of which
        TypeIDs are instantiated. */
    ValueType   value;

    static constexpr ValueType void_value = ValueType(0);

public:
    static const TypeID void_type_id;

    template <class T>
    static constexpr TypeID ForType()
    {
        if constexpr (std::is_same_v<void, T>) {
            return Void();
        } else {
            return TypeID {
                ValueType(TypeName<T>().GetHashCode().Value() % HashCode::ValueType(MathUtil::MaxSafeValue<ValueType>()))
            };
        }
    }

    static TypeID ForName(struct Name name);

    constexpr TypeID()
        : value { }
    {
    }

    constexpr TypeID(ValueType id)
        : value(id)
    {
    }

    constexpr TypeID(const TypeID &other)   = default;
    TypeID &operator=(const TypeID &other)  = default;

    constexpr TypeID(TypeID &&other) noexcept
        : value(other.value)
    {
        other.value = 0;
    }
    
    constexpr TypeID &operator=(TypeID &&other) noexcept
    {
        value = other.value;
        other.value = 0;

        return *this;
    }

    constexpr TypeID &operator=(ValueType id)
    {
        value = id;

        return *this;
    }

    [[nodiscard]]
    HYP_FORCE_INLINE
    constexpr explicit operator bool() const
        { return value != void_value; }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    constexpr bool operator!() const
        { return value == void_value; }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    constexpr bool operator==(const TypeID &other) const
        { return value == other.value; }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    constexpr bool operator!=(const TypeID &other) const
        { return value != other.value; }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    constexpr bool operator<(const TypeID &other) const
        { return value < other.value; }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    constexpr bool operator<=(const TypeID &other) const
        { return value <= other.value; }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    constexpr bool operator>(const TypeID &other) const
        { return value > other.value; }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    constexpr bool operator>=(const TypeID &other) const
        { return value >= other.value; }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    constexpr ValueType Value() const
        { return value; }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    constexpr HashCode GetHashCode() const
        { return HashCode::GetHashCode(value); }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    static constexpr TypeID Void()
        { return TypeID { void_value }; }
};

} // namespace hyperion

#endif
