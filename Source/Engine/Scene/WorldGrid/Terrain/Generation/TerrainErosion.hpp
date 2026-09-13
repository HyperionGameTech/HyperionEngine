/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <Core/Utilities/Span.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

struct TerrainErosionSettings
{
    uint32 seed = 0;
    float spacing = 2.0f;
    uint32 iterations = 60;
    uint32 routingInterval = 4;
    float erodibility = 0.06f;
    float diffusion = 0.05f;
    float talusSlope = 1.2f;
};

void TerrainErodeHeightfield(Span<float> heights, uint32 size, int32 originX, int32 originZ, const TerrainErosionSettings& settings);

} // namespace Hyperion
