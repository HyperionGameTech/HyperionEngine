/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Glimmer/GlimmerTechnique.hpp>
#include <Rendering/Glimmer/SWRT/GlimmerSWRTTechnique.hpp>
#include <Rendering/Glimmer/SH/GlimmerSHTechnique.hpp>
#include <Rendering/Glimmer/GlimmerCVars.hpp>

#include <Rendering/Shared.hpp>

#include <Core/Math/MathUtil.hpp>

namespace Hyperion {

static StaticShaderPropertyId s_propGlimmerTechniqueSH { ShaderProperty(NAME("GLIMMER_TECHNIQUE_SH")) };

GlimmerTechniqueType GetActiveGlimmerTechniqueType()
{
    return GlimmerTechniqueType(MathUtil::Clamp(g_cvGlimmerTechnique.Get(), 0, int(GlimmerTechniqueType::Count) - 1));
}

void AddGlimmerApplyShaderProperties(ShaderPropertySet& outShaderProperties)
{
    if (GetActiveGlimmerTechniqueType() == GlimmerTechniqueType::SH)
    {
        outShaderProperties.Add(s_propGlimmerTechniqueSH);
    }
}

UniquePtr<GlimmerTechnique> CreateGlimmerTechnique(GlimmerTechniqueType type)
{
    switch (type)
    {
    case GlimmerTechniqueType::SH:
        return MakeUnique<GlimmerSHTechnique>();
    case GlimmerTechniqueType::SWRT:
    default:
        return MakeUnique<GlimmerSWRTTechnique>();
    }
}

GlimmerSceneRegionParams GetGlimmerSceneRegionParams(GlimmerTechniqueType type)
{
    switch (type)
    {
    case GlimmerTechniqueType::SH:
        return GlimmerSHTechnique::GetSceneRegionParams();
    case GlimmerTechniqueType::SWRT:
    default:
        return GlimmerSWRTTechnique::GetSceneRegionParams();
    }
}

} // namespace Hyperion
