/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/Core.hpp>

#include <core/object/HypClass.hpp>

namespace hyperion {

const HypClass *GetClass(TypeID type_id)
{
    return HypClassRegistry::GetInstance().GetClass(type_id);
}

const HypClass *GetClass(WeakName type_name)
{
    return HypClassRegistry::GetInstance().GetClass(type_name);
}

bool IsInstanceOfHypClass(const HypClass *hyp_class, TypeID type_id)
{
    if (!hyp_class) {
        return false;
    }

    if (hyp_class->GetTypeID() == type_id) {
        return true;
    }
    
    const HypClass *other_hyp_class = GetClass(type_id);

    while (other_hyp_class) {
        if (other_hyp_class == hyp_class) {
            return true;
        }

        other_hyp_class = other_hyp_class->GetParent();
    }

    return false;
}

} // namespace hyperion