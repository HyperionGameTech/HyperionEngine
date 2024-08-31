/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypObject.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/object/HypClass.hpp>
#include <core/object/HypClassRegistry.hpp>

#include <dotnet/Class.hpp>
#include <dotnet/Object.hpp>

#include <Engine.hpp>

namespace hyperion {

HYP_API void InitHypObjectInitializer(IHypObjectInitializer *initializer, void *native_address, TypeID type_id, const HypClass *hyp_class)
{
    AssertThrowMsg(hyp_class != nullptr, "No HypClass registered for class! Is HYP_DEFINE_CLASS missing for the type?");

    if (!hyp_class->IsAbstract()) {
        // testing
        if (hyp_class->UseHandles()) {
            AssertThrow(ObjectPool::TryGetContainer(type_id) != nullptr);
        }

        if (dotnet::Class *managed_class = hyp_class->GetManagedClass()) {
            initializer->SetManagedObject(managed_class->NewObject(hyp_class, native_address));
        } else {
            HYP_LOG(Object, LogLevel::WARNING, "No managed class found for HypClass {}; Cannot create managed object", hyp_class->GetName());
        }
    }
}

HYP_API HypClassAllocationMethod GetHypClassAllocationMethod(const HypClass *hyp_class)
{
    AssertThrow(hyp_class != nullptr);
    
    return hyp_class->GetAllocationMethod();
}

} // namespace hyperion