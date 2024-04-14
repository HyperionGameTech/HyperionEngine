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

/*! \brief Simple 32-bit identifier for a given type. Stable across DLLs as the type hash is based on the name of the type. */
struct TypeID
{
    using ValueType = TypeIDValue;

private:
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

    template <SizeType size>
    static constexpr TypeID FromString(const char (&str)[size])
    {
        return TypeID {
            ValueType(HashCode::GetHashCode(str).Value() % HashCode::ValueType(MathUtil::MaxSafeValue<ValueType>()))
        };
    }

    static TypeID FromString(const char *str)
    {
        return TypeID {
            ValueType(HashCode::GetHashCode(str).Value() % HashCode::ValueType(MathUtil::MaxSafeValue<ValueType>()))
        };
    }

    constexpr TypeID()
        : value { void_value }
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
