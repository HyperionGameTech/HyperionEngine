#ifndef HYPERION_V2_LIB_TYPE_ID_HPP
#define HYPERION_V2_LIB_TYPE_ID_HPP

#include <core/NameInternal.hpp>
#include <core/Util.hpp>

#include <Types.hpp>
#include <HashCode.hpp>

#include <atomic>

namespace hyperion {

struct TypeIDNameMap
{
    static constexpr SizeType max_size = 4096;

    Name names[max_size];

    HYP_FORCE_INLINE
    void Set(UInt index, Name name)
    {
        AssertThrowMsg(index < max_size, "TypeID out of range");
        names[index] = name;
    }

    HYP_FORCE_INLINE
    Name Get(UInt index) const
    {
        AssertThrowMsg(index < max_size, "TypeID out of range");
        return names[index];
    }
};

struct TypeIDNameMapDefinition
{
    TypeIDNameMapDefinition(TypeIDNameMap &name_map, UInt index, Name name)
    {
        name_map.Set(index, name);
    }
};

struct TypeIDGeneratorBase
{
    static TypeIDNameMap name_map;

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
    Bool operator!() const
        { return value == ForType<void>().value; }

    HYP_FORCE_INLINE
    Bool operator==(const TypeID &other) const
        { return value == other.value; }

    HYP_FORCE_INLINE
    Bool operator!=(const TypeID &other) const
        { return value != other.value; }

    HYP_FORCE_INLINE
    Bool operator<(const TypeID &other) const
        { return value < other.value; }

    HYP_FORCE_INLINE
    Bool operator<=(const TypeID &other) const
        { return value <= other.value; }

    HYP_FORCE_INLINE
    Bool operator>(const TypeID &other) const
        { return value > other.value; }

    HYP_FORCE_INLINE
    Bool operator>=(const TypeID &other) const
        { return value >= other.value; }

    HYP_FORCE_INLINE
    ValueType Value() const
        { return value; }

    // HYP_FORCE_INLINE
    // Name Name() const
    //     { return TypeIDGeneratorBase::name_map.Get(value); }

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
        // Cannot use CreateNameFromStaticString_WithLock here, as we have an include order issue
        // Due to the String class using Bytebuffer which uses UniquePtr/RefCountedPtr, which in turn uses TypeID
        // Bytebuffer should be changed, when that's done, we can use CreateNameFromStaticString_WithLock
        // static const TypeIDNameMapDefinition def(TypeIDGeneratorBase::name_map, id.Value(), CreateNameFromStaticString_WithLock(HashedName<TypeName<T>()>()));

        return id;
    }
};

template <>
struct TypeIDGenerator<void> : TypeIDGeneratorBase
{
    static const TypeID &GetID()
    {
        static const TypeID id = TypeID { 0u };
        // static const TypeIDNameMapDefinition def(TypeIDGeneratorBase::name_map, id.Value(), Name::invalid);

        return id;
    }
};

} // namespace hyperion

#endif
