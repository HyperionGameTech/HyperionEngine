/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/Subsystem.hpp>

#include <Engine.hpp>
#include <Types.hpp>

using namespace hyperion;

extern "C" {

HYP_EXPORT const char *Subsystem_GetName(SubsystemBase *subsystem)
{
    if (subsystem == nullptr) {
        return "";
    }

    return subsystem->GetName().Data();
}

} // extern "C"