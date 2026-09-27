/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Types.hpp>

namespace Hyperion {

static constexpr uint8 UnlitObjectMask = 0x01;
static constexpr uint8 LightmappedObjectMask = 0x02;
static constexpr uint8 FoliageObjectMask = 0x04;
static constexpr uint8 CutoutObjectMask = 0x08;
static constexpr uint8 TreeObjectMask = 0x10;
static constexpr uint8 PlayerObjectMask = 0x20;

} // namespace Hyperion
