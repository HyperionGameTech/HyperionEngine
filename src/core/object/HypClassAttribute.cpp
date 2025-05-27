/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypClassAttribute.hpp>

#include <core/utilities/StringUtil.hpp>

#include <core/json/JSON.hpp>

namespace hyperion {

#pragma region HypClassAttributeValue

const HypClassAttributeValue HypClassAttributeValue::empty = HypClassAttributeValue();

bool HypClassAttributeValue::IsString() const
{
    return m_value.Is<String>();
}

const String& HypClassAttributeValue::GetString() const
{
    if (!IsString())
    {
        return String::empty;
    }

    return m_value.Get<String>();
}

bool HypClassAttributeValue::IsBool() const
{
    return m_value.Is<bool>();
}

bool HypClassAttributeValue::GetBool() const
{
    if (!m_value.HasValue())
    {
        return false;
    }

    if (const bool* bool_ptr = m_value.TryGet<bool>())
    {
        return *bool_ptr;
    }

    if (const String* string_ptr = m_value.TryGet<String>())
    {
        return !string_ptr->Empty();
    }

    if (const int* int_ptr = m_value.TryGet<int>())
    {
        return *int_ptr != 0;
    }

    return true;
}

bool HypClassAttributeValue::IsInt() const
{
    return m_value.Is<int>();
}

int HypClassAttributeValue::GetInt() const
{
    if (!m_value.HasValue())
    {
        return 0;
    }

    if (const int* int_ptr = m_value.TryGet<int>())
    {
        return *int_ptr;
    }

    if (const String* string_ptr = m_value.TryGet<String>())
    {
        int int_value;

        if (StringUtil::Parse(*string_ptr, &int_value))
        {
            return int_value;
        }

        return 0;
    }

    if (const bool* bool_ptr = m_value.TryGet<bool>())
    {
        return *bool_ptr != false;
    }

    return 0;
}

String HypClassAttributeValue::ToString() const
{
    json::JSONValue json_value;

    if (m_value.HasValue())
    {
        Visit(m_value, [&json_value](auto&& value)
            {
                json_value = json::JSONValue(value);
            });
    }

    return json_value.ToString(true);
}

#pragma endregion HypClassAttributeValue

} // namespace hyperion