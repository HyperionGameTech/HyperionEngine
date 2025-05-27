/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypConstant.hpp>

#include <core/Name.hpp>

#include <Types.hpp>

using namespace hyperion;

extern "C"
{

    HYP_EXPORT void HypConstant_GetName(const HypConstant* constant, Name* out_name)
    {
        if (!constant || !out_name)
        {
            return;
        }

        *out_name = constant->GetName();
    }

    HYP_EXPORT void HypConstant_GetTypeID(const HypConstant* constant, TypeID* out_type_id)
    {
        if (!constant || !out_type_id)
        {
            return;
        }

        *out_type_id = constant->GetTypeID();
    }

} // extern "C"