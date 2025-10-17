/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/reflection/ObjId.hpp>
#include <core/reflection/HypClass.hpp>

namespace hyperion {

HYP_API ANSIStringView GetClassName(const TypeId& typeId)
{
    if (typeId != TypeId::Void())
    {
        const HypClass* hypClass = GetClass(typeId);

        if (hypClass)
        {
            return hypClass->GetName().LookupString();
        }
    }

    return ANSIStringView();
}

} // namespace hyperion