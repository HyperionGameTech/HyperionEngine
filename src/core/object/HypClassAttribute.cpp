/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypClassAttribute.hpp>

#include <util/json/JSON.hpp>

namespace hyperion {

#pragma region HypClassAttributeValue

String HypClassAttributeValue::ToString() const
{
    json::JSONValue json_value;

    if (m_value.HasValue()) {
        Visit(m_value, [&json_value](auto &&value)
        {
            json_value = json::JSONValue(value);
        });
    }

    return json_value.ToString(true);
}

#pragma endregion HypClassAttributeValue

} // namespace hyperion