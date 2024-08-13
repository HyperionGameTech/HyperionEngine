/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypClass.hpp>
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

#pragma region HypClass

HYP_EXPORT void *HypClass_CreateInstance(const HypClass *hyp_class)
{
    if (!hyp_class) {
        return nullptr;
    }

    if (hyp_class->UseHandles()) {
        ObjectContainerBase &container = ObjectPool::GetContainer(hyp_class->GetTypeID());
        
        const uint32 index = container.NextIndex();
        container.ConstructAtIndex(index);
        container.IncRefStrong(index);

        return container.GetObjectPointer(index);
    } else if (hyp_class->UseRefCountedPtr()) {
        Any any_value;
        hyp_class->CreateInstance(any_value);

        return UniquePtr<void>(std::move(any_value)).ToRefCountedPtr().GetRefCountData_Internal();
    } else {
        HYP_FAIL("Unsupported allocation method for HypClass %s", *hyp_class->GetName());
    }
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

#pragma endregion HypClass

} // extern "C"