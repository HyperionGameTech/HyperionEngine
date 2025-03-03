/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypClass.hpp>
#include <core/object/HypClassRegistry.hpp>
#include <core/object/HypObject.hpp>
#include <core/object/HypData.hpp>

#include <core/Name.hpp>

#include <dotnet/Object.hpp>

#include <Types.hpp>

using namespace hyperion;

extern "C" {

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

HYP_EXPORT uint32 HypClass_GetSize(const HypClass *hyp_class)
{
    if (!hyp_class) {
        return 0;
    }

    return uint32(hyp_class->GetSize());
}

HYP_EXPORT uint32 HypClass_GetFlags(const HypClass *hyp_class)
{
    if (!hyp_class) {
        return 0;
    }

    return uint32(hyp_class->GetFlags());
}

HYP_EXPORT uint8 HypClass_GetAllocationMethod(const HypClass *hyp_class)
{
    if (!hyp_class) {
        return uint8(HypClassAllocationMethod::INVALID);
    }

    return uint8(hyp_class->GetAllocationMethod());
}

HYP_EXPORT uint32 HypClass_GetAttributes(const HypClass *hyp_class, const void **out_attributes)
{
    if (!hyp_class || !out_attributes) {
        return 0;
    }

    if (hyp_class->GetAttributes().Empty()) {
        return 0;
    }

    *out_attributes = hyp_class->GetAttributes().Begin();

    return (uint32)hyp_class->GetAttributes().Size();
}

HYP_EXPORT const HypClassAttribute *HypClass_GetAttribute(const HypClass *hyp_class, const char *name)
{
    if (!hyp_class || !name) {
        return nullptr;
    }

    auto it = hyp_class->GetAttributes().Find(name);

    if (it == hyp_class->GetAttributes().End()) {
        return nullptr;
    }

    return it;
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

HYP_EXPORT HypProperty *HypClass_GetProperty(const HypClass *hyp_class, const Name *name)
{
    if (!hyp_class || !name) {
        return nullptr;
    }

    return hyp_class->GetProperty(*name);
}

HYP_EXPORT uint32 HypClass_GetMethods(const HypClass *hyp_class, const void **out_methods)
{
    if (!hyp_class || !out_methods) {
        return 0;
    }

    if (hyp_class->GetMethods().Empty()) {
        return 0;
    }

    *out_methods = hyp_class->GetMethods().Begin();

    return (uint32)hyp_class->GetMethods().Size();
}

HYP_EXPORT HypMethod *HypClass_GetMethod(const HypClass *hyp_class, const Name *name)
{
    if (!hyp_class || !name) {
        return nullptr;
    }

    return hyp_class->GetMethod(*name);
}

HYP_EXPORT HypField *HypClass_GetField(const HypClass *hyp_class, const Name *name)
{
    if (!hyp_class || !name) {
        return nullptr;
    }

    return hyp_class->GetField(*name);
}

HYP_EXPORT uint32 HypClass_GetFields(const HypClass *hyp_class, const void **out_fields)
{
    if (!hyp_class || !out_fields) {
        return 0;
    }

    if (hyp_class->GetFields().Empty()) {
        return 0;
    }

    *out_fields = hyp_class->GetFields().Begin();

    return (uint32)hyp_class->GetFields().Size();
}

HYP_EXPORT HypConstant *HypClass_GetConstant(const HypClass *hyp_class, const Name *name)
{
    if (!hyp_class || !name) {
        return nullptr;
    }

    return hyp_class->GetConstant(*name);
}

HYP_EXPORT uint32 HypClass_GetConstants(const HypClass *hyp_class, const void **out_constants)
{
    if (!hyp_class || !out_constants) {
        return 0;
    }

    if (hyp_class->GetConstants().Empty()) {
        return 0;
    }

    *out_constants = hyp_class->GetConstants().Begin();

    return (uint32)hyp_class->GetConstants().Size();
}

#pragma endregion HypClass

} // extern "C"