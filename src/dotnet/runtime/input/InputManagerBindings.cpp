#include <input/InputManager.hpp>

#include <Engine.hpp>
#include <Types.hpp>

using namespace hyperion;
using namespace hyperion::v2;

extern "C" {
HYP_EXPORT bool InputManager_IsKeyDown(InputManager *input_manager, uint16 key)
{
    return input_manager->IsKeyDown(key);
}
} // extern "C"