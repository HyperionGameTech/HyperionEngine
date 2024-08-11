/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypClass.hpp>
#include <core/object/HypProperty.hpp>
#include <core/object/HypClassRegistry.hpp>
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

#pragma region HypProperty

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

#pragma endregion HypProperty

#pragma region HypClass

HYP_EXPORT void *HypClass_CreateInstance(const HypClass *hyp_class)
{
    if (!hyp_class) {
        return nullptr;
    }

    const TypeID type_id = hyp_class->GetTypeID();

    ObjectContainerBase &container = ObjectPool::GetContainer(type_id);
    
    const uint32 index = container.NextIndex();
    container.ConstructAtIndex(index);
    container.IncRefStrong(index);

    // *out_handle = ManagedHandle { index + 1 };

    return container.GetObjectPointer(index);
}

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

HYP_EXPORT HypProperty *HypClass_GetProperty(const HypClass *hyp_class, const Name *name)
{
    if (!hyp_class || !name) {
        return nullptr;
    }

    return hyp_class->GetProperty(*name);
}

#pragma endregion HypClass

} // extern "C"