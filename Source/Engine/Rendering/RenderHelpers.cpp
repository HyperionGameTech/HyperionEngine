/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <RenderingPch.hpp>

#include <Rendering/RenderHelpers.hpp>
#include <Rendering/MaterialTypes.hpp>

#include <Core/Math/MathUtil.hpp>

namespace Hyperion {
namespace RenderHelpers {

uint32 MipmapSize(uint32 srcSize, int lod)
{
    return MathUtil::Max(srcSize >> lod, 1u);
}

Vec3u WrapComputeGroupCount(uint32 numGroups)
{
    numGroups = MathUtil::Max(numGroups, 1u);

    const uint32 groupsX = MathUtil::Min(numGroups, MaxComputeGroupsPerDimension);

    return Vec3u { groupsX, (numGroups + groupsX - 1) / groupsX, 1 };
}

EnumFlags<ShaderTraits> GetShaderTraits(StringHash shaderNameHash)
{
    const ShaderDefinitions* definitions = GetShaderDefinitions();

    if (!definitions)
    {
        return ShaderTraits::None;
    }

    const ShaderDefinition* definition = definitions->FindEntry(shaderNameHash);

    if (!definition)
    {
        return ShaderTraits::None;
    }

    return definition->traits;
}

bool IsEmissive(const MaterialParameters& materialParameters)
{
    return materialParameters.emissiveIntensity > 0.0f
        && (materialParameters.emissiveColor.GetRed() > 0.0f
            || materialParameters.emissiveColor.GetGreen() > 0.0f
            || materialParameters.emissiveColor.GetBlue() > 0.0f);
}

} // namespace RenderHelpers

} // namespace Hyperion
