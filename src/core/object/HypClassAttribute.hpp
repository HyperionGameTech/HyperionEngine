/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_HYP_CLASS_ATTRIBUTE_HPP
#define HYPERION_CORE_HYP_CLASS_ATTRIBUTE_HPP

#include <core/containers/String.hpp>
#include <core/containers/HashMap.hpp>
#include <core/containers/FlatSet.hpp>

#include <core/utilities/StringView.hpp>
#include <core/utilities/Span.hpp>
#include <core/utilities/Variant.hpp>

#include <core/Defines.hpp>
#include <core/Util.hpp>

namespace hyperion {

enum class HypClassAttributeType : uint32
{
    NONE    = 0,
    STRING,
    INT,
    FLOAT,
    BOOLEAN
};

class HypClassAttributeValue
{
public:
    static const HypClassAttributeValue empty;

    HypClassAttributeValue()
        : m_type(HypClassAttributeType::NONE)
    {
    }

    HypClassAttributeValue(const String &value)
        : m_type(HypClassAttributeType::STRING),
          m_value(value)
    {
    }

    HypClassAttributeValue(const char *str)
        : m_type(HypClassAttributeType::STRING),
          m_value(String(str))
    {
    }

    HypClassAttributeValue(int value)
        : m_type(HypClassAttributeType::INT),
          m_value(value)
    {
    }

    HypClassAttributeValue(double value)
        : m_type(HypClassAttributeType::FLOAT),
          m_value(value)
    {
    }

    HypClassAttributeValue(bool value)
        : m_type(HypClassAttributeType::BOOLEAN),
          m_value(value)
    {
    }

    HypClassAttributeValue(const HypClassAttributeValue &other)                 = default;
    HypClassAttributeValue &operator=(const HypClassAttributeValue &other)      = default;
    HypClassAttributeValue(HypClassAttributeValue &&other) noexcept             = default;
    HypClassAttributeValue &operator=(HypClassAttributeValue &&other) noexcept  = default;

    ~HypClassAttributeValue()                                                   = default;

    HYP_FORCE_INLINE HypClassAttributeType GetType() const
        { return m_type; }

    HYP_FORCE_INLINE bool IsValid() const
        { return m_value.HasValue(); }

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return GetBool();
    }

    HYP_FORCE_INLINE bool operator!() const
        { return !bool(*this); }

    HYP_FORCE_INLINE bool operator==(const HypClassAttributeValue &other) const
    {
        return m_value == other.m_value;
    }

    HYP_FORCE_INLINE bool operator!=(const HypClassAttributeValue &other) const
    {
        return m_value != other.m_value;
    }

    HYP_API bool IsString() const;
    HYP_API const String &GetString() const;
    HYP_API bool IsBool() const;
    HYP_API bool GetBool() const;
    HYP_API bool IsInt() const;
    HYP_API int GetInt() const;

    HYP_API String ToString() const;

    // HYP_FORCE_INLINE bool operator==(UTF8StringView other) const
    // {
    //     if (const String *string_ptr = m_value.TryGet<String>()) {
    //         return *string_ptr == other;
    //     }

    //     return false;
    // }

    // HYP_FORCE_INLINE bool operator!=(UTF8StringView other) const
    // {
    //     if (const String *string_ptr = m_value.TryGet<String>()) {
    //         return *string_ptr != other;
    //     }

    //     return true;
    // }

    // HYP_FORCE_INLINE bool operator==(int value) const
    //     { return Compare<int>(value); }

    // HYP_FORCE_INLINE bool operator!=(int value) const
    //     { return !Compare<int>(value); }

    // HYP_FORCE_INLINE bool operator==(double value) const
    //     { return Compare<double>(value); }

    // HYP_FORCE_INLINE bool operator!=(double value) const
    //     { return !Compare<double>(value); }

    // HYP_FORCE_INLINE bool operator==(bool value) const
    //     { return Compare<bool>(value); }

    // HYP_FORCE_INLINE bool operator!=(bool value) const
    //     { return !Compare<bool>(value); }

// private:
    template <class T>
    HYP_FORCE_INLINE bool Compare(const T &other) const
    {
        bool result = false;

        Visit(m_value, [other, &result](auto &&value)
        {
            if constexpr (std::is_same_v<NormalizedType<decltype(value)>, String>) {
                result = false;
            } else {
                result = value == other;
            }
        });

        return result;
    }

    HypClassAttributeType               m_type;
    Variant<String, int, double, bool>  m_value;
};

class HypClassAttribute
{
public:
    HypClassAttribute()                                                 = default;

    HypClassAttribute(const ANSIStringView &name, const HypClassAttributeValue &value)
        : m_name(name),
          m_value(value)
    {
    }

    HypClassAttribute(const HypClassAttribute &other)                   = default;
    HypClassAttribute &operator=(const HypClassAttribute &other)        = default;

    HypClassAttribute(HypClassAttribute &&other) noexcept               = default;
    HypClassAttribute &operator=(HypClassAttribute &&other) noexcept    = default;
    
    ~HypClassAttribute()                                                = default;

    HYP_FORCE_INLINE ANSIStringView GetName() const
        { return m_name; }

    HYP_FORCE_INLINE const HypClassAttributeValue &GetValue() const
        { return m_value; }

    HYP_FORCE_INLINE bool operator==(const HypClassAttribute &other) const
        { return m_name == other.m_name && m_value == other.m_value; }

    HYP_FORCE_INLINE bool operator==(ANSIStringView other) const
        { return m_name == other; }

    HYP_FORCE_INLINE bool operator!=(const HypClassAttribute &other) const
        { return m_name != other.m_name || m_value != other.m_value; }

    HYP_FORCE_INLINE bool operator!=(ANSIStringView other) const
        { return m_name != other; }

    HYP_FORCE_INLINE bool operator<(const HypClassAttribute &other) const
        { return m_name < other.m_name; }

    HYP_FORCE_INLINE bool operator<(ANSIStringView other) const
        { return m_name < other; }

private:
    ANSIStringView          m_name;
    HypClassAttributeValue  m_value;
};

class HypClassAttributeSet
{
public:
    using Iterator = typename FlatSet<HypClassAttribute>::Iterator;
    using ConstIterator = typename FlatSet<HypClassAttribute>::ConstIterator;

    HypClassAttributeSet()                                                     = default;

    HypClassAttributeSet(Span<const HypClassAttribute> attributes)
    {
        if (!attributes) {
            return;
        }

        for (const HypClassAttribute &attribute : attributes) {
            m_attributes.Insert(attribute);
        }
    }

    HypClassAttributeSet(const FlatSet<HypClassAttribute> &attributes)
        : m_attributes(attributes)
    {
    }

    HypClassAttributeSet(FlatSet<HypClassAttribute> &&attributes)
        : m_attributes(std::move(attributes))
    {
    }

    HypClassAttributeSet(const HypClassAttributeSet &other)                   = default;
    HypClassAttributeSet &operator=(const HypClassAttributeSet &other)        = default;
    HypClassAttributeSet(HypClassAttributeSet &&other) noexcept               = default;
    HypClassAttributeSet &operator=(HypClassAttributeSet &&other) noexcept    = default;
    ~HypClassAttributeSet()                                                    = default;

    HYP_FORCE_INLINE bool Any() const
        { return m_attributes.Any(); }

    HYP_FORCE_INLINE bool Empty() const
        { return m_attributes.Empty(); }

    HYP_FORCE_INLINE SizeType Size() const
        { return m_attributes.Size(); }

    HYP_FORCE_INLINE const HypClassAttributeValue &operator[](ANSIStringView name) const
        { return Get(name); }

    const HypClassAttributeValue &Get(ANSIStringView name) const
    {
        static const HypClassAttributeValue invalid_value { };

        return Get(name, invalid_value);
    }

    const HypClassAttributeValue &Get(ANSIStringView name, const HypClassAttributeValue &default_value) const
    {
        const auto it = m_attributes.FindAs(name);

        if (it == m_attributes.End()) {
            return default_value;
        }

        return it->GetValue();
    }

    HYP_FORCE_INLINE void Merge(const HypClassAttributeSet &other)
    {
        m_attributes.Merge(other.m_attributes);
    }

    HYP_FORCE_INLINE void Merge(HypClassAttributeSet &&other)
    {
        m_attributes.Merge(std::move(other.m_attributes));
    }

    HYP_FORCE_INLINE Iterator Find(ANSIStringView name)
        { return m_attributes.FindAs(name); }

    HYP_FORCE_INLINE ConstIterator Find(ANSIStringView name) const
        { return m_attributes.FindAs(name); }

    HYP_DEF_STL_BEGIN_END(m_attributes.Begin(), m_attributes.End())

private:
    FlatSet<HypClassAttribute>  m_attributes;
};

} // namespace hyperion

#endif