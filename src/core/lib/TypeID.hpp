#ifndef HYPERION_V2_LIB_TYPE_ID_HPP
#define HYPERION_V2_LIB_TYPE_ID_HPP

#include <Types.hpp>
#include <HashCode.hpp>

#include <atomic>

namespace hyperion {

struct TypeID
{
    using Value = UInt;

private:
    struct TypeIDGeneratorBase
    {
        static inline std::atomic<Value> counter { 0u };
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

public:
    template <class T>
    static const TypeID &ForType()
    {
        // static const TypeID id = TypeID { ++type_id_counter };

        // return id;

        return TypeIDGenerator<T>::GetID();
    }

    /*! \brief The underlying value of the TypeID object.
        Note, do not rely on using this directly! This could easily
        change between implementations, or depending on the order of which
        TypeIDs are instantiated. */
    Value value;

    constexpr TypeID() : value { } { }
    constexpr TypeID(const Value &id) : value(id) {}
    constexpr TypeID(const TypeID &other) = default;
    TypeID &operator=(const TypeID &other) = default;

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

    TypeID &operator=(Value id)
    {
        value = id;

        return *this;
    }

    bool operator==(const TypeID &other) const { return value == other.value; }
    bool operator!=(const TypeID &other) const { return value != other.value; }
    bool operator<(const TypeID &other) const  { return value < other.value; }
    bool operator>(const TypeID &other) const  { return value > other.value; }

    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(value);

        return hc;
    }
};

} // namespace hyperion

#endif
