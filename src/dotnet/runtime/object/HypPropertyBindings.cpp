/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypClass.hpp>
#include <core/object/HypProperty.hpp>
#include <core/object/HypObject.hpp>
#include <core/object/HypData.hpp>

#include <core/Name.hpp>

#include <dotnet/Object.hpp>

#include <dotnet/runtime/ManagedHandle.hpp>

#include <asset/serialization/fbom/FBOM.hpp>

#include <Engine.hpp>
#include <Types.hpp>

using namespace hyperion;

extern "C" {

HYP_EXPORT void HypProperty_GetName(const HypProperty *property, Name *out_name)
{
    if (!property || !out_name) {
        return;
    }

    *out_name = property->name;
}

HYP_EXPORT void HypProperty_GetTypeID(const HypProperty *property, TypeID *out_type_id)
{
    if (!property || !out_type_id) {
        return;
    }

    *out_type_id = property->type_id;
}

HYP_EXPORT bool HypProperty_InvokeGetter(const HypProperty *property, const HypClass *target_class, void *target_ptr, HypData *out_result)
{
    if (!property || !target_class || !target_ptr || !out_result) {
        return false;
    }

    *out_result = property->InvokeGetter(ConstAnyRef(target_class->GetTypeID(), target_ptr));

    return true;
}

} // extern "C"