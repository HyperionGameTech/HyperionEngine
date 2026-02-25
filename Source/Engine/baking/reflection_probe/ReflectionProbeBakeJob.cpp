/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <HyperionPch.hpp>

#include <baking/reflection_probe/ReflectionProbeBakeJob.hpp>

#include <scene/Scene.hpp>
#include <scene/EnvProbe.hpp>

namespace Hyperion {
namespace Baking {

BakeJob<ReflectionProbe>::~BakeJob()
{
}

void BakeJob<ReflectionProbe>::Start_Internal()
{
}

void BakeJob<ReflectionProbe>::Process_Internal(bool* outIsReadyToProcess)
{
    if (outIsReadyToProcess)
    {
        *outIsReadyToProcess = true;
    }
}

} // namespace Baking
} // namespace Hyperion
