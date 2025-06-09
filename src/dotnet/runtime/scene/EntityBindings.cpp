/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/Entity.hpp>

using namespace hyperion;

extern "C"
{

    HYP_EXPORT uint64 Entity_GetID(const Entity* entity)
    {
        if (!entity)
        {
            return 0;
        }

        uint64 value = 0;
        value |= entity->GetID().Value();
        value |= (uint64(entity->GetID().GetTypeID().Value()) << 32);

        return value;
    }

} // extern "C"
