/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Glimmer/GlimmerTechnique.hpp>
#include <Rendering/Glimmer/SWRT/GlimmerSWRTTechnique.hpp>

namespace Hyperion {

GlimmerTechniqueType GetActiveGlimmerTechniqueType()
{
    return GlimmerTechniqueType::SWRT;
}

UniquePtr<GlimmerTechnique> CreateGlimmerTechnique(GlimmerTechniqueType type)
{
    switch (type)
    {
    case GlimmerTechniqueType::SWRT:
    default:
        return MakeUnique<GlimmerSWRTTechnique>();
    }
}

GlimmerSceneRegionParams GetGlimmerSceneRegionParams(GlimmerTechniqueType type)
{
    switch (type)
    {
    case GlimmerTechniqueType::SWRT:
    default:
        return GlimmerSWRTTechnique::GetSceneRegionParams();
    }
}

} // namespace Hyperion
