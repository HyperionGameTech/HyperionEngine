/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/HypClass.hpp>
#include <core/HypClassRegistry.hpp>

#include <core/Name.hpp>

#include <Engine.hpp>
#include <Types.hpp>

using namespace hyperion;

extern "C" {

HYP_EXPORT const HypClass *HypClass_GetClassByName(const char *name)
{
    const WeakName weak_name(name);

    return HypClassRegistry::GetInstance().GetClass(weak_name);
}

} // extern "C"