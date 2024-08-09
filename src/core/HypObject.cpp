/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/HypObject.hpp>

#include <core/HypClass.hpp>
#include <core/HypClassRegistry.hpp>

#include <dotnet/Class.hpp>
#include <dotnet/Object.hpp>

#include <Engine.hpp>

namespace hyperion {

HYP_API void InitHypObjectInitializer(IHypObjectInitializer *initializer, void *parent, TypeID type_id, const HypClass *hyp_class)
{
    AssertThrowMsg(hyp_class != nullptr, "No HypClass registered for class! Is HYP_DEFINE_CLASS missing for the type?");

    if (dotnet::Class *managed_class = hyp_class->GetManagedClass()) {
        initializer->SetManagedObject(managed_class->NewObject(hyp_class, parent));
    }
}

} // namespace hyperion