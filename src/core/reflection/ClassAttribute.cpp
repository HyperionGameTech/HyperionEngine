/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/reflection/ClassAttribute.hpp>

#include <core/utilities/StringUtil.hpp>

#include <core/utilities/Format.hpp>

#include <core/json/JSON.hpp>

namespace hyperion {

#pragma region ClassAttributeValue

const ClassAttributeValue ClassAttributeValue::empty = ClassAttributeValue();

UTF8StringView ClassAttributeValue::GetString() const
{
    if (!IsString() || !strValue)
    {
        return UTF8StringView();
    }

    return UTF8StringView(strValue);
}

UTF8StringView ClassAttributeValue::GetString(UTF8StringView defaultValue) const
{
    if (!IsString() || !strValue)
    {
        return defaultValue;
    }

    return UTF8StringView(strValue);
}

bool ClassAttributeValue::GetBool(bool defaultValue) const
{
    if (!IsValid())
    {
        return defaultValue;
    }

    if (IsBool())
    {
        return bValue;
    }

    if (IsInt())
    {
        return iValue != 0;
    }

    if (IsString())
    {
        return strValue && Memory::StrLen(strValue) > 0;
    }

    return defaultValue;
}

int ClassAttributeValue::GetInt(int defaultValue) const
{
    if (!IsValid())
    {
        return defaultValue;
    }

    if (IsInt())
    {
        return iValue;
    }

    if (IsString())
    {
        if (strValue)
        {
            int intValue;

            if (StringUtil::Parse(strValue, &intValue))
            {
                return intValue;
            }
        }

        return defaultValue;
    }

    if (IsBool())
    {
        return bValue ? 1 : 0;
    }

    return defaultValue;
}

String ClassAttributeValue::ToString() const
{
    if (IsValid())
    {
        switch (type)
        {
        case ClassAttributeType::STRING:
            return "\"" + String(GetString()) + "\"";
        case ClassAttributeType::INT:
            return String::ToString(iValue);
        case ClassAttributeType::BOOLEAN:
            return bValue ? "true" : "false";
        default:
            break;
        }
    }

    return "<invalid>";
}

#pragma endregion ClassAttributeValue

} // namespace hyperion
