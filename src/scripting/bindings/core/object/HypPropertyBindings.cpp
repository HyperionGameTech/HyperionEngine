/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypClass.hpp>
#include <core/object/HypProperty.hpp>
#include <core/object/HypObject.hpp>
#include <core/object/HypData.hpp>

#include <core/Name.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <dotnet/Object.hpp>

#include <core/serialization/fbom/FBOM.hpp>

#include <core/Types.hpp>

using namespace hyperion;

extern "C"
{

    HYP_EXPORT void HypProperty_GetName(const HypProperty* property, Name* outName)
    {
        if (!property || !outName)
        {
            return;
        }

        *outName = property->GetName();
    }

    HYP_EXPORT void HypProperty_GetTypeId(const HypProperty* property, TypeId* outTypeId)
    {
        if (!property || !outTypeId)
        {
            return;
        }

        *outTypeId = property->GetTypeId();
    }

    HYP_EXPORT bool HypProperty_InvokeGetter(const HypProperty* property, const HypClass* targetClass, void* targetPtr, HypData* outResult)
    {
        if (!property || !targetClass || !targetPtr || !outResult)
        {
            return false;
        }

        if (!property->CanGet())
        {
            return false;
        }

        HypData targetData { AnyRef(targetClass->GetTypeId(), targetPtr) };

        *outResult = property->Get(targetData);

        return true;
    }

    HYP_EXPORT bool HypProperty_InvokeSetter(const HypProperty* property, const HypClass* targetClass, void* targetPtr, HypData* value)
    {
        if (!property || !targetClass || !targetPtr || !value)
        {
            return false;
        }

        if (!property->CanSet())
        {
            return false;
        }

        HypData targetData { AnyRef(targetClass->GetTypeId(), targetPtr) };

        property->Set(targetData, *value);

        return true;
    }

} // extern "C"