/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/HypClass.hpp>
#include <core/HypClassProperty.hpp>
#include <core/HypClassRegistry.hpp>
#include <core/HypObject.hpp>

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

HYP_EXPORT void HypObject_IncRef(const HypClass *hyp_class, void *native_address)
{
    AssertThrow(native_address != nullptr && hyp_class != nullptr);

    const TypeID type_id = hyp_class->GetTypeID();

    ObjectContainerBase &container = ObjectPool::GetContainer(type_id);
    
    const uint32 index = container.GetObjectIndex(native_address);
    if (index == ~0u) {
        HYP_FAIL("Address %p is not valid for object container for TypeID %u", type_id.Value());
    }

    container.IncRefStrong(index);
}

HYP_EXPORT void HypObject_DecRef(const HypClass *hyp_class, void *native_address)
{
    AssertThrow(native_address != nullptr && hyp_class != nullptr);

    const TypeID type_id = hyp_class->GetTypeID();

    ObjectContainerBase &container = ObjectPool::GetContainer(type_id);
    
    const uint32 index = container.GetObjectIndex(native_address);
    if (index == ~0u) {
        HYP_FAIL("Address %p is not valid for object container for TypeID %u", type_id.Value());
    }
    
    container.DecRefStrong(index);
}

#pragma endregion HypObject

} // extern "C"