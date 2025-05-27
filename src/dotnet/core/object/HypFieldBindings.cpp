/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypField.hpp>

#include <core/Name.hpp>

#include <Types.hpp>

using namespace hyperion;

extern "C"
{

    HYP_EXPORT void HypField_GetName(const HypField* field, Name* out_name)
    {
        if (!field || !out_name)
        {
            return;
        }

        *out_name = field->GetName();
    }

    HYP_EXPORT void HypField_GetTypeID(const HypField* field, TypeID* out_type_id)
    {
        if (!field || !out_type_id)
        {
            return;
        }

        *out_type_id = field->GetTypeID();
    }

    HYP_EXPORT uint32 HypField_GetOffset(const HypField* field)
    {
        if (!field)
        {
            return 0;
        }

        return field->GetOffset();
    }

} // extern "C"