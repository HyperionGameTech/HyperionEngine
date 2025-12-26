/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/String.hpp>
#include <core/containers/HashSet.hpp>

#include <core/utilities/StringView.hpp>
#include <core/utilities/Span.hpp>
#include <core/Name.hpp>

#include <core/Defines.hpp>
#include <core/Util.hpp>

namespace Hyperion {

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
        : type(ClassAttributeType::NONE),
          strValue(nullptr)
    {
    }

    ClassAttributeValue(const String& value)
        : type(ClassAttributeType::STRING),
          strValue(new char[value.Size() + 1])
    {
        Memory::StrCpy(strValue, value.Data(), value.Size() + 1);
    }

    ClassAttributeValue(const char* str)
        : type(ClassAttributeType::STRING),
          strValue(nullptr)
    {
        if (str)
        {
            const SizeType len = Memory::StrLen(str);
            strValue = new char[len + 1];

            Memory::StrCpy(strValue, str, len + 1);
        }
    }

    ClassAttributeValue(int value)
        : type(ClassAttributeType::INT),
          iValue(value)
    {
    }

    ClassAttributeValue(bool value)
        : type(ClassAttributeType::BOOLEAN),
          bValue(value)
    {
    }

    ClassAttributeValue(const ClassAttributeValue& other)
        : type(other.type)
    {
        switch (type)
        {
        case ClassAttributeType::STRING:
            if (other.strValue)
            {
                const SizeType len = Memory::StrLen(other.strValue);
                strValue = new char[len + 1];

                Memory::StrCpy(strValue, other.strValue, len + 1);
            }
            else
            {
                strValue = nullptr;
            }
            break;
        case ClassAttributeType::INT:
            iValue = other.iValue;
            break;
        case ClassAttributeType::BOOLEAN:
            bValue = other.bValue;
            break;
        default:
            strValue = nullptr;
            break;
        }
    }

    ClassAttributeValue& operator=(const ClassAttributeValue& other)
    {
        if (this != &other)
        {
            // Clean up existing value
            if (type == ClassAttributeType::STRING && strValue)
            {
                delete[] strValue;
                strValue = nullptr;
            }

            type = other.type;

            switch (type)
            {
            case ClassAttributeType::STRING:
                if (other.strValue)
                {
                    const SizeType len = Memory::StrLen(other.strValue);
                    strValue = new char[len + 1];

                    Memory::StrCpy(strValue, other.strValue, len + 1);
                }
                else
                {
                    strValue = nullptr;
                }

                break;
            case ClassAttributeType::INT:
                iValue = other.iValue;
                break;
            case ClassAttributeType::BOOLEAN:
                bValue = other.bValue;
                break;
            default:
                strValue = nullptr;
                break;
            }
        }

        return *this;
    }

    ClassAttributeValue(ClassAttributeValue&& other) noexcept
        : type(other.type)
    {
        switch (type)
        {
        case ClassAttributeType::STRING:
            strValue = other.strValue;
            other.strValue = nullptr;
            break;
        case ClassAttributeType::INT:
            iValue = other.iValue;
            break;
        case ClassAttributeType::BOOLEAN:
            bValue = other.bValue;
            break;
        default:
            strValue = nullptr;
            break;
        }
    }

    ClassAttributeValue& operator=(ClassAttributeValue&& other) noexcept
    {
        if (this != &other)
        {
            // Clean up existing value
            if (type == ClassAttributeType::STRING && strValue)
            {
                delete[] strValue;
                strValue = nullptr;
            }

            type = other.type;

            switch (type)
            {
            case ClassAttributeType::STRING:
                strValue = other.strValue;
                other.strValue = nullptr;
                break;
            case ClassAttributeType::INT:
                iValue = other.iValue;
                break;
            case ClassAttributeType::BOOLEAN:
                bValue = other.bValue;
                break;
            default:
                strValue = nullptr;
                break;
            }
        }

        return *this;
    }

    ~ClassAttributeValue()
    {
        if (type == ClassAttributeType::STRING && strValue)
        {
            delete[] strValue;
            strValue = nullptr;
        }
    }

    HYP_FORCE_INLINE ClassAttributeType GetType() const
    {
        return type;
    }

    HYP_FORCE_INLINE bool IsValid() const
    {
        return type != ClassAttributeType::NONE;
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
        if (type != other.type)
        {
            return false;
        }

        switch (type)
        {
        case ClassAttributeType::STRING:
            if (strValue == nullptr && other.strValue == nullptr)
            {
                return true;
            }
            else if (strValue == nullptr || other.strValue == nullptr)
            {
                return false;
            }
            else
            {
                return Memory::StrCmp(strValue, other.strValue) == 0;
            }
        case ClassAttributeType::INT:
            return iValue == other.iValue;
        case ClassAttributeType::BOOLEAN:
            return bValue == other.bValue;
        default:
            return false;
        }
    }

    HYP_FORCE_INLINE bool operator!=(const ClassAttributeValue& other) const
    {
        return !(*this == other);
    }

    HYP_FORCE_INLINE bool IsString() const
    {
        return type == ClassAttributeType::STRING;
    }

    HYP_API UTF8StringView GetString() const;
    HYP_API UTF8StringView GetString(UTF8StringView defaultValue) const;

    HYP_FORCE_INLINE bool IsBool() const
    {
        return type == ClassAttributeType::BOOLEAN;
    }

    HYP_API bool GetBool(bool defaultValue = false) const;

    HYP_FORCE_INLINE bool IsInt() const
    {
        return type == ClassAttributeType::INT;
    }

    HYP_API int GetInt(int defaultValue = 0) const;

    HYP_API String ToString() const;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(type);

        switch (type)
        {
        case ClassAttributeType::STRING:
            if (strValue)
            {
                hc.Add(strValue);
            }
            break;
        case ClassAttributeType::INT:
            hc.Add(iValue);
            break;
        case ClassAttributeType::BOOLEAN:
            hc.Add(bValue);
            break;
        default:
            break;
        }

        return hc;
    }

    ClassAttributeType type;

    union
    {
        char* strValue;
        int iValue;
        bool bValue;
    };
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

    HYP_FORCE_INLINE const ClassAttributeValue& operator[](StringHash name) const
    {
        return Get(name);
    }

    const ClassAttributeValue& Get(StringHash name) const
    {
        static const ClassAttributeValue invalidValue {};

        return Get(name, invalidValue);
    }

    const ClassAttributeValue& Get(StringHash name, const ClassAttributeValue& defaultValue) const
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

    HYP_FORCE_INLINE Iterator Find(StringHash name)
    {
        return m_attributes.FindAs(name);
    }

    HYP_FORCE_INLINE ConstIterator Find(StringHash name) const
    {
        return m_attributes.FindAs(name);
    }

    HYP_DEF_STL_BEGIN_END(m_attributes.Begin(), m_attributes.End())

private:
    SetType m_attributes;
};

} // namespace Hyperion
