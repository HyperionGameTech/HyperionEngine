/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/Entity.hpp>

using namespace hyperion;

extern "C"
{

    HYP_EXPORT uint32 Entity_GetID(const Entity* entity)
    {
        if (!entity)
        {
            return 0;
        }

        return entity->GetID().Value();
    }

} // extern "C"
