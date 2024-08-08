/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/HypClass.hpp>
#include <core/HypClassProperty.hpp>
#include <core/HypClassRegistry.hpp>

#include <core/Name.hpp>

#include <asset/serialization/fbom/FBOM.hpp>

#include <Engine.hpp>
#include <Types.hpp>

using namespace hyperion;

extern "C" {

#pragma region HypClassProperty

HYP_EXPORT void HypClassProperty_GetName(const HypClassProperty *property, Name *out_name)
{
    if (!property || !out_name) {
        return;
    }

    *out_name = property->name;
}

HYP_EXPORT void HypClassProperty_GetTypeID(const HypClassProperty *property, TypeID *out_type_id)
{
    if (!property || !out_type_id) {
        return;
    }

    *out_type_id = property->type_id;
}

HYP_EXPORT bool HypClassProperty_InvokeGetter(const HypClassProperty *property, const HypClass *target_class, void *target_ptr, fbom::FBOMData **out_value_ptr)
{
    if (!property || !target_class || !target_ptr || !out_value_ptr) {
        return false;
    }

    *out_value_ptr = new fbom::FBOMData(property->InvokeGetter(ConstAnyRef(target_class->GetTypeID(), target_ptr)));

    return true;
}

#pragma endregion HypClassProperty

#pragma region HypClass

HYP_EXPORT const HypClass *HypClass_GetClassByName(const char *name)
{
    if (!name) {
        return nullptr;
    }

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

HYP_EXPORT void HypClass_GetTypeID(const HypClass *hyp_class, TypeID *out_type_id)
{
    if (!hyp_class || !out_type_id) {
        return;
    }

    *out_type_id = hyp_class->GetTypeID();
}

HYP_EXPORT uint32 HypClass_GetProperties(const HypClass *hyp_class, const void **out_properties)
{
    if (!hyp_class || !out_properties) {
        return 0;
    }

    if (hyp_class->GetProperties().Empty()) {
        return 0;
    }

    *out_properties = hyp_class->GetProperties().Begin();

    return (uint32)hyp_class->GetProperties().Size();
}

HYP_EXPORT HypClassProperty *HypClass_GetProperty(const HypClass *hyp_class, const Name *name)
{
    if (!hyp_class || !name) {
        return nullptr;
    }

    return hyp_class->GetProperty(*name);
}

#pragma endregion HypClass

} // extern "C"