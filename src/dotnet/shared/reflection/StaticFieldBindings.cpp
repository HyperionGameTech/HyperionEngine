/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <core/reflection/StaticField.hpp>

#include <core/Name.hpp>

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

    HYP_EXPORT void StaticField_Get(const StaticField* staticField, HypData* outData)
    {
        Assert(staticField != nullptr);
        Assert(outData != nullptr);

        *outData = staticField->Get();
    }

} // extern "C"
