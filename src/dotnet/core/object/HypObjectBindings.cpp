/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypClass.hpp>
#include <core/object/HypClassRegistry.hpp>
#include <core/object/HypObject.hpp>

#include <core/utilities/GlobalContext.hpp>

#include <core/ObjectPool.hpp>

#include <dotnet/Object.hpp>
#include <dotnet/Class.hpp>
#include <dotnet/interop/ManagedObject.hpp>

#include <Types.hpp>

using namespace hyperion;

extern "C" {

struct HypObjectInitializer
{
    const HypClass  *hyp_class;
    void            *native_address;
};

#pragma region HypObject

HYP_EXPORT void HypObject_Initialize(const HypClass *hyp_class, dotnet::Class *class_object_ptr, dotnet::ObjectReference *object_reference, void **out_instance_ptr)
{
    AssertThrow(hyp_class != nullptr);
    AssertThrow(class_object_ptr != nullptr);
    AssertThrow(object_reference != nullptr);
    AssertThrow(out_instance_ptr != nullptr);

    HypObjectPtr ptr;

    *out_instance_ptr = nullptr;

    {
        // Suppress default managed object creation
        HypObjectInitializerFlagsGuard flags_guard(HypObjectInitializerFlags::SUPPRESS_MANAGED_OBJECT_CREATION);

        HypData value;
        
        // Set allow_abstract to true so we can use classes marked as "Abstract"
        // allowing the managed class to override methods of an abstract class
        hyp_class->CreateInstance(value, /* allow_abstract */ true);

        // Ref counts are kept as 1 for Handle<T> and RC<T>, managed side is responsible for decrementing the ref count
        if (hyp_class->UseHandles()) {
            AnyHandle handle = std::move(value.Get<AnyHandle>());
            AssertThrow(handle.IsValid());

            ptr = HypObjectPtr(hyp_class, handle.Release());
        } else if (hyp_class->UseRefCountedPtr()) {
            RC<void> rc = std::move(value.Get<RC<void>>());
            AssertThrow(rc != nullptr);

            ptr = HypObjectPtr(hyp_class, rc.Release());
        } else {
            HYP_FAIL("Unsupported allocation method for HypClass %s", *hyp_class->GetName());
        }
    }

    *out_instance_ptr = ptr.GetPointer();

    IHypObjectInitializer *initializer = ptr.GetObjectInitializer();
    AssertThrow(initializer != nullptr);

    initializer->SetManagedObjectResource(AllocateResource<ManagedObjectResource>(
        ptr,
        *object_reference,
        ObjectFlags::CREATED_FROM_MANAGED
    ));
}

HYP_EXPORT uint32 HypObject_GetRefCount_Strong(const HypClass *hyp_class, void *native_address)
{
    AssertThrow(hyp_class != nullptr);
    AssertThrow(native_address != nullptr);

    HypObjectPtr hyp_object_ptr = HypObjectPtr(hyp_class, native_address);

    return hyp_object_ptr.GetRefCount_Strong();
}

HYP_EXPORT void HypObject_IncRef(const HypClass *hyp_class, void *native_address, int8 is_weak)
{
    AssertThrow(hyp_class != nullptr);
    AssertThrow(native_address != nullptr);

    HypObjectPtr hyp_object_ptr = HypObjectPtr(hyp_class, native_address);
    hyp_object_ptr.IncRef(is_weak);
}

HYP_EXPORT void HypObject_DecRef(const HypClass *hyp_class, void *native_address, int8 is_weak)
{
    AssertThrow(hyp_class != nullptr);
    AssertThrow(native_address != nullptr);

    HypObjectPtr hyp_object_ptr = HypObjectPtr(hyp_class, native_address);
    hyp_object_ptr.DecRef(is_weak);
}

#pragma endregion HypObject

} // extern "C"