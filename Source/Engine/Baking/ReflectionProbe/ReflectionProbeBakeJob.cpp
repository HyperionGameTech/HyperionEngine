/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Baking/ReflectionProbe/ReflectionProbeBakeJob.hpp>

#include <Scene/Scene.hpp>
#include <Scene/EnvProbe.hpp>

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
