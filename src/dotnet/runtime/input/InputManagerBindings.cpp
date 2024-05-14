/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <input/InputManager.hpp>

#include <Engine.hpp>
#include <Types.hpp>

using namespace hyperion;

extern "C" {
HYP_EXPORT bool InputManager_IsKeyDown(InputManager *input_manager, uint16 key)
{
    return input_manager->IsKeyDown(KeyCode(key));
}
} // extern "C"