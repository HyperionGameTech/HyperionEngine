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

class HypClassAttributeValue
{
public:
    HypClassAttributeValue()                                                    = default;

    HypClassAttributeValue(const String &value)
        : m_value(value)
    {
    }

    HypClassAttributeValue(const char *str)
        : m_value(String(str))
    {
    }

    HypClassAttributeValue(int value)
        : m_value(value)
    {
    }

    HypClassAttributeValue(double value)
        : m_value(value)
    {
    }

    HypClassAttributeValue(bool value)
        : m_value(value)
    {
    }

    HypClassAttributeValue(const HypClassAttributeValue &other)                 = default;
    HypClassAttributeValue &operator=(const HypClassAttributeValue &other)      = default;
    HypClassAttributeValue(HypClassAttributeValue &&other) noexcept             = default;
    HypClassAttributeValue &operator=(HypClassAttributeValue &&other) noexcept  = default;

    ~HypClassAttributeValue()                                                   = default;

    HYP_FORCE_INLINE explicit operator bool() const
    {
        if (!m_value.HasValue()) {
            return false;
        }

        if (const String *string_ptr = m_value.TryGet<String>()) {
            return !string_ptr->Empty();
        }

        if (const bool *bool_ptr = m_value.TryGet<bool>()) {
            return *bool_ptr;
        }

        return true;
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

    HYP_FORCE_INLINE bool IsString() const
    {
        return m_value.Is<String>();
    }

    HYP_FORCE_INLINE const String &GetString() const
    {
        if (!IsString()) {
            return String::empty;
        }

        return m_value.Get<String>();
    }

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

    HypClassAttributeSet(Span<HypClassAttribute> attributes)
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

    HYP_DEF_STL_BEGIN_END(m_attributes.Begin(), m_attributes.End())

private:
    FlatSet<HypClassAttribute>  m_attributes;
};

} // namespace hyperion

#endif