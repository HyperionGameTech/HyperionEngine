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

#include <Types.hpp>

using namespace hyperion;

extern "C" {

HYP_EXPORT void HypProperty_GetName(const HypProperty *property, Name *out_name)
{
    if (!property || !out_name) {
        return;
    }

    *out_name = property->GetName();
}

HYP_EXPORT void HypProperty_GetTypeID(const HypProperty *property, TypeID *out_type_id)
{
    if (!property || !out_type_id) {
        return;
    }

    *out_type_id = property->GetTypeID();
}

HYP_EXPORT bool HypProperty_InvokeGetter(const HypProperty *property, const HypClass *target_class, void *target_ptr, HypData *out_result)
{
    if (!property || !target_class || !target_ptr || !out_result) {
        return false;
    }

    if (!property->CanGet()) {
        return false;
    }

    HypData target_data { AnyRef(target_class->GetTypeID(), target_ptr) };

    *out_result = property->Get(target_data);

    return true;
}

HYP_EXPORT bool HypProperty_InvokeSetter(const HypProperty *property, const HypClass *target_class, void *target_ptr, HypData *value)
{
    if (!property || !target_class || !target_ptr || !value) {
        return false;
    }

    if (!property->CanSet()) {
        return false;
    }

    HypData target_data { AnyRef(target_class->GetTypeID(), target_ptr) };

    property->Set(target_data, *value);

    return true;
}

} // extern "C"