/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <HyperionPch.hpp>

#include <baking/shadow_map/ShadowMapBakeJob.hpp>

#include <scene/Scene.hpp>
#include <scene/Light.hpp>

namespace Hyperion {
namespace Baking {

BakeJob<Light>::~BakeJob()
{
}

void BakeJob<Light>::Start_Internal()
{
    // TODO: Perform any per-job setup needed before ray tracing begins
    //       (e.g. uploading per-light projection matrices to the GPU).
}

void BakeJob<Light>::Process_Internal(bool* outIsReadyToProcess)
{
    // TODO: Check whether any GPU readiness conditions have been met
    //       (acceleration structures built, scene rendered at least once, etc.).
    //       Set *outIsReadyToProcess = true when the job may proceed.

    if (outIsReadyToProcess)
    {
        *outIsReadyToProcess = true;
    }
}

} // namespace Baking
} // namespace Hyperion
