/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypClassAttribute.hpp>

#include <core/Name.hpp>

#include <dotnet/Object.hpp>

#include <core/Types.hpp>

using namespace hyperion;

extern "C"
{

    HYP_EXPORT const char* HypClassAttribute_GetName(HypClassAttribute* attribute)
    {
        Assert(attribute != nullptr);

        return *attribute->GetName();
    }

    HYP_EXPORT const char* HypClassAttribute_GetString(HypClassAttribute* attribute)
    {
        Assert(attribute != nullptr);

        return attribute->GetValue().GetString().Data();
    }

    HYP_EXPORT int8 HypClassAttribute_GetBool(HypClassAttribute* attribute)
    {
        Assert(attribute != nullptr);

        return attribute->GetValue().GetBool();
    }

    HYP_EXPORT int8 HypClassAttribute_GetInt(HypClassAttribute* attribute)
    {
        Assert(attribute != nullptr);

        return attribute->GetValue().GetInt();
    }

} // extern "C"
