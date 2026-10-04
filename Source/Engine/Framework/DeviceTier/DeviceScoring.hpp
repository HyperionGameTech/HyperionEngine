/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Defines.hpp>

#include <Framework/DeviceTier/DeviceFacts.hpp>

namespace Hyperion {

namespace GradeThreshold {

static constexpr int Medium = 25;
static constexpr int High = 50;
static constexpr int Ultra = 75;

} // namespace GradeThreshold

ENGINE_API int ComputeDeviceGradeScore(const DeviceFacts& facts);

} // namespace Hyperion
