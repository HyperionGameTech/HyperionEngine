/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/String.hpp>
#include <core/containers/HashSet.hpp>

#include <core/utilities/StringView.hpp>
#include <core/utilities/Span.hpp>
#include <core/utilities/Variant.hpp>
#include <core/Name.hpp>

#include <core/Defines.hpp>
#include <core/Util.hpp>

namespace hyperion {

enum class ClassAttributeType : uint8
{
    NONE = 0,
    STRING,
    INT,
    BOOLEAN
};

struct ClassAttributeValue final
{
    static const ClassAttributeValue empty;

    ClassAttributeValue()
        : type(ClassAttributeType::NONE)
    {
    }

    ClassAttributeValue(const String& value)
        : type(ClassAttributeType::STRING),
          value(value)
    {
    }

    ClassAttributeValue(const char* str)
        : type(ClassAttributeType::STRING),
          value(String(str))
    {
    }

    ClassAttributeValue(int value)
        : type(ClassAttributeType::INT),
          value(value)
    {
    }

    ClassAttributeValue(bool value)
        : type(ClassAttributeType::BOOLEAN),
          value(value)
    {
    }

    ClassAttributeValue(const ClassAttributeValue& other) = default;
    ClassAttributeValue& operator=(const ClassAttributeValue& other) = default;
    ClassAttributeValue(ClassAttributeValue&& other) noexcept = default;
    ClassAttributeValue& operator=(ClassAttributeValue&& other) noexcept = default;

    ~ClassAttributeValue() = default;

    HYP_FORCE_INLINE ClassAttributeType GetType() const
    {
        return type;
    }

    HYP_FORCE_INLINE bool IsValid() const
    {
        return value.HasValue();
    }

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return GetBool();
    }

    HYP_FORCE_INLINE bool operator!() const
    {
        return !bool(*this);
    }

    HYP_FORCE_INLINE bool operator==(const ClassAttributeValue& other) const
    {
        return value == other.value;
    }

    HYP_FORCE_INLINE bool operator!=(const ClassAttributeValue& other) const
    {
        return value != other.value;
    }

    HYP_API bool IsString() const;
    HYP_API const String& GetString(const String& defaultValue = String::empty) const;

    HYP_API bool IsBool() const;
    HYP_API bool GetBool(bool defaultValue = false) const;

    HYP_API bool IsInt() const;
    HYP_API int GetInt(int defaultValue = 0) const;

    HYP_API String ToString() const;

    // private:
    template <class T>
    HYP_FORCE_INLINE bool Compare(const T& other) const
    {
        bool result = false;

        Visit(value, [&other, &result](auto&& v)
            {
                if constexpr (std::is_same_v<NormalizedType<decltype(v)>, String>)
                {
                    result = false;
                }
                else
                {
                    result = (v == other);
                }
            });

        return result;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(type);
        hc.Add(value);

        return hc;
    }

    ClassAttributeType type;
    Variant<String, int, bool> value;
};

struct ClassAttribute final
{
    ClassAttribute() = default;

    ClassAttribute(Name name, const ClassAttributeValue& value)
        : name(name),
          value(value)
    {
    }

    ClassAttribute(ANSIStringView name, const ClassAttributeValue& value)
        : name(CreateNameFromDynamicString(name)),
          value(value)
    {
    }

    ClassAttribute(const ClassAttribute& other) = default;
    ClassAttribute& operator=(const ClassAttribute& other) = default;

    ClassAttribute(ClassAttribute&& other) noexcept = default;
    ClassAttribute& operator=(ClassAttribute&& other) noexcept = default;

    ~ClassAttribute() = default;

    HYP_FORCE_INLINE Name GetName() const
    {
        return name;
    }

    HYP_FORCE_INLINE const ClassAttributeValue& GetValue() const
    {
        return value;
    }

    HYP_FORCE_INLINE bool operator==(const ClassAttribute& other) const
    {
        return name == other.name && value == other.value;
    }

    HYP_FORCE_INLINE bool operator!=(const ClassAttribute& other) const
    {
        return name != other.name || value != other.value;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(name);
        hc.Add(value);

        return hc;
    }

    Name name;
    ClassAttributeValue value;
};

class ClassAttributeSet final
{
public:
    using SetType = HashSet<ClassAttribute, &ClassAttribute::name, DynamicNodeAllocator>;

    using Iterator = typename SetType::Iterator;
    using ConstIterator = typename SetType::ConstIterator;

    ClassAttributeSet() = default;

    ClassAttributeSet(Span<const ClassAttribute> attributes)
    {
        if (!attributes)
        {
            return;
        }

        for (const ClassAttribute& attribute : attributes)
        {
            m_attributes.Insert(attribute);
        }
    }

    ClassAttributeSet(const SetType& attributes)
        : m_attributes(attributes)
    {
    }

    ClassAttributeSet(SetType&& attributes)
        : m_attributes(std::move(attributes))
    {
    }

    ClassAttributeSet(const ClassAttributeSet& other) = default;
    ClassAttributeSet& operator=(const ClassAttributeSet& other) = default;
    ClassAttributeSet(ClassAttributeSet&& other) noexcept = default;
    ClassAttributeSet& operator=(ClassAttributeSet&& other) noexcept = default;
    ~ClassAttributeSet() = default;

    HYP_FORCE_INLINE bool Any() const
    {
        return m_attributes.Any();
    }

    HYP_FORCE_INLINE bool Empty() const
    {
        return m_attributes.Empty();
    }

    HYP_FORCE_INLINE SizeType Size() const
    {
        return m_attributes.Size();
    }

    HYP_FORCE_INLINE ClassAttributeValue& operator[](Name name)
    {
        auto it = m_attributes.FindAs(name);

        if (it == m_attributes.End())
        {
            it = m_attributes.Insert(ClassAttribute(name, ClassAttributeValue())).first;
        }

        return it->value;
    }

    HYP_FORCE_INLINE const ClassAttributeValue& operator[](WeakName name) const
    {
        return Get(name);
    }

    const ClassAttributeValue& Get(WeakName name) const
    {
        static const ClassAttributeValue invalidValue {};

        return Get(name, invalidValue);
    }

    const ClassAttributeValue& Get(WeakName name, const ClassAttributeValue& defaultValue) const
    {
        const auto it = m_attributes.FindAs(name);

        if (it == m_attributes.End())
        {
            return defaultValue;
        }

        return it->GetValue();
    }

    HYP_FORCE_INLINE void Merge(const ClassAttributeSet& other)
    {
        m_attributes.Merge(other.m_attributes);
    }

    HYP_FORCE_INLINE void Merge(ClassAttributeSet&& other)
    {
        m_attributes.Merge(std::move(other.m_attributes));
    }

    HYP_FORCE_INLINE Iterator Find(WeakName name)
    {
        return m_attributes.FindAs(name);
    }

    HYP_FORCE_INLINE ConstIterator Find(WeakName name) const
    {
        return m_attributes.FindAs(name);
    }

    HYP_DEF_STL_BEGIN_END(m_attributes.Begin(), m_attributes.End())

private:
    SetType m_attributes;
};

} // namespace hyperion
