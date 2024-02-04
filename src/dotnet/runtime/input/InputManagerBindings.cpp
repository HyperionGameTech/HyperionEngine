#include <input/InputManager.hpp>

#include <Engine.hpp>
#include <Types.hpp>

using namespace hyperion;
using namespace hyperion::v2;

extern "C" {
    bool InputManager_IsKeyDown(InputManager *input_manager, UInt16 key)
    {
        return input_manager->IsKeyDown(key);
    }
}