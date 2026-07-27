/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Types.hpp>
#include <Core/Constants.hpp>

namespace Hyperion {

// General shared stencil masks used throughout the engine

static constexpr uint8 SkyStencilMask = 0x20;
static constexpr uint8 DebugStencilMask = 0x40;
static constexpr uint8 LightmapStencilMask = (1u << MaxAtlasesPerLightmapVolume) - 1;

} // namespace Hyperion