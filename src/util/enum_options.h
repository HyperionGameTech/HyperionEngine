#ifndef ENUM_OPTIONS_H
#define ENUM_OPTIONS_H

#include "../hash_code.h"
#include "../util.h"

#include <array>
#include <initializer_list>

namespace hyperion {

template <typename EnumType, typename ValueType, size_t Sz>
class EnumOptions {
public:
    using EnumOption_t = EnumType;
    using Ordinal_t = uint64_t;
    using EnumValueArray_t = std::array<ValueType, Sz>;

    // convert from attachment (2^x) into ordinal (0-5) for use as an array index
    static inline Ordinal_t EnumToOrdinal(EnumOption_t option)
        { return FastLog2(option); }

    // convert from ordinal (0-5) into power-of-two for use as bit flags
    static inline EnumOption_t OrdinalToEnum(Ordinal_t ordinal)
        { return EnumOption_t(1 << ordinal); }

    EnumOptions()
        : m_flags(0)
    {
        static_assert(Sz != 0, "EnumOptions cannot have size of zero");
    }

    EnumOptions(const EnumValueArray_t &array)
        : m_values(array),
          m_flags(0)
    {
        UpdateFlags();
    }

    EnumOptions(const EnumOptions &other)
        : m_values(other.m_values),
          m_flags(other.m_flags)
    {
        static_assert(Sz != 0, "EnumOptions cannot have size of zero");
    }

    EnumOptions &operator=(const EnumOptions &other)
    {
        m_values = other.m_values;
        m_flags = other.m_flags;

        return *this;
    }

    ~EnumOptions()
    {
    }

    using EnumValuePair_t = std::pair<EnumType, ValueType>;

    inline EnumValuePair_t KeyValueAt(size_t index) const
        { return std::make_pair(OrdinalToEnum(index), m_values[index]); }

    inline ValueType &Get(EnumOption_t enum_key)
        { return m_values[EnumToOrdinal(enum_key)]; }

    inline const ValueType &Get(EnumOption_t enum_key) const
        { return m_values[EnumToOrdinal(enum_key)]; }

    inline EnumOptions &Set(EnumOption_t enum_key, const ValueType &value)
    {
        m_values[EnumToOrdinal(enum_key)] = value;
        m_flags |= uint64_t(enum_key);

        return *this;
    }

    inline EnumOptions &Unset(EnumOption_t enum_key)
    {
        m_values[EnumToOrdinal(enum_key)] = ValueType();
        m_flags &= ~uint64_t(enum_key);

        return *this;
    }

    inline bool Has(EnumOption_t enum_key) const
        { return m_flags & uint64_t(enum_key); }

    inline size_t Size() const
        { return m_values.size(); }


    // inline ValueType &operator[](EnumOption_t enum_key) { return m_values[EnumToOrdinal(enum_key)]; }
    // inline const ValueType &operator[](EnumOption_t enum_key) const { return m_values[EnumToOrdinal(enum_key)]; }

    inline HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(m_flags);

        for (const auto &it : m_values) {
            hc.Add(it.GetHashCode());
        }

        return hc;
    }

    /*class iterator {
    public:
        iterator(EnumValuePair_t pair, const EnumValueArray_t *values): pair(pair), values(values) {}
        iterator operator++() {
            size_t index = EnumToOrdinal(pair.first);
            pair = std::make_pair(OrdinalToEnum(index + 1), values->at(index + 1));

            return *this;
        }
        bool operator!=(const iterator &other) const { return pair != other.pair; }
        EnumValuePair_t &operator*() { return pair; }
        const EnumValuePair_t &operator*() const { return pair; }
    private:
        EnumValuePair_t pair;
        const EnumValueArray_t *values;
    };

    iterator begin() const { return iterator(std::make_pair(OrdinalToEnum(0), m_values.front()), &m_values); }
    iterator end() const { return iterator(std::make_pair(OrdinalToEnum(Size - 1), m_values.back()), &m_values); }*/

private:
    EnumValueArray_t m_values;
    uint64_t m_flags;

    inline void UpdateFlags()
    {
        m_flags = 0;

        for (int i = 0; i < m_values.size(); i++) {
            m_flags |= OrdinalToEnum(i);
        }
    }
};

} // namespace hyperion

#endif
