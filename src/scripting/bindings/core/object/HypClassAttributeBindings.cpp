/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/reflection/ClassAttribute.hpp>

#include <core/Name.hpp>

#include <dotnet/ManagedObject.hpp>

#include <core/Types.hpp>

using namespace hyperion;

extern "C"
{

    HYP_EXPORT const char* ClassAttribute_GetName(ClassAttribute* attribute)
    {
        Assert(attribute != nullptr);

        return *attribute->GetName();
    }

    HYP_EXPORT const char* ClassAttribute_GetString(ClassAttribute* attribute)
    {
        Assert(attribute != nullptr);

        return attribute->GetValue().GetString().Data();
    }

    HYP_EXPORT int8 ClassAttribute_GetBool(ClassAttribute* attribute)
    {
        Assert(attribute != nullptr);

        return attribute->GetValue().GetBool();
    }

    HYP_EXPORT int8 ClassAttribute_GetInt(ClassAttribute* attribute)
    {
        Assert(attribute != nullptr);

        return attribute->GetValue().GetInt();
    }

} // extern "C"
