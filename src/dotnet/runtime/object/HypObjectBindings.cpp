/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypClass.hpp>
#include <core/object/HypProperty.hpp>
#include <core/object/HypClassRegistry.hpp>
#include <core/object/HypObject.hpp>

#include <core/ObjectPool.hpp>

#include <dotnet/Object.hpp>

#include <Engine.hpp>
#include <Types.hpp>

using namespace hyperion;

extern "C" {

struct HypObjectInitializer
{
    const HypClass  *hyp_class;
    void            *native_address;
};

#pragma region HypObject

HYP_EXPORT void HypObject_Validate(const HypClass *hyp_class, void *native_address)
{
    if (!hyp_class || !native_address) {
        return;
    }

    const TypeID type_id = hyp_class->GetTypeID();

    ObjectContainerBase &container = ObjectPool::GetContainer(type_id);
    
    const uint32 index = container.GetObjectIndex(native_address);

    if (index == ~0u) {
        HYP_FAIL("Address %p is not valid for object container for TypeID %u", native_address, type_id.Value());
    }
}

HYP_EXPORT void HypObject_IncRef(const HypClass *hyp_class, void *native_address)
{
    if (!hyp_class || !native_address) {
        return;
    }

    const TypeID type_id = hyp_class->GetTypeID();

    ObjectContainerBase &container = ObjectPool::GetContainer(type_id);
    
    const uint32 index = container.GetObjectIndex(native_address);
    if (index == ~0u) {
        HYP_FAIL("Address %p is not valid for object container for TypeID %u", native_address, type_id.Value());
    }

    container.IncRefStrong(index);
}

HYP_EXPORT void HypObject_DecRef(const HypClass *hyp_class, void *native_address)
{
    if (!native_address || !hyp_class) {
        return;
    }

    const TypeID type_id = hyp_class->GetTypeID();

    ObjectContainerBase &container = ObjectPool::GetContainer(type_id);
    
    const uint32 index = container.GetObjectIndex(native_address);
    if (index == ~0u) {
        HYP_FAIL("Address %p is not valid for object container for TypeID %u", native_address, type_id.Value());
    }
    
    container.DecRefStrong(index);
}

HYP_EXPORT HypProperty *HypObject_GetProperty(const HypClass *hyp_class, const Name *name)
{
    if (!hyp_class || !name) {
        return nullptr;
    }

    return hyp_class->GetProperty(*name);
}

#pragma endregion HypObject

} // extern "C"