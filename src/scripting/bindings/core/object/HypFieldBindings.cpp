/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypField.hpp>

#include <core/Name.hpp>

#include <core/Types.hpp>

using namespace hyperion;

extern "C"
{

    HYP_EXPORT void HypField_GetName(const HypField* field, Name* outName)
    {
        if (!field || !outName)
        {
            return;
        }

        *outName = field->GetName();
    }

    HYP_EXPORT void HypField_GetTypeId(const HypField* field, TypeId* outTypeId)
    {
        if (!field || !outTypeId)
        {
            return;
        }

        *outTypeId = field->GetTypeId();
    }

    HYP_EXPORT uint32 HypField_GetOffset(const HypField* field)
    {
        if (!field)
        {
            return 0;
        }

        return field->GetOffset();
    }

    HYP_EXPORT void HypField_Get(const HypField* field, const HypData* targetData, HypData* outData)
    {
        Assert(field != nullptr);
        Assert(targetData != nullptr);
        Assert(outData != nullptr);

        *outData = field->Get(*targetData);
    }

} // extern "C"