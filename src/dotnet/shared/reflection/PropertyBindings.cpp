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

    HYP_EXPORT void Property_GetName(const Property* pProperty, Name* pOutName)
    {
        if (!pProperty || !pOutName)
        {
            return;
        }

        *pOutName = pProperty->GetName();
    }

    HYP_EXPORT void Property_GetTypeId(const Property* property, TypeId* outTypeId)
    {
        if (!property || !outTypeId)
        {
            return;
        }

        *outTypeId = property->GetTypeId();
    }

    HYP_EXPORT const TypeInfo* Property_GetTypeInfo(const Property* property)
    {
        if (!property)
        {
            return nullptr;
        }

        return &property->GetTypeInfo();
    }

    HYP_EXPORT bool Property_InvokeGetter(const Property* property, const Class* targetClass, void* targetPtr, BoxedValue* outResult)
    {
        if (!property || !targetClass || !targetPtr || !outResult)
        {
            return false;
        }

        if (!property->CanGet())
        {
            return false;
        }

        BoxedValue targetData { AnyRef(targetClass->GetTypeInfo(), targetPtr) };

        *outResult = property->Get(targetData);

        return true;
    }

    HYP_EXPORT bool Property_InvokeSetter(const Property* property, const Class* targetClass, void* targetPtr, BoxedValue* value)
    {
        if (!property || !targetClass || !targetPtr || !value)
        {
            return false;
        }

        if (!property->CanSet())
        {
            return false;
        }

        BoxedValue targetData { AnyRef(targetClass->GetTypeInfo(), targetPtr) };

        property->Set(targetData, *value);

        return true;
    }

    HYP_EXPORT uint32 Property_GetAttributes(const Property* property, const ClassAttribute** outAttributes)
    {
        if (!property)
        {
            return 0;
        }

        const ClassAttributeSet& attributes = property->GetAttributes();

        if (!outAttributes)
        {
            return uint32(attributes.Size());
        }

        uint32 index = 0;

        for (const ClassAttribute& attribute : attributes)
        {
            outAttributes[index++] = &attribute;
        }

        return index;
    }

    HYP_EXPORT const ClassAttribute* Property_GetAttribute(const Property* property, const Name* name)
    {
        if (!property || !name)
        {
            return nullptr;
        }

        auto it = property->GetAttributes().Find(StringHash(*name));
        if (it == property->GetAttributes().End())
        {
            return nullptr;
        }

        return &*it;
    }

} // extern "C"
