#ifndef HYPERION_V2_LIB_TYPE_ID_HPP
#define HYPERION_V2_LIB_TYPE_ID_HPP

#include <Types.hpp>
#include <HashCode.hpp>

#include <atomic>

namespace hyperion {

struct TypeIDGeneratorBase
{
    static inline std::atomic<UInt> counter { 0u };
};

template <class T>
struct TypeIDGenerator;

struct TypeID
{
    using ValueType = UInt;

private:

    /*! \brief The underlying value of the TypeID object.
        Note, do not rely on using this directly! This could easily
        change between implementations, or depending on the order of which
        TypeIDs are instantiated. */
    ValueType value;

public:
    template <class T>
    static const TypeID &ForType()
        { return TypeIDGenerator<T>::GetID(); }

    constexpr TypeID() : value { } { }
    constexpr TypeID(const ValueType &id) : value(id) {}
    constexpr TypeID(const TypeID &other)
        : value(other.value)
    {
    }

    TypeID &operator=(const TypeID &other)
    {
        value = other.value;

        return *this;
    }

    constexpr TypeID(TypeID &&other) noexcept
        : value(other.value)
    {
        other.value = 0;
    }
    
    TypeID &operator=(TypeID &&other) noexcept
    {
        value = other.value;
        other.value = 0;

        return *this;
    }

    TypeID &operator=(ValueType id)
    {
        value = id;

        return *this;
    }

    HYP_FORCE_INLINE
    explicit operator Bool() const
        { return value != ForType<void>().value; }

    HYP_FORCE_INLINE
    bool operator!() const
        { return value == ForType<void>().value; }

    HYP_FORCE_INLINE
    bool operator==(const TypeID &other) const
        { return value == other.value; }

    HYP_FORCE_INLINE
    bool operator!=(const TypeID &other) const
        { return value != other.value; }

    HYP_FORCE_INLINE
    bool operator<(const TypeID &other) const
        { return value < other.value; }

    HYP_FORCE_INLINE
    bool operator<=(const TypeID &other) const
        { return value <= other.value; }

    HYP_FORCE_INLINE
    bool operator>(const TypeID &other) const
        { return value > other.value; }

    HYP_FORCE_INLINE
    bool operator>=(const TypeID &other) const
        { return value >= other.value; }

    ValueType Value() const
        { return value; }

    HYP_FORCE_INLINE
    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(value);

        return hc;
    }
};

template <class T>
struct TypeIDGenerator : TypeIDGeneratorBase
{
    static const TypeID &GetID()
    {
        static const TypeID id = TypeID { ++TypeIDGeneratorBase::counter };

        return id;
    }
};

template <>
struct TypeIDGenerator<void> : TypeIDGeneratorBase
{
    static const TypeID &GetID()
    {
        static const TypeID id = TypeID { 0u };

        return id;
    }
};

} // namespace hyperion

#endif
