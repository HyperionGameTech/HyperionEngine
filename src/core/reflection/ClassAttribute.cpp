/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/reflection/ClassAttribute.hpp>

#include <core/utilities/StringUtil.hpp>

#include <core/json/JSON.hpp>

namespace hyperion {

#pragma region ClassAttributeValue

const ClassAttributeValue ClassAttributeValue::empty = ClassAttributeValue();

bool ClassAttributeValue::IsString() const
{
    return value.Is<String>();
}

const String& ClassAttributeValue::GetString(const String& defaultValue) const
{
    if (!IsString())
    {
        return defaultValue;
    }

    return value.Get<String>();
}

bool ClassAttributeValue::IsBool() const
{
    return value.Is<bool>();
}

bool ClassAttributeValue::GetBool(bool defaultValue) const
{
    if (!value.HasValue())
    {
        return defaultValue;
    }

    if (const bool* boolPtr = value.TryGet<bool>())
    {
        return *boolPtr;
    }

    if (const String* stringPtr = value.TryGet<String>())
    {
        return !stringPtr->Empty();
    }

    if (const int* intPtr = value.TryGet<int>())
    {
        return *intPtr != 0;
    }

    return defaultValue;
}

bool ClassAttributeValue::IsInt() const
{
    return value.Is<int>();
}

int ClassAttributeValue::GetInt(int defaultValue) const
{
    if (!value.HasValue())
    {
        return defaultValue;
    }

    if (const int* intPtr = value.TryGet<int>())
    {
        return *intPtr;
    }

    if (const String* stringPtr = value.TryGet<String>())
    {
        int intValue;

        if (StringUtil::Parse(*stringPtr, &intValue))
        {
            return intValue;
        }

        return defaultValue;
    }

    if (const bool* boolPtr = value.TryGet<bool>())
    {
        return *boolPtr != false;
    }

    return defaultValue;
}

String ClassAttributeValue::ToString() const
{
    json::JSONValue jsonValue;

    if (value.HasValue())
    {
        Visit(value, [&jsonValue](auto&& v)
            {
                jsonValue = json::JSONValue(v);
            });
    }

    return jsonValue.ToString(true);
}

#pragma endregion ClassAttributeValue

} // namespace hyperion
