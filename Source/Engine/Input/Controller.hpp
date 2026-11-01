/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Types.hpp>

#include <Core/Defines.hpp>

namespace Hyperion {

/// Opaque handle to a Gamepad/Controller
enum class ControllerId : uint32;
static constexpr ControllerId InvalidController = Invalid<ControllerId>;

static constexpr uint8 MaxAttachedControllers = 8;

bool IsValidController(ControllerId);
bool IsSteamInputController(ControllerId);
uint8 GetControllerIndex(ControllerId);

#include <Input/Controller.inl>

} // namespace Hyperion