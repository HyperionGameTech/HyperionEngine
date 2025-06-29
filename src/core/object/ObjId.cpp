/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/ObjId.hpp>
#include <core/object/HypClass.hpp>

namespace hyperion {

HYP_API ANSIStringView GetClassName(const TypeId& type_id)
{
    if (type_id != TypeId::Void())
    {
        const HypClass* hyp_class = GetClass(type_id);

        if (hyp_class)
        {
            return hyp_class->GetName().LookupString();
        }
    }

    return ANSIStringView();
}

} // namespace hyperion