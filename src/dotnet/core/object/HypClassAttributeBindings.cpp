/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypClassAttribute.hpp>

#include <core/Name.hpp>

#include <dotnet/Object.hpp>

#include <Types.hpp>

using namespace hyperion;

extern "C"
{

    HYP_EXPORT const char* HypClassAttribute_GetName(HypClassAttribute* attribute)
    {
        AssertThrow(attribute != nullptr);

        return attribute->GetName().Data();
    }

    HYP_EXPORT const char* HypClassAttribute_GetString(HypClassAttribute* attribute)
    {
        AssertThrow(attribute != nullptr);

        return attribute->GetValue().GetString().Data();
    }

    HYP_EXPORT int8 HypClassAttribute_GetBool(HypClassAttribute* attribute)
    {
        AssertThrow(attribute != nullptr);

        return attribute->GetValue().GetBool();
    }

    HYP_EXPORT int8 HypClassAttribute_GetInt(HypClassAttribute* attribute)
    {
        AssertThrow(attribute != nullptr);

        return attribute->GetValue().GetInt();
    }

} // extern "C"