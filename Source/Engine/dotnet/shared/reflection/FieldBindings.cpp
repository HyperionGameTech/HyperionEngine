/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#include <HyperionPch.hpp>

#include <Core/reflection/Field.hpp>

#include <Core/Name.hpp>

using namespace Hyperion;

extern "C"
{

    HYP_EXPORT void Field_GetName(const Field* field, Name* outName)
    {
        if (!field || !outName)
        {
            return;
        }

        *outName = field->GetName();
    }

    HYP_EXPORT void Field_GetTypeId(const Field* field, TypeId* outTypeId)
    {
        if (!field || !outTypeId)
        {
            return;
        }

        *outTypeId = field->GetTypeId();
    }

    HYP_EXPORT uint32 Field_GetOffset(const Field* field)
    {
        if (!field)
        {
            return 0;
        }

        return field->GetOffset();
    }

    HYP_EXPORT void Field_Get(const Field* field, const BoxedValue* targetData, BoxedValue* outData)
    {
        Assert(field != nullptr);
        Assert(targetData != nullptr);
        Assert(outData != nullptr);

        *outData = field->Get(*targetData);
    }

} // extern "C"
