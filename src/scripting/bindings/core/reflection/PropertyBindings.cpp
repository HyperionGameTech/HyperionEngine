/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <core/reflection/Class.hpp>
#include <core/reflection/Property.hpp>
#include <core/reflection/Object.hpp>
#include <core/Name.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <dotnet/ManagedObject.hpp>

#include <core/serialization/fbom/FBOM.hpp>

using namespace hyperion;

extern "C"
{

    HYP_EXPORT void Property_GetName(const Property* property, Name* outName)
    {
        if (!property || !outName)
        {
            return;
        }

        *outName = property->GetName();
    }

    HYP_EXPORT void Property_GetTypeId(const Property* property, TypeId* outTypeId)
    {
        if (!property || !outTypeId)
        {
            return;
        }

        *outTypeId = property->GetTypeId();
    }

    HYP_EXPORT bool Property_InvokeGetter(const Property* property, const Class* targetClass, void* targetPtr, HypData* outResult)
    {
        if (!property || !targetClass || !targetPtr || !outResult)
        {
            return false;
        }

        if (!property->CanGet())
        {
            return false;
        }

        HypData targetData { AnyRef(targetClass->GetTypeInfo(), targetPtr) };

        *outResult = property->Get(targetData);

        return true;
    }

    HYP_EXPORT bool Property_InvokeSetter(const Property* property, const Class* targetClass, void* targetPtr, HypData* value)
    {
        if (!property || !targetClass || !targetPtr || !value)
        {
            return false;
        }

        if (!property->CanSet())
        {
            return false;
        }

        HypData targetData { AnyRef(targetClass->GetTypeInfo(), targetPtr) };

        property->Set(targetData, *value);

        return true;
    }

} // extern "C"
