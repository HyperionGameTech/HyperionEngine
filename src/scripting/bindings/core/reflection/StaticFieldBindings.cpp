/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/reflection/StaticField.hpp>

#include <core/Name.hpp>

#include <core/Types.hpp>

using namespace hyperion;

extern "C"
{

    HYP_EXPORT void StaticField_GetName(const StaticField* staticField, Name* outName)
    {
        if (!staticField || !outName)
        {
            return;
        }

        *outName = staticField->GetName();
    }

    HYP_EXPORT void StaticField_GetTypeId(const StaticField* staticField, TypeId* outTypeId)
    {
        if (!staticField || !outTypeId)
        {
            return;
        }

        *outTypeId = staticField->GetTypeId();
    }

} // extern "C"