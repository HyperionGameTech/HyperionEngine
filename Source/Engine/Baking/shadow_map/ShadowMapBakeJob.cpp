/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Baking/shadow_map/ShadowMapBakeJob.hpp>

#include <Scene/Scene.hpp>
#include <Scene/Light.hpp>

namespace Hyperion {
namespace Baking {

BakeJob<Light>::~BakeJob()
{
}

void BakeJob<Light>::Start_Internal()
{
}

void BakeJob<Light>::Process_Internal(bool* outIsReadyToProcess)
{
    if (outIsReadyToProcess)
    {
        *outIsReadyToProcess = true;
    }
}

} // namespace Baking
} // namespace Hyperion
