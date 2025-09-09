/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypConstant.hpp>

#include <core/Name.hpp>

#include <core/Types.hpp>

using namespace hyperion;

extern "C"
{

    HYP_EXPORT void HypConstant_GetName(const HypConstant* constant, Name* outName)
    {
        if (!constant || !outName)
        {
            return;
        }

        *outName = constant->GetName();
    }

    HYP_EXPORT void HypConstant_GetTypeId(const HypConstant* constant, TypeId* outTypeId)
    {
        if (!constant || !outTypeId)
        {
            return;
        }

        *outTypeId = constant->GetTypeId();
    }

} // extern "C"