/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/HypClass.hpp>
#include <core/HypClassProperty.hpp>
#include <core/HypClassRegistry.hpp>

#include <core/Name.hpp>

#include <Engine.hpp>
#include <Types.hpp>

using namespace hyperion;

extern "C" {

#pragma region HypClassProperty

HYP_EXPORT void HypClassProperty_GetName(const HypClassProperty *hyp_class_property, Name *out_name)
{
    if (!hyp_class_property || !out_name) {
        return;
    }

    *out_name = hyp_class_property->name;
}

HYP_EXPORT void HypClassProperty_GetTypeID(const HypClassProperty *hyp_class_property, TypeID *out_type_id)
{
    if (!hyp_class_property || !out_type_id) {
        return;
    }

    *out_type_id = hyp_class_property->type_id;
}

#pragma endregion HypClassProperty

#pragma region HypClass

HYP_EXPORT const HypClass *HypClass_GetClassByName(const char *name)
{
    const WeakName weak_name(name);

    return HypClassRegistry::GetInstance().GetClass(weak_name);
}

HYP_EXPORT void HypClass_GetName(const HypClass *hyp_class, Name *out_name)
{
    if (!hyp_class || !out_name) {
        return;
    }

    *out_name = hyp_class->GetName();
}

#pragma endregion HypClass

} // extern "C"